#include "clocks/table.h"

#include <string.h>

#include <clocks/common.h>
#include <clocks/memory.h>
#include <clocks/object.h>
#include <clocks/vm.h>

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

static Entry* find_entry(Entry* entries, int capacity, const ObjString* key)
{
#ifdef TABLE_OPTIMIZED_FIND_ENTRY
    uint32_t index = key->hash & (capacity - 1);
#else
    uint32_t index = key->hash % capacity;
#endif

    Entry* tombstone = NULL;

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

#ifdef TABLE_OPTIMIZED_FIND_ENTRY
        index = (index + 1) & (capacity - 1);
#else
        index = (index + 1) % capacity;
#endif
    }
}

static void adjust_capacity(Table* table, int capacity)
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

bool table_insert(Table* table, ObjString* key, Value value)
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

bool table_find(const Table* table, const ObjString* key, Value* out_val)
{
    if (table->count == 0)
        return false;

    const Entry* res = find_entry(table->entries, table->capacity, key);
    if (res->key == NULL)
        return false;

    *out_val = res->value;
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

ObjString* table_find_string(const Table* table, const char* chars,
                             int length, uint32_t hash)
{
    if (table->count == 0)
        return NULL;

#ifdef TABLE_OPTIMIZED_FIND_ENTRY
    uint32_t index = hash & (table->capacity - 1);
#else
    uint32_t index = hash % table->capacity;
#endif

    while (true)
    {
        const Entry* entry = &table->entries[index];

        if (entry->key == NULL)
        {
            const bool not_tombstone = IS_NIL(entry->value);
            if (not_tombstone)
                return NULL;
        }
        else if (entry->key->length == length && entry->key->hash == hash
                 && memcmp(entry->key->chars, chars, length) == 0)
        {
            return entry->key;
        }

#ifdef TABLE_OPTIMIZED_FIND_ENTRY
        index = (index + 1) & (table->capacity - 1);
#else
        index = (index + 1) % table->capacity;
#endif
    }
}

void mark_table(const Table* table)
{
    for (int i = 0; i < table->capacity; i++)
    {
        const Entry* entry = &table->entries[i];
        mark_object((Obj*)entry->key);
        mark_value(entry->value);
    }
}

void table_remove_white(Table* table)
{
    for (int i = 0; i < table->capacity; i++)
    {
        const Entry* entry = &table->entries[i];
        if (entry->key != NULL
#ifdef GC_OPTIMIZE_CLEARING_MARK
            && entry->key->obj.mark != vm.mark_value)
#else
            && !entry->key->obj.is_marked)
#endif
        {
            table_remove(table, entry->key);
        }
    }
}
