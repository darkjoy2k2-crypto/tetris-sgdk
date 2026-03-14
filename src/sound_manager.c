#include "sound_manager.h"
#include "sounds.h"
#include "states/states.h" // WICHTIG: Für den Zugriff auf config.flags

typedef struct SoundEntry {
    const u8* data;
    u32 size;
} SoundEntry;

// Master-Array für alle 99 Sounds (WAV_001 bis WAV_099)
static const SoundEntry sfx_bank[100] = {
    { NULL, 0 },
    { WAV_001, sizeof(WAV_001) }, { WAV_002, sizeof(WAV_002) }, { WAV_003, sizeof(WAV_003) },
    { WAV_004, sizeof(WAV_004) }, { WAV_005, sizeof(WAV_005) }, { WAV_006, sizeof(WAV_006) },
    { WAV_007, sizeof(WAV_007) }, { WAV_008, sizeof(WAV_008) }, { WAV_009, sizeof(WAV_009) },
    { WAV_010, sizeof(WAV_010) }, { WAV_011, sizeof(WAV_011) }, { WAV_012, sizeof(WAV_012) },
    { WAV_013, sizeof(WAV_013) }, { WAV_014, sizeof(WAV_014) }, { WAV_015, sizeof(WAV_015) },
    { WAV_016, sizeof(WAV_016) }, { WAV_017, sizeof(WAV_017) }, { WAV_018, sizeof(WAV_018) },
    { WAV_019, sizeof(WAV_019) }, { WAV_020, sizeof(WAV_020) }, { WAV_021, sizeof(WAV_021) },
    { WAV_022, sizeof(WAV_022) }, { WAV_023, sizeof(WAV_023) }, { WAV_024, sizeof(WAV_024) },
    { WAV_025, sizeof(WAV_025) }, { WAV_026, sizeof(WAV_026) }, { WAV_027, sizeof(WAV_027) },
    { WAV_028, sizeof(WAV_028) }, { WAV_029, sizeof(WAV_029) }, { WAV_030, sizeof(WAV_030) },
    { WAV_031, sizeof(WAV_031) }, { WAV_032, sizeof(WAV_032) }, { WAV_033, sizeof(WAV_033) },
    { WAV_034, sizeof(WAV_034) }, { WAV_035, sizeof(WAV_035) }, { WAV_036, sizeof(WAV_036) },
    { WAV_037, sizeof(WAV_037) }, { WAV_038, sizeof(WAV_038) }, { WAV_039, sizeof(WAV_039) },
    { WAV_040, sizeof(WAV_040) }, { WAV_041, sizeof(WAV_041) }, { WAV_042, sizeof(WAV_042) },
    { WAV_043, sizeof(WAV_043) }, { WAV_044, sizeof(WAV_044) }, { WAV_045, sizeof(WAV_045) },
    { WAV_046, sizeof(WAV_046) }, { WAV_047, sizeof(WAV_047) }, { WAV_048, sizeof(WAV_048) },
    { WAV_049, sizeof(WAV_049) }, { WAV_050, sizeof(WAV_050) }, { WAV_051, sizeof(WAV_051) },
    { WAV_052, sizeof(WAV_052) }, { WAV_053, sizeof(WAV_053) }, { WAV_054, sizeof(WAV_054) },
    { WAV_055, sizeof(WAV_055) }, { WAV_056, sizeof(WAV_056) }, { WAV_057, sizeof(WAV_057) },
    { WAV_058, sizeof(WAV_058) }, { WAV_059, sizeof(WAV_059) }, { WAV_060, sizeof(WAV_060) },
    { WAV_061, sizeof(WAV_061) }, { WAV_062, sizeof(WAV_062) }, { WAV_063, sizeof(WAV_063) },
    { WAV_064, sizeof(WAV_064) }, { WAV_065, sizeof(WAV_065) }, { WAV_066, sizeof(WAV_066) },
    { WAV_067, sizeof(WAV_067) }, { WAV_068, sizeof(WAV_068) }, { WAV_069, sizeof(WAV_069) },
    { WAV_070, sizeof(WAV_070) }, { WAV_071, sizeof(WAV_071) }, { WAV_072, sizeof(WAV_072) },
    { WAV_073, sizeof(WAV_073) }, { WAV_074, sizeof(WAV_074) }, { WAV_075, sizeof(WAV_075) },
    { WAV_076, sizeof(WAV_076) }, { WAV_077, sizeof(WAV_077) }, { WAV_078, sizeof(WAV_078) },
    { WAV_079, sizeof(WAV_079) }, { WAV_080, sizeof(WAV_080) }, { WAV_081, sizeof(WAV_081) },
    { WAV_082, sizeof(WAV_082) }, { WAV_083, sizeof(WAV_083) }, { WAV_084, sizeof(WAV_084) },
    { WAV_085, sizeof(WAV_085) }, { WAV_086, sizeof(WAV_086) }, { WAV_087, sizeof(WAV_087) },
    { WAV_088, sizeof(WAV_088) }, { WAV_089, sizeof(WAV_089) }, { WAV_090, sizeof(WAV_090) },
    { WAV_091, sizeof(WAV_091) }, { WAV_092, sizeof(WAV_092) }, { WAV_093, sizeof(WAV_093) },
    { WAV_094, sizeof(WAV_094) }, { WAV_095, sizeof(WAV_095) }, { WAV_096, sizeof(WAV_096) },
    { WAV_097, sizeof(WAV_097) }, { WAV_098, sizeof(WAV_098) }, { WAV_099, sizeof(WAV_099) }
};

void SOUND_init() {
    Z80_loadDriver(Z80_DRIVER_XGM2, 0);
}

static void play(u16 id, SoundPCMChannel channel) {
    // Flag-Check für Sound-Effekte
    if (!GET_FLAG(config.flags, FLAG_SOUND)) return;

    if (id > 0 && id < 100 && sfx_bank[id].data != NULL) {
        XGM2_playPCM(sfx_bank[id].data, sfx_bank[id].size, channel);
    }
}

void SOUND_play(SoundEvent event) {
    play((u16)event, SOUND_PCM_CH_AUTO);
}

void SOUND_playMusic() {
    // Flag-Check für Musik
    if (!GET_FLAG(config.flags, FLAG_MUSIC)) return;

    // XGM2_play(&track1);
}

void SOUND_stopMusic() {
    XGM2_stop();
}