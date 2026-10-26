#define _BSD_SOURCE
#include <stdlib.h>
#include <unistd.h>

void arc4random_buf(void *buf, size_t len)
{
	unsigned char *p = buf;

	while (len) {
		size_t n = len < 256 ? len : 256;

		if (getentropy(p, n)) {
             abort();
        }

		p += n;
		len -= n;
	}
}

uint32_t arc4random(void)
{
	uint32_t value;
	arc4random_buf(&value, sizeof value);
	return value;
}

uint32_t arc4random_uniform(uint32_t upper_bound)
{
	uint32_t threshold, value;

	if (upper_bound < 2) {
        return 0;
    }

	threshold = -upper_bound % upper_bound;

	do {
		value = arc4random();
	} while (value < threshold);
    
	return value % upper_bound;
}