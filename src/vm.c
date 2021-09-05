#include "clocks/vm.h"

#include <stdarg.h>
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

static void runtime_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    const size_t instruction = (vm.ip - vm.chunk->code) - 1;
    const int    line        = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    reset_stack();
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

static Value peek(const int distance)
{
    return vm.stack_top[-1 - distance];
}

static InterpretResult run()
{
#define READ_BYTE()     (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op)                       \
    do {                                                \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            runtime_error("Operands must be numbers");  \
            return InterpretRuntimeError;               \
        }                                               \
        const double b = AS_NUMBER(pop());              \
        const double a = AS_NUMBER(pop());              \
        push(value_type(a op b));                       \
    }                                                   \
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
                if (!IS_NUMBER(peek(0)))
                {
                    runtime_error("Operand must be a number");
                    return InterpretRuntimeError;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;

            case OpAdd:
                BINARY_OP(NUMBER_VAL, +);
                break;
            case OpSubtract:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OpMultiply:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OpDivide:
                BINARY_OP(NUMBER_VAL, /);
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
