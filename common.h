#ifndef CFER_COMMON_H
#define CFER_COMMON_H

// Here are constants, like size_t, unit8_t
// Boolean types
// and a better representation of NULL ?

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING
#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

// #define DEBUG_STRESS_GC
#define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)

char* readFile(const char *path);

#endif //CFER_COMMON_H