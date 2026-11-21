#include "drally.h"
#include "bpa.h"

#pragma pack(push,1)

typedef struct bpa_entry_s {
    __BYTE__    name[0xd];
    __DWORD__   size;
} bpa_entry_t;

typedef struct bpa_s {
    __DWORD__       n;
    bpa_entry_t     fat[255];
} bpa_t;

#pragma pack(pop)

char * strupr_watcom106(char *);

void ___42944h(const char *);
void __VGA3_SETMODE(void);


static bpa_t bpa;

static void BPA_encodeEntryName(char * s){

    int 	n;

    if(s){
	
        n = -1;
        while(s[++n]) s[n] += 0x75-3*n;
    }
}

void old_bpa_read(const char * bpa_fname, __POINTER__ dst, const char * bpa_entry){

    char    err[0x50];
    char    u_bpa_fname[32];
    BPA *   bpa;

    strupr_watcom106(strcpy(u_bpa_fname, bpa_fname));
    bpa = bpa_open(u_bpa_fname);
    bpa_search(bpa, bpa_entry);

#if defined(__3DS__) && defined(DR3_CHAOS)
    /*
     * A BPA that comes up empty here ends the game (___42944h below), and the chaos test build hit that
     * on the way into the second race - the read of the header had not delivered, once.  One fresh
     * attempt is logged and keeps the run going; if it fails too, the error path is still taken.
     */
    if(bpa_entry_size(bpa) == 0){

        dr3_log("[dr3] chaos: BPA read of %s from %s came up empty - retrying", bpa_entry, bpa_fname);
        dr3_log_flush();

        bpa_close(bpa);
        bpa = bpa_open(u_bpa_fname);
        bpa_search(bpa, bpa_entry);

        dr3_log("[dr3] chaos: retry gave %s (%u bytes)", (bpa_entry_size(bpa) != 0) ? "the entry" : "nothing",
                (unsigned)bpa_entry_size(bpa));
        dr3_log_flush();
    }
#endif

    if(bpa_entry_size(bpa) != 0){
        
        bpa_read(bpa, dst);
        bpa_close(bpa);
    }
    else {

        strcpy(err, "Problems with [");
        strcat(err, bpa_fname);
        strcat(err, "] ");
        strcat(err, bpa_entry);
        strcat(err, " file!");
        bpa_close(bpa);
        ___42944h(err);
    }
}

void ___42944h(const char * err){

	__VGA3_SETMODE();
	printf("DEATH RALLY Error: %s\n", err);
	printf("Please consult DRHELP.EXE for more information on how to resolve this problem.\n");
	exit(0x70);
}
