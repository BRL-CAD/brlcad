/*                         I C V _ D E M O . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file demo.c
 *
 * Developer-facing libicv examples.  This executable is intentionally
 * demonstrative: it prints representative output and also returns non-zero
 * when an example no longer works.
 */

#include "common.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "bu/malloc.h"
#include "icv.h"

#define DEMO_CHECK(_expr, _msg) do { \
    if (!(_expr)) { \
	bu_log("icv_demo failure: %s\n", _msg); \
	failures++; \
    } \
} while (0)

static int failures = 0;

static icv_image_t *
demo_rgb(size_t w, size_t h)
{
    icv_image_t *img = icv_create(w, h, ICV_COLOR_SPACE_RGB);

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t off = (y * w + x) * 3;
	    img->data[off + 0] = (w > 1) ? (double)x / (double)(w - 1) : 0.0;
	    img->data[off + 1] = (h > 1) ? (double)y / (double)(h - 1) : 0.0;
	    img->data[off + 2] = (double)(x + y) / (double)(w + h);
	}
    }

    return img;
}

static icv_image_t *
clone_image(const icv_image_t *src)
{
    icv_image_t *dst = icv_create(src->width, src->height, src->color_space);
    size_t len = src->width * src->height * src->channels;

    memcpy(dst->data, src->data, len * sizeof(double));
    return dst;
}

static void
print_crop_diagram(void)
{
    printf("\nQuadrilateral crop example coordinates:\n");
    printf("        (1,5)             (6,4)\n");
    printf("          +---------------+\n");
    printf("         /                 \\\n");
    printf("        /   sampled area    \\\n");
    printf("       +---------------------+\n");
    printf("    (0,1)                 (7,0)\n\n");
}

static void
demo_memory_io(icv_image_t *img)
{
    unsigned char *png = NULL;
    size_t png_len = 0;

    DEMO_CHECK(icv_write_mem(img, &png, &png_len, BU_MIME_IMAGE_PNG) == 0,
	       "encode PNG to memory");
    DEMO_CHECK(png != NULL && png_len > 0, "PNG memory buffer is populated");
    if (png) {
	icv_image_t *decoded = icv_read_mem(png, png_len, BU_MIME_IMAGE_PNG, 0, 0);
	DEMO_CHECK(decoded != NULL, "decode PNG from memory");
	if (decoded) {
	    printf("memory PNG roundtrip: %zux%zu, %zu bytes\n", decoded->width, decoded->height, png_len);
	    icv_destroy(decoded);
	}
	bu_free(png, "demo PNG buffer");
    }
}

static void
demo_file_io(icv_image_t *img)
{
    struct fmt {
	bu_mime_image_t mime;
	const char *label;
	size_t raw_w;
	size_t raw_h;
    } fmts[] = {
	{BU_MIME_IMAGE_PIX, "PIX", img->width, img->height},
	{BU_MIME_IMAGE_PPM, "PPM", 0, 0},
	{BU_MIME_IMAGE_PNG, "PNG", 0, 0}
    };

    for (size_t i = 0; i < sizeof(fmts)/sizeof(fmts[0]); i++) {
	char path[MAXPATHLEN] = {0};
	FILE *fp = bu_temp_file(path, MAXPATHLEN);
	DEMO_CHECK(fp != NULL && path[0] != '\0', "create temporary image path");
	if (!fp)
	    continue;
	fclose(fp);

	DEMO_CHECK(icv_write(img, path, fmts[i].mime) == 0, "write image file");
	icv_image_t *read_img = icv_read(path, fmts[i].mime, fmts[i].raw_w, fmts[i].raw_h);
	DEMO_CHECK(read_img != NULL, "read image file");
	if (read_img) {
	    printf("file %s roundtrip: %zux%zu\n", fmts[i].label, read_img->width, read_img->height);
	    icv_destroy(read_img);
	}
	bu_file_delete(path);
    }
}

static void
demo_crop_filter_ascii(icv_image_t *base)
{
    struct icv_ascii_art_params artparams = ICV_ASCII_ART_PARAMS_DEFAULT;
    icv_image_t *rect = clone_image(base);
    icv_image_t *quad = clone_image(base);
    char *art = NULL;

    print_crop_diagram();

    DEMO_CHECK(icv_rect(rect, 2, 1, 4, 3) == 0, "rectangular crop");
    printf("rectangular crop result: %zux%zu\n", rect->width, rect->height);

    DEMO_CHECK(icv_crop(quad, 1, 5, 6, 4, 7, 0, 0, 1, 4, 5) == 0, "quadrilateral crop");
    printf("quadrilateral crop result: %zux%zu\n", quad->width, quad->height);

    DEMO_CHECK(icv_filter(quad, ICV_FILTER_LOW_PASS) == 0, "low-pass filter");
    DEMO_CHECK(icv_fade(quad, 0.85) == 0, "fade image");
    DEMO_CHECK(icv_rgb2gray(quad, ICV_COLOR_RGB, 0.30, 0.59, 0.11) == 0, "convert filtered crop to grayscale");

    art = icv_ascii_art(quad, &artparams);
    DEMO_CHECK(art != NULL, "generate ASCII art");
    if (art) {
	printf("\nASCII art preview of filtered crop:\n%s\n", art);
	bu_free(art, "demo ASCII art");
    }

    icv_destroy(rect);
    icv_destroy(quad);
}

static void
demo_diffs(void)
{
    icv_image_t *a = demo_rgb(8, 8);
    icv_image_t *b = clone_image(a);
    int matching = 0;
    int off_by_1 = 0;
    int off_by_many = 0;

    b->data[(7 * 8 + 7) * 3 + 0] = 0.0;
    b->data[(4 * 8 + 3) * 3 + 1] = 1.0;

    DEMO_CHECK(icv_diff(&matching, &off_by_1, &off_by_many, a, b) == 1,
	       "detect exact channel differences");
    printf("diff counts: matching=%d off_by_1=%d off_by_many=%d\n", matching, off_by_1, off_by_many);

    fastf_t phash_distance = icv_adiff(a, b, ICV_DIFF_PHASH);
    DEMO_CHECK(!NEAR_EQUAL(phash_distance, -1, SMALL_FASTF), "compute perceptual hash distance");
    printf("perceptual hash distance: %g\n", phash_distance);

    icv_destroy(a);
    icv_destroy(b);
}

static void
demo_animation(void)
{
    char path[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(path, MAXPATHLEN);
    icv_anim_t *anim = icv_anim_create(ICV_ANIM_APNG, 0, 0, 12);
    icv_image_t *a = demo_rgb(4, 4);
    icv_image_t *b = demo_rgb(4, 4);

    b->data[0] = 1.0;
    b->data[1] = 0.0;
    b->data[2] = 0.0;

    DEMO_CHECK(fp != NULL && path[0] != '\0', "create temporary animation path");
    if (fp)
	fclose(fp);

    DEMO_CHECK(anim != NULL, "create animation");
    if (anim && fp) {
	DEMO_CHECK(icv_anim_add_frame(anim, a) == 0, "append first animation frame");
	DEMO_CHECK(icv_anim_add_frame(anim, b) == 0, "append second animation frame");
	DEMO_CHECK(icv_anim_set_frame_delay(anim, 1, 100000) == 0, "set animation frame delay");
	DEMO_CHECK(icv_anim_write(anim, path) == 0, "write APNG animation");
	icv_anim_t *read_anim = icv_anim_read(path);
	DEMO_CHECK(read_anim != NULL, "read APNG animation");
	if (read_anim) {
	    printf("animation roundtrip frames: %zu\n", icv_anim_num_frames(read_anim));
	    icv_anim_destroy(read_anim);
	}
	bu_file_delete(path);
    }

    icv_anim_destroy(anim);
    icv_destroy(a);
    icv_destroy(b);
}

int
main(int argc, char **argv)
{
    (void)argc;
    bu_setprogname(argv[0]);

    printf("libicv developer demo\n");

    icv_image_t *img = demo_rgb(8, 6);
    demo_memory_io(img);
    demo_file_io(img);
    demo_crop_filter_ascii(img);
    demo_diffs();
    demo_animation();
    icv_destroy(img);

    if (failures) {
	bu_log("icv_demo encountered %d failed example checks\n", failures);
	return 1;
    }

    printf("\nicv_demo completed successfully\n");
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
