#define _GNU_SOURCE
#include <error.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *__progname;

void (*error_print_progname)(void);
unsigned int error_message_count;
int error_one_per_line;

static void error_vreport(int status, int errnum, const char *file,
	unsigned int line, const char *fmt, va_list ap)
{
	const char *name = program_invocation_name;

	fflush(stdout);
	flockfile(stderr);

	if (error_print_progname) {
		error_print_progname();
	} else {
		fprintf(stderr, "%s: ", name ? name : __progname);
	}

	if (file) {
        fprintf(stderr, "%s:%u: ", file, line);
    }

	if (fmt) {
        vfprintf(stderr, fmt, ap);
    }

	if (errnum) {
        fprintf(stderr, ": %s", strerror(errnum));
    }

	fputc('\n', stderr);
	error_message_count++;
	funlockfile(stderr);
	fflush(stderr);

	if (status) {
        exit(status);
    }
}

void error(int status, int errnum, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	error_vreport(status, errnum, NULL, 0, fmt, ap);
	va_end(ap);
}

void error_at_line(int status, int errnum, const char *file,
	unsigned int line, const char *fmt, ...)
{
	static const char *old_file;
	static unsigned int old_line;
	va_list ap;

	if (error_one_per_line) {
		if (old_line == line &&
		    (old_file == file || (old_file && file && !strcmp(old_file, file)))) {
			return;
		}
        
		old_file = file;
		old_line = line;
	}

	va_start(ap, fmt);
	error_vreport(status, errnum, file, line, fmt, ap);
	va_end(ap);
}