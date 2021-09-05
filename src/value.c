#include "clocks/value.h"

#include <stdio.h>

#include <clocks/memory.h>

bool values_equal(const Value a, const Value b)
{
    if (a.type != b.type)
        return false;

    switch (a.type)
    {
        case ValBool:
            return AS_BOOL(a) == AS_BOOL(b);
        case ValNil:
            return true;
        case ValNumber:
            return AS_NUMBER(a) == AS_NUMBER(b);
        default:
            return false;
    }
}

void init_value_array(ValueArray* array)
{
    array->count    = 0;
    array->capacity = 0;
    array->values   = NULL;
}

void free_value_array(ValueArray* array)
{
    FREE_ARRAY(Value, array->values, array->capacity);
    init_value_array(array);
}

void write_value_array(ValueArray* array, const Value value)
{
    if (array->capacity < array->count + 1)
    {
        const int old_capacity = array->capacity;

        array->capacity = GROW_CAPACITY(old_capacity);
        array->values   = GROW_ARRAY(Value, array->values,
                                     old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void print_value(const Value value)
{
    switch (value.type)
    {
        case ValBool:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case ValNil:
            printf("nil");
            break;
        case ValNumber:
            printf("%g", AS_NUMBER(value));
            break;
    }
}
