#include "pthread_impl.h"

size_t __pt_gnu_stacksize = 0;
size_t __rlimit_stacksize = 0;
size_t __user_defined_default_stacksize = 0;
size_t __default_stacksize = 0;
unsigned __default_guardsize = DEFAULT_GUARD_SIZE;
