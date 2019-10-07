
#ifndef system_h
#define system_h

#include <stdint.h>

typedef struct system_t system_t;

system_t* system_create( vm_context_t* vm, int sound_buffer_size );
void system_destroy( system_t* system );

void system_update( system_t* system, uint64_t delta_time_us, char const* input_buffer );
uint32_t* system_render_screen( system_t* system, int* width, int* height );
void system_render_samples( system_t* system, int16_t* sample_pairs, int sample_pairs_count );

void system_input_mode( system_t* system );
void system_waitvbl( system_t* system );

void system_cdown( system_t* system );
void system_cup( system_t* system );
void system_cleft( system_t* system );
void system_cright( system_t* system );
void system_curs_on( system_t* system );
void system_curs_off( system_t* system );
void system_set_curs( system_t* system, int top, int base );
void system_home( system_t* system );
void system_inverse_on( system_t* system );
void system_inverse_off( system_t* system );
void system_under_on( system_t* system );
void system_under_off( system_t* system );
void system_shade_on( system_t* system );
void system_shade_off( system_t* system );
void system_locate( system_t* system, int x, int y );
void system_paper( system_t* system, int color );
void system_pen( system_t* system, int color );
void system_print( system_t* system, char const* str );
void system_write( system_t* system, char const* str );
void system_centre( system_t* system, char const* str );
int system_scrn( system_t* system );
void system_square( system_t* system, int wx, int wy, int border );
const char* system_tab( system_t* system, int n );
void system_writing( system_t* system, int effect ); // 1=Replace (Default)  // 2=OR  // 3=XOR  // 4=AND 
int system_xcurs( system_t* system );
int system_ycurs( system_t* system );
int system_xtext( system_t* system, int x );
int system_ytext( system_t* system, int y );
int system_xgraphic( system_t* system, int x );
int system_ygraphic( system_t* system, int y );

void system_freeze( system_t* system ); // TODO
void system_unfreeze( system_t* system ); // TODO
void system_off( system_t* system ); // TODO


void system_load_sprite( system_t* system, int sprite_data_index, char const* string );
void system_sprite( system_t* system, int spr_index, int x, int y, int data_index );
void system_spritepos( system_t* system, int spr_index, int x, int y );

void system_move_x( system_t* system, int spr_index, char const* move_str );
void system_move_y( system_t* system, int spr_index, char const* move_str );
void system_move_on( system_t* system, int spr_index );
void system_move_off( system_t* system, int spr_index );
void system_move_freeze( system_t* system, int spr_index );
void system_move_all_on( system_t* system );
void system_move_all_off( system_t* system );
void system_move_all_freeze( system_t* system );
int system_movon( system_t* system, int spr_index );

void system_anim( system_t* system, int spr_index, char const* anim_str );
void system_anim_on( system_t* system, int spr_index );
void system_anim_off( system_t* system, int spr_index );
void system_anim_freeze( system_t* system, int spr_index );
void system_anim_all_on( system_t* system );
void system_anim_all_off( system_t* system );
void system_anim_all_freeze( system_t* system );

// PUT SPRITE
// GET SPRITE
// UPDATE
// AUTOBACK
// X SPRITE 
// Y SPRITE 
// MOVON 
// COLLIDE 
// LIMIT SPRITE 
// ZONE 
// SET ZONE
// RESET ZONE
// PRIORITY
// REDRAW
// DETECT
// SYNCHRO 

void system_say( system_t* system, char const* text );
void system_loadsong( system_t* system, int index, char const* filename );
void system_playsong( system_t* system, int index );
void system_stopsong( system_t* system );
void system_load_palette( system_t* system, char const* string );
void system_load_sound( system_t* system, int data_index, char const* filename );
void system_play_sound( system_t* system, int sound_index, int data_index );

#endif /* system_h */

#ifdef SYSTEM_ÌMPLEMENTATION
#undef SYSTEM_IMPLEMENTATION

#include "libs/file.h"
#include "libs/mid.h"
#include "libs/thread.h"
#include "libs/palettize.h"
#include "libs/paldither.h"
#include "libs/stb_image.h"
#include "libs/dr_wav.h"
#include "libs/speech.hpp"

struct system_t
    {
    vm_context_t* vm;
    bool input_mode;
    bool wait_vbl;
    uint64_t time_us;
    char input_str[ 256 ];

    bool show_cursor;
    int cursor_top;
    int cursor_base;
    int cursor_x;
    int cursor_y;
    int pen;
    int paper;
    int write_mode;
    bool text_inverse;
    bool text_underline;
    bool text_shaded;

    paldither_palette_t* paldither;
    uint16_t palette[ 32 ];
    uint8_t screen[ 320 * 200 ]; // the screen, which we draw to  
    uint8_t final_screen[ 320 * 200 ]; // final composite of screen with cursor and sprites rendered on top of the screen contents
    uint32_t out_screen_xbgr[ 384 * 288 ]; // XBGR 32-bit de-palettized screen with borders added
    uint8_t charmap[ 40 * 25 ];

    struct sprite_t
        {
        int x;
        int y;
        int data;

        struct anim_t
            {
            struct frames_t
                {
                int data;
                int delay;
                } frames[ 256 ];
            int frame_count;
            int index;
            int timer;
            bool loop;
            bool running;
            } anim;

        struct move_t
            {
            struct frames_t
                {
                int speed;
                int step;
                int count;
                } frames[ 256 ];
            int frame_count;
            int index;
            int timer;
            int count;
            bool loop;            
            bool pos_used;   
            int pos_value;
            } move_x, move_y;
        bool moving;

        } sprites[ 32 ];

    struct sprite_data_t
        {
        uint8_t* pixels;
        int width;
        int height;
        } sprite_data[ 4096 ];

    int sound_buffer_size;
    int16_t* mix_buffers;

    thread_mutex_t song_mutex;
    mid_t* songs[ 16 ];
    int current_song;


    thread_mutex_t sound_mutex;

    struct sound_t
        {
        int data;
        int pos;
        } sounds[ 4 ];

    struct sound_data_t
        {
        int16_t* sample_pairs;
        int sample_pairs_count;
        } sound_data[ 32 ];

    thread_mutex_t speech_mutex;
    thread_ptr_t speech_thread;
    thread_signal_t speech_signal;
    thread_atomic_int_t speech_exit;
    char* speech_text;
    short* speech_sample_pairs;
    int speech_sample_pairs_count;
    int speech_sample_pairs_pos;
    };


extern unsigned long long default_font[ 256 ];
extern unsigned short default_palette[ 32 ];
extern unsigned char soundfont[ 1093878 ];


int speech_thread( void* user_data )
    {
    system_t* system = (system_t*) user_data;
    while( !thread_atomic_int_load( &system->speech_exit ) )
        {
        if( thread_signal_wait( &system->speech_signal, 1000 ) )
            {
            thread_mutex_lock( &system->speech_mutex );
            short* sample_pairs = system->speech_sample_pairs;
            char* speech_text = system->speech_text ? strdup( system->speech_text ) : NULL;
            thread_mutex_unlock( &system->speech_mutex );
            if( !sample_pairs && speech_text )
                {
                int sample_pairs_count = 0;
                sample_pairs = speech_gen( &sample_pairs_count, speech_text, NULL );
                if( sample_pairs && sample_pairs_count > 0 )
                    {
                    thread_mutex_lock( &system->speech_mutex );
                    system->speech_sample_pairs = sample_pairs;
                    system->speech_sample_pairs_count = sample_pairs_count;
                    thread_mutex_unlock( &system->speech_mutex );
                    }
                }
            if( speech_text ) free( speech_text );
            }
        }
    return 0;
    }


system_t* system_create( vm_context_t* vm, int sound_buffer_size )
    {
    size_t sz = sizeof( system_t ); (void) sz;
    system_t* system = (system_t*) malloc( sizeof( system_t ) );
    memset( system, 0, sizeof( *system ) );
    system->vm = vm;
    system->show_cursor = true;
    system->cursor_top = 1;
    system->cursor_base = 8;
    system->pen = 21;
    system->paper = 0;
    system->write_mode = 1;
    system->current_song = 0;
    memcpy( system->palette, default_palette, sizeof( system->palette ) );
    memset( system->charmap, ' ', sizeof( system->charmap ) );
    
    system->sound_buffer_size = sound_buffer_size;
    system->mix_buffers = (int16_t*) malloc( sizeof( int16_t ) * sound_buffer_size * 2 * 6 ); // 6 buffers (song, speech + 4 sounds)

    thread_mutex_init( &system->song_mutex );
    thread_mutex_init( &system->speech_mutex );
    thread_mutex_init( &system->sound_mutex );
    thread_signal_init( &system->speech_signal );
    thread_atomic_int_store( &system->speech_exit, 0 );
    system->speech_thread = thread_create( speech_thread, system, NULL, THREAD_STACK_SIZE_DEFAULT );
    return system;
    }


void system_destroy( system_t* system )
    {
    thread_mutex_lock( &system->song_mutex );
    system->current_song = 0;
    for( int i = 0 ; i < sizeof( system->songs ) / sizeof( *system->songs ); ++i )
        if( system->songs[ i ] ) mid_destroy( system->songs[ i ] );
    thread_mutex_unlock( &system->song_mutex );
    thread_mutex_term( &system->song_mutex );
        
    thread_mutex_term( &system->sound_mutex );
    
    thread_atomic_int_store( &system->speech_exit, 1 );
    thread_signal_raise( &system->speech_signal );
    thread_join( system->speech_thread );
    thread_destroy( system->speech_thread );
    thread_mutex_term( &system->speech_mutex );
    thread_signal_term( &system->speech_signal );
    if( system->speech_sample_pairs ) free( system->speech_sample_pairs );
    if( system->speech_text ) free( system->speech_text );
   
    free( system->mix_buffers );

    for( int i = 0; i < sizeof( system->sprite_data ) /  sizeof( *system->sprite_data ); ++i )
        if( system->sprite_data[ i ].pixels )
            free( system->sprite_data[ i ].pixels );

    for( int i = 0; i < sizeof( system->sound_data ) /  sizeof( *system->sound_data ); ++i )
        if( system->sound_data[ i ].sample_pairs )
        free( system->sound_data[ i ].sample_pairs );

    if( system->paldither ) paldither_palette_destroy( system->paldither );

    free( system );
    }


void system_update( system_t* system, uint64_t delta_time_us, char const* input_buffer )
    {
    system->time_us += delta_time_us;

    // Resume VM at vertical blank refresh (if waiting for vertical blank)
    if( system->wait_vbl )
        {
        system->wait_vbl = false;
        if( !system->input_mode )
            vm_resume( system->vm );
        }

    // Read char input
    if( system->input_mode )
        {      
        for( char char_code = *input_buffer; char_code != '\0'; char_code = *++input_buffer )
            {
            if( char_code == '\r' )
                {
                system->cursor_x = 0;
                system_cdown( system );
                system->input_mode = false;
                ret_cast<char const*> r( system->vm, (char const*) system->input_str ); 
                system->vm->sp[ -1 ] = r.operator u32();
                strcpy( system->input_str, "" );
                if( !system->wait_vbl )
                    vm_resume( system->vm );
                }
            else if( char_code == '\b' )
                {
                if( strlen( system->input_str ) > 0 )
                    {
                    system->input_str[ strlen( system->input_str ) - 1 ] = '\0'; 
                    if( system->cursor_x == 0 )
                        {
                        system->cursor_x = 39;
                        system->cursor_y --;
                        }
                    else
                        {
                        system->cursor_x-- ;
                        }

                    for( int iy = 0; iy < 8; ++iy ) 
                        for( int ix = 0; ix < 8; ++ix ) 
                            system->screen[ system->cursor_x * 8 + ix + ( system->cursor_y * 8 + iy ) * 320 ] = (uint8_t)( system->paper & 31 );
                    }
                }
            else if( strlen( system->input_str ) < 255 )
                {
                char str[ 2 ] = { char_code, '\0' };
                strcat( system->input_str, str );
                system_write( system, str );
                }
            }
        }

    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        // Animate sprites
        system_t::sprite_t* spr = &system->sprites[ i ];
        system_t::sprite_t::anim_t* anim = &spr->anim;
        if( anim->frame_count && anim->running )
            {
            --anim->timer;
            if( anim->timer <= 0 )
                {
                ++anim->index;
                if( anim->index >= anim->frame_count )
                    {
                    if( !anim->loop ) 
                        {
                        anim->frame_count = 0;
                        continue;
                        }
                    anim->index = 0;
                    }
                spr->data = anim->frames[ anim->index ].data;
                anim->timer = anim->frames[ anim->index ].delay;
                }
            }

        // Update X movement
        system_t::sprite_t::move_t* move_x = &spr->move_x;
        if( move_x->frame_count && spr->moving )
            {
            --move_x->timer;
            if( move_x->timer <= 0 )
                {
                int prev_x = spr->x;
                spr->x += move_x->frames[ move_x->index ].step;
                if( move_x->pos_used )
                    {
                    if( ( prev_x < move_x->pos_value && spr->x >= move_x->pos_value ) || ( prev_x > move_x->pos_value && spr->x <= move_x->pos_value ) )
                        {
                        if( !move_x->loop ) 
                            {
                            move_x->frame_count = 0;
                            continue;
                            }
                        move_x->index = 0;
                        move_x->timer = move_x->frames[ move_x->index ].speed;
                        move_x->count = move_x->frames[ move_x->index ].count;
                        continue;
                        }
                    }
                move_x->timer = move_x->frames[ move_x->index ].speed;
                if( move_x->count )
                    {
                    --move_x->count;
                    if( move_x->count <= 0 )
                        {
                        ++move_x->index;
                        if( move_x->index >= move_x->frame_count )
                            {
                            if( !move_x->loop ) 
                                {
                                move_x->frame_count = 0;
                                continue;
                                }
                            move_x->index = 0;
                            }
                        move_x->timer = move_x->frames[ move_x->index ].speed;
                        move_x->count = move_x->frames[ move_x->index ].count;
                        }
                    }
                }
            }

        // Update Y movement
        system_t::sprite_t::move_t* move_y = &spr->move_y;
        if( move_y->frame_count && spr->moving )
            {
            --move_y->timer;
            if( move_y->timer <= 0 )
                {
                int prev_y = spr->y;
                spr->y += move_y->frames[ move_y->index ].step;
                if( move_y->pos_used )
                    {
                    if( ( prev_y < move_y->pos_value && spr->y >= move_y->pos_value ) || ( prev_y > move_y->pos_value && spr->y <= move_y->pos_value ) )
                        {
                        if( !move_y->loop ) 
                            {
                            move_y->frame_count = 0;
                            continue;
                            }
                        move_y->index = 0;
                        move_y->timer = move_y->frames[ move_y->index ].speed;
                        move_y->count = move_y->frames[ move_y->index ].count;
                        continue;
                        }
                    }
                move_y->timer = move_y->frames[ move_y->index ].speed;
                if( move_y->count )
                    {
                    --move_y->count;
                    if( move_y->count <= 0 )
                        {
                        ++move_y->index;
                        if( move_y->index >= move_y->frame_count )
                            {
                            if( !move_y->loop ) 
                                {
                                move_y->frame_count = 0;
                                continue;
                                }
                            move_y->index = 0;
                            }
                        move_y->timer = move_y->frames[ move_y->index ].speed;
                        move_y->count = move_y->frames[ move_y->index ].count;
                        }
                    }
                }
            }
        }
    }


uint32_t* system_render_screen( system_t* system, int* width, int* height )
    {
    // Convert palette
    APP_U32 palette[ 32 ] = { 0 };
    for( int i = 0; i < 32; ++i )
        {
        unsigned short p = system->palette[ i ];
        u32 b = ( p )      & 0x7u;
        u32 g = ( p >> 4 ) & 0x7u;
        u32 r = ( p >> 8 ) & 0x7u;
        b = b * 36;
        g = g * 36;
        r = r * 36;
        palette[ i ] = ( b << 16 ) | ( g << 8 ) | r;
        }

    // Make a copy of the screen so we can draw cursor and sprites on top of it
    memcpy( system->final_screen, system->screen, sizeof( system->screen ) );
    
    // Draw cursor
    if( system->show_cursor && ( system->time_us % 1000000 ) < 500000 )
        {
        for( int iy = 0; iy < 8; ++iy ) 
            for( int ix = 0; ix < 8; ++ix )
                if( iy + 1 >= system->cursor_top && iy + 1 <= system->cursor_base )
                    system->final_screen[ system->cursor_x * 8 + ix + ( system->cursor_y * 8 + iy ) * 320 ] = (uint8_t)( system->pen & 31 );
        }

    // Draw sprites
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        system_t::sprite_t* spr = &system->sprites[ i ];
        int data_index = spr->data;
        if( data_index < 1 || data_index > sizeof( system->sprite_data ) /  sizeof( *system->sprite_data ) ) continue;
        --data_index;
        if( !system->sprite_data[ data_index ].pixels ) continue;
        system_t::sprite_data_t* data = &system->sprite_data[ data_index ];
        for( int y = 0; y < data->height; ++y )
            {
            for( int x = 0; x < data->width; ++x )
                {
                uint8_t p = data->pixels[ x + y * data->width ];
                if( ( p & 0x80 ) == 0 )
                    {
                    int xp = spr->x + x;
                    int yp = spr->y + y;
                    if( xp >= 0 && yp >= 0 && xp < 320 && yp < 200 )
                        system->final_screen[ xp + yp * 320 ] = p  & 31u;                    
                    }
                }
            }
        }

    // Render screen
    for( int y = 0; y < 288; ++y )
        for( int x = 0; x < 384; ++x )
            system->out_screen_xbgr[ x + y * 384 ] = palette[ 0 ];

    for( int y = 0; y < 200; ++y )
        for( int x = 0; x < 320; ++x )
            system->out_screen_xbgr[ ( x + 32 ) + ( y + 44 ) * 384 ] = palette[ system->final_screen[ x + y * 320 ] & 31 ];

    *width = 384;
    *height = 288;
    return system->out_screen_xbgr;
    }


void system_render_samples( system_t* system, int16_t* sample_pairs, int sample_pairs_count )
    {
    // render midi song to local buffer
    int16_t* song = system->mix_buffers;
    thread_mutex_lock( &system->song_mutex );
    if( system->current_song < 1 || system->current_song > 16 || !system->songs[ system->current_song - 1 ] ) 
        memset( song, 0, sizeof( int16_t ) * sample_pairs_count * 2 );
    else    
        mid_render_short( system->songs[ system->current_song - 1 ], song, sample_pairs_count );
    thread_mutex_unlock( &system->song_mutex );

    // render speech to local buffer
    int16_t* speech = song + system->sound_buffer_size * 2;
    thread_mutex_lock( &system->speech_mutex );
    if( !system->speech_sample_pairs ) 
        {
        memset( speech, 0, sizeof( int16_t ) * sample_pairs_count * 2 );
        }
    else    
        {
        int count = system->speech_sample_pairs_count - system->speech_sample_pairs_pos;
        if( count > sample_pairs_count ) count = sample_pairs_count;
        memcpy( speech, system->speech_sample_pairs + system->speech_sample_pairs_pos * 2, sizeof( int16_t ) * count * 2 );
        system->speech_sample_pairs_pos += count;
        }
    thread_mutex_unlock( &system->speech_mutex );

    // render all sound channels to local buffers
    int const sounds_count = sizeof( system->sounds ) / sizeof( *system->sounds );
    int16_t* sound = speech + system->sound_buffer_size * 2;
    thread_mutex_lock( &system->sound_mutex );
    for( int i = 0; i < sounds_count; ++i )
        {
        int16_t* soundbuf = sound + system->sound_buffer_size * 2 * i;
        if( system->sounds[ i ].data < 1 || system->sounds[ i ].data > sounds_count || !system->sound_data[ system->sounds[ i ].data - 1 ].sample_pairs ) 
            {           
            memset( soundbuf, 0, sizeof( int16_t ) * sample_pairs_count * 2 );
            }
        else    
            {
            int data_index = system->sounds[ i ].data - 1;
            int count = system->sound_data[ data_index ].sample_pairs_count - system->sounds[ i ].pos;
            if( count <= 0 )
                {
                memset( soundbuf, 0, sizeof( int16_t ) * sample_pairs_count * 2 );
                }
            else
                {
                if( count > sample_pairs_count ) count = sample_pairs_count;
                memcpy( soundbuf, system->sound_data[ data_index ].sample_pairs + system->sounds[ i ].pos * 2, sizeof( int16_t ) * count * 2 );
                system->sounds[ i ].pos += count;
                }
            }
        }
    thread_mutex_unlock( &system->sound_mutex );

    // mix all local buffers
    for( int i = 0; i < sample_pairs_count * 2; ++i )
        {
        int sample = song[ i ] * 3 + speech[ i ] + speech[ i ] / 2;
        for( int j = 0; j < sounds_count; ++j )
            sample += sound[ system->sound_buffer_size * 2 * j + i ];
        sample = sample > 32767 ? 32767 : sample < -32727 ? -32727 : sample;
        sample_pairs[ i ] = (int16_t) sample;
        }
    }


void system_input_mode( system_t* system )
    {
    vm_pause( system->vm );
    system->input_mode = true;
    }
    

void system_waitvbl( system_t* system )
    {
    vm_pause( system->vm );
    system->wait_vbl = true;
    }



void system_cdown( system_t* system )
    {
    if( system->cursor_y < 24 )
        {
        system->cursor_y++;
        }
    else
        {
        for( int i = 0; i < 40 * 24; ++i ) system->charmap[ i ] = system->charmap[ i + 40 ];
        for( int i = 40 * 24; i < 40 * 25; ++i ) system->charmap[ i ] = (uint8_t) ' ';
        for( int i = 0; i < 320 * 192; ++i ) system->screen[ i ] = system->screen[ i + 320 * 8 ];
        for( int i = 320 * 192; i < 320 * 200; ++i ) system->screen[ i ] = (uint8_t)( system->paper & 31 );
        }
    }


void system_cup( system_t* system )
    {
    if( system->cursor_y > 0 )
        {
        system->cursor_y--;
        }
    }


void system_cleft( system_t* system )
    {
    if( system->cursor_x > 0 )
        {
        system->cursor_x--;
        }
    else if( system->cursor_y > 0 )
        {
        system->cursor_y--;
        system->cursor_x = 39;
        }
    }


void system_cright( system_t* system )
    {
    if( system->cursor_x < 39 )
        {
        system->cursor_x++;
        }
    else
        {
        system_cdown( system );
        system->cursor_x = 0;
        }
   }


void system_curs_on( system_t* system )
    {
    system->show_cursor = true;
    }


void system_curs_off( system_t* system )
    {
    system->show_cursor = false;
    }


void system_set_curs( system_t* system, int top, int base )
    {
    system->cursor_top = top < 1 ? 1 : top > 8 ? 8 : top;
    system->cursor_base = base < 1 ? 1 : base > 8 ? 8 : base;
    }


void system_home( system_t* system )
    {
    system->cursor_x = 0;
    system->cursor_y = 0;
    }


void system_inverse_on( system_t* system )
    {
    system->text_inverse = true;
    }


void system_inverse_off( system_t* system )
    {
    system->text_inverse = false;
    }


void system_under_on( system_t* system )
    {
    system->text_underline = true;
    }


void system_under_off( system_t* system )
    {
    system->text_underline = false;
    }


void system_shade_on( system_t* system )
    {
    system->text_shaded = true;
    }


void system_shade_off( system_t* system )
    {
    system->text_shaded = false;
    }


void system_locate( system_t* system, int x, int y )
    {
    system->cursor_x = x < 0 ? 0 : x > 39 ? 39 : x;
    system->cursor_y = y < 0 ? 0 : y > 24 ? 24 : y;
    }


void system_paper( system_t* system, int color )
    {
    system->paper = color & 31;
    }


void system_pen( system_t* system, int color )
    {
    system->pen = color & 31;
    }


void system_print( system_t* system, char const* str )
    {
    system_write( system, str );
    system_locate( system, 0, system_ycurs( system ) );
    system_cdown( system );
    }


void system_write_char( system_t* system, uint8_t c )
    {
    system->charmap[ system->cursor_x + system->cursor_y * 40 ] = c;

    uint8_t pen = system->text_inverse ? (uint8_t)( system->paper & 31 ) : (uint8_t)( system->pen & 31 );
    uint8_t paper = system->text_inverse ? (uint8_t)( system->pen & 31 ) : (uint8_t)( system->paper & 31 );

    int x = system->cursor_x * 8;
    int y = system->cursor_y * 8;
    unsigned long long chr = default_font[ c ];
    for( int iy = 0; iy < 8; ++iy ) 
        {
        for( int ix = 0; ix < 8; ++ix ) 
            {
            uint8_t col;
            if( chr & ( 1ull << ( ix + iy * 8 ) ) && ( !system->text_shaded || ( ix + iy ) & 1 ) || ( system->text_underline && iy == 7 ) )
                col = pen;
            else
                col = paper;
            switch( system->write_mode )
                {
                case 1: system->screen[ x + ix + ( y + iy ) * 320 ] = col; break;
                case 2: system->screen[ x + ix + ( y + iy ) * 320 ] |= col; break;
                case 3: system->screen[ x + ix + ( y + iy ) * 320 ] ^= col; break;
                case 4: system->screen[ x + ix + ( y + iy ) * 320 ] &= col; break;
                }
            }
        }
    }


void system_write( system_t* system, char const* str )
    {
    for( char const* c = str; *c != 0; ++c )
        {
        system_write_char( system, (uint8_t) *c );
        system->cursor_x++;
        if( system->cursor_x >= 40 ) 
            { 
            system->cursor_x = 0; 
            system_cdown( system ); 
            }
        }
    }


void system_centre( system_t* system, char const* str )
    {
    int len = (int) strlen( str );
    if( len >= 40 )
        {
        system_locate( system, 0, system_ycurs( system ) );
        system_print( system, str );
        }
    else
        {
        int x = ( 40 - len ) / 2;
        system_locate( system, x, system_ycurs( system ) );
        system_print( system, str );
        }
    }


int system_scrn( system_t* system )
    {
    return system->charmap[ system->cursor_x + system->cursor_y * 40 ];
    }


void system_square( system_t* system, int wx, int wy, int border )
    {
    char const* tl[] = { "\xDA", "\xC9", "\xD5", "\xD6", "\xDB", };
    char const* t [] = { "\xC4", "\xCD", "\xCD", "\xC4", "\xDB", };
    char const* tr[] = { "\xBF", "\xBB", "\xB8", "\xB7", "\xDB", };
    char const* l [] = { "\xB3", "\xBA", "\xB3", "\xBA", "\xDB", };
    char const* r [] = { "\xB3", "\xBA", "\xB3", "\xBA", "\xDB", };
    char const* bl[] = { "\xC0", "\xC8", "\xD4", "\xD3", "\xDB", };
    char const* b [] = { "\xC4", "\xCD", "\xCD", "\xC4", "\xDB", };
    char const* br[] = { "\xD9", "\xBC", "\xBE", "\xBD", "\xDB", };

    if( border < 0 || border > 4 ) return;

    if( system->cursor_x + wx >= 40 ) wx = 40 - system->cursor_x;
    if( system->cursor_y + wy >= 25 ) wy = 25 - system->cursor_y;

    if( wx <= 2 || wy <= 2 ) return;

    int x = system->cursor_x;
    int y = system->cursor_y;

    system_locate( system, x, y );
    system_write( system, tl[ border ] );
    for( int i = 0; i < wx - 2; ++i ) system_write( system, t[ border ] );
    system_write( system, tr[ border ] );

    for( int j = 0; j < wy - 2; ++j )
        {
        system_locate( system, x, y + 1 + j );
        system_write( system, l[ border ] );
        for( int i = 0; i < wx - 2; ++i )
            system_write( system, " " );
        system_write( system, r[ border ] );
        }

    system_locate( system, x, y + wy - 1 );
    system_write( system, bl[ border ] );
    for( int i = 0; i < wx - 2; ++i ) system_write( system, b[ border ] );
    system_write_char( system, (uint8_t) *br[ border ] );

    system_locate( system, x + 1, y + 1 );
    }


char const* system_tab( system_t* system, int n )
    {
    (void) system;
    static char str[ 257 ];
    if( n > 255 ) n = 255;
    for( int i = 0; i < n; ++i ) str[ i ] = ' ';
    str[ n ] = '\0';
    return str;
    }


void system_writing( system_t* system, int effect )
    {
    if( effect >= 1 && effect <= 4 )
        system->write_mode = effect;
    }


int system_xcurs( system_t* system )
    {
    return system->cursor_x;
    }


int system_ycurs( system_t* system )
    {
    return system->cursor_y;
    }


int system_xtext( system_t* system, int x )
    {
    (void) system;
    return x / 8;
    }


int system_ytext( system_t* system, int y )
    {
    (void) system;
    return y / 8;
    }


int system_xgraphic( system_t* system, int x )
    {
    (void) system;
    return x * 8;
    }


int system_ygraphic( system_t* system, int y )
    {
    (void) system;
    return y * 8;
    }



void system_load_sprite( system_t* system, int sprite_data_index, char const* string )
    {
    if( sprite_data_index < 1 || sprite_data_index > sizeof( system->sprite_data ) /  sizeof( *system->sprite_data ) )
        return;

    --sprite_data_index;

    if( system->sprite_data[ sprite_data_index ].pixels )
        {
        free( system->sprite_data[ sprite_data_index ].pixels );
        system->sprite_data[ sprite_data_index ].pixels = NULL;
        system->sprite_data[ sprite_data_index ].width = 0;
        system->sprite_data[ sprite_data_index ].height = 0;
        }

    int w, h, c;
    stbi_uc* img = stbi_load( string, &w, &h, &c, 4 );
    if( !img ) return;

    if( !system->paldither  )
        {
        u32 palette[ 32 ];
        for( int i = 0; i < 32; ++i )
            {
            unsigned short p = system->palette[ i ];
            u32 b = ( p )      & 0x7u;
            u32 g = ( p >> 4 ) & 0x7u;
            u32 r = ( p >> 8 ) & 0x7u;
            b = b * 36;
            g = g * 36;
            r = r * 36;
            palette[ i ] = ( b << 16 ) | ( g << 8 ) | r;
            }

        size_t size = 0;
        system->paldither = paldither_palette_create( palette, 32, &size, NULL );
        }
    

    system->sprite_data[ sprite_data_index ].width = w;
    system->sprite_data[ sprite_data_index ].height = h;
    system->sprite_data[ sprite_data_index ].pixels = (uint8_t*) malloc( (size_t) w * h );
    memset( system->sprite_data[ sprite_data_index ].pixels, 0, (size_t) w * h ); 
    paldither_palettize( (PALDITHER_U32*) img, w, h, system->paldither, PALDITHER_TYPE_NONE, system->sprite_data[ sprite_data_index ].pixels );
    
    for( int i = 0; i < w * h; ++i )
        if( ( ( (PALDITHER_U32*) img )[ i ] & 0xff000000 ) >> 24 < 0x80 )
            system->sprite_data[ sprite_data_index ].pixels[ i ] |=  0x80u;       
    stbi_image_free( img );     
    }


void system_sprite( system_t* system, int spr_index, int x, int y, int data_index )
    {
    if( data_index < 1 || data_index > sizeof( system->sprite_data ) /  sizeof( *system->sprite_data ) ) return;
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    if( !system->sprite_data[ data_index - 1 ].pixels ) return;
    --spr_index;
    system->sprites[ spr_index ].data = data_index;
    system->sprites[ spr_index ].x = x;
    system->sprites[ spr_index ].y = y;
    }


void system_spritepos( system_t* system, int spr_index, int x, int y )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;
    system->sprites[ spr_index ].x = x;
    system->sprites[ spr_index ].y = y;
    }


void system_anim( system_t* system, int spr_index, char const* anim_str )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system_t::sprite_t::anim_t* anim = &system->sprites[ spr_index ].anim;  
    anim->frame_count = 0;

    bool loop = false;
    int count = 0;
    char const* str = anim_str;
    while( *str )
        {
        if( *str == 'L' )
            {
            loop = true;
            break;
            }
        else if( *str == '(' )
            {
            ++str;

            char framestr[ 16 ] = "";
            while( *str && *str != ',' )
                {
                if( *str >= '0' && *str <= '9' ) 
                    strncat( framestr, str, 1);
                else
                    return; // Error in anim string
                if( strlen( framestr ) >= sizeof( framestr ) - 1 ) return; // Error in anim string
                ++str;
                }
            if( *str != ',' ) return; // Error in anim string
            ++str;

            char delaystr[ 16 ] = "";
            while( *str && *str != ')' )
                {
                if( *str >= '0' && *str <= '9' ) 
                    strncat( delaystr, str, 1);
                else
                    return; // Error in anim string
                if( strlen( delaystr ) >= sizeof( delaystr ) - 1 ) return; // Error in anim string
                ++str;
                }
            if( *str != ')' ) return; // Error in anim string
            ++str;

            int frame = atoi( framestr );
            int delay = atoi( delaystr );
            if( frame < 1 || frame > sizeof( system->sprite_data ) / sizeof( *system->sprite_data ) ) return; // Error in anim string
            if( delay < 1 || delay > 16384 ) return; // Error in anim string
            anim->frames[ count ].data = frame;
            anim->frames[ count ].delay = delay;
            ++count;
            }
        else
            return; // Error in anim string
        }

    if( count > 0 )
        {
        anim->frame_count = count;
        anim->index = 0;
        anim->timer = anim->frames[ 0 ].delay;
        anim->loop = loop;
        system->sprites[ spr_index ].data = anim->frames[ 0 ].data;
        }
    }


void system_anim_on( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system_t::sprite_t::anim_t* anim = &system->sprites[ spr_index ].anim;  
    anim->running = true;
    }


void system_anim_off( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system_t::sprite_t::anim_t* anim = &system->sprites[ spr_index ].anim;  
    anim->running = false;
    anim->frame_count = 0;
    }


void system_anim_freeze( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system_t::sprite_t::anim_t* anim = &system->sprites[ spr_index ].anim;  
    anim->running = false;
    }


void system_anim_all_on( system_t* system )
    {
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        system->sprites[ i ].anim.running = true;  
    }


void system_anim_all_off( system_t* system )
    {
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        system->sprites[ i ].anim.running = false;  
        system->sprites[ i ].anim.frame_count = 0;
        }
    }


void system_anim_all_freeze( system_t* system )
    {
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        system->sprites[ i ].anim.running = false;  
    }



void system_move( system_t* system, bool horizontal, int spr_index, char const* move_str )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system_t::sprite_t::move_t* move = horizontal ? &system->sprites[ spr_index ].move_x : &system->sprites[ spr_index ].move_y ;  
    move->frame_count = 0;

    bool loop = false;
    bool end = false;
    int frame_count = 0;
    char const* str = move_str;
    while( *str )
        {
        if( *str == 'L' )
            {
            loop = true;
            ++str;
            break;
            }
        else if( *str == 'E' )
            {
            end = true;
            ++str;
            break;
            }
        else if( *str == '(' )
            {
            ++str;

            char speedstr[ 16 ] = "";
            while( *str && *str != ',' )
                {
                if( *str >= '0' && *str <= '9' ) 
                    strncat( speedstr, str, 1);
                else
                    return; // Error in move string
                if( strlen( speedstr ) >= sizeof( speedstr ) - 1 ) return; // Error in move string
                ++str;
                }
            if( *str != ',' ) return; // Error in move string
            ++str;

            char stepstr[ 16 ] = "";
            if( *str == '-' ) strncat( stepstr, str++, 1 );
            while( *str && *str != ',' )
                {
                if( *str >= '0' && *str <= '9' ) 
                    strncat( stepstr, str, 1);
                else
                    return; // Error in move string
                if( strlen( stepstr ) >= sizeof( stepstr ) - 1 ) return; // Error in move string
                ++str;
                }
            if( *str != ',' ) return; // Error in move string
            ++str;

            char countstr[ 16 ] = "";
            while( *str && *str != ')' )
                {
                if( *str >= '0' && *str <= '9' ) 
                    strncat( countstr, str, 1);
                else
                    return; // Error in move string
                if( strlen( countstr ) >= sizeof( countstr ) - 1 ) return; // Error in move string
                ++str;
                }
            if( *str != ')' ) return; // Error in move string
            ++str;

            int speed = atoi( speedstr );
            int step = atoi( stepstr );
            int count = atoi( countstr );
            if( speed < 1 || speed > 32768 ) return; // Error in move string
            if( step < -16384 || step > 16384 ) return; // Error in move string
            if( count < 0 || count > 32767 ) return; // Error in move string
            move->frames[ frame_count ].speed = speed;
            move->frames[ frame_count ].step = step;
            move->frames[ frame_count ].count = count;
            ++frame_count;
            }
        else
            return; // Error in move string
        }

        bool pos_used = false;
        int pos_value = 0;
        if( ( loop || end ) && *str )
            {
            char posstr[ 16 ] = "";
            if( *str == '-' ) strncat( posstr, str++, 1 );
            while( *str )
                {
                if( *str >= '0' && *str <= '9' ) 
                    strncat( posstr, str, 1);
                else
                    return; // Error in move string
                if( strlen( posstr ) >= sizeof( posstr ) - 1 ) return; // Error in move string
                ++str;
                }
            pos_value = atoi( posstr );
            pos_used = true;
            }
        
    if( frame_count > 0 )
        {
        move->frame_count = frame_count;
        move->index = 0;
        move->timer = move->frames[ 0 ].speed;
        move->count = move->frames[ 0 ].count;
        move->loop = loop;
        move->pos_used = pos_used;
        move->pos_value = pos_value;
        }
    }


void system_move_x( system_t* system, int spr_index, char const* move_str )
    {
    system_move( system, true, spr_index, move_str );
    }


void system_move_y( system_t* system, int spr_index, char const* move_str )
    {
    system_move( system, false, spr_index, move_str );
    }


void system_move_on( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system->sprites[ spr_index ].moving = true;  
    }


void system_move_off( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system->sprites[ spr_index ].moving = false;  
    system->sprites[ spr_index ].move_x.count = 0;  
    system->sprites[ spr_index ].move_y.count = 0;  
    }


void system_move_freeze( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system->sprites[ spr_index ].moving = false;  
    }


void system_move_all_on( system_t* system )
    {
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        system->sprites[ i ].moving = true;  
    }


void system_move_all_off( system_t* system )
    {
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        system->sprites[ i ].moving = false;  
        system->sprites[ i ].move_x.count = 0;
        system->sprites[ i ].move_y.count = 0;
        }
    }


void system_move_all_freeze( system_t* system )
    {
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        system->sprites[ i ].moving = false;  
    }


int system_movon( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return 0;
    --spr_index;

    system_t::sprite_t* spr = &system->sprites[ spr_index ];
    return ( spr->moving && ( spr->move_x.frame_count || spr->move_y.frame_count ) ) ? 1 : 0;
    }


void system_say( system_t* system, char const* text )
    {
    thread_mutex_lock( &system->speech_mutex );
    if( system->speech_sample_pairs ) free( system->speech_sample_pairs );
    if( system->speech_text ) 
        {
        free( system->speech_text );
        system->speech_text = NULL;
        }
    system->speech_sample_pairs = 0;
    system->speech_sample_pairs_count = 0;
    system->speech_sample_pairs_pos = 0;
    if( text && *text )
        system->speech_text = strdup( text );
    thread_mutex_unlock( &system->speech_mutex );
    thread_signal_raise( &system->speech_signal );    
    }


void system_loadsong( system_t* system, int index, char const* filename )
    {
    if( index < 1 || index > 16 ) return;
    --index;

    mid_t* mid = NULL;
    file_t* mid_file = file_load( filename, FILE_MODE_BINARY, 0 );
    if( mid_file ) 
        {
        mid = mid_create( mid_file->data, mid_file->size, soundfont, sizeof( soundfont ), 0 );
        file_destroy( mid_file );
        mid_skip_leading_silence( mid );
        }

    thread_mutex_lock( &system->song_mutex );
    if( system->songs[ index ] )
        {
        mid_destroy( system->songs[ index ] );
        system->songs[ index ] = NULL; 
        }
    system->songs[ index ] = mid;
    thread_mutex_unlock( &system->song_mutex );
    }


void system_playsong( system_t* system, int index )
    {
    if( index < 1 || index > 16 ) return;
    thread_mutex_lock( &system->song_mutex );
    if( system->songs[ index - 1 ] ) 
        system->current_song = index;
    thread_mutex_unlock( &system->song_mutex );
    }


void system_stopsong( system_t* system )
    {
    thread_mutex_lock( &system->song_mutex );
    system->current_song = 0;
    thread_mutex_unlock( &system->song_mutex );
    }



void system_load_palette( system_t* system, char const* string )
    {
    int w, h, c;
    stbi_uc* img = stbi_load( string, &w, &h, &c, 4 );
    if( !img ) return;

    u32 palette[ 32 ] = { 0 };
    int count = 0;      
    for( int y = 0; y < h; ++y )
        {
        for( int x = 0; x < w; ++x )    
            {
            u32 pixel = ((u32*)img)[ x + y * w ];
            if( ( pixel & 0xff000000 ) == 0 ) goto skip;
            u32 r = pixel & 0xff;
            u32 g = ( pixel >> 8 ) & 0xff;
            u32 b = ( pixel >> 16 ) & 0xff;
            b = ( b / 32 ) * 36;
            g = ( g / 32 ) * 36;
            r = ( r / 32 ) * 36;
            pixel = ( pixel & 0xff000000 ) | ( b << 16 ) | ( g << 8 ) | r;
            ((u32*)img)[ x + y * w ] = pixel;
            if( count < 32 ) 
                {
                for( int i = 0; i < count; ++i )
                    {
                    if( palette[ i ] == pixel )
                        goto skip;
                    }
                    palette[ count ] = pixel;       
                }
            ++count;
        skip:
            ;
            }
        }   
    if( count > 32 ) 
        {
        memset( palette, 0, sizeof( palette ) );
        count = palettize_generate_palette_xbgr32( (PALETTIZE_U32*) img, w, h, palette, 32, 0 );        
        }
    for( int i = 0; i < count; ++i )
        {
        u32 col = palette[ i ];
        u32 r = col & 0xff;
        u32 g = ( col >> 8 ) & 0xff;
        u32 b = ( col >> 16 ) & 0xff;
        b = ( b / 32 ) & 0x7;
        g = ( g / 32 ) & 0x7;
        r = ( r / 32 ) & 0x7;
        system->palette[ i ] = (uint16_t)( ( r << 8 ) | ( g << 4 ) | b );
        }
    if( count > 0 && system->paldither )
        {
        paldither_palette_destroy( system->paldither );
        system->paldither = NULL;
        }
    stbi_image_free( img );     
    }


void system_load_sound( system_t* system, int data_index, char const* filename )
    {
    if( data_index < 1 || data_index > sizeof( system->sound_data ) /  sizeof( *system->sound_data ) ) return;

    --data_index;

    unsigned int channels;
    unsigned int rate;
    drwav_uint64 frame_count;
    float* samples = drwav_open_file_and_read_pcm_frames_f32( filename, &channels, &rate, &frame_count );
    if( !samples ) return;
    
    // For now, only 44.1kHz Stereo sounds supported
    if( channels != 2 || rate != 44100 ) return; // TODO: Resample and combine/duplicate channels to support all formats

    int16_t* sample_pairs = (int16_t*) malloc( sizeof( int16_t ) * frame_count * 2 );
    for( int i = 0; i < (int)frame_count * 2; ++i )
        {
        float val = samples[ i ] * 32767.0f;
        sample_pairs[ i ] = val > 32767.0f ? 32767 : val < -32767.0f ? -32767 : (int16_t) val;
        }

    drwav_free( samples );

    thread_mutex_lock( &system->sound_mutex );
    if( system->sound_data[ data_index ].sample_pairs )
        {
        free( system->sound_data[ data_index ].sample_pairs );
        system->sound_data[ data_index ].sample_pairs_count = 0;
        }

    system->sound_data[ data_index ].sample_pairs = sample_pairs;
    system->sound_data[ data_index ].sample_pairs_count = (int) frame_count;
    thread_mutex_unlock( &system->sound_mutex );
    }


void system_play_sound( system_t* system, int sound_index, int data_index )
    {
    if( data_index < 1 || data_index > sizeof( system->sound_data ) /  sizeof( *system->sound_data ) ) return;
    if( sound_index < 1 || sound_index > sizeof( system->sounds ) /  sizeof( *system->sounds ) ) return;
    --sound_index;
    thread_mutex_lock( &system->sound_mutex );
    system->sounds[ sound_index ].data = data_index;
    system->sounds[ sound_index ].pos = 0;
    thread_mutex_unlock( &system->sound_mutex );
    }


unsigned long long default_font[ 256 ] = 
    {
    0x0000000000000000,0x7e8199bd81a5817e,0x7effe7c3ffdbff7e,0x00081c3e7f7f7f36,0x00081c3e7f3e1c08,0x1c086b7f7f1c3e1c,
    0x1c083e7f3e1c0808,0x0000183c3c180000,0xffffe7c3c3e7ffff,0x003c664242663c00,0xffc399bdbd99c3ff,0x1e333333bef0e0f0,
    0x187e183c6666663c,0x070f0e0c0cfcccfc,0x0367e6c6c6fec6fe,0x18db3ce7e73cdb18,0x0001071f7f1f0701,0x0040707c7f7c7040,
    0x183c7e18187e3c18,0x0066006666666666,0x00d8d8d8dedbdbfe,0x1e331c36361cc67c,0x007e7e7e00000000,0xff183c7e187e3c18,
    0x00181818187e3c18,0x00183c7e18181818,0x000018307f301800,0x00000c067f060c00,0x00007f0303030000,0x00002466ff662400,
    0x0000ffff7e3c1800,0x0000183c7effff00,0x0000000000000000,0x000c000c0c1e1e0c,0x0000000000363636,0x0036367f367f3636,
    0x000c1f301e033e0c,0x0063660c18336300,0x006e333b6e1c361c,0x0000000000030606,0x00180c0606060c18,0x00060c1818180c06,
    0x0000663cff3c6600,0x00000c0c3f0c0c00,0x060c0c0000000000,0x000000003f000000,0x000c0c0000000000,0x000103060c183060,
    0x003e676f7b73633e,0x003f0c0c0c0c0e0c,0x003f33061c30331e,0x001e33301c30331e,0x0078307f33363c38,0x001e3330301f033f,
    0x001e33331f03061c,0x000c0c0c1830333f,0x001e33331e33331e,0x000e18303e33331e,0x000c0c00000c0c00,0x060c0c00000c0c00,
    0x00180c0603060c18,0x00003f00003f0000,0x00060c1830180c06,0x000c000c1830331e,0x001e037b7b7b633e,0x0033333f33331e0c,
    0x003f66663e66663f,0x003c66030303663c,0x001f36666666361f,0x007f46161e16467f,0x000f06161e16467f,0x007c66730303663c,
    0x003333333f333333,0x001e0c0c0c0c0c1e,0x001e333330303078,0x006766361e366667,0x007f66460606060f,0x0063636b7f7f7763,
    0x006363737b6f6763,0x001c36636363361c,0x000f06063e66663f,0x00381e3b3333331e,0x006766363e66663f,0x001e33180c06331e,
    0x001e0c0c0c0c2d3f,0x003f333333333333,0x000c1e3333333333,0x0063777f6b636363,0x0063361c1c366363,0x001e0c0c1e333333,
    0x007f664c1831637f,0x001e06060606061e,0x00406030180c0603,0x001e18181818181e,0x0000000063361c08,0xff00000000000000,
    0x0000000000180c0c,0x006e333e301e0000,0x003b66663e060607,0x001e3303331e0000,0x006e33333e303038,0x001e033f331e0000,
    0x000f06060f06361c,0x1f303e33336e0000,0x006766666e360607,0x001e0c0c0c0e000c,0x1e33333030300030,0x0067361e36660607,
    0x001e0c0c0c0c0c0e,0x00636b7f7f330000,0x00333333331f0000,0x001e3333331e0000,0x0f063e66663b0000,0x78303e33336e0000,
    0x000f06666e3b0000,0x001f301e033e0000,0x00182c0c0c3e0c08,0x006e333333330000,0x000c1e3333330000,0x00367f7f6b630000,
    0x0063361c36630000,0x1f303e3333330000,0x003f260c193f0000,0x00380c0c070c0c38,0x0018181800181818,0x00070c0c380c0c07,
    0x0000000000003b6e,0x007f6363361c0800,0x1e30181e3303331e,0x007e333333003300,0x001e033f331e0038,0x00fc667c603cc37e,
    0x007e333e301e0033,0x007e333e301e0007,0x007e333e301e0c0c,0x1c301e03031e0000,0x003c067e663cc37e,0x001e033f331e0033,
    0x001e033f331e0007,0x001e0c0c0c0e0033,0x003c1818181c633e,0x001e0c0c0c0e0007,0x0063637f63361c63,0x00333f331e000c0c,
    0x003f061e063f0038,0x00fe33fe30fe0000,0x007333337f33367c,0x001e33331e00331e,0x001e33331e003300,0x001e33331e000700,
    0x007e33333300331e,0x007e333333000700,0x1f303e3333003300,0x00183c66663c18c3,0x001e333333330033,0x18187e03037e1818,
    0x003f67060f26361c,0x0c0c3f0c3f1e3333,0xe363f3635f33331f,0x0e1b18183c18d870,0x007e333e301e0038,0x001e0c0c0c0e001c,
    0x001e33331e003800,0x007e333333003800,0x003333331f001f00,0x00333b3f3733003f,0x00007e007c36363c,0x00003e001c36361c,
    0x001e3303060c000c,0x000003033f000000,0x000030303f000000,0xf03366cc7b3363c3,0xc0f3f6ecdb3363c3,0x0018181818001818,
    0x0000cc663366cc00,0x00003366cc663300,0x1144114411441144,0x55aa55aa55aa55aa,0x77dbeedb77dbeedb,0x1818181818181818,
    0x1818181f18181818,0x1818181f181f1818,0x6c6c6c6f6c6c6c6c,0x6c6c6c7f00000000,0x1818181f181f0000,0x6c6c6c6f606f6c6c,
    0x6c6c6c6c6c6c6c6c,0x6c6c6c6f607f0000,0x0000007f606f6c6c,0x0000007f6c6c6c6c,0x0000001f181f1818,0x1818181f00000000,
    0x000000f818181818,0x000000ff18181818,0x181818ff00000000,0x181818f818181818,0x000000ff00000000,0x181818ff18181818,
    0x181818f818f81818,0x6c6c6cec6c6c6c6c,0x000000fc0cec6c6c,0x6c6c6cec0cfc0000,0x000000ff00ef6c6c,0x6c6c6cef00ff0000,
    0x6c6c6cec0cec6c6c,0x000000ff00ff0000,0x6c6c6cef00ef6c6c,0x000000ff00ff1818,0x000000ff6c6c6c6c,0x181818ff00ff0000,
    0x6c6c6cff00000000,0x000000fc6c6c6c6c,0x000000f818f81818,0x181818f818f80000,0x6c6c6cfc00000000,0x6c6c6cff6c6c6c6c,
    0x181818ff18ff1818,0x0000001f18181818,0x181818f800000000,0xffffffffffffffff,0xffffffff00000000,0x0f0f0f0f0f0f0f0f,
    0xf0f0f0f0f0f0f0f0,0x00000000ffffffff,0x006e3b133b6e0000,0x03031f331f331e00,0x0003030303333f00,0x0036363636367f00,
    0x003f33060c06333f,0x000e1b1b1b7e0000,0x03063e6666666600,0x00181818183b6e00,0x3f0c1e33331e0c3f,0x001c36637f63361c,
    0x007736366363361c,0x001e33333e180c38,0x00007edbdb7e0000,0x03067edbdb7e3060,0x001c06031f03061c,0x003333333333331e,
    0x00003f003f003f00,0x003f000c0c3f0c0c,0x003f00060c180c06,0x003f00180c060c18,0x1818181818d8d870,0x0e1b1b1818181818,
    0x000c0c003f000c0c,0x00003b6e003b6e00,0x000000001c36361c,0x0000001818000000,0x0000001800000000,0x383c3637303030f0,
    0x000000363636361e,0x0000001e060c180e,0x00003c3c3c3c0000,0x0000000000000000,
    };


unsigned short default_palette[ 32 ] = 
    {
    0x000, 0x111, 0x112, 0x113, 0x124, 0x136, 0x356, 0x467, 0x177, 0x274, 0x153, 0x341, 0x132, 0x122, 0x121, 0x311, 
    0x431, 0x732, 0x743, 0x762, 0x766, 0x777, 0x554, 0x434, 0x333, 0x222, 0x423, 0x115, 0x326, 0x536, 0x244, 0x134, 
    };


#endif /* SYSTEM_IMPLEMENTATION */