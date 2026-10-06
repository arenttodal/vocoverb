#!/usr/bin/env python3
"""Writes design/reference/measurements.json: reference identity (size, SHA-256), principal rectangles and
control centres used by the editor, text ink boxes (measure.py), sampled colour anchors and knob calibration."""
import hashlib, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
import measure

REF = "references/playable-ambience-approved-gui.png"
img = Image.open(REF).convert("RGB")
px = img.load()
sha = hashlib.sha256(open(REF, "rb").read()).hexdigest()
rects = {  # canvas px (x, y, w, h); refined against the PNG (see docs/GUI-Implementation-Note.md)
    "outerShell": [15, 15, 1506, 983], "delayCard": [22, 118, 743, 436], "reverbCard": [772, 118, 743, 436],
    "delayWell": [42, 170, 704, 215], "reverbWell": [791, 170, 704, 215], "routingStrip": [22, 563, 1493, 63],
    "harmonyCard": [22, 634, 1493, 225], "harmonyWell": [42, 686, 912, 160], "performanceCard": [22, 866, 1493, 121],
    "keybed": [150, 872, 968, 111], "presetBar": [527, 36, 451, 52], "abSegment": [1204, 45, 79, 34],
    "delayModeDropdown": [556, 130, 156, 34], "reverbModeDropdown": [1298, 130, 163, 34], "syncButton": [670, 182, 61, 28],
    "routeParallel": [255, 572, 136, 43], "routeDR": [411, 572, 144, 43], "routeRD": [576, 572, 145, 43],
    "placement": [994, 574, 379, 39], "chordBadge": [825, 646, 105, 33], "sourceDropdown": [1084, 645, 132, 35],
    "policyHold": [1232, 645, 62, 35], "policyRelease": [1302, 645, 64, 35], "policyAmbient": [1374, 645, 65, 35],
    "pitchWheelWell": [49, 877, 26, 78], "modWheelWell": [98, 877, 26, 78], "latch": [1148, 873, 136, 44],
    "freeze": [1292, 873, 141, 44], "wetOnly": [1148, 929, 136, 47], "advanced": [1292, 929, 141, 47],
}
knobs = {  # centre, face radius (px), readout field
    "delay": {"TIME": [105, 459], "FEEDBACK": [245, 459], "TONE": [388, 459], "AGE": [532, 459], "LEVEL": [675, 459]},
    "reverb": {"DECAY": [858, 459], "BLOOM": [1002, 459], "TONE": [1143, 459], "MOTION": [1288, 459], "LEVEL": [1433, 459]},
    "harmony": {"DEPTH": [1024, 768], "COLOUR": [1169, 768], "TRANSITION": [1298, 768], "DUCK": [1433, 768]},
    "header": {"DRY / WET": [1450, 55]},
    "faceRadius": {"engine": 34, "header": 29.5}, "readout": {"engine": [93, 30, "y 507..537"], "harmony": [93, 30, "y 813..841"]},
    "sweepDegrees": [-135, 135], "tickDots": {"count": 9, "anglesDeg": [-120, 120, 30], "radiusFactor": 1.235},
    "marker": {"fromRadius": 0.40, "toRadius": 0.855, "widthFactor": 0.104, "colour": "#F75A1C"},
    "calibrationNote": "Marker angles measured in the PNG fit a 270 degree sweep best (Feedback 42 % of 0..95 -> -15 deg, Age 28 % -> -64, Motion 30 % -> -55, Duck 24 % -> -68); some (Depth, Colour) are inconsistent in the raster and are not followed.",
}
samples = {k: list(px[x, y]) for k, (x, y) in {
    "shellUpper": (30, 30), "shellUnderHeader": (700, 105), "gapMid": (768, 300), "backdropBottom": (700, 1005),
    "cardTop": (300, 125), "cardMid": (300, 400), "cardBottom": (300, 550), "routingStrip": (300, 595), "harmonyCardTop": (980, 650),
    "wellUpper": (400, 200), "wellLower": (400, 370), "valueFieldFill": (245, 520), "valueFieldBorder": (199, 522),
    "dropdownFillTop": (600, 131), "dropdownBorder": (556, 147), "knobFaceCentre": (1288, 459), "knobFaceUpperLeft": (1273, 444),
    "knobFaceLowerRight": (1303, 474), "knobOutlineBottom": (1288, 493), "knobMarker": (1268, 445), "abSelected": (1225, 62),
    "whiteKey": (300, 960), "blackKey": (190, 900), "activeWhiteKey": (399, 940),
}.items()}
axes = {"delay": {"zeroX": 60.5, "pxPerSecond": 152, "centreY": 277.5, "labels": "0..4 s"},
        "reverb": {"zeroX": 810, "pxPerSecond": 78.1, "centreY": 280, "labels": "0..8 s (reference '8 s' label sits 31 px right of its grid line)"},
        "harmony": {"zeroX": 105, "pxPerSecond": 102.75, "laneCentresY": [727.5, 761, 794.5], "labels": "0..8 s (reference labels unevenly spaced)"}}
out = {"reference": {"file": "references/playable-ambience-approved-gui.png", "size": list(img.size), "sha256": sha, "mode": "8-bit sRGB",
                     "source": "approved design supplied in the conversation (WEBP 1536 x 1024, stored losslessly as PNG)"},
       "canvas": {"width": 1536, "height": 1024, "editorScale": "uniform s = window width / 1536 (standalone adds an 82 px source strip above)"},
       "rectangles": rects, "knobs": knobs, "textInk": measure.measure(REF), "colourSamples": samples, "axes": axes}
os.makedirs("design/reference", exist_ok=True)
json.dump(out, open("design/reference/measurements.json", "w"), indent=1)
print("wrote design/reference/measurements.json", sha)
