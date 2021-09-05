#include "clocks/compiler.h"

#include <stdio.h>
#include <stdlib.h>

#include <clocks/common.h>
#include <clocks/scanner.h>

typedef struct
{
    Token current;
    Token previous;
    bool  had_error;
    bool  panic_mode;
} Parser;

Parser parser;

static void error_at(Token* token, const char* message)
{
    if (parser.panic_mode)
        return;
    parser.panic_mode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TokenEof)
        fprintf(stderr, " at end");
    else if (token->type == TokenError)
    {}
    else
        fprintf(stderr, " at '%.*s'", token->length, token->start);

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
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

bool compile(const char* source, Chunk* chunk)
{
    init_scanner(source);
    parser.had_error  = false;
    parser.panic_mode = false;

    advance();

    consume(TokenEof, "Expect end of expression.");
    return !parser.had_error;
}
