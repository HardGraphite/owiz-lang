#pragma once

#include <stdlib.h>

#define ow_malloc(size)                malloc((size))
#define ow_calloc(count, size)         calloc((count), (size))
#define ow_realloc(pointer, new_size)  realloc((pointer), (new_size))
#define ow_free(pointer)               free((pointer))
