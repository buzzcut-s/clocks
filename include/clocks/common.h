#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLOCKS_DEBUG

#ifdef CLOCKS_DEBUG
#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC
#endif

#define OPTIMIZED_TABLE_FIND_ENTRY

#define NAN_BOXING

#define UINT8_COUNT (UINT8_MAX + 1)

#endif  // COMMON_H
