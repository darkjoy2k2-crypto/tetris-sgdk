#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import build_guile_xgm2 as base


def write_u32_le(buffer: bytearray, offset: int, value: int) -> None:
    buffer[offset:offset + 4] = int(value).to_bytes(4, "little")


def filter_to_beat_only(source_vgm: Path, out_vgm: Path) -> dict[str, int | bool | str]:
    data = base.load_vgm_bytes(source_vgm)
    data_start = base.vgm_data_offset(data)

    out = bytearray(data[:data_start])

    loop_rel = base.read_u32_le(data, 0x1C)
    loop_abs = (0x1C + loop_rel) if loop_rel else None
    loop_out_pos: int | None = None

    # GD3 offset is not valid anymore after filtering.
    write_u32_le(out, 0x14, 0)
    write_u32_le(out, 0x1C, 0)

    pos = data_start
    pcm_command_count = 0
    psg_command_count = 0
    wait_count = 0
    data_block_count = 0
    global_write_count = 0

    def mark_loop(cmd_start: int) -> None:
        nonlocal loop_out_pos
        if (loop_abs is not None) and (loop_out_pos is None) and (cmd_start == loop_abs):
            loop_out_pos = len(out)

    while pos < len(data):
        cmd_start = pos
        cmd = data[pos]
        pos += 1

        if cmd == 0x66:
            mark_loop(cmd_start)
            out.append(cmd)
            break

        if cmd == 0x61:
            mark_loop(cmd_start)
            out.extend(data[cmd_start:cmd_start + 3])
            pos += 2
            wait_count += 1
            continue

        if cmd in (0x62, 0x63):
            mark_loop(cmd_start)
            out.append(cmd)
            wait_count += 1
            continue

        if 0x70 <= cmd <= 0x8F:
            mark_loop(cmd_start)
            out.append(cmd)
            wait_count += 1
            continue

        if cmd == 0x67:
            if data[pos] != 0x66:
                raise ValueError("Invalid VGM data block marker")
            block_len = base.read_u32_le(data, pos + 2)
            mark_loop(cmd_start)
            out.extend(data[cmd_start:cmd_start + 7 + block_len])
            pos += 6 + block_len
            data_block_count += 1
            continue

        if cmd in (0x4F, 0x50):
            mark_loop(cmd_start)
            out.extend(data[cmd_start:cmd_start + 2])
            pos += 1
            if cmd == 0x50:
                psg_command_count += 1
            continue

        if cmd in (0x52, 0x53):
            register = data[pos]
            value = data[pos + 1]
            pos += 2

            if register in (0x22, 0x27, 0x2B):
                mark_loop(cmd_start)
                out.extend((cmd, register, value))
                global_write_count += 1
            continue

        if cmd in (0x90, 0x91, 0x95):
            mark_loop(cmd_start)
            out.extend(data[cmd_start:cmd_start + 5])
            pos += 4
            pcm_command_count += 1
            continue

        if cmd == 0x92:
            mark_loop(cmd_start)
            out.extend(data[cmd_start:cmd_start + 6])
            pos += 5
            pcm_command_count += 1
            continue

        if cmd == 0x93:
            mark_loop(cmd_start)
            out.extend(data[cmd_start:cmd_start + 11])
            pos += 10
            pcm_command_count += 1
            continue

        if cmd == 0x94:
            mark_loop(cmd_start)
            out.extend(data[cmd_start:cmd_start + 2])
            pos += 1
            pcm_command_count += 1
            continue

        if 0x51 <= cmd <= 0x5F:
            pos += 2
        elif 0xA0 <= cmd <= 0xBF:
            pos += 2
        elif 0xC0 <= cmd <= 0xDF:
            pos += 3
        elif 0xE0 <= cmd <= 0xFF:
            pos += 4
        elif cmd == 0x68:
            pos += 11
        else:
            raise ValueError(f"Unsupported VGM command 0x{cmd:02X} at 0x{cmd_start:06X}")

    if not out or out[-1] != 0x66:
        out.append(0x66)

    if loop_out_pos is not None:
        write_u32_le(out, 0x1C, loop_out_pos - 0x1C)

    write_u32_le(out, 0x04, len(out) - 4)
    out_vgm.write_bytes(out)

    return {
        "source_vgm": str(source_vgm),
        "output_vgm": str(out_vgm),
        "kept_pcm_commands": pcm_command_count,
        "kept_psg_commands": psg_command_count,
        "kept_wait_commands": wait_count,
        "kept_data_blocks": data_block_count,
        "kept_global_writes": global_write_count,
        "loop_preserved": loop_out_pos is not None,
        "output_size": len(out),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract the SGDK sample beat into a standalone VGM/XGM2 test track.")
    parser.add_argument("--source", default="res/music/sgdk_drum_run_rave.vgm", help="Source SGDK sample VGM")
    parser.add_argument("--out-dir", default="py/sound/out", help="Output directory")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    source_vgm = (root / args.source).resolve()
    out_dir = (root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not source_vgm.exists():
        raise FileNotFoundError(source_vgm)

    out_vgm = out_dir / "sgdk_test_beat_only.vgm"
    summary = filter_to_beat_only(source_vgm, out_vgm)
    xgm_output_path, converter_log = base.convert_to_xgm(out_vgm, out_dir)
    summary["xgm_output"] = str(xgm_output_path)
    summary["converter_log"] = converter_log

    summary_path = out_dir / "build_summary_sgdk_beat_only.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"Rendered SGDK beat-only VGM: {out_vgm}")
    print(f"Generated SGDK beat-only XGM output: {xgm_output_path}")
    print(f"Summary: {summary_path}")
    print("Converter log:")
    print(converter_log)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
