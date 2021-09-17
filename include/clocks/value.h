#ifndef VALUE_H
#define VALUE_H

#include "common.h"

typedef struct Obj Obj;

#ifdef NAN_BOXING

#include <string.h>

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NIL   1
#define TAG_FALSE 2
#define TAG_TRUE  3

typedef uint64_t Value;

#define IS_BOOL(value)   (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)    ((value) == NIL_VAL)
#define IS_NUMBER(value) (((value)&QNAN) != QNAN)
#define IS_OBJ(value)    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)   ((value) == TRUE_VAL)
#define AS_NUMBER(value) value_to_num(value)
#define AS_OBJ(value)    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

static inline double value_to_num(Value value)
{
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

#define NIL_VAL         ((Value)(uint8_t)(QNAN | TAG_NIL))
#define FALSE_VAL       ((Value)(uint8_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint8_t)(QNAN | TAG_TRUE))
#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(num) num_to_value(num)
#define OBJ_VAL(obj)    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline Value num_to_value(double num)
{
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

typedef enum
{
    ValBool,
    ValNil,
    ValNumber,
    ValObj,
} ValueType;

typedef struct
{
    ValueType type;
    union {
        bool   boolean;
        double number;
        Obj*   obj;
    } as;
} Value;

#define IS_BOOL(value)   ((value).type == ValBool)
#define IS_NIL(value)    ((value).type == ValNil)
#define IS_NUMBER(value) ((value).type == ValNumber)
#define IS_OBJ(value)    ((value).type == ValObj)

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value)    ((value).as.obj)

#define BOOL_VAL(value)   ((Value){ValBool, {.boolean = (value)}})
#define NIL_VAL           ((Value){ValNil, {.number = 0}})
#define NUMBER_VAL(value) ((Value){ValNumber, {.number = (value)}})
#define OBJ_VAL(object)   ((Value){ValObj, {.obj = (Obj*)(object)}})

#endif

typedef struct
{
    int    count;
    int    capacity;
    Value* values;
} ValueArray;

bool values_equal(Value a, Value b);

void init_value_array(ValueArray* array);
void free_value_array(ValueArray* array);
void write_value_array(ValueArray* array, Value value);

void print_value(Value value);

#endif  // VALUE_H
