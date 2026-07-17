#!/usr/bin/env python3
"""Build one EspBle library version against many arduino-esp32 core versions/boards.

Given a single library version (a git tag/ref, or the current working tree), this
builds the requested examples for every core version x target board and writes a
compatibility matrix as Markdown (and JSON).

Mechanism: for each core version the example's `sketch.yaml` platform pin is
rewritten to that version before `arduino-cli compile --profile <target>` runs.
arduino-cli auto-installs the pinned core version on first use. Targets that have
no profile in the sketch.yaml are synthesized by cloning the `esp32s3` profile
with the target's FQBN, so board coverage does not require editing every example.
All sketch.yaml files are restored to their original content when the run ends.

This is primarily driven by CI (`.github/workflows/*-matrix.yml`), because a full
core sweep rewrites and rebuilds every sketch and would dirty a local checkout and
exhaust local disk. Run it locally only for a quick single-core/single-board check.

Examples:
  # core-version sweep on esp32s3 with the default representative examples
  python tools/version_matrix.py --core-versions auto --targets esp32s3

  # board sweep on the verified core with every example
  python tools/version_matrix.py --core-versions 3.3.10 \
      --targets esp32s3,esp32,esp32c3,esp32c6,esp32h2,esp32p4 --examples all

  # CI decomposition: one core per job writes a JSON payload, then merge them
  python tools/version_matrix.py --core-versions 3.3.10 --json-only --json out/3.3.10.json
  python tools/version_matrix.py --render-from out --output docs/COMPATIBILITY.0.1.0.md

  # dry run: show what would build, no compiles
  python tools/version_matrix.py --core-versions 3.3.10 --list

All sketch.yaml files are restored to their committed content when the run ends.
"""

import argparse
import json
import pathlib
import re
import shutil
import subprocess
import sys
import time
import urllib.request

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
PACKAGE_INDEX_URL = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
# Floor for `auto` core discovery. Pass --core-versions explicitly to probe below.
CORE_VERSION_FLOOR = (3, 2, 0)

# One representative example per feature area. Every example links the full src/
# tree, so core compatibility is mostly identical across examples; the spread is
# kept for API-usage coverage. Override with --examples (comma list, or `all`).
DEFAULT_EXAMPLES = [
    ("Smoke", "CompileSmoke"),
    ("GAP", "Gap/Connect"),
    ("GATT Server", "Gatt/NotifyServer"),
    ("GATT Client", "Gatt/Client"),
    ("Security", "Security/StaticPasskeyServer"),
    ("HID Device", "Hid/KeyboardDevice"),
    ("HID Host", "Hid/KeyboardHost"),
]

# BLE-capable boards (plus esp32p4, which documents the Hosted-BLE build gap).
DEFAULT_TARGETS = ["esp32s3"]
KNOWN_TARGET_FQBNS = {
    "esp32": "esp32:esp32:esp32",
    "esp32s3": "esp32:esp32:esp32s3",
    "esp32c3": "esp32:esp32:esp32c3",
    "esp32c6": "esp32:esp32:esp32c6",
    "esp32h2": "esp32:esp32:esp32h2",
    "esp32p4": "esp32:esp32:esp32p4",
}

PASS, FAIL, ABSENT, NO_PROFILE, NA_BOARD = "pass", "fail", "absent", "no-profile", "na-board"
CELL_GLYPH = {PASS: "✅", FAIL: "❌", ABSENT: "—", NO_PROFILE: "·", NA_BOARD: "·"}
CELL_NOTE = {
    ABSENT: "example not present in this library version",
    NO_PROFILE: "no such profile in the example's sketch.yaml",
    NA_BOARD: "target board not available in this core version",
}

NA_BOARD_MARKERS = (
    "not found in platform",
    "Invalid FQBN",
    "board not found",
    "Unknown FQBN",
    "unknown package",
)


def all_examples() -> list[tuple[str, str]]:
    pairs = []
    for sk in sorted((REPO_ROOT / "examples").rglob("sketch.yaml")):
        rel = sk.parent.relative_to(REPO_ROOT / "examples")
        pairs.append((str(rel.parts[0]), str(rel)))
    return pairs


def profiles_in(sketch_yaml: pathlib.Path) -> list[str]:
    names = []
    in_profiles = False
    for line in sketch_yaml.read_text().splitlines():
        if re.match(r"^profiles:\s*$", line):
            in_profiles = True
            continue
        if in_profiles:
            if line and not line[0].isspace():
                break
            m = re.match(r"^  ([A-Za-z0-9_.\-]+):\s*$", line)
            if m:
                names.append(m.group(1))
    return names


def set_platform_version(sketch_yaml: pathlib.Path, version: str) -> None:
    text = sketch_yaml.read_text()
    new = re.sub(
        r"(platform:\s*esp32:esp32\s*\()[^)]+(\))",
        lambda m: f"{m.group(1)}{version}{m.group(2)}",
        text,
    )
    if new != text:
        sketch_yaml.write_text(new)


def ensure_profile(sketch_yaml: pathlib.Path, target: str) -> bool:
    """Synthesize a profile for `target` by cloning the esp32s3 profile block."""
    if target in profiles_in(sketch_yaml):
        return True
    fqbn = KNOWN_TARGET_FQBNS.get(target)
    if fqbn is None:
        return False
    lines = sketch_yaml.read_text().splitlines()
    block = []
    in_block = False
    for line in lines:
        if re.match(r"^  esp32s3:\s*$", line):
            in_block = True
            block.append(f"  {target}:")
            continue
        if in_block:
            if line and not line.startswith("    "):
                break
            block.append(re.sub(r"(fqbn:\s*).*$", rf"\g<1>{fqbn}", line))
    if len(block) <= 1:
        return False
    # Insert the cloned block right before the first non-indented line after profiles.
    out = []
    in_profiles = False
    for line in lines:
        if re.match(r"^profiles:\s*$", line):
            in_profiles = True
            out.append(line)
            continue
        if in_profiles and line and not line[0].isspace():
            out.extend(block)
            in_profiles = False
        out.append(line)
    if in_profiles:
        out.extend(block)
    sketch_yaml.write_text("\n".join(out) + "\n")
    return True


def discover_core_versions() -> list[str]:
    with urllib.request.urlopen(PACKAGE_INDEX_URL, timeout=30) as resp:
        data = json.load(resp)
    versions = set()
    for pkg in data.get("packages", []):
        if pkg.get("name") != "esp32":
            continue
        for plat in pkg.get("platforms", []):
            if plat.get("architecture") == "esp32":
                versions.add(plat.get("version"))
    return sorted((v for v in versions if _ver_tuple(v) >= CORE_VERSION_FLOOR), key=_ver_tuple)


def _ver_tuple(v: str) -> tuple:
    parts = re.findall(r"\d+", v)
    return tuple(int(p) for p in parts[:3]) + (0,) * (3 - len(parts[:3]))


def _read_lib_version(root: pathlib.Path) -> str:
    props = (root / "library.properties").read_text()
    m = re.search(r"^version=(.+)$", props, re.MULTILINE)
    return m.group(1).strip() if m else "unknown"


def classify(returncode: int, output: str) -> str:
    if returncode == 0:
        return PASS
    if any(marker in output for marker in NA_BOARD_MARKERS):
        return NA_BOARD
    return FAIL


def error_summary(output: str) -> str:
    lines = [ln.strip() for ln in output.splitlines() if ln.strip()]
    for ln in lines:
        if "error:" in ln:
            return ln
    return lines[-1] if lines else ""


def render_markdown(lib_version, title, core_versions, targets, examples, results) -> str:
    lines = []
    lines.append(f"# EspBle {lib_version} — {title}")
    lines.append("")
    lines.append(f"- Targets: {', '.join(targets)}")
    lines.append(f"- Core versions: {', '.join(core_versions)}")
    lines.append("")
    lines.append("Legend: ✅ builds · ❌ fails · — example absent in this version · · not applicable (no profile / board not in core)")
    lines.append("")
    for target in targets:
        lines.append(f"## {target}")
        lines.append("")
        lines.append("| Feature (example) | " + " | ".join(core_versions) + " |")
        lines.append("| --- | " + " | ".join("---" for _ in core_versions) + " |")
        for cat, path in examples:
            cells = []
            for core in core_versions:
                state, _ = results[(cat, path, target, core)]
                cells.append(CELL_GLYPH[state])
            lines.append(f"| {cat} (`{path}`) | " + " | ".join(cells) + " |")
        lines.append("")
    failures = [
        (path, t, c, note)
        for (cat, path, t, c), (state, note) in sorted(results.items())
        if state == FAIL and note
    ]
    if failures:
        lines.append("## Failure details")
        lines.append("")
        for path, t, c, note in failures:
            lines.append(f"- `{path}` @ {t} / {c}: `{note}`")
        lines.append("")
    return "\n".join(lines) + "\n"


def build_payload(lib_version, title, core_versions, targets, examples, results) -> dict:
    return {
        "lib_version": lib_version,
        "title": title,
        "core_versions": core_versions,
        "targets": targets,
        "examples": [{"category": cat, "example": path} for cat, path in examples],
        "results": [
            {"category": cat, "example": path, "target": t, "core": c, "state": st, "note": nt}
            for (cat, path, t, c), (st, nt) in results.items()
        ],
    }


def merge_payloads(payloads: list[dict]):
    """Combine per-core/per-target JSON payloads into one renderable matrix.

    Each CI job writes one payload (a single core version, possibly several
    targets/examples); merging stitches the columns/rows back together.
    """
    lib_version = payloads[0].get("lib_version", "unknown")
    title = payloads[0].get("title", "arduino-esp32 compatibility")
    targets, examples, seen_examples = [], [], set()
    core_versions, results = set(), {}
    for p in payloads:
        for t in p.get("targets", []):
            if t not in targets:
                targets.append(t)
        # Preserve the example order declared in each payload.
        for entry in p.get("examples", []):
            key = (entry["category"], entry["example"])
            if key not in seen_examples:
                seen_examples.add(key)
                examples.append(key)
        for row in p.get("results", []):
            cat, path, t, c = row["category"], row["example"], row["target"], row["core"]
            core_versions.add(c)
            if (cat, path) not in seen_examples:
                seen_examples.add((cat, path))
                examples.append((cat, path))
            results[(cat, path, t, c)] = (row["state"], row.get("note", ""))
    core_versions = sorted(core_versions, key=_ver_tuple)
    for cat, path in examples:
        for t in targets:
            for c in core_versions:
                results.setdefault((cat, path, t, c), (ABSENT, ""))
    return lib_version, title, core_versions, targets, examples, results


def render_from_dir(input_dir: pathlib.Path, output_opt: str) -> int:
    payloads = [json.loads(p.read_text()) for p in sorted(input_dir.glob("*.json"))]
    if not payloads:
        print(f"No *.json payloads found in {input_dir}", file=sys.stderr)
        return 2
    lib_version, title, core_versions, targets, examples, results = merge_payloads(payloads)
    md = render_markdown(lib_version, title, core_versions, targets, examples, results)
    output = pathlib.Path(output_opt) if output_opt else REPO_ROOT / "docs" / f"COMPATIBILITY.{lib_version}.md"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(md)
    print(f"Merged {len(payloads)} payload(s) -> {output} (cores: {', '.join(core_versions)})")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--core-versions", default="auto",
                        help="comma-separated core versions, or 'auto' for released cores >= 3.2.0")
    parser.add_argument("--targets", default=",".join(DEFAULT_TARGETS),
                        help=f"comma-separated targets (known: {','.join(KNOWN_TARGET_FQBNS)})")
    parser.add_argument("--examples", default="",
                        help="comma-separated example paths relative to examples/, or 'all'; default: representative set")
    parser.add_argument("--title", default="arduino-esp32 core compatibility",
                        help="report title suffix")
    parser.add_argument("--output", default="", help="Markdown output path (default: docs/COMPATIBILITY.<libver>.md)")
    parser.add_argument("--json", dest="json_out", default="", help="JSON output path (per-job result payload)")
    parser.add_argument("--json-only", action="store_true",
                        help="write only the JSON payload, skip Markdown (for decomposed CI jobs)")
    parser.add_argument("--render-from", default="",
                        help="skip building: merge every *.json in this directory into one Markdown matrix")
    parser.add_argument("--print-cores", action="store_true",
                        help="print the resolved core-version list as a JSON array and exit (for CI matrix setup)")
    parser.add_argument("--list", action="store_true", help="print the build plan and exit (no compiles)")
    args = parser.parse_args()

    if args.render_from:
        return render_from_dir(pathlib.Path(args.render_from), args.output)

    targets = [t.strip() for t in args.targets.split(",") if t.strip()]
    if args.examples == "all":
        examples = all_examples()
    elif args.examples:
        examples = [(p.strip().split("/")[0], p.strip()) for p in args.examples.split(",") if p.strip()]
    else:
        examples = DEFAULT_EXAMPLES

    if args.core_versions == "auto":
        core_versions = discover_core_versions()
    else:
        core_versions = [v.strip() for v in args.core_versions.split(",") if v.strip()]
    if not core_versions:
        print("No core versions to build.", file=sys.stderr)
        return 2

    if args.print_cores:
        print(json.dumps(core_versions))
        return 0

    lib_version = _read_lib_version(REPO_ROOT)
    examples_root = REPO_ROOT / "examples"

    print(f"Library: {lib_version}")
    print(f"Cores  : {', '.join(core_versions)}")
    print(f"Targets: {', '.join(targets)}")
    print(f"Examples: {', '.join(p for _, p in examples)}")
    print()

    if args.list:
        for cat, path in examples:
            sk = examples_root / path / "sketch.yaml"
            print(f"  {cat}: {path}  {'profiles=' + ','.join(profiles_in(sk)) if sk.exists() else 'ABSENT'}")
        return 0

    if not shutil.which("arduino-cli"):
        print("arduino-cli not found on PATH", file=sys.stderr)
        return 2

    # Snapshot every sketch.yaml so pins/synthesized profiles are restored on exit.
    snapshots = {}
    for _, path in examples:
        sk = examples_root / path / "sketch.yaml"
        if sk.exists():
            snapshots[sk] = sk.read_text()

    results = {}
    n_builds = 0
    run_start = time.monotonic()
    try:
        for core in core_versions:
            core_start = time.monotonic()
            for cat, path in examples:
                sketch_dir = examples_root / path
                sk = sketch_dir / "sketch.yaml"
                if not sk.exists():
                    for target in targets:
                        results[(cat, path, target, core)] = (ABSENT, CELL_NOTE[ABSENT])
                    continue
                set_platform_version(sk, core)
                for target in targets:
                    key = (cat, path, target, core)
                    if not ensure_profile(sk, target):
                        results[key] = (NO_PROFILE, CELL_NOTE[NO_PROFILE])
                        continue
                    print(f"==> core {core} | {target} | {path}", flush=True)
                    proc = subprocess.run(
                        ["arduino-cli", "compile", "--profile", target, str(sketch_dir)],
                        capture_output=True, text=True,
                    )
                    combined = (proc.stderr or "") + (proc.stdout or "")
                    state = classify(proc.returncode, combined)
                    note = error_summary(combined) if state == FAIL else CELL_NOTE.get(state, "")
                    results[key] = (state, note)
                    n_builds += 1
                    print(f"    {CELL_GLYPH[state]} {state}" + (f" ({note})" if state == FAIL else ""), flush=True)
            print(f"-- core {core} done in {time.monotonic() - core_start:.0f}s", flush=True)
    finally:
        for sk, text in snapshots.items():
            sk.write_text(text)

    total = time.monotonic() - run_start
    print(f"\nBuilt {n_builds} target(s) in {total:.0f}s ({total / 60:.1f} min)")

    if not args.json_only:
        md = render_markdown(lib_version, args.title, core_versions, targets, examples, results)
        out_path = pathlib.Path(args.output) if args.output else REPO_ROOT / "docs" / f"COMPATIBILITY.{lib_version}.md"
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(md)
        print(f"Wrote {out_path}")

    if args.json_out:
        payload = build_payload(lib_version, args.title, core_versions, targets, examples, results)
        jp = pathlib.Path(args.json_out)
        jp.parent.mkdir(parents=True, exist_ok=True)
        jp.write_text(json.dumps(payload, indent=2, ensure_ascii=False))
        print(f"Wrote {jp}")
    elif args.json_only:
        print("WARNING: --json-only given without --json; no output written", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
