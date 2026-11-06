// *****************************************************************************
//  tools/oracle/orun.cpp -- v32orun: the one headless runner on the stock Vircon32 console logic
//
//  It replaces the separate runners the projects had grown: v32sim's refrun and v32run (from v32c++),
//  v32cc's vrun, and v32lua's tools/headless/v32run. The CPU, memory, buses, timer and RNG are the
//  DesktopEmulator's own ConsoleLogic (V32Console), compiled unchanged except for one switch (CFI, below).
//
//  Two ways to run:
//
//    v32orun [options] PROG.vbin               BARE: PROG.vbin (`assemble -b`) is the cartridge program, on a
//                                              console with logging stand-ins for the GPU, SPU, gamepad,
//                                              cartridge and memory card (nothing is drawn or played) and a
//                                              BIOS of HLTs, so a hardware error ends the run with HWERROR.
//                                              This is what v32lua's audits use.
//    v32orun [options] --bios B --cart C       CONSOLE: the real BIOS and cartridge, the real GPU/SPU/gamepad/
//                                              cartridge/memory-card controllers. -g/-s/-B/-t/-P work too.
//    v32orun [options] --as-bios PROG.vbin     PROG.vbin as the BIOS program (v32cc's `vrun prog.vbin`), real devices
//
//  Options (the v32lua runner's, unchanged, plus the console ones):
//    -f N        frames to run (default 10; 600 with --bios/--cart)       -c N   cycle cap
//    -g / -s     log GPU commands (with the computed screen rectangle) / SPU port writes
//    -d a,b / -D dump globals at exit (needs -a prog.asm); -D = all, with RAW/ESTR/CANON lines (v32lua values)
//    -a FILE     the .asm to read `%define NAME 0xADDR` symbols from
//    -p "F:Btn=1;F:Btn=-1,..."   gamepad 0 script (Left Right Up Down Start A B X Y L R, Conn); --keys "10:A 14: 20:^>"
//    -t A,B      trap at hex addresses: print the trail and the call stack       -B   BP watch (RET with BP != at CALL)
//    -P FILE [F] per-address cycle profile (tools/v32sim-addrprof.py, v32lua's profile.py)
//    -m ADDR,N   dump N words of memory at exit      -G   (bare) start as after the BIOS: dirty RAM and registers
//    -b          per-frame cycle counts: "FC <frame> <cycles> <overrun 0/1>" (what v32lua's v32bench printed)
//    -q          no per-frame overrun lines          --memc FILE   memory card file (console mode; made if missing)
//    --date Y,D --time S   set the clock    --cfi=x86|arm --cfi-log --spu-state --no-memcard --fixed-time --trail=N
//                                           (the environment variables V32_CFI, V32_CFI_LOG, V32_SPU_STATE,
//                                           V32_NO_MEMCARD, V32_FIXED_TIME and V32_TRAIL do the same)
//    --compat=refrun|v32run|vrun   the command line and output of the old runner of that name (the wrappers
//                                  tools/oracle/bin/{refrun,v32run,vrun} are this)
//
//  Output: GPU/SPU lines "F<frame> GPU ..." / "F<frame> SPU ...", then
//    STATUS <FRAMES_EXHAUSTED|HALT|TRAPPED|CYCLE_LIMIT|HWERROR ...> frames= cycles= ip= overruns= gpu_dropped=
//    REGS R0=.. R15=..
//  gpu_dropped counts the draws the real GPU skipped for lack of its per-frame pixel budget (9 screens): in
//  bare mode from a model of that rule, in console mode from the real GPU's own counter.
//
//  CFI of a NaN or out-of-range float is undefined in the emulator's C++ and host-dependent (0x80000000 on
//  x86-64; 0 or saturated on ARM64). v32orun is built with a patched copy of V32CPUProcessors.cpp (build.sh)
//  that asks cfi_hook(): --cfi=arm picks the ARM behaviour, --cfi-log reports every such conversion.
//
//  Exit status: 0 normally, 2 usage/file errors, 3 when the console logic reports an exception.
// *****************************************************************************
#include "V32Console.hpp"
#include "ExternalInterfaces.hpp"
#include "../../VirconDefinitions/Constants.hpp"
#include "../../VirconDefinitions/DataStructures.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;
using namespace V32;

static int  g_frame = 0;
static int  g_overruns = 0;

// ---- the GPU's per-frame pixel budget, as the real one counts it (V32GPU.cpp) -------------------------------------
// every draw costs its full (scaled) region size, capped at the screen, x1.15 when scaled and +0.25 when rotated,
// on screen or not; a clear costs half a screen. Once spent, that draw and every later one in the frame is dropped.
static const long g_gpu_capacity = 9L * 640 * 360;
static long g_gpu_rem = g_gpu_capacity;
static int  g_gpu_dropped = 0, g_gpu_drop_frames = 0, g_gpu_last_drop = -1, g_gpu_first_drop = -1;
static bool g_log_gpu = false, g_log_spu = false;

static void note_drop()
{
    g_gpu_dropped++;
    if (g_gpu_last_drop != g_frame) { g_gpu_drop_frames++; g_gpu_last_drop = g_frame; }
    if (g_gpu_first_drop < 0) g_gpu_first_drop = g_frame;
}

// ---- CFI (called from the patched V32CPUProcessors.cpp) ------------------------------------------------------------
static int g_cfi_arm = 0, g_cfi_log = 0;
int32_t v32orun_cfi(float f, int32_t ip)
{
    bool bad = std::isnan(f) || f >= 2147483648.0f || f < -2147483648.0f;
    if (!bad) return (int32_t) f;
    if (g_cfi_log) {
        V32Word w; w.AsFloat = f;
        printf("CFI-UB F%d ip=%08X in=%08X\n", g_frame, (uint32_t)(ip - 1), w.AsBinary);
    }
    if (!g_cfi_arm) return INT32_MIN;
    if (std::isnan(f)) return 0;
    return f > 0 ? INT32_MAX : INT32_MIN;
}

// ---- v32lua value formatting for the -d / -D dumps ------------------------------------------------------------------
static string fmtword(V32Word w)
{
    char b[128];
    uint32_t u = w.AsBinary;
    if (u == 0xFFC00000u) return "nil";
    if (u == 0xFFC00001u) return "false";
    if (u == 0xFFC00002u) return "true";
    if ((u & 0xFFC00000u) == 0x7FC00000u) { snprintf(b, sizeof b, "romstr@%X", u & 0x3FFFFF); return b; }
    if ((u & 0xFFC00000u) == 0xFF800000u) { snprintf(b, sizeof b, "table@%X", u & 0x3FFFFF); return b; }
    if ((u & 0xFFC00000u) == 0xFFC00000u) { snprintf(b, sizeof b, "ramstr@%X", u & 0x3FFFFF); return b; }
    if ((u & 0xFFC00000u) == 0x7F800000u && u != 0x7F800000u) { snprintf(b, sizeof b, "func@%X", u & 0x3FFFFF); return b; }
    snprintf(b, sizeof b, "%.9g", w.AsFloat);
    return b;
}

// ---- logging GPU: stand-in (bare) or proxy of the real GPU (console) ---------------------------------------------
struct LogGPU : VirconControlInterface {
    V32GPU* inner = nullptr;                 // console mode: forward to the real GPU and read its registers back
    int32_t P[18] = {0};
    struct R { int32_t v[6]; };
    map<long, R> regions;
    LogGPU() { P[3] = (int32_t)0xFFFFFFFF; P[4] = 0x20; float one = 1.0f; memcpy(&P[9], &one, 4); memcpy(&P[10], &one, 4); }
    long key() { return (long)P[5] * 100000 + P[6]; }
    // register value as the log shows it
    int32_t reg(int p) {
        if (inner) { V32Word w; w.AsInteger = 0; inner->ReadPort(p, w); return w.AsInteger; }
        return p >= 12 ? regions[key()].v[p - 12] : P[p];
    }
    bool ReadPort(int32_t p, V32Word& r) override {
        if (inner) return inner->ReadPort(p, r);
        if (p > 17) return false;
        if (p == 1) { r.AsInteger = (int32_t) g_gpu_rem; return true; }
        if (p >= 12) { r.AsInteger = regions[key()].v[p - 12]; return true; }
        r.AsInteger = P[p]; return true;
    }
    bool WritePort(int32_t p, V32Word v) override {
        bool isdraw = (p == 0 && v.AsInteger >= 0x10 && v.AsInteger <= 0x14);
        bool dropped = false;
        if (inner) {
            long before = inner->RemainingPixels;
            if (!inner->WritePort(p, v)) return false;
            if (isdraw) dropped = before < 0 || inner->RemainingPixels < 0;
        } else {
            if (p > 17 || p == 1) return false;
            if (p >= 12) {
                R& rg = regions[key()]; rg.v[p - 12] = v.AsInteger;
                // real HW clamps: Min/Max to 0..TextureSize-1, hotspot to -1024..2047
                if (p < 16) { if (rg.v[p-12] < 0) rg.v[p-12] = 0; if (rg.v[p-12] > 1023) rg.v[p-12] = 1023; }
                return true;
            }
            if (p == 0) {
                long need = -1;
                if (v.AsInteger == 0x10) need = 640L * 360 / 2;
                else if (isdraw) {
                    R& rg = regions[key()];
                    V32Word zx, zy; zx.AsInteger = P[9]; zy.AsInteger = P[10];
                    bool zoom = (v.AsInteger == 0x12 || v.AsInteger == 0x14), rot = (v.AsInteger == 0x13 || v.AsInteger == 0x14);
                    float rw = abs(rg.v[2] - rg.v[0]) + 1, rh = abs(rg.v[3] - rg.v[1]) + 1;
                    if (zoom) { rw *= fabsf(zx.AsFloat); rh *= fabsf(zy.AsFloat); }
                    float cf = 1 + (zoom ? 0.15f : 0) + (rot ? 0.25f : 0);
                    need = (long)(int32_t)(cf * min((int)rw, 640) * min((int)rh, 360));
                }
                if (need >= 0) {
                    if (g_gpu_rem < 0) dropped = true;
                    else { g_gpu_rem -= need; if (g_gpu_rem < 0) { g_gpu_rem = -1; dropped = true; } }
                }
            } else P[p] = v.AsInteger;
        }
        if (dropped) note_drop();
        if (p != 0 || !g_log_gpu) return true;
        V32Word sx, sy, an; sx.AsInteger = reg(9); sy.AsInteger = reg(10); an.AsInteger = reg(11);
        const char* n = "?";
        switch (v.AsInteger) {
            case 0x10: n = "ClearScreen"; break; case 0x11: n = "DrawRegion"; break;
            case 0x12: n = "DrawZoomed"; break;  case 0x13: n = "DrawRotated"; break;
            case 0x14: n = "DrawRotozoomed"; break;
        }
        if (v.AsInteger == 0x10)
            printf("F%d GPU%s %s color=%08X\n", g_frame, dropped ? "-DROPPED" : "", n, (uint32_t)reg(2));
        else {
            int32_t rv[6]; for (int k = 0; k < 6; k++) rv[k] = reg(12 + k);
            bool zoom = (v.AsInteger == 0x12 || v.AsInteger == 0x14);
            float fx = zoom ? sx.AsFloat : 1.0f, fy = zoom ? sy.AsFloat : 1.0f;
            float x0 = reg(7) + fx * (rv[0] - rv[4]);
            float y0 = reg(8) + fy * (rv[1] - rv[5]);
            float w = fx * (rv[2] - rv[0] + 1), h = fy * (rv[3] - rv[1] + 1);
            if (w < 0) { x0 += w; w = -w; }
            if (h < 0) { y0 += h; h = -h; }
            printf("F%d GPU%s %s tex=%d reg=%d pt=(%d,%d) sx=%g sy=%g ang=%g mul=%08X blend=%X src=(%d,%d)-(%d,%d) hot=(%d,%d) SCREEN=(%.2f,%.2f %.2fx%.2f)\n",
                   g_frame, dropped ? "-DROPPED" : "", n, reg(5), reg(6), reg(7), reg(8), sx.AsFloat, sy.AsFloat, an.AsFloat,
                   (uint32_t)reg(3), reg(4), rv[0], rv[1], rv[2], rv[3], rv[4], rv[5], x0, y0, w, h);
        }
        return true;
    }
};

// ---- logging SPU: stand-in (bare) or proxy of the real SPU (console) ----------------------------------------------
struct LogSPU : VirconControlInterface {
    V32SPU* inner = nullptr;
    int32_t P[14] = {0};
    // --spu-state (bare mode): model each of the 16 channels' state and assigned sound, so ChannelState /
    // ChannelAssignedSound read back what the SELECTED channel was given. Sounds never run out. Off by default:
    // the port is then a plain latch and the logs of existing runs don't change.
    bool model = false;
    int32_t chState[16], chSound[16];
    LogSPU() { for (int i = 0; i < 16; i++) { chState[i] = 0x40; chSound[i] = 0; } }
    bool ReadPort(int32_t p, V32Word& r) override {
        if (inner) return inner->ReadPort(p, r);
        if (p > 13) return false;
        if (model && (p == 8 || p == 9)) { int c = P[3] & 15; r.AsInteger = (p == 8) ? chState[c] : chSound[c]; return true; }
        r.AsInteger = P[p]; return true;
    }
    void command(int32_t cmd) {
        int c = P[3] & 15;
        switch (cmd) {
            case 0x30: chState[c] = 0x42; break;
            case 0x31: if (chState[c] == 0x42) chState[c] = 0x41; break;
            case 0x32: chState[c] = 0x40; break;
            case 0x33: for (int i = 0; i < 16; i++) if (chState[i] == 0x42) chState[i] = 0x41; break;
            case 0x34: for (int i = 0; i < 16; i++) if (chState[i] == 0x41) chState[i] = 0x42; break;
            case 0x35: for (int i = 0; i < 16; i++) chState[i] = 0x40; break;
        }
    }
    bool WritePort(int32_t p, V32Word v) override {
        static const char* names[] = {"Command","GlobalVolume","SelectedSound","SelectedChannel","SoundLength",
            "SoundPlayWithLoop","SoundLoopStart","SoundLoopEnd","ChannelState","ChannelAssignedSound",
            "ChannelVolume","ChannelSpeed","ChannelLoopEnabled","ChannelPosition"};
        if (p > 13) return false;
        if (inner) { if (!inner->WritePort(p, v)) return false; }
        else {
            P[p] = v.AsInteger;
            if (model) {
                if (p == 0) command(v.AsInteger);
                else if (p == 9) chSound[P[3] & 15] = v.AsInteger;
            }
        }
        if (g_log_spu) {
            if (p == 1 || p == 10 || p == 11) printf("F%d SPU %s = %g\n", g_frame, names[p], v.AsFloat);
            else printf("F%d SPU %s = %d (0x%X)\n", g_frame, names[p], v.AsInteger, v.AsInteger);
        }
        return true;
    }
};

// ---- bare-mode stand-ins --------------------------------------------------------------------------------------------
struct StubPad : VirconControlInterface {
    int32_t sel = 0;
    int32_t state[4][12];
    StubPad() { for (auto& g : state) { g[0] = 1; for (int i = 1; i < 12; i++) g[i] = -1; } }
    bool ReadPort(int32_t p, V32Word& r) override {
        if (p > 12) return false;
        if (p == 0) { r.AsInteger = sel; return true; }
        r.AsInteger = state[sel & 3][p - 1]; return true;
    }
    bool WritePort(int32_t p, V32Word v) override { if (p != 0) return false; if (v.AsInteger >= 0 && v.AsInteger < 4) sel = v.AsInteger; return true; }
    void frame() { for (auto& g : state) for (int i = 1; i < 12; i++) { if (g[i] > 0) g[i]++; else g[i]--; } }
};

struct StubCar : VirconControlInterface, V32ROM {
    int32_t words = 0;
    bool ReadPort(int32_t p, V32Word& r) override {
        switch (p) { case 0: r.AsInteger = 1; return true; case 1: r.AsInteger = words; return true;
                     case 2: case 3: r.AsInteger = 0; return true; }
        return false;
    }
    bool WritePort(int32_t, V32Word) override { return false; }
};

struct StubMem : VirconControlInterface, V32RAM {
    bool present = true;    // --no-memcard: no card (its memory then faults, as on the console)
    bool ReadPort(int32_t p, V32Word& r) override { if (p) return false; r.AsInteger = present ? 1 : 0; return true; }
    bool WritePort(int32_t, V32Word) override { return false; }
};

// ---------------------------------------------------------------------------------------------------------------------
static vector<uint32_t> load_vbin(const char* path)
{
    ifstream f(path, ios::binary);
    if (!f) { fprintf(stderr, "v32orun: cannot open %s\n", path); exit(2); }
    char sig[8]; uint32_t n;
    f.read(sig, 8); f.read((char*)&n, 4);
    if (!f || memcmp(sig, "V32-VBIN", 8)) { fprintf(stderr, "v32orun: %s: not a VBIN\n", path); exit(2); }
    vector<uint32_t> w(n);
    f.read((char*)w.data(), n * 4);
    return w;
}

static void NoColor(GPUColor) {}
static void NoQuad(GPUQuad&) {}
static void NoInt(int) {}
static void NoLoad(int, void*) {}
static void NoVoid() {}
static void NoLog(const std::string&) {}
static void ThrowIt(const std::string& s) { throw std::runtime_error(s); }
static void install_callbacks(void (*log)(const std::string&))
{
    Callbacks::ClearScreen = NoColor; Callbacks::DrawQuad = NoQuad; Callbacks::SetMultiplyColor = NoColor;
    Callbacks::SetBlendingMode = NoInt; Callbacks::SelectTexture = NoInt; Callbacks::LoadTexture = NoLoad;
    Callbacks::UnloadCartridgeTextures = NoVoid; Callbacks::UnloadBiosTexture = NoVoid;
    Callbacks::LogLine = log; Callbacks::ThrowException = ThrowIt;
}

static V32Console con;       // static: it is large

// gamepad script: frame -> list of (slot, value); slot 0 = Conn, 1..11 = Left Right Up Down Start A B X Y L R
typedef map<int, vector<pair<int,int>>> PadScript;
static void parse_pad(const string& padscript, PadScript& pad)
{
    static const char* bn[] = {"Left","Right","Up","Down","Start","A","B","X","Y","L","R"};
    stringstream ss(padscript); string ev;
    while (getline(ss, ev, ';')) {
        if (ev.empty()) continue;
        int fr = atoi(ev.c_str()); size_t c = ev.find(':'); if (c == string::npos) continue;
        stringstream s2(ev.substr(c + 1)); string kv;
        while (getline(s2, kv, ',')) {
            size_t e = kv.find('='); if (e == string::npos) continue;
            string k = kv.substr(0, e); int v = atoi(kv.substr(e + 1).c_str());
            for (int b = 0; b < 11; b++) if (k == bn[b]) pad[fr].push_back({b + 1, v});
            if (k == "Conn") pad[fr].push_back({0, v});      // gamepad 0 plugged in (1) / out (0)
        }
    }
}
// --keys "10:A 14: 20:^>" : from frame 10 hold A, from 14 nothing, from 20 up and right (< > ^ v S A B X Y L R)
static void parse_keys(const char* keys, PadScript& pad)
{
    const char* names = "<>^vSABXYLR";            // 11 chars: Left Right Up Down Start A B X Y L R
    for (const char* p = keys; p && *p;) {
        while (*p == ' ') p++;
        if (!*p) break;
        int at = (int)strtol(p, (char**)&p, 10);
        if (*p == ':') p++;
        string held;
        while (*p && *p != ' ') held += *p++;
        for (int b = 0; b < 11; b++) pad[at].push_back({b + 1, held.find(names[b]) != string::npos ? 1 : -1});
    }
}

// ===================================================================================================================
//  the old runners' command lines, on the same engine
// ===================================================================================================================
static int compat_run(const string& which, int argc, char** argv)
{
    install_callbacks(which == "v32run" ? [](const std::string& s) { fprintf(stderr, "[console] %s\n", s.c_str()); } : NoLog);
    if (which == "refrun") {
        Callbacks::ThrowException = [](const std::string& m) { fprintf(stderr, "refrun: %s\n", m.c_str()); exit(3); };
        if (argc < 4) { fprintf(stderr, "usage: refrun BIOS CART FRAMES [--date Y,D] [--time S] [--dump N@HEXADDR]\n"); return 2; }
        int Frames = atoi(argv[3]), Year = -1, Day = 0, Seconds = -1, DumpCount = 0; unsigned DumpAddress = 0;
        for (int i = 4; i < argc; i++) {
            if (!strcmp(argv[i], "--date") && i + 1 < argc) sscanf(argv[++i], "%d,%d", &Year, &Day);
            else if (!strcmp(argv[i], "--time") && i + 1 < argc) Seconds = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--dump") && i + 1 < argc) sscanf(argv[++i], "%d@%x", &DumpCount, &DumpAddress);
        }
        con.LoadBios(argv[1]); con.LoadCartridge(argv[2]);
        if (Year >= 0) con.SetCurrentDate(Year, Day);
        if (Seconds >= 0) con.SetCurrentTime(Seconds / 3600, (Seconds / 60) % 60, Seconds % 60);
        con.SetPower(true);
        for (int f = 0; f < Frames; f++) { con.RunNextFrame(); if (con.IsCPUHalted()) break; }
        for (int i = 0; i < DumpCount; i++) {
            unsigned a = DumpAddress + i; if (a >= (unsigned)con.RAM.MemorySize) break;
            printf("%d\n", (int)con.RAM.Memory[a].AsInteger);
        }
        return 0;
    }
    if (which == "v32run") {               // v32c++'s: BIOS CART MEMCARD|- ADDRESS [FRAMES]
        if (argc < 5) { fprintf(stderr, "usage: v32run bios cart memcard|- address [frames]\n"); return 2; }
        int address = (int)strtol(argv[4], nullptr, 0), max_frames = argc > 5 ? atoi(argv[5]) : 600;
        try {
            con.LoadBios(argv[1]); con.LoadCartridge(argv[2]);
            if (string(argv[3]) != "-") {
                FILE* e = fopen(argv[3], "rb"); if (e) fclose(e); else con.CreateMemoryCard(argv[3]);
                con.LoadMemoryCard(argv[3]);
            }
            con.SetGamepadConnection(0, true); con.SetPower(true);
            int frame = 0;
            for (; frame < max_frames && !con.IsCPUHalted(); frame++) con.RunNextFrame();
            if (con.HasMemoryCard()) con.SaveMemoryCard();
            printf("frames=%d halted=%d ram[%d]=%d\n", frame, (int)con.IsCPUHalted(), address, con.RAM.Memory[address].AsInteger);
            return con.IsCPUHalted() ? 0 : 1;
        } catch (std::exception& e) { fprintf(stderr, "v32run: %s\n", e.what()); return 3; }
    }
    // vrun (v32cc's)
    Callbacks::ThrowException = [](const std::string& s) { fprintf(stderr, "vrun: %s\n", s.c_str()); exit(2); };
    const char *vbin = nullptr, *bios = nullptr, *cart = nullptr, *memc = nullptr, *keys = nullptr;
    int frames = 600, dump = 16, start = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) dump = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-a") && i + 1 < argc) start = (int)strtol(argv[++i], nullptr, 0);
        else if (!strcmp(argv[i], "--bios") && i + 1 < argc) bios = argv[++i];
        else if (!strcmp(argv[i], "--cart") && i + 1 < argc) cart = argv[++i];
        else if (!strcmp(argv[i], "--memc") && i + 1 < argc) memc = argv[++i];
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = argv[++i];
        else if (argv[i][0] != '-' && !vbin) vbin = argv[i];
        else { fprintf(stderr, "vrun: bad argument %s\n", argv[i]); return 2; }
    }
    if (!vbin && !(bios && cart)) {
        fprintf(stderr, "usage: vrun prog.vbin [-f frames] [-d count] [-a addr]\n"
                        "       vrun --bios BIOS.v32 --cart CART.v32 [--memc CARD] [--keys \"10:A 14: 20:^>\"] [-f frames] [-d count] [-a addr]\n");
        return 2;
    }
    vector<uint32_t> w;
    if (vbin) {
        w = load_vbin(vbin);
        con.BiosProgramROM.Connect(w.data(), w.size());
    } else {
        con.LoadBios(bios); con.LoadCartridge(cart);
        if (memc) {
            if (ifstream(memc, ios::binary)) con.LoadMemoryCard(memc);
            else { con.CreateMemoryCard(memc); con.LoadMemoryCard(memc); }
        }
    }
    con.SetPower(true);
    PadScript script; if (keys) parse_keys(keys, script);
    if (keys) con.SetGamepadConnection(0, true);
    int f = 0;
    for (; f < frames && !con.IsCPUHalted(); f++) {
        auto it = script.find(f);
        if (it != script.end()) for (auto& e : it->second) con.SetGamepadControl(0, (GamepadControls)(e.first - 1), e.second > 0);
        con.RunNextFrame();
    }
    if (memc && con.HasMemoryCard()) con.SaveMemoryCard();
    for (int i = 0; i < dump; i++) printf("%d\n", con.RAM.Memory[start + i].AsInteger);
    printf("STATUS %s\n", con.IsCPUHalted() ? "halted" : "timeout");
    return 0;
}

// ===================================================================================================================
int main(int argc, char** argv)
{
    // the old names, as command-line compatibility: v32orun --compat=NAME ARGS...
    if (argc >= 2 && !strncmp(argv[1], "--compat=", 9)) {
        string which = argv[1] + 9;
        if (which != "refrun" && which != "v32run" && which != "vrun") { fprintf(stderr, "v32orun: unknown --compat=%s\n", which.c_str()); return 2; }
        try { return compat_run(which, argc - 1, argv + 1); }
        catch (std::exception& e) { fprintf(stderr, "%s: %s\n", which.c_str(), e.what()); return which == "vrun" ? 2 : 3; }
    }

    install_callbacks(NoLog);              // ProcessHLT logs "CPU halted"; the STATUS line says it
    bool bench = false, dirty = false, quiet = false, canon = false, bpwatch = false, asbios = false;
    bool spu_state = false, no_memcard = false, fixed_time = false;
    const char *vbin = nullptr, *asmf = nullptr, *bios = nullptr, *cart = nullptr, *memc = nullptr, *profpath = nullptr, *keys = nullptr;
    int profstart = 0, frames = -1, year = -1, day = 0, seconds = -1;
    long long maxcycles = -1;
    string dump, padscript, memdump;
    vector<long long> traps;
    int trailn = 48;
    if (const char* e = getenv("V32_CFI")) g_cfi_arm = !strcmp(e, "arm");
    g_cfi_log = getenv("V32_CFI_LOG") != nullptr;
    spu_state = getenv("V32_SPU_STATE") && atoi(getenv("V32_SPU_STATE"));
    no_memcard = getenv("V32_NO_MEMCARD") && atoi(getenv("V32_NO_MEMCARD"));
    fixed_time = getenv("V32_FIXED_TIME") != nullptr;
    if (getenv("V32_TRAIL")) trailn = atoi(getenv("V32_TRAIL"));
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto need = [&](const char* what) -> char* {
            if (i + 1 >= argc) { fprintf(stderr, "v32orun: %s needs %s\n", a.c_str(), what); exit(2); }
            return argv[++i]; };
        if (a == "-a") asmf = need("a file");
        else if (a == "-f") frames = atoi(need("a number"));
        else if (a == "-g") g_log_gpu = true;
        else if (a == "-s") g_log_spu = true;
        else if (a == "-d") dump = need("names");
        else if (a == "-D") { dump = "all"; canon = true; }
        else if (a == "-p") padscript = need("a script");
        else if (a == "--keys") keys = need("a script");
        else if (a == "-q") quiet = true;
        else if (a == "-b") bench = true;
        else if (a == "-P") { profpath = need("a file"); if (i + 1 < argc && argv[i+1][0] != '-') profstart = atoi(argv[++i]); }
        else if (a == "-m") memdump = need("addr,n");
        else if (a == "-G") dirty = true;
        else if (a == "-c") maxcycles = atoll(need("a number"));
        else if (a == "-B") bpwatch = true;
        else if (a == "-t") { stringstream ts(need("addresses")); string t; while (getline(ts, t, ',')) if (!t.empty()) traps.push_back(strtoll(t.c_str(), nullptr, 16)); }
        else if (a == "--bios") bios = need("a file");
        else if (a == "--cart") cart = need("a file");
        else if (a == "--memc") memc = need("a file");
        else if (a == "--as-bios") asbios = true;
        else if (a == "--date") sscanf(need("Y,D"), "%d,%d", &year, &day);
        else if (a == "--time") seconds = atoi(need("seconds"));
        else if (a == "--cfi=arm") g_cfi_arm = 1;
        else if (a == "--cfi=x86") g_cfi_arm = 0;
        else if (a == "--cfi-log") g_cfi_log = 1;
        else if (a == "--spu-state") spu_state = true;
        else if (a == "--no-memcard") no_memcard = true;
        else if (a == "--fixed-time") fixed_time = true;
        else if (a.rfind("--trail=", 0) == 0) trailn = atoi(a.c_str() + 8);
        else if (a[0] == '-' && a.size() > 1) { fprintf(stderr, "v32orun: unknown option %s\n", a.c_str()); return 2; }
        else vbin = argv[i];
    }
    if (trailn < 1 || trailn > 4096) trailn = 48;
    bool console_mode = bios && cart;
    if ((bios || cart) && !console_mode) { fprintf(stderr, "v32orun: --bios and --cart go together\n"); return 2; }
    if (!vbin && !console_mode) {
        fprintf(stderr, "usage: v32orun prog.vbin [options]            (bare: logging stand-ins; see the header of tools/oracle/orun.cpp)\n"
                        "       v32orun --bios B.v32 --cart C.v32 [options]\n"
                        "       v32orun --as-bios prog.vbin [options]\n");
        return 2;
    }
    if (asbios && !vbin) { fprintf(stderr, "v32orun: --as-bios needs a .vbin\n"); return 2; }
    if (frames < 0) frames = console_mode ? 600 : 10;
    bool bare = !console_mode && !asbios;           // stand-in devices

    // ---- wire the console -------------------------------------------------------------------------------------------
    static LogGPU lgpu; static LogSPU lspu; static StubPad PAD; static StubCar CAR; static StubMem MEMC; static V32ROM BIOS;
    static uint32_t bios16[16] = {0};               // word 0 = HLT: the hardware error vector lands here
    vector<uint32_t> prog;
    V32Word dummy;
    VirconMemoryInterface* cartmem = &con.CartridgeController;
    try {
        if (console_mode) {
            con.LoadBios(bios); con.LoadCartridge(cart);
            if (memc) {
                if (ifstream(memc, ios::binary)) con.LoadMemoryCard(memc);
                else { con.CreateMemoryCard(memc); con.LoadMemoryCard(memc); }
            }
            lgpu.inner = &con.GPU; con.ControlBus.Slaves[2] = &lgpu;
            if (g_log_spu) { lspu.inner = &con.SPU; con.ControlBus.Slaves[3] = &lspu; }
        } else if (asbios) {
            prog = load_vbin(vbin);
            con.BiosProgramROM.Connect(prog.data(), prog.size());
        } else {
            prog = load_vbin(vbin);
            BIOS.Connect(bios16, 16);
            CAR.Connect(prog.data(), prog.size()); CAR.words = prog.size();
            lspu.model = spu_state;
            if (no_memcard) MEMC.present = false; else MEMC.Connect(Constants::MemoryCardSize);
            con.MemoryBus.Slaves[1] = &BIOS; con.MemoryBus.Slaves[2] = &CAR; con.MemoryBus.Slaves[3] = &MEMC;
            con.ControlBus.Slaves[2] = &lgpu; con.ControlBus.Slaves[3] = &lspu; con.ControlBus.Slaves[4] = &PAD;
            con.ControlBus.Slaves[5] = &CAR; con.ControlBus.Slaves[6] = &MEMC;
            cartmem = &CAR;
        }
        // ThrowException from the console logic (bad ROM, GPU limits) ends the run with status 3
        if (year >= 0) con.SetCurrentDate(year, day);
        if (seconds >= 0) con.SetCurrentTime(seconds / 3600, (seconds / 60) % 60, seconds % 60);
        con.SetPower(true);
        // --fixed-time: start the clock at a fixed date/time, so carts that seed their RNG from it run identically
        // every time -- for comparing two builds' GPU/SPU logs.
        if (fixed_time) { con.Timer.CurrentDate = (2026 << 16) | 100; con.Timer.CurrentTime = 12 * 3600; }
    } catch (std::exception& e) { fprintf(stderr, "v32orun: %s\n", e.what()); return 3; }
    (void)dummy;

    if (bare) {
        if (dirty) {
            // -G: start the cart the way the real BIOS leaves the machine: RAM NOT zeroed (the BIOS's globals and
            // stack are left behind), BIOS texture (-1) still selected; registers hold whatever the BIOS left.
            uint32_t x = 0x12345678;
            for (int a = 0; a < Constants::RAMSize; a++) {
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                V32Word w; w.AsBinary = x; con.RAM.WriteAddress(a, w);
            }
            lgpu.P[5] = -1;
            for (int r = 0; r < 14; r++) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; con.CPU.Registers[r].AsBinary = x; }
            con.CPU.StackPointer.AsInteger = Constants::RAMSize - 7;
            con.CPU.BasePointer.AsInteger = Constants::RAMSize - 3;
        }
        con.CPU.InstructionPointer.AsInteger = Constants::CartridgeProgramROMFirstAddress;
    }

    PadScript pad;
    parse_pad(padscript, pad);
    if (keys) parse_keys(keys, pad);
    if (!bare && !pad.empty()) con.SetGamepadConnection(0, true);

    V32CPU& CPU = con.CPU;
    map<uint32_t, long long> prof;
    long long cyc = 0;
    static int32_t hist[4096]; long long hpos = 0;
    vector<int32_t> shadow;                  // -B: BP at each CALL
    string status = "FRAMES_EXHAUSTED";
    try {
    for (g_frame = 0; g_frame < frames; g_frame++) {
        con.Timer.ChangeFrame(); CPU.ChangeFrame();
        if (bare) { g_gpu_rem = g_gpu_capacity; PAD.frame(); }       // V32GPU::ChangeFrame
        else { con.GPU.ChangeFrame(); con.SPU.ChangeFrame(); con.GamepadController.ChangeFrame(); }
        auto pe = pad.find(g_frame);
        if (pe != pad.end()) for (auto& e : pe->second) {
            if (bare) PAD.state[0][e.first] = e.first == 0 ? (e.second > 0 ? 1 : 0) : (e.second > 0 ? 1 : -1);
            else if (e.first == 0) con.SetGamepadConnection(0, e.second > 0);
            else con.SetGamepadControl(0, (GamepadControls)(e.first - 1), e.second > 0);
        }
        bool hwerr = false; long long frame_start = cyc;
        for (int i = 0; i < Constants::CyclesPerFrame; i++) {
            if (profpath && g_frame >= profstart) prof[(uint32_t) CPU.InstructionPointer.AsInteger]++;
            if (bpwatch) {
                // -B: a routine must return with the caller's BP. Report the first RET where BP differs from its
                // value at the CALL.
                V32Word w; con.MemoryBus.ReadAddress(CPU.InstructionPointer.AsInteger, w);
                int op = (int) ((w.AsBinary >> 26) & 63);
                if (op == 3) shadow.push_back(CPU.BasePointer.AsInteger);
                else if (op == 4 && !shadow.empty()) {
                    int32_t want = shadow.back(); shadow.pop_back();
                    if (want != CPU.BasePointer.AsInteger && traps.empty()) {
                        printf("BPWATCH ret at %08X: BP=%08X, was %08X at the CALL (frame %d)\n",
                               (uint32_t) CPU.InstructionPointer.AsInteger,
                               (uint32_t) CPU.BasePointer.AsInteger, (uint32_t) want, g_frame);
                        traps.push_back(CPU.InstructionPointer.AsInteger);
                    }
                }
            }
            if (!traps.empty()) {
                hist[hpos++ % 4096] = CPU.InstructionPointer.AsInteger;
                bool hit = false;
                for (long long t : traps) if (CPU.InstructionPointer.AsInteger == (int32_t) t) hit = true;
                if (hit) {
                    status = "TRAPPED";
                    printf("TRAP at %08X\nTRAIL", (uint32_t) CPU.InstructionPointer.AsInteger);
                    for (int k = trailn; k >= 1; k--) if (hpos - k >= 0) printf(" %08X", (uint32_t) hist[(hpos - k) % 4096]);
                    printf("\nSTACK");
                    int32_t bp = CPU.BasePointer.AsInteger;
                    for (int d = 0; d < 32 && bp > 0 && bp < Constants::RAMSize - 1; d++) {
                        V32Word ret, nbp; con.RAM.ReadAddress(bp + 1, ret); con.RAM.ReadAddress(bp, nbp);
                        printf(" %08X", ret.AsBinary);
                        if (nbp.AsInteger <= bp) break;
                        bp = nbp.AsInteger;
                    }
                    printf("\n");
                    CPU.Halted = true; break;
                }
            }
            try { con.Timer.RunNextCycle(); CPU.RunNextCycle(); }
            catch (CPUException&) { hwerr = true; cyc++; break; }    // hardware error: the frame ends, as in RunNextFrame
            cyc++;
            if (CPU.Halted || CPU.Waiting) break;
            if (maxcycles > 0 && cyc >= maxcycles) break;
        }
        if (!bare && con.MemoryCardController.PendingSave) con.SaveMemoryCard();
        if (bench) printf("FC %d %lld %d\n", g_frame, cyc - frame_start, (!CPU.Waiting && !CPU.Halted) ? 1 : 0);
        if (CPU.Halted) break;
        if (maxcycles > 0 && cyc >= maxcycles) { status = "CYCLE_LIMIT"; break; }
        if (!CPU.Waiting && !hwerr) { g_overruns++; if (!quiet) printf("F%d: frame budget exhausted without WAIT\n", g_frame); }
    }
    } catch (std::exception& e) { fprintf(stderr, "v32orun: %s\n", e.what()); return 3; }
    if (CPU.Halted && status != "TRAPPED" && !traps.empty()) {
        printf("HALT TRAIL");
        for (int k = trailn; k >= 1; k--) if (hpos - k >= 0) printf(" %08X", (uint32_t) hist[(hpos - k) % 4096]);
        printf("\n");
    }
    if (CPU.Halted && status != "TRAPPED") {
        if (bare && CPU.InstructionPointer.AsInteger == Constants::BiosProgramROMFirstAddress + 1) {
            char b[160];
            snprintf(b, sizeof b, "HWERROR code=%d ip=%08X instr=%08X imm=%08X",
                     CPU.Registers[0].AsInteger, CPU.Registers[1].AsBinary,
                     CPU.Registers[2].AsBinary, CPU.Registers[3].AsBinary);
            status = b;
        } else status = "HALT";
    }
    if (!memdump.empty()) { unsigned a0 = 0, n = 16; sscanf(memdump.c_str(), "%x,%u", &a0, &n);
        for (unsigned k = 0; k < n; k++) { V32Word v; bool ok = a0 >= 0x20000000 ? cartmem->ReadAddress(a0 - 0x20000000 + k, v) : con.RAM.ReadAddress(a0 + k, v);
            printf("MEM %08X = %08X %s\n", a0 + k, ok ? v.AsBinary : 0, ok ? fmtword(v).c_str() : "?"); } }
    if (profpath) { FILE* pf = fopen(profpath, "w"); if (pf) { for (auto& e : prof) fprintf(pf, "%08X %lld\n", e.first, e.second); fclose(pf); } }
    if (!bare && memc && con.HasMemoryCard()) con.SaveMemoryCard();
    printf("STATUS %s frames=%d cycles=%lld ip=%08X overruns=%d gpu_dropped=%d", status.c_str(), g_frame, cyc, (uint32_t) CPU.InstructionPointer.AsInteger, g_overruns, g_gpu_dropped);
    if (g_gpu_dropped) printf(" (in %d frames, first F%d: over the GPU's 9-screen pixel budget)", g_gpu_drop_frames, g_gpu_first_drop);
    printf("\n");
    printf("REGS");
    for (int r = 0; r < 16; r++) { V32Word w; memcpy(&w, (char*)&CPU.Registers[0] + r * 4, 4); printf(" R%d=%08X", r, w.AsBinary); }
    printf("\n");

    if (!dump.empty() && asmf) {
        map<string, uint32_t> sym;
        ifstream f(asmf); string line;
        while (getline(f, line)) {
            char n[256]; unsigned v;
            if (sscanf(line.c_str(), "%%define %255s 0x%x", n, &v) == 2) sym[n] = v;
        }
        vector<string> names;
        if (dump == "all") { for (auto& s : sym) if (s.second < 0x10000 && s.first.rfind("func_", 0) && s.first.find("BOXED") == string::npos) names.push_back(s.first); }
        else { stringstream ss(dump); string n; while (getline(ss, n, ',')) names.push_back(n); }
        for (auto& n : names) {
            if (!sym.count(n)) { printf("SYM %s ?\n", n.c_str()); continue; }
            V32Word w; con.RAM.ReadAddress(sym[n], w);
            auto rdstr = [&](bool rom, uint32_t addr) { string r; for (int k = 0; k < 4000; k++) { V32Word c;
                    bool ok = rom ? cartmem->ReadAddress(addr + k, c) : con.RAM.ReadAddress(addr + k, c);
                    if (!ok || c.AsInteger == 0) break; r += (char) c.AsInteger; } return r; };
            if (canon) {
                // machine-readable: RAW / ESTR / CANON <name> <kind> <value>
                uint32_t u = w.AsBinary; string nm = n.rfind("var_", 0) == 0 ? n.substr(4) : n;
                printf("RAW %s %08X\n", nm.c_str(), u);
                if ((u & 0xFFC00000u) == 0x7FC00000u || ((u & 0xFFC00000u) == 0xFFC00000u && u > 0xFFC00002u)) {
                    // escaped copy (one line, non-printables as \xHH) for the unit-test harness
                    bool rom = (u & 0xFFC00000u) == 0x7FC00000u; string e;
                    for (int k = 0; k < 4000; k++) { V32Word c;
                        bool ok = rom ? cartmem->ReadAddress((u & 0x3FFFFF) + k, c) : con.RAM.ReadAddress((u & 0x3FFFFF) + k, c);
                        if (!ok || c.AsInteger == 0) break;
                        int ch = c.AsInteger; char b[8];
                        if (ch >= 32 && ch < 127) e += (char) ch; else { snprintf(b, sizeof b, "\\x%02X", ch & 0xFF); e += b; } }
                    printf("ESTR %s %s\n", nm.c_str(), e.c_str());
                }
                if (u == 0xFFC00000u) printf("CANON %s nil\n", nm.c_str());
                else if (u == 0xFFC00001u) printf("CANON %s bool false\n", nm.c_str());
                else if (u == 0xFFC00002u) printf("CANON %s bool true\n", nm.c_str());
                else if ((u & 0xFFC00000u) == 0x7FC00000u) printf("CANON %s str %s\n", nm.c_str(), rdstr(true, u & 0x3FFFFF).c_str());
                else if ((u & 0xFFC00000u) == 0xFFC00000u) printf("CANON %s str %s\n", nm.c_str(), rdstr(false, u & 0x3FFFFF).c_str());
                else if ((u & 0xFFC00000u) == 0xFF800000u) printf("CANON %s table\n", nm.c_str());
                else if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFF) != 0) printf("CANON %s function\n", nm.c_str());
                else printf("CANON %s num %.9g\n", nm.c_str(), w.AsFloat);
                continue;
            }
            string extra;
            if ((w.AsBinary & 0xFFC00000u) == 0x7FC00000u) {
                // ROM string: one char per word, NUL-terminated, at cart page + payload
                extra = " \"";
                for (int k = 0; k < 200; k++) {
                    V32Word c;
                    if (!cartmem->ReadAddress((w.AsBinary & 0x3FFFFF) + k, c) || c.AsInteger == 0) break;
                    extra += (char) c.AsInteger;
                }
                extra += "\"";
            }
            printf("SYM %s @%X = %08X  %s%s\n", n.c_str(), sym[n], w.AsBinary, fmtword(w).c_str(), extra.c_str());
            if ((w.AsBinary & 0xFFC00000u) == 0xFF800000u) {
                // table: header [cap, length, array, hash] + contents
                uint32_t t = w.AsBinary & 0x3FFFFF; V32Word h[4];
                for (int k = 0; k < 4; k++) con.RAM.ReadAddress(t + k, h[k]);
                int cap = h[0].AsInteger & 0xFFFF;
                printf("    cap=%d len=%d array@%X hash@%X\n", cap, h[1].AsInteger, h[2].AsBinary, h[3].AsBinary);
                for (int k = 0; k < cap; k++) { V32Word v; con.RAM.ReadAddress(h[2].AsInteger + k, v);
                    if (v.AsBinary != 0xFFC00000u) printf("    [%d] = %s\n", k + 1, fmtword(v).c_str()); }
                if (h[3].AsInteger) { V32Word hc, hu; con.RAM.ReadAddress(h[3].AsInteger, hc); con.RAM.ReadAddress(h[3].AsInteger + 1, hu);
                    printf("    hash cap=%d used=%d\n", hc.AsInteger, hu.AsInteger);
                    for (int k = 0; k < hc.AsInteger && k < 4096; k++) { V32Word kk, vv;
                        con.RAM.ReadAddress(h[3].AsInteger + 2 + 2 * k, kk); con.RAM.ReadAddress(h[3].AsInteger + 3 + 2 * k, vv);
                        if (kk.AsBinary != 0xFFC00000u) printf("    slot %d: %s = %s\n", k, fmtword(kk).c_str(), fmtword(vv).c_str()); } }
            }
        }
    }
    return 0;
}
