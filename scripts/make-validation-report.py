#!/usr/bin/env python3
"""Aggregates out/reports/* into out/reports/Validation-Report.md (tested / not performed / blocked kept distinct)."""
import argparse
import datetime
import os
import platform
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REP = os.path.join(ROOT, "out", "reports")


def read(name):
    p = os.path.join(REP, name)
    return open(p, encoding="utf-8", errors="replace").read() if os.path.exists(p) else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--platform", default=platform.system())
    a = ap.parse_args()
    os.makedirs(REP, exist_ok=True)
    out = []
    w = out.append
    w("# Playable Ambience 0.1.0 — Validation Report")
    w("")
    w(f"Generated {datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M UTC')} on {a.platform} ({platform.machine()}).")
    w("Status words: **PASS** = executed and passed; **FAIL** = executed and failed; **NOT PERFORMED** = not run in this")
    w("environment; **BLOCKED** = could not run for an external reason. Numerical tests do not establish that it sounds good.")
    w("")

    core = read("core-tests.txt")
    w("## 1. Core DSP / graph tests (headless, deterministic)")
    if core:
        tests = re.findall(r"^\[ (\s?OK\s?|FAIL) \] (.+?) \((\d+) ms\)", core, re.M)
        summary = re.search(r"(\d+) tests, (\d+) failed, (\d+) check failures", core)
        w("")
        w("| Test | Result | Time |")
        w("|---|---|---|")
        for res, name, ms in tests:
            w(f"| {name} | {'PASS' if res.strip() == 'OK' else 'FAIL'} | {ms} ms |")
        if summary:
            w("")
            w(f"**{summary.group(1)} tests, {summary.group(2)} failed, {summary.group(3)} check failures.**")
        metrics = re.findall(r"metric (\S+) = (\S+) ?(\S*)", core)
        if metrics:
            w("")
            w("<details><summary>Measured values (thresholds are defined in Tests/*.cpp before judging)</summary>")
            w("")
            w("| Metric | Value |")
            w("|---|---|")
            for k, v, u in metrics:
                w(f"| {k} | {v} {u} |")
            w("")
            w("</details>")
    else:
        w("NOT PERFORMED (no core-tests.txt)")
    w("")

    st = read("standalone-selftest.txt")
    w("## 2. Standalone self-test (real processor + editor, no audio device, no microphone prompt)")
    w("")
    w("```")
    w(st.strip() if st else "NOT PERFORMED")
    w("```")
    ex = read("extracted-selftest.txt")
    if ex:
        w("")
        w("Self-test of the app extracted from the release ZIP:")
        w("```")
        w(ex.strip())
        w("```")
    w("")

    w("## 3. Plugin wrapper validation")
    w("")
    mac = read("validation-macos.md")
    if mac:
        w(mac.replace("# macOS wrapper validation", "### macOS (auval, pluginval, architectures, signatures)"))
    else:
        w("macOS auval / pluginval: NOT PERFORMED in this environment (see the CI artefacts or run scripts/validate-macos.sh).")
    for f, title in (("pluginval-linux-vst3.txt", "pluginval on the Linux VST3 build")):
        pass
    lin = read("pluginval-linux-vst3.txt")
    if lin:
        res = "PASS" if "SUCCESS" in lin else "FAIL"
        lvl = re.search(r"strictness[- ]level (\d+)", lin, re.I)
        w("")
        w(f"- pluginval v1.0.4 on the **Linux** VST3 build: **{res}**" + (f" (strictness {lvl.group(1)})" if lvl else ""))
    san = read("sanitizer-tests.txt")
    if san:
        m = re.search(r"(\d+) tests, (\d+) failed", san)
        w(f"- Core tests under AddressSanitizer + UBSan: **{'PASS' if m and m.group(2) == '0' else 'FAIL'}**")
    w("")

    b = read("benchmark.txt")
    w("## 4. Benchmarks (separate harness, one engine instance)")
    w("")
    w("```")
    w(b.strip() if b else "NOT PERFORMED")
    w("```")
    w("")
    r = read("render.txt")
    w("## 5. Rendered audio comparisons")
    w("")
    w("```")
    w(r.strip() if r else "NOT PERFORMED")
    w("```")
    w("")
    w("## 6. Not performed (requires your machine, hosts or ears)")
    w("")
    for item in [
        "Ableton Live and Logic Pro host tests (MIDI routing, sidechain, automation, project reload) — NOT PERFORMED: no DAW available in the build environment. Setup steps are in Quick-Start.md.",
        "Live audio input through hardware and the macOS microphone-permission flow — NOT PERFORMED (never triggered during unattended validation by design).",
        "Subjective listening evaluation of all modes — NOT PERFORMED; this is the purpose of your feedback round.",
        "Developer ID signing and notarization — BLOCKED: no credentials available; bundles are ad-hoc signed local developer builds.",
        "CPU measurement on your target Mac — the CI/runner numbers above come from shared virtual machines; measure with Advanced > Diagnostics on your Mac.",
    ]:
        w(f"- {item}")
    w("")
    path = os.path.join(REP, "Validation-Report.md")
    open(path, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print(path)


if __name__ == "__main__":
    main()
