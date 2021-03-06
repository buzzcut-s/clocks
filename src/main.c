#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/debug.h>
#include <clocks/vm.h>
#include <linenoise/linenoise.h>

static void repl()
{
    linenoiseHistoryLoad(".clocks_history");

    char* line = NULL;
    while ((line = linenoise("clocks > ")) != NULL)
    {
        linenoiseHistoryAdd(line);
        linenoiseHistorySave(".clocks_history");

        interpret(line);
    }
    free(line);
}

static char* read_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    const size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL)
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    const size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size)
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

static void run_file(const char* path)
{
    char* source = read_file(path);

    const InterpretResult result = interpret(source);
    free(source);

    if (result == InterpretCompileError)
        exit(65);
    if (result == InterpretRuntimeError)
        exit(70);
}

int main(int argc, const char* argv[])
{
    init_vm();

    if (argc == 1)
        repl();
    else if (argc == 2)
        run_file(argv[1]);
    else
    {
        fprintf(stderr, "Usage: clocks [path]\n");
        exit(64);
    }

    free_vm();

    return 0;
}
