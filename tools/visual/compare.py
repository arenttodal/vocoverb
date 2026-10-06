#!/usr/bin/env python3
"""Compares a native editor capture with the approved reference (both 1536 x 1024 canvas coordinates).

Writes, per region: side-by-side (reference | capture), 50 % overlay, amplified absolute difference, plus a full-editor
set and metrics.json (mean absolute difference per channel, edge agreement, vertical/horizontal luminance profiles).
Usage: compare.py REFERENCE CAPTURE OUTDIR [--scale N]  (scale = capture pixels per canvas pixel, default 1)"""
import json, os, sys
from PIL import Image, ImageChops, ImageFilter, ImageOps

REGIONS = {
    "full":            (0, 0, 1536, 1024),
    "header":          (15, 15, 1521, 118),
    "delay-card":      (22, 118, 765, 554),
    "reverb-card":     (772, 118, 1515, 554),
    "delay-graph":     (42, 170, 746, 385),
    "reverb-graph":    (791, 170, 1495, 385),
    "routing":         (22, 563, 1515, 626),
    "harmony-header":  (22, 634, 1515, 686),
    "harmony-graph":   (42, 686, 954, 846),
    "harmony-knobs":   (960, 690, 1510, 856),
    "keyboard-perf":   (22, 866, 1515, 987),
    "knob-feedback":   (190, 395, 300, 545),
    "knob-bloom":      (947, 395, 1057, 545),
    "knob-depth":      (969, 704, 1079, 854),
    "knob-drywet":     (1395, 18, 1505, 118),
    "dropdown-bbd":    (550, 125, 718, 170),
    "preset-bar":      (520, 30, 985, 95),
}

def lum(img):
    return img.convert("L")

def mad(a, b):
    d = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
    h = d.histogram()
    out = []
    for c in range(3):
        hist = h[c * 256:(c + 1) * 256]
        n = sum(hist) or 1
        out.append(sum(i * v for i, v in enumerate(hist)) / n)
    return out

def edge_agreement(a, b):
    ea = lum(a).filter(ImageFilter.FIND_EDGES).point(lambda v: 255 if v > 40 else 0)
    eb = lum(b).filter(ImageFilter.FIND_EDGES).point(lambda v: 255 if v > 40 else 0)
    # tolerate 1 px: dilate each and count overlap
    da, db = ea.filter(ImageFilter.MaxFilter(3)), eb.filter(ImageFilter.MaxFilter(3))
    pa = sum(1 for v in ea.getdata() if v); pb = sum(1 for v in eb.getdata() if v)
    hit_a = sum(1 for v, w in zip(ea.getdata(), db.getdata()) if v and w)
    hit_b = sum(1 for v, w in zip(eb.getdata(), da.getdata()) if v and w)
    prec = hit_b / pb if pb else 1.0
    rec = hit_a / pa if pa else 1.0
    return {"refEdgesMatched": round(rec, 3), "captureEdgesMatched": round(prec, 3)}

def profiles(img):
    L = lum(img)
    w, h = L.size
    px = L.load()
    col = [round(sum(px[x, y] for y in range(h)) / h, 1) for x in range(w)]
    row = [round(sum(px[x, y] for x in range(w)) / w, 1) for y in range(h)]
    return col, row

def main():
    ref_path, cap_path, out = sys.argv[1], sys.argv[2], sys.argv[3]
    scale = float(sys.argv[sys.argv.index("--scale") + 1]) if "--scale" in sys.argv else 1.0
    ref = Image.open(ref_path).convert("RGB")
    cap = Image.open(cap_path).convert("RGB")
    if scale != 1.0 or cap.size != ref.size:
        cap = cap.resize(ref.size, Image.LANCZOS)  # one recorded uniform conversion
    os.makedirs(out, exist_ok=True)
    metrics = {"reference": ref_path, "capture": cap_path, "captureScale": scale, "regions": {}}
    for name, box in REGIONS.items():
        a, b = ref.crop(box), cap.crop(box)
        w, h = a.size
        zoom = 1 if name == "full" else (3 if w < 200 else (2 if w < 800 else 1))
        A, B = a.resize((w * zoom, h * zoom), Image.LANCZOS), b.resize((w * zoom, h * zoom), Image.LANCZOS)
        side = Image.new("RGB", (A.width * 2 + 6, A.height), (255, 0, 255))
        side.paste(A, (0, 0)); side.paste(B, (A.width + 6, 0))
        side.save(os.path.join(out, f"{name}-side.png"))
        Image.blend(A, B, 0.5).save(os.path.join(out, f"{name}-overlay.png"))
        diff = ImageChops.difference(A, B)
        ImageOps.autocontrast(diff.convert("L"), cutoff=0.5).save(os.path.join(out, f"{name}-diff.png"))
        m = {"meanAbsDiffRGB": [round(v, 2) for v in mad(a, b)]}
        if name != "full":
            m.update(edge_agreement(a, b))
        if name.endswith("graph"):
            ca, ra = profiles(a); cb, rb = profiles(b)
            m["columnLumaMeanAbsDiff"] = round(sum(abs(x - y) for x, y in zip(ca, cb)) / len(ca), 2)
            m["rowLumaMeanAbsDiff"] = round(sum(abs(x - y) for x, y in zip(ra, rb)) / len(ra), 2)
        metrics["regions"][name] = m
    with open(os.path.join(out, "metrics.json"), "w") as f:
        json.dump(metrics, f, indent=1)
    for k, v in metrics["regions"].items():
        print(f"{k:16s} {v}")

if __name__ == "__main__":
    main()
