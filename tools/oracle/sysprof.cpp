// sysprof -- whole-system profiler for a v32nix cartridge (tools/sysprof.sh runs it; docs/PERF-ONSYSTEM.md).
//
// Runs the cartridge on the emulator core (as v32prof does) and samples the instruction pointer EVERY cycle:
//   (bios)             IP in the BIOS ROM
//   k:FUNCTION         IP in the cartridge (the kernel), from a function map "addr,x,0,name" (sysprof.sh makes it with v32as --symbols)
//   PROG:FUNCTION      IP in the user window; the resident program is recognised by comparing 64 words of the window with each
//                      candidate image (out/*/PROG.x, or a .vbin), and its functions come from PROG.d/PROG.sym (tools/mkuser.sh writes both)
//   (idle: WAIT)       the cycles of a frame left after WAIT
// Every PERIOD cycles (default 61) it also walks the BP chain, which gives inclusive time per function.
// It stops when the C global test_errors (from cart.asm) goes from negative to >= 0, or after FRAMES frames.
//
// usage: sysprof bios.v32 cart.v32 kernel.map cart.asm FRAMES prog.x:prog.sym [...]
// environment: START/END    count only frames START..END          TOP=N       rows per table (default 60)
//              PERIOD=N     BP-chain sample period (cycles)       TIMELINE=f  one line per frame: swaps, words moved, cycles by class
//              CALLERS="a b" caller chains of these functions     SUBTREE=f   self time of everything called from f
//              DUMPCONS=18  print the kernel's text console       RAMDUMP=f   write the RAM at the end
// Why not v32prof: it only knows `compile`'s __function_ labels (none in a v32cc build), and it cannot see the user window.
#include "V32Console.hpp"
#include "ExternalInterfaces.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
using namespace V32;
static void NoColor( GPUColor ) {}
static void NoQuad( GPUQuad& ) {}
static void NoInt( int ) {}
static void NoLoad( int, void* ) {}
static void NoArgs() {}
static void Log( const std::string& ) {}
static void Throw( const std::string& s ) { throw std::runtime_error( s ); }

struct Fn { uint32_t start; int id; };
struct Table { std::vector<Fn> f; };
static std::vector<std::string> names;
static std::map<std::string,int> nameid;
static int idof( const std::string& n ) { auto it = nameid.find( n ); if( it != nameid.end() ) return it->second; names.push_back( n ); return nameid[ n ] = (int)names.size() - 1; }
static int lookup( const Table& t, uint32_t a )
{
    auto it = std::upper_bound( t.f.begin(), t.f.end(), a, []( uint32_t v, const Fn& x ) { return v < x.start; } );
    if( it == t.f.begin() ) return -1;
    return ( it - 1 )->id;
}
struct Prog { std::string name; std::vector<uint32_t> img; Table t; };
static const uint32_t WB = 0x100000, WT = 0x180000;

int main( int argc, char** argv )
{
    if( argc < 7 ) { fprintf( stderr, "usage\n" ); return 2; }
    Callbacks::ClearScreen = NoColor; Callbacks::DrawQuad = NoQuad; Callbacks::SetMultiplyColor = NoColor;
    Callbacks::SetBlendingMode = NoInt; Callbacks::SelectTexture = NoInt; Callbacks::LoadTexture = NoLoad;
    Callbacks::UnloadCartridgeTextures = NoArgs; Callbacks::UnloadBiosTexture = NoArgs; Callbacks::LogLine = Log; Callbacks::ThrowException = Throw;
    int maxframes = atoi( argv[ 5 ] );
    int PERIOD = getenv( "PERIOD" ) ? atoi( getenv( "PERIOD" ) ) : 61;
    Table kt;
    { std::ifstream in( argv[ 3 ] ); std::string line;
      while( std::getline( in, line ) ) { std::stringstream ss( line ); std::string a, f, l, lab; std::getline( ss, a, ',' ); std::getline( ss, f, ',' ); std::getline( ss, l, ',' ); std::getline( ss, lab, ',' );
        if( !lab.empty() ) kt.f.push_back( { (uint32_t)strtoul( a.c_str(), 0, 16 ), idof( "k:" + lab ) } ); }
      std::sort( kt.f.begin(), kt.f.end(), []( const Fn& a, const Fn& b ) { return a.start < b.start; } ); }
    std::map<std::string,uint32_t> globals;
    { std::ifstream in( argv[ 4 ] ); std::string line;
      while( std::getline( in, line ) ) { std::stringstream ss( line ); std::string d, n, v; ss >> d >> n >> v; if( d == "%define" && n.rfind( "global_", 0 ) == 0 ) globals[ n.substr( 7 ) ] = (uint32_t)strtoul( v.c_str(), 0, 0 ); } }
    std::vector<Prog> progs;
    for( int i = 6; i < argc; i++ )
    {
        std::string s = argv[ i ]; size_t c = s.find( ':' ); Prog p; std::string vb = s.substr( 0, c ), sy = s.substr( c + 1 );
        p.name = vb.substr( vb.find_last_of( '/' ) + 1 ); p.name = p.name.substr( 0, p.name.find( '.' ) );
        FILE* f = fopen( vb.c_str(), "rb" ); { uint32_t m = 0; fread( &m, 4, 1, f ); fseek( f, m == 0x58323356u ? 32 : 12, SEEK_SET ); }   /* V32X (8-word header) or VBIN */ uint32_t w; while( fread( &w, 4, 1, f ) == 1 ) p.img.push_back( w ); fclose( f );
        std::ifstream in( sy ); std::string line;
        while( std::getline( in, line ) ) { std::stringstream ss( line ); std::string a, n, k; std::getline( ss, a, ',' ); std::getline( ss, n, ',' ); std::getline( ss, k, ',' );
            if( k != "F" ) continue; if( n.find( '.' ) != std::string::npos ) continue;
            if( n.rfind( "__function_", 0 ) == 0 ) n = n.substr( 11 );
            p.t.f.push_back( { (uint32_t)strtoul( a.c_str(), 0, 16 ), idof( p.name + ":" + n ) } ); }
        std::sort( p.t.f.begin(), p.t.f.end(), []( const Fn& a, const Fn& b ) { return a.start < b.start; } );
        progs.push_back( p );
    }
    int BIOS = idof( "(bios)" ), IDLE = idof( "(idle: WAIT)" ), UNK = idof( "(unknown)" );
    std::vector<uint64_t> self, incl;
    int START = getenv( "START" ) ? atoi( getenv( "START" ) ) : 0; int gframe = 0;
    auto bump = [&]( std::vector<uint64_t>& v, int id, uint64_t n ) { if( gframe < START || ( getenv( "END" ) && gframe > atoi( getenv( "END" ) ) ) ) return; if( (int)v.size() <= id ) v.resize( names.size() + 16 ); v[ id ] += n; };
    V32Console console;
    console.LoadBios( argv[ 1 ] ); console.LoadCartridge( argv[ 2 ] ); console.SetPower( true );
    auto ram = [&]( uint32_t a ) -> uint32_t { return a < 0x400000 ? (uint32_t)console.RAM.Memory[ a ].AsInteger : 0; };
    int cur = -1;
    auto identify = [&]() {
        int best = -1, bs = -1;
        for( size_t i = 0; i < progs.size(); i++ ) { int s = 0; const auto& im = progs[ i ].img;
            for( uint32_t k = 0; k < 64; k++ ) { uint32_t off = ( k * 2654435761u ) % im.size(); if( ram( WB + off ) == im[ off ] ) s++; }
            if( s > bs ) { bs = s; best = (int)i; } }
        return bs >= 60 ? best : -1;
    };
    auto fnof = [&]( uint32_t ip ) -> int {
        if( ip >= 0x20000000u ) { int r = lookup( kt, ip ); return r < 0 ? UNK : r; }
        if( ip >= 0x10000000u ) return BIOS;
        if( ip >= WB && ip < WT ) { if( cur < 0 ) return idof( "(user ?)" ); int r = lookup( progs[ cur ].t, ip ); return r < 0 ? idof( progs[ cur ].name + ":(?)" ) : r; }
        return idof( "(ram code)" );
    };
    uint32_t a_err = globals.count( "test_errors" ) ? globals[ "test_errors" ] : 0;
    uint32_t a_sw = globals.count( "swap_switches" ) ? globals[ "swap_switches" ] : 0, a_mv = globals.count( "swap_words_moved" ) ? globals[ "swap_words_moved" ] : 0;
    int frame = 0, done_frame = -1; bool armed = false;
    uint64_t cyc = 0;
    FILE* tl = getenv( "TIMELINE" ) ? fopen( getenv( "TIMELINE" ), "w" ) : nullptr;
    std::set<int> seen; std::map<std::string,uint64_t> subself; std::map<uint32_t,uint64_t> unkip; std::map<std::string,uint64_t> callers; const char* CW = getenv( "CALLERS" );
    for( ; frame < maxframes && !console.IsCPUHalted(); frame++ )
    {
        console.Timer.ChangeFrame(); console.CPU.ChangeFrame(); console.GPU.ChangeFrame(); console.SPU.ChangeFrame(); console.GamepadController.ChangeFrame();
        std::map<std::string,uint64_t> fr; gframe = frame;
        int i = 0;
        try {
            for( ; i < Constants::CyclesPerFrame; i++ )
            {
                if( console.CPU.Waiting || console.CPU.Halted ) break;
                uint32_t ip = (uint32_t)console.CPU.InstructionPointer.AsInteger;
                if( ip >= WB && ip < WT && cur < 0 ) cur = identify();
                int f = fnof( ip );
                if( ip >= 0x20000000u && ( names[ f ] == "k:window_load" || names[ f ] == "k:window_switch" || names[ f ] == "k:exec_load" || names[ f ] == "k:kexec_user" ) ) cur = -1;
                bump( self, f, 1 ); if( f == UNK && gframe >= START ) unkip[ ip ]++;
                fr[ ip >= 0x20000000u ? "kernel" : ip >= 0x10000000u ? "bios" : ( cur >= 0 ? progs[ cur ].name : "user?" ) ]++;
                if( ( cyc++ % PERIOD ) == 0 )
                {
                    seen.clear(); seen.insert( f ); bump( incl, f, PERIOD ); bool wantc = CW && gframe >= START && strstr( CW, names[ f ].c_str() ); std::string chain = names[ f ];
                    uint32_t bp = (uint32_t)console.CPU.BasePointer.AsInteger;
                    for( int d = 0; d < 200 && bp > 0 && bp < 0x3FFFFE; d++ )
                    {
                        uint32_t ra = ram( bp + 1 ), nbp = ram( bp );
                        int g = fnof( ra );
                        if( g != UNK && !seen.count( g ) ) { seen.insert( g ); bump( incl, g, PERIOD ); }
                        if( wantc && d < 4 ) chain += " < " + names[ g ];
                        if( nbp <= bp ) break;
                        bp = nbp;
                    }
                    if( wantc ) callers[ chain ] += PERIOD;
                    if( getenv( "SUBTREE" ) && gframe >= START ) { int st = idof( getenv( "SUBTREE" ) ); if( seen.count( st ) ) subself[ names[ f ] ] += PERIOD; }
                }
                console.Timer.RunNextCycle(); console.CPU.RunNextCycle();
            }
        } catch( CPUException& ) {}
        bump( self, IDLE, Constants::CyclesPerFrame - i );
        fr[ "idle" ] += Constants::CyclesPerFrame - i;
        if( tl ) { fprintf( tl, "%d sw=%u mv=%u", frame, ram( a_sw ), ram( a_mv ) ); for( auto& x : fr ) fprintf( tl, " %s=%llu", x.first.c_str(), (unsigned long long)x.second ); fprintf( tl, "\n" ); }
        if( a_err && (int32_t)ram( a_err ) < 0 ) armed = true;
        if( a_err && armed && done_frame < 0 && (int32_t)ram( a_err ) >= 0 ) { done_frame = frame; break; }
    }
    if( getenv( "DUMPCONS" ) && globals.count( "cons_buf" ) ) { uint32_t a = globals[ "cons_buf" ]; int R = atoi( getenv( "DUMPCONS" ) ), C = 64;
        for( int r = 0; r < R; r++ ) { std::string l; for( int c = 0; c < C; c++ ) { uint32_t ch = ram( a + r * C + c ); l += ( ch >= 32 && ch < 127 ) ? (char)ch : '.'; } printf( "|%s|\n", l.c_str() ); } }
    if( getenv( "RAMDUMP" ) ) { FILE* d = fopen( getenv( "RAMDUMP" ), "wb" ); fwrite( &console.RAM.Memory[ 0 ], 4, console.RAM.Memory.size(), d ); fclose( d ); }
    uint64_t total = 0; for( auto v : self ) total += v;
    printf( "frames: %d (test_errors>=0 at frame %d, value %d) swaps=%u moved=%u\n", frame, done_frame, a_err ? (int32_t)ram( a_err ) : 0, ram( a_sw ), ram( a_mv ) );
    std::vector<std::pair<uint64_t,int>> rows;
    for( size_t i = 0; i < self.size(); i++ ) if( self[ i ] ) rows.push_back( { self[ i ], (int)i } );
    std::sort( rows.rbegin(), rows.rend() );
    int top = getenv( "TOP" ) ? atoi( getenv( "TOP" ) ) : 60;
    printf( "\nSELF (%llu cycles = %.1f frames)\n", (unsigned long long)total, total / 250000.0 );
    for( int i = 0; i < (int)rows.size() && i < top; i++ ) printf( "  %6.2f%% %9.2f fr  %s\n", 100.0 * rows[ i ].first / total, rows[ i ].first / 250000.0, names[ rows[ i ].second ].c_str() );
    rows.clear(); for( size_t i = 0; i < incl.size(); i++ ) if( incl[ i ] ) rows.push_back( { incl[ i ], (int)i } );
    std::sort( rows.rbegin(), rows.rend() );
    printf( "\nINCLUSIVE (BP-chain samples every %d cycles)\n", PERIOD );
    for( int i = 0; i < (int)rows.size() && i < top; i++ ) printf( "  %6.2f%% %9.2f fr  %s\n", 100.0 * rows[ i ].first / total, rows[ i ].first / 250000.0, names[ rows[ i ].second ].c_str() );
    { std::vector<std::pair<uint64_t,uint32_t>> u; for( auto& x : unkip ) u.push_back( { x.second, x.first } ); std::sort( u.rbegin(), u.rend() );
      printf( "\nUNKNOWN IPs\n" ); for( size_t i = 0; i < u.size() && i < 15; i++ ) printf( "  %08X %llu\n", u[ i ].second, (unsigned long long)u[ i ].first ); }
    { std::vector<std::pair<uint64_t,std::string>> c; for( auto& x : callers ) c.push_back( { x.second, x.first } ); std::sort( c.rbegin(), c.rend() );
      printf( "\nCALLER CHAINS\n" ); for( size_t i = 0; i < c.size() && i < 40; i++ ) printf( "  %7.2f fr  %s\n", c[ i ].first / 250000.0, c[ i ].second.c_str() ); }
    { std::vector<std::pair<uint64_t,std::string>> c; uint64_t t = 0; for( auto& x : subself ) { c.push_back( { x.second, x.first } ); t += x.second; } std::sort( c.rbegin(), c.rend() );
      if( t ) { printf( "\nSELF WITHIN SUBTREE %s (%.2f fr)\n", getenv( "SUBTREE" ), t / 250000.0 ); for( size_t i = 0; i < c.size() && i < 30; i++ ) printf( "  %5.1f%% %7.3f fr  %s\n", 100.0 * c[ i ].first / t, c[ i ].first / 250000.0, c[ i ].second.c_str() ); } }
    if( tl ) fclose( tl );
}
