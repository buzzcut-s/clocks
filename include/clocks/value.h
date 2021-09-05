#ifndef VALUE_H
#define VALUE_H

#include "common.h"

typedef enum
{
    ValBool,
    ValNil,
    ValNumber,
} ValueType;

typedef struct
{
    ValueType type;
    union {
        bool   boolean;
        double number;
    } as;
} Value;

#define IS_BOOL(value)   ((value).type == ValBool)
#define IS_NIL(value)    ((value).type == ValNil)
#define IS_NUMBER(value) ((value).type == ValNumber)

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value)   ((Value){ValBool, {.boolean = (value)}})
#define NIL_VAL           ((Value){ValNil, {.number = 0}})
#define NUMBER_VAL(value) ((Value){ValNumber, {.number = (value)}})

typedef struct
{
    int    count;
    int    capacity;
    Value* values;
} ValueArray;

void init_value_array(ValueArray* array);
void free_value_array(ValueArray* array);
void write_value_array(ValueArray* array, Value value);

void print_value(Value value);

#endif  // VALUE_H
