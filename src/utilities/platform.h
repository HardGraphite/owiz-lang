#pragma once

#if defined(_WIN32)
#    define OW_SYSTEM_NAME "Windows"
#    define _IS_WINDOWS_ 1
#elif defined(__linux__)
#    define OW_SYSTEM_NAME "Linux"
#    define _IS_POSIX_ 1
#elif defined(__APPLE__) && defined(__MACH__)
#    define OW_SYSTEM_NAME "Apple"
#    define _IS_POSIX_ 1
#elif defined(__FreeBSD__) || defined(__FreeBSD)
#    define OW_SYSTEM_NAME "FreeBSD"
#    define _IS_POSIX_ 1
#else
#    define OW_SYSTEM_NAME "?"
#endif

#if defined(_M_IA64)
#    define OW_ARCHITECTURE "ia64"
#elif defined(__x86_64) || defined(__x86_64__) || defined(__amd64) \
        || defined(__amd64__) || defined(_M_X64) || defined(_M_AMD64)
#    define OW_ARCHITECTURE "x86_64"
#elif defined(__x86) || defined(__x86__) || defined(_M_IX86)
#    define OW_ARCHITECTURE "x86"
#elif defined(_M_ARM64)
#    define OW_ARCHITECTURE "arm64"
#elif defined(_M_ARM)
#    define OW_ARCHITECTURE "arm"
#elif defined(_M_MIPS)
#    define OW_ARCHITECTURE "mips"
#else
#    define OW_ARCHITECTURE "?"
#endif
