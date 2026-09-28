#ifndef KANNADA_SHAPE_H
#define KANNADA_SHAPE_H

#include <stdint.h>
#include <stddef.h>
#include "glyphrender.h"

#ifdef __cplusplus
extern "C" {
#endif

/* On-device Kannada shaping engine. Takes raw UTF-8 text and a baked
 * shaping asset (rule tables + glyph atlas, produced offline by
 * tools/build_shaping_asset.py) and produces a gr_document_t ready for
 * gr_render() -- no HarfBuzz or any shaping engine needed at runtime.
 *
 * Scope (see README/FORMAT.md for detail):
 *  - correct reph reordering (ra + virama + consonant -> reph mark deferred
 *    after the syllable)
 *  - correct 2-consonant conjuncts via a precomputed lookup table (covers
 *    the common cases, including true ligatures like ksha/jna)
 *  - correct consonant+vowel-sign fusion via a precomputed lookup table
 *  - 3+ consonant chains fall back to a readable but non-ligated rendering
 *    (each consonant shown with its explicit-virama form) -- documented
 *    limitation, not a generic shaping engine.
 */

typedef struct {
    const uint8_t *data;
    size_t size;
    uint16_t glyph_count;
    uint16_t consonant_count;
    uint16_t matra_count;
    uint16_t conjunct_count;
    uint16_t simple_count;
    uint16_t conjunct_matra_count;
    uint16_t triple_tail_count;
    uint16_t triple_override_count;
    uint16_t reph_idx;
    uint32_t triple_matra_count;
    uint16_t conjunct_virama_count;
    uint16_t double_vowel_count;
    uint16_t long_chain_count;
    const uint8_t *glyph_table_raw;     /* stride 14 bytes */
    const uint8_t *bitmap_blob;
    const uint8_t *consonant_table_raw; /* stride 6 bytes, sorted by codepoint */
    const uint8_t *matra_table_raw;     /* stride 13 bytes, sorted by (c_cp,v_cp) */
    const uint8_t *conjunct_table_raw;  /* stride 9 bytes, sorted by (c1_cp,c2_cp) */
    const uint8_t *simple_table_raw;    /* stride 4 bytes, sorted by codepoint */
    const uint8_t *conjunct_matra_table_raw; /* stride 17 bytes, sorted by (c1_cp,c2_cp,v_cp) */
    const uint8_t *triple_tail_table_raw;     /* stride 9 bytes, sorted by (c2_cp,c3_cp) */
    const uint8_t *triple_override_table_raw; /* stride 13 bytes, sorted by (c1_cp,c2_cp,c3_cp), full glyph sequence */
    const uint8_t *triple_matra_table_raw;    /* stride 21 bytes, sorted by (c1_cp,c2_cp,c3_cp,v_cp), full glyph sequence */
    const uint8_t *conjunct_virama_table_raw; /* stride 9 bytes, sorted by (c1_cp,c2_cp); word-final variant */
    const uint8_t *double_vowel_table_raw;    /* stride 13 bytes, sorted by (v1_cp,v2_cp); consonant-independent subset only */
    const uint8_t *long_chain_table_raw;      /* stride 22 bytes, unsorted linear scan; corpus-scoped 4-5 consonant chain overrides */
} kshape_asset_t;

/* Parses a KSH1 asset blob in place (no copy; `asset` borrows `data`,
 * which must outlive it). Returns 0 on success, -1 on malformed input. */
int kshape_load(const uint8_t *data, size_t size, kshape_asset_t *asset);

/* Shapes and word-wraps utf8_text (plain UTF-8, words separated by ASCII
 * space or '\n') to fit panel_width, producing a fully-resolved document.
 * Lines that would fall below panel_height are dropped. On success, the
 * caller owns out_doc's backing arrays and must call kshape_free_document.
 * Returns 0 on success, -1 on error (bad UTF-8, allocation failure). */
int kshape_layout(const kshape_asset_t *asset, const char *utf8_text,
                   int panel_width, int panel_height,
                   int margin_x, int margin_y, int line_spacing,
                   gr_document_t *out_doc);

void kshape_free_document(gr_document_t *doc);

#ifdef __cplusplus
}
#endif

#endif /* KANNADA_SHAPE_H */
