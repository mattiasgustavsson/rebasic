
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

#define PALDITHER_IMPLEMENTATION
#include "libs/paldither.h"

#define SPEECH_IMPLEMENTATION
#include "libs/speech.hpp"


#pragma warning( push )
#pragma warning( disable: 4296 ) 
#pragma warning( disable: 4365 ) // conversion, signed/unsigned mismatch

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define DR_WAV_IMPLEMENTATION
#include "libs/dr_wav.h"
#undef DR_WAV_IMPLEMENTATION

#pragma warning( pop )


#define COMPILE_IMPLEMENTATION
#include "compile.h"

#define VM_ÌMPLEMENTATION
#include "vm.h"

#define SYSTEM_ÌMPLEMENTATION
#include "system.h"
