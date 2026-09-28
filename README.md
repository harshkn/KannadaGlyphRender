# KannadaGlyphRender — deliverables

On-device Kannada text shaping and rendering. No HarfBuzz at runtime —
every table here was derived once, offline, using HarfBuzz as a
reference oracle against Noto Sans Kannada, then baked into a binary
asset the C engine reads directly.

## Structure

```
src/                          Portable C engine (any board, any font)
  glyphrender.h/.c              generic 1-bit glyph-run blit renderer
  kannada_shape.h/.c             Kannada syllable clustering + shaping

fonts/NotoSansKannada/        Font-specific compiled asset
  kannada_shaping.kasset          binary shaping tables + glyph atlas (85.6 KB)
  kannada_shaping.kasset.atlasmap.json
                                   debug map: HarfBuzz glyph id -> local
                                   atlas index (used by the demo's
                                   comparison script, not needed on-device)

demo/                         Reference usage + verification
  demo_render.c                  renders text through the real engine
  compare_with_harfbuzz.py       verifies output against HarfBuzz + renders
                                  a side-by-side comparison image

comparison_results/           Sample verification output (see below)

FORMAT.md                     Binary format spec + design notes
```

## Building and running the demo

```bash
cd demo
cc -std=c99 -O2 -o demo_render demo_render.c ../src/glyphrender.c ../src/kannada_shape.c
python3 compare_with_harfbuzz.py "ನಿಮ್ಮ ಪಠ್ಯ ಇಲ್ಲಿ" 300 100 24
```

This shapes and renders the given text through the actual on-device
code path, then independently re-shapes the same text via HarfBuzz,
translates its glyph ids into this asset's local numbering via
`atlasmap.json`, and diffs the two glyph sequences exactly (not just
visually) — printing `EXACT MATCH` or `MISMATCH` and writing
`comparison.png`.

## Verified accuracy

Benchmarked against 137,429 real words from the ALAR Kannada-English
dictionary (the corpus and benchmark tooling are not part of this
repository): **99.999% exact glyph-sequence match
against HarfBuzz (137,427/137,429)** — both before and after compacting
the shaping tables down to only the entries that corpus actually uses.

## `comparison_results/`

Three worked examples, each showing the HarfBuzz reference (left) next
to this engine's actual output (right), both confirmed exact glyph-index
matches:

| File | Text | Exercises |
|---|---|---|
| `comparison_1_basic_reph.png` | ಕರ್ನಾಟಕ ಸುಂದರವಾಗಿದೆ | reph reordering (ರ್ನ) |
| `comparison_2_conjuncts.png` | ರಾಷ್ಟ್ರೀಯ ಶಿಕ್ಷಣ ವಿಜ್ಞಾನ | triple conjunct (ಷ್ಟ್ರ) + true ligatures (ಕ್ಷ, ಜ್ಞ) |
| `comparison_3_triple_conjunct.png` | ಸ್ವಾತಂತ್ರ್ಯ ಸಂಸ್ಕೃತಿ ಸಾಹಿತ್ಯ | 4-consonant chain, vocalic-r conjunct (ಸ್ಕೃ) |

## Known scope limits

See `FORMAT.md`'s "Known scope limits" and "Usage-driven compaction"
sections — in short: chains of 4+ consonants and a couple of
lookahead-dependent contextual variants outside the ALAR corpus's
vocabulary can fall back to an approximate (not table-exact) rendering.

## What this repository contains

Only the final deliverable: the C engine, the compiled shaping asset,
and the demo that verifies it. The offline tools that build the asset
(`tools/`, `sim/` and `research/` in `FORMAT.md`) and the corpora are not
included.

## License

The code and documentation are MIT-licensed (see `LICENSE`). The font
asset below is not covered by the MIT license.

### Font

The glyph atlas in `fonts/NotoSansKannada/` is rendered from
[Noto Sans Kannada](https://fonts.google.com/noto/specimen/Noto+Sans+Kannada),
© Google, licensed under the
[SIL Open Font License 1.1](https://openfontlicense.org).
