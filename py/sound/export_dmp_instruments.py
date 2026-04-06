#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

import build_guile_xgm2 as base

DMP_VERSION = 11
DMP_SYSTEM_GENESIS = 0x02
DMP_MODE_FM = 1


@dataclass
class DmpOperator:
    mult: int
    tl: int
    ar: int
    dr: int
    sl: int
    rr: int
    am: int
    rs: int
    dt: int
    dt2: int
    d2r: int
    ssg_env: int


@dataclass
class DmpInstrument:
    name: str
    alg: int
    fb: int
    fms: int
    ams: int
    operators: list[DmpOperator]


def sanitize_name(value: str) -> str:
    value = value.strip().lower().replace("'", "")
    value = re.sub(r"[^a-z0-9]+", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    return value or "patch"


def patch_to_dmp_instrument(patch: base.Patch, label: str) -> DmpInstrument:
    regs = patch.regs
    b0 = regs.get(0xB0, 0)
    b4 = regs.get(0xB4, 0)
    operators: list[DmpOperator] = []

    for offset in (0x00, 0x04, 0x08, 0x0C):
        reg30 = regs.get(0x30 + offset, 0)
        reg40 = regs.get(0x40 + offset, 0)
        reg50 = regs.get(0x50 + offset, 0)
        reg60 = regs.get(0x60 + offset, 0)
        reg70 = regs.get(0x70 + offset, 0)
        reg80 = regs.get(0x80 + offset, 0)
        reg90 = regs.get(0x90 + offset, 0)

        operators.append(
            DmpOperator(
                mult=reg30 & 0x0F,
                tl=reg40 & 0x7F,
                ar=reg50 & 0x1F,
                dr=reg60 & 0x1F,
                sl=(reg80 >> 4) & 0x0F,
                rr=reg80 & 0x0F,
                am=(reg60 >> 7) & 0x01,
                rs=(reg50 >> 6) & 0x03,
                dt=(reg30 >> 4) & 0x07,
                dt2=0,
                d2r=reg70 & 0x1F,
                ssg_env=reg90 & 0x0F,
            )
        )

    return DmpInstrument(
        name=label,
        alg=b0 & 0x07,
        fb=(b0 >> 3) & 0x07,
        fms=b4 & 0x07,
        ams=(b4 >> 4) & 0x03,
        operators=operators,
    )


def validate_instrument(instrument: DmpInstrument) -> None:
    if not (0 <= instrument.alg <= 7):
        raise ValueError(f"Invalid algorithm for {instrument.name}: {instrument.alg}")
    if not (0 <= instrument.fb <= 7):
        raise ValueError(f"Invalid feedback for {instrument.name}: {instrument.fb}")
    if not (0 <= instrument.fms <= 7):
        raise ValueError(f"Invalid FMS for {instrument.name}: {instrument.fms}")
    if not (0 <= instrument.ams <= 3):
        raise ValueError(f"Invalid AMS for {instrument.name}: {instrument.ams}")

    for op_index, op in enumerate(instrument.operators):
        checks = {
            "mult": (op.mult, 0, 15),
            "tl": (op.tl, 0, 127),
            "ar": (op.ar, 0, 31),
            "dr": (op.dr, 0, 31),
            "sl": (op.sl, 0, 15),
            "rr": (op.rr, 0, 15),
            "am": (op.am, 0, 1),
            "rs": (op.rs, 0, 3),
            "dt": (op.dt, 0, 7),
            "dt2": (op.dt2, 0, 3),
            "d2r": (op.d2r, 0, 31),
            "ssg_env": (op.ssg_env, 0, 15),
        }
        for field_name, (value, minimum, maximum) in checks.items():
            if not (minimum <= value <= maximum):
                raise ValueError(
                    f"{instrument.name} op{op_index} {field_name}={value} outside {minimum}..{maximum}"
                )


def write_dmp(path: Path, instrument: DmpInstrument) -> None:
    validate_instrument(instrument)

    payload = bytearray()
    payload.append(DMP_VERSION)
    payload.append(DMP_SYSTEM_GENESIS)
    payload.append(DMP_MODE_FM)
    payload.append(instrument.fms & 0x07)
    payload.append(instrument.fb & 0x07)
    payload.append(instrument.alg & 0x07)
    payload.append(instrument.ams & 0x03)

    for op in instrument.operators:
        payload.append(op.mult & 0x0F)
        payload.append(op.tl & 0x7F)
        payload.append(op.ar & 0x1F)
        payload.append(op.dr & 0x1F)
        payload.append(op.sl & 0x0F)
        payload.append(op.rr & 0x0F)
        payload.append(op.am & 0x01)
        payload.append(op.rs & 0x03)
        payload.append((op.dt & 0x0F) | ((op.dt2 & 0x03) << 4))
        payload.append(op.d2r & 0x1F)
        payload.append(op.ssg_env & 0x0F)

    path.write_bytes(payload)


def export_bank(source_path: Path, out_dir: Path, prefix: str) -> dict:
    patches = base.extract_patches(source_path)
    out_dir.mkdir(parents=True, exist_ok=True)
    summary: list[dict] = []

    for index, patch in enumerate(patches):
        base_name = f"{prefix}_{index:02d}_{sanitize_name(patch.name)}"
        instrument = patch_to_dmp_instrument(patch, base_name.upper())
        file_path = out_dir / f"{base_name}.dmp"
        write_dmp(file_path, instrument)

        summary.append(
            {
                "patch_id": patch.id,
                "name": patch.name,
                "label": instrument.name,
                "file": str(file_path),
                "first_channel": patch.first_channel,
                "alg": instrument.alg,
                "fb": instrument.fb,
                "fms": instrument.fms,
                "ams": instrument.ams,
            }
        )

    summary_path = out_dir / "_export_summary.json"
    summary_path.write_text(
        json.dumps(
            {
                "source": str(source_path),
                "output_dir": str(out_dir),
                "count": len(summary),
                "instruments": summary,
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    return {
        "source": str(source_path),
        "output_dir": str(out_dir),
        "count": len(summary),
        "summary_file": str(summary_path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Export DefleMask/Furnace .dmp instruments from VGM/VGZ patch sources.")
    parser.add_argument("--root", default=None, help="Project root override")
    args = parser.parse_args()

    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parents[2]
    inst_root = root / "inst"

    targets = [
        {
            "source": root / "res" / "music" / "sgdk_drum_run_rave.vgm",
            "out_dir": inst_root / "sgdk_test",
            "prefix": "sgdk_test",
        },
        {
            "source": root / "py" / "sound" / "vgz" / "10 - Guile's Theme.vgz",
            "out_dir": inst_root / "guile_theme",
            "prefix": "guile",
        },
    ]

    exported: list[dict] = []
    for target in targets:
        source_path = target["source"]
        if not source_path.exists():
            raise FileNotFoundError(source_path)
        exported.append(export_bank(source_path, target["out_dir"], target["prefix"]))

    manifest_path = inst_root / "manifest.json"
    manifest_path.write_text(json.dumps({"exports": exported}, indent=2), encoding="utf-8")

    print(f"Exported instrument banks to: {inst_root}")
    for entry in exported:
        print(f"- {entry['count']:2d} patches -> {entry['output_dir']}")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
