#ifndef OBJECT_H
#define OBJECT_H

#include "value.h"

typedef enum
{
    ObjTypeString,
} ObjType;

struct Obj
{
    ObjType type;
};

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

void print_object(const Value* value);

typedef struct ObjString
{
    Obj   obj;
    int   length;
    char* chars;
} ObjString;

#define IS_STRING(value)  is_obj_type(&(value), ObjTypeString)
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

ObjString* copy_string(const char* chars, int length);

static inline bool is_obj_type(const Value* value, const ObjType type)
{
    return IS_OBJ(*value) && AS_OBJ(*value)->type == type;
}

#endif  // OBJECT_H
