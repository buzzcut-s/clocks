#include "clocks/compiler.h"

#include <stdio.h>
#include <stdlib.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/scanner.h>

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

typedef void (*ParseFn)();

typedef struct
{
    ParseFn    prefix;
    ParseFn    infix;
    Precedence prec;
} ParseRule;

Parser parser;
Chunk* compiling_chunk;

static ParseRule* get_rule(TokenType type);

static void parse_precedence(Precedence prec);

static void expression();

static Chunk* current_chunk()
{
    return compiling_chunk;
}

static void error_at(Token* token, const char* message)
{
    if (parser.panic_mode)
        return;
    parser.panic_mode = true;
    parser.had_error  = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TokenEof)
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

static void consume(TokenType type, const char* message)
{
    if (parser.current.type == type)
        advance();
    else
        error_at_current(message);
}

static void emit_byte(uint8_t byte)
{
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2)
{
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_return()
{
    emit_byte(OpReturn);
}

static uint8_t make_constant(Value value)
{
    int constant = add_constant(current_chunk(), value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emit_constant(Value value)
{
    emit_bytes(OpConstant, make_constant(value));
}

static void end_compiler()
{
    emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
        disassemble_chunk(current_chunk(), "code");
#endif
}

static void grouping()
{
    expression();
    consume(TokenRightParen, "Expect ')' after expression.");
}

static void number()
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(value);
}

static void unary()
{
    TokenType op_type = parser.previous.type;

    parse_precedence(PrecUnary);

    switch (op_type)
    {
        case TokenMinus:
            emit_byte(OpNegate);
            break;
        default:
            return;
    }
}

static void binary()
{
    TokenType  op_type = parser.previous.type;
    ParseRule* rule    = get_rule(op_type);

    parse_precedence((Precedence)(rule->prec + 1));

    switch (op_type)
    {
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

ParseRule rules[] = {
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
  [TokenBang]         = {NULL, NULL, PrecNone},
  [TokenBangEqual]    = {NULL, NULL, PrecNone},
  [TokenEqual]        = {NULL, NULL, PrecNone},
  [TokenEqualEqual]   = {NULL, NULL, PrecNone},
  [TokenGreater]      = {NULL, NULL, PrecNone},
  [TokenGreaterEqual] = {NULL, NULL, PrecNone},
  [TokenLess]         = {NULL, NULL, PrecNone},
  [TokenLessEqual]    = {NULL, NULL, PrecNone},
  [TokenIdentifier]   = {NULL, NULL, PrecNone},
  [TokenString]       = {NULL, NULL, PrecNone},
  [TokenNumber]       = {number, NULL, PrecNone},
  [TokenAnd]          = {NULL, NULL, PrecNone},
  [TokenClass]        = {NULL, NULL, PrecNone},
  [TokenElse]         = {NULL, NULL, PrecNone},
  [TokenFalse]        = {NULL, NULL, PrecNone},
  [TokenFor]          = {NULL, NULL, PrecNone},
  [TokenFun]          = {NULL, NULL, PrecNone},
  [TokenIf]           = {NULL, NULL, PrecNone},
  [TokenNil]          = {NULL, NULL, PrecNone},
  [TokenOr]           = {NULL, NULL, PrecNone},
  [TokenPrint]        = {NULL, NULL, PrecNone},
  [TokenReturn]       = {NULL, NULL, PrecNone},
  [TokenSuper]        = {NULL, NULL, PrecNone},
  [TokenThis]         = {NULL, NULL, PrecNone},
  [TokenTrue]         = {NULL, NULL, PrecNone},
  [TokenVar]          = {NULL, NULL, PrecNone},
  [TokenWhile]        = {NULL, NULL, PrecNone},
  [TokenError]        = {NULL, NULL, PrecNone},
  [TokenEof]          = {NULL, NULL, PrecNone},
};

static ParseRule* get_rule(TokenType type)
{
    return &rules[type];
}

static void parse_precedence(Precedence prec)
{
    advance();

    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL)
    {
        error("Expect expression.");
        return;
    }

    prefix_rule();

    while (prec <= get_rule(parser.current.type)->prec)
    {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule();
    }
}

static void expression()
{
    parse_precedence(PrecAssignment);
}

bool compile(const char* source, Chunk* chunk)
{
    init_scanner(source);
    compiling_chunk   = chunk;
    parser.had_error  = false;
    parser.panic_mode = false;

    advance();
    expression();

    consume(TokenEof, "Expect end of expression.");
    end_compiler();
    return !parser.had_error;
}
