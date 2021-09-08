#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"
#include "value.h"

typedef enum
{
    ObjTypeString,
    ObjTypeFunction,
    ObjTypeNative,
} ObjType;

struct Obj
{
    ObjType     type;
    struct Obj* next;
};

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

static inline bool is_obj_type(const Value value, const ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void print_object(const Value* value);

typedef struct ObjString
{
    Obj      obj;
    int      length;
    char*    chars;
    uint32_t hash;
} ObjString;

#define IS_STRING(value)  is_obj_type(value, ObjTypeString)
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

ObjString* take_string(char* chars, int length);
ObjString* copy_string(const char* chars, int length);

typedef struct
{
    Obj        obj;
    int        arity;
    Chunk      chunk;
    ObjString* name;
} ObjFunction;

#define IS_FUNCTION(value) is_obj_type(value, ObjTypeFunction)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))

ObjFunction* new_function();

typedef Value (*NativeFn)(const int arg_count, const Value* args);

typedef struct
{
    Obj      obj;
    NativeFn func;
} ObjNative;

ObjNative* new_native(NativeFn func);

#define IS_NATIVE(value) is_obj_type(value, ObjTypeNative)
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->func)

#endif  // OBJECT_H
