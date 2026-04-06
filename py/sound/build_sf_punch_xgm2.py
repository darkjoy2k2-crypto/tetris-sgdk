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


@dataclass
class ArrangedEvent:
    tick: int
    time_sec: float
    kind: str
    note: int
    velocity: int
    channel: int
    track: int
    priority: int = 3


@dataclass
class PunchVoiceState:
    patch_name: str | None = None
    note: int | None = None
    source: tuple[int, int] | None = None
    started_frame: int = -1
    priority: int = 0


BASS_SOURCE = (-10, 0)
RHYTHM_SOURCE = (-11, 1)
LEAD_SOURCE = (-12, 2)
DOUBLE_SOURCE = (-13, 3)
HARM_SOURCE = (-14, 4)
ACCENT_SOURCE = (-15, 5)


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


def clone_event(event: base.MidiEvent, source: tuple[int, int], note: int, priority: int) -> ArrangedEvent:
    return ArrangedEvent(
        tick=event.tick,
        time_sec=event.time_sec,
        kind=event.kind,
        note=max(24, min(95, note)),
        velocity=event.velocity,
        channel=source[1],
        track=source[0],
        priority=priority,
    )


def select_patch_bank(root: Path, primary_vgz: Path) -> tuple[dict[str, base.Patch], dict[str, str]]:
    sources = {
        "guile": primary_vgz,
        "ryu": root / "py/sound/vgz/07 - Ryu's Theme.vgz",
        "ken": root / "py/sound/vgz/12 - Ken's Theme.vgz",
        "chun": root / "py/sound/vgz/13 - Chun Li's Theme.vgz",
        "balrog": root / "py/sound/vgz/22 - Balrog's Theme.vgz",
    }
    extracted = {name: base.extract_patches(path) for name, path in sources.items()}

    bank = {
        "bass_punch": boost_patch(pick_patch(extracted["guile"], 5), "guile_bass_punch", total_level_drop=8, feedback_boost=1),
        "rhythm_guitar": boost_patch(pick_patch(extracted["ken"], 6), "ken_rhythm_guitar", total_level_drop=12, feedback_boost=0),
        "trumpet_lead": boost_patch(pick_patch(extracted["balrog"], 7), "balrog_trumpet_lead", total_level_drop=14, feedback_boost=1),
        "guitar_lead": boost_patch(pick_patch(extracted["ryu"], 7), "ryu_guitar_lead", total_level_drop=12, feedback_boost=0),
        "support_pad": boost_patch(pick_patch(extracted["guile"], 0), "guile_support_pad", total_level_drop=5, feedback_boost=0),
        "accent_stab": boost_patch(pick_patch(extracted["chun"], 4), "chun_accent_stab", total_level_drop=12, feedback_boost=1),
    }
    return bank, {name: str(path) for name, path in sources.items()}


def generate_drive_events(note_events: list[base.MidiEvent], bass_sources: set[tuple[int, int]], max_frame: int) -> list[ArrangedEvent]:
    bass_by_frame: DefaultDict[int, list[base.MidiEvent]] = defaultdict(list)
    for event in note_events:
        if (event.track, event.channel) in bass_sources:
            frame = max(0, int(round(event.time_sec * base.NTSC_RATE)))
            bass_by_frame[frame].append(event)

    active_notes: list[int] = []
    rhythm_events: list[ArrangedEvent] = []
    root_note = 40

    for frame in range(max_frame + 1):
        for event in bass_by_frame.get(frame, []):
            if event.kind == "note_on":
                active_notes.append(event.note)
            elif event.note in active_notes:
                active_notes.remove(event.note)

        if active_notes:
            root_note = min(active_notes)

        if frame % 30 != 0:
            continue

        step = (frame // 30) % 4
        beat_note = max(40, min(76, root_note + 12 + (7 if step == 3 else 0)))
        duration = 6 if step in (1, 3) else 8

        if step in (0, 1, 3):
            rhythm_events.append(
                ArrangedEvent(frame * 8, frame / base.NTSC_RATE, "note_on", beat_note, 108, RHYTHM_SOURCE[1], RHYTHM_SOURCE[0], 1)
            )
            rhythm_events.append(
                ArrangedEvent((frame + duration) * 8, (frame + duration) / base.NTSC_RATE, "note_off", beat_note, 0, RHYTHM_SOURCE[1], RHYTHM_SOURCE[0], 1)
            )

        if step == 0 and ((frame // 30) % 8 in (0, 4)):
            accent_note = max(48, min(84, root_note + 19))
            rhythm_events.append(
                ArrangedEvent(frame * 8, frame / base.NTSC_RATE, "note_on", accent_note, 112, ACCENT_SOURCE[1], ACCENT_SOURCE[0], 2)
            )
            rhythm_events.append(
                ArrangedEvent((frame + 5) * 8, (frame + 5) / base.NTSC_RATE, "note_off", accent_note, 0, ACCENT_SOURCE[1], ACCENT_SOURCE[0], 2)
            )

    return rhythm_events


def prepare_punch_arrangement(note_events: list[base.MidiEvent], patch_bank: dict[str, base.Patch]) -> tuple[list[ArrangedEvent], dict[tuple[int, int], base.Patch], dict[str, object]]:
    profiles = analyze_groups(note_events)
    if len(profiles) < 3:
        raise RuntimeError("Not enough melodic MIDI groups to build the punch arrangement")

    bass_sources = {profile.source for profile in profiles[:2]}
    lead_candidates = [profile for profile in profiles if profile.avg_note >= 68.0]
    primary_lead = max(lead_candidates, key=lambda profile: (profile.note_count, profile.max_note)).source if lead_candidates else profiles[-1].source
    harmony_sources = {profile.source for profile in lead_candidates if profile.source != primary_lead}

    arranged: list[ArrangedEvent] = []
    for event in note_events:
        if event.channel == 9:
            continue

        source = (event.track, event.channel)
        if source == primary_lead:
            arranged.append(clone_event(event, LEAD_SOURCE, event.note, 6))
            arranged.append(clone_event(event, DOUBLE_SOURCE, event.note + 12, 5))
        elif source in bass_sources:
            arranged.append(clone_event(event, BASS_SOURCE, event.note - 12 if event.note >= 36 else event.note, 5))
            arranged.append(clone_event(event, RHYTHM_SOURCE, event.note + 7, 2))
        elif source in harmony_sources:
            harmony_note = event.note - 12 if event.note > 76 else event.note
            arranged.append(clone_event(event, HARM_SOURCE, harmony_note, 4))
            if event.kind == "note_on" or event.kind == "note_off":
                arranged.append(clone_event(event, ACCENT_SOURCE, harmony_note + 7, 2))
        else:
            accent_note = event.note - 12 if event.note > 78 else event.note
            arranged.append(clone_event(event, ACCENT_SOURCE, accent_note, 3))

    max_frame = 0
    for event in arranged:
        frame = max(0, int(round(event.time_sec * base.NTSC_RATE)))
        if frame > max_frame:
            max_frame = frame

    arranged.extend(generate_drive_events(note_events, bass_sources, max_frame))
    arranged.sort(key=lambda event: (event.time_sec, 0 if event.kind == "note_off" else 1, -event.priority))

    patch_map = {
        BASS_SOURCE: patch_bank["bass_punch"],
        RHYTHM_SOURCE: patch_bank["rhythm_guitar"],
        LEAD_SOURCE: patch_bank["trumpet_lead"],
        DOUBLE_SOURCE: patch_bank["guitar_lead"],
        HARM_SOURCE: patch_bank["support_pad"],
        ACCENT_SOURCE: patch_bank["accent_stab"],
    }

    summary = {
        "primary_lead": list(primary_lead),
        "bass_sources": [list(source) for source in sorted(bass_sources)],
        "harmony_sources": [list(source) for source in sorted(harmony_sources)],
        "group_profiles": [
            {
                "source": list(profile.source),
                "count": profile.note_count,
                "range": [profile.min_note, profile.max_note],
                "avg_note": round(profile.avg_note, 2),
            }
            for profile in profiles
        ],
    }
    return arranged, patch_map, summary


def release_voice(writer: base.VgmWriter, voices: list[PunchVoiceState], note_map: DefaultDict[tuple[tuple[int, int], int], list[int]], voice_index: int) -> None:
    voice = voices[voice_index]
    if voice.note is None or voice.source is None:
        return

    key = (voice.source, voice.note)
    stack = note_map.get(key)
    if stack and voice_index in stack:
        stack.remove(voice_index)
        if not stack:
            note_map.pop(key, None)

    base.key_off(writer, voice_index)
    voice.note = None
    voice.source = None
    voice.started_frame = -1
    voice.priority = 0


def render_vgm_punch(note_events: list[ArrangedEvent], patch_map: dict[tuple[int, int], base.Patch], out_vgm: Path) -> dict[str, object]:
    events_by_frame: DefaultDict[int, list[ArrangedEvent]] = defaultdict(list)
    max_frame = 0
    for event in note_events:
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

    voices = [PunchVoiceState() for _ in range(6)]
    note_map: DefaultDict[tuple[tuple[int, int], int], list[int]] = defaultdict(list)

    current_frame = 0
    while current_frame <= max_frame:
        frame_events = events_by_frame.get(current_frame, [])
        frame_events.sort(key=lambda event: (0 if event.kind == "note_off" else 1, -event.priority))

        for event in frame_events:
            source = (event.track, event.channel)
            if source not in patch_map:
                continue

            key = (source, event.note)
            if event.kind == "note_off":
                stack = note_map.get(key)
                if stack:
                    release_voice(writer, voices, note_map, stack[-1])
                continue

            free_voice = next((index for index, voice in enumerate(voices) if voice.note is None), None)
            if free_voice is None:
                candidate = min(range(6), key=lambda index: (voices[index].priority, voices[index].started_frame))
                if voices[candidate].priority > event.priority:
                    continue
                release_voice(writer, voices, note_map, candidate)
                free_voice = candidate

            patch = patch_map[source]
            voice = voices[free_voice]
            if voice.patch_name != patch.name:
                base.apply_patch(writer, free_voice, patch)
                voice.patch_name = patch.name

            base.key_on(writer, free_voice, event.note)
            voice.note = event.note
            voice.source = source
            voice.started_frame = current_frame
            voice.priority = event.priority
            note_map[key].append(free_voice)

        current_frame += 1
        writer.wait_frames(1)

    for voice_index in range(6):
        release_voice(writer, voices, note_map, voice_index)

    out_vgm.write_bytes(writer.finish())
    group_counts = Counter((event.track, event.channel) for event in note_events if event.kind == "note_on")
    return {
        "frames": max_frame + 1,
        "groups": [
            {
                "source": [track, channel],
                "note_count": count,
                "patch": patch_map[(track, channel)].name,
            }
            for (track, channel), count in group_counts.most_common()
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a stronger Street-Fighter-inspired XGM2 arrangement of tetris.mid.")
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
    patch_bank, source_paths = select_patch_bank(root, vgz_path)
    arranged_events, patch_map, arrangement_summary = prepare_punch_arrangement(note_events, patch_bank)

    vgm_output_path = out_dir / "tetris_sf_punch_mix.vgm"
    patch_report_path = out_dir / "sf_punch_patches.json"
    base.save_patch_report(list(patch_bank.values()), patch_report_path)
    render_info = render_vgm_punch(arranged_events, patch_map, vgm_output_path)
    xgm_output_path, converter_log = base.convert_to_xgm(vgm_output_path, out_dir)

    summary = {
        "style": "street_fighter_punch_mix",
        "sources": source_paths,
        "source_midi": str(midi_path),
        "selected_patch_count": len(patch_bank),
        "vgm_output": str(vgm_output_path),
        "xgm_output": str(xgm_output_path),
        "arrangement": arrangement_summary,
        "render": render_info,
        "converter_log": converter_log,
    }
    (out_dir / "build_summary_punch.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"Rendered stronger VGM: {vgm_output_path}")
    print(f"Generated stronger XGM output: {xgm_output_path}")
    print(f"Patch report: {patch_report_path}")
    print("Converter log:")
    print(converter_log)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover - CLI failure path
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
