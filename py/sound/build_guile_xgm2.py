#!/usr/bin/env python3
from __future__ import annotations

import argparse
import gzip
import json
import os
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import DefaultDict

YM2612_CLOCK = 7670454
NTSC_RATE = 60
FRAME_SAMPLES = 735
DEFAULT_TEMPO_US = 500_000
PATCH_REGS = [
    0x30, 0x34, 0x38, 0x3C,
    0x40, 0x44, 0x48, 0x4C,
    0x50, 0x54, 0x58, 0x5C,
    0x60, 0x64, 0x68, 0x6C,
    0x70, 0x74, 0x78, 0x7C,
    0x80, 0x84, 0x88, 0x8C,
    0x90, 0x94, 0x98, 0x9C,
    0xB0, 0xB4,
]
PATCH_SIGNATURE_REGS = [
    reg for reg in PATCH_REGS if not (0x40 <= reg <= 0x4C) and reg != 0xB4
]
NOTE_FNUM = [0x284, 0x2AB, 0x2D3, 0x2FE, 0x32D, 0x35C, 0x38F, 0x3C5, 0x3FF, 0x43C, 0x47C, 0x4C0]


@dataclass
class Patch:
    id: int
    name: str
    regs: dict[int, int]
    first_channel: int


@dataclass
class MidiEvent:
    tick: int
    time_sec: float
    kind: str
    note: int
    velocity: int
    channel: int
    track: int


@dataclass
class VoiceState:
    patch_id: int | None = None
    note: int | None = None
    source: tuple[int, int] | None = None
    started_frame: int = -1


def read_u32_le(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "little")


def read_u16_be(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "big")


def read_u32_be(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "big")


def read_vlq(data: bytes, pos: int) -> tuple[int, int]:
    value = 0
    while True:
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if (byte & 0x80) == 0:
            return value, pos


def load_vgm_bytes(path: Path) -> bytes:
    if path.suffix.lower() == ".vgz":
        with gzip.open(path, "rb") as handle:
            return handle.read()
    return path.read_bytes()


def vgm_data_offset(data: bytes) -> int:
    offset = read_u32_le(data, 0x34)
    return 0x34 + offset if offset else 0x40


def ym_channel_from_key(value: int) -> int | None:
    raw = value & 0x07
    if raw <= 2:
        return raw
    if 4 <= raw <= 6:
        return raw - 1
    return None


def reg_channel_index(register: int) -> int | None:
    low = register & 0x03
    return low if low <= 2 else None


def extract_patches(vgm_path: Path) -> list[Patch]:
    data = load_vgm_bytes(vgm_path)
    pos = vgm_data_offset(data)
    channel_state: list[dict[int, int]] = [dict() for _ in range(6)]
    patches: list[Patch] = []
    seen: set[tuple[int, ...]] = set()

    while pos < len(data):
        cmd = data[pos]
        pos += 1

        if cmd in (0x52, 0x53):
            register = data[pos]
            value = data[pos + 1]
            pos += 2
            port = 0 if cmd == 0x52 else 1

            if port == 0 and register == 0x28:
                channel = ym_channel_from_key(value)
                if channel is not None and (value & 0xF0):
                    regs = {
                        reg: channel_state[channel].get(reg, 0)
                        for reg in PATCH_REGS
                    }
                    signature = tuple(regs.get(reg, 0) for reg in PATCH_SIGNATURE_REGS)
                    if signature not in seen:
                        seen.add(signature)
                        patches.append(
                            Patch(
                                id=len(patches),
                                name=f"guile_patch_{len(patches):02d}",
                                regs=regs,
                                first_channel=channel,
                            )
                        )
            else:
                channel_index = reg_channel_index(register)
                if channel_index is not None:
                    channel = channel_index + (port * 3)
                    base_register = register - channel_index
                    if base_register in PATCH_REGS:
                        channel_state[channel][base_register] = value

        elif cmd == 0x61:
            pos += 2
        elif cmd in (0x62, 0x63, 0x66):
            if cmd == 0x66:
                break
        elif 0x70 <= cmd <= 0x7F:
            pass
        elif cmd == 0x67:
            if data[pos] != 0x66:
                raise ValueError("Invalid VGM data block marker")
            block_len = read_u32_le(data, pos + 2)
            pos += 6 + block_len
        elif cmd == 0x68:
            pos += 11
        elif cmd in (0x4F, 0x50):
            pos += 1
        elif 0x51 <= cmd <= 0x5F:
            pos += 2
        elif 0x80 <= cmd <= 0x8F:
            pass
        elif cmd in (0x90, 0x91, 0x95):
            pos += 4
        elif cmd == 0x92:
            pos += 5
        elif cmd == 0x93:
            pos += 10
        elif cmd == 0x94:
            pos += 1
        elif 0xA0 <= cmd <= 0xBF:
            pos += 2
        elif 0xC0 <= cmd <= 0xDF:
            pos += 3
        elif 0xE0 <= cmd <= 0xFF:
            pos += 4
        else:
            raise ValueError(f"Unsupported VGM command 0x{cmd:02X} at 0x{pos - 1:06X}")

    return patches


def parse_midi(midi_path: Path) -> tuple[int, list[MidiEvent]]:
    data = midi_path.read_bytes()
    if data[0:4] != b"MThd":
        raise ValueError("Invalid MIDI header")

    header_len = read_u32_be(data, 4)
    midi_format = read_u16_be(data, 8)
    track_count = read_u16_be(data, 10)
    division = read_u16_be(data, 12)
    if division & 0x8000:
        raise ValueError("SMPTE MIDI timing is not supported")

    pos = 8 + header_len
    tempo_events: list[tuple[int, int]] = [(0, DEFAULT_TEMPO_US)]
    note_events_raw: list[tuple[int, str, int, int, int, int]] = []

    for track_index in range(track_count):
        if data[pos:pos + 4] != b"MTrk":
            raise ValueError(f"Missing MIDI track header at offset {pos}")
        length = read_u32_be(data, pos + 4)
        track_data = data[pos + 8:pos + 8 + length]
        pos += 8 + length

        absolute_tick = 0
        cursor = 0
        running_status = None

        while cursor < len(track_data):
            delta, cursor = read_vlq(track_data, cursor)
            absolute_tick += delta

            status = track_data[cursor]
            if status < 0x80:
                if running_status is None:
                    raise ValueError("Running status encountered before a status byte")
                status = running_status
            else:
                cursor += 1
                if status < 0xF0:
                    running_status = status

            if status == 0xFF:
                meta_type = track_data[cursor]
                cursor += 1
                payload_len, cursor = read_vlq(track_data, cursor)
                payload = track_data[cursor:cursor + payload_len]
                cursor += payload_len
                if meta_type == 0x51 and payload_len == 3:
                    tempo_events.append((absolute_tick, int.from_bytes(payload, "big")))
                continue

            if status in (0xF0, 0xF7):
                payload_len, cursor = read_vlq(track_data, cursor)
                cursor += payload_len
                continue

            event_type = status & 0xF0
            channel = status & 0x0F

            if event_type in (0x80, 0x90):
                note = track_data[cursor]
                velocity = track_data[cursor + 1]
                cursor += 2
                if event_type == 0x90 and velocity > 0:
                    kind = "note_on"
                else:
                    kind = "note_off"
                note_events_raw.append((absolute_tick, kind, note, velocity, channel, track_index))
            elif event_type in (0xA0, 0xB0, 0xE0):
                cursor += 2
            elif event_type in (0xC0, 0xD0):
                cursor += 1
            else:
                raise ValueError(f"Unsupported MIDI event 0x{status:02X}")

    timeline: list[tuple[str, int, object]] = []
    for tick, tempo in tempo_events:
        timeline.append(("tempo", tick, tempo))
    for entry in note_events_raw:
        timeline.append((entry[1], entry[0], entry))

    timeline.sort(key=lambda item: (item[1], 0 if item[0] == "tempo" else 1, 0 if item[0] == "note_off" else 1))

    current_tempo = DEFAULT_TEMPO_US
    current_sec = 0.0
    last_tick = 0
    note_events: list[MidiEvent] = []

    for item_type, tick, payload in timeline:
        delta_ticks = tick - last_tick
        current_sec += (delta_ticks * current_tempo) / (division * 1_000_000.0)
        last_tick = tick

        if item_type == "tempo":
            current_tempo = int(payload)
        else:
            _, kind, note, velocity, channel, track_index = payload
            note_events.append(
                MidiEvent(
                    tick=tick,
                    time_sec=current_sec,
                    kind=kind,
                    note=note,
                    velocity=velocity,
                    channel=channel,
                    track=track_index,
                )
            )

    if midi_format not in (0, 1):
        raise ValueError(f"Unsupported MIDI format {midi_format}")

    return division, note_events


class VgmWriter:
    def __init__(self) -> None:
        self.commands = bytearray()
        self.total_samples = 0
        self.loop_command_offset: int | None = None
        self.loop_start_samples = 0

    def ym_write_raw(self, port: int, register: int, value: int) -> None:
        command = 0x52 if port == 0 else 0x53
        self.commands.extend((command, register & 0xFF, value & 0xFF))

    def ym_write_global(self, register: int, value: int) -> None:
        self.ym_write_raw(0, register, value)

    def ym_write(self, channel: int, base_register: int, value: int) -> None:
        port = 0 if channel < 3 else 1
        offset = channel % 3
        self.ym_write_raw(port, base_register + offset, value)

    def ym_key(self, channel: int, value: int) -> None:
        raw = channel if channel < 3 else channel + 1
        self.commands.extend((0x52, 0x28, (value & 0xF0) | raw))

    def wait_frames(self, frame_count: int) -> None:
        for _ in range(frame_count):
            self.commands.append(0x62)
        self.total_samples += frame_count * FRAME_SAMPLES

    def set_loop_start(self) -> None:
        if self.loop_command_offset is None:
            self.loop_command_offset = len(self.commands)
            self.loop_start_samples = self.total_samples

    def finish(self) -> bytes:
        self.commands.append(0x66)
        header = bytearray(0x100)
        header[0:4] = b"Vgm "
        header[0x04:0x08] = (len(header) + len(self.commands) - 4).to_bytes(4, "little")
        header[0x08:0x0C] = (0x00000170).to_bytes(4, "little")
        header[0x18:0x1C] = self.total_samples.to_bytes(4, "little")
        if self.loop_command_offset is not None and self.total_samples > self.loop_start_samples:
            loop_data_offset = len(header) + self.loop_command_offset
            header[0x1C:0x20] = (loop_data_offset - 0x1C).to_bytes(4, "little")
            header[0x20:0x24] = (self.total_samples - self.loop_start_samples).to_bytes(4, "little")
        header[0x24:0x28] = NTSC_RATE.to_bytes(4, "little")
        header[0x2C:0x30] = YM2612_CLOCK.to_bytes(4, "little")
        header[0x34:0x38] = (0x100 - 0x34).to_bytes(4, "little")
        return bytes(header + self.commands)

def note_to_ym2612(note: int) -> tuple[int, int]:
    note = max(24, min(95, note))
    octave = max(0, min(7, (note // 12) - 1))
    fnum = NOTE_FNUM[note % 12]
    a4 = ((octave & 0x07) << 3) | ((fnum >> 8) & 0x07)
    a0 = fnum & 0xFF
    return a4, a0


def apply_patch(writer: VgmWriter, channel: int, patch: Patch) -> None:
    for register in PATCH_REGS:
        writer.ym_write(channel, register, patch.regs.get(register, 0))


def key_on(writer: VgmWriter, channel: int, note: int) -> None:
    a4, a0 = note_to_ym2612(note)
    writer.ym_write(channel, 0xA4, a4)
    writer.ym_write(channel, 0xA0, a0)
    writer.ym_key(channel, 0xF0)


def key_off(writer: VgmWriter, channel: int) -> None:
    writer.ym_key(channel, 0x00)


def release_voice(writer: VgmWriter, voices: list[VoiceState], note_map: DefaultDict[tuple[tuple[int, int], int], list[int]], voice_index: int) -> None:
    voice = voices[voice_index]
    if voice.note is None or voice.source is None:
        return

    key = (voice.source, voice.note)
    stack = note_map.get(key)
    if stack and voice_index in stack:
        stack.remove(voice_index)
        if not stack:
            note_map.pop(key, None)

    key_off(writer, voice_index)
    voice.note = None
    voice.source = None
    voice.started_frame = -1


def render_vgm(note_events: list[MidiEvent], patches: list[Patch], out_vgm: Path) -> dict[str, object]:
    melodic_events = [event for event in note_events if event.channel != 9]
    if not melodic_events:
        raise ValueError("No melodic MIDI events found")

    group_counts = Counter((event.track, event.channel) for event in melodic_events if event.kind == "note_on")
    ordered_groups = [group for group, _ in group_counts.most_common()]
    patch_map = {
        group: patches[index % len(patches)].id
        for index, group in enumerate(ordered_groups)
    }

    events_by_frame: DefaultDict[int, list[MidiEvent]] = defaultdict(list)
    max_frame = 0
    for event in melodic_events:
        frame = max(0, int(round(event.time_sec * NTSC_RATE)))
        events_by_frame[frame].append(event)
        if frame > max_frame:
            max_frame = frame

    writer = VgmWriter()
    writer.ym_write_global(0x22, 0x00)
    writer.ym_write_global(0x27, 0x00)
    writer.ym_write_global(0x2B, 0x00)
    for channel in range(6):
        writer.ym_write(channel, 0xB4, 0xC0)
        key_off(writer, channel)

    voices = [VoiceState() for _ in range(6)]
    note_map: DefaultDict[tuple[tuple[int, int], int], list[int]] = defaultdict(list)

    current_frame = 0
    while current_frame <= max_frame:
        frame_events = events_by_frame.get(current_frame, [])
        frame_events.sort(key=lambda event: 0 if event.kind == "note_off" else 1)

        for event in frame_events:
            source = (event.track, event.channel)
            key = (source, event.note)

            if event.kind == "note_off":
                stack = note_map.get(key)
                if stack:
                    release_voice(writer, voices, note_map, stack[-1])
                continue

            free_voice = next((index for index, voice in enumerate(voices) if voice.note is None), None)
            if free_voice is None:
                free_voice = min(range(6), key=lambda index: voices[index].started_frame)
                release_voice(writer, voices, note_map, free_voice)

            patch_id = patch_map[source]
            patch = patches[patch_id]
            voice = voices[free_voice]
            if voice.patch_id != patch_id:
                apply_patch(writer, free_voice, patch)
                voice.patch_id = patch_id

            key_on(writer, free_voice, event.note)
            voice.note = event.note
            voice.source = source
            voice.started_frame = current_frame
            note_map[key].append(free_voice)

        current_frame += 1
        writer.wait_frames(1)

    for voice_index in range(6):
        release_voice(writer, voices, note_map, voice_index)

    out_vgm.write_bytes(writer.finish())
    return {
        "frames": max_frame + 1,
        "groups": [
            {
                "track": track,
                "midi_channel": channel,
                "note_count": count,
                "patch": patches[patch_map[(track, channel)]].name,
            }
            for (track, channel), count in group_counts.most_common()
        ],
    }


def save_patch_report(patches: list[Patch], output_path: Path) -> None:
    serializable = []
    for patch in patches:
        serializable.append(
            {
                "id": patch.id,
                "name": patch.name,
                "first_channel": patch.first_channel,
                "algorithm": patch.regs.get(0xB0, 0) & 0x07,
                "feedback": (patch.regs.get(0xB0, 0) >> 3) & 0x07,
                "regs": {f"0x{register:02X}": value for register, value in sorted(patch.regs.items())},
            }
        )
    output_path.write_text(json.dumps(serializable, indent=2), encoding="utf-8")


def try_run(command: list[str]) -> tuple[bool, str]:
    try:
        completed = subprocess.run(command, capture_output=True, text=True, check=False)
    except FileNotFoundError as exc:
        return False, str(exc)

    message = (completed.stdout or "") + (completed.stderr or "")
    return completed.returncode == 0, message.strip()


def convert_to_xgm(vgm_path: Path, out_dir: Path) -> tuple[Path, str]:
    gdk = os.environ.get("GDK")
    if not gdk:
        raise EnvironmentError("GDK environment variable is not set")

    xgm2_tool = Path(gdk) / "bin" / "xgm2tool.jar"
    legacy_tool = Path(gdk) / "bin" / "xgmtool.exe"
    attempts: list[str] = []
    stem = vgm_path.stem

    if xgm2_tool.exists():
        for suffix in (".xgm2", ".xgm", ".xgc"):
            output_path = out_dir / f"{stem}{suffix}"
            ok, message = try_run(["java", "-jar", str(xgm2_tool), str(vgm_path), str(output_path)])
            attempts.append(f"xgm2tool -> {output_path.name}: {'ok' if ok else 'fail'} {message}".strip())
            if output_path.exists() and output_path.stat().st_size > 0:
                return output_path, "\n".join(attempts)

    if legacy_tool.exists():
        output_path = out_dir / f"{stem}.xgm"
        ok, message = try_run([str(legacy_tool), str(vgm_path), str(output_path), "-n", "-s"])
        attempts.append(f"xgmtool -> {output_path.name}: {'ok' if ok else 'fail'} {message}".strip())
        if output_path.exists() and output_path.stat().st_size > 0:
            return output_path, "\n".join(attempts)

    raise RuntimeError("XGM conversion failed\n" + "\n".join(attempts))

def main() -> int:
    parser = argparse.ArgumentParser(description="Create an XGM/XGM2 arrangement of tetris.mid using Guile's Theme FM patches.")
    parser.add_argument("--vgz", default="py/sound/vgz/10 - Guile's Theme.vgz", help="Source VGZ for patch extraction")
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

    patches = extract_patches(vgz_path)
    if not patches:
        raise RuntimeError("No YM2612 patches were extracted from the VGZ source")

    _, note_events = parse_midi(midi_path)
    patch_report_path = out_dir / "guile_patches.json"
    vgm_output_path = out_dir / "tetris_guile_arrangement.vgm"

    save_patch_report(patches, patch_report_path)
    render_info = render_vgm(note_events, patches, vgm_output_path)
    xgm_output_path, converter_log = convert_to_xgm(vgm_output_path, out_dir)

    summary = {
        "source_vgz": str(vgz_path),
        "source_midi": str(midi_path),
        "patch_count": len(patches),
        "vgm_output": str(vgm_output_path),
        "xgm_output": str(xgm_output_path),
        "render": render_info,
        "converter_log": converter_log,
    }
    (out_dir / "build_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"Extracted {len(patches)} patches from {vgz_path.name}")
    print(f"Rendered VGM: {vgm_output_path}")
    print(f"Generated XGM output: {xgm_output_path}")
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
