#pragma once
#include <stdint.h>


/* this gets "case 0: case 0:", which is illegal => compile-time asserts :) */
#define COMPILE_TIME_ASSERT(cond) switch(0){case 0:case cond:;}

/* linux kernel stuff... compile time checks */
#define COMPILE_TIME_ASSERT2(cond) ((void)sizeof(char[1 - 2*!!(cond)]))

/* getting the offset of a member */
//#define offsetof(type, member)  __builtin_offsetof (type, member)

