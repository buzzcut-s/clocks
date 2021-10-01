#ifndef OBJECT_H
#define OBJECT_H

#include "chunk.h"
#include "table.h"
#include "value.h"

typedef struct ObjUpvalue ObjUpvalue;

typedef enum
{
    ObjTypeString,
    ObjTypeFunction,
    ObjTypeNative,
    ObjTypeClosure,
    ObjTypeUpvalue,
    ObjTypeClass,
    ObjTypeInstance,
    ObjTypeBoundMethod,
} ObjType;

struct Obj
{
    ObjType type;
#ifdef GC_OPTIMIZE_CLEARING_MARK
    bool mark;
#else
    bool  is_marked;
#endif
    struct Obj* next;
};

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

static inline bool is_obj_type(const Value value, const ObjType type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void print_object(const Value* value);

struct ObjString
{
    Obj      obj;
    int      length;
    uint32_t hash;
#ifdef OBJECT_STRING_FLEXIBLE_ARRAY
    char chars[];
#else
    char* chars;
#endif
};

#define IS_STRING(value)  is_obj_type(value, ObjTypeString)
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

#ifdef OBJECT_STRING_FLEXIBLE_ARRAY
ObjString* allocate_string(int length);
#else
ObjString* take_string(char* chars, int length);
#endif

ObjString* copy_string(const char* chars, int length);

uint32_t hash_string(const char* key, int length);

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

typedef struct
{
    Obj        obj;
    ObjString* name;
#ifdef OBJECT_CACHE_CLASS_INITIALIZER
    Value initializer;
#endif
    Table methods;
} ObjClass;

#define IS_CLASS(value) is_obj_type(value, ObjTypeClass)
#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))

ObjClass* new_class(ObjString* name);

typedef struct
{
    Obj       obj;
    ObjClass* klass;
    Table     fields;
} ObjInstance;

#define IS_INSTANCE(value) is_obj_type(value, ObjTypeInstance)
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))

ObjInstance* new_instance(ObjClass* klass);

typedef struct
{
    Obj         obj;
    Value       recv;
    ObjClosure* method;
} ObjBoundMethod;

#define IS_BOUND_METHOD(value) is_obj_type(value, ObjTypeBoundMethod)
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))

ObjBoundMethod* new_bound_method(Value recv, ObjClosure* method);

#endif  // OBJECT_H
