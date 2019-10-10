
namespace functions {

thread_local system_t* system = NULL;

void cdown() { system_cdown( system ); }
void cup() { system_cup( system ); }
void cleft() { system_cleft( system ); }
void cright() { system_cright( system); }
void curs_on() { system_curs_on( system ); }
void curs_off() { system_curs_off( system ); }
void set_curs( int top, int base ) { system_set_curs( system, top, base ); }
void home() { system_home( system ); }
void inverse_on() { system_inverse_on( system ); }
void inverse_off() { system_inverse_off( system ); }
void under_on() { system_under_on( system ); }
void under_off() { system_under_off( system ); }
void shade_on() { system_shade_on( system ); }
void shade_off() { system_shade_off( system ); }
void locate( int x, int y ) { system_locate( system, x, y ); }
void paper( int color ) { system_paper( system, color ); }
void pen( int color ) { system_pen( system, color ); }
void println() { system_print( system, "" ); }
void print( char const* str ) { system_print( system, str ); }
void write( char const* str ) { system_write( system, str ); }
void centre( char const* str ) { system_centre( system, str ); }
int scrn() { return system_scrn( system ); }
void square( int wx, int wy, int border ) { system_square( system, wx, wy, border ); }
char const* tab( int n ) { return system_tab( system, n ); }
void writing( int effect ) { system_writing( system, effect ); }
int xcurs() { return system_xcurs( system ); }
int ycurs() { return system_ycurs( system ); }
int xtext( int x ) { return system_xtext( system, x ); }
int ytext( int y ) { return system_ytext( system, y ); }
int xgraphic( int x ) { return system_xgraphic( system, x ); }
int ygraphic( int y ) { return system_ygraphic( system, y ); }

void loadsprite( int data_index, char const* filename ) { system_load_sprite( system, data_index, filename ); }
void sprite( int spr_index, int x, int y, int data_index ) { system_sprite( system, spr_index, x, y, data_index ); }
void spritepos( int spr_index, int x, int y ) { system_spritepos( system, spr_index, x, y ); }
void anim( int spr_index, char const* anim_string ) { system_anim( system, spr_index, anim_string ); }
void anim_on( int spr_index ) { system_anim_on( system, spr_index ); }
void anim_off( int spr_index ) { system_anim_off( system, spr_index ); }
void anim_freeze( int spr_index ) { system_anim_freeze( system, spr_index ); }
void anim_all_on() { system_anim_all_on( system ); }
void anim_all_off() { system_anim_all_off( system ); }
void anim_all_freeze() { system_anim_all_freeze( system ); }
void move_x( int spr_index, char const* move_string ) { system_move_x( system, spr_index, move_string ); }
void move_y( int spr_index, char const* move_string ) { system_move_y( system, spr_index, move_string ); }
void move_on( int spr_index ) { system_move_on( system, spr_index ); }
void move_off( int spr_index ) { system_move_off( system, spr_index ); }
void move_freeze( int spr_index ) { system_move_freeze( system, spr_index ); }
void move_all_on() { system_move_all_on( system ); }
void move_all_off() { system_move_all_off( system ); }
void move_all_freeze() { system_move_all_freeze( system ); }
int movon( int spr_index ) { return system_movon( system, spr_index ); }
void freeze() { system_freeze( system ); }
void unfreeze() { system_unfreeze( system ); }
void off() { system_off( system ); }
void put_sprite( int spr_index ) { system_put_sprite( system, spr_index ); }
void get_sprite( int x, int y, int w, int h, int data_index ) { system_get_sprite( system, x, y, w, h, data_index, 0 ); }
void get_sprite_mask( int x, int y, int w, int h, int data_index, int mask ) { system_get_sprite( system, x, y, w, h, data_index, mask ); }
void update_on() { system_update_on( system ); }
void update_off() { system_update_off( system ); }
void update() { system_update( system ); }
void priority_on() { system_priority_on( system ); }
void priority_off() { system_priority_off( system ); }
int detect( int spr_index ) { return system_detect( system, spr_index ); }
void synchro_on() { system_synchro_on( system ); }
void synchro_off() { system_synchro_off( system ); }
void synchro() { system_synchro( system ); }

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
void loadsong( int song, char const* filename ) { system_loadsong( system, song, filename ); }
void playsong( int song ) { system_playsong( system, song ); }
void stopsong() { system_stopsong( system ); }
void loadpalette( char const* filename ) { system_load_palette( system, filename ); }
void say( char const* text ) { system_say( system, text ); }
void waitvbl() { system_waitvbl( system ); }
void loadsound( int data_index, char const* filename ) { system_load_sound( system, data_index, filename ); }
void playsound( int sound_index, int data_index ) { system_play_sound( system, sound_index, data_index ); }


static struct { char const* signature; vm_func_t func; } host_functions[] = 
    {
    { "Proc CDOWN()", vm_proc< cdown > },
    { "Proc CUP()", vm_proc< cup > },
    { "Proc CLEFT()", vm_proc< cleft > },
    { "Proc CRIGHT()", vm_proc< cright > },
    { "Proc CURS_ON()", vm_proc< curs_on > },
    { "Proc CURS_OFF()", vm_proc< curs_off > },
    { "Proc SETCURS( Integer, Integer )", vm_proc< set_curs, int, int > },
    { "Proc HOME()", vm_proc< home > },
    { "Proc INVERSE_ON()", vm_proc< inverse_on > },
    { "Proc INVERSE_OFF()", vm_proc< inverse_off > },
    { "Proc UNDER_ON()", vm_proc< under_on > },
    { "Proc UNDER_OFF()", vm_proc< under_off > },
    { "Proc SHADE_ON()", vm_proc< shade_on > },
    { "Proc SHADE_OFF()", vm_proc< shade_off > },
    { "Proc LOCATE( Integer, Integer )", vm_proc< locate, int, int > },
    { "Proc PAPER( Integer )", vm_proc< paper, int > },
    { "Proc PEN( Integer )", vm_proc< pen, int > },
    { "Proc PRINT()", vm_proc< println > },
    { "Proc PRINT( String )", vm_proc< print, char const* > },
    { "Proc WRITE( String )", vm_proc< write, char const* > },
    { "Proc CENTRE( String )", vm_proc< centre, char const* > },
    { "Func Integer SCRN()", vm_func< int, scrn > },
    { "Proc SQUARE( Integer, Integer, Integer )", vm_proc< square, int, int, int > },
    { "Func String TAB( Integer )", vm_func< char const*, tab, int > },
    { "Proc WRITING( Integer )", vm_proc< writing, int > },
    { "Func Integer XCURS()", vm_func< int, xcurs > },
    { "Func Integer YCURS()", vm_func< int, ycurs > },
    { "Func Integer XTEXT( Integer )", vm_func< int, xtext, int > },
    { "Func Integer YTEXT( Integer )", vm_func< int, ytext, int > },
    { "Func Integer XGRAPHIC( Integer )", vm_func< int, xgraphic, int > },
    { "Func Integer YGRAPHIC( Integer )", vm_func< int, ygraphic, int > },
    
    { "Proc LOADSPRITE( Integer, String )", vm_proc< loadsprite, int, char const* > },
    { "Proc SPRITE( Integer, Integer, Integer, Integer )", vm_proc< sprite, int, int, int, int > },
    { "Proc SPRITE( Integer, Integer, Integer )", vm_proc< spritepos, int, int, int > },
    { "Proc ANIM( Integer, String )", vm_proc< anim, int, char const* > },
    { "Proc ANIM_ON( Integer )", vm_proc< anim_on, int > },
    { "Proc ANIM_OFF( Integer )", vm_proc< anim_off, int > },
    { "Proc ANIM_FREEZE( Integer )", vm_proc< anim_freeze, int > },
    { "Proc ANIM_ON()", vm_proc< anim_all_on > },
    { "Proc ANIM_OFF()", vm_proc< anim_all_off > },
    { "Proc ANIM_FREEZE()", vm_proc< anim_all_freeze > },
    { "Proc MOVE_X( Integer, String )", vm_proc< move_x, int, char const* > },
    { "Proc MOVE_Y( Integer, String )", vm_proc< move_y, int, char const* > },
    { "Proc MOVE_ON( Integer )", vm_proc< move_on, int > },
    { "Proc MOVE_OFF( Integer )", vm_proc< move_off, int > },
    { "Proc MOVE_FREEZE( Integer )", vm_proc< move_freeze, int > },
    { "Proc MOVE_ON()", vm_proc< move_all_on > },
    { "Proc MOVE_OFF()", vm_proc< move_all_off > },
    { "Proc MOVE_FREEZE()", vm_proc< move_all_freeze > },
    { "Func Integer MOVON( Integer )", vm_func< int, movon, int > },
    { "Proc FREEZE()", vm_proc< freeze > },
    { "Proc UNFREEZE()", vm_proc< unfreeze > },
    { "Proc OFF()", vm_proc< off > },
    { "Proc PUTSPRITE( Integer )", vm_proc< put_sprite, int > },
    { "Proc GETSPRITE( Integer, Integer, Integer, Integer, Integer )", vm_proc< get_sprite, int, int, int, int, int > },
    { "Proc GETSPRITE( Integer, Integer, Integer, Integer, Integer, Integer )", vm_proc< get_sprite_mask, int, int, int, int, int, int > },
    { "Proc UPDATE_ON()", vm_proc< update_on > },
    { "Proc UPDATE_OFF()", vm_proc< update_off > },
    { "Proc UPDATE()", vm_proc< update > },
    { "Proc PRIORITY_ON()", vm_proc< priority_on > },
    { "Proc PRIORITY_OFF()", vm_proc< priority_off > },
    { "Func Integer DETECT( Integer )", vm_func< int, detect, int > },
    { "Proc SYNCHRO_ON()", vm_proc< synchro_on > },
    { "Proc SYNCHRO_OFF()", vm_proc< synchro_off > },
    { "Proc SYNCHRO()", vm_proc< synchro > },
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
    { "Proc LOADSONG( Integer, String )", vm_proc< loadsong, int, char const* > },
    { "Proc PLAYSONG( Integer )", vm_proc< playsong, int > },
    { "Proc STOPSONG()", vm_proc< stopsong > },
    { "Proc LOADPALETTE( String )", vm_proc< loadpalette, char const* > },
    { "Proc WAITVBL()", vm_proc< waitvbl > },
    { "Proc SAY( String )", vm_proc< say, char const* > },
    { "Proc LOADSOUND( Integer, String )", vm_proc< loadsound, int, char const* > },
    { "Proc PLAYSOUND( Integer, Integer )", vm_proc< playsound, int, int > },
    };

} /* namespace functions */