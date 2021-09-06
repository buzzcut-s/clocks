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

    Obj* obj_head;
} VM;

typedef enum
{
    InterpretOk,
    InterpretCompileError,
    InterpretRuntimeError
} InterpretResult;

extern VM vm;

void init_vm();
void free_vm();

void  push(Value value);
Value pop();

InterpretResult interpret(const char* source);

#endif  // VM_H
