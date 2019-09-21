#ifndef compile_h
#define compile_h

enum compile_op_t
	{
	COMPILE_OP_HALT, 
    COMPILE_OP_TRON, COMPILE_OP_TROFF,
	COMPILE_OP_PUSH, COMPILE_OP_POP, COMPILE_OP_LOAD, COMPILE_OP_STORE, 
	COMPILE_OP_PUSHC, COMPILE_OP_POPC, COMPILE_OP_LOADC, COMPILE_OP_STOREC, 
	COMPILE_OP_JSR, COMPILE_OP_RET, COMPILE_OP_JMP, COMPILE_OP_JNZ, 
	COMPILE_OP_EQS, COMPILE_OP_NES, COMPILE_OP_LES, COMPILE_OP_GES, COMPILE_OP_LTS, COMPILE_OP_GTS,
	COMPILE_OP_EQF, COMPILE_OP_NEF, COMPILE_OP_LEF, COMPILE_OP_GEF, COMPILE_OP_LTF, COMPILE_OP_GTF,
	COMPILE_OP_EQC, COMPILE_OP_NEC, COMPILE_OP_LEC, COMPILE_OP_GEC, COMPILE_OP_LTC, COMPILE_OP_GTC,
	COMPILE_OP_ADD, COMPILE_OP_SUB, COMPILE_OP_OR, COMPILE_OP_XOR, COMPILE_OP_MUL, COMPILE_OP_DIV, 
	COMPILE_OP_MOD, COMPILE_OP_AND, COMPILE_OP_NEG, COMPILE_OP_NOT, 
	COMPILE_OP_ORB, COMPILE_OP_XORB, COMPILE_OP_ANDB, COMPILE_OP_NOTB, 	
	COMPILE_OP_ADDF, COMPILE_OP_SUBF, COMPILE_OP_MULF, COMPILE_OP_DIVF, COMPILE_OP_MODF, COMPILE_OP_NEGF, 	
	COMPILE_OP_CATC,
	COMPILE_OP_READ, COMPILE_OP_READF, COMPILE_OP_READC, COMPILE_OP_READB, COMPILE_OP_RSTO,
	COMPILE_OP_RTSS,

	COMPILE_OPCOUNT,
	};

struct compile_opcode_map_t
    {
    compile_op_t compiler_op;
    unsigned int vm_op;
    };


struct compile_bytecode_t
	{
	void* code;
	int code_size;
	void* map;
	int map_size;
	void* data;
	int data_size;
	char* strings;
	int string_count;
	int globals_size;
	};

compile_bytecode_t compile( char const* sourcecode, int length, compile_opcode_map_t* opcode_map, int opcode_count,
    char const** host_func_signatures, int host_func_count, int* error_pos, char error_msg[ 256 ] );

#endif /* compile_h */


#ifdef COMPILE_IMPLEMENTATION
#undef COMPILE_IMPLEMENTATION


#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE 
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "libs/strpool.h"


struct
	{
	bool state;
	char message[ 256 ];
	int pos;
	} compile_error;

static void compile_error_clear() { compile_error.state = false; }

typedef unsigned int u32;



//////// LEXER BEGIN ////////


static void lexer_error( char const* message, int pos )
	{
	assert( !compile_error.state && "Overwriting error message" );
	strcpy( compile_error.message, message );
	compile_error.pos = pos;
	compile_error.state = true;
	}

enum token_type_t
	{
	TOKEN_EOS,
	TOKEN_NEWLINE,
	TOKEN_IDENTIFIER,
	TOKEN_INT,
	TOKEN_FLOAT,
	TOKEN_STRING,
	TOKEN_SYMBOL,
	TOKEN_ERROR,
	};


struct token_t
	{
	int pos;
	token_type_t type;
	union
		{
		u32 identifier;
		char symbol;
		float float_val;
		int int_val;
		u32 string_val;
		};
	};


struct lexer_context_t
	{
	char const* source;
	int pos;
	strpool_t* identifier_pool;
	strpool_t* string_pool;
	};


static char get_char( lexer_context_t* ctx )
	{
	char c = *ctx->source;
	if( c )
		{
		++ctx->pos;
		++ctx->source;
		}
	return c;
	}


static token_t next_token( lexer_context_t* ctx )
	{
	token_t token;
	static char last = ' ';

	// Ignore whitespace
	while( isspace(last) && last != '\n' )
		{
		last = get_char( ctx );
		}

	// Identifier [a-zA-Z][a-zA-Z0-9]*
	if( isalpha( last ) || last == '$' ) 
		{ 
		token.pos = ctx->pos - 1;
		char const* start = ctx->source - 1;
		char const* end = start;
		while( isalnum( last ) || last == '$' )
			{
			end = ctx->source;
			last = get_char( ctx );
			}
		int length = (int) ( end - start );
		
		if( length == 3 && strnicmp( start, "REM", 3 ) == 0 )
			{
			last = '\''; // Comment until end of line
			}
		else
			{
			u32 str = (u32) strpool_inject( ctx->identifier_pool, start, length );
			strpool_incref( ctx->identifier_pool, str );
			token.identifier = str;
			token.type = TOKEN_IDENTIFIER;
			return token;
			}
		}

    // Comment until end of line
	if( last == '\'' ) 
		{
		while( last != 0 && last != '\n' && last != '\r' ) 
			{
			last = get_char( ctx );
			}
		}

	// Number: [0-9.]+
	if( isdigit( last ) ) 
		{   
		token.pos = ctx->pos - 1;
		static char num[ 256 ];
		bool decimal = false;
		char s[] = { last, 0 };
		strcpy( num, s );
		last = get_char( ctx );
		while( isdigit( last ) || last == '.' )
			{
			if( decimal && last == '.' )
				{
				lexer_error( "Invalid number", token.pos );
				token.type = TOKEN_EOS;
				return token;
				}
			decimal |= ( last == '.' );
			char s2[] = { last, 0 };
			strcat( num, s2 );
			last = get_char( ctx );
			}
		if( !decimal )
			{
			token.type = TOKEN_INT;			
			token.int_val = atoi( num );
			return token;
			}
		else
			{
			token.type = TOKEN_FLOAT;			
			token.float_val = (float) atof( num );
			return token;
			}
		} 

	// End of stream
	if( last == 0 ) 
		{
		token.pos = ctx->pos - 1;
		token.type = TOKEN_EOS;
		return token;
		}
	
	// Newline
	if( last == '\n' ) 
		{
		token.pos = ctx->pos - 1;
		token.type = TOKEN_NEWLINE;
		last = ' ';
		return token;
		}

	if( last == '"' )
		{	
		token.pos = ctx->pos - 1;
		char const* start = ctx->source;
		char const* end = start;
		last = get_char( ctx );
		while( last != '"') 
			{
			if( last == 0 || last == '\n' || last == '\r' )
				{
				lexer_error( "'\"' expected", token.pos );
				return token;
				}
			end = ctx->source;
			last = get_char( ctx );
			}
		int length = (int) ( end - start );
		u32 str = (u32) strpool_inject( ctx->string_pool, start, length );
		strpool_incref( ctx->string_pool, str );
		token.type = TOKEN_STRING;
		token.string_val = str;
		last = ' ';
		return token;
		}

	if( last > 32 )
		{
		token.pos = ctx->pos - 1;
		token.type = TOKEN_SYMBOL;
		token.symbol = last;
		last = ' ';
		return token;
		}

	token.pos = ctx->pos - 1;
	token.type = TOKEN_ERROR;
	return token;
	}


static token_t* lex( char const* code, int length, strpool_t* identifier_pool, strpool_t* string_pool )
	{
	int count = 0;
	int capacity = 8 * length / (int) sizeof( token_t ); // First guess, which is probably more than enough
	token_t* tokens = (token_t*) malloc( capacity * sizeof( token_t ) );
	assert( tokens );

	lexer_context_t ctx;
	ctx.source = code;
	ctx.pos = 0;
	ctx.identifier_pool = identifier_pool;
	ctx.string_pool = string_pool;
	
	token_t token = next_token( &ctx );
	if( token.type == TOKEN_ERROR || compile_error.state )
		{
		free( tokens );
		return 0;
		}
	while( token.type != TOKEN_EOS )
		{
		if( count >= capacity - 1 ) // -1 to ensure room for EOS token at the end
			{
			capacity *= 2;
			tokens = (token_t*) realloc( tokens, capacity * sizeof( token_t ) );			
			assert( tokens );
			}

		tokens[ count++ ] = token;
		
		token = next_token( &ctx );
		if( token.type == TOKEN_ERROR || compile_error.state )
			{
			free( tokens );
			return 0;
			}
		}	
	tokens[ count ] = token; // add EOS token

	return tokens;
	}


//////// LEXER END ////////



//////// PARSER BEGIN ////////


static void parser_error( char const* message, int pos )
	{
	assert( !compile_error.state && "Overwriting error message" );
	strcpy( compile_error.message, message );
	compile_error.pos = pos;
	compile_error.state = true;
	}

typedef unsigned int u32;

enum ast_type_t
	{
	AST_TYPE_NONE,
	AST_TYPE_BOOL,
	AST_TYPE_INTEGER,
	AST_TYPE_FLOAT,
	AST_TYPE_STRING,
	};


enum ast_node_t
	{
	AST_PROGRAM,
	AST_LINE,
	AST_LIST_NODE,
	AST_PROCCALL,
	AST_FUNCCALL,
	AST_DIM,
	AST_DATA,
	AST_READ,
	AST_RESTORE,
	AST_BRANCH,
	AST_RETURN,
	AST_LOOP,
	AST_NEXT,
	AST_CONDITION,
	AST_ASSIGNMENT,
	AST_EXPRESSION,
	AST_SIMPLEEXP,
	AST_TERM,
	AST_FACTOR,
	AST_UNARYEXP,
	AST_FLOAT,
	AST_INTEGER,
	AST_STRING,
	AST_VARIABLE,
	AST_END,
	AST_TRON,
	AST_TROFF,
	};

struct ast_var_t
	{
	ast_type_t type;
	int globals_index;
	int dim_a;
	int dim_b;		
	};

struct ast_program_t { int line_list; };
struct ast_list_node_t { int item; int next; };
struct ast_line_t { int number; int statement; };
struct ast_statement_t {  };
struct ast_proccall_t { u32 id; int arg_list; ast_type_t type; };
struct ast_read_t { int var_list; };
struct ast_restore_t { int index; };
struct ast_branch_t { enum brachtype_t { BRANCH_JUMP, BRANCH_SUB, BRANCH_COND, BRANCH_LOOP, }; brachtype_t type; int target; };
struct ast_loop_t { int assignment; };
struct ast_next_t { int assignment; int condition; };
struct ast_condition_t { int expression; int branch; };
struct ast_assignment_t { int variable; int expression; };
struct ast_expression_t { int primary; int simpleexp_list; };
struct ast_simpleexp_t { enum optype_t { OP_EQ, OP_NE, OP_LE, OP_GE, OP_LT, OP_GT, }; optype_t op; int primary; int term_list; };
struct ast_term_t { enum optype_t { OP_ADD, OP_SUB, OP_OR, OP_XOR, }; optype_t op; int primary; int factor_list; };
struct ast_factor_t { enum optype_t { OP_MUL, OP_DIV, OP_MOD, OP_AND, }; optype_t op; int primary; };
struct ast_unaryexp_t { enum optype_t { OP_NEG, OP_NOT, }; optype_t op; int factor; };
struct ast_float_t { float value; };
struct ast_integer_t { int value; };
struct ast_string_t { u32 handle; };
struct ast_variable_t { int index; int offset_a; int offset_b; };


union ast_data_t
	{
	ast_program_t program;
	ast_list_node_t list_node;
	ast_line_t line;
	ast_statement_t statement;
	ast_proccall_t proccall;
	ast_read_t read;
	ast_restore_t restore;
	ast_branch_t branch;
	ast_loop_t loop;
	ast_next_t next;
	ast_condition_t condition;
	ast_assignment_t assignment;
	ast_expression_t expression;
	ast_simpleexp_t simpleexp;
	ast_term_t term;
	ast_factor_t factor;
	ast_unaryexp_t unaryexp;
	ast_float_t float_;
	ast_integer_t integer;
	ast_string_t string;
	ast_variable_t variable;
	};


struct host_funcs_t
	{
	struct func_t
		{
		u32 identifier;
		ast_type_t ret_type;
		int arg_count;
		int arg_start;
		};
	
	func_t* funcs;
	int func_count;

	ast_type_t* arg_types;
	int arg_count;
	int arg_capacity;
	};


struct parser_context_t
	{
	host_funcs_t host_funcs;

	token_t* tokens;
	int pos;

	ast_node_t* node_type;
	ast_data_t* node_data;
	int* node_pos;
	int node_capacity;
	int node_count;

	struct line_map_t
		{
		int line_number;
		int node_index;
		};
	line_map_t* line_map;
	int line_map_capacity;
	int line_map_count;

	int var_size;

	ast_var_t* vars;
	u32* vars_identifiers;
	int vars_capacity;
	int vars_count;

	int jump_targets_capacity;
	int jump_targets_count;
	int* jump_targets;

	struct loop_stack_t	
		{
		int target;
		int var_index;
		int limit;
		int step;
		};
	loop_stack_t* loop_stack;
	int loop_stack_capacity;
	int loop_stack_count;

	struct data_t
		{
		u32 type;
		u32 value;
		};
	data_t* data;
	int* data_lines;
	int data_capacity;
	int data_count;

	strpool_t* identifier_pool;

	u32 keyword_DIM;
	u32 keyword_GOTO;
	u32 keyword_GOSUB;
	u32 keyword_RETURN;
	u32 keyword_FOR;
	u32 keyword_TO;  
	u32 keyword_STEP;
	u32 keyword_NEXT;
	u32 keyword_IF;
	u32 keyword_THEN;
	u32 keyword_OR;
	u32 keyword_XOR;
	u32 keyword_MOD;
	u32 keyword_AND;
	u32 keyword_NOT;
	u32 keyword_DATA;
	u32 keyword_READ;
	u32 keyword_RESTORE;
	u32 keyword_TRUE;
	u32 keyword_FALSE;
	u32 keyword_END;
	u32 keyword_STOP;
	u32 keyword_TRON;
	u32 keyword_TROFF;
 	};


static token_t* get_token( parser_context_t* ctx )
	{
	int index = ctx->pos;
	if( ctx->tokens[ index ].type != TOKEN_EOS ) ++ctx->pos;
	return &ctx->tokens[ index ];
	}


static void rewind_token( parser_context_t* ctx )
	{
	if( ctx->pos > 0 )	--ctx->pos;
	}


static token_t* peek_token( parser_context_t* ctx )
	{
	return &ctx->tokens[ ctx->pos ];
	}


static ast_type_t ast_get_type( const ast_node_t* node_type, const ast_data_t* node_data, const ast_var_t* vars, int index )
	{
	switch( node_type[ index ] )
		{
		case AST_PROGRAM: return AST_TYPE_NONE;
		case AST_LINE: return ast_get_type( node_type, node_data, vars, node_data[ index ].line.statement );
		case AST_LIST_NODE: return AST_TYPE_NONE;
		case AST_PROCCALL: return node_data[ index ].proccall.type;
		case AST_FUNCCALL: return node_data[ index ].proccall.type;
		case AST_LOOP: return AST_TYPE_NONE;
		case AST_CONDITION: return AST_TYPE_NONE;
		case AST_ASSIGNMENT: return AST_TYPE_NONE;
		case AST_EXPRESSION: return ( node_data[ index ].expression.simpleexp_list >= 0 ) ? AST_TYPE_BOOL : ast_get_type( node_type, node_data, vars, node_data[ index ].expression.primary );
		case AST_SIMPLEEXP: return ast_get_type( node_type, node_data, vars, node_data[ index ].simpleexp.primary );
		case AST_TERM:  return ast_get_type( node_type, node_data, vars, node_data[ index ].term.primary );
		case AST_FACTOR:  return ast_get_type( node_type, node_data, vars, node_data[ index ].factor.primary );
		case AST_UNARYEXP:  return ast_get_type( node_type, node_data, vars, node_data[ index ].unaryexp.factor );
		case AST_FLOAT: return AST_TYPE_FLOAT;
		case AST_INTEGER: return AST_TYPE_INTEGER;
		case AST_STRING: return AST_TYPE_STRING;
		case AST_VARIABLE: return vars[ node_data[ index ].variable.index ].type;
        case AST_DIM:
	    case AST_DATA:
	    case AST_READ:
	    case AST_RESTORE:
	    case AST_BRANCH:
	    case AST_RETURN:
	    case AST_NEXT:
	    case AST_END:
	    case AST_TRON:
	    case AST_TROFF:
        default:
            return AST_TYPE_NONE;
		}
	}


static ast_type_t ast_get_type( const parser_context_t* ctx, int index )
	{
	return ast_get_type( ctx->node_type, ctx->node_data, ctx->vars, index );
	}

static int map_var( parser_context_t* ctx, u32 identifier )
	{
	for( int i = 0; i < ctx->vars_count; ++i )
		{
		if( ctx->vars_identifiers[ i ] == identifier ) return i;
		}

	if( ctx->vars_count >= ctx->vars_capacity )
		{
		ctx->vars_capacity *= 2;
		ctx->vars = (ast_var_t*) realloc( ctx->vars, sizeof( *ctx->vars ) * ctx->vars_capacity );
		assert( ctx->vars );
		ctx->vars_identifiers = (u32*) realloc( ctx->vars_identifiers, sizeof( *ctx->vars_identifiers ) * ctx->vars_capacity );
		assert( ctx->vars_identifiers );
		}

	ctx->vars_identifiers[ ctx->vars_count ] = identifier;
	ctx->vars[ ctx->vars_count ].globals_index = ctx->var_size++;
	ctx->vars[ ctx->vars_count ].type = AST_TYPE_NONE;
	ctx->vars[ ctx->vars_count ].dim_a = 0;
	ctx->vars[ ctx->vars_count ].dim_b = 0;

	return ctx->vars_count++;
	}


static int dim_var( parser_context_t* ctx, u32 identifier, int a, int b )
	{
	for( int i = 0; i < ctx->vars_count; ++i )
		{
		if( ctx->vars_identifiers[ i ] == identifier ) 
			{
			parser_error( "Variable already in use", ctx->pos );
			return -1;
			}
		}

	if( ctx->vars_count >= ctx->vars_capacity )
		{
		ctx->vars_capacity *= 2;
		ctx->vars = (ast_var_t*) realloc( ctx->vars, sizeof( *ctx->vars ) * ctx->vars_capacity );
		assert( ctx->vars );
		ctx->vars_identifiers = (u32*) realloc( ctx->vars_identifiers, sizeof( *ctx->vars_identifiers ) * ctx->vars_capacity );
		assert( ctx->vars_identifiers );
		}

	int size = ( a > 0 && b > 0 ) ? ( a + 1 ) * ( b + 1 ) : ( a > 0 ) ? ( a + 1 ) : 1 ;

	ctx->vars_identifiers[ ctx->vars_count ] = identifier;
	ctx->vars[ ctx->vars_count ].globals_index = ctx->var_size;	
	ctx->vars[ ctx->vars_count ].type = AST_TYPE_NONE;
	ctx->vars[ ctx->vars_count ].dim_a = a;
	ctx->vars[ ctx->vars_count ].dim_b = b;
	ctx->var_size += size;
	return ctx->vars_count++;
	}


static int var_dimensions( parser_context_t* ctx, u32 identifier )
	{
	for( int i = 0; i < ctx->vars_count; ++i )
		{
		if( ctx->vars_identifiers[ i ] == identifier ) 
			{			
			return ctx->vars[ i ].dim_b > 0 ? 2 : ctx->vars[ i ].dim_a > 0 ? 1 : 0;
			}
		}
	return 0;
	}


static int create_node( parser_context_t* ctx, ast_node_t type )
	{
	if( ctx->node_count >= ctx->node_capacity )
		{
		ctx->node_capacity *= 2;
		ctx->node_type = (ast_node_t*) realloc( ctx->node_type, sizeof( *ctx->node_type ) * ctx->node_capacity );
		assert( ctx->node_type );
		ctx->node_data = (ast_data_t*) realloc( ctx->node_data, sizeof( *ctx->node_data ) * ctx->node_capacity );
		assert( ctx->node_data );
		ctx->node_pos = (int*) realloc( ctx->node_pos, sizeof( *ctx->node_pos ) * ctx->node_capacity );
		assert( ctx->node_pos );
		}

	int index = ctx->node_count++;
	ctx->node_type[ index ] = type;	
	ctx->node_pos[ index ] = ctx->tokens[ ctx->pos ].pos;
	return index;
	}


static int parse( parser_context_t* ctx, ast_node_t in_type )
	{
	#define node ctx->node_data[ index ]
	
	int const index = create_node( ctx, in_type );

	if( compile_error.state ) return index;
	

	switch( in_type )
		{
		case AST_PROGRAM:
			{
			node.program.line_list = -1;
			int list_tail = -1;
			while( peek_token( ctx )->type != TOKEN_EOS )
				{
				int line = parse( ctx, AST_LINE ); if( compile_error.state ) return index;

				int list_next = parse( ctx, AST_LIST_NODE ); if( compile_error.state ) return index;			
				if( list_tail >= 0 ) ctx->node_data[ list_tail ].list_node.next = list_next;
				else node.program.line_list = list_next;

				ctx->node_data[ list_next ].list_node.item = line;
				ctx->node_data[ list_next ].list_node.next = -1;

				list_tail = list_next;
				}
			} break;

		case AST_LINE:
			{
			token_t* initial_token = get_token( ctx );
			if( initial_token->type != TOKEN_INT ) 
				{
				parser_error( "Line number expected", initial_token->pos );
				return index;
				}

			node.line.number = initial_token->int_val;
			node.line.statement = -1;

			for( int i = 0; i < ctx->line_map_count; ++i )
				{
				if( ctx->line_map[ i ].line_number == node.line.number )
					{
					parser_error( "Duplicate line number", initial_token->pos );
					return index;
					}
				}

			if( ctx->line_map_count >= ctx->line_map_capacity )
				{
				ctx->line_map_capacity *= 2;
				ctx->line_map = (parser_context_t::line_map_t*) realloc( ctx->line_map, sizeof( *ctx->line_map ) * ctx->line_map_capacity );
				assert( ctx->line_map );
				}
			ctx->line_map[ ctx->line_map_count ].line_number = node.line.number;
			ctx->line_map[ ctx->line_map_count ].node_index = index;
			++ctx->line_map_count;

			if( peek_token( ctx )->type != TOKEN_NEWLINE ) 
				{ 
				token_t* token = get_token( ctx );

				if( token->type != TOKEN_IDENTIFIER ) { parser_error( "Identifier expected", token->pos ); return index; }
		
				token_t* next_token = peek_token( ctx );
				rewind_token( ctx );

					 if( token->identifier == ctx->keyword_DIM ) node.line.statement = parse( ctx, AST_DIM );
				else if( token->identifier == ctx->keyword_DATA ) node.line.statement = parse( ctx, AST_DATA );
				else if( token->identifier == ctx->keyword_READ ) node.line.statement = parse( ctx, AST_READ );
				else if( token->identifier == ctx->keyword_RESTORE ) node.line.statement = parse( ctx, AST_RESTORE );
				else if( token->identifier == ctx->keyword_GOTO ) node.line.statement = parse( ctx, AST_BRANCH );
				else if( token->identifier == ctx->keyword_GOSUB ) node.line.statement = parse( ctx, AST_BRANCH );
				else if( token->identifier == ctx->keyword_RETURN ) node.line.statement = parse( ctx, AST_RETURN );
				else if( token->identifier == ctx->keyword_FOR ) node.line.statement = parse( ctx, AST_LOOP );
				else if( token->identifier == ctx->keyword_NEXT ) node.line.statement = parse( ctx, AST_NEXT );
				else if( token->identifier== ctx->keyword_IF ) node.line.statement = parse( ctx, AST_CONDITION );
				else if( token->identifier== ctx->keyword_END ) node.line.statement = parse( ctx, AST_END );
				else if( token->identifier== ctx->keyword_STOP ) node.line.statement = parse( ctx, AST_END );
				else if( token->identifier== ctx->keyword_TRON ) node.line.statement = parse( ctx, AST_TRON );
				else if( token->identifier== ctx->keyword_TROFF ) node.line.statement = parse( ctx, AST_TROFF );
				else if( next_token->type == TOKEN_SYMBOL && next_token->symbol == '=' ) node.line.statement = parse( ctx, AST_ASSIGNMENT );
				else if( var_dimensions( ctx, token->identifier ) > 0 ) node.line.statement = parse( ctx, AST_ASSIGNMENT );
				else node.line.statement = parse( ctx, AST_PROCCALL );
				if( compile_error.state ) return index;
				}
		
			initial_token = get_token( ctx );
			if( initial_token->type != TOKEN_NEWLINE ) parser_error( "Newline expected", initial_token->pos );
			} break;

		case AST_PROCCALL:
		case AST_FUNCCALL:
			{
			token_t* initial_token = get_token( ctx );
			node.proccall.arg_list = -1;
			int list_tail = -1;
			int arg_count = 0;
			if( in_type == AST_FUNCCALL )
				{
				token_t* token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL || token->symbol != '(' ) { parser_error( "'(' expected", token->pos ); return index; }
				}
			if( in_type == AST_FUNCCALL || peek_token( ctx )->type != TOKEN_NEWLINE )
				{
				while( true )
					{
					if( in_type == AST_FUNCCALL && peek_token( ctx )->type == TOKEN_SYMBOL && peek_token( ctx )->symbol == ')' ) goto skip_args;

					int arg = parse( ctx, AST_EXPRESSION ); if( compile_error.state ) return index;
					++arg_count;
			
					int list_next = parse( ctx, AST_LIST_NODE ); if( compile_error.state ) return index;
					if( list_tail >= 0 ) ctx->node_data[ list_tail ].list_node.next = list_next;
					else node.proccall.arg_list = list_next;

					ctx->node_data[ list_next ].list_node.item = arg;
					ctx->node_data[ list_next ].list_node.next = -1;

					list_tail = list_next;
				skip_args:
					token_t* token = get_token( ctx );
					if( in_type == AST_FUNCCALL )
						{
						if( token->type == TOKEN_SYMBOL && token->symbol == ')' ) break;
						if( token->type != TOKEN_SYMBOL || token->symbol != ',' ) { parser_error( "')' or ',' expected", token->pos ); return index; }
						}
					else
						{
						if( token->type == TOKEN_NEWLINE ) { rewind_token( ctx ); break; }
						if( token->type != TOKEN_SYMBOL || token->symbol != ',' ) { parser_error( "',' or newline expected", token->pos ); return index; }
						}
					}
				}

			// find the right host function
			for( int i = 0; i < ctx->host_funcs.func_count; ++i )
				{
				if( ctx->host_funcs.funcs[ i ].arg_count == arg_count && ctx->host_funcs.funcs[ i ].identifier == initial_token->identifier )
					{
					int arg_index = 0;
					int list = node.proccall.arg_list;
					while( list >= 0 )
						{
						int arg = ctx->node_data[ list ].list_node.item;
						if( ast_get_type( ctx, arg ) != ctx->host_funcs.arg_types[ ctx->host_funcs.funcs[ i ].arg_start + arg_index ] )
							{
							goto prototype_no_match; // try next function
							}

						list = ctx->node_data[ list ].list_node.next;
						++arg_index;
						}		

					node.proccall.id = (u32)( COMPILE_OPCOUNT + i );
					node.proccall.type = ctx->host_funcs.funcs[ i ].ret_type;				
					return index;
					}
				prototype_no_match: ;
				}

			parser_error( "Unknown command", initial_token->pos );
			} break;

		case AST_DIM:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_DIM )
				{
				parser_error( "'DIM' expected", token->pos ); 
				return index;
				}
			while( true )
				{
				token = get_token( ctx );
				if( token->type != TOKEN_IDENTIFIER )
					{
					parser_error( "Identifier expected", token->pos ); 
					return index;
					}
				u32 identifier = token->identifier;

				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL && token->symbol != '(' )
					{
					parser_error( "'(' expected", token->pos ); 
					return index;
					}

				token = get_token( ctx );
				if( token->type != TOKEN_INT  )
					{
					parser_error( "Integer expected", token->pos ); 
					return index;
					}

				int dim_a = token->int_val;
				int dim_b = 0;

				token = get_token( ctx );
				if( token->type == TOKEN_SYMBOL && token->symbol == ',' )
					{
					token = get_token( ctx );
					if( token->type != TOKEN_INT  )
						{
						parser_error( "Integer expected", token->pos ); 
						return index;
						}
					dim_b = token->int_val;
					token = get_token( ctx );
					}

				if( token->type != TOKEN_SYMBOL && token->symbol != ')' )
					{
					parser_error( "')' expected", token->pos ); 
					return index;
					}

				dim_var( ctx, identifier, dim_a, dim_b );
				if( peek_token( ctx )->type == TOKEN_NEWLINE ) break;
				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL && token->symbol != ',' )
					{
					parser_error( "',' or newline expected", token->pos ); 
					return index;
					}
				}
			} break;

		case AST_DATA:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_DATA )
				{
				parser_error( "'DATA' expected", token->pos ); 
				return index;
				}
			while( true )
				{
				token = get_token( ctx );
				bool is_bool = token->type == TOKEN_IDENTIFIER && ( token->identifier == ctx->keyword_TRUE || token->identifier == ctx->keyword_FALSE );
				if( token->type != TOKEN_INT && token->type != TOKEN_FLOAT && token->type != TOKEN_STRING && !is_bool)
					{
					parser_error( "Literal expected", token->pos ); 
					return index;
					}

				if( ctx->data_count >= ctx->data_capacity )
					{
					ctx->data_capacity *= 2;
					ctx->data = (parser_context_t::data_t*) realloc( ctx->data, sizeof( *ctx->data ) * ctx->data_capacity );
					assert( ctx->data );
					ctx->data_lines = (int*) realloc( ctx->data_lines, sizeof( *ctx->data_lines ) * ctx->data_capacity );
					assert( ctx->data_lines );
					}

				if( is_bool ) 
					{
					ctx->data[ ctx->data_count ].type = AST_TYPE_BOOL;
					ctx->data[ ctx->data_count ].value = token->identifier == ctx->keyword_TRUE ? 1U : 0U;
					}
				else if( token->type == TOKEN_INT )
					{
					ctx->data[ ctx->data_count ].type = AST_TYPE_INTEGER;
					ctx->data[ ctx->data_count ].value = (u32) token->int_val;
					}
				else if( token->type == TOKEN_FLOAT ) 
					{
					ctx->data[ ctx->data_count ].type = AST_TYPE_FLOAT;
					ctx->data[ ctx->data_count ].value = *( (u32*) &token->float_val );
					}
				else if( token->type == TOKEN_STRING ) 
					{
					ctx->data[ ctx->data_count ].type = AST_TYPE_STRING;
					ctx->data[ ctx->data_count ].value = token->string_val;
					}
				ctx->data_lines[ ctx->data_count ] = ctx->line_map[ ctx->line_map_count - 1 ].line_number;
				++ctx->data_count;

				if( peek_token( ctx )->type == TOKEN_NEWLINE ) break;
				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL && token->symbol != ',' )
					{
					parser_error( "',' or newline expected", token->pos ); 
					return index;
					}
				}
			} break;


		case AST_READ:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_READ )
				{
				parser_error( "'READ' expected", token->pos ); 
				return index;
				}
			int list_tail = -1;
			while( true )
				{
				int list_next = parse( ctx, AST_LIST_NODE ); if( compile_error.state ) return index;
				if( list_tail >= 0 ) ctx->node_data[ list_tail ].list_node.next = list_next;
				else node.read.var_list = list_next;

				ctx->node_data[ list_next ].list_node.item = parse( ctx, AST_VARIABLE ); if( compile_error.state ) return index;
				ctx->node_data[ list_next ].list_node.next = -1;
				list_tail = list_next;

				if( peek_token( ctx )->type == TOKEN_NEWLINE ) break;
				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL && token->symbol != ',' )
					{
					parser_error( "',' or newline expected", token->pos ); 
					return index;
					}
				}
			} break;

		case AST_RESTORE:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_RESTORE )
				{
				parser_error( "'RESTORE' expected", token->pos ); 
				return index;
				}

			token = get_token( ctx );
			if( token->type != TOKEN_INT )
				{
				parser_error( "Line number expected", token->pos ); 
				return index;
				}

			node.restore.index = token->int_val;
			} break;

		case AST_BRANCH:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER )
				{
				parser_error( "Identifier expected", token->pos ); 
				return index;
				}

			     if( token->identifier == ctx->keyword_GOTO ) node.branch.type = ast_branch_t::BRANCH_JUMP;
			else if( token->identifier == ctx->keyword_GOSUB ) node.branch.type = ast_branch_t::BRANCH_SUB;
			else if( token->identifier == ctx->keyword_THEN ) node.branch.type = ast_branch_t::BRANCH_COND;
			else { parser_error( "Unknown branch type", token->pos ); return index; }	

			token = get_token( ctx );
			if( token->type != TOKEN_INT )
				{
				parser_error( "Line number expected", token->pos ); 
				return index;
				}

			node.branch.target = token->int_val;
			} break;

		case AST_RETURN:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_RETURN )
				{
				parser_error( "'RETURN' expected", token->pos ); 
				return index;
				}
			} break;

		case AST_LOOP:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_FOR )
				{
				parser_error( "'FOR' expected", token->pos ); 
				return index;
				}

			if( peek_token( ctx )->type != TOKEN_IDENTIFIER )
				{
				parser_error( "Identifier expected", token->pos ); 
				return index;
				}

			int var_index = map_var( ctx, peek_token( ctx )->identifier );

			node.loop.assignment = parse( ctx, AST_ASSIGNMENT ); if( compile_error.state ) return index;

			token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_TO )
				{
				parser_error( "'TO' expected", token->pos ); 
				return index;
				}
			
			int limit = parse( ctx, AST_EXPRESSION ); if( compile_error.state ) return index;
		
			int step = -1;
			token = get_token( ctx );
			if( token->type == TOKEN_IDENTIFIER && token->identifier == ctx->keyword_STEP )
				{
				step = parse( ctx, AST_EXPRESSION ); if( compile_error.state ) return index;
				}
			else
				{
				rewind_token( ctx );
				}
			
			if( ctx->loop_stack_count >= ctx->loop_stack_capacity )
				{
				ctx->loop_stack_capacity *= 2;
				ctx->loop_stack = (parser_context_t::loop_stack_t*) realloc( ctx->loop_stack, sizeof( *ctx->loop_stack ) * ctx->loop_stack_capacity );
				assert( ctx->loop_stack );
				}

			ctx->loop_stack[ ctx->loop_stack_count ].target = index;
			ctx->loop_stack[ ctx->loop_stack_count ].var_index = var_index;
			ctx->loop_stack[ ctx->loop_stack_count ].limit = limit;
			ctx->loop_stack[ ctx->loop_stack_count ].step = step;
			++ctx->loop_stack_count;
			} break;

		case AST_NEXT:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_NEXT )
				{
				parser_error( "'NEXT' expected", token->pos ); 
				return index;
				}

			token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER )
				{
				parser_error( "Identifier expected", token->pos ); 
				return index;
				}

			parser_context_t::loop_stack_t* loop = ( ctx->loop_stack_count <= 0 ) ? 0 : &ctx->loop_stack[ ctx->loop_stack_count - 1 ];
			
			if( !loop || map_var( ctx, token->identifier ) != loop->var_index )
				{
				parser_error( "'NEXT' without matching 'FOR'", token->pos ); 
				return index;
				}

			{
			int assign_var = create_node( ctx, AST_VARIABLE );
			int assignment = create_node( ctx, AST_ASSIGNMENT );
			int expression = create_node( ctx, AST_SIMPLEEXP );
			int variable = create_node( ctx, AST_VARIABLE );
			int term_list = create_node( ctx, AST_LIST_NODE );
			int term = create_node( ctx, AST_TERM );

			node.next.assignment = assignment;
			
			ctx->node_data[ assign_var ].variable.index = loop->var_index;
			ctx->node_data[ assign_var ].variable.offset_a = -1;
			ctx->node_data[ assign_var ].variable.offset_b = -1;
			ctx->node_data[ assignment ].assignment.variable = assign_var;
			ctx->node_data[ assignment ].assignment.expression = expression;
			ctx->node_data[ expression ].simpleexp.primary = variable;
			ctx->node_data[ expression ].simpleexp.term_list = term_list;
			ctx->node_data[ variable ].variable.index = loop->var_index;
			ctx->node_data[ variable ].variable.offset_a = -1;
			ctx->node_data[ variable ].variable.offset_b = -1;
			ctx->node_data[ term_list ].list_node.item = term;
			ctx->node_data[ term_list ].list_node.next = -1;
			ctx->node_data[ term ].term.op = ast_term_t::OP_ADD;
			if( loop->step >= 0 )
				{
				ctx->node_data[ term ].term.primary = loop->step;
				}
			else
				{
				int one = create_node( ctx, AST_INTEGER );
				ctx->node_data[ term ].term.primary = one;
				ctx->node_data[ one ].integer.value = 1;
				}
			ctx->node_data[ term ].term.factor_list = -1;
			}

			// TODO: Build the condition so it works for negative STEP as well
			//  currently: IF var <= limit THEN loop
			//  change to: IF ( step > 0 AND var <= limit ) OR ( step < 0 AND var >= limit ) THEN loop
			{
			int condition = create_node( ctx, AST_CONDITION );
			int branch = create_node( ctx, AST_BRANCH );
			int expression = create_node( ctx, AST_EXPRESSION );
			int variable = create_node( ctx, AST_VARIABLE );
			int simpleexp_list = create_node( ctx, AST_LIST_NODE );
			int simpleexp = create_node( ctx, AST_SIMPLEEXP );

			node.next.condition = condition;
			ctx->node_data[ condition ].condition.expression = expression;
			ctx->node_data[ condition ].condition.branch = branch;
			ctx->node_data[ branch ].branch.type = ast_branch_t::BRANCH_LOOP;
			ctx->node_data[ branch ].branch.target = loop->target;
			ctx->node_data[ expression ].expression.primary = variable;
			ctx->node_data[ expression ].expression.simpleexp_list= simpleexp_list;
			ctx->node_data[ variable ].variable.index = loop->var_index;
			ctx->node_data[ variable ].variable.offset_a = -1;
			ctx->node_data[ variable ].variable.offset_b = -1;
			ctx->node_data[ simpleexp_list ].list_node.item = simpleexp;
			ctx->node_data[ simpleexp_list ].list_node.next = -1;
			ctx->node_data[ simpleexp ].simpleexp.op = ast_simpleexp_t::OP_LE;
			ctx->node_data[ simpleexp ].simpleexp.primary = loop->limit;
			ctx->node_data[ simpleexp ].simpleexp.term_list = -1;
			}

			--ctx->loop_stack_count;

			} break;

		case AST_CONDITION:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_IF )
				{
				parser_error( "'IF' expected", token->pos ); 
				return index;
				}

			node.condition.expression = parse( ctx, AST_EXPRESSION ); if( compile_error.state ) return index;
			if( ast_get_type( ctx, node.condition.expression ) != AST_TYPE_BOOL )
				{
				parser_error( "Boolean expression expected", token->pos ); 
				return index;
				}

			token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_THEN )
				{
				parser_error( "'THEN' expected", token->pos ); 
				return index;
				}

			rewind_token( ctx );
			node.condition.branch = parse( ctx, AST_BRANCH ); if( compile_error.state ) return index;
			} break;

		case AST_ASSIGNMENT:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER )
				{
				parser_error( "Identifier expected", token->pos ); 
				return index;
				}

			int var_index = map_var( ctx, token->identifier );

			int assign_var = create_node( ctx, AST_VARIABLE );
			ctx->node_data[ assign_var ].variable.index = var_index;
			ctx->node_data[ assign_var ].variable.offset_a = -1;
			ctx->node_data[ assign_var ].variable.offset_b = -1;

			node.assignment.variable = assign_var;

			int var_dim = var_dimensions( ctx, token->identifier );
			if( var_dim > 0 )
				{
				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL || token->symbol != '(' )
					{
					parser_error( "'(' expected", token->pos ); 
					return index;
					}

				ctx->node_data[ assign_var ].variable.offset_a = parse( ctx, AST_EXPRESSION );

				if( var_dim > 1 )
					{
					token = get_token( ctx );
					if( token->type != TOKEN_SYMBOL || token->symbol != ',' )
						{
						parser_error( "',' expected", token->pos ); 
						return index;
						}

					ctx->node_data[ assign_var ].variable.offset_b = parse( ctx, AST_EXPRESSION );
					}

				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL || token->symbol != ')' )
					{
					parser_error( "')' expected", token->pos ); 
					return index;
					}
				}

			token = get_token( ctx );
			if( token->type != TOKEN_SYMBOL || token->symbol != '=' )
				{
				parser_error( "'=' expected", token->pos ); 
				return index;
				}

			node.assignment.expression = parse( ctx, AST_EXPRESSION ); if( compile_error.state ) return index;			
			ast_type_t type = ast_get_type( ctx, node.assignment.expression );
			if( type != ctx->vars[ var_index ].type && ctx->vars[ var_index ].type != AST_TYPE_NONE )
				{
				parser_error( "New type assigned to variable already in use", token->pos ); 
				return index;				
				}
			ctx->vars[ var_index ].type = type;
			
			} break;

		case AST_EXPRESSION:
			// SimpleExpression 
			//		repeated:
			// =  <>  <  <=  >  >=
			// SimpleExpression 
			{
			node.expression.primary = parse( ctx, AST_SIMPLEEXP ); if( compile_error.state ) return index;
			node.expression.simpleexp_list = -1;
			int list_tail = -1;
			ast_type_t type = ast_get_type( ctx, node.expression.primary );

			while( true )
				{		
				token_t* token = get_token( ctx );
				token_t* next_token = peek_token( ctx );
	
				ast_simpleexp_t::optype_t op = (ast_simpleexp_t::optype_t) -1;
				if( token->type == TOKEN_SYMBOL )
					{
					if( token->symbol == '=' ) op = ast_simpleexp_t::OP_EQ;
					else if( token->symbol == '<' )
						{
							 if( next_token->type == TOKEN_SYMBOL && next_token->symbol == '>' ) { op = ast_simpleexp_t::OP_NE; get_token( ctx ); }
						else if( next_token->type == TOKEN_SYMBOL && next_token->symbol == '=' ) { op = ast_simpleexp_t::OP_LE; get_token( ctx ); }
						else op = ast_simpleexp_t::OP_LT;
						}
					else if( token->symbol == '>' )
						{
						if( next_token->type == TOKEN_SYMBOL && next_token->symbol == '=' ) { op = ast_simpleexp_t::OP_GE; get_token( ctx ); }
						else op = ast_simpleexp_t::OP_GT;
						}
					else
						{
						rewind_token( ctx );
						break;
						}
					}
				else
					{
					rewind_token( ctx );
					break;
					}

				if( op == -1 ) { rewind_token( ctx ); return index; }

				int simpleexp = parse( ctx, AST_SIMPLEEXP ); if( compile_error.state ) return index;
				ctx->node_data[ simpleexp ].simpleexp.op = op;

				if( ast_get_type( ctx, simpleexp ) != type )
					{
					parser_error( "Type mismatch", token->pos );
					return index;
					}

				int list_next = parse( ctx, AST_LIST_NODE ); if( compile_error.state ) return index;
				if( list_tail >= 0 ) ctx->node_data[ list_tail ].list_node.next = list_next;
				else node.expression.simpleexp_list = list_next;

				ctx->node_data[ list_next ].list_node.item = simpleexp;
				ctx->node_data[ list_next ].list_node.next = -1;

				list_tail = list_next;
				}

			} break;

		case AST_SIMPLEEXP:
			// Term
			//		repeated:
			// + - OR XOR
			// Term
			{
			node.simpleexp.primary = parse( ctx, AST_TERM ); if( compile_error.state ) return index;
			node.simpleexp.term_list = -1;
			int list_tail = -1;
			ast_type_t type = ast_get_type( ctx, node.simpleexp.primary );

			while( true )
				{		
				token_t* token = get_token( ctx );
	
				ast_term_t::optype_t op = (ast_term_t::optype_t) -1;
				if( token->type == TOKEN_SYMBOL )
					{
					if( token->symbol == '+' ) op = ast_term_t::OP_ADD;
					else if( token->symbol == '-' ) op = ast_term_t::OP_SUB;
					}
				else if( token->type == TOKEN_IDENTIFIER )
					{
					if( token->identifier == ctx->keyword_OR ) op = ast_term_t::OP_OR;
					else if( token->identifier == ctx->keyword_XOR ) op = ast_term_t::OP_XOR;
					}

				if( op == -1 ) { rewind_token( ctx ); return index; }

				int term = parse( ctx, AST_TERM ); if( compile_error.state ) return index;
				ctx->node_data[ term ].term.op = op;

				if( ast_get_type( ctx, term ) != type )
					{
					parser_error( "Type mismatch", token->pos );
					return index;
					}

				int list_next = parse( ctx, AST_LIST_NODE ); if( compile_error.state ) return index;
				if( list_tail >= 0 ) ctx->node_data[ list_tail ].list_node.next = list_next;
				else node.simpleexp.term_list = list_next;

				ctx->node_data[ list_next ].list_node.item = term;
				ctx->node_data[ list_next ].list_node.next = -1;

				list_tail = list_next;
				}
			} break;

		case AST_TERM:
			// Factor
			//		repeated:
			//  *  /  MOD  AND 
			// Factor
			{
			node.term.primary = parse( ctx, AST_FACTOR ); if( compile_error.state ) return index;
			node.term.factor_list = -1;
			int list_tail = -1;
			ast_type_t type = ast_get_type( ctx, node.term.primary );

			while( true )
				{		
				token_t* token = get_token( ctx );
	
				ast_factor_t::optype_t op = (ast_factor_t::optype_t) -1;
				if( token->type == TOKEN_SYMBOL )
					{
					if( token->symbol == '*' ) op = ast_factor_t::OP_MUL;
					else if( token->symbol == '/' ) op = ast_factor_t::OP_DIV;
					}
				else if( token->type == TOKEN_IDENTIFIER )
					{
					if( token->identifier == ctx->keyword_MOD ) op = ast_factor_t::OP_MOD;
					else if( token->identifier == ctx->keyword_AND ) op = ast_factor_t::OP_AND;
					}

				if( op == -1 ) { rewind_token( ctx ); return index; }

				int factor = parse( ctx, AST_FACTOR ); if( compile_error.state ) return index;
				ctx->node_data[ factor ].factor.op = op;

				if( ast_get_type( ctx, factor ) != type )
					{
					parser_error( "Type mismatch", token->pos );
					return index;
					}

				int list_next = parse( ctx, AST_LIST_NODE ); if( compile_error.state ) return index;
				if( list_tail >= 0 ) ctx->node_data[ list_tail ].list_node.next = list_next;
				else node.term.factor_list = list_next;

				ctx->node_data[ list_next ].list_node.item = factor;
				ctx->node_data[ list_next ].list_node.next = -1;

				list_tail = list_next;
				}
			} break;

		case AST_FACTOR:
			// Variable   Literal   MethodCall   ConstantIdentifier   UnaryExp   (Expression)
			{
			token_t* token = get_token( ctx );
			if( token->type == TOKEN_SYMBOL && token->symbol == '(' )
				{
				node.factor.primary = parse( ctx, AST_EXPRESSION ); if( compile_error.state ) return index;
				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL || token->symbol != ')' )
					{
					parser_error( "')' expected", token->pos );
					return index;
					}
				}
			else if( ( token->type == TOKEN_SYMBOL && token->symbol == '-' ) || ( token->type == TOKEN_IDENTIFIER && token->identifier == ctx->keyword_NOT ) )
				{
				rewind_token( ctx );
				node.factor.primary = parse( ctx, AST_UNARYEXP ); if( compile_error.state ) return index;
				}
			else if( token->type == TOKEN_INT )
				{
				rewind_token( ctx );
				node.factor.primary = parse( ctx, AST_INTEGER ); if( compile_error.state ) return index;
				}
			else if( token->type == TOKEN_FLOAT )
				{
				rewind_token( ctx );
				node.factor.primary = parse( ctx, AST_FLOAT ); if( compile_error.state ) return index;
				}
			else if( token->type == TOKEN_STRING )
				{
				rewind_token( ctx );
				node.factor.primary = parse( ctx, AST_STRING ); if( compile_error.state ) return index;
				}
			else if( token->type == TOKEN_IDENTIFIER && peek_token( ctx )->type == TOKEN_SYMBOL && peek_token( ctx )->symbol == '(' && var_dimensions( ctx, token->identifier ) == 0 ) 
				{
				rewind_token( ctx );
				node.factor.primary = parse( ctx, AST_FUNCCALL ); if( compile_error.state ) return index;
				}
			else if( token->type == TOKEN_IDENTIFIER )
				{
				rewind_token( ctx );
				node.factor.primary = parse( ctx, AST_VARIABLE ); if( compile_error.state ) return index;
				}
			else
				{
				parser_error( "Factor expected", token->pos );
				}
			} break;

		case AST_UNARYEXP:
			// -Factor   NOT Factor
			{
			token_t* token = get_token( ctx );
			if( token->type == TOKEN_SYMBOL && token->symbol == '-' ) 
				{
				node.unaryexp.op = ast_unaryexp_t::OP_NEG;
				node.unaryexp.factor = parse( ctx, AST_FACTOR ); if( compile_error.state ) return index;
				}
			else if ( token->type == TOKEN_IDENTIFIER && token->identifier == ctx->keyword_NOT ) 
				{
				node.unaryexp.op = ast_unaryexp_t::OP_NOT;
				node.unaryexp.factor = parse( ctx, AST_FACTOR ); if( compile_error.state ) return index;
				}
			else
				{
				parser_error( "'-' or 'NOT' expected", token->type );
				}
			} break;

		case AST_FLOAT:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_FLOAT )
				{
				parser_error( "Float expected", token->pos ); 
				return index;
				}
			node.float_.value = token->float_val;
			} break;

		case AST_INTEGER:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_INT )
				{
				parser_error( "Integer expected", token->pos ); 
				return index;
				}
			node.integer.value = token->int_val;
			} break;

		case AST_STRING:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_STRING )
				{
				parser_error( "String expected", token->pos ); 
				return index;
				}
			node.string.handle = token->string_val;
			} break;

		case AST_VARIABLE:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER )
				{
				parser_error( "Identifier expected", token->pos ); 
				return index;
				}

			node.variable.index = map_var( ctx, token->identifier );
			node.variable.offset_a = -1;
			node.variable.offset_b = -1;

			int var_dim = var_dimensions( ctx, token->identifier );
			if( var_dim > 0 )
				{
				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL || token->symbol != '(' )
					{
					parser_error( "'(' expected", token->pos ); 
					return index;
					}

				node.variable.offset_a = parse( ctx, AST_EXPRESSION );

				if( var_dim > 1 )
					{
					token = get_token( ctx );
					if( token->type != TOKEN_SYMBOL || token->symbol != ',' )
						{
						parser_error( "',' expected", token->pos ); 
						return index;
						}

					node.variable.offset_b = parse( ctx, AST_EXPRESSION );
					}

				token = get_token( ctx );
				if( token->type != TOKEN_SYMBOL || token->symbol != ')' )
					{
					parser_error( "')' expected", token->pos ); 
					return index;
					}
				}
			
			if( ctx->vars[ node.variable.index ].type == AST_TYPE_NONE )
				{
				ctx->vars[ node.variable.index ].type = AST_TYPE_INTEGER;
				}
			} break;
		
		case AST_END:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || ( token->identifier != ctx->keyword_END && token->identifier != ctx->keyword_STOP ) )
				{
				parser_error( "'END' or 'STOP' expected", token->pos ); 
				return index;
				}
			} break;

		case AST_TRON:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_TRON )
				{
				parser_error( "'TRON", token->pos ); 
				return index;
				}
			} break;

		case AST_TROFF:
			{
			token_t* token = get_token( ctx );
			if( token->type != TOKEN_IDENTIFIER || token->identifier != ctx->keyword_TROFF )
				{
				parser_error( "'TROFF", token->pos ); 
				return index;
				}
			} break;

        case AST_LIST_NODE:
            break;
		}	

	return index;
	#undef node
	}


static void resolve_targets( parser_context_t* ctx )
	{
	for( int i = 0; i < ctx->node_count; ++i )
		{
		if( ctx->node_type[ i ] == AST_BRANCH )
			{
			if( ctx->node_data[ i ].branch.type == ast_branch_t::BRANCH_LOOP )
				{
				int target = ctx->node_data[ i ].branch.target;
				ctx->node_data[ i ].branch.target = ctx->jump_targets_count;
				if( ctx->jump_targets_count >= ctx->jump_targets_capacity )
					{
					ctx->jump_targets_capacity *= 2;
					ctx->jump_targets = (int*) realloc( ctx->jump_targets, sizeof( *ctx->jump_targets ) * ctx->jump_targets_capacity );
					assert( ctx->jump_targets );
					}
				ctx->jump_targets[ ctx->jump_targets_count++ ] = target;
				goto next_node;
				}
			else
				{	
				for( int j = 0; j < ctx->line_map_count; ++j )
					{
					if( ctx->line_map[ j ].line_number == ctx->node_data[ i ].branch.target )
						{
						ctx->node_data[ i ].branch.target = ctx->jump_targets_count;
						if( ctx->jump_targets_count >= ctx->jump_targets_capacity )
							{
							ctx->jump_targets_capacity *= 2;
							ctx->jump_targets = (int*) realloc( ctx->jump_targets, sizeof( *ctx->jump_targets ) * ctx->jump_targets_capacity );
							assert( ctx->jump_targets );
							}
						ctx->jump_targets[ ctx->jump_targets_count++ ] = ctx->line_map[ j ].node_index;
						goto next_node;
						}
					}
				}
			parser_error( "Invalid jump target", ctx->node_pos[ i ] );
			return;
		next_node: ;
			}
		else if( ctx->node_type[ i ] == AST_RESTORE )
			{
			int line = ctx->node_data[ i ].restore.index;
			for( int j = 0; j < ctx->data_count; ++j )
				{
				if( ctx->data_lines[ j ] == line )
					{
					ctx->node_data[ i ].restore.index = j;
					goto next_node2;
					}
				}
			parser_error( "Invalid restore target", ctx->node_pos[ i ] );
			return;
		next_node2: ;
			}
		}
	}



static u32 upper_power_of_two( u32 v )
	{
	--v;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	++v;
	v += ( v == 0 );
	return v;
	}


static void parse_host_signatures( host_funcs_t* host_funcs, char const** host_func_signatures, int host_func_count, strpool_t* pool )
	{
	struct local
		{
		static char const* skip_nonalnum( char const* str )
			{
			if( !str ) return 0;
			while( *str && !isalnum( *str ) ) ++str;
			return *str ? str : 0;
			}

		static char const* skip_identifier( char const* str )
			{
			if( !str ) return 0;
			while( *str && isalnum( *str ) ) ++str;
			return *str ? str : 0;
			}

		static bool is_identifier( char const* str, char const* identifier )
			{
			if( !str ) return false;
			char const* end = skip_identifier( str );
			int length = (int)( end - str );
			if( length != (int) strlen( identifier ) ) return false;
			return strnicmp( str, identifier, (size_t) length ) == 0;
			}

		static u32 get_identifier( char const* str, strpool_t* pool )
			{
			if( !str ) return false;
			char const* end = skip_identifier( str );
			int length = (int)( end - str );
			return (u32) strpool_inject( pool, str, length );
			}

		static ast_type_t get_type( char const* str )
			{
			     if( is_identifier( str, "Integer" ) ) return AST_TYPE_INTEGER;
			else if( is_identifier( str, "Real" ) ) return AST_TYPE_FLOAT;
			else if( is_identifier( str, "String" ) ) return AST_TYPE_STRING;
			else if( is_identifier( str, "Bool" ) ) return AST_TYPE_BOOL;
			else return AST_TYPE_NONE;
			}

		static bool parse_args( char const* str, host_funcs_t* host_funcs, int index )
			{
			if( !str ) return false;
			str = skip_nonalnum( str );
			host_funcs->funcs[ index ].arg_start = host_funcs->arg_count;
			int arg_count = 0;
			while( str )
				{
				if( host_funcs->arg_count >= host_funcs->arg_capacity )
					{
					host_funcs->arg_capacity *= 2;
					host_funcs->arg_types = (ast_type_t*) realloc( host_funcs->arg_types, sizeof( *host_funcs->arg_types ) * host_funcs->arg_capacity );
					assert( host_funcs->arg_types );
					}
				ast_type_t type = get_type( str );
				if( type == AST_TYPE_NONE ) return false;
				host_funcs->arg_types[ host_funcs->arg_count++ ] = type;
				str = skip_identifier( str );
				str = skip_nonalnum( str );
				++arg_count;
				}
			host_funcs->funcs[ index ].arg_count = arg_count;
			return true;
			}
		};

	host_funcs->func_count = host_func_count;
	host_funcs->funcs = (host_funcs_t::func_t*) malloc( sizeof( *host_funcs->funcs ) * host_funcs->func_count );
	assert( host_funcs->funcs );

	host_funcs->arg_capacity = host_func_count * 4; // Guessing 4 args per func, on average
	host_funcs->arg_count = 0;
	host_funcs->arg_types = (ast_type_t*) malloc( sizeof( *host_funcs->arg_types ) * host_funcs->arg_capacity );
	assert( host_funcs->arg_types );


	for( int i = 0; i < host_func_count; ++i )
		{
		char const* str = host_func_signatures[ i ];
		str = local::skip_nonalnum( str );
		if( !str ) { parser_error( "Invalid host function signature", -( i + 1 ) ); goto cleanup; }
		if( local::is_identifier( str, "Proc") || local::is_identifier( str, "Procedure") )
			{
			str = local::skip_identifier( str );
			str = local::skip_nonalnum( str );
			if( !str ) { parser_error( "Invalid host function signature", -( i + 1 ) ); goto cleanup; }
			host_funcs->funcs[ i ].ret_type = AST_TYPE_NONE;
			host_funcs->funcs[ i ].identifier = local::get_identifier( str, pool );
			str = local::skip_identifier( str );
			if( !local::parse_args( str, host_funcs, i ) ) { parser_error( "Invalid host function signature", -( i + 1 ) ); goto cleanup; }
			}
		else if( local::is_identifier( str, "Func") || local::is_identifier( str, "Function") )
			{
			str = local::skip_identifier( str );
			str = local::skip_nonalnum( str );
			if( !str ) { parser_error( "Invalid host function signature", -( i + 1 ) ); goto cleanup; }
			host_funcs->funcs[ i ].ret_type = local::get_type( str );
			if( host_funcs->funcs[ i ].ret_type == AST_TYPE_NONE ) { parser_error( "Invalid host function signature", -( i + 1 ) ); goto cleanup; }
			str = local::skip_identifier( str );
			str = local::skip_nonalnum( str );
			if( !str ) { parser_error( "Invalid host function signature", -( i + 1 ) ); goto cleanup; }
			host_funcs->funcs[ i ].identifier = local::get_identifier( str, pool );
			str = local::skip_identifier( str );
			if( !local::parse_args( str, host_funcs, i ) ) { parser_error( "Invalid host function signature", -( i + 1 ) ); goto cleanup; }
			}
		else { parser_error( "Invalid host function signature", -( i + 1 ) );  goto cleanup; }
		}

	return;

cleanup:
	free( host_funcs->arg_types );
	free( host_funcs->funcs );
	}


struct ast_t
	{
	ast_node_t* node_type;
	ast_data_t* node_data;
	int* pos;
	int* jump_targets;
	int jump_targets_count;
	int var_size;
	ast_var_t* vars;
	void* data;
	int data_size;
	};


static ast_t parse( token_t* tokens, int length, char const** host_func_signatures, int host_func_count, strpool_t* identifier_pool )
	{
	ast_t ast;
	ast.node_type = 0;
	ast.node_data = 0;
	ast.pos = 0;
	ast.vars = 0;
	ast.jump_targets = 0;
	ast.data = 0;

	parser_context_t ctx;

	#define MAKE_KEYWORD( x ) ( (u32) strpool_inject( identifier_pool, ( x ), sizeof( x ) - 1) ); 
	ctx.keyword_DIM		= MAKE_KEYWORD( "DIM" );
	ctx.keyword_GOTO	= MAKE_KEYWORD( "GOTO" );
	ctx.keyword_GOSUB	= MAKE_KEYWORD( "GOSUB" );
	ctx.keyword_RETURN	= MAKE_KEYWORD( "RETURN" );
	ctx.keyword_FOR		= MAKE_KEYWORD( "FOR" );
	ctx.keyword_TO		= MAKE_KEYWORD( "TO" );
	ctx.keyword_STEP	= MAKE_KEYWORD( "STEP" );
	ctx.keyword_NEXT	= MAKE_KEYWORD( "NEXT" );
	ctx.keyword_IF		= MAKE_KEYWORD( "IF" );
	ctx.keyword_THEN	= MAKE_KEYWORD( "THEN" );
	ctx.keyword_OR		= MAKE_KEYWORD( "OR" );
	ctx.keyword_XOR		= MAKE_KEYWORD( "XOR" );
	ctx.keyword_MOD		= MAKE_KEYWORD( "MOD" );
	ctx.keyword_AND		= MAKE_KEYWORD( "AND" );
	ctx.keyword_NOT		= MAKE_KEYWORD( "NOT" );
	ctx.keyword_DATA	= MAKE_KEYWORD( "DATA" );
	ctx.keyword_READ	= MAKE_KEYWORD( "READ" );
	ctx.keyword_RESTORE = MAKE_KEYWORD( "RESTORE" );
	ctx.keyword_TRUE	= MAKE_KEYWORD( "TRUE" );
	ctx.keyword_FALSE	= MAKE_KEYWORD( "FALSE" );
	ctx.keyword_END		= MAKE_KEYWORD( "END" );
	ctx.keyword_STOP	= MAKE_KEYWORD( "STOP" );
	ctx.keyword_TRON	= MAKE_KEYWORD( "TRON" );
	ctx.keyword_TROFF	= MAKE_KEYWORD( "TROFF" );
	#undef MAKE_KEYWORD

	parse_host_signatures( &ctx.host_funcs, host_func_signatures, host_func_count, identifier_pool );
	if( compile_error.state ) return ast;

	ctx.tokens = tokens;
	ctx.pos = 0;

	ctx.node_capacity = (int) upper_power_of_two( (u32) ( length / 1.5f ) + 1 ); // First guess, which is probably more than enough
	ctx.node_count = 0;
	ctx.node_type = (ast_node_t*) malloc( sizeof( *ctx.node_type ) * ctx.node_capacity );
	assert( ctx.node_type );
	ctx.node_data = (ast_data_t*) malloc( sizeof( *ctx.node_data ) * ctx.node_capacity );
	assert( ctx.node_data );
	ctx.node_pos = (int*) malloc( sizeof( *ctx.node_pos ) * ctx.node_capacity );
	assert( ctx.node_pos );


	ctx.line_map_capacity = (int) upper_power_of_two( (u32) ( length / 20.0f ) + 1 );
	ctx.line_map_count = 0;
	ctx.line_map = (parser_context_t::line_map_t*) malloc( sizeof( *ctx.line_map ) * ctx.line_map_capacity );
	assert( ctx.line_map );

	ctx.var_size = 0;
	ctx.vars_capacity = (int) upper_power_of_two( (u32) ( length / 20.0f ) + 1 );
	ctx.vars_count = 0;
	ctx.vars = (ast_var_t*) malloc( sizeof( *ctx.vars ) * ctx.vars_capacity );
	assert( ctx.vars );
	ctx.vars_identifiers = (u32*) malloc( sizeof( *ctx.vars_identifiers ) * ctx.vars_capacity );
	assert( ctx.vars_identifiers );

	ctx.jump_targets_capacity = (int) upper_power_of_two( (u32) ( length / 20.0f ) + 1 );
	ctx.jump_targets_count = 0;
	ctx.jump_targets = (int*) malloc( sizeof( *ctx.jump_targets ) * ctx.jump_targets_capacity );
	assert( ctx.jump_targets );

	ctx.loop_stack_capacity = (int) upper_power_of_two( (u32) ( length / 20.0f ) + 1 );
	ctx.loop_stack_count = 0;
	ctx.loop_stack = (parser_context_t::loop_stack_t*) malloc( sizeof( *ctx.loop_stack ) * ctx.loop_stack_capacity );
	assert( ctx.loop_stack );

	ctx.data_capacity = (int) upper_power_of_two( (u32) ( length / 20.0f ) + 1 );
	ctx.data_count = 0;
	ctx.data = (parser_context_t::data_t*) malloc( sizeof( *ctx.data ) * ctx.data_capacity );
	assert( ctx.data );
	ctx.data_lines = (int*) malloc( sizeof( *ctx.data_lines ) * ctx.data_capacity );
	assert( ctx.data_lines );

	ctx.identifier_pool = identifier_pool;

	parse( &ctx, AST_PROGRAM );

	if( !compile_error.state ) resolve_targets( &ctx );
	
	free( ctx.host_funcs.funcs );
	free( ctx.host_funcs.arg_types );
	free( ctx.vars_identifiers );
	free( ctx.loop_stack );
	free( ctx.line_map );
	free( ctx.data_lines );
	
	if( compile_error.state ) 
		{
		free( ctx.data );
		free( ctx.vars );
		free( ctx.node_type );
		free( ctx.node_data );
		free( ctx.node_pos );
		free( ctx.jump_targets );
		return ast;
		}
	
	ast.node_type = ctx.node_type;
	ast.node_data = ctx.node_data;
	ast.pos = ctx.node_pos;
	ast.jump_targets = ctx.jump_targets;
	ast.jump_targets_count = ctx.jump_targets_count;
	ast.var_size = ctx.var_size;
	ast.vars = ctx.vars;
	ast.data = ctx.data;
	ast.data_size = (int)( sizeof( *ctx.data ) * ctx.data_count );
	return ast;
	}


//////// PARSER END ////////

    

//////// EMITTER BEGIN ////////


static void emitter_error( char const* message, int pos )
	{
	assert( !compile_error.state && "Overwriting error message" );
	strcpy( compile_error.message, message );
	compile_error.pos = pos;
	compile_error.state = true;
	}

struct emitter_context_t
	{
	ast_t ast;
	u32* code;
	int* code_map;
	int count;
	int capacity;

    u32* opcode;

	int* jump_targets;
	
	int* jump_sites;
	int jump_sites_count;
	int jump_sites_capacity;
	};


static ast_type_t ast_get_type( const emitter_context_t* ctx, int index )
	{
	return ast_get_type( ctx->ast.node_type, ctx->ast.node_data, ctx->ast.vars, index );
	}


static void emit_val( emitter_context_t* ctx, u32 value, int index )
	{
	if( ctx->count >= ctx->capacity )
		{
		ctx->capacity *= 2;
		ctx->code = (u32*) realloc( ctx->code, ctx->capacity * sizeof( *ctx->code ) );
		assert( ctx->code );
		ctx->code_map = (int*) realloc( ctx->code_map, ctx->capacity * sizeof( *ctx->code_map ) );
		assert( ctx->code_map );
		}
	ctx->code[ ctx->count ] = value;
	ctx->code_map[ ctx->count ] = ctx->ast.pos[ index ];
	++ctx->count;
	}


static void emit_val( emitter_context_t* ctx, int value, int index )
	{
	emit_val( ctx, (u32)value, index );
	}


static void emit_val( emitter_context_t* ctx, float value, int index )
	{
	emit_val( ctx, *(u32*)&value, index );
	}


static void emit( emitter_context_t* ctx, int const index )
	{
	#define node ctx->ast.node_data[ index ]

	switch( ctx->ast.node_type[ index ] )
		{
		case AST_PROGRAM:
			{
			int list = node.program.line_list;
			while( list >= 0 )
				{
				int line = ctx->ast.node_data[ list ].list_node.item;
				emit( ctx, line );
				list = ctx->ast.node_data[ list ].list_node.next;
				}		
			} break;

		case AST_LINE:
			{
			for( int i = 0; i < ctx->ast.jump_targets_count; ++i )
				{
				if( ctx->ast.jump_targets[ i ] == index )
					{
					assert( ctx->jump_targets[ i ] == -1 );
					ctx->jump_targets[ i ] = ctx->count;
					}
				}
	
			if( node.line.statement >= 0 )
				emit( ctx, node.line.statement );
			} break;

		case AST_FUNCCALL:
		case AST_PROCCALL:
			{
			int list = node.proccall.arg_list;
			while( list >= 0 )
				{
				int arg = ctx->ast.node_data[ list ].list_node.item;
				emit( ctx, arg );
				list = ctx->ast.node_data[ list ].list_node.next;
				}		
			emit_val( ctx, node.proccall.id, index );
			} break;

		case AST_READ:
			{
			int list = node.read.var_list;
			while( list >= 0 )
				{
				int var_index = ctx->ast.node_data[ list ].list_node.item;

				ast_var_t* var = &ctx->ast.vars[ ctx->ast.node_data[ var_index ].variable.index ];
				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, ctx->ast.pos[ index ], index );
				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, var->globals_index, index );

				if( ctx->ast.node_data[ var_index ].variable.offset_a >= 0 )
					{
					emit( ctx, ctx->ast.node_data[ var_index ].variable.offset_a );

					int dim_a = ctx->ast.vars[ ctx->ast.node_data[ var_index ].variable.index ].dim_a;
					int dim_b = ctx->ast.vars[ ctx->ast.node_data[ var_index ].variable.index ].dim_b;

					if( ctx->ast.node_data[ var_index ].variable.offset_b >= 0 )
						{
						emit( ctx, ctx->ast.node_data[ var_index ].variable.offset_b );
						emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
						emit_val( ctx, (u32) dim_a + 1, index );
						emit_val( ctx, ctx->opcode[ COMPILE_OP_MUL ], index );
						emit_val( ctx, ctx->opcode[ COMPILE_OP_ADD ], index );
						}
			
					int min_subscript = 0;
					int max_subscript = dim_b > 0 ? ( dim_a + 1 ) * ( dim_b + 1 ) - 1 : dim_a;

					emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
					emit_val( ctx, (u32) min_subscript, index );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
					emit_val( ctx, (u32) max_subscript, index );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
					emit_val( ctx, (u32) ctx->ast.pos[ index ], index );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_RTSS ], index );

					emit_val( ctx, ctx->opcode[ COMPILE_OP_ADD ], index );
					}
				
				switch( var->type )
					{
					case AST_TYPE_INTEGER:  emit_val( ctx, ctx->opcode[ COMPILE_OP_READ ], index ); break;
					case AST_TYPE_FLOAT:    emit_val( ctx, ctx->opcode[ COMPILE_OP_READF ], index ); break;
					case AST_TYPE_BOOL:		emit_val( ctx, ctx->opcode[ COMPILE_OP_READB ], index ); break;
					case AST_TYPE_STRING:	emit_val( ctx, ctx->opcode[ COMPILE_OP_READC ], index ); break;
                    case AST_TYPE_NONE:
					default: emitter_error( "Invalid type", ctx->ast.pos[ index ] ); return;				
					}
			
				list = ctx->ast.node_data[ list ].list_node.next;
				}		
			} break;

		case AST_RESTORE:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
			emit_val( ctx, node.restore.index, index );
			emit_val( ctx, ctx->opcode[ COMPILE_OP_RSTO ], index );
			} break;

		case AST_BRANCH:
			{
			if( ctx->jump_sites_count >= ctx->jump_sites_capacity )
				{
				ctx->jump_sites_capacity *= 2;
				ctx->jump_sites = (int*) realloc( ctx->jump_sites, sizeof( *ctx->jump_sites ) * ctx->jump_sites_capacity );
				assert( ctx->jump_sites );
				}

			emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
			ctx->jump_sites[ ctx->jump_sites_count++ ] = ctx->count;
			emit_val( ctx, node.branch.target, index );
			switch( node.branch.type )
				{
			    case ast_branch_t::BRANCH_JUMP: emit_val( ctx, ctx->opcode[ COMPILE_OP_JMP ], index ); break;
				case ast_branch_t::BRANCH_SUB:	emit_val( ctx, ctx->opcode[ COMPILE_OP_JSR ], index ); break;
				case ast_branch_t::BRANCH_COND:	emit_val( ctx, ctx->opcode[ COMPILE_OP_JNZ ], index ); break;
				case ast_branch_t::BRANCH_LOOP:	emit_val( ctx, ctx->opcode[ COMPILE_OP_JNZ ], index ); break;
				default: emitter_error( "Invalid branch type", ctx->ast.pos[ index ] ); return;
				}
			} break;

		case AST_RETURN:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_RET ], index );
			} break;

		case AST_LOOP:
			{
			emit( ctx, node.loop.assignment );
			for( int i = 0; i < ctx->ast.jump_targets_count; ++i )
				{
				if( ctx->ast.jump_targets[ i ] == index )
					{
					assert( ctx->jump_targets[ i ] == -1 );
					ctx->jump_targets[ i ] = ctx->count;
					}
				}
			} break;

		case AST_NEXT:
			{
			emit( ctx, node.next.assignment );
			emit( ctx, node.next.condition );
			} break;

		case AST_CONDITION:
			{
			emit( ctx, node.condition.expression );
			emit( ctx, node.condition.branch);
			} break;

		case AST_ASSIGNMENT:
			{
			emit( ctx, node.assignment.expression );
			emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
			emit_val( ctx, ctx->ast.vars[ ctx->ast.node_data[ node.assignment.variable ].variable.index ].globals_index, index );

			if( ctx->ast.node_data[ node.assignment.variable ].variable.offset_a >= 0 )
				{
				emit( ctx, ctx->ast.node_data[ node.assignment.variable ].variable.offset_a );

				int dim_a = ctx->ast.vars[ ctx->ast.node_data[ node.assignment.variable ].variable.index ].dim_a;
				int dim_b = ctx->ast.vars[ ctx->ast.node_data[ node.assignment.variable ].variable.index ].dim_b;

				if( ctx->ast.node_data[ node.assignment.variable ].variable.offset_b >= 0 )
					{
					emit( ctx, ctx->ast.node_data[ node.assignment.variable ].variable.offset_b );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
					emit_val( ctx, (u32) dim_a + 1, index );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_MUL ], index );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_ADD ], index );
					}
			
				int min_subscript = 0;
				int max_subscript = dim_b > 0 ? ( dim_a + 1 ) * ( dim_b + 1 ) - 1 : dim_a;

				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, (u32) min_subscript, index );
				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, (u32) max_subscript, index );
				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, (u32) ctx->ast.pos[ index ], index );
				emit_val( ctx, ctx->opcode[ COMPILE_OP_RTSS ], index );

				emit_val( ctx, ctx->opcode[ COMPILE_OP_ADD ], index );
				}

			ast_type_t type = ast_get_type( ctx, node.assignment.expression );
			switch( type )
				{
				case AST_TYPE_INTEGER:
				case AST_TYPE_FLOAT:
				case AST_TYPE_BOOL:		emit_val( ctx, ctx->opcode[ COMPILE_OP_STORE ], index ); break;
				case AST_TYPE_STRING:	emit_val( ctx, ctx->opcode[ COMPILE_OP_STOREC ], index ); break;
                case AST_TYPE_NONE:
				default: emitter_error( "Invalid type", ctx->ast.pos[ index ] ); return;				
				}

			} break;

		case AST_EXPRESSION:
			{
			ast_type_t type = ast_get_type( ctx, node.expression.primary );
			emit( ctx, node.expression.primary );
			int list = node.expression.simpleexp_list;
			while( list >= 0 )
				{
				int simpleexp = ctx->ast.node_data[ list ].list_node.item;
				emit( ctx, simpleexp );

				switch( type )
					{
					case AST_TYPE_INTEGER:
						{
						switch( ctx->ast.node_data[ simpleexp ].simpleexp.op )
							{
							case ast_simpleexp_t::OP_EQ:	emit_val( ctx, ctx->opcode[ COMPILE_OP_EQS ], index ); break;
							case ast_simpleexp_t::OP_NE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_NES ], index ); break;
							case ast_simpleexp_t::OP_LE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_LES ], index ); break;
							case ast_simpleexp_t::OP_GE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_GES ], index ); break;
							case ast_simpleexp_t::OP_LT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_LTS ], index ); break;
							case ast_simpleexp_t::OP_GT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_GTS ], index ); break;
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_FLOAT:
						{
						switch( ctx->ast.node_data[ simpleexp ].simpleexp.op )
							{
							case ast_simpleexp_t::OP_EQ:	emit_val( ctx, ctx->opcode[ COMPILE_OP_EQF ], index ); break;
							case ast_simpleexp_t::OP_NE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_NEF ], index ); break;
							case ast_simpleexp_t::OP_LE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_LEF ], index ); break;
							case ast_simpleexp_t::OP_GE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_GEF ], index ); break;
							case ast_simpleexp_t::OP_LT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_LTF ], index ); break;
							case ast_simpleexp_t::OP_GT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_GTF ], index ); break;
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_STRING:
						{
						switch( ctx->ast.node_data[ simpleexp ].simpleexp.op )
							{
							case ast_simpleexp_t::OP_EQ:	emit_val( ctx, ctx->opcode[ COMPILE_OP_EQC ], index ); break;
							case ast_simpleexp_t::OP_NE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_NEC ], index ); break;
							case ast_simpleexp_t::OP_LE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_LEC ], index ); break;
							case ast_simpleexp_t::OP_GE:	emit_val( ctx, ctx->opcode[ COMPILE_OP_GEC ], index ); break;
							case ast_simpleexp_t::OP_LT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_LTC ], index ); break;
							case ast_simpleexp_t::OP_GT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_GTC ], index ); break;
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_BOOL:
                    case AST_TYPE_NONE:
					default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
					}

				list = ctx->ast.node_data[ list ].list_node.next;
				}		
			} break;

		case AST_SIMPLEEXP:
			{
			ast_type_t type = ast_get_type( ctx, node.simpleexp.primary );
			emit( ctx, node.simpleexp.primary );
			int list = node.simpleexp.term_list;
			while( list >= 0 )
				{
				int term = ctx->ast.node_data[ list ].list_node.item;
				emit( ctx, term );

				switch( type )
					{
					case AST_TYPE_BOOL:
						{
						switch( ctx->ast.node_data[ term ].term.op )
							{
							case ast_term_t::OP_OR:		emit_val( ctx, ctx->opcode[ COMPILE_OP_ORB ],  index ); break;
							case ast_term_t::OP_XOR:	emit_val( ctx, ctx->opcode[ COMPILE_OP_XORB ], index ); break;
                            case ast_term_t::OP_ADD:
                            case ast_term_t::OP_SUB:
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_INTEGER:
						{
						switch( ctx->ast.node_data[ term ].term.op )
							{
							case ast_term_t::OP_ADD:	emit_val( ctx, ctx->opcode[ COMPILE_OP_ADD ], index ); break;
							case ast_term_t::OP_SUB:	emit_val( ctx, ctx->opcode[ COMPILE_OP_SUB ], index ); break;
							case ast_term_t::OP_OR:		emit_val( ctx, ctx->opcode[ COMPILE_OP_OR ],  index  ); break;
							case ast_term_t::OP_XOR:	emit_val( ctx, ctx->opcode[ COMPILE_OP_XOR ], index ); break;
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_FLOAT:
						{
						switch( ctx->ast.node_data[ term ].term.op )
							{
							case ast_term_t::OP_ADD:	emit_val( ctx, ctx->opcode[ COMPILE_OP_ADDF ], index ); break;
							case ast_term_t::OP_SUB:	emit_val( ctx, ctx->opcode[ COMPILE_OP_SUBF ], index ); break;
                            case ast_term_t::OP_OR:
                            case ast_term_t::OP_XOR:
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_STRING:
						{
						switch( ctx->ast.node_data[ term ].term.op )
							{
							case ast_term_t::OP_ADD:	emit_val( ctx, ctx->opcode[ COMPILE_OP_CATC ], index ); break;
                            case ast_term_t::OP_SUB:
                            case ast_term_t::OP_OR:
                            case ast_term_t::OP_XOR:
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

                    case AST_TYPE_NONE:
					default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
					}

				list = ctx->ast.node_data[ list ].list_node.next;
				}		
			} break;

		case AST_TERM:
			{	
			ast_type_t type = ast_get_type( ctx, node.term.primary );
			emit( ctx, node.term.primary );
			int list = node.term.factor_list;
			while( list >= 0 )
				{
				int factor = ctx->ast.node_data[ list ].list_node.item;
				emit( ctx, factor );

				switch( type )
					{
					case AST_TYPE_BOOL:
						{
						switch( ctx->ast.node_data[ factor ].factor.op )
							{
							case ast_factor_t::OP_AND:	emit_val( ctx, ctx->opcode[ COMPILE_OP_ANDB ], index ); break;
                            case ast_factor_t::OP_MUL:
                            case ast_factor_t::OP_DIV:
                            case ast_factor_t::OP_MOD:
                            default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_INTEGER:
						{
						switch( ctx->ast.node_data[ factor ].factor.op )
							{
							case ast_factor_t::OP_MUL:	emit_val( ctx, ctx->opcode[ COMPILE_OP_MUL ], index ); break;
							case ast_factor_t::OP_DIV:	emit_val( ctx, ctx->opcode[ COMPILE_OP_DIV ], index ); break;
							case ast_factor_t::OP_MOD:	emit_val( ctx, ctx->opcode[ COMPILE_OP_MOD ], index ); break;
							case ast_factor_t::OP_AND:	emit_val( ctx, ctx->opcode[ COMPILE_OP_AND ],  index ); break;
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

					case AST_TYPE_FLOAT:
						{
						switch( ctx->ast.node_data[ factor ].factor.op )
							{
							case ast_factor_t::OP_MUL:	emit_val( ctx, ctx->opcode[ COMPILE_OP_MULF ], index ); break;
							case ast_factor_t::OP_DIV:	emit_val( ctx, ctx->opcode[ COMPILE_OP_DIVF ], index ); break;
							case ast_factor_t::OP_MOD:	emit_val( ctx, ctx->opcode[ COMPILE_OP_MODF ], index ); break;
                            case ast_factor_t::OP_AND:
							default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
							}
						} break;

                    case AST_TYPE_STRING:
                    case AST_TYPE_NONE:
					default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
					}

				list = ctx->ast.node_data[ list ].list_node.next;
				}		
			} break;

		case AST_FACTOR:
			{
			emit( ctx, node.factor.primary );
			} break;

		case AST_UNARYEXP:
			{
			ast_type_t type = ast_get_type( ctx, node.unaryexp.factor );
			emit( ctx, node.unaryexp.factor );
			switch( type )
				{
				case AST_TYPE_INTEGER:
					{
					switch( node.unaryexp.op )
						{
						case ast_unaryexp_t::OP_NEG:	emit_val( ctx, ctx->opcode[ COMPILE_OP_NEG ], index ); break;
						case ast_unaryexp_t::OP_NOT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_NOT ], index ); break;
						default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
						}
					} break;

				case AST_TYPE_BOOL:
					{
					switch( node.unaryexp.op )
						{
						case ast_unaryexp_t::OP_NOT:	emit_val( ctx, ctx->opcode[ COMPILE_OP_NOTB ], index ); break;
                        case ast_unaryexp_t::OP_NEG:	
						default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
						}
					} break;

				case AST_TYPE_FLOAT:
					{
					switch( node.unaryexp.op )
						{
						case ast_unaryexp_t::OP_NEG:	emit_val( ctx, ctx->opcode[ COMPILE_OP_NEGF ], index ); break;
                        case ast_unaryexp_t::OP_NOT:	
						default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
						}
					} break;

                case AST_TYPE_STRING:
                case AST_TYPE_NONE:
				default: emitter_error( "Invalid operation", ctx->ast.pos[ index ] ); return;				
				}
			} break;

		case AST_FLOAT:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
			emit_val( ctx, node.float_.value, index );
			} break;

		case AST_INTEGER:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
			emit_val( ctx, (u32) node.integer.value, index );
			} break;

		case AST_STRING:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSHC ], index );
			emit_val( ctx, (u32) node.string.handle, index );
			} break;

		case AST_VARIABLE:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
			emit_val( ctx, (u32) ctx->ast.vars[ node.variable.index ].globals_index, index );

			if( node.variable.offset_a >= 0 )
				{
				emit( ctx, node.variable.offset_a );

				int dim_a = ctx->ast.vars[ node.variable.index ].dim_a;
				int dim_b = ctx->ast.vars[ node.variable.index ].dim_b;

				if( node.variable.offset_b >= 0 )
					{
					emit( ctx, node.variable.offset_b );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
					emit_val( ctx, (u32) dim_a + 1, index );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_MUL ], index );
					emit_val( ctx, ctx->opcode[ COMPILE_OP_ADD ], index );
					}
			
				int min_subscript = 0;
				int max_subscript = dim_b > 0 ? ( dim_a + 1 ) * ( dim_b + 1 ) - 1: dim_a;

				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, (u32) min_subscript, index );
				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, (u32) max_subscript, index );
				emit_val( ctx, ctx->opcode[ COMPILE_OP_PUSH ], index );
				emit_val( ctx, (u32) ctx->ast.pos[ index ], index );
				emit_val( ctx, ctx->opcode[ COMPILE_OP_RTSS ], index );

				emit_val( ctx, ctx->opcode[ COMPILE_OP_ADD ], index );
				}

			ast_type_t type = ctx->ast.vars[ node.variable.index ].type;
			switch( type )
				{
				case AST_TYPE_INTEGER:
				case AST_TYPE_FLOAT:
				case AST_TYPE_BOOL:		emit_val( ctx, ctx->opcode[ COMPILE_OP_LOAD ],  index ); break;
				case AST_TYPE_STRING:	emit_val( ctx, ctx->opcode[ COMPILE_OP_LOADC ], index ); break;
                case AST_TYPE_NONE:
				default: emitter_error( "Invalid variable type", ctx->ast.pos[ index ] ); return;				
				}
			} break;	

		case AST_END:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_HALT ], index );
			} break;

		case AST_TRON:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_TRON ], index );
			} break;

		case AST_TROFF:
			{
			emit_val( ctx, ctx->opcode[ COMPILE_OP_TROFF ], index );
			} break;

        case AST_LIST_NODE:
		case AST_DIM:
        case AST_DATA:
            break;
		}

	#undef node
	}


static void patch_jumps( emitter_context_t* ctx )
	{
	for( int i = 0; i < ctx->jump_sites_count; ++i )
		{
		u32* jump_site = &ctx->code[ ctx->jump_sites[ i ] ];
		int jump_target = ctx->jump_targets[ *jump_site ];
		if( jump_target < 0 )
			{
			emitter_error( "Could not resolve jump target", ctx->ast.pos[ ctx->ast.jump_targets[ *jump_site ] ] );
			return;
			}
		*jump_site = (u32)( jump_target - ctx->jump_sites[ i ] - 1 );
		}
	}

struct emitted_t
	{
	u32* code;
	int* map;
	int size;
	};

static emitted_t emit( ast_t ast, u32* opcodes )
	{
	emitted_t emitted;
	emitted.code = 0;
	emitted.map = 0;
	emitted.size = 0;

	emitter_context_t ctx;
	ctx.ast = ast;
	ctx.count = 0;
	ctx.capacity = 1024;
	ctx.code = (u32*) malloc( ctx.capacity * sizeof( *ctx.code ) );
	assert( ctx.code );
	ctx.code_map = (int*) malloc( ctx.capacity * sizeof( *ctx.code_map ) );
	assert( ctx.code_map );

    ctx.opcode = opcodes;

	ctx.jump_targets = (int*) malloc( sizeof( *ctx.jump_targets ) * ast.jump_targets_count );
	assert( ctx.jump_targets );
	memset( ctx.jump_targets, 0xff, sizeof( *ctx.jump_targets ) * ast.jump_targets_count );

	ctx.jump_sites_capacity = 1024;
	ctx.jump_sites_count = 0;
	ctx.jump_sites = (int*) malloc( sizeof( *ctx.jump_sites ) * ctx.jump_sites_capacity );
	assert( ctx.jump_sites );

	emit( &ctx, 0 );

	if( !compile_error.state ) patch_jumps( &ctx );
	free( ctx.jump_sites );
	free( ctx.jump_targets );

	if( compile_error.state )
		{
		free( ctx.code );
		free( ctx.code_map );
		return emitted;
		}
	emit_val( &ctx, ctx.opcode[ COMPILE_OP_HALT ], 0 );

	emitted.code = ctx.code;
	emitted.map = ctx.code_map;
	emitted.size = (int)( ctx.count * sizeof( u32 ) );
	return emitted;
	}


//////// EMITTER END ////////



compile_bytecode_t compile( char const* sourcecode, int length, compile_opcode_map_t* opcode_map, int opcode_count,
    char const** host_func_signatures, int host_func_count, int* error_pos, char error_msg[ 256 ] )
	{
	compile_bytecode_t bytecode;
	bytecode.code = 0;
	bytecode.code_size = 0;
	bytecode.data = 0;
	bytecode.data_size = 0;
	bytecode.strings = 0;
	bytecode.string_count = 0;
	bytecode.globals_size = 0;

	strpool_config_t config_identifier = strpool_default_config;
	config_identifier.counter_bits = 0;
	config_identifier.ignore_case = true;
	strpool_t identifier_pool;
	strpool_init( &identifier_pool, &config_identifier );

	strpool_config_t config_str = strpool_default_config;
	config_str.counter_bits = 0;
	strpool_t string_pool;
	strpool_init( &string_pool, &config_str );

	compile_error_clear();

    if( opcode_count != COMPILE_OPCOUNT ) 
        {
		if( error_pos ) *error_pos = 0;
		if( error_msg ) strcpy( error_msg, "opcode_count must equal COMPILE_OPCOUNT" );
		goto cleanup;
        }

    bool opcodes_valid[ COMPILE_OPCOUNT ] = { false };
    u32 opcodes[ COMPILE_OPCOUNT ] = { 0 };
    for( int i = 0; i < opcode_count; ++i )
        {
        int index = (int) opcode_map[ i ].compiler_op;
        if( index < 0 || index >= COMPILE_OPCOUNT )
            {
		    if( error_pos ) *error_pos = 0;
		    if( error_msg ) strcpy( error_msg, "Invalid opcode found in opcode_map" );
		    goto cleanup;
            }
        opcodes[ index ] = opcode_map[ i ].vm_op;
        opcodes_valid[ index ] = true;
        }

    bool all_valid = true;
    for( int i = 0; i < COMPILE_OPCOUNT; ++i ) if( !opcodes_valid[ i ] ) all_valid = false; 
    if( !all_valid )
        {
		if( error_pos ) *error_pos = 0;
		if( error_msg ) strcpy( error_msg, "One or more opcodes missing from opcode_map" );
		goto cleanup;
        }


    ast_t ast;
	ast.node_type = 0;
	ast.node_data = 0;
	ast.pos = 0;
	ast.jump_targets = 0;
	ast.vars = 0;

	emitted_t emit_data;
	emit_data.code = 0;
	emit_data.map = 0;
	emit_data.size = 0;

	token_t* tokens = lex( sourcecode, length, &identifier_pool, &string_pool );
	if( compile_error.state ) 
		{
		assert( !tokens );
		if( error_pos ) *error_pos = compile_error.pos;
		if( error_msg ) strcpy( error_msg, compile_error.message );
		goto cleanup;
		}

	ast = parse( tokens, length, host_func_signatures, host_func_count, &identifier_pool );
	free( tokens );
	if( compile_error.state ) 
		{
		if( error_pos ) *error_pos = compile_error.pos;
		if( error_msg ) strcpy( error_msg, compile_error.message );
		goto cleanup;
		}

	emit_data = emit( ast, opcodes );
	free( ast.vars );
	free( ast.jump_targets );
	free( ast.pos );
	free( ast.node_data );
	free( ast.node_type );
	if( compile_error.state ) 
		{
		assert( !emit_data.code && !emit_data.map );
		free( ast.data );
		if( error_pos ) *error_pos = compile_error.pos;
		if( error_msg ) strcpy( error_msg, compile_error.message );
		goto cleanup;
		}


	bytecode.code = emit_data.code; 
	bytecode.code_size = emit_data.size;
	bytecode.map = emit_data.map;
	bytecode.map_size = emit_data.size;
	bytecode.data = ast.data;
	bytecode.data_size = ast.data_size;
	bytecode.strings = strpool_collate( &string_pool, &bytecode.string_count );
	bytecode.globals_size = (int)( ast.var_size * sizeof( u32 ) );

cleanup:
	strpool_term( &string_pool );
	strpool_term( &identifier_pool );

	return bytecode;
	}


#endif /* COMPILE_IMPLEMENTATION */