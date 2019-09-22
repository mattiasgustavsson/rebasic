
#define APP_IMPLEMENTATION
#define APP_WINDOWS
#define APP_LOG( ctx, level, message )
#include "libs/app.h"

#define  CRTEMU_IMPLEMENTATION
#include "libs/crtemu.h"

#define  CRT_FRAME_IMPLEMENTATION
#include "libs/crt_frame.h"

#define STRPOOL_IMPLEMENTATION
#include "libs/strpool.h"

#define FILE_IMPLEMENTATION
#include "libs/file.h"

#define MID_IMPLEMENTATION
#include "libs/mid.h"

#define THREAD_IMPLEMENTATION
#include "libs/thread.h"

#define PALETTIZE_IMPLEMENTATION
#include "libs/palettize.h"

#pragma warning( push )
#pragma warning( disable: 4018 ) // 'expression' : signed/unsigned mismatch
#pragma warning( disable: 4244 ) // conversion, possible loss of data
#pragma warning( disable: 4296 ) 
#pragma warning( disable: 4365 ) // conversion, signed/unsigned mismatch
#pragma warning( disable: 4388 ) // signed/unsigned mismatch
#pragma warning( disable: 4456 ) 

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"

#pragma warning( pop )

#define COMPILE_IMPLEMENTATION
#include "compile.h"

#define VM_ÌMPLEMENTATION
#include "vm.h"
