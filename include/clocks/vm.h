#ifndef VM_H
#define VM_H

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX  (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    ObjClosure* closure;
    uint8_t*    ip;
    Value*      slots;
} CallFrame;

typedef struct
{
    CallFrame frames[FRAMES_MAX];
    int       frame_count;

    Value  stack[STACK_MAX];
    Value* stack_top;

    Table globals;
    Table strings;

    Obj*        obj_head;
    ObjUpvalue* open_upvalues_head;
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
