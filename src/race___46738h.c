#include "drally.h"
#if defined(DR3_CHAOS)
#include "platform_3ds/dr3_chaos.h"
#endif

	extern __BYTE__ * VGA13_ACTIVESCREEN;

void __VGA13_PRESENTSCREEN__(void);
int dr3_vga13_active_bytes(void);

void race___46738h__VGA13_PRESENTCLEARSCREEN(void){

	memset(VGA13_ACTIVESCREEN, 0, (size_t)dr3_vga13_active_bytes());
#if defined(DR3_CHAOS)
	memset(dr3_chaos_car_screen, 0, sizeof(dr3_chaos_car_screen));
#endif
	__VGA13_PRESENTSCREEN__();
}
