#ifndef vm_h
#define vm_h


struct vm_context_t;

typedef void (*vm_func_t)( vm_context_t* ctx );
typedef void (*vm_trace_callback_t)( void* ctx, int pos );

void vm_init( vm_context_t* ctx, void* code, int code_size, void* map, int map_size, void* data, int data_size, 
              int stack_size, int globals_size, vm_func_t* host_funcs, int host_funcs_count,
              char const* strings, int string_count, vm_trace_callback_t trace_callback, void* trace_context );

void vm_term( vm_context_t* ctx );

bool vm_halted( vm_context_t* ctx );

int vm_run( vm_context_t* ctx, int op_count );

void vm_pause( vm_context_t* ctx );

void vm_resume( vm_context_t* ctx );

bool vm_paused( vm_context_t* ctx );

enum vm_op_t
    {
    VM_OP_HALT, 
    VM_OP_TRON, VM_OP_TROFF,
    VM_OP_PUSH, VM_OP_POP, VM_OP_LOAD, VM_OP_STORE, 
    VM_OP_PUSHC, VM_OP_POPC, VM_OP_LOADC, VM_OP_STOREC, 
    VM_OP_JSR, VM_OP_RET, VM_OP_JMP, VM_OP_JNZ, 
    VM_OP_EQS, VM_OP_NES, VM_OP_LES, VM_OP_GES, VM_OP_LTS, VM_OP_GTS,
    VM_OP_EQF, VM_OP_NEF, VM_OP_LEF, VM_OP_GEF, VM_OP_LTF, VM_OP_GTF,
    VM_OP_EQC, VM_OP_NEC, VM_OP_LEC, VM_OP_GEC, VM_OP_LTC, VM_OP_GTC,
    VM_OP_ADD, VM_OP_SUB, VM_OP_OR, VM_OP_XOR, VM_OP_MUL, VM_OP_DIV, 
    VM_OP_MOD, VM_OP_AND, VM_OP_NEG, VM_OP_NOT, 
    VM_OP_ORB, VM_OP_XORB, VM_OP_ANDB, VM_OP_NOTB,  
    VM_OP_ADDF, VM_OP_SUBF, VM_OP_MULF, VM_OP_DIVF, VM_OP_MODF, VM_OP_NEGF,     
    VM_OP_CATC,
    VM_OP_READ, VM_OP_READF, VM_OP_READC, VM_OP_READB, VM_OP_RSTO,
    VM_OP_RTSS,

    VM_OPCOUNT,
    };


#endif /* vm_h */


#ifndef vm_impl
#define vm_impl

typedef unsigned int u32;
#include "libs/strpool.h"
#include <string.h>
#include <assert.h>

struct vm_context_t;

typedef void (*vm_func_t)( vm_context_t* ctx );
typedef void (*vm_trace_callback_t)( void* ctx, int pos );

struct vm_context_t
    {
    vm_trace_callback_t trace_callback;
    void* trace_context;
    bool tracing;
    bool is_paused;
    vm_func_t* optable;
    void* code;
    u32* map;
    void* data;
    void* data_end;
    void* stack;
    u32* globals;
    u32* pc;
    u32* sp;
    u32* dp;

    strpool_t string_pool;
    };

////////////////////////////////////////////////////////////////////////

template< typename T > struct param_cast
    {
    param_cast( vm_context_t* ctx, u32 p ) : p( p ) { (void) ctx; } 
    operator T() { return *(T*)(&p); }

    private:
        u32 p;
    };

template<> struct param_cast<bool>
    {
    param_cast( vm_context_t* ctx, u32 p ) : ctx( ctx ), p( p ) { } 
    operator bool() { return p == 0 ? false : true; }

    private:
        vm_context_t* ctx;
        u32 p;
    };

template<> struct param_cast<char const*>
    {
    param_cast( vm_context_t* ctx, u32 p ) : ctx( ctx ), p( p ) { } 
    ~param_cast() { if( strpool_decref( &ctx->string_pool, p ) == 0 ) strpool_discard( &ctx->string_pool, p ); }
    operator char const*() { char const* x = strpool_cstr( &ctx->string_pool, p); return x == 0 ? "" : x; }

    private:
        vm_context_t* ctx;
        u32 p;
    };

template< typename T > struct ret_cast
    {
    ret_cast( vm_context_t* ctx, T p ) : p( p ) { (void) ctx; } 
    operator u32() { return *(u32*)(&p); }
    private:
        T p;
    };

template<> struct ret_cast<bool>
    {
    ret_cast( vm_context_t* ctx, bool p ) : p( p ) { (void) ctx; } 
    operator u32() { return p ? 1U : 0U; }
    private:
        bool p;
    };

template<> struct ret_cast<char const*>
    {
    ret_cast( vm_context_t* ctx, char const* x )
        { 
        p = (u32) strpool_inject( &ctx->string_pool, x == 0 ? "" : x, x == 0 ? 0 : ( (int) strlen( x ) ) ); 
        assert( p );
        strpool_incref( &ctx->string_pool, p );  
        }   
    
    operator u32() { return p; }

    private:
        vm_context_t* ctx;
        u32 p;
    };


template< typename R, void* F >
void vm_func( vm_context_t* ctx )
    {
    typedef R (*func_t)();
    ret_cast<R> r( ctx, ((func_t)F)() );
    *(ctx->sp++) = r;
    }

template< typename R, void* F, typename P0 >
void vm_func( vm_context_t* ctx )
    {
    typedef R (*func_t)( P0 );
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ret_cast<R> r( ctx, ((func_t)F)( a ) );
    *(ctx->sp++) = r;
    }

template< typename R, void* F, typename P0, typename P1 >
void vm_func( vm_context_t* ctx )
    {
    typedef R (*func_t)( P0, P1 );
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ret_cast<R> r( ctx, ((func_t)F)( a, b ) );
    *(ctx->sp++) = r;
    }

template< typename R, void* F, typename P0, typename P1, typename P2 >
void vm_func( vm_context_t* ctx )
    {
    typedef R (*func_t)( P0, P1, P2 );
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ret_cast<R> r( ctx, ((func_t)F)( a, b, c ) );
    *(ctx->sp++) = r;
    }

template< typename R, void* F, typename P0, typename P1, typename P2, typename P3 >
void vm_func( vm_context_t* ctx )
    {
    typedef R (*func_t)( P0, P1, P2, P3 );
    param_cast<P3> d( ctx, *(--ctx->sp) ); 
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ret_cast<R> r( ctx, ((func_t)F)( a, b, c, d ) );
    *(ctx->sp++) = r;
    }

template< typename R, void* F, typename P0, typename P1, typename P2, typename P3, typename P4 >
void vm_func( vm_context_t* ctx )
    {
    typedef R (*func_t)( P0, P1, P2, P3, P4 );
    param_cast<P4> e( ctx, *(--ctx->sp) ); 
    param_cast<P3> d( ctx, *(--ctx->sp) ); 
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ret_cast<R> r( ctx, ((func_t)F)( a, b, c, d, e ) );
    *(ctx->sp++) = r;
    }

////////////////////////////////////////////////////////////////////////

template< void* F >
void vm_proc( vm_context_t* ctx )
    {
    (void) ctx;
    typedef void (*func_t)();
    ((func_t)F)();
    }

template< void* F, typename P0 >
void vm_proc( vm_context_t* ctx )
    {
    typedef void (*func_t)( P0 );
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ((func_t)F)( a );
    }

template< void* F, typename P0, typename P1 >
void vm_proc( vm_context_t* ctx )
    {
    typedef void (*func_t)( P0, P1 );
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ((func_t)F)( a, b );
    }

template< void* F, typename P0, typename P1, typename P2 >
void vm_proc( vm_context_t* ctx )
    {
    typedef void (*func_t)( P0, P1, P2 );
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ((func_t)F)( a, b, c );
    }

template< void* F, typename P0, typename P1, typename P2, typename P3 >
void vm_proc( vm_context_t* ctx )
    {
    typedef void (*func_t)( P0, P1, P2, P3 );
    param_cast<P3> d( ctx, *(--ctx->sp) ); 
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ((func_t)F)( a, b, c, d );
    }

template< void* F, typename P0, typename P1, typename P2, typename P3, typename P4 >
void vm_proc( vm_context_t* ctx )
    {
    typedef void (*func_t)( P0, P1, P2, P3, P4 );
    param_cast<P4> e( ctx, *(--ctx->sp) ); 
    param_cast<P3> d( ctx, *(--ctx->sp) ); 
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ((func_t)F)( a, b, c, d, e );
    }

template< void* F, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5 >
void vm_proc( vm_context_t* ctx )
    {
    typedef void (*func_t)( P0, P1, P2, P3, P4, P5 );
    param_cast<P5> f( ctx, *(--ctx->sp) ); 
    param_cast<P4> e( ctx, *(--ctx->sp) ); 
    param_cast<P3> d( ctx, *(--ctx->sp) ); 
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ((func_t)F)( a, b, c, d, e, f );
    }

template< void* F, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6 >
void vm_proc( vm_context_t* ctx )
    {
    typedef void (*func_t)( P0, P1, P2, P3, P4, P5, P6 );
    param_cast<P6> g( ctx, *(--ctx->sp) ); 
    param_cast<P5> f( ctx, *(--ctx->sp) ); 
    param_cast<P4> e( ctx, *(--ctx->sp) ); 
    param_cast<P3> d( ctx, *(--ctx->sp) ); 
    param_cast<P2> c( ctx, *(--ctx->sp) ); 
    param_cast<P1> b( ctx, *(--ctx->sp) ); 
    param_cast<P0> a( ctx, *(--ctx->sp) ); 
    ((func_t)F)( a, b, c, d, e, f, g );
    }

#endif /* vm_impl */



#ifdef VM_ÌMPLEMENTATION
#undef VM_IMPLEMENTATION

#define _CRT_SECURE_NO_WARNINGS
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define snprintf _snprintf

#define PUSH(x) (*(ctx->sp++)) = (*(u32*)&(x))
#define POP() (*(--ctx->sp))
#define POPF() (*((float*)(--ctx->sp)))

////////////////////////////////////////////////////////////////////////

static char* vm_temp_buffer = 0;
static int vm_temp_capacity = 0;

static char* vm_temp_string( int length ) 
    {
    if( !vm_temp_buffer )
        {
        vm_temp_capacity = length + 1;
        vm_temp_buffer = (char*) malloc( (size_t) vm_temp_capacity );
        assert( vm_temp_buffer );
        }
    else if( vm_temp_capacity <= length )
        {
        vm_temp_capacity = length + 1;
        vm_temp_buffer = (char*) realloc( vm_temp_buffer, (size_t) vm_temp_capacity );
        assert( vm_temp_buffer );
        }

    return vm_temp_buffer;
    }

////////////////////////////////////////////////////////////////////////

static void op_halt( vm_context_t* ctx )
    {
    --ctx->pc;
    }

static void op_tron( vm_context_t* ctx )
    {
    ctx->tracing = true;
    }

static void op_troff( vm_context_t* ctx )
    {
    ctx->tracing = false;
    }

static void op_push( vm_context_t* ctx )
    {
    PUSH( *ctx->pc++ );
    }

static void op_pop( vm_context_t* ctx )
    {
    POP();
    }

static void op_load( vm_context_t* ctx )
    {
    u32 index = POP();
    PUSH( ctx->globals[ index ] );
    }

static void op_store( vm_context_t* ctx )
    {
    u32 index = POP();
    ctx->globals[ index ] = POP();
    }

static void op_pushc( vm_context_t* ctx )
    {
    u32 value = *ctx->pc++;
    strpool_incref( &ctx->string_pool, value );
    PUSH( value );
    }

static void op_popc( vm_context_t* ctx )
    {
    u32 value = POP();
    if( strpool_decref( &ctx->string_pool, value ) == 0 ) strpool_discard( &ctx->string_pool, value );
    }

static void op_loadc( vm_context_t* ctx )
    {
    u32 index = POP();
    u32 value = ctx->globals[ index ];
    PUSH( value );
    strpool_incref( &ctx->string_pool, value );
    }

static void op_storec( vm_context_t* ctx )
    {
    u32 index = POP();
    u32 value = POP();
    if( ctx->globals[ index ] != 0 )
        {
        if( strpool_decref( &ctx->string_pool, ctx->globals[ index ] ) == 0 ) strpool_discard( &ctx->string_pool, ctx->globals[ index ] );
        }
    ctx->globals[ index ] = value;
    }

static void op_jsr( vm_context_t* ctx )
    {
    u32 offset = POP(); 
    u32 addr = (u32)(uintptr_t)( ctx->pc - (u32*) ctx->code );
    PUSH( addr );
    --ctx->pc;
    ctx->pc += (int) offset; 
    }

static void op_ret( vm_context_t* ctx )
    {
    ctx->pc = ( (u32*) ctx->code ) + POP();
    }

static void op_jmp( vm_context_t* ctx )
    {
    u32 offset = POP(); 
    --ctx->pc;
    ctx->pc += (int) offset; 
    }

static void op_jnz( vm_context_t* ctx )
    {
    u32 offset = POP(); 
    int value = (int) POP();
    if( value != 0 )
        {
        --ctx->pc;
        ctx->pc += (int) offset; 
        }
    }

static bool op_eqs( int a, int b ) { return a == b; }
static bool op_nes( int a, int b ) { return a != b; }
static bool op_les( int a, int b ) { return a <= b; }
static bool op_ges( int a, int b ) { return a >= b; }
static bool op_lts( int a, int b ) { return a < b; }
static bool op_gts( int a, int b ) { return a > b; }

static bool op_eqf( float a, float b ) { return fabsf( a - b ) < FLT_EPSILON; }
static bool op_nef( float a, float b ) { return fabsf( a - b ) >= FLT_EPSILON; }
static bool op_lef( float a, float b ) { return a <= b; }
static bool op_gef( float a, float b ) { return a >= b; }
static bool op_ltf( float a, float b ) { return a < b; }
static bool op_gtf( float a, float b ) { return a > b; }

static bool op_eqc( char const* a, char const* b ) { return a == b; }
static bool op_nec( char const* a, char const* b ) { return a != b; }
static bool op_lec( char const* a, char const* b ) { return strcmp( a, b ) <= 0; }
static bool op_gec( char const* a, char const* b ) { return strcmp( a, b ) >= 0; }
static bool op_ltc( char const* a, char const* b ) { return strcmp( a, b ) < 0; }
static bool op_gtc( char const* a, char const* b ) { return strcmp( a, b ) > 0; }

static u32 op_add( u32 a, u32 b ) { return a + b; }
static u32 op_sub( u32 a, u32 b ) { return a - b; }
static u32 op_or( u32 a, u32 b ) { return a | b; }
static u32 op_xor( u32 a, u32 b ) { return a ^ b; }
static int op_mul( int a, int b ) { return a * b; }
static int op_div( int a, int b) { return a / b; }
static int op_mod( int a, int b ) { return a % b; }
static u32 op_and( u32 a, u32 b ) { return a & b; }
static int op_neg( int a ) { return -a; }
static u32 op_not( u32 a ) { return ~a; }

static bool op_orb( bool a, bool b ) { return a || b; }
static bool op_xorb( bool a, bool b ) { return (!a && b ) || ( !b && a ); }
static bool op_andb( bool a, bool b ) { return a && b; }
static bool op_notb( bool a ) { return !a; }

static float op_addf( float a, float b ) { return a + b; }
static float op_subf( float a, float b ) { return a - b; }
static float op_mulf( float a, float b ) { return a * b; }
static float op_divf( float a, float b ) { return a / b; }
static float op_modf( float a, float b ) { return fmodf( a , b ); }
static float op_negf( float a ) { return -a; }

static void op_catc( vm_context_t* ctx )
    {
    u32 b = POP();
    u32 a = POP();
    char const* sa = strpool_cstr( &ctx->string_pool, a );
    char const* sb = strpool_cstr( &ctx->string_pool, b );
    assert( sa && sb );
    int la = strpool_length( &ctx->string_pool, a );
    int lb = strpool_length( &ctx->string_pool, b );
    char* temp = vm_temp_string( la + lb );
    strcpy( temp, sa );
    strcat( temp, sb );
    u32 r = (u32) strpool_inject( &ctx->string_pool, temp, la + lb );
    PUSH( r );
    strpool_incref( &ctx->string_pool, r );
    if( strpool_decref( &ctx->string_pool, a ) == 0 ) strpool_discard( &ctx->string_pool, a );
    if( strpool_decref( &ctx->string_pool, b ) == 0 ) strpool_discard( &ctx->string_pool, b );
    }

enum type_t
    {
    TYPE_NONE,
    TYPE_BOOL,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_ID,
    };

static void op_read( vm_context_t* ctx )
    {
    u32 index = POP();
    u32 pos = POP();
    if( ctx->dp >= ctx->data_end ) { printf( "\nRUNTIME ERROR: Out of data, at %d\n\n", pos ); return; }
    if( *ctx->dp++ != TYPE_INTEGER ) { printf( "\nRUNTIME ERROR: Type mismatch, expected INTEGER, at %d\n\n", pos ); }
    ctx->globals[ index ] = *ctx->dp++;
    }

static void op_readf( vm_context_t* ctx )
    {
    u32 index = POP();
    u32 pos = POP();
    if( ctx->dp >= ctx->data_end ) { printf( "\nRUNTIME ERROR: Out of data, at %d\n\n", pos ); return; }
    if( *ctx->dp++ != TYPE_FLOAT ) { printf( "\nRUNTIME ERROR: Type mismatch, expected FLOAT, at %d\n\n", pos ); }
    ctx->globals[ index ] = *ctx->dp++;
    }

static void op_readc( vm_context_t* ctx )
    {
    u32 index = POP();
    u32 pos = POP();
    if( ctx->dp >= ctx->data_end ) { printf( "\nRUNTIME ERROR: Out of data, at %d\n\n", pos ); return; }
    if( *ctx->dp++ != TYPE_STRING ) { printf( "\nRUNTIME ERROR: Type mismatch, expected STRING, at %d\n\n", pos ); }
    if( ctx->globals[ index ] != 0 )
        {
        if( strpool_decref( &ctx->string_pool, ctx->globals[ index ] ) == 0 ) strpool_discard( &ctx->string_pool, ctx->globals[ index ] );
        }
    ctx->globals[ index ] = *ctx->dp++;
    strpool_incref( &ctx->string_pool, ctx->globals[ index ]  );
    }

static void op_readb( vm_context_t* ctx )
    {
    u32 index = POP();
    u32 pos = POP();
    if( ctx->dp >= ctx->data_end ) { printf( "\nRUNTIME ERROR: Out of data, at %d\n\n", pos ); return; }
    if( *ctx->dp++ != TYPE_BOOL ) { printf( "\nRUNTIME ERROR: Type mismatch, expected BOOLEAN, at %d\n\n", pos ); }
    ctx->globals[ index ] = *ctx->dp++;
    }

static void op_rsto( vm_context_t* ctx )
    {
    u32 offset = POP();
    ctx->dp = (u32*) ctx->data;
    ctx->dp += offset;
    }

static int op_rtss( int index, int min, int max, int pos )
    {
    if( index < min || index > max )
        {
        printf( "\nRUNTIME ERROR: Subscript out of range [ %d <= %d <= %d ] at %d\n\n", min, index, max, pos );
        if( index > max ) index = max;
        if( index < min ) index = min;
        }
    return index;
    }

////////////////////////////////////////////////////////////////////////

typedef void (*vm_func_t)( vm_context_t* ctx );

static vm_func_t vm_ops[] = 
    {
    op_halt, op_tron, op_troff, 
    op_push, op_pop, op_load, op_store,
    op_pushc, op_popc, op_loadc, op_storec,
    op_jsr, op_ret, op_jmp, op_jnz,     
    
    vm_func< bool, op_eqs, int, int>, 
    vm_func< bool, op_nes, int, int>, 
    vm_func< bool, op_les, int, int>,  
    vm_func< bool, op_ges, int, int>, 
    vm_func< bool, op_lts, int, int>, 
    vm_func< bool, op_gts, int, int>,
    
    vm_func< bool, op_eqf, float, float>, 
    vm_func< bool, op_nef, float, float>, 
    vm_func< bool, op_lef, float, float>,  
    vm_func< bool, op_gef, float, float>, 
    vm_func< bool, op_ltf, float, float>, 
    vm_func< bool, op_gtf, float, float>,
    
    vm_func< bool, op_eqc, char const*, char const*>, 
    vm_func< bool, op_nec, char const*, char const*>, 
    vm_func< bool, op_lec, char const*, char const*>,  
    vm_func< bool, op_gec, char const*, char const*>, 
    vm_func< bool, op_ltc, char const*, char const*>, 
    vm_func< bool, op_gtc, char const*, char const*>,
    
    vm_func< u32, op_add, u32, u32 >,
    vm_func< u32, op_sub, u32, u32 >,
    vm_func< u32, op_or, u32, u32 >,
    vm_func< u32, op_xor, u32, u32 >,
    vm_func< int, op_mul, int, int >,
    vm_func< int, op_div, int, int >,
    vm_func< int, op_mod, int, int>,
    vm_func< u32, op_and, u32, u32 >,
    vm_func< int, op_neg, int >,
    vm_func< u32, op_not, u32 >,

    vm_func< bool, op_orb, bool, bool >,
    vm_func< bool, op_xorb, bool, bool >,
    vm_func< bool, op_andb, bool, bool >,
    vm_func< bool, op_notb, bool >,

    vm_func< float, op_addf, float, float >,
    vm_func< float, op_subf, float, float >,
    vm_func< float, op_mulf, float, float >,
    vm_func< float, op_divf, float, float >,
    vm_func< float, op_modf, float, float >,
    vm_func< float, op_negf, float >,

    op_catc,
    op_read, op_readf, op_readc, op_readb, op_rsto, 
    
    vm_func< int, op_rtss, int, int, int, int >,
    };

////////////////////////////////////////////////////////////////////////

void vm_init( vm_context_t* ctx, void* code, int code_size, void* map, int map_size, void* data, int data_size,  
    int stack_size, int globals_size, vm_func_t* host_funcs, int host_funcs_count, 
    char const* strings, int string_count, vm_trace_callback_t trace_callback, void* trace_context )
    {
    ctx->trace_callback = trace_callback;
    ctx->trace_context = trace_context;
    ctx->tracing = false;

    ctx->is_paused = false;

    int vm_op_count = sizeof( vm_ops ) / sizeof( vm_ops[ 0 ] );
    assert( vm_op_count == VM_OPCOUNT );
    int optable_count = vm_op_count + host_funcs_count;
    ctx->optable = (vm_func_t*) malloc( optable_count * sizeof( vm_func_t ) );
    assert( ctx->optable );
    for( int i = 0; i < vm_op_count; ++i ) ctx->optable[ i ] = vm_ops[ i ];
    for( int i = 0; i < host_funcs_count; ++i ) ctx->optable[ vm_op_count + i ] = host_funcs[ i ];

    ctx->code = malloc( (size_t) code_size );
    assert( ctx->code );
    ctx->map = (u32*) malloc( (size_t) map_size );
    assert( ctx->map );
    ctx->data = malloc( (size_t) data_size );
    assert( ctx->data );
    ctx->data_end = (void*) ( ( (uintptr_t) ctx->data ) + data_size );
    ctx->stack = malloc( (size_t) stack_size );
    assert( ctx->stack );
    ctx->globals = (u32*) malloc( (size_t) globals_size );
    assert( ctx->globals );
    if( globals_size > 0 ) memset( ctx->globals, 0, (size_t) globals_size );

    ctx->pc = (u32*) ctx->code;
    ctx->sp = (u32*) ctx->stack;
    ctx->dp = (u32*) ctx->data;

    if( code_size > 0 ) memcpy( ctx->code, code, (size_t) code_size );
    if( map_size > 0 ) memcpy( ctx->map, map, (size_t) map_size );
    if( data_size > 0 ) memcpy( ctx->data, data, (size_t) data_size );

    strpool_config_t config_str = strpool_default_config;
    config_str.counter_bits = 0;
    strpool_init( &ctx->string_pool, &config_str );
    char const* ptr = strings;
    for( int i = 0; i < string_count; ++i )
        {
        int length = (int) strlen( ptr );
        u32 handle = (u32) strpool_inject( &ctx->string_pool, ptr, length );
        strpool_incref( &ctx->string_pool, handle );
        ptr += length + 1;
        }
    }


void vm_term( vm_context_t* ctx )
    {
    strpool_term( &ctx->string_pool );

    if( vm_temp_buffer )
        {
        free( vm_temp_buffer );
        vm_temp_buffer = 0;
        vm_temp_capacity = 0;
        }
    free( ctx->globals );


    free( ctx->stack );
    free( ctx->data );
    free( ctx->map );
    free( ctx->code );
    free( ctx->optable );
    }


bool vm_halted( vm_context_t* ctx )
    {
    return *ctx->pc == VM_OP_HALT;
    }



void vm_pause( vm_context_t* ctx )
    {
    ctx->is_paused = true;
    }


void vm_resume( vm_context_t* ctx )
    {
    ctx->is_paused = false;
    }


bool vm_paused( vm_context_t* ctx )
    {
    return ctx->is_paused;
    }



int vm_run( vm_context_t* ctx, int op_count )
    {
    if( ctx->is_paused ) return 0;

    if( ctx->trace_callback )
        {
        for( int i = 0; i < op_count; ++i ) 
            {
            assert( ctx->optable[ *ctx->pc ] );
            if( ctx->tracing ) 
                {
                ctx->trace_callback( ctx->trace_context, (int)( ctx->map[ ( ( (uintptr_t)ctx->pc) - ((uintptr_t)ctx->code )) / 4 ] ) );
                }
            ctx->optable[ *ctx->pc++ ]( ctx );
            if( ctx->is_paused ) return i + 1;
            }
        }
    else
        {
        for( int i = 0; i < op_count; ++i ) 
            {
            assert( ctx->optable[ *ctx->pc ] );
            ctx->optable[ *ctx->pc++ ]( ctx );
            if( ctx->is_paused ) return i + 1;
            }
        }
    
    return op_count;
    }


#endif /* VM_IMPLEMENTATION */

