#include "menubg/space_mode.h"
#include "states/game/game_core.h"
#include <string.h>

static void update_star_tiles(u8 intensity) {
    u32 tiles[16];
    u16 length = 1 + (intensity * 2);
    u16 num_tiles;

    if (length > 16) length = 16;
    num_tiles = (length > 8) ? 2 : 1;

    for (u16 t = 0; t < 3; t++) {
        u32 p = t + 1;
        u32 pattern = (p << 12);
        memset(tiles, 0, sizeof(tiles));
        for (u16 i = 0; i < length; i++) tiles[i] = pattern;
        VDP_loadTileData(tiles, TILE_STAR_BASE + (t * 2), num_tiles, CPU);
    }
}

static void draw_stars_dynamic(u8 intensity) {
    VDP_clearPlane(BG_B, TRUE);
    u16 count = 45 + (intensity * 12);

    for (u16 i = 0; i < count; i++) {
        u16 rx = random() % 64;
        u16 ry = random() % 32;
        u16 channel = i % 3;
        u16 tile_idx = TILE_STAR_BASE + (channel * 2);

        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, tile_idx), rx, ry);
        if (intensity > 3 && (intensity * 2) > 8) {
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, tile_idx + 1), rx, (ry + 1) % 32);
        }
    }
}

void menubg_space_init_tiles(u8 intensity) {
    update_star_tiles(intensity);
}

void menubg_space_apply(MenuBgState* state) {
    state->target_dx = F16_0;
    state->target_dy = STARFIELD_BASE_SP;
    state->mode.space.current_intensity = 1;
    state->mode.space.is_flashing = FALSE;
    state->mode.space.flash_level = F16_0;

    update_star_tiles(1);
    draw_stars_dynamic(1);
}

void menubg_space_update(MenuBgState* state) {
    state->curr_dx = F16_0;
    if (state->curr_dy < state->target_dy) state->curr_dy += FIX16(0.05);
    else if (state->curr_dy > state->target_dy) state->curr_dy -= FIX16(0.05);
}

void menubg_space_set_intensity(MenuBgState* state, u8 level, bool is_active) {
    fix16 speed_factor;

    if (level < 1) level = 1;
    if (level > 10) level = 10;

    if (level == 1 && state->mode.space.current_intensity > 5) {
        state->mode.space.is_flashing = TRUE;
        state->mode.space.flash_level = FIX16(1.0);
    }

    state->mode.space.current_intensity = level;
    speed_factor = (level < 7) ? FIX16(-0.2) : FIX16(-0.35);
    state->target_dy = STARFIELD_BASE_SP + F16_mul(FIX16(level - 1), speed_factor);

    update_star_tiles(level);
    draw_stars_dynamic(level);

    if (is_active && !state->mode.space.is_flashing) {
        state->fade_level = FIX16(1.0);
    }
}
