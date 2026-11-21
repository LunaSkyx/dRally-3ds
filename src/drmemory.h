#ifndef __DRMEMORY_H
#define __DRMEMORY_H

#include "types.h"

__POINTER__ dRMemory_alloc(size_t size);
__POINTER__ dRMemory_resize(__POINTER__ mem, size_t size);
void dRMemory_free(__POINTER__ mem);

#if defined(DR3_CHAOS)
/* chaos test build: checks the guard bytes behind every block this allocator handed out (drmemory.c) */
void dr3_drheap_guard(const char * where);
#endif

#endif // __DRMEMORY_H
