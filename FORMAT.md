# Glyph-run binary format (v1)

Produced by the server-side shaping tool (`tools/shape_text.py` prototype),
consumed by the device-side renderer (`src/glyphrender.c`). All integers are
little-endian. No further shaping logic is needed on the read side — every
position is already fully resolved.

```
Header (12 bytes):
  magic[4]        = "KGR1"
  glyph_count     : u16   number of distinct glyph bitmaps in the atlas
  line_count      : u16   number of text lines
  run_count       : u16   total glyph placements across all lines
  reserved        : u16   padding, must be 0

Glyph table (glyph_count entries, 12 bytes each):
  width           : u8    bitmap width in pixels
  height          : u8    bitmap height in pixels
  bearing_x       : i8    offset from pen x to bitmap left edge
  bearing_y       : i8    offset from baseline to bitmap top edge (+ = up)
  bitmap_offset   : u32   byte offset into the bitmap blob
  bitmap_size     : u32   bytes of packed bitmap data for this glyph

Bitmap blob (sum of bitmap_size across all glyphs):
  1-bit rows, MSB first, each row padded to a byte boundary
  (row stride = ceil(width / 8) bytes)

Line table (line_count entries, 8 bytes each):
  glyph_start     : u16   index into the glyph-run array where this line begins
  glyph_run_count : u16   number of glyph placements on this line
  baseline_y      : i16   absolute y coordinate of this line's baseline
  reserved        : i16   padding, must be 0

Glyph-run array (run_count entries, 6 bytes each):
  glyph_index     : u16   index into the glyph table
  pen_x           : i16   absolute x coordinate of the pen on this line
  reserved        : i16   padding, must be 0
```

Rendering a run: for each entry, blit `glyph_table[glyph_index]`'s bitmap so
its top-left pixel lands at
`(pen_x + bearing_x, baseline_y - bearing_y)`.

# Kannada shaping asset format (KSH9)

Produced offline by `tools/build_shaping_asset.py` from rule tables in
`tools/kannada_rules.json` (themselves derived once via `hb-shape` as a
reference oracle — see that script's docstring). Consumed at runtime by
`src/kannada_shape.c`, which does its own UTF-8 decoding, syllable
clustering, and reph/conjunct/matra table lookups — no HarfBuzz dependency
at render time. All integers little-endian.

```
Header (32 bytes):
  magic[4]               = "KSH9"
  glyph_count            : u16
  consonant_count        : u16
  matra_count            : u16
  conjunct_count         : u16
  simple_count           : u16
  conjunct_matra_count   : u16
  triple_tail_count      : u16
  triple_override_count  : u16
  reph_glyph_idx         : u16   atlas index of the reph mark glyph
  triple_matra_count     : u32   full uncompacted table, can exceed 65535
  conjunct_virama_count  : u16
  double_vowel_count     : u16
  long_chain_count       : u16

Glyph table (glyph_count entries, 14 bytes each):
  width, height   : u8, u8
  bearing_x/y     : i8, i8
  advance_x       : i16   pen advance for this glyph alone
  bitmap_offset   : u32
  bitmap_size     : u32

Bitmap blob: same packed 1-bit-row convention as KGR1.

Consonant table (consonant_count entries, 6 bytes each, sorted by codepoint):
  codepoint       : u16
  base_glyph_idx  : u16   plain consonant form (implicit 'a' vowel)
  virama_glyph_idx: u16   form when followed by an explicit bare virama

Matra table (matra_count entries, 13 bytes each, sorted by (consonant_cp, vowel_sign_cp)):
  consonant_cp    : u16
  vowel_sign_cp   : u16
  glyph_count     : u8    1-4
  glyph_idx[4]    : u16   unused trailing slots are 0xFFFF

Conjunct table (conjunct_count entries, 9 bytes each, sorted by (c1_cp, c2_cp)):
  c1_cp, c2_cp    : u16, u16   two consonants joined by virama
  glyph_count     : u8    1-2
  glyph_idx[2]    : u16

Simple table (simple_count entries, 9 bytes each, sorted by codepoint):
  independent vowels, digits, anusvara/visarga, avagraha, space, a few
  ASCII punctuation marks (all single-glyph, via direct cmap), and the 13
  standalone (unfused) vowel-sign forms (from the HarfBuzz oracle, since
  their in-context shaped form differs from the font's raw cmap glyph and
  can itself be multiple glyphs — used when a vowel sign has no consonant
  to fuse with, e.g. two consecutive vowel signs):
  codepoint       : u16
  glyph_count     : u8    1-3
  glyph_idx[3]    : u16   unused trailing slots are 0xFFFF

Conjunct+matra table (conjunct_matra_count entries, 17 bytes each,
sorted by (c1_cp, c2_cp, vowel_sign_cp)):
  c1_cp, c2_cp    : u16, u16   the two consonants of the conjunct
  vowel_sign_cp   : u16        the vowel sign attached to the conjunct
  glyph_count     : u8    1-5
  glyph_idx[5]    : u16   unused trailing slots are 0xFFFF

Triple-conjunct tail table (triple_tail_count entries, 9 bytes each,
sorted by (c2_cp, c3_cp)): a 3-consonant conjunct's trailing pair renders
the same way regardless of what leads the chain, in 1,155 of 1,225
possible pairs (94%) -- so instead of a full 35^3 matrix, this stores
just the trailing pair's fused "tail" glyphs. The device renders
base(c1) + tail(c2,c3):
  c2_cp, c3_cp    : u16, u16   the trailing two consonants of the triple
  glyph_count     : u8    1-2
  glyph_idx[2]    : u16

Triple-conjunct override table (triple_override_count entries, 13 bytes
each, sorted by (c1_cp, c2_cp, c3_cp)): the exceptions where the tail
genuinely depends on c1 too (concentrated around one consonant, ಞ/nya,
whose presence as c2 sometimes reshapes the leading glyph -- including
cases where c1+c2 itself forms an irregular ligature, e.g. ja+nya reusing
the jna glyph, so the head isn't base(c1) either). Stores the FULL
resolved glyph sequence, not a tail to append to base(c1):
  c1_cp, c2_cp, c3_cp : u16 x3   the three consonants of the conjunct
  glyph_count         : u8       1-3
  glyph_idx[3]        : u16      unused trailing slots are 0xFFFF

Triple-conjunct + vowel-sign table (triple_matra_count entries, 21 bytes
each, sorted by (c1_cp, c2_cp, c3_cp, vowel_sign_cp)): a 3-consonant
conjunct immediately followed by a vowel sign (e.g. ರಾಷ್ಟ್ರೀಯ). Verified
NOT decomposable from matra_table + the tail table above: naive
concatenation only matches 25.7% of sampled cases, and even allowing for
reordering, ~22% resolve to a genuinely different contextual glyph
variant that doesn't appear in either smaller table. Stored in full
(uncompacted) for now -- ~541K rows, since c1=ra is excluded on the same
grounds as the other triple tables. Revisit compaction once verified
correct end-to-end.
  c1_cp, c2_cp, c3_cp : u16 x3   the three consonants of the conjunct
  vowel_sign_cp       : u16      the vowel sign attached to the conjunct
  glyph_count         : u8       1-6
  glyph_idx[6]        : u16      unused trailing slots are 0xFFFF

Conjunct+virama table (conjunct_virama_count entries, 9 bytes each,
sorted by (c1_cp, c2_cp)): a 2-consonant conjunct immediately followed by
ANOTHER bare virama (word-final, no vowel at all — loanword endings like
"-st", "-ft", e.g. ಪೋಸ್ಟ್) uses a genuinely different leading-glyph
variant than the normal mid-word conjunct, analogous to how a single
consonant has a separate virama_form. Universal: verified all 1,225
pairs differ from the plain conjunct table.
  c1_cp, c2_cp    : u16, u16   two consonants joined by virama
  glyph_count     : u8    1-2
  glyph_idx[2]    : u16

Double-vowel-sign table (double_vowel_count entries, 13 bytes each, sorted
by (v1_cp, v2_cp)): two consecutive vowel signs (irregular, e.g. short-i
followed by long-ii). The combined TAIL is deduplicated and
consonant-independent (verified via oracle across 4 probe consonants) —
NOT a concatenation of each vowel's own standalone form. The device
renders matra_table[(consonant, v1)] (the consonant's own contextual
form when fused with the FIRST vowel sign — not its bare base form, e.g.
ka+i alone renders as glyph 205, not base ka's glyph 22) followed by this
table's tail. Covers 103 of 169 possible pairs where the tail is
verified stable; the rest fall back to concatenating each vowel's
standalone form via the simple table, which is imprecise but rare enough
not to be worth a full 35-consonant-dependent table.
  v1_cp, v2_cp    : u16, u16   the two consecutive vowel signs
  glyph_count     : u8    1-4
  glyph_idx[4]    : u16   unused trailing slots are 0xFFFF

Long-chain override table (long_chain_count entries, 22 bytes each, not
sorted — linear scan, only a handful of entries): exact answers for
specific 4-5 consonant chains that occur in the ALAR dictionary corpus
(research/corpora/alar_headwords.txt), via the oracle. A corpus-scoped
exception list, not a general 4-consonant table (35^4 is impractical) —
a chain outside this list still falls back to the approximate
per-consonant rendering.
  chain_cp[5]     : u16 x5   the consonant chain, unused trailing slots 0
  chain_len       : u8       2-5 (only 4-5 entries are actually present)
  glyph_count     : u8       1-5
  glyph_idx[5]    : u16      unused trailing slots are 0xFFFF
```

Rows with `c1_cp = ra` are never generated for any triple table: reph
detection always strips a leading `ra + virama` before triple lookup can
run, so those combinations are unreachable and would be dead weight.

## Usage-driven compaction

`tools/build_shaping_asset.py --prune-triple-matra <keys> --prune-conjunct-matra <keys>`
filters those two tables down to only the rows a given corpus actually
uses, instead of the full combinatorial matrix. The `sim/kannada_shaping.kasset`
shipped here was pruned against every word in `research/corpora/alar_headwords.txt`
(137,429 words after filtering out ones using out-of-scope archaic
letters), verified with zero regression on the full benchmark:

| | triple_matra rows | conjunct_matra rows | asset size | benchmark |
|---|---|---|---|---|
| full (uncompacted) | 541,450 | 15,925 | 11.70 MB | 137,427/137,429 (99.999%) |
| pruned to corpus usage | 127 | 1,302 | 85.6 KB | 137,427/137,429 (99.999%) — **identical mismatch set** |

This is a real tradeoff, not a free lunch: a combination absent from
`used_triple_matra_keys.txt` / `used_conjunct_matra_keys.txt` now falls
back to the documented approximate rendering (per-consonant virama form,
or the tail-table/simple-vowel fallback) instead of an exact table hit,
even though the full table would have had the exact answer. The
uncompacted asset is preserved as `sim/kannada_shaping_full.kasset` for
reference or to re-derive a differently-scoped pruned table.

To regenerate the usage key lists from a (possibly different) corpus:
build `sim/kannada_shaping_full.kasset` uncompacted, compile
`kannada_shape.c` with `-DKSH_TRACE_USAGE` (adds `fprintf(stderr, ...)`
calls at the `TRIPLE_MATRA`/`CONJ_MATRA` table-hit sites, zero-cost when
undefined), run `sim/bench_corpus` over every word in the target corpus,
and `grep`/`sort -u` the two log prefixes into key list files.

## Known scope limits (v9)

- **2-consonant conjuncts** (the common case, including true ligatures like
  ಕ್ಷ/ಜ್ಞ) are exact, table-driven, including when followed by a vowel
  sign (e.g. ಮತ್ತು, ಸ್ವಾತಂತ್ರ್ಯ) or a word-final bare virama (e.g. ಪೋಸ್ಟ್).
- **3-consonant conjuncts** (e.g. ಷ್ಟ್ರ), with or without a following vowel
  sign (e.g. ರಾಷ್ಟ್ರೀಯ), are exact, table-driven.
- **`ra + virama + {ra, vocalic-r/rr/l/ll}`** renders as a normal
  conjunct/cluster, not reph — verified via oracle, handled as a
  structural exception. Any other `ra+virama+consonant` triggers reph.
- **Two consecutive vowel signs** (an irregular sequence) skip matra
  fusion for the *pair* but still use the consonant's normal
  single-vowel-fused form for the first vowel sign — verified via oracle,
  covers 103/169 vowel-sign pairs exactly.
- **Specific 4-5 consonant chains observed in the ALAR corpus** are exact
  via the long-chain override table; any other 4+ chain falls back to
  showing each non-final consonant in its explicit-virama form —
  readable, not typographically exact.
- Benchmarked against 137K real dictionary words (see
  `research/corpora/`): 99.999% exact match against HarfBuzz (137,427/137,429)
  as of the last full run, identical before and after usage-driven
  compaction. The 2 remaining misses are lookahead-dependent contextual
  variants too narrow to generalize (see `research/corpora/mismatches_v6.tsv`
  for the last captured list — rerun the benchmark after table changes).
- The shipped asset is pruned to the ALAR corpus's actual usage (see
  "Usage-driven compaction" above) — combinations outside that corpus can
  still hit the approximate fallback even where the full table would have
  been exact.
