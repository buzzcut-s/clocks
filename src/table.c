#include "clocks/table.h"

#include <clocks/memory.h>
#include <clocks/object.h>

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table)
{
    table->count    = 0;
    table->capacity = 0;
    table->entries  = NULL;
}

void free_table(Table* table)
{
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

static Entry* find_entry(Entry* entries, const int capacity, const ObjString* key)
{
    uint32_t index     = key->hash % capacity;
    Entry*   tombstone = NULL;

    while (true)
    {
        Entry* entry = &entries[index];

        if (entry->key == NULL)
        {
            if (IS_NIL(entry->value))
                return tombstone == NULL ? entry : tombstone;

            if (tombstone == NULL)
                tombstone = entry;
        }
        else if (entry->key == key)
            return entry;

        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(Table* table, const int capacity)
{
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++)
    {
        entries[i].key   = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++)
    {
        const Entry* entry = &table->entries[i];
        if (entry->key == NULL)
            continue;

        Entry* dest = find_entry(entries, capacity, entry->key);
        dest->key   = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries  = entries;
    table->capacity = capacity;
}

bool table_insert(Table* table, const ObjString* key, const Value value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        const int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    Entry* res = find_entry(table->entries, table->capacity, key);

    const bool is_new_key = (res->key == NULL);
    if (is_new_key)
    {
        res->key = key;

        const bool not_tombstone = IS_NIL(res->value);
        if (not_tombstone)
            table->count++;
    }

    res->value = value;
    return is_new_key;
}

bool table_find(const Table* table, const ObjString* key, Value* value)
{
    if (table->count == 0)
        return false;

    const Entry* res = find_entry(table->entries, table->capacity, key);
    if (res->key == NULL)
        return false;

    *value = res->value;
    return true;
}

bool table_remove(Table* table, const ObjString* key)
{
    if (table->count == 0)
        return false;

    Entry* res = find_entry(table->entries, table->capacity, key);
    if (res->key == NULL)
        return false;

    res->key   = NULL;
    res->value = BOOL_VAL(true);
    return true;
}

void table_copy(const Table* src, Table* dest)
{
    for (int i = 0; i < src->capacity; i++)
    {
        const Entry* entry = &src->entries[i];
        if (entry->key != NULL)
            table_insert(dest, entry->key, entry->value);
    }
}
