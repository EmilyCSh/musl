#include <string.h>

void *memset_explicit(void *d, int c, size_t n)
{
	d = memset(d, c, n);
	__asm__ __volatile__ ("" : : "r"(d) : "memory");
	return d;
}