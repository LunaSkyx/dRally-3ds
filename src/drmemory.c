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
 * The caller a block was freed from, for the quarantine report below.  GCC has __builtin_return_address,
 * MSVC has _ReturnAddress() - the PC harness build (-DDR3_HOST) is compiled with MSVC.
 */
#if defined(_MSC_VER)
#include <intrin.h>
#define DR3_FREED_BY()  ((void *)_ReturnAddress())
#else
#define DR3_FREED_BY()  (__builtin_return_address(0))
#endif

/*
 * Chaos test build (see doc/3ds.md): every block gets four magic bytes behind it and they are checked
 * before each new allocation.  A race with twelve cars can run a buffer over that the original - sized
 * for four - was written against, and a wrecked malloc block only shows up much later, as a hang inside
 * malloc (or as "ESP Fatal Error").  The report names the block whose guard is gone and the block right
 * in front of it, which is the one the write came out of.
 */
#define DR3_HEAP_GUARD_BYTES    4
/* behind the guard: an overrun stops in here instead of wrecking malloc's chunk header */
#define DR3_HEAP_SLACK_BYTES    0x1000
#define DR3_HEAP_SLACK_FILL     0xe7
static const unsigned char dr3_heap_guard[DR3_HEAP_GUARD_BYTES] = { 0xa5, 0x5a, 0xc3, 0x3c };
/*
 * Re-fills the guard and the slack behind a block after a check found them damaged - that is what makes the
 * next check (one pass later) say whether the write is still happening, instead of reporting the same old
 * damage for every pass of every frame.
 */
void dr3_drheap_refill(void *p, unsigned nbytes)
{
    unsigned char * g = (unsigned char *)p + nbytes;

    if(p == (void *)0) return;

    memcpy(g, dr3_heap_guard, DR3_HEAP_GUARD_BYTES);
    memset(g + DR3_HEAP_GUARD_BYTES, DR3_HEAP_SLACK_FILL, DR3_HEAP_SLACK_BYTES);
}

static int dr3_heap_reports;
#define DR3_HEAP_REPORT_MAX 40      /* how many overruns are reported (the first one used to be the last) */

/*
 * Checks the guard and the slack the allocator put behind a block and returns the offset past the end at
 * which they are damaged (or -1 when the block is intact).  The race frame calls this for the screen buffer
 * after every drawing pass, so the pass that writes past the end of the screen names itself in the log.
 */
int dr3_drheap_overrun(const void *p, unsigned nbytes)
{
    const unsigned char * g = (const unsigned char *)p + nbytes;
    unsigned              n;

    if(p == (const void *)0) return -1;

    for(n = 0; n < DR3_HEAP_GUARD_BYTES; ++n)
        if(g[n] != dr3_heap_guard[n]) return (int)(nbytes + n);

    for(n = 0; n < DR3_HEAP_SLACK_BYTES; ++n)
        if(g[DR3_HEAP_GUARD_BYTES + n] != DR3_HEAP_SLACK_FILL)
            return (int)(nbytes + DR3_HEAP_GUARD_BYTES + n);

    return -1;
}

/*
 * A block that is given back is parked here instead of being freed.  Writing into a block *after* it was
 * freed is what corrupts malloc's free list, and that shows up as a hang inside malloc, far away from
 * the code that did it - with the guard bytes unable to see it, because the block is not the engine's
 * any more.  Parked blocks are filled with a pattern and watched, and the ring keeps the extra memory
 * bounded: the oldest parked block is freed for real once the ring wraps.
 */
#define DR3_HEAP_QUARANTINE     512
#define DR3_HEAP_QUAR_BUDGET    0x2000000u    /* at most 32 MB of parked blocks */
#define DR3_HEAP_QUAR_FILL      0xdd
#define DR3_HEAP_QUAR_FILL_MAX  0x10000       /* park big blocks, but only wipe their first 64 KB */
#define DR3_HEAP_QUAR_CHECK     0x400         /* how much of a parked block the check looks at */

typedef struct {
    __POINTER__ linear;
    __DWORD__   nbytes;
    __POINTER__ freed_by;
} dr3_quar_t;

static dr3_quar_t dr3_quar[DR3_HEAP_QUARANTINE];
static int        dr3_quar_next;
static __DWORD__  dr3_quar_bytes;
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
 * Parks a block the engine gave back (see the quarantine notes above) and reports writes into parked
 * blocks, which is what a damaged malloc free list comes from.
 */
static void dr3_drheap_park(__POINTER__ mem, __DWORD__ nbytes, __POINTER__ freed_by){

    dr3_quar_t * slot = &dr3_quar[dr3_quar_next];
    __DWORD__    fill = (nbytes < DR3_HEAP_QUAR_FILL_MAX) ? nbytes : DR3_HEAP_QUAR_FILL_MAX;

    /* keep the ring inside its budget: give up the oldest parked blocks for real */
    while((slot->linear != (__POINTER__)0) && ((dr3_quar_bytes + nbytes) > DR3_HEAP_QUAR_BUDGET)){

        dr3_quar_bytes -= slot->nbytes;
        free(slot->linear);
        slot->linear = (__POINTER__)0;
        slot->nbytes = 0;

        /* the oldest is the ring's *next* slot once it wraps - walk on until room is free */
        dr3_quar_next = (dr3_quar_next + 1) % DR3_HEAP_QUARANTINE;
        slot = &dr3_quar[dr3_quar_next];
    }

    if(slot->linear != (__POINTER__)0){                  /* the oldest one goes for real */

        dr3_quar_bytes -= slot->nbytes;
        free(slot->linear);
    }

    slot->linear   = mem;
    slot->nbytes   = nbytes;
    slot->freed_by = freed_by;

    dr3_quar_bytes += nbytes;

    dr3_quar_next = (dr3_quar_next + 1) % DR3_HEAP_QUARANTINE;

    if(mem != (__POINTER__)0) memset(mem, DR3_HEAP_QUAR_FILL, fill);
}

/*
 * Walks the registry and checks the guard bytes behind every block (see the notes above); reports the
 * first block whose guard is gone plus the block right in front of it, which is the one that overran.
 */
void dr3_drheap_guard(const char * where){

    AllocEntry *    ptr;
    __DWORD__       n, m, over;
    unsigned char * end;

    if(dr3_heap_reports >= DR3_HEAP_REPORT_MAX) return;

    /* first the blocks the engine already gave back: a write into one of these wrecks malloc */
    for(m = 0; m < DR3_HEAP_QUARANTINE; m++){

        unsigned char * p    = (unsigned char *)dr3_quar[m].linear;
        __DWORD__       seen = (dr3_quar[m].nbytes < DR3_HEAP_QUAR_CHECK) ? dr3_quar[m].nbytes : DR3_HEAP_QUAR_CHECK;

        if(p == (unsigned char *)0) continue;

        for(n = 0; n < seen; n++){

            if(p[n] != DR3_HEAP_QUAR_FILL){

                dr3_log("[dr3] chaos: FREED block written to (%s): %u bytes at %p, offset %u, freed by %p,"
                        " bytes %02x %02x %02x %02x %02x %02x %02x %02x",
                        where, (unsigned)dr3_quar[m].nbytes, p, (unsigned)n, dr3_quar[m].freed_by,
                        p[n], p[n+1], p[n+2], p[n+3], p[n+4], p[n+5], p[n+6], p[n+7]);

                ++dr3_heap_reports;
                dr3_log_flush();        /* the heap may go bad next - get this out first */
                return;
            }
        }
    }

    if((ptr = (AllocEntry *)___24ccb0h.linear) == (AllocEntry *)0) return;

    for(n = 0; (n < (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) && ptr[n].type; n++){

        unsigned char * guard;
        unsigned char * slack;

        if(ptr[n].type != DOS4G_MEMORY) continue;

        guard = (unsigned char *)ptr[n].linear + ptr[n].nbytes;
        end   = guard + DR3_HEAP_GUARD_BYTES + DR3_HEAP_SLACK_BYTES;

        /* how far past the end the write went: -1 = nothing, 0..3 = into the guard, >=4 = into the
           slack (which keeps it away from malloc), -2 = the whole slack is used up */
        over = 0xffffffff;

        for(slack = guard + DR3_HEAP_GUARD_BYTES; slack < end; slack++){

            if(*slack != DR3_HEAP_SLACK_FILL){ over = (__DWORD__)(slack - guard); break; }
        }

        if(over == 0xffffffff) continue;

        dr3_log("[dr3] chaos: heap guard broken (%s): block %u, %u bytes at %p, written %u bytes past the end",
                where, (unsigned)n, (unsigned)ptr[n].nbytes, ptr[n].linear, (unsigned)over);

        for(m = 0; (m < (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) && ptr[m].type; m++){

            if(ptr[m].type != DOS4G_MEMORY) continue;

            if(((unsigned char *)ptr[m].linear + ptr[m].nbytes) == (unsigned char *)ptr[n].linear){

                dr3_log("[dr3] chaos:   overrun by block %u, %u bytes at %p",
                        (unsigned)m, (unsigned)ptr[m].nbytes, ptr[m].linear);
            }
        }

        ++dr3_heap_reports;
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
    if((alloc = malloc(size + DR3_HEAP_GUARD_BYTES + DR3_HEAP_SLACK_BYTES)) != (__POINTER__)0){

        ptr[n].linear = alloc;
        ptr[n].nbytes = size;
        ptr[n].type = DOS4G_MEMORY;

        memcpy((unsigned char *)alloc + size, dr3_heap_guard, DR3_HEAP_GUARD_BYTES);
        memset((unsigned char *)alloc + size + DR3_HEAP_GUARD_BYTES, DR3_HEAP_SLACK_FILL, DR3_HEAP_SLACK_BYTES);
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

#if defined(DR3_CHAOS)
    dr3_drheap_guard("free");
#endif

    ptr = (AllocEntry *)___24ccb0h.linear;

    n = 0;
    while((n < (MEM_REGISTRY_SIZE/sizeof(AllocEntry)))&&(mem != ptr[n].linear)) n++;

    if(n == (MEM_REGISTRY_SIZE/sizeof(AllocEntry))) ___58b20h(0xe);

    switch(ptr[n].type){
    case NO_MEMORY:
        ___58b20h(0xe);
        break;
    case DOS4G_MEMORY:
#if defined(DR3_CHAOS)
        dr3_drheap_park(ptr[n].linear, ptr[n].nbytes, DR3_FREED_BY());
#else
        free(ptr[n].linear);
#endif
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
