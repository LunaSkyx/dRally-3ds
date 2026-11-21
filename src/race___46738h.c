#include "drally.h"

	extern __BYTE__ * VGA13_ACTIVESCREEN;

void __VGA13_PRESENTSCREEN__(void);
int dr3_vga13_active_bytes(void);

void race___46738h__VGA13_PRESENTCLEARSCREEN(void){

	memset(VGA13_ACTIVESCREEN, 0, (size_t)dr3_vga13_active_bytes());
	__VGA13_PRESENTSCREEN__();
}
