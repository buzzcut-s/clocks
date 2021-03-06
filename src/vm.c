#include "clocks/vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/compiler.h>
#include <clocks/debug.h>
#include <clocks/memory.h>
#include <clocks/object.h>
#include <clocks/table.h>
#include <clocks/value.h>

VM vm;

static Value clock_native(__attribute__((unused)) int          arg_count,
                          __attribute__((unused)) const Value* args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value has_field_native(int arg_count, const Value* args)
{
    if (arg_count != 2 || !IS_INSTANCE(args[0]) || !IS_STRING(args[1]))
        return NIL_VAL;

    ObjInstance* instance = AS_INSTANCE(args[0]);

    Value dummy;
    return BOOL_VAL(table_find(&instance->fields, AS_STRING(args[1]), &dummy));
}

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
        const ObjFunction* func  = frame->closure->func;

        const size_t instruction_offset = frame->ip - func->chunk.code - 1;
        fprintf(stderr, "[line %d] in ",
#ifdef CHUNK_LINE_RUN_LENGTH_ENCODING
                get_line(&func->chunk, instruction_offset));
#else
                func->chunk.lines[instruction_offset]);
#endif
        if (func->name == NULL)
            fprintf(stderr, "script\n");
        else
            fprintf(stderr, "%s()\n", func->name->chars);
    }

    reset_stack();
}

static void define_native(const char* name, const NativeFn func)
{
    push(OBJ_VAL(copy_string(name, (int)strlen(name))));
    push(OBJ_VAL(new_native(func)));
    table_insert(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void init_vm()
{
    reset_stack();
    init_table(&vm.globals);
    init_table(&vm.strings);
    vm.init_string        = NULL;
    vm.obj_head           = NULL;
    vm.open_upvalues_head = NULL;
    vm.gray_count         = 0;
    vm.gray_capacity      = 0;
    vm.gray_stack         = NULL;
    vm.bytes_allocated    = 0;
    vm.next_gc_thresh     = 1024 * 1024;

#ifdef GC_OPTIMIZE_CLEARING_MARK
    vm.mark_value = true;
#endif

    vm.init_string = copy_string("init", 4);

    define_native("clock", clock_native);
    define_native("has_field", has_field_native);
}

void free_vm()
{
    free_table(&vm.globals);
    free_table(&vm.strings);
    vm.init_string = NULL;
    free_objects();
}

void push(Value value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

#ifdef VM_OPTIMIZED_POP
Value pop_and_return()
{
    vm.stack_top--;
    return *vm.stack_top;
}

void pop()
{
    vm.stack_top--;
}
#else
Value pop_and_return()
{
    return pop();
}

Value pop()
{
    vm.stack_top--;
    return *vm.stack_top;
}
#endif

static Value peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool is_falsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
    const ObjString* b = AS_STRING(peek(0));
    const ObjString* a = AS_STRING(peek(1));

    const int length = a->length + b->length;

#ifdef OBJECT_STRING_FLEXIBLE_ARRAY
    ObjString* res = allocate_string(length);
    memcpy(res->chars, a->chars, a->length);
    memcpy(res->chars + a->length, b->chars, b->length);
    res->hash          = hash_string(res->chars, length);
    res->chars[length] = '\0';
#else
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* res = take_string(chars, length);
#endif
    pop();
    pop();
    push(OBJ_VAL(res));
}

static bool call(const ObjClosure* closure, int arg_count)
{
    if (arg_count != closure->func->arity)
    {
        runtime_error("Expected %d arguments but got %d.",
                      closure->func->arity, arg_count);
        return false;
    }
    if (vm.frame_count == FRAMES_MAX)
    {
        runtime_error("You know it : Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->closure   = closure;
    frame->ip        = closure->func->chunk.code;
    frame->slots     = vm.stack_top - arg_count - 1;
    return true;
}

static bool call_value(Value callee, int arg_count)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case ObjTypeClosure:
                return call(AS_CLOSURE(callee), arg_count);

            case ObjTypeNative:
            {
                const NativeFn native = AS_NATIVE(callee);
                const Value    result = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                push(result);
                return true;
            }

            case ObjTypeClass:
            {
                ObjClass* klass              = AS_CLASS(callee);
                vm.stack_top[-arg_count - 1] = OBJ_VAL(new_instance(klass));

#ifdef OBJECT_CACHE_CLASS_INITIALIZER
                if (!IS_NIL(klass->initializer))
                    return call(AS_CLOSURE(klass->initializer), arg_count);
#else
                Value initializer;
                if (table_find(&klass->methods, vm.init_string, &initializer))
                    return call(AS_CLOSURE(initializer), arg_count);
#endif
                if (arg_count != 0)
                {
                    runtime_error("Expected 0 arguments but got %d", arg_count);
                    return false;
                }

                return true;
            }

            case ObjTypeBoundMethod:
            {
                const ObjBoundMethod* bound  = AS_BOUND_METHOD(callee);
                vm.stack_top[-arg_count - 1] = bound->recv;
                return call(bound->method, arg_count);
            }

            default:
                break;
        }
    }

    runtime_error("Can call only functions and classes");
    return false;
}

static bool invoke_from_class(const ObjClass*  klass,
                              const ObjString* name, int arg_count)
{
    Value method;
    if (!table_find(&klass->methods, name, &method))
    {
        runtime_error("Undefined property '%s'.", name->chars);
        return false;
    }

    return call(AS_CLOSURE(method), arg_count);
}

static bool invoke(const ObjString* method_name, int arg_count)
{
    const Value recv = peek(arg_count);
    if (!IS_INSTANCE(recv))
    {
        runtime_error("Only instances have methods.");
        return false;
    }

    const ObjInstance* instance = AS_INSTANCE(recv);

    Value value;
    if (table_find(&instance->fields, method_name, &value))
    {
        vm.stack_top[-arg_count - 1] = value;
        return call_value(value, arg_count);
    }

    return invoke_from_class(instance->klass, method_name, arg_count);
}

static ObjUpvalue* capture_upvalue(Value* local)
{
    ObjUpvalue* prev_upvalue = NULL;
    ObjUpvalue* upvalue      = vm.open_upvalues_head;

    while (upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue      = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)
        return upvalue;

    ObjUpvalue* captured_upvalue = new_upvalue(local);
    captured_upvalue->next       = upvalue;

    if (prev_upvalue == NULL)
        vm.open_upvalues_head = captured_upvalue;
    else
        prev_upvalue->next = captured_upvalue;

    return captured_upvalue;
}

static void close_upvalues(const Value* last)
{
    while (vm.open_upvalues_head != NULL
           && vm.open_upvalues_head->location >= last)
    {
        ObjUpvalue* hoisted_upvalue = vm.open_upvalues_head;
        hoisted_upvalue->closed     = *hoisted_upvalue->location;
        hoisted_upvalue->location   = &hoisted_upvalue->closed;
        vm.open_upvalues_head       = hoisted_upvalue->next;
    }
}

static void define_method(ObjString* name)
{
    const Value method = peek(0);
    ObjClass*   klass  = AS_CLASS(peek(1));
#ifdef OBJECT_CACHE_CLASS_INITIALIZER
    if (name == vm.init_string)
        klass->initializer = method;
#endif
    table_insert(&klass->methods, name, method);
    pop();
}

static bool bind_method(const ObjClass* klass, const ObjString* name)
{
    Value method;
    if (!table_find(&klass->methods, name, &method))
    {
        runtime_error("Undefined property '%s'.", name->chars);
        return false;
    }

    const ObjBoundMethod* bound = new_bound_method(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

static InterpretResult run()
{
    CallFrame* frame = &vm.frames[vm.frame_count - 1];
#ifdef VM_CACHE_IP
    register uint8_t* ip = frame->ip;
#endif

#ifdef VM_CACHE_IP
#define READ_BYTE()  (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)(ip[-2] << 8) | ip[-1])
#else
#define READ_BYTE()  (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)(frame->ip[-2] << 8) | frame->ip[-1])
#endif
#define READ_CONSTANT() (frame->closure->func->chunk.constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())

#ifdef VM_CACHE_IP
#define BINARY_OP(value_type, op)                       \
    do {                                                \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            frame->ip = ip;                             \
            runtime_error("Operands must be numbers."); \
            return InterpretRuntimeError;               \
        }                                               \
        const double b = AS_NUMBER(pop_and_return());   \
        const double a = AS_NUMBER(pop_and_return());   \
        push(value_type(a op b));                       \
    }                                                   \
    while (false)
#else
#define BINARY_OP(value_type, op)                       \
    do {                                                \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            runtime_error("Operands must be numbers."); \
            return InterpretRuntimeError;               \
        }                                               \
        const double b = AS_NUMBER(pop_and_return());   \
        const double a = AS_NUMBER(pop_and_return());   \
        push(value_type(a op b));                       \
    }                                                   \
    while (false)
#endif

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
        disassemble_instruction(&frame->closure->func->chunk,
                                (int)(frame->ip - frame->closure->func->chunk.code));
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
#ifdef VM_CACHE_IP
                    frame->ip = ip;
#endif
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
#ifdef VM_CACHE_IP
                    frame->ip = ip;
#endif
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return InterpretRuntimeError;
                }
                break;
            }
            case OpReadUpvalue:
            {
                const uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OpAssignUpvalue:
            {
                const uint8_t slot                        = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }

            case OpSetField:
            {
                if (!IS_INSTANCE(peek(1)))
                {
#ifdef VM_CACHE_IP
                    frame->ip = ip;
#endif
                    runtime_error("Only instances have properties.");
                    return InterpretRuntimeError;
                }

                ObjInstance* instance = AS_INSTANCE(peek(1));
                table_insert(&instance->fields, READ_STRING(), peek(0));

                const Value value = pop_and_return();
                pop();
                push(value);
                break;
            }

            case OpGetProperty:
            {
                if (!IS_INSTANCE(peek(0)))
                {
#ifdef VM_CACHE_IP
                    frame->ip = ip;
#endif
                    runtime_error("Only instances have properties.");
                    return InterpretRuntimeError;
                }

                const ObjInstance* instance = AS_INSTANCE(peek(0));
                const ObjString*   name     = READ_STRING();

                Value value;
                if (table_find(&instance->fields, name, &value))
                {
                    pop();
                    push(value);
                    break;
                }
#ifdef VM_CACHE_IP
                frame->ip = ip;
#endif
                if (!bind_method(instance->klass, name))
                    return InterpretRuntimeError;

                break;
            }

            case OpGetSuper:
            {
                const ObjString* name       = READ_STRING();
                const ObjClass*  superclass = AS_CLASS(pop_and_return());
#ifdef VM_CACHE_IP
                frame->ip = ip;
#endif
                if (!bind_method(superclass, name))
                    return InterpretRuntimeError;

                break;
            }

            case OpEqual:
            {
                const Value b = pop_and_return();
                const Value a = pop_and_return();
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
                else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
                {
                    const double b = AS_NUMBER(pop_and_return());
                    const double a = AS_NUMBER(pop_and_return());
                    push(NUMBER_VAL(a + b));
                }
                else
                {
#ifdef VM_CACHE_IP
                    frame->ip = ip;
#endif
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
                push(BOOL_VAL(is_falsey(pop_and_return())));
                break;

            case OpNegate:
                if (!IS_NUMBER(peek(0)))
                {
#ifdef VM_CACHE_IP
                    frame->ip = ip;
#endif
                    runtime_error("Operand must be a number");
                    return InterpretRuntimeError;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop_and_return())));
                break;

            case OpPrint:
                print_value(pop_and_return());
                printf("\n");
                break;

            case OpJump:
            {
                const uint16_t offset = READ_SHORT();
#ifdef VM_CACHE_IP
                ip += offset;
#else
                frame->ip += offset;
#endif
                break;
            }

            case OpJumpIfFalse:
            {
                const uint16_t offset = READ_SHORT();
                if (is_falsey(peek(0)))
#ifdef VM_CACHE_IP
                    ip += offset;
#else
                    frame->ip += offset;
#endif
                break;
            }
            case OpLoop:
            {
                const uint16_t offset = READ_SHORT();
#ifdef VM_CACHE_IP
                ip -= offset;
#else
                frame->ip -= offset;
#endif
                break;
            }

            case OpCall:
            {
                const int arg_count = READ_BYTE();
#ifdef VM_CACHE_IP
                frame->ip = ip;
#endif
                if (!call_value(peek(arg_count), arg_count))
                    return InterpretRuntimeError;
                frame = &vm.frames[vm.frame_count - 1];
#ifdef VM_CACHE_IP
                ip = frame->ip;
#endif
                break;
            }
            case OpInvoke:
            {
                const ObjString* method    = READ_STRING();
                const int        arg_count = READ_BYTE();
#ifdef VM_CACHE_IP
                frame->ip = ip;
#endif
                if (!invoke(method, arg_count))
                    return InterpretRuntimeError;

                frame = &vm.frames[vm.frame_count - 1];
#ifdef VM_CACHE_IP
                ip = frame->ip;
#endif
                break;
            }
            case OpSuperInvoke:
            {
                const ObjString* method     = READ_STRING();
                const int        arg_count  = READ_BYTE();
                const ObjClass*  superclass = AS_CLASS(pop_and_return());
#ifdef VM_CACHE_IP
                frame->ip = ip;
#endif
                if (!invoke_from_class(superclass, method, arg_count))
                    return InterpretRuntimeError;

                frame = &vm.frames[vm.frame_count - 1];
#ifdef VM_CACHE_IP
                ip = frame->ip;
#endif
                break;
            }

            case OpClosure:
            {
                ObjFunction*      func    = AS_FUNCTION(READ_CONSTANT());
                const ObjClosure* closure = new_closure(func);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalue_count; i++)
                {
                    const uint8_t is_local = READ_BYTE();
                    const uint8_t index    = READ_BYTE();
                    closure->upvalues[i]   = (is_local) ? capture_upvalue(frame->slots + index)
                                                        : frame->closure->upvalues[index];
                }
                break;
            }

            case OpCloseUpvalue:
                close_upvalues(vm.stack_top - 1);
                pop();
                break;

            case OpReturn:
            {
                const Value result = pop_and_return();

                close_upvalues(frame->slots);
                vm.frame_count--;
                if (vm.frame_count == 0)
                {
                    pop();
                    return InterpretOk;
                }

                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
#ifdef VM_CACHE_IP
                ip = frame->ip;
#endif
                break;
            }

            case OpInherit:
            {
                const Value superclass = peek(1);
                if (!IS_CLASS(superclass))
                {
#ifdef VM_CACHE_IP
                    frame->ip = ip;
#endif
                    runtime_error("Superclass must be a class.");
                    return InterpretRuntimeError;
                }

                ObjClass* subclass = AS_CLASS(peek(0));
                table_copy(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop();
                break;
            }
            case OpClass:
                push(OBJ_VAL(new_class(READ_STRING())));
                break;
            case OpMethod:
                define_method(READ_STRING());
                break;

            default:
                printf("Unknown opcode %d\n", instruction);
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
    ObjFunction* compiled_source = compile(source);
    if (compiled_source == NULL)
        return InterpretCompileError;

    push(OBJ_VAL(compiled_source));
    ObjClosure* top_level_closure = new_closure(compiled_source);
    pop();
    push(OBJ_VAL(top_level_closure));

    call(top_level_closure, 0);

    return run();
}
