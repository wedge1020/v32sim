// *****************************************************************************
//  tools/oracle/v32trace-ref.cpp -- the DesktopEmulator's V32TRACE recorder, headless
//
//  Builds only against a ComputerSoftware tree with GBtoV32's emulator/emulator-v32trace.patch applied (the recorder lives in
//  ConsoleLogic: V32Trace.cpp). Plays a cartridge with the input script v32prof/v32shot/v32peek take, recording a V32TRACE
//  session from power-on, and writes the recorder's own files to OUTDIR:
//      <cart>_<stamp>.trace.txt / .traps.txt / .blocks.txt  (and the _latest copies)
//
//  usage: v32trace-ref bios.v32 cart.v32 script frames outdir [--norestart]
//
//  Script: as v32prof (connect / press / release / tap / end), plus
//      <frame> mark [NOTE]        drop a marker (F8 in the emulator)
//  It is the reference v32sim's --v32trace is compared with (tests/run_harness_tests.sh). Not built by default:
//      ONLY=v32trace-ref tools/oracle/build.sh PATCHED_CHECKOUT OUT
// *****************************************************************************
#include "V32Console.hpp"
#include "ExternalInterfaces.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace V32;

static void NoColor( GPUColor ) {}
static void NoQuad( GPUQuad& ) {}
static void NoInt( int ) {}
static void NoLoad( int, void* ) {}
static void NoArgs() {}
static void Log( const std::string& ) {}
static void Throw( const std::string& s ) { throw std::runtime_error( s ); }

struct Command { int frame; std::string op; int port; std::string arg; std::string note; };

static bool control_from_name( const std::string& n, GamepadControls& c )
{
    static const std::map<std::string, GamepadControls> names = {
        { "left", GamepadControls::Left }, { "right", GamepadControls::Right },
        { "up", GamepadControls::Up }, { "down", GamepadControls::Down },
        { "start", GamepadControls::ButtonStart }, { "a", GamepadControls::ButtonA },
        { "b", GamepadControls::ButtonB }, { "x", GamepadControls::ButtonX },
        { "y", GamepadControls::ButtonY }, { "l", GamepadControls::ButtonL },
        { "r", GamepadControls::ButtonR } };
    auto it = names.find( n );
    if( it == names.end() ) return false;
    c = it->second;
    return true;
}

int main( int argc, char** argv )
{
    if( argc < 6 )
    {
        fprintf( stderr, "usage: v32trace-ref bios.v32 cart.v32 script frames outdir [--norestart]\n" );
        return 2;
    }
    Callbacks::ClearScreen = NoColor;
    Callbacks::DrawQuad = NoQuad;
    Callbacks::SetMultiplyColor = NoColor;
    Callbacks::SetBlendingMode = NoInt;
    Callbacks::SelectTexture = NoInt;
    Callbacks::LoadTexture = NoLoad;
    Callbacks::UnloadCartridgeTextures = NoArgs;
    Callbacks::UnloadBiosTexture = NoArgs;
    Callbacks::LogLine = Log;
    Callbacks::ThrowException = Throw;

    std::vector<Command> script;
    {
        std::ifstream in( argv[ 3 ] );
        std::string line;
        while( std::getline( in, line ) )
        {
            size_t hash = line.find( '#' );
            if( hash != std::string::npos ) line.erase( hash );
            std::istringstream ss( line );
            Command c{ 0, "", 0, "", "" };
            if( !( ss >> c.frame >> c.op ) ) continue;
            if( c.op == "segment" ) continue;
            if( c.op == "mark" ) { std::getline( ss, c.note ); size_t b = c.note.find_first_not_of( " \t" ); c.note = (b == std::string::npos)? "" : c.note.substr( b ); }
            else if( c.op != "end" ) ss >> c.port >> c.arg;
            script.push_back( c );
        }
    }
    int last_frame = atoi( argv[ 4 ] ) - 1;

    std::vector<std::pair<int, GamepadControls>> pending_release;
    try
    {
        V32Console console;
        console.LoadBios( argv[ 1 ] );
        console.LoadCartridge( argv[ 2 ] );
        if( getenv( "V32PEEK_CARD" ) )
        {
            const char* card = getenv( "V32PEEK_CARD" );
            FILE* existing = fopen( card, "rb" );
            if( existing ) fclose( existing );
            else console.CreateMemoryCard( card );
            console.LoadMemoryCard( card );
        }
        console.Trace.OutputFolder = std::string( argv[ 5 ] ) + "/";
        console.Trace.ContinueAfterTrap = !( argc > 6 && std::string( argv[ 6 ] ) == "--norestart" );
        console.Trace.Arm( true );
        console.SetPower( true );
        size_t next = 0;
        int frame = 0;
        for( ; frame <= last_frame && !console.IsCPUHalted(); frame++ )
        {
            for( auto& r : pending_release ) console.SetGamepadControl( r.first, r.second, false );
            pending_release.clear();
            for( ; next < script.size() && script[ next ].frame == frame; next++ )
            {
                const Command& c = script[ next ];
                GamepadControls ctl;
                if( c.op == "mark" ) console.Trace.AddMarker( c.note );
                else if( c.op == "connect" ) console.SetGamepadConnection( c.port, c.arg == "1" );
                else if( control_from_name( c.arg, ctl ) )
                {
                    if( c.op == "press" || c.op == "tap" ) console.SetGamepadControl( c.port, ctl, true );
                    if( c.op == "release" ) console.SetGamepadControl( c.port, ctl, false );
                    if( c.op == "tap" ) pending_release.push_back( { c.port, ctl } );
                }
            }
            console.RunNextFrame();
        }
        console.Trace.EndSession( "recording stopped" );
        printf( "frames=%d\n", frame );
    }
    catch( const std::exception& e )
    {
        fprintf( stderr, "v32trace-ref: %s\n", e.what() );
        return 1;
    }
    return 0;
}
