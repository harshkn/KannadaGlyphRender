#include "kannada_shape.h"
#include <stdlib.h>
#include <string.h>
#ifdef KSH_TRACE_USAGE
#include <stdio.h>
#endif

#define KSH_HEADER_SIZE 32
#define KSH_GLYPH_STRIDE 14
#define KSH_CONSONANT_STRIDE 6
#define KSH_MATRA_STRIDE 13
#define KSH_CONJUNCT_STRIDE 9
#define KSH_SIMPLE_STRIDE 9
#define KSH_CONJUNCT_MATRA_STRIDE 17
#define KSH_TRIPLE_TAIL_STRIDE 9
#define KSH_TRIPLE_OVERRIDE_STRIDE 13
#define KSH_TRIPLE_MATRA_STRIDE 21
#define KSH_CONJUNCT_VIRAMA_STRIDE 9
#define KSH_DOUBLE_VOWEL_STRIDE 13
#define KSH_LONG_CHAIN_STRIDE 22

#define KN_VIRAMA 0x0CCDu
#define KN_RA 0x0CB0u
#define KN_ANUSVARA 0x0C82u
#define KN_VISARGA 0x0C83u
#define KN_VOCALIC_R 0x0C8Bu
#define KN_VOCALIC_RR 0x0CE0u
#define KN_VOCALIC_L 0x0C8Cu
#define KN_VOCALIC_LL 0x0CE1u
#define MAX_CHAIN 8
#define MAX_LINE_GLYPHS 256
#define MAX_WORDS 256
#define MAX_LINES 64

static uint16_t rd_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static int16_t rd_i16(const uint8_t *p) { return (int16_t)rd_u16(p); }
static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int kshape_load(const uint8_t *data, size_t size, kshape_asset_t *asset) {
    if (!data || !asset || size < KSH_HEADER_SIZE) return -1;
    if (data[0] != 'K' || data[1] != 'S' || data[2] != 'H' || data[3] != '9') return -1;

    uint16_t glyph_count = rd_u16(data + 4);
    uint16_t consonant_count = rd_u16(data + 6);
    uint16_t matra_count = rd_u16(data + 8);
    uint16_t conjunct_count = rd_u16(data + 10);
    uint16_t simple_count = rd_u16(data + 12);
    uint16_t conjunct_matra_count = rd_u16(data + 14);
    uint16_t triple_tail_count = rd_u16(data + 16);
    uint16_t triple_override_count = rd_u16(data + 18);
    uint16_t reph_idx = rd_u16(data + 20);
    uint32_t triple_matra_count = rd_u32(data + 22);
    uint16_t conjunct_virama_count = rd_u16(data + 26);
    uint16_t double_vowel_count = rd_u16(data + 28);
    uint16_t long_chain_count = rd_u16(data + 30);

    size_t off = KSH_HEADER_SIZE;
    size_t glyph_bytes = (size_t)glyph_count * KSH_GLYPH_STRIDE;
    if (off + glyph_bytes > size) return -1;
    const uint8_t *glyph_table_raw = data + off;
    off += glyph_bytes;

    uint32_t bitmap_blob_len = 0;
    for (uint16_t i = 0; i < glyph_count; i++) {
        const uint8_t *e = glyph_table_raw + (size_t)i * KSH_GLYPH_STRIDE;
        uint32_t bo = rd_u32(e + 6);
        uint32_t bs = rd_u32(e + 10);
        if (bo + bs > bitmap_blob_len) bitmap_blob_len = bo + bs;
    }
    if (off + bitmap_blob_len > size) return -1;
    const uint8_t *bitmap_blob = data + off;
    off += bitmap_blob_len;

    size_t consonant_bytes = (size_t)consonant_count * KSH_CONSONANT_STRIDE;
    if (off + consonant_bytes > size) return -1;
    const uint8_t *consonant_table_raw = data + off;
    off += consonant_bytes;

    size_t matra_bytes = (size_t)matra_count * KSH_MATRA_STRIDE;
    if (off + matra_bytes > size) return -1;
    const uint8_t *matra_table_raw = data + off;
    off += matra_bytes;

    size_t conjunct_bytes = (size_t)conjunct_count * KSH_CONJUNCT_STRIDE;
    if (off + conjunct_bytes > size) return -1;
    const uint8_t *conjunct_table_raw = data + off;
    off += conjunct_bytes;

    size_t simple_bytes = (size_t)simple_count * KSH_SIMPLE_STRIDE;
    if (off + simple_bytes > size) return -1;
    const uint8_t *simple_table_raw = data + off;
    off += simple_bytes;

    size_t conjunct_matra_bytes = (size_t)conjunct_matra_count * KSH_CONJUNCT_MATRA_STRIDE;
    if (off + conjunct_matra_bytes > size) return -1;
    const uint8_t *conjunct_matra_table_raw = data + off;
    off += conjunct_matra_bytes;

    size_t triple_tail_bytes = (size_t)triple_tail_count * KSH_TRIPLE_TAIL_STRIDE;
    if (off + triple_tail_bytes > size) return -1;
    const uint8_t *triple_tail_table_raw = data + off;
    off += triple_tail_bytes;

    size_t triple_override_bytes = (size_t)triple_override_count * KSH_TRIPLE_OVERRIDE_STRIDE;
    if (off + triple_override_bytes > size) return -1;
    const uint8_t *triple_override_table_raw = data + off;
    off += triple_override_bytes;

    size_t triple_matra_bytes = (size_t)triple_matra_count * KSH_TRIPLE_MATRA_STRIDE;
    if (off + triple_matra_bytes > size) return -1;
    const uint8_t *triple_matra_table_raw = data + off;
    off += triple_matra_bytes;

    size_t conjunct_virama_bytes = (size_t)conjunct_virama_count * KSH_CONJUNCT_VIRAMA_STRIDE;
    if (off + conjunct_virama_bytes > size) return -1;
    const uint8_t *conjunct_virama_table_raw = data + off;
    off += conjunct_virama_bytes;

    size_t double_vowel_bytes = (size_t)double_vowel_count * KSH_DOUBLE_VOWEL_STRIDE;
    if (off + double_vowel_bytes > size) return -1;
    const uint8_t *double_vowel_table_raw = data + off;
    off += double_vowel_bytes;

    size_t long_chain_bytes = (size_t)long_chain_count * KSH_LONG_CHAIN_STRIDE;
    if (off + long_chain_bytes > size) return -1;
    const uint8_t *long_chain_table_raw = data + off;

    asset->data = data;
    asset->size = size;
    asset->glyph_count = glyph_count;
    asset->consonant_count = consonant_count;
    asset->matra_count = matra_count;
    asset->conjunct_count = conjunct_count;
    asset->simple_count = simple_count;
    asset->conjunct_matra_count = conjunct_matra_count;
    asset->triple_tail_count = triple_tail_count;
    asset->triple_override_count = triple_override_count;
    asset->reph_idx = reph_idx;
    asset->triple_matra_count = triple_matra_count;
    asset->conjunct_virama_count = conjunct_virama_count;
    asset->double_vowel_count = double_vowel_count;
    asset->long_chain_count = long_chain_count;
    asset->glyph_table_raw = glyph_table_raw;
    asset->bitmap_blob = bitmap_blob;
    asset->consonant_table_raw = consonant_table_raw;
    asset->matra_table_raw = matra_table_raw;
    asset->conjunct_table_raw = conjunct_table_raw;
    asset->simple_table_raw = simple_table_raw;
    asset->conjunct_matra_table_raw = conjunct_matra_table_raw;
    asset->triple_tail_table_raw = triple_tail_table_raw;
    asset->triple_override_table_raw = triple_override_table_raw;
    asset->triple_matra_table_raw = triple_matra_table_raw;
    asset->conjunct_virama_table_raw = conjunct_virama_table_raw;
    asset->double_vowel_table_raw = double_vowel_table_raw;
    asset->long_chain_table_raw = long_chain_table_raw;
    return 0;
}

/* --- table lookups (binary search; all tables sorted ascending by key) --- */

static const uint8_t *find_consonant(const kshape_asset_t *a, uint32_t cp) {
    int lo = 0, hi = (int)a->consonant_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->consonant_table_raw + (size_t)mid * KSH_CONSONANT_STRIDE;
        uint16_t key = rd_u16(row);
        if (key == cp) return row;
        if (key < cp) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static const uint8_t *find_simple(const kshape_asset_t *a, uint32_t cp) {
    int lo = 0, hi = (int)a->simple_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->simple_table_raw + (size_t)mid * KSH_SIMPLE_STRIDE;
        uint16_t key = rd_u16(row);
        if (key == cp) return row;
        if (key < cp) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static int cmp_pair(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1) {
    if (a0 != b0) return a0 < b0 ? -1 : 1;
    if (a1 != b1) return a1 < b1 ? -1 : 1;
    return 0;
}

static const uint8_t *find_matra(const kshape_asset_t *a, uint32_t c_cp, uint32_t v_cp) {
    int lo = 0, hi = (int)a->matra_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->matra_table_raw + (size_t)mid * KSH_MATRA_STRIDE;
        int c = cmp_pair(rd_u16(row), rd_u16(row + 2), c_cp, v_cp);
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static const uint8_t *find_conjunct(const kshape_asset_t *a, uint32_t c1_cp, uint32_t c2_cp) {
    int lo = 0, hi = (int)a->conjunct_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->conjunct_table_raw + (size_t)mid * KSH_CONJUNCT_STRIDE;
        int c = cmp_pair(rd_u16(row), rd_u16(row + 2), c1_cp, c2_cp);
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* Word-final variant: conjunct immediately followed by another bare
 * virama (no vowel anywhere). Universally different from the plain
 * conjunct table -- verified all 1,225 pairs differ. */
static const uint8_t *find_conjunct_virama(const kshape_asset_t *a, uint32_t c1_cp, uint32_t c2_cp) {
    int lo = 0, hi = (int)a->conjunct_virama_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->conjunct_virama_table_raw + (size_t)mid * KSH_CONJUNCT_VIRAMA_STRIDE;
        int c = cmp_pair(rd_u16(row), rd_u16(row + 2), c1_cp, c2_cp);
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* Two consecutive vowel signs: the combined result is deduplicated and
 * consonant-independent for this subset (verified via oracle) -- NOT a
 * concatenation of each vowel's own standalone form. */
static const uint8_t *find_double_vowel(const kshape_asset_t *a, uint32_t v1_cp, uint32_t v2_cp) {
    int lo = 0, hi = (int)a->double_vowel_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->double_vowel_table_raw + (size_t)mid * KSH_DOUBLE_VOWEL_STRIDE;
        int c = cmp_pair(rd_u16(row), rd_u16(row + 2), v1_cp, v2_cp);
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* Corpus-scoped 4-5 consonant chain overrides -- only a handful of
 * entries, so a linear scan is fine (no need to sort/binary-search). */
static const uint8_t *find_long_chain(const kshape_asset_t *a, const uint32_t *chain, int chain_n) {
    for (uint16_t i = 0; i < a->long_chain_count; i++) {
        const uint8_t *row = a->long_chain_table_raw + (size_t)i * KSH_LONG_CHAIN_STRIDE;
        if (row[10] != (uint8_t)chain_n) continue;
        int match = 1;
        for (int k = 0; k < chain_n; k++) {
            if (rd_u16(row + k * 2) != chain[k]) { match = 0; break; }
        }
        if (match) return row;
    }
    return NULL;
}

static int cmp_triple(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t b0, uint32_t b1, uint32_t b2) {
    if (a0 != b0) return a0 < b0 ? -1 : 1;
    if (a1 != b1) return a1 < b1 ? -1 : 1;
    if (a2 != b2) return a2 < b2 ? -1 : 1;
    return 0;
}

static const uint8_t *find_conjunct_matra(const kshape_asset_t *a, uint32_t c1_cp, uint32_t c2_cp, uint32_t v_cp) {
    int lo = 0, hi = (int)a->conjunct_matra_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->conjunct_matra_table_raw + (size_t)mid * KSH_CONJUNCT_MATRA_STRIDE;
        int c = cmp_triple(rd_u16(row), rd_u16(row + 2), rd_u16(row + 4), c1_cp, c2_cp, v_cp);
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* Exceptions where the trailing pair's rendering depends on c1 too (rare,
 * concentrated around one consonant) -- checked before the tail table. */
static const uint8_t *find_triple_override(const kshape_asset_t *a, uint32_t c1_cp, uint32_t c2_cp, uint32_t c3_cp) {
    int lo = 0, hi = (int)a->triple_override_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->triple_override_table_raw + (size_t)mid * KSH_TRIPLE_OVERRIDE_STRIDE;
        int c = cmp_triple(rd_u16(row), rd_u16(row + 2), rd_u16(row + 4), c1_cp, c2_cp, c3_cp);
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* Full 3-consonant-conjunct + vowel-sign lookup, exact and uncompacted --
 * verified this does not decompose into matra(c1,v) + tail(c2,c3), see
 * FORMAT.md. Stores the complete resolved glyph sequence. */
static const uint8_t *find_triple_matra(const kshape_asset_t *a, uint32_t c1_cp, uint32_t c2_cp,
                                         uint32_t c3_cp, uint32_t v_cp) {
    int64_t lo = 0, hi = (int64_t)a->triple_matra_count - 1;
    while (lo <= hi) {
        int64_t mid = (lo + hi) / 2;
        const uint8_t *row = a->triple_matra_table_raw + (size_t)mid * KSH_TRIPLE_MATRA_STRIDE;
        int c = cmp_triple(rd_u16(row), rd_u16(row + 2), rd_u16(row + 4), c1_cp, c2_cp, c3_cp);
        if (c == 0) c = (rd_u16(row + 6) < v_cp) ? -1 : (rd_u16(row + 6) > v_cp) ? 1 : 0;
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* The trailing pair's fused tail, independent of c1 in the common case. */
static const uint8_t *find_triple_tail(const kshape_asset_t *a, uint32_t c2_cp, uint32_t c3_cp) {
    int lo = 0, hi = (int)a->triple_tail_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *row = a->triple_tail_table_raw + (size_t)mid * KSH_TRIPLE_TAIL_STRIDE;
        int c = cmp_pair(rd_u16(row), rd_u16(row + 2), c2_cp, c3_cp);
        if (c == 0) return row;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static int is_vowel_sign(uint32_t cp) {
    switch (cp) {
        case 0x0CBE: case 0x0CBF: case 0x0CC0: case 0x0CC1: case 0x0CC2:
        case 0x0CC3: case 0x0CC4: case 0x0CC6: case 0x0CC7: case 0x0CC8:
        case 0x0CCA: case 0x0CCB: case 0x0CCC:
            return 1;
        default:
            return 0;
    }
}

/* Independent vocalic vowel letters (RRA/RR/L/LL) trigger reph exactly
 * like consonants when following ra+virama -- verified via oracle. */
static int is_vocalic_letter(uint32_t cp) {
    return cp == KN_VOCALIC_R || cp == KN_VOCALIC_RR || cp == KN_VOCALIC_L || cp == KN_VOCALIC_LL;
}

/* --- shaping --- */

typedef struct { uint16_t glyph_idx; int pen_x; } shaped_glyph_t;

static void emit_idx(const kshape_asset_t *a, shaped_glyph_t *out, int *count, int max_out,
                      int *pen, uint16_t idx) {
    if (*count < max_out) {
        out[*count].glyph_idx = idx;
        out[*count].pen_x = *pen;
        (*count)++;
    }
    const uint8_t *g = a->glyph_table_raw + (size_t)idx * KSH_GLYPH_STRIDE;
    *pen += rd_i16(g + 4);
}

static void emit_row_glyphs(const kshape_asset_t *a, shaped_glyph_t *out, int *count, int max_out,
                             int *pen, const uint8_t *row, int count_off, int idx_off) {
    uint8_t n = row[count_off];
    for (int k = 0; k < n; k++) {
        uint16_t idx = rd_u16(row + idx_off + k * 2);
        emit_idx(a, out, count, max_out, pen, idx);
    }
}

/* Shapes one already-word-wrapped line of codepoints. Returns final pen
 * width; writes up to max_out glyphs to out, actual count to *out_count. */
static int shape_codepoints(const kshape_asset_t *a, const uint32_t *cps, int n,
                             shaped_glyph_t *out, int max_out, int *out_count) {
    int i = 0, pen = 0, count = 0;

    while (i < n) {
        uint32_t cp = cps[i];

        if (cp == ' ') {
            const uint8_t *srow = find_simple(a, cp);
            if (srow) emit_row_glyphs(a, out, &count, max_out, &pen, srow, 2, 3);
            i++;
            continue;
        }

        int use_reph = 0;
        /* ra+virama+ra (geminated ra) renders as a normal conjunct, not
         * reph -- confirmed via HarfBuzz oracle, matches conjunct_table.
         * Independent vocalic letters (ऋ-type) trigger reph exactly like
         * consonants -- also confirmed via oracle. */
        if (cp == KN_RA && i + 2 < n && cps[i + 1] == KN_VIRAMA && cps[i + 2] != KN_RA
            && (find_consonant(a, cps[i + 2]) || is_vocalic_letter(cps[i + 2]))) {
            use_reph = 1;
            i += 2;
            cp = cps[i];
        }

        if (!find_consonant(a, cp)) {
            const uint8_t *srow = find_simple(a, cp);
            if (srow) emit_row_glyphs(a, out, &count, max_out, &pen, srow, 2, 3);
            i++;
            if (use_reph) emit_idx(a, out, &count, max_out, &pen, a->reph_idx);
            continue;
        }

        uint32_t chain[MAX_CHAIN];
        int chain_n = 0;
        chain[chain_n++] = cp;
        i++;
        int j = i;
        while (j + 1 < n && cps[j] == KN_VIRAMA && find_consonant(a, cps[j + 1]) && chain_n < MAX_CHAIN) {
            chain[chain_n++] = cps[j + 1];
            j += 2;
        }
        i = j;

        uint32_t vowel_cp = 0;
        uint32_t extra_vowel_cp = 0; /* second of two consecutive vowel signs */
        if (i < n && is_vowel_sign(cps[i])) {
            vowel_cp = cps[i];
            i++;
            if (i < n && is_vowel_sign(cps[i])) {
                /* Two consecutive vowel signs is irregular -- confirmed via
                 * HarfBuzz oracle that matra fusion is skipped entirely:
                 * the consonant renders in its bare base form and both
                 * vowel signs render as their own standalone glyphs. */
                extra_vowel_cp = cps[i];
                i++;
            }
        }
        int fuse_vowel = vowel_cp && !extra_vowel_cp;

        /* Two consecutive vowel signs after a single consonant: the
         * combined result is matra_table[(consonant,vowel_cp)] (the
         * consonant's own contextual variant when fused with the FIRST
         * vowel sign -- NOT its bare base form) followed by the
         * consonant-independent tail for this vowel pair. Verified via
         * oracle; only handled for chain_n==1, matching every observed
         * real-word case. */
        int handled_double_vowel = 0;
        if (chain_n == 1 && extra_vowel_cp) {
            const uint8_t *dvrow = find_double_vowel(a, vowel_cp, extra_vowel_cp);
            if (dvrow) {
                const uint8_t *mrow = find_matra(a, chain[0], vowel_cp);
                if (mrow) {
                    emit_row_glyphs(a, out, &count, max_out, &pen, mrow, 4, 5);
                } else {
                    const uint8_t *crow = find_consonant(a, chain[0]);
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(crow + 2));
                }
                emit_row_glyphs(a, out, &count, max_out, &pen, dvrow, 4, 5);
                handled_double_vowel = 1;
            }
        }

        if (handled_double_vowel) {
            /* fully emitted above */
        } else if (chain_n == 1) {
            if (fuse_vowel) {
                const uint8_t *mrow = find_matra(a, chain[0], vowel_cp);
                if (mrow) {
                    emit_row_glyphs(a, out, &count, max_out, &pen, mrow, 4, 5);
                } else {
                    const uint8_t *crow = find_consonant(a, chain[0]);
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(crow + 2));
                    const uint8_t *vrow = find_simple(a, vowel_cp);
                    if (vrow) emit_row_glyphs(a, out, &count, max_out, &pen, vrow, 2, 3);
                }
            } else if (i < n && cps[i] == KN_VIRAMA && (i + 1 >= n || !find_consonant(a, cps[i + 1]))) {
                const uint8_t *crow = find_consonant(a, chain[0]);
                emit_idx(a, out, &count, max_out, &pen, rd_u16(crow + 4));
                i++;
            } else {
                const uint8_t *crow = find_consonant(a, chain[0]);
                emit_idx(a, out, &count, max_out, &pen, rd_u16(crow + 2));
            }
        } else if (chain_n == 2) {
            if (fuse_vowel) {
                const uint8_t *cmrow = find_conjunct_matra(a, chain[0], chain[1], vowel_cp);
                if (cmrow) {
#ifdef KSH_TRACE_USAGE
                    fprintf(stderr, "CONJ_MATRA %u,%u,%u\n", chain[0], chain[1], vowel_cp);
#endif
                    emit_row_glyphs(a, out, &count, max_out, &pen, cmrow, 6, 7);
                } else {
                    const uint8_t *crow2 = find_conjunct(a, chain[0], chain[1]);
                    if (crow2) {
                        emit_row_glyphs(a, out, &count, max_out, &pen, crow2, 4, 5);
                    } else {
                        const uint8_t *c0 = find_consonant(a, chain[0]);
                        const uint8_t *c1 = find_consonant(a, chain[1]);
                        emit_idx(a, out, &count, max_out, &pen, rd_u16(c0 + 4));
                        emit_idx(a, out, &count, max_out, &pen, rd_u16(c1 + 2));
                    }
                    const uint8_t *vrow = find_simple(a, vowel_cp);
                    if (vrow) emit_row_glyphs(a, out, &count, max_out, &pen, vrow, 2, 3);
                }
            } else if (i < n && cps[i] == KN_VIRAMA && (i + 1 >= n || !find_consonant(a, cps[i + 1]))) {
                /* Trailing bare virama, no vowel anywhere: word-final
                 * variant, distinct from the mid-word conjunct form. */
                const uint8_t *cvrow = find_conjunct_virama(a, chain[0], chain[1]);
                if (cvrow) {
                    emit_row_glyphs(a, out, &count, max_out, &pen, cvrow, 4, 5);
                } else {
                    const uint8_t *crow2 = find_conjunct(a, chain[0], chain[1]);
                    if (crow2) emit_row_glyphs(a, out, &count, max_out, &pen, crow2, 4, 5);
                }
                i++;
            } else {
                const uint8_t *crow2 = find_conjunct(a, chain[0], chain[1]);
                if (crow2) {
                    emit_row_glyphs(a, out, &count, max_out, &pen, crow2, 4, 5);
                } else {
                    const uint8_t *c0 = find_consonant(a, chain[0]);
                    const uint8_t *c1 = find_consonant(a, chain[1]);
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(c0 + 4));
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(c1 + 2));
                }
            }
        } else if (chain_n == 3) {
            const uint8_t *mrow3 = fuse_vowel ? find_triple_matra(a, chain[0], chain[1], chain[2], vowel_cp) : NULL;
            const uint8_t *orow = mrow3 ? NULL : find_triple_override(a, chain[0], chain[1], chain[2]);
            const uint8_t *trow = (mrow3 || orow) ? NULL : find_triple_tail(a, chain[1], chain[2]);
            if (mrow3) {
#ifdef KSH_TRACE_USAGE
                fprintf(stderr, "TRIPLE_MATRA %u,%u,%u,%u\n", chain[0], chain[1], chain[2], vowel_cp);
#endif
                emit_row_glyphs(a, out, &count, max_out, &pen, mrow3, 8, 9);
            } else if (orow) {
                /* Override rows store the FULL resolved sequence -- the
                 * head isn't necessarily base(c1) (e.g. c1+c2 forms its
                 * own irregular ligature), so render exactly what's stored. */
                emit_row_glyphs(a, out, &count, max_out, &pen, orow, 6, 7);
            } else if (trow) {
                const uint8_t *c0 = find_consonant(a, chain[0]);
                emit_idx(a, out, &count, max_out, &pen, rd_u16(c0 + 2));
                emit_row_glyphs(a, out, &count, max_out, &pen, trow, 4, 5);
            }
            if (mrow3) {
                /* vowel already fused into the sequence above */
            } else if (orow || trow) {
                if (fuse_vowel) {
                    /* No entry in the triple+vowel table for this exact
                     * combination (shouldn't happen given full coverage,
                     * but stay defensive); fall back to the vowel sign's
                     * standalone glyph, unfused but visible. */
                    const uint8_t *vrow = find_simple(a, vowel_cp);
                    if (vrow) emit_row_glyphs(a, out, &count, max_out, &pen, vrow, 2, 3);
                }
            } else {
                for (int k = 0; k < chain_n - 1; k++) {
                    const uint8_t *ck = find_consonant(a, chain[k]);
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(ck + 4));
                }
                uint32_t last = chain[chain_n - 1];
                if (fuse_vowel) {
                    const uint8_t *mrow = find_matra(a, last, vowel_cp);
                    if (mrow) {
                        emit_row_glyphs(a, out, &count, max_out, &pen, mrow, 4, 5);
                    } else {
                        const uint8_t *clast = find_consonant(a, last);
                        emit_idx(a, out, &count, max_out, &pen, rd_u16(clast + 2));
                        const uint8_t *vrow = find_simple(a, vowel_cp);
                        if (vrow) emit_row_glyphs(a, out, &count, max_out, &pen, vrow, 2, 3);
                    }
                } else {
                    const uint8_t *clast = find_consonant(a, last);
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(clast + 2));
                }
            }
        } else {
            const uint8_t *lcrow = find_long_chain(a, chain, chain_n);
            if (lcrow) {
                emit_row_glyphs(a, out, &count, max_out, &pen, lcrow, 11, 12);
            } else {
                for (int k = 0; k < chain_n - 1; k++) {
                    const uint8_t *ck = find_consonant(a, chain[k]);
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(ck + 4));
                }
                uint32_t last = chain[chain_n - 1];
                if (fuse_vowel) {
                    const uint8_t *mrow = find_matra(a, last, vowel_cp);
                    if (mrow) {
                        emit_row_glyphs(a, out, &count, max_out, &pen, mrow, 4, 5);
                    } else {
                        const uint8_t *clast = find_consonant(a, last);
                        emit_idx(a, out, &count, max_out, &pen, rd_u16(clast + 2));
                        const uint8_t *vrow = find_simple(a, vowel_cp);
                        if (vrow) emit_row_glyphs(a, out, &count, max_out, &pen, vrow, 2, 3);
                    }
                } else {
                    const uint8_t *clast = find_consonant(a, last);
                    emit_idx(a, out, &count, max_out, &pen, rd_u16(clast + 2));
                }
            }
        }

        if (extra_vowel_cp && !handled_double_vowel) {
            /* chain_n != 1 (no oracle data for this case) or the pair
             * isn't in the consonant-independent table -- concatenate
             * each vowel's own standalone form as a best-effort fallback. */
            const uint8_t *vrow1 = find_simple(a, vowel_cp);
            if (vrow1) emit_row_glyphs(a, out, &count, max_out, &pen, vrow1, 2, 3);
            const uint8_t *vrow2 = find_simple(a, extra_vowel_cp);
            if (vrow2) emit_row_glyphs(a, out, &count, max_out, &pen, vrow2, 2, 3);
        }

        if (use_reph) emit_idx(a, out, &count, max_out, &pen, a->reph_idx);

        while (i < n && (cps[i] == KN_ANUSVARA || cps[i] == KN_VISARGA)) {
            const uint8_t *srow = find_simple(a, cps[i]);
            if (srow) emit_row_glyphs(a, out, &count, max_out, &pen, srow, 2, 3);
            i++;
        }
    }

    *out_count = count;
    return pen;
}

/* --- UTF-8 decode --- */

static int utf8_decode(const char *s, size_t byte_len, uint32_t *out, int max_out) {
    int count = 0;
    size_t i = 0;
    const unsigned char *u = (const unsigned char *)s;
    while (i < byte_len && count < max_out) {
        unsigned char c = u[i];
        uint32_t cp;
        int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { i++; continue; } /* invalid lead byte, skip */
        if (i + (size_t)extra >= byte_len + 1 && extra > 0) { break; }
        int ok = 1;
        for (int k = 1; k <= extra; k++) {
            if (i + (size_t)k >= byte_len) { ok = 0; break; }
            unsigned char cc = u[i + (size_t)k];
            if ((cc & 0xC0) != 0x80) { ok = 0; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { i++; continue; }
        out[count++] = cp;
        i += (size_t)extra + 1;
    }
    return count;
}

/* --- layout / word-wrap --- */

typedef struct { uint32_t cps[64]; int n; } word_t;

int kshape_layout(const kshape_asset_t *asset, const char *utf8_text,
                   int panel_width, int panel_height,
                   int margin_x, int margin_y, int line_spacing,
                   gr_document_t *out_doc) {
    if (!asset || !utf8_text || !out_doc) return -1;

    size_t text_len = strlen(utf8_text);
    uint32_t *all_cps = (uint32_t *)malloc(sizeof(uint32_t) * (text_len + 1));
    if (!all_cps) return -1;
    int total_cps = utf8_decode(utf8_text, text_len, all_cps, (int)text_len + 1);

    /* Split into words on ASCII space / newline; newline forces a break. */
    word_t words[MAX_WORDS];
    int force_break_before[MAX_WORDS];
    int word_n = 0;
    int wi = 0;
    int pending_break = 0;
    for (int k = 0; k <= total_cps && word_n < MAX_WORDS; k++) {
        uint32_t cp = (k < total_cps) ? all_cps[k] : (uint32_t)'\n';
        if (cp == ' ' || cp == '\n') {
            if (wi > 0) {
                words[word_n].n = wi;
                force_break_before[word_n] = pending_break;
                word_n++;
                wi = 0;
                pending_break = 0;
            }
            if (cp == '\n') pending_break = 1;
        } else if (wi < 64) {
            words[word_n].cps[wi++] = cp;
        }
    }
    free(all_cps);

    int max_text_width = panel_width - 2 * margin_x;
    if (max_text_width < 1) max_text_width = 1;

    /* Greedy wrap, measuring each candidate line with the real shaper. */
    typedef struct { uint32_t cps[MAX_LINE_GLYPHS]; int n; } line_text_t;
    line_text_t lines[MAX_LINES];
    int line_n = 0;
    uint32_t current[MAX_LINE_GLYPHS];
    int current_n = 0;
    shaped_glyph_t scratch[MAX_LINE_GLYPHS];

    for (int w = 0; w < word_n; w++) {
        int need_break = force_break_before[w] && current_n > 0;
        if (need_break && line_n < MAX_LINES) {
            memcpy(lines[line_n].cps, current, sizeof(uint32_t) * (size_t)current_n);
            lines[line_n].n = current_n;
            line_n++;
            current_n = 0;
        }

        int candidate_n = current_n;
        if (candidate_n > 0 && candidate_n < MAX_LINE_GLYPHS) current[candidate_n++] = ' ';
        for (int k = 0; k < words[w].n && candidate_n < MAX_LINE_GLYPHS; k++)
            current[candidate_n++] = words[w].cps[k];

        int scratch_count = 0;
        int width = shape_codepoints(asset, current, candidate_n, scratch, MAX_LINE_GLYPHS, &scratch_count);

        if (width <= max_text_width || current_n == 0) {
            current_n = candidate_n;
        } else {
            if (line_n < MAX_LINES) {
                memcpy(lines[line_n].cps, current, sizeof(uint32_t) * (size_t)current_n);
                lines[line_n].n = current_n;
                line_n++;
            }
            current_n = 0;
            for (int k = 0; k < words[w].n && current_n < MAX_LINE_GLYPHS; k++)
                current[current_n++] = words[w].cps[k];
        }
    }
    if (current_n > 0 && line_n < MAX_LINES) {
        memcpy(lines[line_n].cps, current, sizeof(uint32_t) * (size_t)current_n);
        lines[line_n].n = current_n;
        line_n++;
    }

    int max_lines = (panel_height - margin_y) / line_spacing + 1;
    if (max_lines < 1) max_lines = 1;
    if (line_n > max_lines) line_n = max_lines;

    /* Shape each final line for real, accumulate runs. */
    shaped_glyph_t *line_glyphs[MAX_LINES];
    int line_glyph_counts[MAX_LINES];
    int total_runs = 0;
    for (int L = 0; L < line_n; L++) {
        shaped_glyph_t *buf = (shaped_glyph_t *)malloc(sizeof(shaped_glyph_t) * MAX_LINE_GLYPHS);
        int cnt = 0;
        shape_codepoints(asset, lines[L].cps, lines[L].n, buf, MAX_LINE_GLYPHS, &cnt);
        line_glyphs[L] = buf;
        line_glyph_counts[L] = cnt;
        total_runs += cnt;
    }

    /* Build gr_document_t backing arrays. */
    gr_glyph_t *glyph_table = (gr_glyph_t *)malloc(sizeof(gr_glyph_t) * asset->glyph_count);
    for (uint16_t gi = 0; gi < asset->glyph_count; gi++) {
        const uint8_t *src = asset->glyph_table_raw + (size_t)gi * KSH_GLYPH_STRIDE;
        glyph_table[gi].width = src[0];
        glyph_table[gi].height = src[1];
        glyph_table[gi].bearing_x = (int8_t)src[2];
        glyph_table[gi].bearing_y = (int8_t)src[3];
        glyph_table[gi].bitmap_offset = rd_u32(src + 6);
        glyph_table[gi].bitmap_size = rd_u32(src + 10);
    }

    gr_line_t *line_table = (gr_line_t *)malloc(sizeof(gr_line_t) * (size_t)(line_n > 0 ? line_n : 1));
    gr_run_entry_t *run_table = (gr_run_entry_t *)malloc(sizeof(gr_run_entry_t) * (size_t)(total_runs > 0 ? total_runs : 1));

    int run_cursor = 0;
    for (int L = 0; L < line_n; L++) {
        line_table[L].glyph_start = (uint16_t)run_cursor;
        line_table[L].glyph_run_count = (uint16_t)line_glyph_counts[L];
        line_table[L].baseline_y = (int16_t)(margin_y + L * line_spacing);
        line_table[L].reserved = 0;
        for (int r = 0; r < line_glyph_counts[L]; r++) {
            run_table[run_cursor].glyph_index = line_glyphs[L][r].glyph_idx;
            run_table[run_cursor].pen_x = (int16_t)(margin_x + line_glyphs[L][r].pen_x);
            run_table[run_cursor].reserved = 0;
            run_cursor++;
        }
        free(line_glyphs[L]);
    }

    out_doc->glyph_count = asset->glyph_count;
    out_doc->line_count = (uint16_t)line_n;
    out_doc->run_count = (uint16_t)total_runs;
    out_doc->glyphs = glyph_table;
    out_doc->bitmap_blob = asset->bitmap_blob;
    out_doc->lines = line_table;
    out_doc->runs = run_table;
    return 0;
}

void kshape_free_document(gr_document_t *doc) {
    if (!doc) return;
    free((void *)doc->glyphs);
    free((void *)doc->lines);
    free((void *)doc->runs);
    doc->glyphs = NULL;
    doc->lines = NULL;
    doc->runs = NULL;
    doc->glyph_count = doc->line_count = doc->run_count = 0;
}
