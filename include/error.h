#ifndef _ERROR_H
#define _ERROR_H

#include <features.h>

#ifdef __cplusplus
extern "C" {
#endif

void error(int, int, const char *, ...)
	__attribute__((__format__(__printf__, 3, 4)));
void error_at_line(int, int, const char *, unsigned int, const char *, ...)
	__attribute__((__format__(__printf__, 5, 6)));

extern void (*error_print_progname)(void);
extern unsigned int error_message_count;
extern int error_one_per_line;

#ifdef __cplusplus
}
#endif

#endif