#include "clocks/memory.h"

#include <stdlib.h>

#include <clocks/chunk.h>
#include <clocks/object.h>
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

void collect_garbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif

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
