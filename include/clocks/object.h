#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"
#include "value.h"

typedef struct ObjUpvalue ObjUpvalue;

typedef enum
{
    ObjTypeString,
    ObjTypeFunction,
    ObjTypeNative,
    ObjTypeClosure,
    ObjTypeUpvalue,
} ObjType;

struct Obj
{
    ObjType     type;
    bool        is_marked;
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
    int        upvalue_count;
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

#define IS_NATIVE(value) is_obj_type(value, ObjTypeNative)
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->func)

ObjNative* new_native(NativeFn func);

typedef struct
{
    Obj          obj;
    ObjFunction* func;
    ObjUpvalue** upvalues;
    int          upvalue_count;
} ObjClosure;

#define IS_CLOSURE(value) is_obj_type(value, ObjTypeClosure)
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))

ObjClosure* new_closure(ObjFunction* func);

struct ObjUpvalue
{
    Obj         obj;
    Value*      loc;
    Value       closed;
    ObjUpvalue* next;
};

ObjUpvalue* new_upvalue(Value* slot);

#endif  // OBJECT_H
