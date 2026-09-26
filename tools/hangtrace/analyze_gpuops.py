#!/usr/bin/env python3
"""Summarize KytyPS5 sampled GPU operation captures (KYTY_GPU_OP_PROFILE=<seconds>).

Usage:
  python analyze_gpuops.py DIR_OR_CSV [--top 25] [--capture N] [--exe kyty_emulator.exe]

DIR_OR_CSV is the output directory (hang-trace folder or _Profiling/gpuops-*) or one
gpuops-<flip>.csv file. For a directory the most recent capture is analyzed unless --capture
(summary capture index) or a specific CSV is given. Prints GPU time by category, op kind, site,
scope>site, pipeline and render target, the render-pass / barrier / command-buffer counts, and
(with --exe) the top callers symbolized with llvm-symbolizer (first non-vulkan.hpp frame).

delta_ns is the completion time since the previous timestamp in the same command buffer; GPUs
overlap work, so treat per-op numbers as attribution heuristics (see gpuops-README.txt).
Pure standard library.
"""
import argparse
import collections
import csv
import glob
import os
import re
import shutil
import subprocess
import sys


def to_int(value, default=0):
    try:
        return int(value)
    except (TypeError, ValueError):
        try:
            return int(float(value))
        except (TypeError, ValueError):
            return default


def ms(ns):
    return ns / 1e6


def find_symbolizer():
    found = shutil.which("llvm-symbolizer")
    if found:
        return found
    for pattern in (r"C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\Llvm\x64\bin\llvm-symbolizer.exe",
                    r"C:\Program Files\LLVM\bin\llvm-symbolizer.exe"):
        matches = glob.glob(pattern)
        if matches:
            return matches[0]
    return None


def symbolize(exe, callers):
    """Map 'exe+0xRVA' strings to 'function (file:line)' of the first non-Vulkan frame."""
    tool = find_symbolizer()
    if tool is None or not exe:
        return {}
    rvas = []
    for caller in callers:
        m = re.match(r"exe\+0x([0-9a-fA-F]+)$", caller)
        if m:
            rvas.append((caller, int(m.group(1), 16)))
    if not rvas:
        return {}
    # Return addresses point after the call; step back one byte to land on the call.
    stdin = "\n".join(f"0x{rva - 1:x}" for _, rva in rvas) + "\n"
    try:
        proc = subprocess.run([tool, f"--obj={exe}", "--relative-address", "--inlining",
                               "--functions=linkage", "--demangle"],
                              input=stdin, capture_output=True, text=True, timeout=300)
    except (OSError, subprocess.SubprocessError) as error:
        print(f"(symbolization failed: {error})")
        return {}
    blocks = [b for b in proc.stdout.split("\n\n")]
    result = {}
    for (caller, _), block in zip(rvas, blocks):
        lines = [l for l in block.strip().splitlines() if l.strip()]
        frames = list(zip(lines[0::2], lines[1::2]))
        chosen = None
        for function, location in frames:
            if "vulkan" in location.lower() or function.startswith("vk::") or "VULKAN_HPP" in function:
                continue
            chosen = (function, location)
            break
        if chosen is None and frames:
            chosen = frames[-1]
        if chosen:
            function, location = chosen
            location = re.sub(r"^.*[\\/](src[\\/])", r"\1", location)
            result[caller] = f"{function} ({location})"
    return result


def pick_csv(path, capture):
    if os.path.isfile(path):
        return path, os.path.dirname(path) or "."
    files = glob.glob(os.path.join(path, "gpuops-*.csv"))
    files = [f for f in files if re.search(r"gpuops-\d+\.csv$", f)]
    if not files:
        sys.exit(f"no gpuops-<flip>.csv in {path}")
    if capture is not None:
        summary = os.path.join(path, "gpuops-summary.csv")
        flips = {}
        if os.path.exists(summary):
            with open(summary, newline="", encoding="utf-8", errors="replace") as f:
                for row in csv.DictReader(f):
                    flips[to_int(row["capture"])] = row["flip"]
        if capture not in flips:
            sys.exit(f"capture {capture} not in summary")
        return os.path.join(path, f"gpuops-{flips[capture]}.csv"), path
    files.sort(key=os.path.getmtime)
    return files[-1], path


def table(title, counter, counts, total, top, labels=None):
    print(f"\n== {title}")
    print(f"{'gpu_ms':>9} {'share':>6} {'count':>7}  key")
    for key, ns in counter.most_common(top):
        label = labels.get(key, "") if labels else ""
        suffix = f"  {label}" if label else ""
        print(f"{ms(ns):9.3f} {100.0 * ns / max(total, 1):5.1f}% {counts[key]:7d}  {key}{suffix}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("path")
    parser.add_argument("--top", type=int, default=25)
    parser.add_argument("--capture", type=int, default=None)
    parser.add_argument("--exe", default=None, help="kyty_emulator.exe (with its PDB) for caller symbols")
    args = parser.parse_args()

    csv_path, folder = pick_csv(args.path, args.capture)
    print(f"capture file: {csv_path}")
    with open(csv_path, newline="", encoding="utf-8", errors="replace") as f:
        rows = list(csv.DictReader(f))

    ns_by = {name: collections.Counter() for name in
             ("category", "kind", "site", "scope_site", "pipeline", "target", "caller")}
    count_by = {name: collections.Counter() for name in ns_by}
    barrier_sites = collections.Counter()
    transition_sites = collections.Counter()
    pass_sites = collections.Counter()
    end_sites = collections.Counter()
    cbs = set()
    gap_ns = 0
    total = 0
    unavailable = 0
    passes = set()
    draws = dispatches = barriers = transitions = 0

    for row in rows:
        kind = row["kind"]
        cbs.add(row["cb"])
        if kind == "cb_begin":
            gap_ns += to_int(row["delta_ns"])
            continue
        if row["delta_ns"] == "":
            unavailable += 1
        delta = to_int(row["delta_ns"])
        total += delta
        site, scope = row["site"], row["scope"]
        keys = {
            "category": row["category"],
            "kind": kind,
            "site": site,
            "scope_site": f"{scope}>{site}",
        }
        if row["category"] in ("draw", "dispatch"):
            keys["pipeline"] = row["shaders"] or "none"
        if row["rt_pass"] and kind not in ("begin_rendering", "end_rendering"):
            keys["target"] = (f"{row['rt_width']}x{row['rt_height']}x{row['rt_layers']} "
                              f"c{row['rt_colors']}:{row['rt_color0_format']} d:{row['rt_depth_format']}")
        if row["caller"]:
            keys["caller"] = row["caller"]
        for name, key in keys.items():
            ns_by[name][key] += delta
            count_by[name][key] += 1
        if row["category"] == "draw":
            draws += 1
        elif row["category"] == "dispatch":
            dispatches += 1
        elif row["category"] == "barrier":
            barriers += 1
            n = to_int(row["layout_transitions"])
            transitions += n
            barrier_sites[f"{scope}>{site}"] += 1
            transition_sites[f"{scope}>{site}"] += n
        elif kind == "begin_rendering":
            passes.add(row["rt_pass"])
            pass_sites[f"{scope}>{site}"] += 1
        elif kind == "end_rendering":
            end_sites[f"{scope}>{site}"] += 1

    ops = len(rows) - len(cbs)
    print(f"ops {ops}, command buffers {len(cbs)}, render passes {len(passes)}, barriers {barriers} "
          f"({transitions} layout transitions), draws {draws}, dispatches {dispatches}")
    print(f"op GPU total {ms(total):.3f} ms, gaps between command buffers {ms(gap_ns):.3f} ms, "
          f"ops without timestamp {unavailable}")

    summary = os.path.join(folder, "gpuops-summary.csv")
    if os.path.exists(summary):
        flip = re.search(r"gpuops-(\d+)\.csv$", csv_path)
        with open(summary, newline="", encoding="utf-8", errors="replace") as f:
            totals = [r for r in csv.DictReader(f)
                      if r["section"] == "totals" and flip and r["flip"] == flip.group(1)]
        if totals:
            print("summary totals: " + ", ".join(f"{r['key']}={r['count']}" for r in totals))

    labels = symbolize(args.exe, [k for k, _ in ns_by["caller"].most_common(args.top)])
    table("category", ns_by["category"], count_by["category"], total, args.top)
    table("op kind", ns_by["kind"], count_by["kind"], total, args.top)
    table("site (innermost KYTY_GPU_OP_SITE tag)", ns_by["site"], count_by["site"], total, args.top)
    table("scope>site", ns_by["scope_site"], count_by["scope_site"], total, args.top)
    table("pipeline (guest shader hashes)", ns_by["pipeline"], count_by["pipeline"], total, args.top)
    table("render target (ops inside passes)", ns_by["target"], count_by["target"], total, args.top)
    table("caller (vkCmd return address)", ns_by["caller"], count_by["caller"], total, args.top, labels)

    print("\n== barrier calls by scope>site (calls, layout transitions)")
    for key, n in barrier_sites.most_common(args.top):
        print(f"{n:7d} {transition_sites[key]:7d}  {key}")
    print("\n== render pass begins by scope>site")
    for key, n in pass_sites.most_common(args.top):
        print(f"{n:7d}  {key}")
    print("\n== render pass ends by scope>site (what broke / closed the pass)")
    for key, n in end_sites.most_common(args.top):
        print(f"{n:7d}  {key}")


if __name__ == "__main__":
    main()
