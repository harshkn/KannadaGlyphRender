#include "glyphrender.h"

#define GR_HEADER_SIZE 12
#define GR_GLYPH_ENTRY_SIZE 12
#define GR_LINE_ENTRY_SIZE 8
#define GR_RUN_ENTRY_SIZE 6

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static int16_t rd_i16(const uint8_t *p) {
    return (int16_t)rd_u16(p);
}
static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int gr_parse(const uint8_t *data, size_t size, gr_document_t *doc) {
    if (!data || !doc || size < GR_HEADER_SIZE) return -1;
    if (data[0] != 'K' || data[1] != 'G' || data[2] != 'R' || data[3] != '1')
        return -1;

    uint16_t glyph_count = rd_u16(data + 4);
    uint16_t line_count = rd_u16(data + 6);
    uint16_t run_count = rd_u16(data + 8);

    size_t off = GR_HEADER_SIZE;
    size_t glyph_table_bytes = (size_t)glyph_count * GR_GLYPH_ENTRY_SIZE;
    if (off + glyph_table_bytes > size) return -1;
    const uint8_t *glyph_table_raw = data + off;
    off += glyph_table_bytes;

    /* Bitmap blob size isn't stored directly; it's implied by the highest
     * (offset + size) among glyph entries. Validate as we scan below. */
    uint32_t bitmap_blob_len = 0;
    for (uint16_t i = 0; i < glyph_count; i++) {
        const uint8_t *e = glyph_table_raw + (size_t)i * GR_GLYPH_ENTRY_SIZE;
        uint32_t bo = rd_u32(e + 4);
        uint32_t bs = rd_u32(e + 8);
        if (bo + bs > bitmap_blob_len) bitmap_blob_len = bo + bs;
    }
    if (off + bitmap_blob_len > size) return -1;
    const uint8_t *bitmap_blob = data + off;
    off += bitmap_blob_len;

    size_t line_table_bytes = (size_t)line_count * GR_LINE_ENTRY_SIZE;
    if (off + line_table_bytes > size) return -1;
    const uint8_t *line_table_raw = data + off;
    off += line_table_bytes;

    size_t run_table_bytes = (size_t)run_count * GR_RUN_ENTRY_SIZE;
    if (off + run_table_bytes > size) return -1;
    const uint8_t *run_table_raw = data + off;

    /* gr_glyph_t / gr_line_t / gr_run_entry_t are plain fixed-width structs
     * without padding on any target this project cares about (verified by
     * the sizeof checks below); decode in place instead of allocating. */
    doc->glyph_count = glyph_count;
    doc->line_count = line_count;
    doc->run_count = run_count;
    doc->bitmap_blob = bitmap_blob;
    doc->glyphs = (const gr_glyph_t *)(const void *)glyph_table_raw;
    doc->lines = (const gr_line_t *)(const void *)line_table_raw;
    doc->runs = (const gr_run_entry_t *)(const void *)run_table_raw;

    return 0;
}

void gr_render(const gr_document_t *doc, gr_blit_fn blit, void *user_data) {
    if (!doc || !blit) return;

    for (uint16_t li = 0; li < doc->line_count; li++) {
        const uint8_t *line_raw =
            (const uint8_t *)doc->lines + (size_t)li * GR_LINE_ENTRY_SIZE;
        uint16_t glyph_start = rd_u16(line_raw + 0);
        uint16_t glyph_run_count = rd_u16(line_raw + 2);
        int16_t baseline_y = rd_i16(line_raw + 4);

        for (uint16_t ri = 0; ri < glyph_run_count; ri++) {
            uint16_t run_idx = glyph_start + ri;
            if (run_idx >= doc->run_count) break;
            const uint8_t *run_raw =
                (const uint8_t *)doc->runs + (size_t)run_idx * GR_RUN_ENTRY_SIZE;
            uint16_t glyph_index = rd_u16(run_raw + 0);
            int16_t pen_x = rd_i16(run_raw + 2);
            if (glyph_index >= doc->glyph_count) continue;

            const uint8_t *g_raw =
                (const uint8_t *)doc->glyphs + (size_t)glyph_index * GR_GLYPH_ENTRY_SIZE;
            uint8_t width = g_raw[0];
            uint8_t height = g_raw[1];
            int8_t bearing_x = (int8_t)g_raw[2];
            int8_t bearing_y = (int8_t)g_raw[3];
            uint32_t bitmap_offset = rd_u32(g_raw + 4);

            if (width == 0 || height == 0) continue;

            int x = pen_x + bearing_x;
            int y = baseline_y - bearing_y;
            blit(x, y, width, height, doc->bitmap_blob + bitmap_offset, user_data);
        }
    }
}
