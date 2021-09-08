#include "clocks/vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/compiler.h>
#include <clocks/debug.h>
#include <clocks/memory.h>
#include <clocks/object.h>
#include <clocks/table.h>
#include <clocks/value.h>

VM vm;

static void reset_stack()
{
    vm.stack_top   = vm.stack;
    vm.frame_count = 0;
}

static void runtime_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n== stack trace ==\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; i--)
    {
        const CallFrame*   frame = &vm.frames[i];
        const ObjFunction* func  = frame->func;

        const size_t instruction = frame->ip - func->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", func->chunk.lines[instruction]);
        if (func->name == NULL)
            fprintf(stderr, "script\n");
        else
            fprintf(stderr, "%s()\n", func->name->chars);
    }

    reset_stack();
}

void init_vm()
{
    reset_stack();
    init_table(&vm.globals);
    init_table(&vm.strings);
    vm.obj_head = NULL;
}

void free_vm()
{
    free_table(&vm.globals);
    free_table(&vm.strings);
    free_objects();
}

void push(const Value value)
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

static bool is_falsey(const Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
    const ObjString* b = AS_STRING(pop());
    const ObjString* a = AS_STRING(pop());

    const int length = a->length + b->length;

    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* res = take_string(chars, length);
    push(OBJ_VAL(res));
}

static bool call(ObjFunction* func, const int arg_count)
{
    if (arg_count != func->arity)
    {
        runtime_error("Expected %d arguments but got %d.",
                      func->arity, arg_count);
        return false;
    }
    if (vm.frame_count == FRAMES_MAX)
    {
        runtime_error("You know it : Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->func      = func;
    frame->ip        = func->chunk.code;
    frame->slots     = vm.stack_top - arg_count - 1;
    return true;
}

static bool call_value(const Value callee, const int arg_count)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case ObjTypeFunction:
                return call(AS_FUNCTION(callee), arg_count);
            default:
                break;
        }
    }
    runtime_error("Can call only functions and classes");
    return false;
}

static InterpretResult run()
{
    CallFrame* frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE()     (*frame->ip++)
#define READ_CONSTANT() (frame->func->chunk.constants.values[READ_BYTE()])
#define READ_SHORT()    (frame->ip += 2, (uint16_t)(frame->ip[-2] << 8) | frame->ip[-1])
#define READ_STRING()   AS_STRING(READ_CONSTANT())
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

#ifdef DEBUG_TRACE_EXECUTION
    printf("== execution trace ==");
#endif

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
        disassemble_instruction(&frame->func->chunk, (int)(frame->ip - frame->func->chunk.code));
#endif
        const uint8_t instruction = READ_BYTE();
        switch (instruction)
        {
            case OpConstant:
            {
                const Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OpNil:
                push(NIL_VAL);
                break;
            case OpTrue:
                push(BOOL_VAL(true));
                break;
            case OpFalse:
                push(BOOL_VAL(false));
                break;

            case OpPop:
                pop();
                break;

            case OpReadLocal:
            {
                const uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OpAssignLocal:
            {
                const uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }

            case OpReadGlobal:
            {
                const ObjString* name = READ_STRING();

                Value value;
                if (!table_find(&vm.globals, name, &value))
                {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return InterpretRuntimeError;
                }
                push(value);
                break;
            }
            case OpDefineGlobal:
            {
                ObjString* name = READ_STRING();
                table_insert(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OpAssignGlobal:
            {
                ObjString* name = READ_STRING();
                if (table_insert(&vm.globals, name, peek(0)))
                {
                    table_remove(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return InterpretRuntimeError;
                }
                break;
            }

            case OpEqual:
            {
                const Value b = pop();
                const Value a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OpGreater:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OpLess:
                BINARY_OP(BOOL_VAL, <);
                break;

            case OpAdd:
            {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
                    concatenate();
                else if (IS_NUMBER(peek(0)))
                    BINARY_OP(NUMBER_VAL, +);
                else
                {
                    runtime_error("Operands must be two numbers or two strings.");
                    return InterpretRuntimeError;
                }
                break;
            }
            case OpSubtract:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OpMultiply:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OpDivide:
                BINARY_OP(NUMBER_VAL, /);
                break;

            case OpNot:
                push(BOOL_VAL(is_falsey(pop())));
                break;

            case OpNegate:
                if (!IS_NUMBER(peek(0)))
                {
                    runtime_error("Operand must be a number");
                    return InterpretRuntimeError;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;

            case OpPrint:
                print_value(pop());
                printf("\n");
                break;

            case OpJump:
            {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }

            case OpJumpIfFalse:
            {
                uint16_t offset = READ_SHORT();
                if (is_falsey(peek(0)))
                    frame->ip += offset;
                break;
            }
            case OpLoop:
            {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            case OpCall:
            {
                const int arg_count = READ_BYTE();
                if (!call_value(peek(arg_count), arg_count))
                    return InterpretRuntimeError;
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }

            case OpReturn:
                return InterpretOk;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source)
{
    ObjFunction* compiled_top = compile(source);
    if (compiled_top == NULL)
        return InterpretCompileError;

    push(OBJ_VAL(compiled_top));
    call(compiled_top, 0);

    return run();
}
