#include "clocks/vm.h"

#include <stdio.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/compiler.h>
#include <clocks/debug.h>
#include <clocks/value.h>

VM vm;

static void reset_stack()
{
    vm.stack_top = vm.stack;
}

void init_vm()
{
    reset_stack();
}

void free_vm()
{
}

void push(Value value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

Value pop()
{
    vm.stack_top--;
    return *vm.stack_top;
}

static InterpretResult run()
{
#define READ_BYTE()     (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op)           \
    do {                        \
        const double b = pop(); \
        const double a = pop(); \
        push(a op b);           \
    }                           \
    while (false)

    while (true)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stack_top; slot++)
        {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        const uint8_t instruction = READ_BYTE();
        switch (instruction)
        {
            case OpConstant:
            {
                const Value constant = READ_CONSTANT();
                push(constant);
                print_value(constant);
                printf("\n");
                break;
            }

            case OpNegate:
                push(-pop());
                break;

            case OpAdd:
                BINARY_OP(+);
                break;
            case OpSubtract:
                BINARY_OP(-);
                break;
            case OpMultiply:
                BINARY_OP(*);
                break;
            case OpDivide:
                BINARY_OP(/);
                break;

            case OpReturn:
                print_value(pop());
                printf("\n");
                return InterpretOk;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    Chunk chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk))
    {
        free_chunk(&chunk);
        return InterpretCompileError;
    }

    vm.chunk = &chunk;
    vm.ip    = vm.chunk->code;

    const InterpretResult result = run();

    free_chunk(&chunk);
    return result;
}
