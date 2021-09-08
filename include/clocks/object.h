#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"
#include "value.h"

typedef enum
{
    ObjTypeString,
    ObjTypeFunction,
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

ObjFunction* new_function();

#define IS_FUNCTION(value) is_obj_type(value, ObjTypeFunction)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))

#endif  // OBJECT_H
