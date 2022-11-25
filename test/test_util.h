#pragma once

#define TEST_ASSERT(EXPR) \
	((EXPR) ? (void)0 : __test_assert_fail(#EXPR, __FILE__, __LINE__))

#define TEST_ASSERT_EQ(LHS, RHS) \
	TEST_ASSERT((LHS) == (RHS))

#define TEST_ASSERT_NE(LHS, RHS) \
	TEST_ASSERT((LHS) != (RHS))

#include <stdio.h>
#include <stdlib.h>

_Noreturn static inline void __test_assert_fail(
		const char *expr, const char *file, unsigned int line) {
	fprintf(stderr, "%s:%u: assertion failed: %s\n", file, line, expr);
	exit(EXIT_FAILURE);
}
