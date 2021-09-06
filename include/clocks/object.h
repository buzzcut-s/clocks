#ifndef OBJECT_H
#define OBJECT_H

typedef enum
{
    ObjTypeString,
} ObjType;

struct Obj
{
    ObjType type;
};

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#endif  // OBJECT_H
