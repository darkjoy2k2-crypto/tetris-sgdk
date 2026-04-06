#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import DefaultDict

import build_guile_xgm2 as base
import build_sgdk_test_style_xgm2 as style


FrameCommands = DefaultDict[int, bytearray]


def advance_frames(sample_accum: int, wait_samples: int, current_frame: int) -> tuple[int, int]:
    sample_accum += wait_samples
    while sample_accum >= base.FRAME_SAMPLES:
        current_frame += 1
        sample_accum -= base.FRAME_SAMPLES
    return sample_accum, current_frame


def extract_beat_frames(source_vgm: Path) -> tuple[FrameCommands, int]:
    data = base.load_vgm_bytes(source_vgm)
    pos = base.vgm_data_offset(data)
    frames: FrameCommands = defaultdict(bytearray)
    current_frame = 0
    sample_accum = 0

    while pos < len(data):
        cmd_start = pos
        cmd = data[pos]
        pos += 1

        if cmd == 0x66:
            break

        if cmd == 0x67:
            if data[pos] != 0x66:
                raise ValueError("Invalid VGM data block marker")
            block_len = base.read_u32_le(data, pos + 2)
            frames[current_frame].extend(data[cmd_start:cmd_start + 7 + block_len])
            pos += 6 + block_len
            continue

        if cmd in (0x4F, 0x50):
            frames[current_frame].extend(data[cmd_start:cmd_start + 2])
            pos += 1
            continue

        if cmd in (0x52, 0x53):
            frames[current_frame].extend(data[cmd_start:cmd_start + 3])
            pos += 2
            continue

        if cmd in (0x90, 0x91, 0x95):
            frames[current_frame].extend(data[cmd_start:cmd_start + 5])
            pos += 4
            continue

        if cmd == 0x92:
            frames[current_frame].extend(data[cmd_start:cmd_start + 6])
            pos += 5
            continue

        if cmd == 0x93:
            frames[current_frame].extend(data[cmd_start:cmd_start + 11])
            pos += 10
            continue

        if cmd == 0x94:
            frames[current_frame].extend(data[cmd_start:cmd_start + 2])
            pos += 1
            continue

        if cmd == 0x61:
            wait_samples = int.from_bytes(data[pos:pos + 2], "little")
            pos += 2
            sample_accum, current_frame = advance_frames(sample_accum, wait_samples, current_frame)
            continue

        if cmd == 0x62:
            sample_accum, current_frame = advance_frames(sample_accum, base.FRAME_SAMPLES, current_frame)
            continue

        if cmd == 0x63:
            sample_accum, current_frame = advance_frames(sample_accum, 882, current_frame)
            continue

        if 0x70 <= cmd <= 0x7F:
            sample_accum, current_frame = advance_frames(sample_accum, (cmd & 0x0F) + 1, current_frame)
            continue

        if 0x80 <= cmd <= 0x8F:
            frames[current_frame].append(cmd)
            sample_accum, current_frame = advance_frames(sample_accum, cmd & 0x0F, current_frame)
            continue

        if cmd == 0x68:
            pos += 11
            continue

        if 0x51 <= cmd <= 0x5F:
            pos += 2
            continue

        if 0xA0 <= cmd <= 0xBF:
            pos += 2
            continue

        if 0xC0 <= cmd <= 0xDF:
            pos += 3
            continue

        if 0xE0 <= cmd <= 0xFF:
            pos += 4
            continue

        raise ValueError(f"Unsupported VGM command 0x{cmd:02X} at 0x{cmd_start:06X}")

    return frames, current_frame


def build_overlay_bank(source_vgm: Path) -> tuple[dict[str, base.Patch], list[base.Patch]]:
    patches = base.extract_patches(source_vgm)
    if len(patches) < 8:
        raise RuntimeError("SGDK sample did not yield enough FM patches")

    bank = {
        "bass": style.shape_patch(style.pick_patch(patches, 5), "sgdk_mix_bass", total_level_drop=2, feedback_boost=1),
        "main_lead": style.shape_patch(style.pick_patch(patches, 17), "sgdk_mix_main", total_level_drop=8, feedback_boost=1),
        "second_lead": style.shape_patch(style.pick_patch(patches, 10), "sgdk_mix_second", total_level_drop=-3, feedback_boost=0),
        "support": style.shape_patch(style.pick_patch(patches, 4), "sgdk_mix_support", total_level_drop=-4, feedback_boost=0),
    }
    return bank, patches


def render_mix_vgm(
    beat_frames: FrameCommands,
    note_events: list[base.MidiEvent],
    patch_map: dict[tuple[int, int], base.Patch],
    out_vgm: Path,
    loop_start_frame: int | None = None,
) -> dict[str, object]:
    melodic_events = [event for event in note_events if event.channel != 9 and (event.track, event.channel) in patch_map]
    if not melodic_events:
        raise ValueError("No melodic MIDI events found")

    group_counts = Counter((event.track, event.channel) for event in melodic_events if event.kind == "note_on")
    events_by_frame: DefaultDict[int, list[base.MidiEvent]] = defaultdict(list)
    max_frame = max(beat_frames.keys(), default=0)

    for event in melodic_events:
        frame = max(0, int(round(event.time_sec * base.NTSC_RATE)))
        events_by_frame[frame].append(event)
        if frame > max_frame:
            max_frame = frame

    writer = base.VgmWriter()
    writer.ym_write_global(0x22, 0x00)
    writer.ym_write_global(0x27, 0x00)
    writer.ym_write_global(0x2B, 0x00)
    for channel in range(6):
        writer.ym_write(channel, 0xB4, 0xC0)
        base.key_off(writer, channel)

    voices = [base.VoiceState() for _ in range(6)]
    note_map: DefaultDict[tuple[tuple[int, int], int], list[int]] = defaultdict(list)

    current_frame = 0
    while current_frame <= max_frame:
        if loop_start_frame is not None and current_frame == loop_start_frame:
            writer.set_loop_start()

        beat_bytes = beat_frames.get(current_frame)
        if beat_bytes:
            writer.commands.extend(beat_bytes)

        frame_events = events_by_frame.get(current_frame, [])
        frame_events.sort(key=lambda event: 0 if event.kind == "note_off" else 1)

        for event in frame_events:
            source = (event.track, event.channel)
            key = (source, event.note)

            if event.kind == "note_off":
                stack = note_map.get(key)
                if stack:
                    base.release_voice(writer, voices, note_map, stack[-1])
                continue

            free_voice = next((index for index, voice in enumerate(voices) if voice.note is None), None)
            if free_voice is None:
                free_voice = min(range(6), key=lambda index: voices[index].started_frame)
                base.release_voice(writer, voices, note_map, free_voice)

            patch = patch_map[source]
            voice = voices[free_voice]
            if voice.patch_id != patch.id:
                base.apply_patch(writer, free_voice, patch)
                voice.patch_id = patch.id

            base.key_on(writer, free_voice, event.note)
            voice.note = event.note
            voice.source = source
            voice.started_frame = current_frame
            note_map[key].append(free_voice)

        current_frame += 1
        writer.wait_frames(1)

    for voice_index in range(6):
        base.release_voice(writer, voices, note_map, voice_index)

    out_vgm.write_bytes(writer.finish())
    return {
        "frames": max_frame + 1,
        "groups": [
            {
                "track": track,
                "midi_channel": channel,
                "note_count": count,
                "patch": patch_map[(track, channel)].name,
            }
            for (track, channel), count in group_counts.most_common()
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Combine the extracted SGDK beat with the Tetris MIDI as a second XGM2 music track.")
    parser.add_argument("--beat-vgm", default="py/sound/out/sgdk_test_beat_only.vgm", help="Beat-only VGM used as the exact rhythm foundation")
    parser.add_argument("--source-vgm", default="res/music/sgdk_drum_run_rave.vgm", help="Original SGDK sample VGM for FM patch extraction")
    parser.add_argument("--midi", default="py/sound/midi/tetris.mid", help="Tetris MIDI arrangement")
    parser.add_argument("--out-dir", default="py/sound/out", help="Output directory")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    beat_vgm = (root / args.beat_vgm).resolve()
    source_vgm = (root / args.source_vgm).resolve()
    midi_path = (root / args.midi).resolve()
    out_dir = (root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not beat_vgm.exists():
        raise FileNotFoundError(beat_vgm)
    if not source_vgm.exists():
        raise FileNotFoundError(source_vgm)
    if not midi_path.exists():
        raise FileNotFoundError(midi_path)

    beat_frames, beat_frame_count = extract_beat_frames(beat_vgm)
    _, note_events = base.parse_midi(midi_path)
    roles = style.select_roles(style.analyze_groups(note_events))
    patch_bank, extracted_patches = build_overlay_bank(source_vgm)

    patch_map: dict[tuple[int, int], base.Patch] = {}
    for source in roles["bass_sources"]:
        patch_map[source] = patch_bank["bass"]
    patch_map[roles["primary_lead"]] = patch_bank["main_lead"]
    patch_map[roles["secondary_lead"]] = patch_bank["second_lead"]
    for source in roles["support_sources"]:
        patch_map[source] = patch_bank["support"]

    adjusted_events = style.prepare_note_events(note_events, roles)
    loop_start_frame = 206
    vgm_output_path = out_dir / "tetris_sgdk_beat_mix.vgm"
    patch_report_path = out_dir / "sgdk_beat_mix_patches.json"
    base.save_patch_report(list(patch_bank.values()) + extracted_patches, patch_report_path)
    render_info = render_mix_vgm(beat_frames, adjusted_events, patch_map, vgm_output_path, loop_start_frame=loop_start_frame)
    xgm_output_path, converter_log = base.convert_to_xgm(vgm_output_path, out_dir)

    summary = {
        "style": "sgdk_beat_plus_tetris",
        "source_beat_vgm": str(beat_vgm),
        "source_vgm": str(source_vgm),
        "source_midi": str(midi_path),
        "beat_frames": beat_frame_count,
        "loop_start_frame": loop_start_frame,
        "loop_start_seconds": round(loop_start_frame / base.NTSC_RATE, 2),
        "vgm_output": str(vgm_output_path),
        "xgm_output": str(xgm_output_path),
        "render": render_info,
        "converter_log": converter_log,
    }
    (out_dir / "build_summary_sgdk_beat_mix.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"Rendered SGDK beat+tetris VGM: {vgm_output_path}")
    print(f"Generated SGDK beat+tetris XGM output: {xgm_output_path}")
    print(f"Patch report: {patch_report_path}")
    print("Converter log:")
    print(converter_log)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
