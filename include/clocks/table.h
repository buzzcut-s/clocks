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

bool table_insert(Table* table, ObjString* key, Value value);
bool table_find(const Table* table, const ObjString* key, Value* out_val);
bool table_remove(Table* table, const ObjString* key);

void table_copy(const Table* src, Table* dest);

ObjString* table_find_string(const Table* table, const char* chars,
                             int length, uint32_t hash);

void mark_table(const Table* table);
void table_remove_white(Table* table);

#endif  // TABLE_H
