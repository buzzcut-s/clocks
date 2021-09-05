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
