#ifndef __khrplatform_h_
#define __khrplatform_h_
#include <stdint.h>
#include <stddef.h>
typedef int8_t khronos_int8_t;
typedef uint8_t khronos_uint8_t;
typedef int16_t khronos_int16_t;
typedef uint16_t khronos_uint16_t;
typedef int32_t khronos_int32_t;
typedef uint32_t khronos_uint32_t;
typedef int64_t khronos_int64_t;
typedef uint64_t khronos_uint64_t;
typedef float khronos_float_t;
typedef intptr_t khronos_intptr_t;
typedef ptrdiff_t khronos_ssize_t;
typedef uint64_t khronos_utime_nanoseconds_t;
typedef int64_t khronos_stime_nanoseconds_t;
#define KHRONOS_MAX_ENUM 0x7FFFFFFF
typedef enum { KHRONOS_FALSE = 0, KHRONOS_TRUE = 1, KHRONOS_BOOLEAN_ENUM_FORCE_SIZE = KHRONOS_MAX_ENUM } khronos_boolean_enum_t;
#endif

