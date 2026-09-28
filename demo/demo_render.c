/* Demo: renders Kannada text using the portable device-side engine
 * (../src/glyphrender.c + ../src/kannada_shape.c) and the font-specific
 * shaping asset (../fonts/NotoSansKannada/kannada_shaping.kasset).
 * No HarfBuzz at runtime -- this is the exact code path a real device
 * would run.
 *
 * Blits into an in-memory framebuffer (the one platform-specific seam any
 * board would implement differently) and writes a PPM image. Also prints
 * the resolved glyph-index sequence per line to stdout, in the same
 * format compare_with_harfbuzz.py expects, so the two can be diffed.
 *
 * Usage: demo_render "text" out.ppm [panel_width] [panel_height] [font_size]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/glyphrender.h"
#include "../src/kannada_shape.h"

typedef struct {
    uint8_t *pixels; /* 0 = white, 1 = black */
    int width;
    int height;
} sim_framebuffer_t;

static void sim_blit(int x, int y, uint8_t width, uint8_t height,
                      const uint8_t *bitmap, void *user_data) {
    sim_framebuffer_t *fb = (sim_framebuffer_t *)user_data;
    int stride = (width + 7) / 8;
    for (int row = 0; row < height; row++) {
        int fy = y + row;
        if (fy < 0 || fy >= fb->height) continue;
        const uint8_t *row_bytes = bitmap + row * stride;
        for (int col = 0; col < width; col++) {
            int fx = x + col;
            if (fx < 0 || fx >= fb->width) continue;
            uint8_t byte = row_bytes[col / 8];
            uint8_t bit = (byte >> (7 - (col % 8))) & 1;
            if (bit) fb->pixels[fy * fb->width + fx] = 1;
        }
    }
}

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz + 1);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { perror("fread"); exit(1); }
    buf[sz] = '\0';
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s \"text\" out.ppm [panel_width] [panel_height] [font_size]\n", argv[0]);
        return 1;
    }
    const char *text = argv[1];
    const char *out_path = argv[2];
    int panel_width = argc > 3 ? atoi(argv[3]) : 200;
    int panel_height = argc > 4 ? atoi(argv[4]) : 200;
    int font_size = argc > 5 ? atoi(argv[5]) : 24;

    size_t asset_size;
    uint8_t *asset_data = read_file("../fonts/NotoSansKannada/kannada_shaping.kasset", &asset_size);

    kshape_asset_t asset;
    if (kshape_load(asset_data, asset_size, &asset) != 0) {
        fprintf(stderr, "failed to parse shaping asset\n");
        return 1;
    }

    int margin_x = 4;
    int margin_y = (int)(font_size * 1.05);
    int line_spacing = (int)(font_size * 1.25);

    gr_document_t doc;
    if (kshape_layout(&asset, text, panel_width, panel_height,
                       margin_x, margin_y, line_spacing, &doc) != 0) {
        fprintf(stderr, "shaping failed\n");
        return 1;
    }

    fprintf(stderr, "shaped on-device (no HarfBuzz at runtime): %u lines, %u glyph placements\n",
            doc.line_count, doc.run_count);

    /* Print resolved glyph indices per line, for compare_with_harfbuzz.py */
    uint16_t run_i = 0;
    for (uint16_t li = 0; li < doc.line_count; li++) {
        printf("LINE");
        uint16_t count = doc.lines[li].glyph_run_count;
        for (uint16_t k = 0; k < count && run_i < doc.run_count; k++, run_i++) {
            printf(" %u", doc.runs[run_i].glyph_index);
        }
        printf("\n");
    }

    sim_framebuffer_t fb;
    fb.width = panel_width;
    fb.height = panel_height;
    fb.pixels = calloc((size_t)panel_width * (size_t)panel_height, 1);
    if (!fb.pixels) { perror("calloc"); return 1; }

    gr_render(&doc, sim_blit, &fb);

    FILE *out = fopen(out_path, "wb");
    if (!out) { perror("fopen out"); return 1; }
    fprintf(out, "P6\n%d %d\n255\n", panel_width, panel_height);
    for (int y = 0; y < panel_height; y++) {
        for (int x = 0; x < panel_width; x++) {
            uint8_t v = fb.pixels[y * panel_width + x] ? 0 : 255;
            fputc(v, out); fputc(v, out); fputc(v, out);
        }
    }
    fclose(out);
    fprintf(stderr, "wrote %s (%dx%d)\n", out_path, panel_width, panel_height);

    kshape_free_document(&doc);
    free(fb.pixels);
    free(asset_data);
    return 0;
}
