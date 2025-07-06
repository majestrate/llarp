#pragma once
#ifdef ENABLE_JEMALLOC
#ifdef __cplusplus
#include "alloc.hpp"
#else
#include <jemalloc/jemalloc.h>
#endif
#endif
