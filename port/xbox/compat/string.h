/* string.h for the Xbox build: the CRT's plus strnlen */
#ifndef RK_STRING_H
#define RK_STRING_H
#include_next <string.h>
static __inline size_t strnlen(const char* s, size_t n) { size_t i = 0; while (i < n && s[i]) i++; return i; }
#endif
