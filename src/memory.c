#include "clocks/memory.h"

#include <stdlib.h>

#include <clocks/object.h>
#include <clocks/vm.h>

void* reallocate(void* pointer, const size_t old_size, const size_t new_size)
{
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

static void free_object(Obj* object)
{
    switch (object->type)
    {
        case ObjTypeString:
        {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
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
