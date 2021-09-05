#ifndef VM_H
#define VM_H

#include "chunk.h"

typedef struct
{
    Chunk* chunk;
} VM;

void init_vm();
void free_vm();

#endif  // VM_H
