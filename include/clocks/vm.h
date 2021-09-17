#ifndef VM_H
#define VM_H

#include "common.h"
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
    Value* stack_top;
    Value  stack[STACK_MAX];

    int       frame_count;
    CallFrame frames[FRAMES_MAX];

    Table globals;
    Table strings;

    size_t bytes_allocated;
    size_t next_gc_thresh;

    ObjString* init_string;

    Obj*        obj_head;
    ObjUpvalue* open_upvalues_head;

    int   gray_count;
    int   gray_capacity;
    Obj** gray_stack;
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

void push(Value value);

#ifdef OPTIMIZED_POP
Value pop_and_return();
void  pop();
#else
Value pop_and_return();
Value pop();
#endif

InterpretResult interpret(const char* source);

#endif  // VM_H
