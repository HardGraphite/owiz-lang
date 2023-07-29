#pragma once

#ifndef NDEBUG
#    include <stdlib.h>
#    define ow_unreachable() abort()
#elif defined __GNUC__
#    define ow_unreachable() __builtin_unreachable()
#elif defined _MSC_VER // MSVC
#    define ow_unreachable() __assume(0)
#else
#    include <stdlib.h>
#    define ow_unreachable() abort()
#endif
