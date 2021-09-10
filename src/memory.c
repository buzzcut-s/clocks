#include "clocks/memory.h"

#include <stdlib.h>

#include <clocks/chunk.h>
#include <clocks/compiler.h>
#include <clocks/object.h>
#include <clocks/value.h>
#include <clocks/vm.h>

#ifdef DEBUG_LOG_GC
#include <stdio.h>

#include <clocks/debug.h>
#endif

void* reallocate(void* pointer, const size_t old_size, const size_t new_size)
{
    if (new_size > old_size)
    {
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#endif
    }

    if (new_size == 0)
    {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    if (result == NULL)
        exit(1);

    return result;
}

void mark_object(Obj* object)
{
    if (object == NULL)
        return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    print_value(OBJ_VAL(object));
    printf("\n");
#endif

    object->is_marked = true;
}

void mark_value(Value value)
{
    if (IS_OBJ(value))
        mark_object(AS_OBJ(value));
}

static void mark_roots()
{
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++)
        mark_value(*slot);

    for (int i = 0; i < vm.frame_count; i++)
        mark_object((Obj*)vm.frames[i].closure);

    for (ObjUpvalue* upvalue = vm.open_upvalues_head; upvalue != NULL;
         upvalue             = upvalue->next)
        mark_object((Obj*)upvalue);

    mark_table(&vm.globals);
    mark_compiler_roots();
}

void collect_garbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif

    mark_roots();

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif
}

static void free_object(Obj* object)
{
#ifdef DEBUG_LOG_GC
    const char* types[] = {"ObjString", "ObjFunction", "ObjNative", "ObjClosure", "ObjUpvalue"};
    printf("%p free type %s\n", (void*)object, types[object->type]);
#endif

    switch (object->type)
    {
        case ObjTypeString:
        {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case ObjTypeFunction:
        {
            ObjFunction* func = (ObjFunction*)object;
            free_chunk(&func->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case ObjTypeNative:
            FREE(ObjNative, object);
            break;
        case ObjTypeClosure:
        {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE(ObjClosure, object);
            break;
        }
        case ObjTypeUpvalue:
            FREE(ObjUpvalue, object);
            break;
    }
}

void free_objects()
{
    Obj* curr = vm.obj_head;
    while (curr != NULL)
    {
        Obj* next = curr->next;
        free_object(curr);
        curr = next;
    }
}
