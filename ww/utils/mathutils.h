#pragma once
#include <math.h> //cel,log2,pow
#include <stdlib.h>
#include <stdint.h>
#include "hplatform.h"

#if defined(OS_UNIX)
#include <sys/types.h>
#elif ! defined(ssize_t)
typedef int64_t    ssize_t;
#endif


#undef max
#undef min
static inline ssize_t min(ssize_t x, ssize_t y) { return (((x) < (y)) ? (x) : (y)); }
static inline ssize_t max(ssize_t x, ssize_t y) { return (((x) < (y)) ? (y) : (x)); }
