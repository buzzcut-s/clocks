#include "clocks/chunk.h"

#include <stdlib.h>

#include <clocks/memory.h>
#include <clocks/value.h>

void init_chunk(Chunk* chunk)
{
    chunk->count    = 0;
    chunk->capacity = 0;
    chunk->code     = NULL;
    chunk->lines    = NULL;
    init_value_array(&chunk->constants);
}

void free_chunk(Chunk* chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

void write_chunk(Chunk* chunk, const uint8_t byte, const int lines)
{
    if (chunk->capacity < chunk->count + 1)
    {
        const int old_capacity = chunk->capacity;

        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code     = GROW_ARRAY(uint8_t, chunk->code,
                                     old_capacity, chunk->capacity);
        chunk->lines    = GROW_ARRAY(int, chunk->lines,
                                     old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count]  = byte;
    chunk->lines[chunk->count] = lines;
    chunk->count++;
}

int add_constant(Chunk* chunk, const Value value)
{
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}
