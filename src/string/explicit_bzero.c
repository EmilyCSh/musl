#define _BSD_SOURCE
#include <string.h>

void explicit_bzero(void *d, size_t n)
{
	memset_explicit(d, 0, n);
}
