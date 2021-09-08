#ifndef COMPILER_H
#define COMPILER_H

#include <clocks/chunk.h>
#include <clocks/object.h>

ObjFunction* compile(const char* source);

#endif  // COMPILER_H
