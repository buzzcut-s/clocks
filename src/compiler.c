#include "clocks/compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/object.h>
#include <clocks/scanner.h>
#include <clocks/value.h>

#ifdef DEBUG_PRINT_CODE
#include <clocks/debug.h>
#endif

typedef struct
{
    Token current;
    Token previous;
    bool  had_error;
    bool  panic_mode;
} Parser;

typedef enum
{
    PrecNone,
    PrecAssignment,  // =
    PrecOr,          // or
    PrecAnd,         // and
    PrecEquality,    // == !=
    PrecComparison,  // < > <= >=
    PrecTerm,        // + -
    PrecFactor,      // * /
    PrecUnary,       // ! -
    PrecCall,        // . ()
    PrecPrimary
} Precedence;

typedef void (*ParseFn)(const bool can_assign);

typedef struct
{
    const ParseFn    prefix;
    const ParseFn    infix;
    const Precedence prec;
} ParseRule;

typedef struct
{
    Token name;
    int   depth;
} Local;

typedef enum
{
    FuncTypeFunction,
    FuncTypeScript
} FunctionType;

typedef struct Compiler
{
    struct Compiler* enclosing;
    ObjFunction*     func;
    FunctionType     type;
    Local            locals[UINT8_COUNT];
    int              local_count;
    int              scope_depth;
} Compiler;

Parser    parser;
Compiler* current = NULL;
Chunk*    compiling_chunk;

static const ParseRule* get_rule(TokenType type);

static void parse_precedence(Precedence prec);

static void expression();
static void statement();
static void declaration();

static uint8_t identifier_constant(const Token* name);
static int     resolve_local(const Compiler* compiler, const Token* name);

static uint8_t argument_list();

static Chunk* current_chunk()
{
    return &current->func->chunk;
}

static void error_at(const Token* token, const char* message)
{
    if (parser.panic_mode)
        return;
    parser.panic_mode = true;
    parser.had_error  = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TokenEOF)
        fprintf(stderr, " at end");
    else if (token->type == TokenError)
    {}
    else
        fprintf(stderr, " at '%.*s'", token->length, token->start);

    fprintf(stderr, ": %s\n", message);
}

static void error(const char* message)
{
    error_at(&parser.previous, message);
}

static void error_at_current(const char* message)
{
    error_at(&parser.current, message);
}

static void advance()
{
    parser.previous = parser.current;
    while (true)
    {
        parser.current = scan_token();
        if (parser.current.type != TokenError)
            break;
        error_at_current(parser.current.start);
    }
}

static void consume(const TokenType type, const char* message)
{
    if (parser.current.type == type)
        advance();
    else
        error_at_current(message);
}

static bool check(const TokenType type)
{
    return parser.current.type == type;
}

static bool match(const TokenType type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

static void emit_byte(const uint8_t byte)
{
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(const uint8_t byte1, const uint8_t byte2)
{
    emit_byte(byte1);
    emit_byte(byte2);
}

static int emit_jump(const uint8_t instruction)
{
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

static void emit_loop(const int loop_start)
{
    emit_byte(OpLoop);

    const int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static void emit_return()
{
    emit_byte(OpNil);
    emit_byte(OpReturn);
}

static uint8_t make_constant(const Value value)
{
    const int constant = add_constant(current_chunk(), value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emit_constant(const Value value)
{
    emit_bytes(OpConstant, make_constant(value));
}

static void patch_jump(const int offset)
{
    const int jump = current_chunk()->count - offset - 2;
    if (jump > UINT16_MAX)
        error("Too much code to jump over.");

    current_chunk()->code[offset]     = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = (jump & 0xff);
}

static void init_compiler(Compiler* compiler, const FunctionType type)
{
    compiler->enclosing   = current;
    compiler->func        = NULL;
    compiler->type        = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->func        = new_function();

    current = compiler;
    if (type != FuncTypeScript)
    {
        current->func->name = copy_string(parser.previous.start,
                                          parser.previous.length);
    }

    Local* local       = &current->locals[current->local_count++];
    local->depth       = 0;
    local->name.start  = "";
    local->name.length = 0;
}

static ObjFunction* end_compiler()
{
    emit_return();
    ObjFunction* func = current->func;
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
        disassemble_chunk(current_chunk(), func->name != NULL
                                             ? func->name->chars
                                             : "<script>");
#endif
    current = current->enclosing;
    return func;
}

static void begin_scope()
{
    current->scope_depth++;
}

static void end_scope()
{
    current->scope_depth--;
    while (current->local_count > 0
           && current->locals[current->local_count - 1].depth > current->scope_depth)
    {
        emit_byte(OpPop);
        current->local_count--;
    }
}

static void grouping(__attribute__((unused)) const bool can_assign)
{
    expression();
    consume(TokenRightParen, "Expect ')' after expression.");
}

static void number(__attribute__((unused)) const bool can_assign)
{
    const double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(__attribute__((unused)) const bool can_assign)
{
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1,
                                      parser.previous.length - 2)));
}

static void named_variable(const Token name, const bool can_assign)
{
    uint8_t read_op   = 0;
    uint8_t assign_op = 0;

    int arg = resolve_local(current, &name);
    if (arg == -1)
    {
        arg       = identifier_constant(&name);
        read_op   = OpReadGlobal;
        assign_op = OpAssignGlobal;
    }
    else
    {
        read_op   = OpReadLocal;
        assign_op = OpAssignLocal;
    }

    if (can_assign && match(TokenEqual))
    {
        expression();
        emit_bytes(assign_op, arg);
    }
    else
        emit_bytes(read_op, arg);
}

static void variable(const bool can_assign)
{
    named_variable(parser.previous, can_assign);
}

static void unary(__attribute__((unused)) const bool can_assign)
{
    const TokenType op_type = parser.previous.type;

    parse_precedence(PrecUnary);

    switch (op_type)
    {
        case TokenMinus:
            emit_byte(OpNegate);
            break;
        case TokenBang:
            emit_byte(OpNot);
            break;
        default:
            return;
    }
}

static void binary(__attribute__((unused)) const bool can_assign)
{
    const TokenType  op_type = parser.previous.type;
    const ParseRule* rule    = get_rule(op_type);

    parse_precedence((Precedence)(rule->prec + 1));

    switch (op_type)
    {
        case TokenBangEqual:
            emit_bytes(OpEqual, OpNot);
            break;
        case TokenEqualEqual:
            emit_byte(OpEqual);
            break;
        case TokenGreater:
            emit_byte(OpGreater);
            break;
        case TokenGreaterEqual:
            emit_bytes(OpLess, OpNot);
            break;
        case TokenLess:
            emit_byte(OpLess);
            break;
        case TokenLessEqual:
            emit_bytes(OpGreater, OpNot);
            break;

        case TokenPlus:
            emit_byte(OpAdd);
            break;
        case TokenMinus:
            emit_byte(OpSubtract);
            break;
        case TokenStar:
            emit_byte(OpMultiply);
            break;
        case TokenSlash:
            emit_byte(OpDivide);
            break;
        default:
            return;
    }
}

static void literal(__attribute__((unused)) const bool can_assign)
{
    switch (parser.previous.type)
    {
        case TokenNil:
            emit_byte(OpNil);
            break;
        case TokenTrue:
            emit_byte(OpTrue);
            break;
        case TokenFalse:
            emit_byte(OpFalse);
            break;
        default:
            return;
    }
}

static void and_fn(__attribute__((unused)) const bool can_assign)
{
    int end_jump = emit_jump(OpJumpIfFalse);
    emit_byte(OpPop);
    parse_precedence(PrecAnd);
    patch_jump(end_jump);
}

static void or_fn(__attribute__((unused)) const bool can_assign)
{
    const int else_jump = emit_jump(OpJumpIfFalse);
    const int end_jump  = emit_jump(OpJump);

    patch_jump(else_jump);
    emit_byte(OpPop);

    parse_precedence(PrecOr);
    patch_jump(end_jump);
}

static void call(__attribute__((unused)) const bool can_assign)
{
    const uint8_t arg_count = argument_list();
    emit_bytes(OpCall, arg_count);
}

const ParseRule RULES[] = {
  [TokenLeftParen]    = {grouping, call, PrecCall},
  [TokenRightParen]   = {NULL, NULL, PrecNone},
  [TokenLeftBrace]    = {NULL, NULL, PrecNone},
  [TokenRightBrace]   = {NULL, NULL, PrecNone},
  [TokenComma]        = {NULL, NULL, PrecNone},
  [TokenDot]          = {NULL, NULL, PrecNone},
  [TokenMinus]        = {unary, binary, PrecTerm},
  [TokenPlus]         = {NULL, binary, PrecTerm},
  [TokenSemicolon]    = {NULL, NULL, PrecNone},
  [TokenSlash]        = {NULL, binary, PrecFactor},
  [TokenStar]         = {NULL, binary, PrecFactor},
  [TokenBang]         = {unary, NULL, PrecNone},
  [TokenBangEqual]    = {NULL, binary, PrecEquality},
  [TokenEqual]        = {NULL, NULL, PrecNone},
  [TokenEqualEqual]   = {NULL, binary, PrecEquality},
  [TokenGreater]      = {NULL, binary, PrecComparison},
  [TokenGreaterEqual] = {NULL, binary, PrecComparison},
  [TokenLess]         = {NULL, binary, PrecComparison},
  [TokenLessEqual]    = {NULL, binary, PrecComparison},
  [TokenIdentifier]   = {variable, NULL, PrecNone},
  [TokenString]       = {string, NULL, PrecNone},
  [TokenNumber]       = {number, NULL, PrecNone},
  [TokenAnd]          = {NULL, and_fn, PrecNone},
  [TokenClass]        = {NULL, NULL, PrecNone},
  [TokenElse]         = {NULL, NULL, PrecNone},
  [TokenFalse]        = {literal, NULL, PrecNone},
  [TokenFor]          = {NULL, NULL, PrecNone},
  [TokenFun]          = {NULL, NULL, PrecNone},
  [TokenIf]           = {NULL, NULL, PrecNone},
  [TokenNil]          = {literal, NULL, PrecNone},
  [TokenOr]           = {NULL, or_fn, PrecNone},
  [TokenPrint]        = {NULL, NULL, PrecNone},
  [TokenReturn]       = {NULL, NULL, PrecNone},
  [TokenSuper]        = {NULL, NULL, PrecNone},
  [TokenThis]         = {NULL, NULL, PrecNone},
  [TokenTrue]         = {literal, NULL, PrecNone},
  [TokenVar]          = {NULL, NULL, PrecNone},
  [TokenWhile]        = {NULL, NULL, PrecNone},
  [TokenError]        = {NULL, NULL, PrecNone},
  [TokenEOF]          = {NULL, NULL, PrecNone},
};

static const ParseRule* get_rule(const TokenType type)
{
    return &RULES[type];
}

static void parse_precedence(const Precedence prec)
{
    advance();

    const ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL)
    {
        error("Expect expression.");
        return;
    }

    const bool can_assign = prec <= PrecAssignment;
    prefix_rule(can_assign);

    while (prec <= get_rule(parser.current.type)->prec)
    {
        advance();
        const ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && match(TokenEqual))
        error("Invalid assignment target.");
}

static uint8_t identifier_constant(const Token* name)
{
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static bool identifiers_equal(const Token* a, const Token* b)
{
    return (a->length != b->length) ? false
                                    : memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(const Compiler* compiler, const Token* name)
{
    for (int i = compiler->local_count - 1; i >= 0; i--)
    {
        const Local* local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name))
        {
            if (local->depth == -1)
                error("Can't read local variable in its own initializer.");
            return i;
        }
    }
    return -1;
}

static void add_local(const Token name)
{
    if (current->local_count == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->local_count++];
    local->name  = name;
    local->depth = -1;
}

static void declare_variable()
{
    if (current->scope_depth == 0)
        return;

    const Token* name = &parser.previous;
    for (int i = current->local_count - 1; i >= 0; i--)
    {
        const Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth)
            break;
        if (identifiers_equal(name, &local->name))
            error("Already a variable with this name in this scope");
    }

    add_local(*name);
}

static uint8_t parse_variable(const char* message)
{
    consume(TokenIdentifier, message);

    declare_variable();
    if (current->scope_depth > 0)
        return 0;

    return identifier_constant(&parser.previous);
}

static void mark_initialized()
{
    if (current->scope_depth == 0)
        return;
    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(const uint8_t global)
{
    if (current->scope_depth > 0)
    {
        mark_initialized();
        return;
    }
    emit_bytes(OpDefineGlobal, global);
}

static uint8_t argument_list()
{
    uint8_t arg_count = 0;
    if (!check(TokenRightParen))
    {
        do
        {
            expression();
            if (arg_count == 255)
                error("Can't have more than 255 arguments.");
            arg_count++;
        }
        while (match(TokenComma));
    }
    consume(TokenRightParen, "Expect ')' after arguments.");
    return arg_count;
}

static void expression()
{
    parse_precedence(PrecAssignment);
}

static void block()
{
    while (!check(TokenRightBrace) && !check(TokenEOF))
        declaration();
    consume(TokenRightBrace, "Expect '}' after block.");
}

static void var_declaration()
{
    const uint8_t global = parse_variable("Expect variable name.");

    if (match(TokenEqual))
        expression();
    else
        emit_byte(OpNil);
    consume(TokenSemicolon, "Expect ';' after variable declaration.");

    define_variable(global);
}

static void function(const FunctionType type)
{
    Compiler compiler;
    init_compiler(&compiler, type);

    begin_scope();
    consume(TokenLeftParen, "Expect '(' after function name.");
    if (!check(TokenRightParen))
    {
        do {
            current->func->arity++;
            if (current->func->arity > 255)
                error_at_current("Can't have more than 255 parameters");

            const uint8_t param = parse_variable("Expect parameter name");
            define_variable(param);
        }
        while (match(TokenComma));
    }
    consume(TokenRightParen, "Expect ')' after function name.");
    consume(TokenLeftBrace, "Expect '{' after function name.");
    block();

    ObjFunction* func = end_compiler();
    emit_bytes(OpConstant, make_constant(OBJ_VAL(func)));
}

static void fun_declaration()
{
    uint8_t global = parse_variable("Expect function name");
    mark_initialized();
    function(FuncTypeFunction);
    define_variable(global);
}

static void print_statement()
{
    expression();
    consume(TokenSemicolon, "Expect ';' after value.");
    emit_byte(OpPrint);
}

static void expression_statement()
{
    expression();
    consume(TokenSemicolon, "Expect ';' after expression.");
    emit_byte(OpPop);
}

static void if_statement()
{
    consume(TokenLeftParen, "Expect '(' after 'if'.");
    expression();
    consume(TokenRightParen, "Expect ')' after condition.");

    const int then_jump = emit_jump(OpJumpIfFalse);
    emit_byte(OpPop);

    statement();

    const int else_jump = emit_jump(OpJump);
    patch_jump(then_jump);
    emit_byte(OpPop);

    if (match(TokenElse))
        statement();

    patch_jump(else_jump);
}

static void while_statement()
{
    const int loop_start = current_chunk()->count;
    consume(TokenLeftParen, "Expect '(' after 'while'.");
    expression();
    consume(TokenRightParen, "Expect ')' after condition.");

    const int exit_jump = emit_jump(OpJumpIfFalse);
    emit_byte(OpPop);

    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OpPop);
}

static void for_statement()
{
    begin_scope();
    consume(TokenLeftParen, "Expect '(' after 'for'.");

    // Initializer clause
    if (match(TokenSemicolon))
    {}
    else if (match(TokenVar))
        var_declaration();
    else
        expression_statement();

    // Just before the condition
    int loop_start = current_chunk()->count;

    // Condition clause
    int exit_jump = -1;
    if (!match(TokenSemicolon))
    {
        expression();
        consume(TokenSemicolon, "Expect ';' after loop condition.");
        exit_jump = emit_jump(OpJumpIfFalse);
        emit_byte(OpPop);
    }

    // Increment clause
    if (!match(TokenRightParen))
    {
        const int body_jump  = emit_jump(OpJump);  // Jump unconditionally over increment
        const int incr_start = current_chunk()->count;

        expression();  // Compile increment expression (only for its side effect)
        emit_byte(OpPop);
        consume(TokenRightParen, "Expect ')' after for clauses.");

        // Jump to right before the condition expr, if there is any.
        emit_loop(loop_start);

        // Change loop_start to point to the offset where the increment expression begins
        // Later, when we emit the loop instruction after the body statement,
        // this will cause it to jump up to the increment expression
        // instead of the top of the loop like it does when there is no increment.
        loop_start = incr_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);

    if (exit_jump != -1)
    {
        patch_jump(exit_jump);
        emit_byte(OpPop);  // Condition
    }

    end_scope();
}

static void return_statement()
{
    if (current->type == FuncTypeScript)
        error("Can't return from top level code.");

    if (match(TokenSemicolon))
        emit_return();
    else
    {
        expression();
        consume(TokenSemicolon, "Expect ';' after return value.");
        emit_byte(OpReturn);
    }
}

static void statement()
{
    if (match(TokenPrint))
        print_statement();
    else if (match(TokenLeftBrace))
    {
        begin_scope();
        block();
        end_scope();
    }
    else if (match(TokenIf))
        if_statement();
    else if (match(TokenWhile))
        while_statement();
    else if (match(TokenFor))
        for_statement();
    else if (match(TokenReturn))
        return_statement();
    else
        expression_statement();
}

static void synchronize()
{
    parser.panic_mode = false;
    while (parser.current.type != TokenEOF)
    {
        if (parser.previous.type == TokenSemicolon)
            return;
        switch (parser.current.type)
        {
            case TokenClass:
            case TokenFun:
            case TokenVar:
            case TokenFor:
            case TokenIf:
            case TokenWhile:
            case TokenPrint:
            case TokenReturn:
                return;
            default:
            {
            }
        }
        advance();
    }
}

static void declaration()
{
    if (match(TokenVar))
        var_declaration();
    else if (match(TokenFun))
        fun_declaration();
    else
        statement();

    if (parser.panic_mode)
        synchronize();
}

ObjFunction* compile(const char* source)
{
    init_scanner(source);
    Compiler compiler;
    init_compiler(&compiler, FuncTypeScript);
    parser.had_error  = false;
    parser.panic_mode = false;

    advance();

    while (!match(TokenEOF))
        declaration();

    ObjFunction* func = end_compiler();
    return parser.had_error ? NULL : func;
}
