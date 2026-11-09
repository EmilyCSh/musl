#define _GNU_SOURCE
#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include "dynlink.h"
#include <sys/mman.h>
#include <unistd.h>
#include "pthread_impl.h"

#define MI_TLS_MODEL_MUSL 1
/* mimalloc reserves IDs 0, 4 and 8 for page and heap states. */
#define MI_PRIM_THREAD_ID() (((uintptr_t)__pthread_self()->tid + 2) << 2)
#define MI_PRIM_HAS_PROCESS_ATTACH 1
#undef PAGE_SIZE
#undef weak
#include <mimalloc.h>

#define libc_export __attribute__((visibility("default")))
#define libc_hidden __attribute__((visibility("hidden")))

#include "mimalloc-impl/src/static.c"

libc_hidden void __malloc_init(void)
{
	_mi_auto_process_init();
}

libc_hidden void __malloc_process_cleanup(void)
{
	_mi_auto_process_done();
}

libc_hidden void __malloc_thread_cleanup(void)
{
	_mi_thread_done(NULL);
}

libc_hidden void *__libc_malloc_impl(size_t size)
{
	return mi_malloc(size);
}

libc_hidden void *__libc_calloc(size_t count, size_t size)
{
	void *ptr = mi_calloc(count, size);
	if (!ptr) errno = ENOMEM;
	return ptr;
}

libc_hidden void __libc_free(void *ptr)
{
	mi_free(ptr);
}

libc_hidden void *__libc_realloc(void *ptr, size_t size)
{
	return mi_realloc(ptr, size);
}

libc_export void *aligned_alloc(size_t alignment, size_t size)
{
	return mi_malloc_aligned(size, alignment);
}

libc_export size_t malloc_usable_size(void *ptr)
{
	return mi_usable_size(ptr);
}

libc_hidden void __malloc_donate(char *start, char *end)
{
	(void)start;
	(void)end;
}