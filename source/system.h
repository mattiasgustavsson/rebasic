


extern unsigned long long default_font[ 256 ];
extern unsigned short default_palette[ 32 ];
extern unsigned char soundfont[ 1093878 ];

struct sprite_data_t
    {
    uint8_t* pixels;
    int width;
    int height;
    };


struct sprite_t
    {
    int x;
    int y;
    int data;
    };


struct sound_data_t
    {
    int16_t* sample_pairs;
    int sample_pairs_count;
    };


struct sound_t
    {
    int data;
    int pos;
    };


struct system_t
    {
    vm_context_t* vm;
    bool input_mode;
    bool wait_vbl;

    int cursor_x;
    int cursor_y;
    int pen;
    int paper;

    paldither_palette_t* paldither;
    uint16_t palette[ 32 ];
    uint8_t screen[ 320 * 200 ];   

    thread_mutex_t song_mutex;
    mid_t* songs[ 16 ];
    int current_song;

    sprite_t sprites[ 32 ];
    sprite_data_t sprite_data[ 256 ];

    thread_mutex_t sound_mutex;
    sound_t sounds[ 4 ];
    sound_data_t sound_data[ 32 ];

    thread_mutex_t speech_mutex;
    thread_ptr_t speech_thread;
    thread_signal_t speech_signal;
    thread_atomic_int_t speech_exit;
    char* speech_text;
    short* speech_sample_pairs;
    int speech_sample_pairs_count;
    int speech_sample_pairs_pos;
    } g_system;


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


void system_init( system_t* system, vm_context_t* vm )
    {
    memset( system, 0, sizeof( *system ) );
    system->vm = vm;
    system->pen = 21;
    system->paper = 0;
    system->current_song = 0;
    memcpy( g_system.palette, default_palette, sizeof( g_system.palette ) );

    thread_mutex_init( &system->song_mutex );
    thread_mutex_init( &system->speech_mutex );
    thread_mutex_init( &system->sound_mutex );
    thread_signal_init( &system->speech_signal );
    thread_atomic_int_store( &system->speech_exit, 0 );
    system->speech_thread = thread_create( speech_thread, system, NULL, THREAD_STACK_SIZE_DEFAULT );
    }


void system_term( system_t* system )
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
   

    for( int i = 0; i < sizeof( g_system.sprite_data ) /  sizeof( *g_system.sprite_data ); ++i )
        if( g_system.sprite_data[ i ].pixels )
            free( g_system.sprite_data[ i ].pixels );

    for( int i = 0; i < sizeof( g_system.sound_data ) /  sizeof( *g_system.sound_data ); ++i )
        if( g_system.sound_data[ i ].sample_pairs )
        free( g_system.sound_data[ i ].sample_pairs );


    if( g_system.paldither ) paldither_palette_destroy( g_system.paldither );
    }


void system_say( char const* text )
    {
    thread_mutex_lock( &g_system.speech_mutex );
    if( g_system.speech_sample_pairs ) free( g_system.speech_sample_pairs );
    if( g_system.speech_text ) 
        {
        free( g_system.speech_text );
        g_system.speech_text = NULL;
        }
    g_system.speech_sample_pairs = 0;
    g_system.speech_sample_pairs_count = 0;
    g_system.speech_sample_pairs_pos = 0;
    if( text && *text )
        g_system.speech_text = strdup( text );
    thread_mutex_unlock( &g_system.speech_mutex );
    thread_signal_raise( &g_system.speech_signal );    
    }


void system_loadsong( int index, char const* filename )
    {
    if( index < 1 || index > 16 ) return;
    thread_mutex_lock( &g_system.song_mutex );
    --index;
    if( g_system.songs[ index ] )
        {
        mid_destroy( g_system.songs[ index ] );
        g_system.songs[ index ] = NULL; 
        }
	file_t* mid_file = file_load( filename, FILE_MODE_BINARY, 0 );
    if( mid_file ) 
        {
        g_system.songs[ index ] =  mid_create( mid_file->data, mid_file->size, soundfont, sizeof( soundfont ), 0 );
        file_destroy( mid_file );
        mid_skip_leading_silence( g_system.songs[ index ] );
        }
    thread_mutex_unlock( &g_system.song_mutex );
    }


void system_playsong( int index )
    {
    if( index < 1 || index > 16 ) return;
    thread_mutex_lock( &g_system.song_mutex );
    if( g_system.songs[ index - 1 ] ) 
        g_system.current_song = index;
    thread_mutex_unlock( &g_system.song_mutex );
    }


void system_stopsong()
    {
    thread_mutex_lock( &g_system.song_mutex );
    g_system.current_song = 0;
    thread_mutex_unlock( &g_system.song_mutex );
    }



void system_cdown()
    {
    if( g_system.cursor_y < 24 )
        {
        g_system.cursor_y++;
        }
    else
        {
        for( int i = 0; i < 320 * 192; ++i ) g_system.screen[ i ] = g_system.screen[ i + 320 * 8 ];
        for( int i = 320 * 192; i < 320 * 200; ++i ) g_system.screen[ i ] = (uint8_t)( g_system.paper & 31 );
        }
    }


void system_print( char const* str )
    {
    for( char const* c = str; *c != 0; ++c )
        {
        int x = g_system.cursor_x * 8;
        int y = g_system.cursor_y * 8;
        unsigned long long chr = default_font[ *c ];
        for( int iy = 0; iy < 8; ++iy )	
			{
			for( int ix = 0; ix < 8; ++ix )	
				{			
				if( chr & ( 1ull << ( ix + iy * 8 ) ) )
					g_system.screen[ x + ix + ( y + iy ) * 320 ] = (uint8_t)( g_system.pen & 31 );
				else
					g_system.screen[ x + ix + ( y + iy ) * 320 ] = (uint8_t)( g_system.paper & 31 );
				}
			}
        g_system.cursor_x++;
        if( g_system.cursor_x >= 40 ) { g_system.cursor_x = 0; system_cdown(); }
        }
    }


void system_load_palette( char const* string )
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
        g_system.palette[ i ] = (uint16_t)( ( r << 8 ) | ( g << 4 ) | b );
        }
    if( count > 0 && g_system.paldither )
        {
        paldither_palette_destroy( g_system.paldither );
        g_system.paldither = NULL;
        }
    stbi_image_free( img );		
    }


void system_load_sprite( int sprite_data_index, char const* string )
    {
    if( sprite_data_index < 1 || sprite_data_index > sizeof( g_system.sprite_data ) /  sizeof( *g_system.sprite_data ) )
        return;

    --sprite_data_index;

    if( g_system.sprite_data[ sprite_data_index ].pixels )
        {
        free( g_system.sprite_data[ sprite_data_index ].pixels );
        g_system.sprite_data[ sprite_data_index ].pixels = NULL;
        g_system.sprite_data[ sprite_data_index ].width = 0;
        g_system.sprite_data[ sprite_data_index ].height = 0;
        }

	int w, h, c;
	stbi_uc* img = stbi_load( string, &w, &h, &c, 4 );
    if( !img ) return;

    if( !g_system.paldither  )
        {
        u32 palette[ 32 ];
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

        size_t size = 0;
        g_system.paldither = paldither_palette_create( palette, 32, &size, NULL );
        }
    

    g_system.sprite_data[ sprite_data_index ].width = w;
    g_system.sprite_data[ sprite_data_index ].height = h;
    g_system.sprite_data[ sprite_data_index ].pixels = (uint8_t*) malloc( (size_t) w * h );
    memset( g_system.sprite_data[ sprite_data_index ].pixels, 0, (size_t) w * h ); 
    paldither_palettize( (PALDITHER_U32*) img, w, h, g_system.paldither, PALDITHER_TYPE_NONE, g_system.sprite_data[ sprite_data_index ].pixels );
    
    for( int i = 0; i < w * h; ++i )
        if( ( ( (PALDITHER_U32*) img )[ i ] & 0xff000000 ) >> 24 < 0x80 )
            g_system.sprite_data[ sprite_data_index ].pixels[ i ] |=  0x80u;       
    stbi_image_free( img );		
    }


void system_sprite( int spr_index, int x, int y, int data_index )
    {
    if( data_index < 1 || data_index > sizeof( g_system.sprite_data ) /  sizeof( *g_system.sprite_data ) ) return;
    if( spr_index < 1 || spr_index > sizeof( g_system.sprites ) /  sizeof( *g_system.sprites ) ) return;
    if( !g_system.sprite_data[ data_index - 1 ].pixels ) return;
    --spr_index;
    g_system.sprites[ spr_index ].data = data_index;
    g_system.sprites[ spr_index ].x = x;
    g_system.sprites[ spr_index ].y = y;
    }


void system_sprite( int spr_index, int x, int y )
    {
    if( spr_index < 1 || spr_index > sizeof( g_system.sprites ) /  sizeof( *g_system.sprites ) ) return;
    --spr_index;
    g_system.sprites[ spr_index ].x = x;
    g_system.sprites[ spr_index ].y = y;
    }


void system_load_sound( int data_index, char const* filename )
    {
    if( data_index < 1 || data_index > sizeof( g_system.sound_data ) /  sizeof( *g_system.sound_data ) ) return;

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

    thread_mutex_lock( &g_system.sound_mutex );
    if( g_system.sound_data[ data_index ].sample_pairs )
        {
        free( g_system.sound_data[ data_index ].sample_pairs );
        g_system.sound_data[ data_index ].sample_pairs_count = 0;
        }

    g_system.sound_data[ data_index ].sample_pairs = sample_pairs;
    g_system.sound_data[ data_index ].sample_pairs_count = (int) frame_count;
    thread_mutex_unlock( &g_system.sound_mutex );
    }


void system_play_sound( int sound_index, int data_index )
    {
    if( data_index < 1 || data_index > sizeof( g_system.sound_data ) /  sizeof( *g_system.sound_data ) ) return;
    if( sound_index < 1 || sound_index > sizeof( g_system.sounds ) /  sizeof( *g_system.sounds ) ) return;
    --sound_index;
    thread_mutex_lock( &g_system.sound_mutex );
    g_system.sounds[ sound_index ].data = data_index;
    g_system.sounds[ sound_index ].pos = 0;
    thread_mutex_unlock( &g_system.sound_mutex );
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

