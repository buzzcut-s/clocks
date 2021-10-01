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
#define OPTIMIZED_TABLE_FIND_ENTRY
#define NAN_BOXING
#define OPTIMIZED_POP
#define FNV_GCC_OPTIMIZATION
#define CACHE_CLASS_INITIALIZER
#define GC_OPTIMIZE_CLEARING_MARK
#define VM_CACHE_IP
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif  // COMMON_H
