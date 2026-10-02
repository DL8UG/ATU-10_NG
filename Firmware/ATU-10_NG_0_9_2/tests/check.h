// Minimal test helpers for the host unit tests
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int check_fail, check_count;

#define CHECK(cond) do { check_count++; if(!(cond)) { check_fail++; \
   printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); } } while(0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); check_count++; \
   if(_a != _b) { check_fail++; printf("%s:%d: FAILED: %s == %s (%ld != %ld)\n", \
   __FILE__, __LINE__, #a, #b, _a, _b); } } while(0)

static int check_done(const char *name) {
   printf("%s: %d checks, %d failed\n", name, check_count, check_fail);
   return check_fail != 0;
}

#endif
