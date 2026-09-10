#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int g_tests = 0, g_failed = 0;

#define CHECK(expr) do { g_tests++; if (!(expr)) { g_failed++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

#endif /* TEST_UTIL_H */
