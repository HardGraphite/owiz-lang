#pragma once

#if defined(__has_include) && __has_include(<bits/wordsize.h>)
#    include <bits/wordsize.h>
#    define OW_WORDSIZE __WORDSIZE
#elif defined(_WIN32)
#    ifdef _WIN64
#        define OW_WORDSIZE 64
#    else
#        define OW_WORDSIZE 32
#    endif
#else
#    include <stdint.h>
#    if SIZE_MAX == UINT64_MAX
#        define OW_WORDSIZE 64
#    elif SIZE_MAX == UINT32_MAX
#        define OW_WORDSIZE 32
#    else
#        error "unexpected SIZE_MAX value"
#    endif
#endif
