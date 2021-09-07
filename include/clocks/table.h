#ifndef TABLE_H
#define TABLE_H

#include "object.h"
#include "value.h"

typedef struct
{
    ObjString* key;
    Value      value;
} Entry;

typedef struct
{
    int    count;
    int    capacity;
    Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);

#endif  // TABLE_H
