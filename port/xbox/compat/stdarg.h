/*
 * stdarg.h for the Xbox build: clang's built-ins in place of the XDK's (Visual C++ 2003)
 * StdArg.h, whose va_start takes the address of the last named parameter and steps past it.
 * That only holds while the function keeps its own frame: clang inlines a variadic function
 * that has no built-in va_start, and the "arguments" become unrelated stack (rk_log printed
 * garbage, then faulted in printf). The type stays the CRT's char* va_list.
 */
#ifndef RK_STDARG_H
#define RK_STDARG_H
#define _INC_STDARG   /* the XDK's StdArg.h stays out */
#ifndef _VA_LIST_DEFINED
typedef char* va_list;
#define _VA_LIST_DEFINED
#endif
#define va_start(ap, v) __builtin_va_start(ap, v)
#define va_arg(ap, t) __builtin_va_arg(ap, t)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(d, s) __builtin_va_copy(d, s)
#endif
