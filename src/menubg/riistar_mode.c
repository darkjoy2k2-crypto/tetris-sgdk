#include "menubg/riistar_mode.h"
#include "bg.h"
#include "states/game/game_core.h"
#include <string.h>

static const u8 riistar_group_sizes[5] = { 6, 8, 8, 4, 6 };
static const fix16 riistar_group_speeds[5] = {
    FIX16(-1.60), FIX16(-1.20), FIX16(-0.85), FIX16(-0.55), FIX16(-0.35)
};

static void reset_riistar_state(MenuBgState* state) {
    state->scroll_x = F16_0;
    state->sub_y = state->mode.riistar.vertical_center;
    state->curr_dx = F16_0;
    state->curr_dy = F16_0;

    for (u16 i = 0; i < 5; i++) state->mode.riistar.group_scroll[i] = F16_0;
    for (u16 y = 0; y < 32; y++) state->mode.riistar.map_row_scroll[y] = 0;
    for (u16 y = 0; y < 240; y++) state->mode.riistar.hscroll_lines[y] = 0;

    state->y_offset = F16_0;
    state->y_velocity = F16_0;
    state->mode.riistar.speed_scale = F16_0;
    state->mode.riistar.stack_top_row = -1;
}

static void update_riistar_scroll_lines(MenuBgState* state, s16 verticalScrollPx) {
    const u16 screenHeight = IS_PAL_SYSTEM ? 240 : 224;
    s16 row = 31;

    for (u16 g = 0; g < 5; g++) {
        for (u16 r = 0; r < riistar_group_sizes[g] && row >= 0; r++) {
            state->mode.riistar.map_row_scroll[row--] = F16_toInt(state->mode.riistar.group_scroll[g]);
        }
    }

    for (u16 line = 0; line < screenHeight; line++) {
        s16 srcPixelY = verticalScrollPx + (s16)line;
        u16 srcRow;

        if (srcPixelY < 0) srcPixelY = 0;
        srcRow = (u16)(srcPixelY >> 3);
        if (srcRow > 31) srcRow = 31;

        state->mode.riistar.hscroll_lines[line] = state->mode.riistar.map_row_scroll[srcRow];
    }

    VDP_setHorizontalScrollLine(BG_B, 0, state->mode.riistar.hscroll_lines, screenHeight, CPU);
}

void menubg_riistar_apply(MenuBgState* state) {
    u16 tileBase;
    u16 tileCount = riistar_bg_tileset.numTile;
    u16 safeUserMax;
    u16 mapW;
    u16 mapH;
    s16 maxScrollPx;
    u16 baseAttr;

    if (tileCount == 0) return;

    // Riistar relies on per-line horizontal scroll (tile-row zones) for layered speed.
    VDP_setScrollingMode(HSCROLL_LINE, VSCROLL_PLANE);

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

    mapW = riistar_bg_tilemap.w;
    mapH = riistar_bg_tilemap.h;
    if (mapW > 64) mapW = 64;
    if (mapH > 32) mapH = 32;

    maxScrollPx = (s16)(mapH << 3) - (IS_PAL_SYSTEM ? 240 : 224);
    if (maxScrollPx < 0) maxScrollPx = 0;

    state->mode.riistar.vertical_center = FIX16(maxScrollPx / 2);
    state->mode.riistar.vertical_amplitude = FIX16(maxScrollPx / 2);
    state->mode.riistar.vertical_max = FIX16(maxScrollPx);

    state->y_offset = F16_0;
    state->y_velocity = F16_0;

    baseAttr = TILE_ATTR_FULL(PAL0, 0, 0, 0, tileBase);

    if (!VDP_loadTileSet(&riistar_bg_tileset, tileBase, CPU)) return;

    VDP_setTileMapEx(BG_B, &riistar_bg_tilemap, baseAttr, 0, 0, 0, 0, mapW, mapH, CPU);

    reset_riistar_state(state);

    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, F16_toInt(state->sub_y));

    update_riistar_scroll_lines(state, F16_toInt(state->sub_y));
}

void menubg_riistar_update(MenuBgState* state) {
    for (u16 g = 0; g < 5; g++) {
        state->mode.riistar.group_scroll[g] += F16_mul(riistar_group_speeds[g], state->mode.riistar.speed_scale);
    }

    state->curr_dx = F16_0;
    state->y_velocity += F16_mul((F16_0 - state->y_offset), RIISTAR_SPRING_K);
    state->y_velocity = F16_mul(state->y_velocity, RIISTAR_DAMPING);
    state->y_offset += state->y_velocity;

    state->sub_y = state->mode.riistar.vertical_center + state->y_offset;
    if (state->sub_y < F16_0) {
        state->sub_y = F16_0;
        state->y_offset = state->sub_y - state->mode.riistar.vertical_center;
        if (state->y_velocity < F16_0) state->y_velocity = F16_0;
    } else if (state->sub_y > state->mode.riistar.vertical_max) {
        state->sub_y = state->mode.riistar.vertical_max;
        state->y_offset = state->sub_y - state->mode.riistar.vertical_center;
        if (state->y_velocity > F16_0) state->y_velocity = F16_0;
    }

    update_riistar_scroll_lines(state, F16_toInt(state->sub_y));
}

void menubg_riistar_set_stack_top(MenuBgState* state, s16 topRow) {
    s16 fillHeight;
    const s16 maxFillForFullSpeed = (BOARD_HEIGHT - 2);

    if (topRow < -1) topRow = -1;
    if (topRow >= BOARD_HEIGHT) topRow = -1;

    state->mode.riistar.stack_top_row = topRow;

    if (topRow < 0) {
        state->mode.riistar.speed_scale = F16_0;
        return;
    }

    fillHeight = (s16)(BOARD_HEIGHT - topRow);
    if (fillHeight <= 0) {
        state->mode.riistar.speed_scale = F16_0;
        return;
    }

    state->mode.riistar.speed_scale = F16_div(FIX16(fillHeight), FIX16(maxFillForFullSpeed));
    if (state->mode.riistar.speed_scale > FIX16(1.0)) state->mode.riistar.speed_scale = FIX16(1.0);
    if (state->mode.riistar.speed_scale < F16_0) state->mode.riistar.speed_scale = F16_0;
}

void menubg_riistar_pulse(MenuBgState* state, u8 pulseType, u8 bg_mode, bool is_active) {
    fix16 impulse = F16_0;

    if (bg_mode != BG_MODE_RIISTAR && bg_mode != BG_MODE_CLUB) return;
    if (!is_active) return;

    switch (pulseType) {
        case 1: impulse = RIISTAR_PULSE_SOFT; break;
        case 2: impulse = RIISTAR_PULSE_SOFTDROP; break;
        case 3: impulse = RIISTAR_PULSE_HARDDROP; break;
        default: return;
    }

    state->y_velocity += impulse;
}
