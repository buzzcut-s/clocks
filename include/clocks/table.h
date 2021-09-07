#ifndef TABLE_H
#define TABLE_H

#include "object.h"
#include "value.h"

typedef struct
{
    const ObjString* key;
    Value            value;
} Entry;

typedef struct
{
    int    count;
    int    capacity;
    Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);

bool table_insert(Table* table, const ObjString* key, Value value);

void table_copy(const Table* src, Table* dest);

#endif  // TABLE_H
