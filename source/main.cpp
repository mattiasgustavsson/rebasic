#define _CRT_NONSTDC_NO_DEPRECATE 
#define _CRT_SECURE_NO_WARNINGS
#include <stddef.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <sys/stat.h>

#include "libs/app.h"
#include "libs/crtemu.h"
#include "libs/crt_frame.h"
#include "libs/file.h"
#include "libs/mid.h"
#include "libs/thread.h"
#include "libs/palettize.h"
#include "libs/stb_image.h"

#include "compile.h"
#include "vm.h"
#include "system.h"
#include "functions.h"

////////////////////////////////////////////////////////////////////////////////////////////

static int pos_to_line( int pos, char const* str )	
	{
	int line = 1;
	for( int i = 0; i < pos; ++i )
		{
		if( *str == 0 ) return line;
		if( *str == '\n' ) ++line;
		++str;
		}
	return line;
	}

void trace_callback( void* ctx, int pos )
	{
	char const* source = (char const*) ctx;
	int line = pos_to_line( pos, source );
	printf( " [%d] ", line );
	}

// opcode mappings
compile_opcode_map_t opcodes[ COMPILE_OPCOUNT ] = 
    {
    { COMPILE_OP_HALT, VM_OP_HALT }, { COMPILE_OP_TRON, VM_OP_TRON }, { COMPILE_OP_TROFF , VM_OP_TROFF },
    { COMPILE_OP_PUSH, VM_OP_PUSH }, { COMPILE_OP_POP, VM_OP_POP }, { COMPILE_OP_LOAD, VM_OP_LOAD },
    { COMPILE_OP_STORE, VM_OP_STORE }, { COMPILE_OP_PUSHC, VM_OP_PUSHC }, { COMPILE_OP_POPC, VM_OP_POPC },
    { COMPILE_OP_LOADC, VM_OP_LOADC }, { COMPILE_OP_STOREC, VM_OP_STOREC }, { COMPILE_OP_JSR, VM_OP_JSR },
    { COMPILE_OP_RET, VM_OP_RET }, { COMPILE_OP_JMP, VM_OP_JMP }, { COMPILE_OP_JNZ, VM_OP_JNZ },
    { COMPILE_OP_EQS, VM_OP_EQS }, { COMPILE_OP_NES, VM_OP_NES }, { COMPILE_OP_LES, VM_OP_LES },
    { COMPILE_OP_GES, VM_OP_GES }, { COMPILE_OP_LTS, VM_OP_LTS }, { COMPILE_OP_GTS, VM_OP_GTS },
    { COMPILE_OP_EQF, VM_OP_EQF }, { COMPILE_OP_NEF, VM_OP_NEF }, { COMPILE_OP_LEF, VM_OP_LEF },
    { COMPILE_OP_GEF, VM_OP_GEF }, { COMPILE_OP_LTF, VM_OP_LTF }, { COMPILE_OP_GTF, VM_OP_GTF },
    { COMPILE_OP_EQC, VM_OP_EQC }, { COMPILE_OP_NEC, VM_OP_NEC }, { COMPILE_OP_LEC, VM_OP_LEC },
    { COMPILE_OP_GEC, VM_OP_GEC }, { COMPILE_OP_LTC, VM_OP_LTC }, { COMPILE_OP_GTC, VM_OP_GTC },
    { COMPILE_OP_ADD, VM_OP_ADD }, { COMPILE_OP_SUB, VM_OP_SUB }, { COMPILE_OP_OR, VM_OP_OR },
    { COMPILE_OP_XOR, VM_OP_XOR }, { COMPILE_OP_MUL, VM_OP_MUL }, { COMPILE_OP_DIV, VM_OP_DIV },
    { COMPILE_OP_MOD, VM_OP_MOD }, { COMPILE_OP_AND, VM_OP_AND }, { COMPILE_OP_NEG, VM_OP_NEG },
    { COMPILE_OP_NOT, VM_OP_NOT }, { COMPILE_OP_ORB, VM_OP_ORB }, { COMPILE_OP_XORB, VM_OP_XORB },
    { COMPILE_OP_ANDB, VM_OP_ANDB }, { COMPILE_OP_NOTB, VM_OP_NOTB }, { COMPILE_OP_ADDF, VM_OP_ADDF },
    { COMPILE_OP_SUBF, VM_OP_SUBF }, { COMPILE_OP_MULF, VM_OP_MULF }, { COMPILE_OP_DIVF, VM_OP_DIVF },
    { COMPILE_OP_MODF, VM_OP_MODF }, { COMPILE_OP_NEGF, VM_OP_NEGF }, { COMPILE_OP_CATC, VM_OP_CATC },
    { COMPILE_OP_READ, VM_OP_READ }, { COMPILE_OP_READF, VM_OP_READF }, { COMPILE_OP_READC, VM_OP_READC },
    { COMPILE_OP_READB, VM_OP_READB }, { COMPILE_OP_RSTO, VM_OP_RSTO }, { COMPILE_OP_RTSS, VM_OP_RTSS },
    };

char* load_source( char const* filename )
    {
	// Load source file
	struct stat st;
	if( stat( filename, &st ) < 0 ) return NULL;

    int size = st.st_size;
	FILE* fp = fopen( filename, "r" );
	if( !fp ) return NULL;

    char* source = (char*) malloc( (size_t)( size + 1 ) );
	assert( source );
	size = (int) fread( source, 1, (size_t) size, fp );
	source[ size ] = 0;
	fclose( fp );

    return source;
    }

////////////////////////////////////////////////////////////////////////////////////////////


void sound_callback( APP_S16* sample_pairs, int sample_pairs_count, void* user_data )
    {
    system_t* system = (system_t*) user_data;
    thread_mutex_lock( &system->sound_mutex );
    if( system->current_song < 1 || system->current_song > 16 || !system->songs[ system->current_song - 1 ] ) 
        memset( sample_pairs, 0, sizeof( APP_S16 ) * sample_pairs_count * 2 );
    else    
        mid_render_short( system->songs[ system->current_song - 1 ], sample_pairs, sample_pairs_count );
    thread_mutex_unlock( &system->sound_mutex );
    }


int app_proc( app_t* app, void* user_data )
    {
    // Setup host function arrays
	int const host_func_count = sizeof( functions::host_functions ) / sizeof( *functions::host_functions );
    char const* host_func_signatures[ host_func_count ];
    vm_func_t host_funcs[ host_func_count ];
    for( int i = 0; i < host_func_count; ++i )
        {
        host_func_signatures[ i ] = functions::host_functions[ i ].signature;
        host_funcs[ i ] = functions::host_functions[ i ].func;
        }

    // Load source code
	char const* source_filename = (char const*) user_data;
    char* source = load_source( source_filename );
    if( !source )
		{
		printf( "Couldn't find the file:%s\n\n", source_filename );
		return 1;
		}

    // Compile code
	char error_msg[ 256 ] = "Unknown error.";
	int error_pos = 0;
	compile_bytecode_t byte_code = compile( source, (int) strlen( source ), opcodes, COMPILE_OPCOUNT, 
        host_func_signatures, host_func_count, &error_pos, error_msg );
	if( !byte_code.code )
		{
		printf( "Compile error at (%d): %s\n", pos_to_line( error_pos, source ), error_msg );
		free( source );
		return 1;
		}

	// Set up VM
    vm_context_t ctx;

    vm_init( &ctx, byte_code.code, byte_code.code_size, byte_code.map, byte_code.map_size, byte_code.data, 
			 byte_code.data_size, /* stack size */ 1024 * 1024, byte_code.globals_size, host_funcs, host_func_count, 
			 byte_code.strings, byte_code.string_count, trace_callback, source );

	free( byte_code.code );
	free( byte_code.map );
	free( byte_code.data );
	free( byte_code.strings );


    // Init app

    crtemu_t* crtemu = crtemu_create( NULL );
    CRTEMU_U64 start_time = app_time_count( app );

    CRT_FRAME_U32* frame = (CRT_FRAME_U32*) malloc( sizeof( CRT_FRAME_U32 ) * CRT_FRAME_WIDTH * CRT_FRAME_HEIGHT );
    crt_frame( frame );
    crtemu_frame( crtemu, frame, CRT_FRAME_WIDTH, CRT_FRAME_HEIGHT );
    free( frame );

    //app_screenmode( app, APP_SCREENMODE_WINDOW );
    app_interpolation( app, APP_INTERPOLATION_NONE );

    // Setup system
    system_init( &g_system, &ctx );
    g_system.vm = &ctx;

    app_sound( app, 8192, sound_callback, &g_system );

    char input_str[ 256 ] = "";

    // Main loop
    while( app_yield( app ) != APP_STATE_EXIT_REQUESTED && !vm_halted( &ctx ) )
        {
        // Read input
        if( g_system.input_mode )
            {
            app_input_t input = app_input( app );
            for( int i = 0; i < input.count; ++i )
                {
                app_input_event_t* event = &input.events[ i ];
                if( event->type == APP_INPUT_CHAR )
                    {
                    char char_code = event->data.char_code;
                    if( char_code == '\r' )
                        {
                        functions::println();
                        g_system.input_mode = false;
                        ret_cast<char const*> r( g_system.vm, (char const*) input_str ); 
                        g_system.vm->sp[ -1 ] = r.operator u32();
                        strcpy( input_str, "" );
                        vm_resume( g_system.vm );
                        }
                    else if( char_code == '\b' )
                        {
                        if( strlen( input_str ) > 0 )
                            {
                            input_str[ strlen( input_str ) - 1 ] = '\0'; 
                            if( g_system.cursor_x == 0 )
                                {
                                g_system.cursor_x = 39;
                                g_system.cursor_y --;
                                }
                            else
                                {
                                g_system.cursor_x-- ;
                                }

                            for( int iy = 0; iy < 8; ++iy )	
				                for( int ix = 0; ix < 8; ++ix )	
					                g_system.screen[ g_system.cursor_x * 8 + ix + ( g_system.cursor_y * 8 + iy ) * 320 ] = (uint8_t)( g_system.paper & 31 );
                            }
                        }
                    else if( strlen( input_str ) < 255 )
                        {
                        char str[ 2 ] = { char_code, '\0' };
                        strcat( input_str, str );
                        functions::printc( str );
                        }
                    }
                }
            }

        // Run VM
        APP_U64 start = app_time_count( app );
        APP_U64 end = start + app_time_freq( app ) / ( 1000 / 8 ); // Run VM for 8 ms
        while( app_time_count( app ) < end )
            if( vm_run( &ctx, 256 ) < 256 ) break; // Run VM for 256 instructions, break if it ran less

        // Update palette
        APP_U32 palette[ 32 ] = { 0 };
        for( int i = 0; i < 32; ++i )
            {
            unsigned short p = g_system.palette[ i ];
            u32 b = ( p )      & 0x7u;
            u32 g = ( p >> 4 ) & 0x7u;
            u32 r = ( p >> 8 ) & 0x7u;
			b = b * 36;
			g = g * 36;
			r = r * 36;
            palette[ i ] = ( b << 16 ) | ( g << 8 ) | r;
            }

        // Render screen
        static APP_U32 screenbuf[ 384 * 288 ] = { 0 };

        for( int y = 0; y < 288; ++y )
            for( int x = 0; x < 384; ++x )
                screenbuf[ x + y * 384 ] = palette[ 0 ];

        for( int y = 0; y < 200; ++y )
            for( int x = 0; x < 320; ++x )
                screenbuf[ ( x + 32 ) + ( y + 44 ) * 384 ] = palette[ g_system.screen[ x + y * 320 ] & 31 ];

        CRTEMU_U64 time_us = ( app_time_count( app ) - start_time ) / ( app_time_freq( app ) / 1000000 );

        if( ( time_us % 1000000 ) < 500000 )
            {
            for( int iy = 0; iy < 8; ++iy )	
			    for( int ix = 0; ix < 8; ++ix )	
				    screenbuf[ 32 + g_system.cursor_x * 8 + ix + ( 44 + g_system.cursor_y * 8 + iy ) * 384 ] = palette[ 21 ];
            }

        // Present
        crtemu_present( crtemu, time_us, screenbuf, 384, 288, 0xffffff, 0x1c1c1c );
        app_present( app, NULL, 1, 1, 0xffffff, 0x000000 );
        }

    vm_term( &ctx );    
	free( source );

    app_sound( app, 0, NULL, NULL );
    thread_mutex_lock( &g_system.sound_mutex );
    thread_mutex_unlock( &g_system.sound_mutex );

    system_term( &g_system );

    crtemu_destroy( crtemu );
    return 0;
    }



#ifndef NDEBUG
    #pragma warning( push ) 
    #pragma warning( disable: 4619 ) // pragma warning : there is no warning number 'number'
    #pragma warning( disable: 4668 ) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
    #include <crtdbg.h>
    #pragma warning( pop ) 
#endif


int main( int argc, char** argv )
    {
    (void) argc, argv;
    #ifndef NDEBUG
        int flag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG ); // Get current flag
        flag ^= _CRTDBG_LEAK_CHECK_DF; // Turn on leak-checking bit
        _CrtSetDbgFlag( flag ); // Set flag to the new value
//        _CrtSetBreakAlloc( 0 );
    #endif

	if( argc != 2 )
		{
		printf( "USAGE:\n\n\tREBASIC filename.bas\n\n");
		return 1;
		}

    return app_run( app_proc, argv[ 1 ], NULL, NULL, NULL );
    }
   

// pass-through so the program will build with either /SUBSYSTEM:WINDOWS or /SUBSYSTEN:CONSOLE
extern "C" int __stdcall WinMain( struct HINSTANCE__*, struct HINSTANCE__*, char*, int ) { return main( __argc, __argv ); }

