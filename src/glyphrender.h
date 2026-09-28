#ifndef GLYPHRENDER_H
#define GLYPHRENDER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Portable device-side renderer for pre-shaped Kannada (or any pre-shaped
 * script) text. All linguistic shaping (reordering, conjunct formation)
 * has already happened upstream; this side only blits resolved glyph
 * bitmaps at resolved positions. See FORMAT.md for the binary layout. */

typedef struct {
    uint8_t width;
    uint8_t height;
    int8_t bearing_x;
    int8_t bearing_y;
    uint32_t bitmap_offset;
    uint32_t bitmap_size;
} gr_glyph_t;

typedef struct {
    uint16_t glyph_start;
    uint16_t glyph_run_count;
    int16_t baseline_y;
    int16_t reserved; /* must be 0; keeps sizeof() matching the 8-byte
                        binary stride glyphrender.c reads with */
} gr_line_t;

typedef struct {
    uint16_t glyph_index;
    int16_t pen_x;
    int16_t reserved; /* must be 0; keeps sizeof() matching the 6-byte
                        binary stride glyphrender.c reads with */
} gr_run_entry_t;

typedef struct {
    uint16_t glyph_count;
    uint16_t line_count;
    uint16_t run_count;
    const gr_glyph_t *glyphs;
    const uint8_t *bitmap_blob;
    const gr_line_t *lines;
    const gr_run_entry_t *runs;
} gr_document_t;

/* Platform seam: caller supplies this to blit one 1-bit glyph bitmap.
 * (x, y) is the top-left pixel of the bitmap in framebuffer coordinates;
 * bitmap rows are MSB-first, packed, row stride = ceil(width/8) bytes.
 * `user_data` is passed through unchanged from gr_render(). */
typedef void (*gr_blit_fn)(int x, int y, uint8_t width, uint8_t height,
                            const uint8_t *bitmap, void *user_data);

/* Parses a v1 glyph-run binary blob in-place (no allocation, no copying --
 * `doc` fields point into `data`, which must outlive `doc`).
 * Returns 0 on success, -1 on malformed/truncated input. */
int gr_parse(const uint8_t *data, size_t size, gr_document_t *doc);

/* Draws every glyph in the document by calling `blit` once per placement. */
void gr_render(const gr_document_t *doc, gr_blit_fn blit, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* GLYPHRENDER_H */
