#include "pthread_impl.h"

int pthread_attr_setstack(pthread_attr_t *a, void *addr, size_t size)
{
	if (__pthread_attr_clamp_stacksize(size) != size) {
		return EINVAL;
	}

	a->_a_stackaddr = (size_t)addr + size;
	a->_a_stacksize = size;
	return 0;
}
