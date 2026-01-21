#ifndef _XPRINTF_H_
#define _XPRINTF_H_

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void xprintf(const char *format, ...);

#ifdef __cplusplus
}
#endif


#endif
