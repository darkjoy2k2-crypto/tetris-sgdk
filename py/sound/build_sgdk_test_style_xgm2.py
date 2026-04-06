#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import DefaultDict

import build_guile_xgm2 as base


@dataclass
class GroupProfile:
    source: tuple[int, int]
    note_count: int
    min_note: int
    max_note: int
    avg_note: float


STOMP_BEAT_SOURCE = (-30, 10)
VOCAL_CHOP_SOURCE = (-31, 11)
INTRO_PAD_SOURCE = (-32, 12)


def shape_patch(patch: base.Patch, name: str, total_level_drop: int = 0, feedback_boost: int = 0) -> base.Patch:
    regs = dict(patch.regs)

    for register in (0x40, 0x44, 0x48, 0x4C):
        regs[register] = max(0, min(127, regs.get(register, 0) - total_level_drop))

    b0 = regs.get(0xB0, 0)
    algorithm = b0 & 0x07
    feedback = min(7, ((b0 >> 3) & 0x07) + feedback_boost)
    regs[0xB0] = (feedback << 3) | algorithm
    regs[0xB4] = 0xC0

    unique_id = ((patch.id & 0xFF) << 8) | (sum(ord(ch) for ch in name) & 0xFF)
    return base.Patch(id=unique_id, name=name, regs=regs, first_channel=patch.first_channel)


def pick_patch(patches: list[base.Patch], index: int) -> base.Patch:
    if not patches:
        raise RuntimeError("Patch bank is empty")
    return patches[index % len(patches)]


def analyze_groups(note_events: list[base.MidiEvent]) -> list[GroupProfile]:
    grouped: DefaultDict[tuple[int, int], list[int]] = defaultdict(list)
    for event in note_events:
        if event.kind == "note_on" and event.channel != 9:
            grouped[(event.track, event.channel)].append(event.note)

    profiles = [
        GroupProfile(
            source=source,
            note_count=len(notes),
            min_note=min(notes),
            max_note=max(notes),
            avg_note=sum(notes) / len(notes),
        )
        for source, notes in grouped.items()
    ]
    profiles.sort(key=lambda profile: (profile.avg_note, -profile.note_count))
    return profiles


def select_roles(profiles: list[GroupProfile]) -> dict[str, object]:
    if len(profiles) < 4:
        raise RuntimeError("Not enough MIDI groups to assign SGDK-style roles")

    bass_sources = [profile.source for profile in profiles[:2]]
    melodic_profiles = sorted(
        [profile for profile in profiles if profile.source not in bass_sources],
        key=lambda profile: (-profile.note_count, -profile.max_note, profile.avg_note),
    )

    primary_lead = melodic_profiles[0].source
    secondary_lead = melodic_profiles[1].source
    support_sources = [profile.source for profile in melodic_profiles[2:]]

    return {
        "bass_sources": bass_sources,
        "primary_lead": primary_lead,
        "secondary_lead": secondary_lead,
        "support_sources": support_sources,
        "profiles": profiles,
    }


def build_patch_bank(source_vgm: Path) -> tuple[dict[str, base.Patch], list[base.Patch]]:
    patches = base.extract_patches(source_vgm)
    if len(patches) < 8:
        raise RuntimeError("SGDK sample did not yield enough FM patches")

    bank = {
        "bass": shape_patch(pick_patch(patches, 5), "sgdk_bass_drive", total_level_drop=3, feedback_boost=1),
        "main_lead": shape_patch(pick_patch(patches, 17), "sgdk_bright_lead", total_level_drop=6, feedback_boost=1),
        "second_lead": shape_patch(pick_patch(patches, 10), "sgdk_synth_second", total_level_drop=-5, feedback_boost=0),
        "support": shape_patch(pick_patch(patches, 4), "sgdk_soft_pad", total_level_drop=-4, feedback_boost=0),
        "sparkle": shape_patch(pick_patch(patches, 13), "sgdk_pluck_support", total_level_drop=-2, feedback_boost=1),
        "beat": shape_patch(pick_patch(patches, 7), "sgdk_stomp_beat", total_level_drop=1, feedback_boost=1),
        "vocal": shape_patch(pick_patch(patches, 15), "sgdk_vocal_chop", total_level_drop=-1, feedback_boost=1),
        "intro_pad": shape_patch(pick_patch(patches, 11), "sgdk_intro_pad", total_level_drop=-2, feedback_boost=0),
    }
    return bank, patches


def prepare_note_events(note_events: list[base.MidiEvent], roles: dict[str, object]) -> list[base.MidiEvent]:
    primary_lead = roles["primary_lead"]
    secondary_lead = roles["secondary_lead"]
    bass_sources = set(roles["bass_sources"])
    support_sources = set(roles["support_sources"])
    adjusted: list[base.MidiEvent] = []

    for event in note_events:
        if event.channel == 9:
            continue

        source = (event.track, event.channel)
        note = event.note

        if source in bass_sources and note >= 60:
            note -= 12
        elif source == primary_lead:
            if note >= 86:
                note -= 12
        elif source == secondary_lead:
            if note >= 74:
                note -= 12
            if event.time_sec < 3.45 and note >= 67:
                note -= 12
        elif source in support_sources:
            if event.time_sec < 3.45 and note >= 69:
                note -= 12
            elif note >= 72:
                note -= 12

        adjusted.append(
            base.MidiEvent(
                tick=event.tick,
                time_sec=event.time_sec,
                kind=event.kind,
                note=max(24, min(95, note)),
                velocity=event.velocity,
                channel=event.channel,
                track=event.track,
            )
        )

    return adjusted


def generate_intro_pad_events(loop_start_frame: int) -> list[base.MidiEvent]:
    chords = [
        (0, (57, 60, 64), 44),
        (52, (55, 59, 62), 44),
        (104, (53, 57, 60), 44),
        (156, (55, 59, 62), 40),
    ]
    events: list[base.MidiEvent] = []

    for start_frame, notes, duration in chords:
        end_frame = min(loop_start_frame - 4, start_frame + duration)
        for note in notes:
            events.append(base.MidiEvent(start_frame * 8, start_frame / base.NTSC_RATE, "note_on", note, 92, INTRO_PAD_SOURCE[1], INTRO_PAD_SOURCE[0]))
            events.append(base.MidiEvent(end_frame * 8, end_frame / base.NTSC_RATE, "note_off", note, 0, INTRO_PAD_SOURCE[1], INTRO_PAD_SOURCE[0]))

    return events


def generate_stomp_events(note_events: list[base.MidiEvent], loop_start_frame: int) -> list[base.MidiEvent]:
    max_frame = 0
    for event in note_events:
        frame = max(0, int(round(event.time_sec * base.NTSC_RATE)))
        if frame > max_frame:
            max_frame = frame

    rhythm_events: list[base.MidiEvent] = []
    step_frames = 15

    def add_phrase(source: tuple[int, int], frame: int, note: int, duration: int, velocity: int) -> None:
        rhythm_events.append(base.MidiEvent(frame * 8, frame / base.NTSC_RATE, "note_on", note, velocity, source[1], source[0]))
        rhythm_events.append(base.MidiEvent((frame + duration) * 8, (frame + duration) / base.NTSC_RATE, "note_off", note, 0, source[1], source[0]))

    vocal_pattern = (69, 72, 74, 72)

    for frame in range(loop_start_frame, max_frame + 1, step_frames):
        step = ((frame - loop_start_frame) // step_frames) % 8

        if step in (0, 4):
            add_phrase(STOMP_BEAT_SOURCE, frame, 34, 7, 108)
            add_phrase(STOMP_BEAT_SOURCE, frame + 1, 46, 5, 96)
        elif step in (2, 6):
            add_phrase(STOMP_BEAT_SOURCE, frame, 38, 5, 94)

        if step in (1, 3, 5, 7):
            note = vocal_pattern[(step >> 1) & 0x03]
            add_phrase(VOCAL_CHOP_SOURCE, frame + 1, note, 4, 84)

    return rhythm_events


def render_vgm_with_map(note_events: list[base.MidiEvent], patch_map: dict[tuple[int, int], base.Patch], out_vgm: Path, loop_start_frame: int | None = None) -> dict[str, object]:
    melodic_events = [event for event in note_events if event.channel != 9 and (event.track, event.channel) in patch_map]
    if not melodic_events:
        raise ValueError("No melodic MIDI events found")

    group_counts = Counter((event.track, event.channel) for event in melodic_events if event.kind == "note_on")

    events_by_frame: DefaultDict[int, list[base.MidiEvent]] = defaultdict(list)
    max_frame = 0
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
    parser = argparse.ArgumentParser(description="Rebuild tetris.mid with FM instruments extracted from the SGDK XGM2 sample track.")
    parser.add_argument("--vgm", default="res/music/sgdk_drum_run_rave.vgm", help="Source VGM carrying the desired SGDK test instruments")
    parser.add_argument("--midi", default="py/sound/midi/tetris.mid", help="Source MIDI arrangement")
    parser.add_argument("--out-dir", default="py/sound/out", help="Output directory")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    vgm_path = (root / args.vgm).resolve()
    midi_path = (root / args.midi).resolve()
    out_dir = (root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not vgm_path.exists():
        raise FileNotFoundError(vgm_path)
    if not midi_path.exists():
        raise FileNotFoundError(midi_path)

    _, note_events = base.parse_midi(midi_path)
    roles = select_roles(analyze_groups(note_events))
    patch_bank, extracted_patches = build_patch_bank(vgm_path)

    patch_map: dict[tuple[int, int], base.Patch] = {}
    for source in roles["bass_sources"]:
        patch_map[source] = patch_bank["bass"]
    patch_map[roles["primary_lead"]] = patch_bank["main_lead"]
    patch_map[roles["secondary_lead"]] = patch_bank["second_lead"]

    support_cycle = [patch_bank["support"], patch_bank["sparkle"]]
    for index, source in enumerate(roles["support_sources"]):
        patch_map[source] = support_cycle[index % len(support_cycle)]

    patch_map[STOMP_BEAT_SOURCE] = patch_bank["beat"]
    patch_map[VOCAL_CHOP_SOURCE] = patch_bank["vocal"]
    patch_map[INTRO_PAD_SOURCE] = patch_bank["intro_pad"]

    adjusted_events = prepare_note_events(note_events, roles)
    loop_start_frame = 206
    intro_pad_events = generate_intro_pad_events(loop_start_frame)
    stomp_events = generate_stomp_events(adjusted_events + intro_pad_events, loop_start_frame)
    render_events = adjusted_events + intro_pad_events + stomp_events
    vgm_output_path = out_dir / "tetris_sgdk_test_style.vgm"
    patch_report_path = out_dir / "sgdk_test_style_patches.json"

    base.save_patch_report(list(patch_bank.values()) + extracted_patches, patch_report_path)
    render_info = render_vgm_with_map(render_events, patch_map, vgm_output_path, loop_start_frame=loop_start_frame)
    xgm_output_path, converter_log = base.convert_to_xgm(vgm_output_path, out_dir)

    summary = {
        "style": "sgdk_test_instruments",
        "source_vgm": str(vgm_path),
        "source_midi": str(midi_path),
        "roles": {
            "primary_lead": list(roles["primary_lead"]),
            "secondary_lead": list(roles["secondary_lead"]),
            "bass_sources": [list(source) for source in roles["bass_sources"]],
            "support_sources": [list(source) for source in roles["support_sources"]],
            "profiles": [
                {
                    "source": list(profile.source),
                    "count": profile.note_count,
                    "range": [profile.min_note, profile.max_note],
                    "avg_note": round(profile.avg_note, 2),
                }
                for profile in roles["profiles"]
            ],
        },
        "loop_start_frame": loop_start_frame,
        "loop_start_seconds": round(loop_start_frame / base.NTSC_RATE, 2),
        "vgm_output": str(vgm_output_path),
        "xgm_output": str(xgm_output_path),
        "render": render_info,
        "converter_log": converter_log,
    }
    (out_dir / "build_summary_sgdk_test_style.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"Rendered SGDK-style VGM: {vgm_output_path}")
    print(f"Generated SGDK-style XGM output: {xgm_output_path}")
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
