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
    OpReadLocal,
    OpAssignLocal,
    OpReadGlobal,
    OpDefineGlobal,
    OpAssignGlobal,
    OpReadUpvalue,
    OpAssignUpvalue,
    OpSetField,
    OpGetProperty,
    OpGetSuper,
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
    OpJump,
    OpJumpIfFalse,
    OpLoop,
    OpCall,
    OpInvoke,
    OpSuperInvoke,
    OpClosure,
    OpCloseUpvalue,
    OpReturn,
    OpClass,
    OpInherit,
    OpMethod,
} OpCode;

#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
typedef struct
{
    int offset;
    int line;
} LineStart;
#endif

typedef struct
{
    int        count;
    int        capacity;
    uint8_t*   code;
    ValueArray constants;
#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
    int        line_count;
    int        line_capacity;
    LineStart* lines;
#else
    int* lines;
#endif
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, int line);

#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
int get_line(const Chunk* chunk, int offset);
#endif

int add_constant(Chunk* chunk, Value value);

#endif  // CHUNK_H
