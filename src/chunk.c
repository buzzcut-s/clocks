#include "clocks/chunk.h"

#include <stdlib.h>

#include <clocks/memory.h>
#include <clocks/value.h>
#include <clocks/vm.h>

void init_chunk(Chunk* chunk)
{
    chunk->count    = 0;
    chunk->capacity = 0;
    chunk->code     = NULL;
#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
    chunk->line_count    = 0;
    chunk->line_capacity = 0;
#endif
    chunk->lines = NULL;
    init_value_array(&chunk->constants);
}

void free_chunk(Chunk* chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
    FREE_ARRAY(LineStart, chunk->lines, chunk->line_capacity)
#else
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
#endif
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

void write_chunk(Chunk* chunk, const uint8_t byte, const int line)
{
    if (chunk->capacity < chunk->count + 1)
    {
        const int old_capacity = chunk->capacity;

        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code     = GROW_ARRAY(uint8_t, chunk->code,
                                     old_capacity, chunk->capacity);
#ifndef CHUNK_LINE_RUN_LENGTH_ENCODING
        chunk->lines = GROW_ARRAY(int, chunk->lines,
                                  old_capacity, chunk->capacity);
#endif
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
    if (chunk->line_count > 0 && chunk->lines[chunk->line_count - 1].line == line)
        return;

    if (chunk->line_capacity < chunk->line_count + 1)
    {
        const int old_capacity = chunk->line_capacity;
        chunk->line_capacity   = GROW_CAPACITY(old_capacity);
        chunk->lines           = GROW_ARRAY(LineStart, chunk->lines,
                                            old_capacity, chunk->line_capacity);
    }

    LineStart* line_start = &chunk->lines[chunk->line_count++];
    line_start->offset    = chunk->count - 1;
    line_start->line      = line;
#else
    chunk->lines[chunk->count] = line;
#endif
}

#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
int get_line(const Chunk* chunk, const int offset)
{
    int start = 0;
    int end   = chunk->line_count - 1;

    while (true)
    {
        const int mid = (start + end) / 2;

        const LineStart* current = &chunk->lines[mid];

        if (offset < current->offset)
            end = mid + 1;
        else if (mid == chunk->line_count - 1 || offset < chunk->lines[mid + 1].offset)
            return current->line;
        else
            start = mid + 1;
    }
}
#endif

int add_constant(Chunk* chunk, const Value value)
{
    push(value);
    write_value_array(&chunk->constants, value);
    pop();
    return chunk->constants.count - 1;
}
