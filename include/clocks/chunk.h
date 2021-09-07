#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"

typedef enum
{
    OpConstant,
    OpNil,
    OpTrue,
    OpFalse,
    OpPop,
    OpReadGlobal,
    OpDefineGlobal,
    OpEqual,
    OpGreater,
    OpLess,
    OpAdd,
    OpSubtract,
    OpMultiply,
    OpDivide,
    OpNot,
    OpNegate,
    OpPrint,
    OpReturn,
} OpCode;

typedef struct
{
    int        count;
    int        capacity;
    uint8_t*   code;
    int*       lines;
    ValueArray constants;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, int lines);

int add_constant(Chunk* chunk, Value value);

#endif  // CHUNK_H
