
namespace functions {

thread_local system_t* system = NULL;

void println()
    {
    ::printf( "\n" );
    system_chome( system );
    system_cdown( system );
    }

void printl( int x )
    {
    char str[ 256 ];
    ::sprintf( str, "%d\n", x );
    system_print( system, str );
    system_chome( system );
    system_cdown( system );
    ::printf( "%d\n", x );
    }

void printlf( float x )
    {
    char str[ 256 ];
    ::sprintf( str, "%f\n", x );
    system_print( system, str );
    system_chome( system );
    system_cdown( system );
    ::printf( "%f\n", x );
    }

void printlc( char const* x )
    {
    system_print( system, x );
    system_chome( system );
    system_cdown( system );
    ::printf( "%s\n", x );
    }

void printlc2( char const* a, char const* b )                   
    {
    system_print( system, a );
    system_print( system, " " );
    system_print( system, b );
    system_chome( system );
    system_cdown( system );
    ::printf( "%s\t%s\n", a, b );
    }

void printlc3( char const* a, char const* b, char const* c )
    {
    system_print( system, a );
    system_print( system, " " );
    system_print( system, b );
    system_print( system, " " );
    system_print( system, c );
    system_chome( system );
    system_cdown( system );
    ::printf( "%s\t%s\t%s\n", a, b, c );
    }

void print( int x )
    {
    char str[ 256 ];
    ::sprintf( str, "%d\n", x );
    system_print( system, str );
    ::printf( "%d", x );
    }

void printf( float x )
    {
    char str[ 256 ];
    ::sprintf( str, "%f\n", x );
    system_print( system, str );
    ::printf( "%f", x );
    }

void printc( char const* x )
    {
    system_print( system, x );
    ::printf( "%s", x );
    }

char const* str( int a )
    {
    static char temp[ 256 ];
    ::snprintf( temp, 255, "%d", a ); 
    return temp; 
    }

char const* strf( float a )
    {
    static char temp[ 256 ];
    ::snprintf( temp, 255, "%f", a ); 
    return temp; 
    }

char const* strb( bool a )
    {
    static char temp[ 256 ];
    ::snprintf( temp, 255, "%s", a ? "True" : "False" ); 
    return temp; 
    }

float rnd( int a ) { (void) a; return ( (float) rand() ) / (float)( RAND_MAX + 1 ); }
int intf( float a ) { return (int) a;  }
int ints( int a ) { return a; }
int intc( char const* a ) { return atoi( a );  }
float flt( int a ) { return (float) a; }


FILE* files[ 16 ]; // TODO: Move file handles into system_t

void fopen( int n , char const* x )
    {
    if( n < 0 || n >=16 ) return;
    struct stat st;
    if( stat( x, &st ) >= 0 )
        files[ n ] = ::fopen( x, "r+" );
    else
        files[ n ] = ::fopen( x, "w+" );
    }

void frestore( int n )
    {
    if( n < 0 || n >=16 ) return;
    fseek( files[ n ], 0, SEEK_SET );
    }

char const* fread( int n )
    {
    if( n < 0 || n >=16 ) return 0;
    static char str[ 256 ];
    fgets( str, 256, files[ n ] );
    return str; 
    }

void fwrite( int n, char const* x )
    {
    if( n < 0 || n >=16 ) return;
    fprintf( files[ n ], "%s\n", x );
    fflush( files[ n ] );
    }

int abs( int a ) { return ::abs( a ); }
int sqr( int a ) { return (int) sqrtf( (float) a ); }

char const* input() { system_input_mode( system ); return ""; }
void pen( int color ) { system_pen( system, color ); }
void paper( int color ) { system_paper( system, color ); }
void loadsong( int song, char const* filename ) { system_loadsong( system, song, filename ); }
void playsong( int song ) { system_playsong( system, song ); }
void stopsong() { system_stopsong( system ); }
void loadpalette( char const* filename ) { system_load_palette( system, filename ); }
void loadsprite( int data_index, char const* filename ) { system_load_sprite( system, data_index, filename ); }
void sprite( int spr_index, int x, int y, int data_index ) { system_sprite( system, spr_index, x, y, data_index ); }
void say( char const* text ) { system_say( system, text ); }
void spritepos( int spr_index, int x, int y ) { system_sprite( system, spr_index, x, y ); }
void waitvbl() { system_waitvbl( system ); }
void loadsound( int data_index, char const* filename ) { system_load_sound( system, data_index, filename ); }
void playsound( int sound_index, int data_index ) { system_play_sound( system, sound_index, data_index ); }


static struct { char const* signature; vm_func_t func; } host_functions[] = 
    {
    { "Proc PRINTL()", vm_proc< println > },
    { "Proc PRINTL( Integer )", vm_proc< printl, int > },
    { "Proc PRINTL( Real )", vm_proc< printlf, float > },
    { "Proc PRINTL( String )", vm_proc< printlc, char const* > },
    { "Proc PRINTL( String, String )", vm_proc< printlc2, char const*, char const* > },
    { "Proc PRINTL( String, String, String )", vm_proc< printlc3, char const*, char const*, char const* > },
    { "Proc PRINT( Integer )", vm_proc< print, int > },
    { "Proc PRINT( Real )", vm_proc< printf, float > },
    { "Proc PRINT( String )", vm_proc< printc, char const* > },
    { "Func String STR( Integer )", vm_func< char const*, str, int > },
    { "Func String STR( Real )", vm_func< char const*, strf, float > },
    { "Func String STR( Bool )", vm_func< char const*, strb, bool > },
    { "Func Real RND( Integer )", vm_func< float, rnd, float > },
    { "Func Integer INT( Real )", vm_func< int, intf, float > },
    { "Func Integer INT( Integer )", vm_func< int, ints, int > },
    { "Func Integer INT( String )", vm_func< int, intc, char const* > },
    { "Func Real FLT( Integer )", vm_func< float, flt, int > },
    { "Proc FOPEN( Integer, String )", vm_proc< fopen, int, char const* > },
    { "Proc FRESTORE( Integer )", vm_proc< frestore, int > },
    { "Func String FREAD( Integer )", vm_func< char const*, fread, int> },
    { "Proc FWRITE( Integer, String )", vm_proc< fwrite, int, char const* > },
    { "Func String INPUT()", vm_func< char const*, input > },
    { "Func Integer ABS( Integer )", vm_func< int, abs, int > },
    { "Func Integer SQR( Integer )", vm_func< int, sqr, int > },
    { "Proc PEN( Integer )", vm_proc< pen, int > },
    { "Proc PAPER( Integer )", vm_proc< paper, int > },
    { "Proc LOADSONG( Integer, String )", vm_proc< loadsong, int, char const* > },
    { "Proc PLAYSONG( Integer )", vm_proc< playsong, int > },
    { "Proc STOPSONG()", vm_proc< stopsong > },
    { "Proc LOADPALETTE( String )", vm_proc< loadpalette, char const* > },
    { "Proc LOADSPRITE( Integer, String )", vm_proc< loadsprite, int, char const* > },
    { "Proc SPRITE( Integer, Integer, Integer, Integer )", vm_proc< sprite, int, int, int, int > },
    { "Proc SPRITE( Integer, Integer, Integer )", vm_proc< spritepos, int, int, int > },
    { "Proc WAITVBL()", vm_proc< waitvbl > },
    { "Proc SAY( String )", vm_proc< say, char const* > },
    { "Proc LOADSOUND( Integer, String )", vm_proc< loadsound, int, char const* > },
    { "Proc PLAYSOUND( Integer, Integer )", vm_proc< playsound, int, int > },
    };

} /* namespace functions */