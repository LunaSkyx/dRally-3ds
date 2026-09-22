/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_files.h - game file check before the engine starts.
 *
 * The engine opens its assets with relative names and does not survive a failed fopen (it dereferences
 * the NULL).  This runs after the display is up: it collects what is missing, prints the list on the
 * bottom screen and waits for a button.  Called from main(), before the engine boots.
 */
#ifndef DR3_FILES_H
#define DR3_FILES_H

/* 1 when every required file is there.  Otherwise the missing ones are shown, a button press is waited
   for and 0 is returned - the caller then leaves through the normal shutdown path. */
int dr3_files_ok_or_wait(void);

#endif /* DR3_FILES_H */
