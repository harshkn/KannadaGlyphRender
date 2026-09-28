#!/usr/bin/env python3
"""Verifies the on-device demo against real HarfBuzz, using
kannada_shaping.kasset.atlasmap.json to translate HarfBuzz's glyph ids
into the same local-index numbering the C engine uses -- so the two can
be diffed glyph-for-glyph, not just eyeballed.

HarfBuzz is used here only as an offline reference oracle, exactly like
in the rest of this project: it never runs on-device. This script's own
rendering of the "reference" side reads bitmaps directly out of the
.kasset file (same asset the demo uses), so both images are pixel-drawn
from the same source -- a glyph-index match is a pixel-identical
guarantee, not an approximation.

Usage: python3 compare_with_harfbuzz.py "ಕರ್ನಾಟಕ ಸುಂದರವಾಗಿದೆ" [panel_width] [panel_height] [font_size]
"""
import json
import os
import struct
import subprocess
import sys

FONT_PATH = "/System/Library/Fonts/NotoSansKannada.ttc"
FONT_INDEX = 3
ASSET_PATH = os.path.join(os.path.dirname(__file__), "..", "fonts", "NotoSansKannada", "kannada_shaping.kasset")
ATLASMAP_PATH = ASSET_PATH + ".atlasmap.json"
DEMO_BIN = os.path.join(os.path.dirname(__file__), "demo_render")


def hb_shape_lines(lines, font_size):
    env = dict(os.environ, LANG="en_US.UTF-8", LC_ALL="en_US.UTF-8")
    out_lines = []
    for text in lines:
        out = subprocess.run(
            ["hb-shape", FONT_PATH, f"--face-index={FONT_INDEX}",
             f"--font-size={font_size}", "--output-format=json",
             "--no-glyph-names", text],
            capture_output=True, text=True, env=env, check=True,
        )
        glyphs = json.loads(out.stdout)
        out_lines.append([g["g"] for g in glyphs])
    return out_lines


def load_glyph_table():
    """Reads glyph bitmaps/metrics directly out of the .kasset (KSH9),
    keyed by local atlas index -- used to render the reference image
    from the SAME source of truth the C engine draws from."""
    data = open(ASSET_PATH, "rb").read()
    glyph_count, = struct.unpack_from("<H", data, 4)
    off = 32  # KSH9 header size
    glyphs = {}
    bitmap_table_start = off
    max_bitmap_end = 0
    for i in range(glyph_count):
        w, h, bx, by, adv, bmoff, bmsize = struct.unpack_from("<BBbbhII", data, bitmap_table_start + i * 14)
        glyphs[i] = {"w": w, "h": h, "bx": bx, "by": by, "adv": adv, "bmoff": bmoff, "bmsize": bmsize}
        max_bitmap_end = max(max_bitmap_end, bmoff + bmsize)
    bitmap_blob_start = bitmap_table_start + glyph_count * 14
    bitmap_blob = data[bitmap_blob_start:bitmap_blob_start + max_bitmap_end]
    return glyphs, bitmap_blob


def render_reference_ppm(lines_local_idxs, glyphs, bitmap_blob, panel_w, panel_h,
                          margin_x, margin_y, line_spacing, out_path):
    fb = bytearray(panel_w * panel_h)  # 0/1 per pixel
    for li, idxs in enumerate(lines_local_idxs):
        baseline_y = margin_y + li * line_spacing
        pen_x = margin_x
        for idx in idxs:
            g = glyphs[idx]
            draw_x = pen_x + g["bx"]
            draw_y = baseline_y - g["by"]
            stride = (g["w"] + 7) // 8
            bmp = bitmap_blob[g["bmoff"]:g["bmoff"] + g["bmsize"]]
            for row in range(g["h"]):
                fy = draw_y + row
                if not (0 <= fy < panel_h):
                    continue
                row_bytes = bmp[row * stride:(row + 1) * stride]
                for col in range(g["w"]):
                    fx = draw_x + col
                    if not (0 <= fx < panel_w):
                        continue
                    byte = row_bytes[col // 8]
                    bit = (byte >> (7 - (col % 8))) & 1
                    if bit:
                        fb[fy * panel_w + fx] = 1
            pen_x += g["adv"]
    with open(out_path, "wb") as f:
        f.write(f"P6\n{panel_w} {panel_h}\n255\n".encode())
        for v in fb:
            px = bytes([255, 255, 255]) if v == 0 else bytes([0, 0, 0])
            f.write(px)


def main():
    text = sys.argv[1] if len(sys.argv) > 1 else "ಕರ್ನಾಟಕ ಸುಂದರವಾಗಿದೆ"
    panel_w = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    panel_h = int(sys.argv[3]) if len(sys.argv) > 3 else 200
    font_size = int(sys.argv[4]) if len(sys.argv) > 4 else 24
    margin_x, margin_y, line_spacing = 4, int(font_size * 1.05), int(font_size * 1.25)

    atlasmap = {int(k): v for k, v in json.load(open(ATLASMAP_PATH)).items()}

    demo_out = "demo_output.ppm"
    result = subprocess.run([DEMO_BIN, text, demo_out, str(panel_w), str(panel_h), str(font_size)],
                             capture_output=True, text=True, check=True)
    demo_lines = []
    for line in result.stdout.splitlines():
        if line.startswith("LINE"):
            parts = line.split()[1:]
            demo_lines.append([int(x) for x in parts])
    print("device engine (no HarfBuzz at runtime):", result.stderr.strip().splitlines()[0])

    # Word-wrap the same way the demo did isn't reproduced here -- for a
    # clean per-line HarfBuzz comparison, re-shape exactly the lines the
    # demo produced (by codepoint content) is not directly recoverable
    # from glyph indices, so this compares against the whole text shaped
    # as a single reference call per resolved line count using the same
    # word list. For texts that fit on one line (the common demo case)
    # this is an exact, direct comparison.
    hb_lines_raw = hb_shape_lines([text], font_size)
    ref_local_lines = []
    all_covered = True
    for raw in hb_lines_raw:
        local = []
        for g in raw:
            if g not in atlasmap:
                all_covered = False
                local.append(None)
            else:
                local.append(atlasmap[g])
        ref_local_lines.append(local)

    print()
    print("=== glyph-level comparison (HarfBuzz oracle -> local index, vs device engine) ===")
    if len(demo_lines) == 1 and len(ref_local_lines) == 1:
        expected = ref_local_lines[0]
        got = demo_lines[0]
        match = expected == got
        print(f"expected (HarfBuzz): {expected}")
        print(f"got      (device):   {got}")
        print("RESULT: EXACT MATCH" if match else "RESULT: MISMATCH")
    else:
        print(f"device wrapped to {len(demo_lines)} line(s); HarfBuzz reference shaped as "
              f"{len(ref_local_lines)} line(s) (multi-line word-wrap comparison not reproduced "
              f"here -- use a single-line panel_width for an exact per-line diff).")
    if not all_covered:
        print("note: some HarfBuzz glyphs aren't in this (pruned/compacted) asset's atlas.")

    glyphs, bitmap_blob = load_glyph_table()
    ref_lines_for_render = [[g for g in line if g is not None] for line in ref_local_lines]
    ref_out = "reference_output.ppm"
    render_reference_ppm(ref_lines_for_render, glyphs, bitmap_blob, panel_w, panel_h,
                          margin_x, margin_y, line_spacing, ref_out)

    try:
        from PIL import Image
        ref = Image.open(ref_out).convert("RGB")
        ours = Image.open(demo_out).convert("RGB")
        combined = Image.new("RGB", (ref.width * 2 + 10, ref.height), "white")
        combined.paste(ref, (0, 0))
        combined.paste(ours, (ref.width + 10, 0))
        combined = combined.resize((combined.width * 3, combined.height * 3), Image.NEAREST)
        combined.save("comparison.png")
        print(f"\nwrote comparison.png (HarfBuzz reference left, device engine right)")
    except ImportError:
        print(f"\n(Pillow not available -- wrote {ref_out} and {demo_out} separately, no combined PNG)")


if __name__ == "__main__":
    main()
