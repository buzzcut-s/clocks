#ifndef VALUE_H
#define VALUE_H

#include "common.h"

typedef double Value;

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
