#include "drally.h"
#include "drmath.h"
#include <string.h>
#if defined(DR3_CHAOS)
#include "platform_3ds/dr3_chaos.h"
#include "platform_3ds/dr3_log.h"
#endif

    extern __BYTE__ * BACKBUFFER;
    extern int CURRENT_VIEWPORT_X;

#define OFFSET 400

static int ___5a86ch[1000];
static int ___5b80ch[1000];
static int ___5c7ach[1000];
static int ___5d74ch[1000];

static int helper_bounds_y(int * PminY, int * PmaxY){

	if((*PminY < 0x00)&&(*PmaxY <= (*PminY = 0x00))) return 0;
	if((*PmaxY > DR3_RACE_H)&&(*PminY >= (*PmaxY = DR3_RACE_H))) return 0;

	return !(*PminY == *PmaxY);
}

static void helper1_fp(int A0, int LoY_XZ, int LoY, int HiY_XZ, int HiY, float * dst){

    float       vX, pX;
    int         n, vY, pY;


    pX = (float)LoY_XZ;
    pY = LoY;
    vX = (float)HiY_XZ-pX;
	vY = HiY-pY;

	if(vY != 0){

        n = -1;
		while(++n < (vY+A0)) dst[pY+OFFSET+n] = pX+(float)n*vX/(float)vY+0.5f;
	}
}

static void helper1(int A0, int LoY_XZ, int LoY, int HiY_XZ, int HiY, int * A5){

	int 	    eax, ecx;
    int         n, vX, vY, pX, pY;


    pX = LoY_XZ;
    pY = LoY;
    vX = HiY_XZ-pX;
	vY = HiY-pY;

	if(vY != 0){

		eax = (vX<<0x10)/vY;
		ecx = (pX<<0x10)+0x8000;

        n = -1;
		while(++n < (vY+A0)) A5[pY+OFFSET+n] = ecx+n*eax;
	}
}

static void helper_order_y(int * X1, int * Y1, int * Z1, int * X2, int * Y2, int * Z2, int * X3, int * Y3, int * Z3){

    int     i, j, args[9];

	if(Z3) args[0] = *Z3;
	args[1] = *Y3;
	args[2] = *X3;
	if(Z2) args[3] = *Z2;
	args[4] = *Y2;
	args[5] = *X2;
    if(Z1) args[6] = *Z1;
    args[7] = *Y1;
    args[8] = *X1;

    i = (*Y1 < *Y2) ? !(*Y2 < *Y3) : 2*!(*Y1 < *Y3);
    j = (*Y1 < *Y2) ? 2*(*Y1 < *Y3) : (*Y2 < *Y3);

   *X1 = args[3*j+2];
   *Y1 = args[3*j+1];
   if(Z1) *Z1 = args[3*j];
   *X2 = args[3*(3-(i+j))+2];
   *Y2 = args[3*(3-(i+j))+1];
   if(Z2) *Z2 = args[3*(3-(i+j))];
   *X3 = args[3*i+2];
   *Y3 = args[3*i+1];
   if(Z3) *Z3 = args[3*i];
}

// Trees
void ___5e3e8h(int Z3, int Y3, int X3, int Z2, int Y2, int X2, int Z1, int Y1, int X1){

    int     eax, edx, TREE_XL, TREE_XR;

    helper_order_y(&X1, &Y1, &Z1, &X2, &Y2, &Z2, &X3, &Y3, &Z3);

    helper1(0, X1, Y1, X2, Y2, ___5a86ch);
    helper1(0, X2, Y2, X3, Y3, ___5a86ch);
    helper1(0, X1, Y1, X3, Y3, ___5b80ch);
    helper1(1, Z1, Y1, Z2, Y2, ___5c7ach);
    helper1(1, Z2, Y2, Z3, Y3, ___5c7ach);
    helper1(1, Z1, Y1, Z3, Y3, ___5d74ch);

    if(helper_bounds_y(&Y1, &Y3)){

        int* LEFT = &___5b80ch[OFFSET];
        int* RIGHT = &___5a86ch[OFFSET];
        int* BOTTOM = &___5c7ach[OFFSET];
        int* TOP = &___5d74ch[OFFSET];
        unsigned char* dest = &BACKBUFFER[0x60];

        edx = (Y1 + Y3) / 2;
        eax = (LEFT[edx] - RIGHT[edx]) >> 0x10;
        eax = (eax != 0) ? (TOP[edx] - BOTTOM[edx]) / eax : 0;

#if defined(DR3_CHAOS)
        if (dr3_log_enabled()) {
            while (Y1 < Y3) {

                TREE_XL = dRMath_min_i(LEFT[Y1], RIGHT[Y1]) >> 0x10;
                TREE_XL = dRMath_max_i(TREE_XL, CURRENT_VIEWPORT_X);
                TREE_XR = dRMath_max_i(LEFT[Y1], RIGHT[Y1]) >> 0x10;
                TREE_XR = dRMath_min_i(TREE_XR, DR3_RACE_W);
                edx = (LEFT[Y1] < RIGHT[Y1]) ? TOP[Y1] : BOTTOM[Y1];

                while (TREE_XL < TREE_XR) {

                    dest[0x200 * Y1 + TREE_XL] = edx >> 0x10;
                    dr3_chaos_car_mask[0x60 + 0x200 * Y1 + TREE_XL] = 0;
                    dr3_chaos_watch(0, "tree", TREE_XL, Y1, edx >> 0x10);
                    TREE_XL++;
                    edx += eax;
                }

                Y1++;
            }
        }
        else
#endif
        {
            while (Y1 < Y3) {

                const int l = LEFT[Y1], r = RIGHT[Y1];

                TREE_XL = dRMath_min_i(l, r) >> 0x10;
                if (TREE_XL < CURRENT_VIEWPORT_X) TREE_XL = CURRENT_VIEWPORT_X;
                TREE_XR = dRMath_max_i(l, r) >> 0x10;
                if (TREE_XR > DR3_RACE_W) TREE_XR = DR3_RACE_W;

                if (TREE_XL < TREE_XR) {
                    unsigned char *p = dest + 0x200 * Y1 + TREE_XL;
                    unsigned char *e = dest + 0x200 * Y1 + TREE_XR;

                    edx = (l < r) ? TOP[Y1] : BOTTOM[Y1];
                    while (p < e) {
                        *p++ = (unsigned char)(edx >> 0x10);
                        edx += eax;
                    }
#if defined(DR3_CHAOS)
                    memset(dr3_chaos_car_mask + 0x60 + 0x200 * Y1 + TREE_XL, 0, (size_t)(TREE_XR - TREE_XL));
#endif
                }

                Y1++;
            }
        }
    }
}

// Objects
void ___5e137h(__BYTE__ A0, int Y3, int X3, int Y2, int X2, int Y1, int X1){

    int     OBJ_XL, OBJ_XR;

    helper_order_y(&X1, &Y1, NULL, &X2, &Y2, NULL, &X3, &Y3, NULL);

    helper1(0, X1, Y1, X2, Y2, ___5a86ch);
    helper1(0, X2, Y2, X3, Y3, ___5a86ch);
    helper1(0, X1, Y1, X3, Y3, ___5b80ch);

    if(helper_bounds_y(&Y1, &Y3)){

        int* LEFT = &___5b80ch[OFFSET];
        int* RIGHT = &___5a86ch[OFFSET];
        unsigned char* dest = &BACKBUFFER[0x60];

#if defined(DR3_CHAOS)
        if (dr3_log_enabled()) {
            while (Y1 < Y3) {

                OBJ_XL = dRMath_min_i(LEFT[Y1], RIGHT[Y1]) >> 0x10;
                OBJ_XL = dRMath_max_i(OBJ_XL, CURRENT_VIEWPORT_X);
                OBJ_XR = dRMath_max_i(LEFT[Y1], RIGHT[Y1]) >> 0x10;
                OBJ_XR = dRMath_min_i(OBJ_XR, DR3_RACE_W);

                while (OBJ_XL < OBJ_XR) {

                    dest[0x200 * Y1 + OBJ_XL] = A0;
                    dr3_chaos_car_mask[0x60 + 0x200 * Y1 + OBJ_XL] = 0;
                    dr3_chaos_watch(1, "object fill", OBJ_XL, Y1, A0);
                    OBJ_XL++;
                }

                Y1++;
            }
        }
        else
#endif
        {
            while (Y1 < Y3) {

                const int l = LEFT[Y1], r = RIGHT[Y1];

                OBJ_XL = dRMath_min_i(l, r) >> 0x10;
                if (OBJ_XL < CURRENT_VIEWPORT_X) OBJ_XL = CURRENT_VIEWPORT_X;
                OBJ_XR = dRMath_max_i(l, r) >> 0x10;
                if (OBJ_XR > DR3_RACE_W) OBJ_XR = DR3_RACE_W;

                if (OBJ_XL < OBJ_XR) {
                    memset(dest + 0x200 * Y1 + OBJ_XL, A0, (size_t)(OBJ_XR - OBJ_XL));
#if defined(DR3_CHAOS)
                    memset(dr3_chaos_car_mask + 0x60 + 0x200 * Y1 + OBJ_XL, 0, (size_t)(OBJ_XR - OBJ_XL));
#endif
                }

                Y1++;
            }
        }
    }
}

// Shadows
void ___5e769h(__BYTE__ * A0, int Y3, int X3, int Y2, int X2, int Y1, int X1){

    int     SHD_XL, SHD_XR;
//    int     * LEFT, * RIGHT;

    helper_order_y(&X1, &Y1, NULL, &X2, &Y2, NULL, &X3, &Y3, NULL);

    helper1(0, X1, Y1, X2, Y2, ___5a86ch);
	helper1(0, X2, Y2, X3, Y3, ___5a86ch);
	helper1(0, X1, Y1, X3, Y3, ___5b80ch);

//    if((X2-X1)*(Y3-Y1)<(X3-X1)*(Y2-Y1)){
//
//        LEFT = ___5a86ch;
//        RIGHT = ___5b80ch;
//    }
//    else {
//
//        LEFT = ___5b80ch;
//        RIGHT = ___5a86ch;
//    }

	if(helper_bounds_y(&Y1, &Y3)){

        int* LEFT = &___5b80ch[OFFSET];
        int* RIGHT = &___5a86ch[OFFSET];
        unsigned char* dest = &BACKBUFFER[0x60];

#if defined(DR3_CHAOS)
        if (dr3_log_enabled()) {
            while (Y1 < Y3) {

                SHD_XL = dRMath_min_i(LEFT[Y1], RIGHT[Y1]) >> 0x10;
                SHD_XL = dRMath_max_i(SHD_XL, CURRENT_VIEWPORT_X);
                SHD_XR = dRMath_max_i(LEFT[Y1], RIGHT[Y1]) >> 0x10;
                SHD_XR = dRMath_min_i(SHD_XR, DR3_RACE_W);

                while (SHD_XL < SHD_XR) {

                    dest[0x200 * Y1 + SHD_XL] = A0[dest[0x200 * Y1 + SHD_XL]];
                    /* the caller named itself (dr3_chaos_remap_who) - this filler has three of them */
                    dr3_chaos_watch(2, dr3_chaos_remap_who_get(), SHD_XL, Y1, dest[0x200 * Y1 + SHD_XL]);
                    SHD_XL++;
                }

                Y1++;
            }
        }
        else
#endif
        {
            while (Y1 < Y3) {

                const int l = LEFT[Y1], r = RIGHT[Y1];

                SHD_XL = dRMath_min_i(l, r) >> 0x10;
                if (SHD_XL < CURRENT_VIEWPORT_X) SHD_XL = CURRENT_VIEWPORT_X;
                SHD_XR = dRMath_max_i(l, r) >> 0x10;
                if (SHD_XR > DR3_RACE_W) SHD_XR = DR3_RACE_W;

                if (SHD_XL < SHD_XR) {
                    unsigned char *p = dest + 0x200 * Y1 + SHD_XL;
                    unsigned char *e = dest + 0x200 * Y1 + SHD_XR;

                    while (p < e) {
                        *p = A0[*p];
                        ++p;
                    }
                }

                Y1++;
            }
        }
	}
}
