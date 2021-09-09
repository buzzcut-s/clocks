#ifndef COMPILER_H
#define COMPILER_H

#include "chunk.h"
#include "object.h"

ObjFunction* compile(const char* source);

#endif  // COMPILER_H
