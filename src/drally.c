#include "drally.h"
#include "drally_display.h"

#if defined(__3DS__)
#include "platform_3ds/dr3_files.h"
#include "platform_3ds/dr3_log.h"
#include "platform_3ds/dr3_paths.h"
#include "platform_3ds/dr3_prof.h"
#endif
#if defined(DR3_HOST)
#include "platform_3ds/dr3_host.h"      /* the PC harness: script, switches and the quick race */
#endif

#if defined(DR_MULTIPLAYER)
extern __DWORD__ ___19bd60h;
void ___623d4h(void);
#endif

extern void_cb ___2432c8h;

void dRally_Keyboard_init(void);
void ___60466h(int, int);
void ___3e720h(void);
void __VGA3_SETMODE(void);
void dRally_System_init(void);
void dRally_Sound_quit(void);
void dRally_System_clean(void);

static void ___10060h(void){

	printf("\nDeath Rally *** Full Version 1.1\n");
}

static void ___100dch(void){

	__VGA3_SETMODE();
	printf("DEATH RALLY Exit: CTRL+ALT+DEL pressed!\n");
	exit(0x70);
}

int main(int argc, char * argv[]){

#if defined(DR3_HOST)
    /*
     * The PC harness (dr3_host.h): it reads dr3_host.txt / dr3_keys.txt from the current directory, sets
     * the log switches and drives the front end from there.  It runs before anything else, because the
     * engine's own first line ("boot") should already be in the harness' log.
     */
    dr3_host_init(argc, argv);
#endif
#if defined(__3DS__)
	dr3_log("[dr3] === dRally 3DS boot ===");
	dr3_fix_paths(argc > 0 ? argv[0] : NULL);	/* assets are opened relative to the CWD */

	/* autonomous profiler (debug build only): clock calibration + bottom screen display */
	dr3_prof_init();
#endif

	dRally_System_init();
#if defined(DR_LETTERBOX)
	dRally_Display_init(W_LETTERBOX);
#else
	dRally_Display_init(W_SHRINK);
#endif // DR_LETTERBOX
#if defined(__3DS__) && defined(DR3_USE_GFX)
	dr3_log("[dr3] display init returned");

	/* the engine dereferences a failed fopen, so list what is missing instead of crashing.
	   DR3_USE_GFX keeps this out of the MSVC check build, which has no libctru (same as the
	   dr3_bottom hooks in drally_linux_c.c) */
	if (!dr3_files_ok_or_wait()) {
		dRally_Display_clean();
		dRally_System_clean();
		return 1;
	}
#endif
	___10060h();
	___60466h(70, 1);
	___2432c8h = &___100dch;
	dRally_Keyboard_init();
#if defined(__3DS__)
	dr3_log("[dr3] keyboard init returned");
#endif
	___3e720h();
#if defined(__3DS__)
	dr3_log("[dr3] entering game (___3e720h returned)");
#endif

#if defined(DR_MULTIPLAYER)
	if(___19bd60h != 0) ___623d4h();
#endif

	dRally_Sound_quit();
	dRally_Display_clean();
	dRally_System_clean();

#if defined(DR3_HOST)
	dr3_host_shutdown();        /* the harness' summary, and the last flush of the log */
#endif

	return 0;
}
