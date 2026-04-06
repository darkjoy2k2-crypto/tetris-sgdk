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


DRUM_KICK_SOURCE = (-20, 10)
DRUM_SNARE_SOURCE = (-21, 11)
DRUM_TOM_SOURCE = (-22, 12)


def boost_patch(patch: base.Patch, name: str, total_level_drop: int = 0, feedback_boost: int = 0) -> base.Patch:
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
        raise RuntimeError("Not enough MIDI groups to assign trumpet and guitar roles")

    bass_sources = [profile.source for profile in profiles[:2]]
    high_profiles = sorted(
        [profile for profile in profiles if profile.source not in bass_sources],
        key=lambda profile: (-profile.note_count, -profile.max_note, profile.avg_note),
    )

    primary_lead = high_profiles[0].source
    secondary_lead = high_profiles[1].source
    support_sources = [profile.source for profile in high_profiles[2:]]

    return {
        "bass_sources": bass_sources,
        "primary_lead": primary_lead,
        "secondary_lead": secondary_lead,
        "support_sources": support_sources,
        "profiles": profiles,
    }


def build_patch_bank(root: Path, primary_vgz: Path) -> tuple[dict[str, base.Patch], dict[str, str]]:
    sources = {
        "guile": primary_vgz,
        "ryu": root / "py/sound/vgz/07 - Ryu's Theme.vgz",
        "ken": root / "py/sound/vgz/12 - Ken's Theme.vgz",
        "balrog": root / "py/sound/vgz/22 - Balrog's Theme.vgz",
        "chun": root / "py/sound/vgz/13 - Chun Li's Theme.vgz",
    }
    extracted = {name: base.extract_patches(path) for name, path in sources.items()}

    bank = {
        "bass": boost_patch(pick_patch(extracted["guile"], 5), "guile_bass_drive", total_level_drop=6, feedback_boost=1),
        "trumpet_lead": boost_patch(pick_patch(extracted["balrog"], 7), "balrog_trumpet_lead", total_level_drop=9, feedback_boost=1),
        "guitar_second": boost_patch(pick_patch(extracted["ryu"], 7), "ryu_guitar_second", total_level_drop=6, feedback_boost=0),
        "support_pad": boost_patch(pick_patch(extracted["guile"], 0), "guile_support_pad", total_level_drop=0, feedback_boost=0),
        "drum_kick": boost_patch(pick_patch(extracted["guile"], 5), "guile_drum_kick", total_level_drop=10, feedback_boost=1),
        "drum_snare": boost_patch(pick_patch(extracted["chun"], 4), "chun_drum_snare", total_level_drop=8, feedback_boost=1),
        "drum_tom": boost_patch(pick_patch(extracted["ken"], 6), "ken_drum_tom", total_level_drop=10, feedback_boost=0),
    }
    return bank, {name: str(path) for name, path in sources.items()}


def prepare_note_events(note_events: list[base.MidiEvent], roles: dict[str, object]) -> list[base.MidiEvent]:
    primary_lead = roles["primary_lead"]
    secondary_lead = roles["secondary_lead"]
    support_sources = set(roles["support_sources"])
    adjusted: list[base.MidiEvent] = []

    for event in note_events:
        if event.channel == 9:
            continue

        source = (event.track, event.channel)
        note = event.note

        if source == primary_lead and note >= 84:
            note -= 12
        elif source == secondary_lead and note >= 72:
            note -= 12
        elif source in support_sources:
            if event.time_sec < 3.45 and note >= 69:
                note -= 12
            elif note >= 74:
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


def generate_drum_events(note_events: list[base.MidiEvent], loop_start_frame: int) -> list[base.MidiEvent]:
    max_frame = 0
    for event in note_events:
        frame = max(0, int(round(event.time_sec * base.NTSC_RATE)))
        if frame > max_frame:
            max_frame = frame

    drum_events: list[base.MidiEvent] = []
    step_frames = 13

    def add_hit(source: tuple[int, int], frame: int, note: int, duration: int) -> None:
        drum_events.append(
            base.MidiEvent(frame * 8, frame / base.NTSC_RATE, "note_on", note, 110, source[1], source[0])
        )
        drum_events.append(
            base.MidiEvent((frame + duration) * 8, (frame + duration) / base.NTSC_RATE, "note_off", note, 0, source[1], source[0])
        )

    for frame in range(loop_start_frame, max_frame + 1, step_frames):
        step = ((frame - loop_start_frame) // step_frames) % 8

        if step in (0, 3, 4):
            add_hit(DRUM_KICK_SOURCE, frame, 34 if step == 0 else 36, 4)
        if step in (2, 6):
            add_hit(DRUM_SNARE_SOURCE, frame, 50, 3)
        if step == 7:
            add_hit(DRUM_TOM_SOURCE, frame, 43, 4)

    return drum_events


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
    parser = argparse.ArgumentParser(description="Create a driving Street-Fighter-flavored XGM2 arrangement of tetris.mid with trumpet lead, guitar second voice, and FM drum hits.")
    parser.add_argument("--vgz", default="py/sound/vgz/10 - Guile's Theme.vgz", help="Primary VGZ source")
    parser.add_argument("--midi", default="py/sound/midi/tetris.mid", help="Source MIDI arrangement")
    parser.add_argument("--out-dir", default="py/sound/out", help="Output directory")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    vgz_path = (root / args.vgz).resolve()
    midi_path = (root / args.midi).resolve()
    out_dir = (root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not vgz_path.exists():
        raise FileNotFoundError(vgz_path)
    if not midi_path.exists():
        raise FileNotFoundError(midi_path)

    _, note_events = base.parse_midi(midi_path)
    roles = select_roles(analyze_groups(note_events))
    patch_bank, source_paths = build_patch_bank(root, vgz_path)

    patch_map: dict[tuple[int, int], base.Patch] = {}
    for source in roles["bass_sources"]:
        patch_map[source] = patch_bank["bass"]
    patch_map[roles["primary_lead"]] = patch_bank["trumpet_lead"]
    patch_map[roles["secondary_lead"]] = patch_bank["guitar_second"]

    for source in roles["support_sources"]:
        patch_map[source] = patch_bank["support_pad"]

    patch_map[DRUM_KICK_SOURCE] = patch_bank["drum_kick"]
    patch_map[DRUM_SNARE_SOURCE] = patch_bank["drum_snare"]
    patch_map[DRUM_TOM_SOURCE] = patch_bank["drum_tom"]

    adjusted_events = prepare_note_events(note_events, roles)
    loop_start_frame = 206
    drum_events = generate_drum_events(adjusted_events, loop_start_frame)
    render_events = adjusted_events + drum_events

    vgm_output_path = out_dir / "tetris_sf_clear_leads.vgm"
    patch_report_path = out_dir / "sf_clear_leads_patches.json"
    base.save_patch_report(list(patch_bank.values()), patch_report_path)
    render_info = render_vgm_with_map(render_events, patch_map, vgm_output_path, loop_start_frame=loop_start_frame)
    xgm_output_path, converter_log = base.convert_to_xgm(vgm_output_path, out_dir)

    summary = {
        "style": "street_fighter_clear_leads",
        "sources": source_paths,
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
    (out_dir / "build_summary_clear_leads.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"Rendered drive-leads VGM: {vgm_output_path}")
    print(f"Generated drive-leads XGM output: {xgm_output_path}")
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
