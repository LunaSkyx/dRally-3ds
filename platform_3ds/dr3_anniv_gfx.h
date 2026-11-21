#ifndef DR3_ANNIV_GFX_H
#define DR3_ANNIV_GFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Procedural 1996-style VGA graphics for car 6 (Anniversary / SPECIAL) so buying it
 * in the shop never reads past the 6-slot MENU.BPA buffers and displays a distinct
 * stealth-obsidian supercar with a swept GT rear wing, twin front autocannons,
 * live player-color racing trim, and an authentic chrome "ANNIVERSARY" nameplate.
 */

#define DR3_ANNIV_SCENECAR_W    96
#define DR3_ANNIV_SCENECAR_H    64
#define DR3_ANNIV_SCENECAR_SZ   (DR3_ANNIV_SCENECAR_W * DR3_ANNIV_SCENECAR_H) /* 0x1800 */

#define DR3_ANNIV_CARNAME_W     96
#define DR3_ANNIV_CARNAME_H     16
#define DR3_ANNIV_CARNAME_SZ    (DR3_ANNIV_CARNAME_W * DR3_ANNIV_CARNAME_H)   /* 0x0600 */

#define DR3_ANNIV_CARRES_W      100
#define DR3_ANNIV_CARRES_H      52
#define DR3_ANNIV_CARRES_SLICE  (DR3_ANNIV_CARRES_W * DR3_ANNIV_CARRES_H)     /* 0x1450 */
#define DR3_ANNIV_CARRES_SZ     (4 * DR3_ANNIV_CARRES_SLICE)                  /* 0x5140 */

/* Build slot 6 of scenecar.bpk (96x64) from slot 5 (Deliverator). */
void dr3_anniv_build_scenecar(const uint8_t *deliv_96x64, uint8_t *out_96x64);

/* Build slot 6 of carname.bpk (96x16 "ANNIVERSARY") from slots 0..5 of carname.bpk. */
void dr3_anniv_build_carname(const uint8_t *carname_6slots, uint8_t *out_96x16);

/* Build slot 6 of carres.bpk (4x 100x52) from slot 5 (Deliverator) of carres.bpk. */
void dr3_anniv_build_carres(const uint8_t *deliv_carres, uint8_t *out_carres);

/* Tint a decoded 96x64 turntable spin frame (lotus.bpk) into the Anniversary stealth finish. */
void dr3_anniv_tint_spin(uint8_t *frame_96x64);

#ifdef __cplusplus
}
#endif

#endif /* DR3_ANNIV_GFX_H */
