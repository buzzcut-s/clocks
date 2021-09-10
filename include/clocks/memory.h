#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"
#include "value.h"

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity)*2)

#define GROW_ARRAY(type, pointer, old_size, new_size)     \
    (type*)reallocate(pointer, sizeof(type) * (old_size), \
                      sizeof(type) * (new_size))

#define FREE_ARRAY(type, pointer, old_size) \
    reallocate(pointer, sizeof(type) * (old_size), 0);

void* reallocate(void* pointer, size_t old_size, size_t new_size);

void mark_object(Obj* object);

void mark_value(Value value);

void collect_garbage();

void free_objects();

#endif  // MEMORY_H
