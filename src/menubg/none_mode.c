#include <genesis.h>

void menubg_none_apply(void) {
    VDP_clearPlane(BG_B, TRUE);
    PAL_setColor(0, 0x0000);
}
