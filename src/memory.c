#include "clocks/memory.h"

#include <stdlib.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/compiler.h>
#include <clocks/object.h>
#include <clocks/value.h>
#include <clocks/vm.h>

#ifdef DEBUG_LOG_GC
#include <stdio.h>

#include <clocks/debug.h>
#endif

static void blacken_object(Obj* gray_obj);
static void free_object(Obj* object);

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
    if (object == NULL || object->is_marked)
        return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    print_value(OBJ_VAL(object));
    printf("\n");
#endif

    object->is_marked = true;

    if (object->type == ObjTypeString || object->type == ObjTypeNative)
    {
        blacken_object(object);
        return;
    }

    if (vm.gray_capacity < vm.gray_count + 1)
    {
        vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
        vm.gray_stack    = (Obj**)realloc(vm.gray_stack,
                                          sizeof(Obj*) * vm.gray_capacity);
        if (vm.gray_stack == NULL)
            exit(1);
    }

    vm.gray_stack[vm.gray_count++] = object;
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

static void mark_array(ValueArray* array)
{
    for (int i = 0; i < array->count; i++)
        mark_value(array->values[i]);
}

static void mark_upvalues(ObjClosure* closure)
{
    for (int i = 0; i < closure->upvalue_count; i++)
        mark_object((Obj*)closure->upvalues[i]);
}

static void blacken_object(Obj* gray_obj)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)gray_obj);
    print_value(OBJ_VAL(gray_obj));
    printf("\n");
#endif

    switch (gray_obj->type)
    {
        case ObjTypeFunction:
        {
            ObjFunction* func = (ObjFunction*)gray_obj;
            mark_object((Obj*)func->name);
            mark_array(&func->chunk.constants);
            break;
        }

        case ObjTypeClosure:
        {
            ObjClosure* closure = (ObjClosure*)gray_obj;
            mark_object((Obj*)closure->func);
            mark_upvalues(closure);
        }

        case ObjTypeUpvalue:
            mark_value(((ObjUpvalue*)gray_obj)->closed);
            break;

        case ObjTypeString:
        case ObjTypeNative:
            break;
    }
}

static void trace_references()
{
    while (vm.gray_count > 0)
    {
        Obj* gray_object = vm.gray_stack[--vm.gray_count];
        blacken_object(gray_object);
    }
}

static void sweep()
{
    Obj* prev = NULL;
    Obj* curr = vm.obj_head;
    while (curr != NULL)
    {
        if (curr->is_marked)
        {
            curr->is_marked = false;
            prev            = curr;
            curr            = curr->next;
        }
        else
        {
            Obj* unreached = curr;

            curr = curr->next;
            if (prev != NULL)
                prev->next = curr;
            else
                vm.obj_head = curr;

            free_object(unreached);
        }
    }
}

void collect_garbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif

    mark_roots();
    trace_references();
    sweep();

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

    free(vm.gray_stack);
}
