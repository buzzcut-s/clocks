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

typedef struct
{
    Local locals[UINT8_COUNT];
    int   local_count;
    int   scope_depth;
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

static Chunk* current_chunk()
{
    return compiling_chunk;
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

static void emit_return()
{
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

static void init_compiler(Compiler* compiler)
{
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    current               = compiler;
}

static void end_compiler()
{
    emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
        disassemble_chunk(current_chunk(), "code");
#endif
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
    const uint8_t arg = identifier_constant(&name);

    if (can_assign && match(TokenEqual))
    {
        expression();
        emit_bytes(OpAssignGlobal, arg);
    }
    else
        emit_bytes(OpReadGlobal, arg);
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

const ParseRule RULES[] = {
  [TokenLeftParen]    = {grouping, NULL, PrecNone},
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
  [TokenAnd]          = {NULL, NULL, PrecNone},
  [TokenClass]        = {NULL, NULL, PrecNone},
  [TokenElse]         = {NULL, NULL, PrecNone},
  [TokenFalse]        = {literal, NULL, PrecNone},
  [TokenFor]          = {NULL, NULL, PrecNone},
  [TokenFun]          = {NULL, NULL, PrecNone},
  [TokenIf]           = {NULL, NULL, PrecNone},
  [TokenNil]          = {literal, NULL, PrecNone},
  [TokenOr]           = {NULL, NULL, PrecNone},
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

static void add_local(const Token name)
{
    if (current->local_count == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals[current->local_count++];
    local->name  = name;
    local->depth = current->scope_depth;
}

static void declare_variable()
{
    if (current->scope_depth == 0)
        return;

    const Token* name = &parser.previous;
    for (int i = current->local_count - 1; i >= 0; i--)
    {
        Local* local = &current->locals[i];
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

static void define_variable(uint8_t global)
{
    if (current->scope_depth > 0)
        return;
    emit_bytes(OpDefineGlobal, global);
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
    consume(TokenSemicolon, "Expect ';' after value.");

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
    consume(TokenSemicolon, "Expect ';' after value.");
    emit_byte(OpPop);
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
    }
}

static void declaration()
{
    if (match(TokenVar))
        var_declaration();
    else
        statement();

    if (parser.panic_mode)
        synchronize();
}

bool compile(const char* source, Chunk* chunk)
{
    init_scanner(source);
    Compiler compiler;
    init_compiler(&compiler);
    compiling_chunk   = chunk;
    parser.had_error  = false;
    parser.panic_mode = false;

    advance();

    while (!match(TokenEOF))
        declaration();

    end_compiler();
    return !parser.had_error;
}
