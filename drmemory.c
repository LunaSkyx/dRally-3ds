#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drmemory.h"


typedef enum MEM_TYPE {
    NO_MEMORY, DOS4G_MEMORY
} MemoryType;

#pragma pack(1)

#if defined(DR3_CHAOS)
#include "platform_3ds/dr3_log.h"

/*
 * Chaos test build (see doc/3ds.md): every block gets four magic bytes behind it and they are checked
 * before each new allocation.  A race with twelve cars can run a buffer over that the original - sized
 * for four - was written against, and a wrecked malloc block only shows up much later, as a hang inside
 * malloc (or as "ESP Fatal Error").  The report names the block whose guard is gone and the block right
 * in front of it, which is the one the write came out of.
 */
#define DR3_HEAP_GUARD_BYTES    4
static const unsigned char dr3_heap_guard[DR3_HEAP_GUARD_BYTES] = { 0xa5, 0x5a, 0xc3, 0x3c };
static int dr3_heap_guard_reported;
#endif

typedef struct AllocBase {
    __POINTER__     linear;     // +0
    __DWORD__       nbytes;     // +4
} AllocBase;

typedef struct AllocEntry {
	__BYTE__ 	    type;		// +0
	__POINTER__     linear;		// +1
	__DWORD__ 	    nbytes;		// +5
	__DWORD__ 	    handle;		// +9
	__WORD__ 	    selector;	// +0dh
	__WORD__ 	    segment;	// +0fh
	__BYTE__ 	    lock;		// +11h
} AllocEntry;

#define MEM_REGISTRY_SIZE   0x10000

static AllocBase ___24ccb0h;

void ___58b20h(unsigned int err_n, ...);

#if defined(DR3_CHAOS)
/*
 * Walks the registry and checks the guard bytes behind every block (see the notes above); reports the
 * first block whose guard is gone plus the block right in front of it, which is the one that overran.
 */
void dr3_drheap_guard(const char * where){

    AllocEntry *     ptr;
    __DWORD__       n, m;
    unsigned char * guard_at;

    if(dr3_heap_guard_reported) return;

    if((ptr = (AllocEntry *)___24ccb0h.linear) == (AllocEntry *)0) return;

    for(n = 0; (n < (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) && ptr[n].type; n++){

        if(ptr[n].type != DOS4G_MEMORY) continue;

        guard_at = (unsigned char *)ptr[n].linear + ptr[n].nbytes;

        if(memcmp(guard_at, dr3_heap_guard, DR3_HEAP_GUARD_BYTES) == 0) continue;

        dr3_log("[dr3] chaos: heap guard broken (%s): block %u, %u bytes at %p",
                where, (unsigned)n, (unsigned)ptr[n].nbytes, ptr[n].linear);

        for(m = 0; (m < (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) && ptr[m].type; m++){

            if(ptr[m].type != DOS4G_MEMORY) continue;

            if(((unsigned char *)ptr[m].linear + ptr[m].nbytes) == (unsigned char *)ptr[n].linear){

                dr3_log("[dr3] chaos:   overrun by block %u, %u bytes at %p",
                        (unsigned)m, (unsigned)ptr[m].nbytes, ptr[m].linear);
            }
        }

        dr3_heap_guard_reported = 1;
        dr3_log_flush();        /* the heap is damaged - the game may not get another chance to log */
        return;
    }
}
#endif

__POINTER__ dRMemory_alloc(size_t size){

    __POINTER__          alloc;
    __DWORD__           alloc_dos, n;
    AllocEntry *    ptr;

    ptr = (AllocEntry *)___24ccb0h.linear;

#if defined(DR3_CHAOS)
    dr3_drheap_guard("alloc");
#endif

    n = 0;
    while(ptr[n].type&&(n < (MEM_REGISTRY_SIZE/sizeof(AllocEntry)))) n++;

    if(n == (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) ___58b20h(0xd);

    if(size >= 0x100000){

        size |= 0xfff;
        size++;
    }

#if defined(DR3_CHAOS)
    if((alloc = malloc(size + DR3_HEAP_GUARD_BYTES)) != (__POINTER__)0){

        ptr[n].linear = alloc;
        ptr[n].nbytes = size;
        ptr[n].type = DOS4G_MEMORY;

        memcpy((unsigned char *)alloc + size, dr3_heap_guard, DR3_HEAP_GUARD_BYTES);
    }
#else
    if((alloc = malloc(size)) != (__POINTER__)0){

        ptr[n].linear = alloc;
        ptr[n].nbytes = size;
        ptr[n].type = DOS4G_MEMORY;
    }
#endif

    return ptr[n].linear;
}

void dRMemory_free(__POINTER__ mem){

    __DWORD__           n;
    AllocEntry *    ptr;

    ptr = (AllocEntry *)___24ccb0h.linear;

    n = 0;
    while((n < (MEM_REGISTRY_SIZE/sizeof(AllocEntry)))&&(mem != ptr[n].linear)) n++;

    if(n == (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) ___58b20h(0xe);

    switch(ptr[n].type){
    case NO_MEMORY:
        ___58b20h(0xe);
        break;
    case DOS4G_MEMORY:
        free(ptr[n].linear);
        break;
    default:
        break;
    }

    ptr[n].type = NO_MEMORY;
}

__POINTER__ dRMemory_resize(__POINTER__ mem, size_t size){
/*
    __DWORD__           n;
    AllocEntry *    ptr;
    __POINTER__          new_mem;

    ptr = ___24ccb0h.linear;

    n = 0;
    while((mem != ptr[n].linear)&&(n < (MEM_REGISTRY_SIZE/sizeof(AllocEntry)))) n++;

    if(n == (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) ___58b20h(0xf);

    switch(ptr[n].type){
    case NO_MEMORY:
        ___58b20h(0xf);
        break;
    case DOS4G_MEMORY:
        new_mem = realloc(ptr[n].linear, size);
        if(new_mem == 0) ___58b20h(0xf);
        ptr[n].linear = new_mem;
        break;
    default:
        break;
    }

    ptr[n].nbytes = size;

    return ptr[n].linear;
*/

    return mem;
}

void dRally_Memory_init(void){

    __POINTER__  alloc;
    size_t  size = MEM_REGISTRY_SIZE;

    if(size >= 0x100000){
    
        size |= 0xfff;
        size++;
    }

    alloc = malloc(size);

    if(alloc == 0) ___58b20h(4);
    
    ___24ccb0h.linear = alloc;
    ___24ccb0h.nbytes = size;

    memset(___24ccb0h.linear, 0, size);
}

void dRally_Memory_clean(void){

    __DWORD__           n;
    AllocEntry *    ptr;


    if((ptr = (AllocEntry *)___24ccb0h.linear) != (AllocEntry *)0){    

        n = 0;
        while(n < (MEM_REGISTRY_SIZE/sizeof(AllocEntry))){

            if(ptr[n].type != NO_MEMORY) dRMemory_free(ptr[n].linear);
            n++;
        }

        free(___24ccb0h.linear);
        memset(&___24ccb0h, 0, sizeof(AllocBase));
    }
}
