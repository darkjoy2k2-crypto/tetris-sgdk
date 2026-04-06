#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import DefaultDict

import build_guile_xgm2 as base
import build_sgdk_test_style_xgm2 as style

DMF_MAGIC = b".DelekDefleMask."
DMF_VERSION = 26  # DefleMask 1.1.3+ compatible
DMF_SYSTEM_GENESIS = 0x02
DMF_CHANNELS = 10
PATTERN_LEN = 64
ROWS_PER_BEAT = 4
DMF_NOTE_OFF = 100


@dataclass
class DmfOperator:
    am: int
    ar: int
    dr: int
    mult: int
    rr: int
    sl: int
    tl: int
    dt2: int
    rs: int
    dt: int
    d2r: int
    ssg_env: int


@dataclass
class DmfInstrument:
    name: str
    alg: int
    fb: int
    fms: int
    ams: int
    operators: list[DmfOperator]


@dataclass
class PatternCell:
    note: int = 0
    octave: int = 0
    volume: int = -1
    effect: int = -1
    effect_value: int = -1
    instrument: int = -1


def write_u8(buffer: bytearray, value: int) -> None:
    buffer.append(value & 0xFF)


def write_s16(buffer: bytearray, value: int) -> None:
    buffer.extend(struct.pack("<h", int(value)))


def write_u32(buffer: bytearray, value: int) -> None:
    buffer.extend(struct.pack("<I", int(value) & 0xFFFFFFFF))


def write_string(buffer: bytearray, value: str) -> None:
    encoded = value.encode("utf-8", errors="replace")[:255]
    write_u8(buffer, len(encoded))
    buffer.extend(encoded)


def midi_note_to_dmf(note: int) -> tuple[int, int]:
    note = max(12, min(95, note))
    return (note % 12) + 1, max(0, min(8, note // 12))


def validate_instruments(instruments: list[DmfInstrument]) -> None:
    for instrument_index, instrument in enumerate(instruments):
        if not (0 <= instrument.alg <= 7 and 0 <= instrument.fb <= 7 and 0 <= instrument.fms <= 7 and 0 <= instrument.ams <= 3):
            raise ValueError(f"Instrument {instrument_index} header has out-of-range values")

        for operator_index, operator in enumerate(instrument.operators):
            checks = {
                "mult": (operator.mult, 0, 15),
                "tl": (operator.tl, 0, 127),
                "ar": (operator.ar, 0, 31),
                "dr": (operator.dr, 0, 31),
                "sl": (operator.sl, 0, 15),
                "rr": (operator.rr, 0, 15),
                "am": (operator.am, 0, 1),
                "dt2": (operator.dt2, 0, 3),
                "rs": (operator.rs, 0, 3),
                "dt": (operator.dt, 0, 7),
                "d2r": (operator.d2r, 0, 31),
                "ssg_env": (operator.ssg_env, 0, 15),
            }
            for field_name, (value, minimum, maximum) in checks.items():
                if not (minimum <= value <= maximum):
                    raise ValueError(
                        f"Instrument {instrument_index} operator {operator_index} field {field_name}={value} is outside {minimum}..{maximum}"
                    )


def sanitize_instrument_for_dmf(instrument: DmfInstrument) -> DmfInstrument:
    safe_operators: list[DmfOperator] = []
    for operator in instrument.operators:
        safe_operators.append(
            DmfOperator(
                am=operator.am & 0x01,
                ar=max(1, min(31, operator.ar)),
                dr=max(0, min(31, operator.dr)),
                mult=max(0, min(15, operator.mult)),
                rr=max(1, min(15, operator.rr)),
                sl=max(0, min(15, operator.sl)),
                tl=max(0, min(127, operator.tl)),
                dt2=0,
                rs=0,
                dt=0,
                d2r=max(0, min(31, operator.d2r)),
                ssg_env=0,
            )
        )

    return DmfInstrument(
        name=instrument.name,
        alg=max(0, min(7, instrument.alg)),
        fb=max(0, min(7, instrument.fb)),
        fms=0,
        ams=0,
        operators=safe_operators,
    )


def make_main_lead_audible(instrument: DmfInstrument) -> DmfInstrument:
    # DefleMask uses attenuation-style FM volumes. A simple all-carrier setup is
    # much more reliable here than preserving a fragile arcade patch verbatim.
    tl_targets = (18, 28, 10, 0)
    safe_operators: list[DmfOperator] = []

    for index, operator in enumerate(instrument.operators):
        safe_operators.append(
            DmfOperator(
                am=operator.am,
                ar=max(24, operator.ar),
                dr=min(operator.dr, 10),
                mult=operator.mult,
                rr=max(4, operator.rr),
                sl=max(4, min(8, operator.sl if operator.sl > 0 else 6)),
                tl=min(operator.tl, tl_targets[index]),
                dt2=0,
                rs=0,
                dt=0,
                d2r=min(operator.d2r, 8),
                ssg_env=0,
            )
        )

    return DmfInstrument(
        name=instrument.name,
        alg=7,
        fb=max(2, min(5, instrument.fb)),
        fms=0,
        ams=0,
        operators=safe_operators,
    )


def patch_to_dmf_instrument(patch: base.Patch, name: str) -> DmfInstrument:
    regs = patch.regs
    b0 = regs.get(0xB0, 0)
    b4 = regs.get(0xB4, 0)
    operators: list[DmfOperator] = []

    for offset in (0x00, 0x04, 0x08, 0x0C):
        reg30 = regs.get(0x30 + offset, 0)
        reg40 = regs.get(0x40 + offset, 0)
        reg50 = regs.get(0x50 + offset, 0)
        reg60 = regs.get(0x60 + offset, 0)
        reg70 = regs.get(0x70 + offset, 0)
        reg80 = regs.get(0x80 + offset, 0)
        reg90 = regs.get(0x90 + offset, 0)

        operators.append(
            DmfOperator(
                am=(reg60 >> 7) & 0x01,
                ar=reg50 & 0x1F,
                dr=reg60 & 0x1F,
                mult=reg30 & 0x0F,
                rr=reg80 & 0x0F,
                sl=(reg80 >> 4) & 0x0F,
                tl=reg40 & 0x7F,
                dt2=0,
                rs=(reg50 >> 6) & 0x03,
                dt=(reg30 >> 4) & 0x07,
                d2r=reg70 & 0x1F,
                ssg_env=reg90 & 0x0F,
            )
        )

    return DmfInstrument(
        name=name,
        alg=b0 & 0x07,
        fb=(b0 >> 3) & 0x07,
        fms=b4 & 0x07,
        ams=(b4 >> 4) & 0x03,
        operators=operators,
    )


def build_patch_bank(source_vgm: Path) -> tuple[dict[str, DmfInstrument], list[base.Patch]]:
    patches = base.extract_patches(source_vgm)
    if len(patches) < 4:
        raise RuntimeError("Source VGM did not yield enough FM patches for DefleMask export")

    if len(patches) >= 18:
        lead_index, second_index, bass_index, pad_index = 17, 10, 5, 4
    else:
        # Small banks such as `Guile's Theme.vgz` work more reliably in DefleMask
        # when we stay on the early melodic patches instead of reusing the tail-end ones.
        lead_index, second_index, bass_index, pad_index = 0, 1, 2, 3

    lead = style.shape_patch(style.pick_patch(patches, min(lead_index, len(patches) - 1)), "dfm_main_lead", total_level_drop=7, feedback_boost=1)
    second = style.shape_patch(style.pick_patch(patches, min(second_index, len(patches) - 1)), "dfm_second_voice", total_level_drop=-2, feedback_boost=0)
    bass = style.shape_patch(style.pick_patch(patches, min(bass_index, len(patches) - 1)), "dfm_bass", total_level_drop=1, feedback_boost=1)
    pad = style.shape_patch(style.pick_patch(patches, min(pad_index, len(patches) - 1)), "dfm_pad", total_level_drop=-3, feedback_boost=0)

    bank = {
        "main_lead": make_main_lead_audible(sanitize_instrument_for_dmf(patch_to_dmf_instrument(lead, "MAIN LEAD"))),
        "second_lead": sanitize_instrument_for_dmf(patch_to_dmf_instrument(second, "SECOND")),
        "bass": sanitize_instrument_for_dmf(patch_to_dmf_instrument(bass, "BASS")),
        "pad": sanitize_instrument_for_dmf(patch_to_dmf_instrument(pad, "PAD")),
    }
    return bank, patches


def build_channel_roles(note_events: list[base.MidiEvent]) -> dict[tuple[int, int], tuple[int, int, int]]:
    roles = style.select_roles(style.analyze_groups(note_events))
    mapping: dict[tuple[int, int], tuple[int, int, int]] = {}

    mapping[roles["primary_lead"]] = (0, 0, 104)
    mapping[roles["secondary_lead"]] = (1, 1, 72)

    bass_sources = list(roles["bass_sources"])
    if bass_sources:
        mapping[bass_sources[0]] = (3, 2, 86)
    if len(bass_sources) > 1:
        mapping[bass_sources[1]] = (4, 2, 74)

    for index, source in enumerate(roles["support_sources"]):
        channel = 2 if index == 0 else 5
        mapping[source] = (channel, 3, 58)

    return mapping


def build_pattern_grid(division: int, note_events: list[base.MidiEvent], channel_roles: dict[tuple[int, int], tuple[int, int, int]]) -> tuple[list[list[list[PatternCell]]], int]:
    rows_per_quarter = max(1, division // ROWS_PER_BEAT)
    last_row = 0

    for event in note_events:
        if event.kind in ("note_on", "note_off") and event.channel != 9:
            row = max(0, round(event.tick / rows_per_quarter))
            last_row = max(last_row, row)

    order_count = max(1, (last_row // PATTERN_LEN) + 1)
    grid = [
        [[PatternCell() for _ in range(PATTERN_LEN)] for _ in range(order_count)]
        for _ in range(DMF_CHANNELS)
    ]

    sorted_events = sorted(
        [event for event in note_events if event.channel != 9 and (event.track, event.channel) in channel_roles],
        key=lambda event: (event.tick, 0 if event.kind == "note_off" else 1),
    )

    for event in sorted_events:
        source = (event.track, event.channel)
        channel, instrument_index, volume_base = channel_roles[source]
        row_index = max(0, round(event.tick / rows_per_quarter))
        order = row_index // PATTERN_LEN
        row = row_index % PATTERN_LEN
        cell = grid[channel][order][row]

        if event.kind == "note_off":
            if cell.note == 0:
                cell.note = DMF_NOTE_OFF
                cell.octave = 0
            continue

        note_value, octave = midi_note_to_dmf(event.note)
        cell.note = note_value
        cell.octave = octave
        cell.instrument = instrument_index
        scaled_volume = int((max(1, event.velocity) * volume_base) / 127)
        # DefleMask stores FM volume as attenuation: smaller values are louder.
        cell.volume = max(0, min(0x7F, 0x7F - scaled_volume))

    return grid, order_count


def build_dmf_bytes(name: str, author: str, instruments: list[DmfInstrument], patterns: list[list[list[PatternCell]]], order_count: int) -> bytes:
    payload = bytearray()
    payload.extend(DMF_MAGIC)
    write_u8(payload, DMF_VERSION)
    write_u8(payload, DMF_SYSTEM_GENESIS)
    write_string(payload, name)
    write_string(payload, author)

    write_u8(payload, 4)   # highlight A
    write_u8(payload, 16)  # highlight B
    write_u8(payload, 0)   # time base
    write_u8(payload, 6)   # speed 1
    write_u8(payload, 6)   # speed 2
    write_u8(payload, 1)   # PAL/NTSC flag used by DefleMask/Furnace compat layer
    write_u8(payload, 1)   # custom tempo flag
    payload.extend(b"060")
    write_u32(payload, PATTERN_LEN)
    write_u8(payload, order_count)

    for channel in range(DMF_CHANNELS):
        for order in range(order_count):
            write_u8(payload, order)
            write_string(payload, "")

    validate_instruments(instruments)
    write_u8(payload, len(instruments))
    for instrument in instruments:
        write_string(payload, instrument.name)
        write_u8(payload, 1)  # FM instrument
        write_u8(payload, instrument.alg)
        write_u8(payload, instrument.fb)
        write_u8(payload, instrument.fms)
        write_u8(payload, instrument.ams)

        for op in instrument.operators:
            # DefleMask/Furnace FM order: MULT, TL, AR, DR, SL, RR, AM, DT2, RS, DT, D2R, SSG-EG
            write_u8(payload, op.mult)
            write_u8(payload, op.tl)
            write_u8(payload, op.ar)
            write_u8(payload, op.dr)
            write_u8(payload, op.sl)
            write_u8(payload, op.rr)
            write_u8(payload, op.am)
            write_u8(payload, op.dt2)
            write_u8(payload, op.rs)
            write_u8(payload, op.dt)
            write_u8(payload, op.d2r)
            write_u8(payload, op.ssg_env)

    # DMF layout for Genesis: instruments -> wavetables -> patterns -> samples
    write_u8(payload, 0)  # no wavetables

    for channel in range(DMF_CHANNELS):
        write_u8(payload, 1)  # one effect column
        for order in range(order_count):
            for cell in patterns[channel][order]:
                write_s16(payload, cell.note)
                write_s16(payload, cell.octave)
                write_s16(payload, cell.volume)
                write_s16(payload, cell.effect)
                write_s16(payload, cell.effect_value)
                write_s16(payload, cell.instrument)

    write_u8(payload, 0)  # no samples

    return zlib.compress(bytes(payload), level=9)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a DefleMask .dmf from a source VGM patch set and a MIDI arrangement.")
    parser.add_argument("--vgm", default="res/music/sgdk_drum_run_rave.vgm", help="Source VGM used for FM instrument extraction")
    parser.add_argument("--midi", default="py/sound/midi/tetris.mid", help="Source MIDI arrangement")
    parser.add_argument("--out-dir", default="py/sound/out", help="Output directory")
    parser.add_argument("--stem", default="tetris_vgm_cross", help="Output filename stem for the generated DMF and summary")
    parser.add_argument("--title", default="Tetris VGM Cross", help="Song title embedded in the DMF")
    parser.add_argument("--author", default="GitHub Copilot", help="Author tag embedded in the DMF")
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

    division, note_events = base.parse_midi(midi_path)
    patch_bank, extracted_patches = build_patch_bank(vgm_path)
    channel_roles = build_channel_roles(note_events)
    patterns, order_count = build_pattern_grid(division, note_events, channel_roles)

    instruments = [
        patch_bank["main_lead"],
        patch_bank["second_lead"],
        patch_bank["bass"],
        patch_bank["pad"],
    ]

    out_path = out_dir / f"{args.stem}.dmf"
    out_path.write_bytes(build_dmf_bytes(args.title, args.author, instruments, patterns, order_count))

    summary = {
        "source_vgm": str(vgm_path),
        "source_midi": str(midi_path),
        "dmf_output": str(out_path),
        "dmf_version": DMF_VERSION,
        "system_id": DMF_SYSTEM_GENESIS,
        "order_count": order_count,
        "pattern_length": PATTERN_LEN,
        "instrument_count": len(instruments),
        "extracted_patch_count": len(extracted_patches),
        "channel_roles": {
            f"track{source[0]}_ch{source[1]}": {
                "dmf_channel": channel,
                "instrument_index": instrument_index,
                "volume_base": volume_base,
            }
            for source, (channel, instrument_index, volume_base) in channel_roles.items()
        },
    }
    summary_path = out_dir / f"build_summary_{args.stem}.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"Created DefleMask module: {out_path}")
    print(f"Summary: {summary_path}")
    print("Open the generated .dmf in DefleMask and retune channels/instruments as needed.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
