#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #define CLOCKS_DEBUG
#define CLOCKS_OPTIMIZATIONS

#ifdef CLOCKS_DEBUG
#define DEBUG_PRINT_CODE       // Prints resulting bytecode from the compiler
#define DEBUG_TRACE_EXECUTION  // Print execution trace from the VM
#define DEBUG_STRESS_GC        // Stress GC by collecting before every allocation
#define DEBUG_LOG_GC           // Allocation information (bytes, type) and GC phases (mark, blacken)
#endif

#ifdef CLOCKS_OPTIMIZATIONS
#define TABLE_OPTIMIZED_FIND_ENTRY
#define VM_CACHE_IP
#define VALUE_NAN_BOXING

#define GC_OPTIMIZE_CLEARING_MARK
#define OBJECT_CACHE_CLASS_INITIALIZER
#define OBJECT_STRING_FLEXIBLE_ARRAY

#define TABLE_FNV_GCC_OPTIMIZATION
#define VM_OPTIMIZED_POP
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif  // COMMON_H
