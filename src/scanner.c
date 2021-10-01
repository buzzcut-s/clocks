#include "clocks/scanner.h"

#include <stdio.h>
#include <string.h>

#include <clocks/common.h>

typedef struct
{
    const char* start;
    const char* current;
    int         line;
} Scanner;

Scanner scanner;

void init_scanner(const char* source)
{
    scanner.start   = source;
    scanner.current = source;
    scanner.line    = 1;
}

static bool is_alpha(const char c)
{
    return (c >= 'a' && c <= 'z')
           || (c >= 'A' && c <= 'Z')
           || (c == '_');
}

static bool is_digit(const char c)
{
    return c >= '0' && c <= '9';
}

static bool is_at_end()
{
    return *scanner.current == '\0';
}

static char advance()
{
    scanner.current++;
    return scanner.current[-1];
}

static char peek()
{
    return *scanner.current;
}

static char peek_next()
{
    if (is_at_end())
        return '\0';
    return scanner.current[1];
}

static bool match(const char expected)
{
    if (is_at_end())
        return false;

    if (*scanner.current != expected)
        return false;

    scanner.current++;
    return true;
}

static int token_length()
{
    return (int)(scanner.current - scanner.start);
}

static Token make_token(const TokenType type)
{
    Token token;
    token.type   = type;
    token.start  = scanner.start;
    token.length = token_length();
    token.line   = scanner.line;
    return token;
}

static Token error_token(const char* message)
{
    Token token;
    token.type   = TokenError;
    token.start  = message;
    token.length = (int)strlen(message);
    token.line   = scanner.line;
    return token;
}

static void skip_whitespace()
{
    while (true)
    {
        const char c = peek();
        switch (c)
        {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/')
                {
                    while (peek() != '\n' && !is_at_end())
                        advance();
                }
                else
                    return;
                break;
            default:
                return;
        }
    }
}

static TokenType check_keyword(const size_t start, const size_t length,
                               const char* rest, const TokenType type)
{
    if (token_length() == start + length
        && memcmp(scanner.start + start, rest, length) == 0)
    {
        return type;
    }
    return TokenIdentifier;
}

// clang-format off
static TokenType identifier_type()
{
    switch (scanner.start[0])
    {
        case 'a': return check_keyword(1, 2, "nd", TokenAnd);
        case 'c': return check_keyword(1, 4, "lass", TokenClass);
        case 'e': return check_keyword(1, 3, "lse", TokenElse);
        case 'f':
            if (token_length() > 1)
            {
                switch (scanner.start[1])
                {
                    case 'a': return check_keyword(2, 3, "lse", TokenFalse);
                    case 'o': return check_keyword(2, 1, "r", TokenFor);
                    case 'u': return check_keyword(2, 1, "n", TokenFun);
                }
            }
            break;
        case 'i': return check_keyword(1, 1, "f", TokenIf);
        case 'n': return check_keyword(1, 2, "il", TokenNil);
        case 'o': return check_keyword(1, 1, "r", TokenOr);
        case 'p': return check_keyword(1, 4, "rint", TokenPrint);
        case 'r': return check_keyword(1, 5, "eturn", TokenReturn);
        case 's': return check_keyword(1, 4, "uper", TokenSuper);
        case 't':
            if (token_length() > 1)
            {
                switch (scanner.start[1])
                {
                    case 'h': return check_keyword(2, 2, "is", TokenThis);
                    case 'r': return check_keyword(2, 2, "ue", TokenTrue);
                }
            }
            break;
        case 'v': return check_keyword(1, 2, "ar", TokenVar);
        case 'w': return check_keyword(1, 4, "hile", TokenWhile);
    }
    return TokenIdentifier;
}
// clang-format on

static Token identifier()
{
    while (is_alpha(peek()) || is_digit(peek()))
        advance();
    return make_token(identifier_type());
}

static Token number()
{
    while (is_digit(peek()))
        advance();

    if (peek() == '.' && is_digit(peek_next()))
        advance();

    while (is_digit(peek()))
        advance();

    return make_token(TokenNumber);
}

static Token string()
{
    while (peek() != '"' && !is_at_end())
    {
        if (peek() == '\n')
            scanner.line++;
        advance();
    }

    if (is_at_end())
        return error_token("Unterminated string.");

    advance();
    return make_token(TokenString);
}

Token scan_token()
{
    skip_whitespace();

    scanner.start = scanner.current;
    if (is_at_end())
        return make_token(TokenEOF);

    const char c = advance();

    if (is_digit(c))
        return number();

    if (is_alpha(c))
        return identifier();

    // clang-format off
    switch (c)
    {
        case '(': return make_token(TokenLeftParen);
        case ')': return make_token(TokenRightParen);
        case '{': return make_token(TokenLeftBrace);
        case '}': return make_token(TokenRightBrace);
        case ';': return make_token(TokenSemicolon);
        case ',': return make_token(TokenComma);
        case '.': return make_token(TokenDot);
        case '-': return make_token(TokenMinus);
        case '+': return make_token(TokenPlus);
        case '/': return make_token(TokenSlash);
        case '*': return make_token(TokenStar);

        case '!': return make_token(match('=') ? TokenBangEqual : TokenBang);
        case '=': return make_token(match('=') ? TokenEqualEqual : TokenEqual);
        case '<': return make_token(match('=') ? TokenLessEqual : TokenLess);
        case '>': return make_token( match('=') ? TokenGreaterEqual : TokenGreater);

        case '"': return string();

        default: return error_token("Unexpected character.");
    }
    // clang-format on
}
