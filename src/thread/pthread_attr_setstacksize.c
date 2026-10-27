#include "pthread_impl.h"

int pthread_attr_setstacksize(pthread_attr_t *a, size_t size)
{
	if (__pthread_attr_clamp_stacksize(size) != size)  {
		return EINVAL;
	}
	
	a->_a_stackaddr = 0;
	a->_a_stacksize = size;
	return 0;
}
