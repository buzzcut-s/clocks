#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct
{
    Chunk*   chunk;
    uint8_t* ip;

    Value  stack[STACK_MAX];
    Value* stack_top;
} VM;

typedef enum
{
    InterpretOk,
    InterpretCompileError,
    InterpretRuntimeError
} InterpretResult;

void init_vm();
void free_vm();

void  push(Value value);
Value pop();

InterpretResult interpret(Chunk* chunk);

#endif  // VM_H
