
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
void system_put_sprite( system_t* system, int spr_index );
void system_get_sprite( system_t* system, int x, int y, int w, int h, int data_index, int mask );
void system_update_on( system_t* system );
void system_update_off( system_t* system );
void system_update( system_t* system );
int system_xsprite( system_t* system, int spr_index );
int system_ysprite( system_t* system, int spr_index );

// COLLIDE 
// LIMIT SPRITE 
// ZONE 
// SET ZONE
// RESET ZONE

void system_priority_on( system_t* system );
void system_priority_off( system_t* system );
int system_detect( system_t* system, int spr_index );
void system_synchro_on( system_t* system );
void system_synchro_off( system_t* system );
void system_synchro( system_t* system );
void system_off( system_t* system );
void system_freeze( system_t* system );
void system_unfreeze( system_t* system );

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

#include "libs/dr_wav.h"
#include "libs/file.h"
#include "libs/mid.h"
#include "libs/palettize.h"
#include "libs/stb_image.h"
#include "libs/sort.hpp"
#include "libs/speech.hpp"
#include "libs/thread.h"

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

    uint16_t palette[ 32 ];
    uint8_t screen[ 320 * 200 ]; // the screen, which we draw to  
    uint8_t final_screen[ 320 * 200 ]; // final composite of screen with cursor and sprites rendered on top of the screen contents
    uint32_t out_screen_xbgr[ 384 * 288 ]; // XBGR 32-bit de-palettized screen with borders added
    uint8_t charmap[ 40 * 25 ];

    bool frozen;
    bool ypos_priority;
    bool manual_sprite_update;
    bool manual_sprite_synchro;
    int sprite_synchro_count;   

    struct sprite_t
        {
        int x;
        int y;
        int data;

        int draw_x;
        int draw_y;
        int draw_data;

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

    struct sprite_order_t
        {
        int index;
        int ypos;
        } sprite_order[ sizeof( sprites ) / sizeof( *sprites ) ];

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

    free( system );
    }


void system_update_sprites( system_t* system )
    {
    // Animate sprites
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
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
                if( !system->manual_sprite_update ) spr->draw_data = spr->data;
                anim->timer = anim->frames[ anim->index ].delay;
                }
            }
        }

    // Update X movement
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        system_t::sprite_t* spr = &system->sprites[ i ];
        system_t::sprite_t::move_t* move_x = &spr->move_x;
        if( move_x->frame_count && spr->moving )
            {
            --move_x->timer;
            if( move_x->timer <= 0 )
                {
                int prev_x = spr->x;
                spr->x += move_x->frames[ move_x->index ].step;
                if( !system->manual_sprite_update ) spr->draw_x = spr->x;
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
        }
    
    // Update Y movement
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        system_t::sprite_t* spr = &system->sprites[ i ];
        system_t::sprite_t::move_t* move_y = &spr->move_y;
        if( move_y->frame_count && spr->moving )
            {
            --move_y->timer;
            if( move_y->timer <= 0 )
                {
                int prev_y = spr->y;
                spr->y += move_y->frames[ move_y->index ].step;
                if( !system->manual_sprite_update ) spr->draw_y = spr->y;
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

    if( !system->frozen && !system->manual_sprite_synchro)
        system_update_sprites( system );

    if( system->manual_sprite_synchro )
        ++system->sprite_synchro_count;
    }


static int system_priority_compare_func( system_t::sprite_order_t const& a, system_t::sprite_order_t const& b ) 
    {
    if( a.ypos < b.ypos ) return -1;
    else if( a.ypos > b.ypos ) return 1;
    else return 0;
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
        int index = (int) ( sizeof( system->sprites ) /  sizeof( *system->sprites ) ) - i - 1;
        system->sprite_order[ i ].index = index;
        system->sprite_order[ i ].ypos = system->sprites[ index ].draw_y;
        }

    if( system->ypos_priority ) 
        sort_ns::sort<system_t::sprite_order_t, system_priority_compare_func>( 
            system->sprite_order, sizeof( system->sprites ) /  sizeof( *system->sprites ) );

    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        system_t::sprite_t* spr = &system->sprites[ system->sprite_order[ i ].index ];
        int data_index = spr->draw_data;
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
                    int xp = spr->draw_x + x;
                    int yp = spr->draw_y + y;
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
    if( system->frozen || system->current_song < 1 || system->current_song > 16 || !system->songs[ system->current_song - 1 ] ) 
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
    char const* tl[] = { "\xC0", "\xC8", "\xD0", "\xD8", "\xE0", "\xE4", "\xE8", "\xEC", "\xE0", "\xC0", "\xF4", "\xF8", "\xF8", "\xFC", "\xFD" };
    char const* t [] = { "\xC1", "\xC9", "\xD1", "\xD9", "\xC1", "\xC9", "\xD1", "\xD9", "\xF0", "\xF0", "\xC9", "\xC1", "\xF0", "\xFC", "\xFD" };
    char const* tr[] = { "\xC2", "\xCA", "\xD2", "\xDA", "\xE1", "\xE5", "\xE9", "\xED", "\xE1", "\xC2", "\xF5", "\xF9", "\xF9", "\xFC", "\xFD" };
    char const* l [] = { "\xC3", "\xCB", "\xD3", "\xDB", "\xC3", "\xCB", "\xD3", "\xDB", "\xF1", "\xF1", "\xCB", "\xC3", "\xF1", "\xFC", "\xFD" };
    char const* r [] = { "\xC4", "\xCC", "\xD4", "\xDC", "\xC4", "\xCC", "\xD4", "\xDC", "\xF2", "\xF2", "\xCC", "\xC4", "\xF2", "\xFC", "\xFD" };
    char const* bl[] = { "\xC5", "\xCD", "\xD5", "\xDD", "\xE2", "\xE6", "\xEA", "\xEE", "\xE2", "\xC5", "\xF6", "\xFA", "\xFA", "\xFC", "\xFD" };
    char const* b [] = { "\xC6", "\xCE", "\xD6", "\xDE", "\xC6", "\xCE", "\xD6", "\xDE", "\xF3", "\xF3", "\xCE", "\xC6", "\xF3", "\xFC", "\xFD" };
    char const* br[] = { "\xC7", "\xCF", "\xD7", "\xDF", "\xE3", "\xE7", "\xEB", "\xEF", "\xE3", "\xC7", "\xF7", "\xFB", "\xFB", "\xFC", "\xFD" };

    if( border < 1 || border > sizeof( tl ) / sizeof( *tl ) ) return;
    --border;

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
            system_cright( system );
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

    system->sprite_data[ sprite_data_index ].width = w;
    system->sprite_data[ sprite_data_index ].height = h;
    system->sprite_data[ sprite_data_index ].pixels = (uint8_t*) malloc( (size_t) w * h );
    memset( system->sprite_data[ sprite_data_index ].pixels, 0, (size_t) w * h ); 
    palettize_remap_xbgr32( (PALETTIZE_U32*) img, w, h, palette, 32, system->sprite_data[ sprite_data_index ].pixels );
    
    for( int i = 0; i < w * h; ++i )
        if( ( ( (PALETTIZE_U32*) img )[ i ] & 0xff000000 ) >> 24 < 0x80 )
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
    system->sprites[ spr_index ].draw_data = data_index;
    system->sprites[ spr_index ].draw_x = x;
    system->sprites[ spr_index ].draw_y = y;
    }


void system_spritepos( system_t* system, int spr_index, int x, int y )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;
    system->sprites[ spr_index ].x = x;
    system->sprites[ spr_index ].y = y;
    system->sprites[ spr_index ].draw_x = x;
    system->sprites[ spr_index ].draw_y = y;
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


void system_put_sprite( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return;
    --spr_index;

    system_t::sprite_t* spr = &system->sprites[ spr_index ];
    
    int data_index = spr->draw_data;
    if( data_index < 1 || data_index > sizeof( system->sprite_data ) /  sizeof( *system->sprite_data ) ) return;
    --data_index;
    if( !system->sprite_data[ data_index ].pixels ) return;

    system_t::sprite_data_t* data = &system->sprite_data[ data_index ];
    for( int y = 0; y < data->height; ++y )
        {
        for( int x = 0; x < data->width; ++x )
            {
            uint8_t p = data->pixels[ x + y * data->width ];
            if( ( p & 0x80 ) == 0 )
                {
                int xp = spr->draw_x + x;
                int yp = spr->draw_y + y;
                if( xp >= 0 && yp >= 0 && xp < 320 && yp < 200 )
                    system->screen[ xp + yp * 320 ] = p  & 31u;                    
                }
            }
        }
    }


void system_get_sprite( system_t* system, int x, int y, int w, int h, int data_index, int mask )
    {
    if( data_index < 1 || data_index > sizeof( system->sprite_data ) /  sizeof( *system->sprite_data ) ) return;
    --data_index;

    system_t::sprite_data_t* data = &system->sprite_data[ data_index ];
    if( data->pixels ) free( data->pixels );
    data->pixels = (uint8_t*) malloc( w * h * sizeof( uint8_t ) );
    data->width = w;
    data->height = h;
    for( int iy = 0; iy < h; ++iy )
        {
        for( int ix = 0; ix < w; ++ix )
            {
            int xp = ix + x;
            int yp = iy + y;
            uint8_t p = 0;
            if( xp >= 0 && yp >= 0 && xp < 320 && yp < 200 )
                p = system->screen[ xp + yp * 320 ] & 31u;                    
            if( p == mask ) p |= 0x80u;
            data->pixels[ ix + iy * w ] = p;
            }
        }
    }


void system_update_on( system_t* system )
    {
    system->manual_sprite_update = false;
    }


void system_update_off( system_t* system )
    {
    system->manual_sprite_update = true;
    }


void system_update( system_t* system )
    {
    if( system->manual_sprite_update )
        {
        for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
            {
            system->sprites[ i ].draw_data = system->sprites[ i ].data;
            system->sprites[ i ].draw_x = system->sprites[ i ].x;
            system->sprites[ i ].draw_y = system->sprites[ i ].y;
            }
        }
    }


int system_xsprite( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return 0;
    --spr_index;

    return system->sprites[ spr_index ].x;
    }


int system_ysprite( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return 0;
    --spr_index;

    return system->sprites[ spr_index ].y;
    }


void system_priority_on( system_t* system )
    {
    system->ypos_priority = true;
    }


void system_priority_off( system_t* system )
    {
    system->ypos_priority = false;
    }


int system_detect( system_t* system, int spr_index )
    {
    if( spr_index < 1 || spr_index > sizeof( system->sprites ) /  sizeof( *system->sprites ) ) return 0;
    --spr_index;

    int x = system->sprites[ spr_index ].x;
    int y = system->sprites[ spr_index ].y;
    if( x < 0 || x >= 320 || y < 0 || y >= 200 ) return 0;

    return system->screen[ x + y * 320 ];
    }


void system_synchro_on( system_t* system )
    {
    system->manual_sprite_synchro = true;
    system->sprite_synchro_count = 0;
    }


void system_synchro_off( system_t* system )
    {
    system->manual_sprite_synchro = false;
    }


void system_synchro( system_t* system )
    {
    if( system->manual_sprite_synchro )
        {
        for( int i = 0; i < system->sprite_synchro_count; ++i )
            system_update_sprites( system );

        system->sprite_synchro_count = 0;
        }
    }


void system_off( system_t* system )
    {
    for( int i = 0; i < sizeof( system->sprites ) /  sizeof( *system->sprites ); ++i )
        {
        system->sprites[ i ].data = 0;
        system->sprites[ i ].draw_data = 0;
        system->sprites[ i ].anim.running = false;  
        system->sprites[ i ].anim.frame_count = 0;
        }
    }


void system_freeze( system_t* system )
    {
    system->frozen = true;
    }


void system_unfreeze( system_t* system )
    {
    system->frozen = false;
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


void load_font( char const* filename, unsigned long long font[ 256 ] )
    {
    int w, h, c;
    APP_U32* img = (APP_U32*) stbi_load( filename, &w, &h, &c, 4 );
    if( !img || w != 128 || h != 128 ) 
        {
        stbi_image_free( (stbi_uc*) img );
        memset( font, 0, 256 * sizeof( unsigned long long ) );
        return;    
        }

    for( int i = 0; i < 256; ++i )
        {
        APP_U64 v = 0;
        int xp = ( i & 15 ) * 8;
        int yp = ( i / 15 ) * 8;
        for( int y = 0; y < 8; ++y )
            for( int x = 0; x < 8; ++x )
                v |= ( ( img[ xp + x + ( yp + y ) * w ] & 0xff ) == 0xe4 ? 1ull : 0ull ) << ( x + y * 8 ); 
        font[ i ] = v;
        }

    stbi_image_free( (stbi_uc*) img );
    }


unsigned long long default_font[ 256 ] = 
    {
    0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,
    0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0000000000000000,0x0018001818181818,0x0000000000363636,
    0x006cfe6c6cfe6c00,0x00143e643c167c14,0x0062660c18366600,0x006e371b2e1c361c,0x0000000000060c0c,0x00180c0c0c0c0c18,0x0018303030303018,
    0x0000663cff3c6600,0x000008083e080800,0x060c0c0000000000,0x000000007e000000,0x0018180000000000,0x03060c183060c080,0x003e63676b73633e,
    0x003c181818181c18,0x007e0c183060663c,0x003c66603860663c,0x0018187f1b1b0303,0x003c6660603e067e,0x003c66663e06663c,0x0002060c1830607e,
    0x003c66663c66663c,0x003c66607c66663c,0x0000181800181800,0x000c181800181800,0x0030180c060c1830,0x00007e00007e0000,0x000c18306030180c,
    0x001800183060663c,0x007e037b4b7b633e,0x006666667e66663c,0x003e66663e66663e,0x003c66060606663c,0x003e66666666663e,0x007e06061e06067e,
    0x000606061e06067e,0x007c66760606663c,0x006666667e666666,0x003c18181818183c,0x001e333330303038,0x0066361e0e1e3666,0x007e060606060606,
    0x0063636b7f7f7763,0x006363737b6f6763,0x003e63636363633e,0x000606063e66663e,0x006e335b6363633e,0x006666663e66663e,0x003c66603c06663c,
    0x001818181818187e,0x003c666666666666,0x0018183c3c666666,0x0063777f6b636363,0x0063361c081c3663,0x000c0c0c1e333333,0x007f060c1830607f,
    0x003c0c0c0c0c0c3c,0x00406030180c0603,0x003c30303030303c,0x0000000063361c08,0x007f000000000000,0x0000000030180c00,0x006e3333331e0000,
    0x003e6666663e0606,0x003c6606663c0000,0x007c6666667c6060,0x003c067e663c0000,0x000c0c0c1c0c6c38,0x3e607c66667c0000,0x006666666e360606,
    0x0018181818180018,0x1c30303030300030,0x0066361e36660606,0x0018181818181818,0x006b6b6b7f360000,0x00666666663e0000,0x003c6666663c0000,
    0x06063e66663e0000,0x60607c66667c0000,0x00060606361e0000,0x003e603c063c0000,0x00386c0c0c3c0c0c,0x007c666666660000,0x00183c2466660000,
    0x00367f6b6b6b0000,0x0063361c36630000,0x3e607c6666660000,0x007e0c18307e0000,0x00380c0c0e0c0c38,0x1818181818181818,0x001c30307030301c,
    0x000000306b060000,0x007e422424181800,0x18103c06063c0000,0x007c666666660042,0x003c067e663c1020,0x00dc667c603c007e,0x00dc667c603c0066,
    0x00dc667c603c000e,0x00dc7e603c001818,0x18103c06063c0000,0x003c067e663c003c,0x003c067e663c0066,0x003c067e663c000e,0x003c1818181c0024,
    0x003c18181c002418,0x003c18181c000804,0x00667e663c180066,0x00667e663c180018,0x007e023e027e1020,0x007e19fe987e0000,0x00f9191f79191afc,
    0x003c66663c002418,0x003c6666663c0066,0x003c6666663c000e,0x007c66666666003c,0x007c66666666000e,0x3e607c6666660042,0x003c6666663c0042,
    0x007c666666660042,0x18183c02023c1818,0x007f66061f06663c,0x0008081c081c3636,0x023e66663e666c38,0x00060c0c0c1ecc78,0x00dc7e603c001830,
    0x003c18181c001020,0x003c66663c001020,0x007c666666001020,0x006666663b001a2c,0x0062524a46001a2c,0x3c00dc667c603c00,0x3c003c6666663c00,
    0x3c66060c18001800,0x000404047c000000,0x002020203e000000,0xf021429469112141,0x40f94a5469112141,0x1818181818001800,0x0048241209122448,
    0x0012244890482412,0x00dc7e603c001a2c,0x003c66663c001a2c,0x023c666e76663c40,0x023c666e763c4000,0x007e19f9997e0000,0x00fe1919791919fe,
    0x667e663c1800180c,0x667e663c18001a2c,0x3c666666663c1a2c,0x0000000000000066,0x00000000000c1830,0x00000808081c0800,0x5050505e5353535e,
    0x7ec3b98d8db9c37e,0x7ec3b59da5bdc37e,0x0000008aaafada8f,0x08080808f0000000,0x00000000ff000000,0x101010100f000000,0x0808080808080808,
    0x1010101010101010,0x000000f008080808,0x000000ff00000000,0x0000000f10101010,0x3c3c3cfcfcf80000,0x000000ffffff0000,0x3c3c3c3f3f1f0000,
    0x3c3c3c3c3c3c3c3c,0x3c3c3c3c3c3c3c3c,0x0000f8fcfc3c3c3c,0x0000ffffff000000,0x00001f3f3f3c3c3c,0x242424c408f00000,0x000000ff00ff0000,
    0x24242423100f0000,0x2424242424242424,0x2424242424242424,0x0000f008c4242424,0x0000ff00ff000000,0x00000f1023242424,0x3434f4e408f00000,
    0x0000ffff00ff0000,0x3434373f381f0000,0x3434343434343434,0x3434343434343434,0x0000f0f81cf43434,0x0000ffff00ff0000,0x00000f1f38333434,
    0xf880808080000000,0x1f01010101000000,0x00000080808080f8,0x000000010101011f,0xfcfcf8e0e0c00000,0x3f3f1f0707030000,0x0000c0e0e0f8fcfc,
    0x00000307071f3f3f,0xc484982020c00000,0x2321190404030000,0x0000c020209884c4,0x0000030404192123,0xf4f4e48830c00000,0x373733190c030000,
    0x0000c0f018c4f4f4,0x0000030f1e3c3933,0x0000000699600000,0x1020201008040408,0x0804040810202010,0x0000069960000000,0x7e7e61f9fdfb3e00,
    0x7e7ec69fbfdf7c00,0x003efbfdf9637e7e,0x007cdfbf9fc67e7e,0x08080808ff09090f,0x10101010ff9090f0,0x0f0909ff08080808,0xf09090ff10101010,
    0xffffffffffffffff,0xaa55aa55aa55aa55,0xcc33cc33cc33cc33,0x0000000000000000,
    };


unsigned short default_palette[ 32 ] = 
    {
    0x000, 0x111, 0x112, 0x113, 0x124, 0x136, 0x356, 0x467, 0x177, 0x274, 0x153, 0x341, 0x132, 0x122, 0x121, 0x311, 
    0x431, 0x732, 0x743, 0x762, 0x766, 0x777, 0x554, 0x434, 0x333, 0x222, 0x423, 0x115, 0x326, 0x536, 0x244, 0x134, 
    };


#endif /* SYSTEM_IMPLEMENTATION */