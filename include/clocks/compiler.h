#ifndef COMPILER_H
#define COMPILER_H

#include "chunk.h"
#include "object.h"

ObjFunction* compile(const char* source);

void mark_compiler_roots();

#endif  // COMPILER_H
