#ifndef DEBUG_H
#define DEBUG_H

#include "chunk.h"

void disassemble_chunk(const Chunk* chunk, const char* name);
int  disassemble_instruction(const Chunk* chunk, int offset);

#endif  // DEBUG_H
