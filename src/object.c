#include "clocks/object.h"

#include <string.h>

#include <clocks/memory.h>

#define ALLOCATE_OBJ(type, obj_type) \
    (type*)allocate_obj(sizeof(type), obj_type)

static Obj* allocate_obj(const size_t size, const ObjType type)
{
    Obj* object  = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

static ObjString* allocate_string(char* chars, const int length)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, ObjTypeString);
    string->length    = length;
    string->chars     = chars;
    return string;
}

ObjString* copy_string(const char* chars, int length)
{
    char* heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(heap_chars, length);
}
