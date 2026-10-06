#!/usr/bin/env python3
"""Ink bounding boxes inside named regions of a 1536x1024 editor image (reference or capture).

Usage: measure.py IMAGE [--json]
Regions and ink rules are listed in REGIONS; 'dark' = text/icons on ivory, 'light' = labels on the dark wells,
'orange' = active accents. Used to build design/reference/measurements.json and to compare captures."""
import json, sys
from PIL import Image

# name: (x0, y0, x1, y1, ink)
REGIONS = {
    "title":            (40, 25, 470, 75, "dark"),
    "subtitle":         (40, 74, 420, 92, "mid"),
    "presetText":       (600, 48, 870, 76, "dark"),
    "delayTitle":       (70, 125, 200, 170, "dark"),
    "reverbTitle":      (820, 125, 960, 170, "dark"),
    "harmonyTitle":     (70, 642, 230, 684, "dark"),
    "lblTIME":          (60, 398, 150, 418, "dark"),
    "lblFEEDBACK":      (195, 398, 300, 418, "dark"),
    "valFeedback":      (205, 510, 290, 534, "dark"),
    "valTime":          (65, 510, 145, 534, "dark"),
    "valLevel":         (630, 510, 720, 534, "dark"),
    "lblDEPTH":         (980, 703, 1070, 722, "dark"),
    "valDepth":         (985, 815, 1065, 840, "dark"),
    "routingLbl":       (150, 585, 240, 606, "dark"),
    "placementLbl":     (860, 585, 970, 606, "dark"),
    "afterSpace":       (1000, 582, 1170, 604, "light"),
    "beforeSpace":      (1180, 582, 1370, 604, "dark"),
    "chordBadge":       (830, 652, 925, 674, "dark"),
    "sourceLbl":        (1005, 652, 1075, 674, "dark"),
    "sourceVal":        (1092, 652, 1160, 676, "dark"),
    "dryWetLbl":        (1405, 86, 1495, 102, "dark"),
    "dryWetVal":        (1420, 101, 1480, 116, "dark"),
    "latch":            (1160, 884, 1272, 906, "light"),
    "freeze":           (1305, 884, 1420, 906, "dark"),
    "wetOnly":          (1160, 941, 1272, 964, "dark"),
    "advanced":         (1340, 941, 1425, 964, "dark"),
    "pitchLbl":         (40, 960, 85, 976, "dark"),
    "modLbl":           (90, 960, 130, 976, "dark"),
    "delayAxis":        (50, 365, 740, 382, "light"),
    "reverbAxis":       (800, 365, 1490, 382, "light"),
    "harmAxis":         (90, 823, 950, 840, "light"),
    "harmNotes":        (55, 712, 85, 805, "light"),
}

def ink(px, x, y, kind):
    r, g, b = px[x, y][:3]
    if kind == "dark":   return r + g + b < 3 * 95
    if kind == "mid":    return r + g + b < 3 * 150
    if kind == "light":  return r + g + b > 3 * 200
    if kind == "orange": return r > 200 and g < 140 and b < 90
    return False

def bbox(img, region):
    x0, y0, x1, y1, kind = region
    px = img.load()
    xs, ys = [], []
    for y in range(y0, y1):
        for x in range(x0, x1):
            if ink(px, x, y, kind): xs.append(x); ys.append(y)
    if not xs: return None
    return [min(xs), min(ys), max(xs) + 1, max(ys) + 1]

def measure(path):
    img = Image.open(path).convert("RGB")
    return {k: bbox(img, r) for k, r in REGIONS.items()}

if __name__ == "__main__":
    res = measure(sys.argv[1])
    if "--json" in sys.argv: print(json.dumps(res, indent=1))
    else:
        for k, v in res.items(): print(f"{k:14s} {v}")
