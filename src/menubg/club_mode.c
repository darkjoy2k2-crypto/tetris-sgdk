#include "menubg/club_mode.h"
#include "bg.h"
#include "states/states.h"

static s16 hscrollClearLines[240];

static void draw_club_debug_info(const MenuBgState* state) {
    char text[40];
    const u16 screenHeight = IS_PAL_SYSTEM ? 30 : 28;
    const u16 y = screenHeight - 1;

    if (!GET_FLAG(config.flags, FLAG_DEBUG)) {
        VDP_drawTextBG(BG_A, "                    ", 0, y);
        return;
    }

    sprintf(text, "X:%d W:%d MW:%ld", state->mode.club.debug_scroll_px, club_bg_map.w, ((s32)club_bg_map.w << 7));
    VDP_drawTextBG(BG_A, text, 0, y);
}

static void reset_club_state(MenuBgState* state) {
    state->mode.club.pos_x = FIX32(0);
    state->mode.club.scroll_diff = FIX32(0.5);
    state->mode.club.debug_scroll_px = 0;
    state->y_offset = F16_0;
    state->y_velocity = F16_0;
}

void menubg_club_cleanup(MenuBgState* state) {
    if (state->mode.club.map != NULL) {
        MAP_release(state->mode.club.map);
        state->mode.club.map = NULL;
    }
}

void menubg_club_apply(MenuBgState* state) {
    u16 tileBase;
    u16 tileCount = club_bg_tileset.numTile;
    u16 safeUserMax;
    u16 baseAttr;
    s16 maxScrollY;
    s16 mapY;

    if (tileCount == 0) return;

    SYS_setHIntCallback(NULL);
    VDP_setHInterrupt(0);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    safeUserMax = TILE_USER_MAX_INDEX;
    {
        u16 spriteReservedMax = (u16)(TILE_FONT_INDEX - SPRITE_VRAM_RESERVE_TILES - 1);
        if (spriteReservedMax < safeUserMax) safeUserMax = spriteReservedMax;
    }

    if ((u16)(tileCount + 16) >= safeUserMax) {
        tileBase = TILE_USER_INDEX;
    } else {
        tileBase = safeUserMax - tileCount - 16;
        if (tileBase < TILE_USER_INDEX) tileBase = TILE_USER_INDEX;
    }

    VDP_clearPlane(BG_B, TRUE);

    state->y_offset = F16_0;
    state->y_velocity = F16_0;

    if (!VDP_loadTileSet(&club_bg_tileset, tileBase, CPU)) return;

    SYS_doVBlankProcess();

    PAL_setPalette(PAL0, club_bg_palette.data, CPU);

    baseAttr = TILE_ATTR_FULL(PAL0, 0, 0, 0, tileBase);

    menubg_club_cleanup(state);
    state->mode.club.map = MAP_create(&club_bg_map, BG_B, baseAttr);

    maxScrollY = (s16)(club_bg_map.h << 7) - (IS_PAL_SYSTEM ? 240 : 224);
    if (maxScrollY < 0) maxScrollY = 0;

    reset_club_state(state);
    state->mode.club.vertical_offset = FIX16(CLUB_SCROLL_Y_OFFSET);
    state->mode.club.vertical_center = FIX16(maxScrollY / 2) + state->mode.club.vertical_offset;
    state->mode.club.vertical_max = FIX16(maxScrollY);

    if (state->mode.club.map != NULL) {
        mapY = F16_toInt(state->mode.club.vertical_center);
        if (mapY < 0) mapY = 0;
        if (mapY > maxScrollY) mapY = maxScrollY;
        MAP_scrollTo(state->mode.club.map, 0, (u32)mapY);
        SYS_doVBlankProcess();
    }

    state->scroll_x = F16_0;
    state->sub_y = state->mode.club.vertical_center;
    state->curr_dx = F16_0;
    state->curr_dy = F16_0;

    {
        const u16 screenHeight = IS_PAL_SYSTEM ? 240 : 224;
        VDP_setHorizontalScrollLine(BG_B, 0, hscrollClearLines, screenHeight, CPU);
    }

    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, 0);

    draw_club_debug_info(state);
}

void menubg_club_update(MenuBgState* state) {
    state->mode.club.pos_x += state->mode.club.scroll_diff;
    state->mode.club.debug_scroll_px = F32_toInt(state->mode.club.pos_x);

    if (state->mode.club.pos_x >= FIX32(1216) || state->mode.club.pos_x < FIX32(0)) {
        state->mode.club.scroll_diff = -state->mode.club.scroll_diff;
    }

    state->curr_dx = F16_0;
    state->curr_dy = F16_0;

    state->y_velocity += F16_mul((F16_0 - state->y_offset), RIISTAR_SPRING_K);
    state->y_velocity = F16_mul(state->y_velocity, RIISTAR_DAMPING);
    state->y_offset += state->y_velocity;
    state->sub_y = state->mode.club.vertical_center + state->y_offset;

    {
        s16 mapYPx = F16_toInt(state->sub_y);
        if (state->mode.club.map != NULL) {
            MAP_scrollTo(state->mode.club.map, F32_toInt(state->mode.club.pos_x), (u32)mapYPx);
        }
        VDP_setVerticalScroll(BG_B, mapYPx);
    }

    draw_club_debug_info(state);
}
