#include "clocks/object.h"

#include <stdio.h>
#include <string.h>

#include <clocks/chunk.h>
#include <clocks/common.h>
#include <clocks/memory.h>
#include <clocks/table.h>
#include <clocks/value.h>
#include <clocks/vm.h>

#define ALLOCATE_OBJ(type, obj_type) \
    (type*)allocate_obj(sizeof(type), obj_type)

static Obj* allocate_obj(const size_t size, const ObjType type)
{
    Obj* object  = (Obj*)reallocate(NULL, 0, size);
    object->type = type;

#ifdef GC_OPTIMIZE_CLEARING_MARK
    object->mark = !vm.mark_value;
#else
    object->is_marked = false;
#endif

    object->next = vm.obj_head;
    vm.obj_head  = object;

#ifdef DEBUG_LOG_GC
    static const char* types[] = {"ObjString", "ObjFunction", "ObjNative", "ObjClosure",
                                  "ObjUpvalue", "ObjClass", "ObjTypeInstance", "ObjTypeBoundMethod"};
    printf("%p allocate %zu bytes for %s\n", (void*)object, size, types[type]);
#endif

    return object;
}

#ifndef OBJECT_STRING_FLEXIBLE_ARRAY
static ObjString* allocate_string(char* chars, const int length, const uint32_t hash)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, ObjTypeString);
    string->length    = length;
    string->chars     = chars;
    string->hash      = hash;
    push(OBJ_VAL(string));
    table_insert(&vm.strings, string, NIL_VAL);
    pop();
    return string;
}
#endif

uint32_t hash_string(const char* key, const int length)
{
#define FNV_OFFSET_BASIS 2166136261U
#ifndef TABLE_FNV_GCC_OPTIMIZATION
#define FNV_PRIME 16777619U
#endif
    uint32_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
#ifdef TABLE_FNV_GCC_OPTIMIZATION  // http://www.isthe.com/chongo/tech/comp/fnv/#gcc-O3
        hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
#else
        hash *= FNV_PRIME;
#endif
    }
    return hash;
}

#ifdef OBJECT_STRING_FLEXIBLE_ARRAY
ObjString* allocate_string(const int length)
{
    ObjString* string = (ObjString*)allocate_obj(sizeof(ObjString) + length + 1, ObjTypeString);
    string->length    = length;
    return string;
}

static void intern_string(ObjString* string)
{
    push(OBJ_VAL(string));
    table_insert(&vm.strings, string, NIL_VAL);
    pop();
}

static void init_string(ObjString* string, const char* chars,
                        const int length, const uint32_t hash)
{
    memcpy(string->chars, chars, length);
    string->hash          = hash;
    string->chars[length] = '\0';
    intern_string(string);
}
#endif

ObjString* take_string(char* chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

#ifdef OBJECT_STRING_FLEXIBLE_ARRAY
    ObjString* string = allocate_string(length);
    init_string(string, chars, length, hash);

    return string;
#else
    return allocate_string(chars, length, hash);
#endif
}

ObjString* copy_string(const char* chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

#ifdef OBJECT_STRING_FLEXIBLE_ARRAY
    ObjString* string = allocate_string(length);
    init_string(string, chars, length, hash);

    return string;
#else
    char* heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return allocate_string(heap_chars, length, hash);
#endif
}

ObjFunction* new_function()
{
    ObjFunction* func   = ALLOCATE_OBJ(ObjFunction, ObjTypeFunction);
    func->arity         = 0;
    func->upvalue_count = 0;
    func->name          = NULL;
    init_chunk(&func->chunk);
    return func;
}

ObjNative* new_native(NativeFn func)
{
    ObjNative* native = ALLOCATE_OBJ(ObjNative, ObjTypeNative);
    native->func      = func;
    return native;
}

ObjClosure* new_closure(ObjFunction* func)
{
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, func->upvalue_count);
    for (int i = 0; i < func->upvalue_count; i++)
        upvalues[i] = NULL;

    ObjClosure* closure    = ALLOCATE_OBJ(ObjClosure, ObjTypeClosure);
    closure->func          = func;
    closure->upvalues      = upvalues;
    closure->upvalue_count = func->upvalue_count;
    return closure;
}

ObjUpvalue* new_upvalue(Value* slot)
{
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, ObjTypeUpvalue);
    upvalue->loc        = slot;
    upvalue->closed     = NIL_VAL;
    upvalue->next       = NULL;
    return upvalue;
}

ObjClass* new_class(ObjString* name)
{
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, ObjTypeClass);
    klass->name     = name;
#ifdef OBJECT_CACHE_CLASS_INITIALIZER
    klass->initializer = NIL_VAL;
#endif
    init_table(&klass->methods);
    return klass;
}

ObjInstance* new_instance(ObjClass* klass)
{
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, ObjTypeInstance);
    instance->klass       = klass;
    init_table(&instance->fields);
    return instance;
}

ObjBoundMethod* new_bound_method(Value recv, ObjClosure* method)
{
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, ObjTypeBoundMethod);
    bound->recv           = recv;
    bound->method         = method;
    return bound;
}

static void print_function(const ObjFunction* func)
{
    if (func->name == NULL)
    {
        printf("<script>");
        return;
    }
    printf("<fn %s>", func->name->chars);
}

void print_object(const Value* value)
{
    switch (OBJ_TYPE(*value))
    {
        case ObjTypeString:
            printf("%s", AS_CSTRING(*value));
            break;
        case ObjTypeFunction:
            print_function(AS_FUNCTION(*value));
            break;
        case ObjTypeNative:
            printf("<native fn>");
            break;
        case ObjTypeClosure:
            print_function(AS_CLOSURE(*value)->func);
            break;
        case ObjTypeUpvalue:
            printf("upvalue");
            break;
        case ObjTypeClass:
            printf("%s", AS_CLASS(*value)->name->chars);
            break;
        case ObjTypeInstance:
            printf("%s instance", AS_INSTANCE(*value)->klass->name->chars);
            break;
        case ObjTypeBoundMethod:
            print_function(AS_BOUND_METHOD(*value)->method->func);
            break;
    }
}
