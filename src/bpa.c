#include "bpa.h"

#if defined(__3DS__) && (defined(DR3_CHAOS) || defined(DR3_USE_GFX))
#include "platform_3ds/dr3_log.h"
#endif

#pragma pack(1)
typedef struct bpa_entry_s {
    __BYTE__        name[0xd];
    __DWORD__       size;
} bpa_entry_t;
typedef struct bpa_s {
    __DWORD__       n;
    bpa_entry_t     fat[255];
} bpa_header_t;

struct BPA {
    char            file[32];
    FILE *          fd;
    bpa_header_t    header;
    int             entry;
};


static BPA BPAContext;

static char * strupr_watcom106(char * s){

    char *  p;

    p = s-1;
    while(++p&&*p){

        if((unsigned char)(*p-0x61) <= 0x19) *p -= 0x20;
    }

    return s;
}

static void BPA_decodeEntryName(char * s){

    int 	n;

    if(s){
	
        n = -1;
        while(s[++n]&&(n<0xd)) s[n] -= 0x75-3*n;
    }
}

static void BPA_encodeEntryName(char * s){

    int 	n;

    if(s){
	
        n = -1;
        while(s[++n]&&(n<0xd)) s[n] += 0x75-3*n;
    }
}

static int bpa_sanity(BPA * bpa){

    int     ok, n, i;
    size_t  size;
    size_t  save_position;

    ok = (bpa == (BPA *)0) ? BPA_FAILED : BPA_OK;

    if(ok == BPA_OK){

        if(bpa->header.n>>8){

            bpa->header.n &= 0xff;   
            ok = BPA_MALFORMED;
        }

        save_position = ftell(bpa->fd);
        fseek(bpa->fd, 0, SEEK_END);
        size = ftell(bpa->fd);
        if(size < sizeof(bpa_header_t)) ok = BPA_FAILED;
        size -= sizeof(bpa_header_t);
    }

    n = -1;
    if(ok != BPA_FAILED){

        fseek(bpa->fd, save_position, SEEK_SET);

        while(++n < 255){

            if(n < bpa->header.n){

                if(bpa->header.fat[n].size == 0){
                    
                    ok = BPA_MALFORMED;
                }

                if(bpa->header.fat[n].size > size){

                    ok = BPA_FAILED;
                    break;
                }

                size -= bpa->header.fat[n].size;

                if(bpa->header.fat[n].name[0xc] != 0){

                    ok = BPA_FAILED;
                    break;
                }
            }
            else {
                
                if(bpa->header.fat[n].size != 0){
                    
                    ok = BPA_MALFORMED;
                    break;
                }

                i = -1;
                while(++i < 0xd){

                    if(bpa->header.fat[n].name[i] != 0){

                        ok = BPA_MALFORMED;
                        break;
                    }
                }
            }
        }

        if((ok != BPA_FAILED)&&(size > 0)) ok = BPA_MALFORMED;
    }

    switch(ok){
    case BPA_OK:
        //printf("[%s] Looks like a valid BPA\n", bpa->file);
        break;
    case BPA_MALFORMED:
        printf("[%s] Looks like a malformed BPA\n", bpa->file);
        break;
    default:
        printf("[%s] Doesn't look like a valid BPA\n", bpa->file);
        break;
    }

    return ok;
}

BPA * bpa_open(const char * bpa_fname){

    int     n, sane;
    BPA *   bpa;

    bpa = &BPAContext;
    memset(bpa, 0, sizeof(BPA));

    strcpy(bpa->file, bpa_fname);
    bpa->entry = 0;
    
    bpa->fd = fopen(bpa_fname, "rb");
#if defined(__3DS__) && defined(DR3_USE_GFX)
    if(bpa->fd == (FILE *)0){
        char rom_path[64];
        snprintf(rom_path, sizeof(rom_path), "romfs:/%s", bpa_fname);
        bpa->fd = fopen(rom_path, "rb");
        if(bpa->fd != (FILE *)0){
            dr3_log("[dr3] bpa: loaded %s from built-in RomFS", bpa_fname);
        }
    }
#endif
    if(bpa->fd == (FILE *)0){
        return (BPA *)0;
    }

    fread(&bpa->header, 1, sizeof(bpa_header_t), bpa->fd);
    bpa->header.n = SDL_SwapLE32(bpa->header.n);
    for (int i = 0; i < bpa->header.n; i++)
    {
        bpa->header.fat[i].size = SDL_SwapLE32(bpa->header.fat[i].size);
    }
    
    sane = bpa_sanity(bpa);

    if(sane == BPA_FAILED){

        bpa_close(bpa);
        return (BPA *)0;
    }

#if defined(__3DS__) && defined(DR3_CHAOS)
    /*
     * The chaos test build has to say *why* a read came up empty: this is the check that ends the game
     * with "Problems with [...] file!" after a race (the palette is black by then, so it looks like a
     * hang first).  A header with no entries in a file that has data means the header read did not
     * deliver.
     */
    if(sane != BPA_OK){

        dr3_log("[dr3] chaos: BPA %s: fd=%p, entries=%d, first bytes %02x %02x %02x %02x",
                bpa->file, bpa->fd, (int)bpa->header.n,
                bpa->header.fat[0].name[0], bpa->header.fat[0].name[1],
                bpa->header.fat[0].name[2], bpa->header.fat[0].name[3]);
        dr3_log_flush();
    }
#endif

    n = -1;
    while(++n < bpa->header.n) BPA_decodeEntryName(bpa->header.fat[n].name);

    return bpa;
}

void bpa_read(BPA * bpa, __POINTER__ dst){

    if(bpa != (BPA *)0){

        if(bpa->entry == bpa->header.n){

            printf("[%s] There is nothing to read!\n", bpa->file);
            return;
        }

        if(dst){
            
            //printf("[%s] Extracting ... %12s [%d]\n", bpa->file, bpa_entry_name(bpa), bpa_entry_size(bpa));
#if defined(__3DS__) && defined(DR3_CHAOS)
            /*
             * A read that comes up short leaves the caller with half a picture (the chaos test build saw
             * grey lines in the car sprites and cars that were there but invisible) and it is also what
             * makes a BPA header look malformed.  The chaos build checks the count, reads the entry again
             * once and zero fills whatever is still missing, so nothing parses stale bytes.
             */
            {
                size_t  want = bpa_entry_size(bpa);
                long    at   = ftell(bpa->fd);
                size_t  got  = fread(dst, 1, want, bpa->fd);

                if(got != want){

                    dr3_log("[dr3] chaos: short read of %s from %s (%u of %u bytes) - retrying",
                            bpa_entry_name(bpa), bpa->file, (unsigned)got, (unsigned)want);
                    dr3_log_flush();

                    fseek(bpa->fd, at, SEEK_SET);
                    got = fread(dst, 1, want, bpa->fd);

                    if(got != want){

                        dr3_log("[dr3] chaos: retry gave %u of %u bytes - the rest is zeroed",
                                (unsigned)got, (unsigned)want);
                        dr3_log_flush();

                        memset((__BYTE__ *)dst + got, 0, want - got);
                    }

                    bpa->entry++;
                    return;
                }
            }

            /* the read went through - nothing more to do here */
#else
            fread(dst, 1, bpa_entry_size(bpa), bpa->fd);
#endif
        }
        else {

            fseek(bpa->fd, bpa_entry_size(bpa), SEEK_CUR);
        }

        bpa->entry++;
    }
}

void bpa_search(BPA * bpa, const char * bpa_entry){

    int     n, offset;
    char    entry_name[0xd];

    if(bpa != (BPA *)0){

        strupr_watcom106(strcpy(entry_name, bpa_entry));

        offset = sizeof(bpa_header_t);
        n = -1;
        while((++n < bpa->header.n)&&strcmp(bpa->header.fat[n].name, entry_name)) offset += bpa->header.fat[n].size;

        bpa->entry = n;
        fseek(bpa->fd, offset, SEEK_SET);

        if(n == bpa->header.n){

            printf("[%s] BPA doesn't contain %s!\n", bpa->file, entry_name);
        }
    }
}

size_t bpa_entry_size(BPA * bpa){

    if(bpa == (BPA *)0) return 0;
    if(bpa->entry == bpa->header.n) return 0;

    return bpa->header.fat[bpa->entry].size;
}

const char * bpa_entry_name(BPA * bpa){

    if(bpa == (BPA *)0) return (__POINTER__)0;
    if(bpa->entry == bpa->header.n) return (__POINTER__)0;

    return bpa->header.fat[bpa->entry].name;
}

void bpa_close(BPA * bpa){

    if(bpa != (BPA *)0){

        if(bpa->fd) fclose(bpa->fd);

        memset(bpa, 0, sizeof(BPA));
    }
}

