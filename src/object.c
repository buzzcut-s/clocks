#include "clocks/object.h"

#include <stdio.h>
#include <string.h>

#include <clocks/memory.h>
#include <clocks/table.h>
#include <clocks/vm.h>

#define ALLOCATE_OBJ(type, obj_type) \
    (type*)allocate_obj(sizeof(type), obj_type)

static Obj* allocate_obj(const size_t size, const ObjType type)
{
    Obj* object  = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.obj_head;
    vm.obj_head  = object;

    return object;
}

static ObjString* allocate_string(char* chars, const int length, uint32_t hash)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, ObjTypeString);
    string->length    = length;
    string->chars     = chars;
    string->hash      = hash;
    table_insert(&vm.strings, string, NIL_VAL);
    return string;
}

static uint32_t hash_string(const char* key, int length)
{
    const uint32_t fnv_offset_basis = 2166136261U;
    const uint32_t fnv_prime        = 16777619U;

    uint32_t hash = fnv_offset_basis;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= fnv_prime;
    }
    return hash;
}

ObjString* take_string(char* chars, const int length)
{
    uint32_t hash = hash_string(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash);
}

ObjString* copy_string(const char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char* heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return allocate_string(heap_chars, length, hash);
}

void print_object(const Value* value)
{
    switch (OBJ_TYPE(*value))
    {
        case ObjTypeString:
            printf("%s", AS_CSTRING(*value));
            break;
    }
}
