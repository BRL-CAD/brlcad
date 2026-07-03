/*                    I C V _ R O B U S T N E S S . C
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
/** @file robustness.c
 *
 * Behavioral and robustness coverage for libicv public APIs.
 */

#include "common.h"

#include <math.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "icv.h"

#define CHECK(_expr, _msg) do { \
    if (!(_expr)) { \
	bu_log("FAIL: %s\n", _msg); \
	failures++; \
    } \
} while (0)

static int failures = 0;

static int
near_equal(double a, double b)
{
    return fabs(a - b) < 1.0e-12;
}

static icv_image_t *
make_gray(size_t w, size_t h)
{
    icv_image_t *img = icv_image_create(w, h, ICV_COLOR_SPACE_GRAY);
    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    img->data[y * w + x] = (double)(y * 10 + x);
	}
    }
    return img;
}

static icv_image_t *
make_unit_gray(size_t w, size_t h)
{
    icv_image_t *img = icv_image_create(w, h, ICV_COLOR_SPACE_GRAY);
    size_t npix = w * h;
    for (size_t i = 0; i < npix; i++) {
	img->data[i] = (npix > 1) ? (double)i / (double)(npix - 1) : 0.0;
    }
    return img;
}

static icv_image_t *
make_rgb(size_t w, size_t h)
{
    icv_image_t *img = icv_image_create(w, h, ICV_COLOR_SPACE_RGB);
    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t off = (y * w + x) * 3;
	    img->data[off + 0] = (double)x / 10.0;
	    img->data[off + 1] = (double)y / 10.0;
	    img->data[off + 2] = (double)(x + y) / 10.0;
	}
    }
    return img;
}

static icv_image_t *
make_unit_rgb(size_t w, size_t h)
{
    icv_image_t *img = icv_image_create(w, h, ICV_COLOR_SPACE_RGB);
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
make_solid_rgb(size_t w, size_t h, double r, double g, double b)
{
    icv_image_t *img = icv_image_create(w, h, ICV_COLOR_SPACE_RGB);
    for (size_t i = 0; i < w * h; i++) {
	img->data[i * 3 + 0] = r;
	img->data[i * 3 + 1] = g;
	img->data[i * 3 + 2] = b;
    }
    return img;
}

static icv_image_t *
make_rgba(size_t w, size_t h)
{
    icv_image_t *img;
    BU_ALLOC(img, struct icv_image);
    ICV_IMAGE_INIT(img);
    img->width = w;
    img->height = h;
    img->color_space = ICV_COLOR_SPACE_RGB;
    img->channels = 4;
    img->alpha_channel = 1;
    img->data = (double *)bu_calloc(w * h * img->channels, sizeof(double), "rgba test data");
    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t off = (y * w + x) * img->channels;
	    img->data[off + 0] = (double)x / 10.0;
	    img->data[off + 1] = (double)y / 10.0;
	    img->data[off + 2] = (double)(x + y) / 10.0;
	    img->data[off + 3] = 0.25 + (double)(x + y) / 20.0;
	}
    }
    return img;
}

static void
free_bins(size_t **bins, size_t channels)
{
    if (!bins) return;
    for (size_t c = 0; c < channels; c++) {
	if (bins[c]) bu_free(bins[c], "histogram channel bins");
    }
    bu_free(bins, "histogram bins");
}

static void
fill_render_info(struct icv_render_info *ri, const char *db, const char *objects)
{
    ri->db_filename = bu_strdup(db);
    ri->objects = bu_strdup(objects);
    for (size_t i = 0; i < 16; i++) {
	ri->viewrotscale[i] = (i % 5 == 0) ? 1.0 : 0.0;
    }
    ri->viewrotscale[15] = 1.0;
    ri->eye_model[0] = 10.0;
    ri->eye_model[1] = 20.0;
    ri->eye_model[2] = 30.0;
    ri->viewsize = 2.0;
    ri->aspect = 1.0;
    ri->perspective = 0.0;
}

static void
test_create_zero_and_pixel_io(void)
{
    icv_image_t *rgb = icv_image_create(2, 3, ICV_COLOR_SPACE_RGB);
    double px[3] = {0.25, 0.50, 0.75};
    unsigned char row_uc[6] = {0, 64, 128, 255, 127, 1};
    double row_d[6] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6};

    CHECK(rgb != NULL, "icv_image_create returns an image");
    CHECK(rgb->magic == ICV_IMAGE_MAGIC, "icv_image_create initializes magic");
    CHECK(rgb->width == 2 && rgb->height == 3, "icv_image_create records dimensions");
    CHECK(rgb->color_space == ICV_COLOR_SPACE_RGB && rgb->channels == 3, "icv_image_create records RGB layout");
    CHECK(rgb->render_info == NULL, "icv_image_create starts without render metadata");
    for (size_t i = 0; i < 18; i++) {
	CHECK(near_equal(rgb->data[i], 0.0), "icv_image_create zero-fills image data");
    }
    CHECK(icv_image_create(1, 1, (ICV_COLOR_SPACE)999) == NULL, "icv_image_create rejects invalid color space without terminating");
    CHECK(icv_image_create(0, 1, ICV_COLOR_SPACE_RGB) == NULL, "icv_image_create rejects zero width without terminating");
    CHECK(icv_image_create(1, 0, ICV_COLOR_SPACE_RGB) == NULL, "icv_image_create rejects zero height without terminating");

    CHECK(icv_writepixel(rgb, 1, 2, px) == 0, "icv_writepixel accepts valid coordinates");
    CHECK(near_equal(rgb->data[(2 * 2 + 1) * 3 + 0], 0.25), "icv_writepixel writes red channel");
    CHECK(near_equal(rgb->data[(2 * 2 + 1) * 3 + 1], 0.50), "icv_writepixel writes green channel");
    CHECK(near_equal(rgb->data[(2 * 2 + 1) * 3 + 2], 0.75), "icv_writepixel writes blue channel");
    CHECK(icv_writepixel(rgb, 2, 0, px) == -1, "icv_writepixel rejects out-of-bounds x");
    CHECK(icv_writepixel(rgb, 0, 3, px) == -1, "icv_writepixel rejects out-of-bounds y");
    CHECK(icv_writepixel(rgb, 0, 0, NULL) == -1, "icv_writepixel rejects null pixel data");

    CHECK(icv_writeline(rgb, 1, row_uc, ICV_DATA_UCHAR) == 0, "icv_writeline accepts uchar data");
    CHECK(near_equal(rgb->data[6], 0.0), "icv_writeline converts uchar 0");
    CHECK(near_equal(rgb->data[7], 64.0 / 255.0), "icv_writeline converts uchar 64");
    CHECK(near_equal(rgb->data[9], 1.0), "icv_writeline converts uchar 255");
    CHECK(icv_writeline(rgb, 0, row_d, ICV_DATA_DOUBLE) == 0, "icv_writeline accepts double data");
    CHECK(near_equal(rgb->data[0], 0.1), "icv_writeline copies double data");
    CHECK(icv_writeline(rgb, 3, row_d, ICV_DATA_DOUBLE) == -1, "icv_writeline rejects out-of-bounds row");
    CHECK(icv_writeline(rgb, 0, NULL, ICV_DATA_DOUBLE) == -1, "icv_writeline rejects null line data");

    CHECK(icv_zero(rgb) == rgb, "icv_zero returns its input image");
    for (size_t i = 0; i < 18; i++) {
	CHECK(near_equal(rgb->data[i], 0.0), "icv_zero clears image data");
    }

    icv_destroy(rgb);
}

static void
test_data_conversion_and_size_guessing(void)
{
    unsigned char src[4] = {0, 127, 128, 255};
    double *dbl = icv_uchar2double(src, 4);
    icv_image_t *img = icv_image_create(4, 1, ICV_COLOR_SPACE_GRAY);
    unsigned char *uch = NULL;
    size_t w = 0;
    size_t h = 0;

    CHECK(dbl != NULL, "icv_uchar2double allocates output");
    CHECK(near_equal(dbl[0], 0.0), "icv_uchar2double converts 0");
    CHECK(near_equal(dbl[1], 127.0 / 255.0), "icv_uchar2double converts 127");
    CHECK(near_equal(dbl[3], 1.0), "icv_uchar2double converts 255");
    CHECK(icv_uchar2double(src, 0) == NULL, "icv_uchar2double rejects zero size");

    img->data[0] = -1.0;
    img->data[1] = 0.5;
    img->data[2] = 2.0;
    img->data[3] = 1.0;
    uch = icv_data2uchar(img);
    CHECK(uch != NULL, "icv_data2uchar allocates output");
    CHECK(uch[0] == 0, "icv_data2uchar clamps negative values");
    CHECK(uch[1] == 128, "icv_data2uchar rounds mid-range values");
    CHECK(uch[2] == 255, "icv_data2uchar clamps high values");
    CHECK(uch[3] == 255, "icv_data2uchar preserves one");
    bu_free(uch, "uchar data");

    img->gamma_corr = 2.0;
    uch = icv_data2uchar(img);
    CHECK(uch != NULL, "icv_data2uchar handles gamma correction");
    CHECK(uch[0] <= 1, "gamma-corrected conversion clamps negative values");
    CHECK(uch[1] >= 180 && uch[1] <= 182, "gamma-corrected conversion applies exponent");
    CHECK(uch[2] == 255, "gamma-corrected conversion clamps high values");
    CHECK(uch[3] == 255, "gamma-corrected conversion preserves one");
    bu_free(uch, "gamma uchar data");

    CHECK(icv_data2uchar(NULL) == NULL, "icv_data2uchar rejects null image");
    CHECK(icv_image_size(NULL, 0, 12, BU_MIME_IMAGE_PIX, &w, &h) == 1 && w == 2 && h == 2,
	  "icv_image_size infers square PIX dimensions");
    CHECK(icv_image_size(NULL, 0, 9, BU_MIME_IMAGE_BW, &w, &h) == 1 && w == 3 && h == 3,
	  "icv_image_size infers square BW dimensions");
    CHECK(icv_image_size("VGA *", 0, 640 * 480 * 3, BU_MIME_IMAGE_PIX, &w, &h) == 1 && w == 640 && h == 480,
	  "icv_image_size validates named display dimensions");
    CHECK(icv_image_size("VGA *", 0, 10, BU_MIME_IMAGE_PIX, &w, &h) == 0,
	  "icv_image_size rejects mismatched named display dimensions");
    CHECK(icv_image_size("Letter", 72, 0, BU_MIME_IMAGE_PIX, &w, &h) == 1 && w == 612 && h == 792,
	  "icv_image_size converts named inch paper size with requested DPI");
    CHECK(icv_image_size("A4", 300, 0, BU_MIME_IMAGE_BW, &w, &h) == 1 && w == 2480 && h == 3507,
	  "icv_image_size converts named metric paper size with requested DPI");
    CHECK(icv_image_size("Letter", 0, 612 * 792 * 3, BU_MIME_IMAGE_PIX, &w, &h) == 1 && w == 612 && h == 792,
	  "icv_image_size infers paper DPI from file size");
    CHECK(icv_image_size("not-a-size", 0, 0, BU_MIME_IMAGE_PIX, &w, &h) == 0,
	  "icv_image_size rejects unknown labels with no data size");

    bu_free(dbl, "double data");
    icv_destroy(img);
}

static void
test_colorspace_conversions(void)
{
    icv_image_t *gray = icv_image_create(2, 1, ICV_COLOR_SPACE_GRAY);
    gray->data[0] = 0.2;
    gray->data[1] = 0.8;

    CHECK(icv_gray2rgb(gray) == 0, "icv_gray2rgb converts grayscale images");
    CHECK(gray->color_space == ICV_COLOR_SPACE_RGB && gray->channels == 3, "icv_gray2rgb updates layout");
    CHECK(near_equal(gray->data[0], 0.2) && near_equal(gray->data[1], 0.2) && near_equal(gray->data[2], 0.2),
	  "icv_gray2rgb replicates first pixel");
    CHECK(near_equal(gray->data[3], 0.8) && near_equal(gray->data[4], 0.8) && near_equal(gray->data[5], 0.8),
	  "icv_gray2rgb replicates second pixel");
    CHECK(icv_gray2rgb(gray) == 0, "icv_gray2rgb is a no-op on RGB images");
    icv_destroy(gray);

    icv_image_t *rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.2;
    rgb->data[1] = 0.4;
    rgb->data[2] = 0.8;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_RGB, 0.25, 0.50, 0.25) == 0, "icv_rgb2gray accepts explicit weights");
    CHECK(rgb->color_space == ICV_COLOR_SPACE_GRAY && rgb->channels == 1, "icv_rgb2gray updates layout");
    CHECK(near_equal(rgb->data[0], 0.45), "icv_rgb2gray applies explicit weights");
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_RGB, 0, 0, 0) == 0, "icv_rgb2gray is a no-op on grayscale images");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    CHECK(icv_rgb2gray(rgb, (ICV_COLOR)999, 0, 0, 0) == -1, "icv_rgb2gray rejects invalid channel selector");
    icv_destroy(rgb);

    rgb = icv_image_create(2, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.1; rgb->data[1] = 0.2; rgb->data[2] = 0.3;
    rgb->data[3] = 0.9; rgb->data[4] = 0.8; rgb->data[5] = 0.7;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_G, 0, 0, 0) == 0, "icv_rgb2gray selects a single channel");
    CHECK(near_equal(rgb->data[0], 0.2) && near_equal(rgb->data[1], 0.8), "icv_rgb2gray copies selected channel values");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.1; rgb->data[1] = 0.4; rgb->data[2] = 0.9;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_R, 0, 0, 0) == 0, "icv_rgb2gray selects red channel");
    CHECK(near_equal(rgb->data[0], 0.1), "red channel conversion value");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.1; rgb->data[1] = 0.4; rgb->data[2] = 0.9;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_B, 0, 0, 0) == 0, "icv_rgb2gray selects blue channel");
    CHECK(near_equal(rgb->data[0], 0.9), "blue channel conversion value");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.2; rgb->data[1] = 0.6; rgb->data[2] = 1.0;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_RG, 0, 0, 0) == 0, "icv_rgb2gray averages red and green");
    CHECK(near_equal(rgb->data[0], 0.4), "red-green conversion value");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.2; rgb->data[1] = 0.6; rgb->data[2] = 1.0;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_RB, 0, 0, 0) == 0, "icv_rgb2gray averages red and blue");
    CHECK(near_equal(rgb->data[0], 0.6), "red-blue conversion value");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.2; rgb->data[1] = 0.6; rgb->data[2] = 1.0;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_BG, 0, 0, 0) == 0, "icv_rgb2gray averages blue and green");
    CHECK(near_equal(rgb->data[0], 0.8), "blue-green conversion value");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.9; rgb->data[1] = 0.6; rgb->data[2] = 0.3;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_RGB, 0, 0, 0) == 0, "icv_rgb2gray applies uniform RGB weights by default");
    CHECK(near_equal(rgb->data[0], 0.6), "uniform RGB conversion value");
    icv_destroy(rgb);

    rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.9; rgb->data[1] = 0.6; rgb->data[2] = 0.3;
    CHECK(icv_rgb2gray(rgb, ICV_COLOR_RGB, 2.0, 0.0, 0.0) == 0, "icv_rgb2gray clamps weighted high values");
    CHECK(near_equal(rgb->data[0], 1.0), "weighted RGB conversion clamps above one");
    icv_destroy(rgb);
}

static void
test_scalar_and_image_operations(void)
{
    icv_image_t *img = icv_image_create(4, 1, ICV_COLOR_SPACE_GRAY);
    img->data[0] = -0.5;
    img->data[1] = NAN;
    img->data[2] = 0.5;
    img->data[3] = 2.0;
    CHECK(icv_sanitize(img) == 0, "icv_sanitize accepts initialized images");
    CHECK(near_equal(img->data[0], 0.0), "icv_sanitize clamps low values");
    CHECK(near_equal(img->data[1], 0.0), "icv_sanitize maps NaN to zero");
    CHECK(near_equal(img->data[2], 0.5), "icv_sanitize preserves in-range values");
    CHECK(near_equal(img->data[3], 1.0), "icv_sanitize clamps high values");
    CHECK((img->flags & ICV_SANITIZED) != 0, "icv_sanitize sets sanitized flag");

    CHECK(icv_add_val(img, 0.75) == 0, "icv_add_val accepts image");
    CHECK(near_equal(img->data[0], 0.75), "icv_add_val adds scalar");
    CHECK(near_equal(img->data[3], 1.0), "icv_add_val sanitizes by default");

    img->flags |= ICV_OPERATIONS_MODE;
    CHECK(icv_multiply_val(img, 2.0) == 0, "icv_multiply_val accepts image");
    CHECK(near_equal(img->data[0], 1.5), "operations mode preserves out-of-range scalar results");
    CHECK((img->flags & ICV_SANITIZED) == 0, "operations mode clears sanitized flag");
    CHECK(icv_divide_val(img, 2.0) == 0, "icv_divide_val accepts image");
    CHECK(near_equal(img->data[0], 0.75), "icv_divide_val divides scalar");
    CHECK(icv_pow_val(img, 2.0) == 0, "icv_pow_val accepts image");
    CHECK(near_equal(img->data[0], 0.5625), "icv_pow_val raises scalar");
    icv_destroy(img);

    icv_image_t *a = icv_image_create(2, 1, ICV_COLOR_SPACE_GRAY);
    icv_image_t *b = icv_image_create(2, 1, ICV_COLOR_SPACE_GRAY);
    a->data[0] = 0.75; a->data[1] = 0.25;
    b->data[0] = 0.50; b->data[1] = 0.50;
    icv_image_t *out = icv_add(a, b);
    CHECK(out != NULL && near_equal(out->data[0], 1.0) && near_equal(out->data[1], 0.75),
	  "icv_add returns sanitized sum image");
    icv_destroy(out);
    out = icv_sub(a, b);
    CHECK(out != NULL && near_equal(out->data[0], 0.25) && near_equal(out->data[1], 0.0),
	  "icv_sub returns sanitized difference image");
    icv_destroy(out);
    out = icv_multiply(a, b);
    CHECK(out != NULL && near_equal(out->data[0], 0.375) && near_equal(out->data[1], 0.125),
	  "icv_multiply returns product image");
    icv_destroy(out);
    out = icv_divide(a, b);
    CHECK(out != NULL && near_equal(out->data[0], 1.0) && near_equal(out->data[1], 0.5),
	  "icv_divide returns sanitized quotient image");
    icv_destroy(out);

    icv_image_t *mismatch = icv_image_create(3, 1, ICV_COLOR_SPACE_GRAY);
    CHECK(icv_add(a, mismatch) == NULL, "icv_add rejects mismatched dimensions");
    CHECK(icv_sub(a, mismatch) == NULL, "icv_sub rejects mismatched dimensions");
    CHECK(icv_multiply(a, mismatch) == NULL, "icv_multiply rejects mismatched dimensions");
    CHECK(icv_divide(a, mismatch) == NULL, "icv_divide rejects mismatched dimensions");
    icv_destroy(mismatch);
    icv_destroy(a);
    icv_destroy(b);

    icv_image_t *rgba1 = make_rgba(2, 1);
    icv_image_t *rgba2 = make_rgba(2, 1);
    rgba2->data[3] = 0.10;
    out = icv_add(rgba1, rgba2);
    CHECK(out != NULL && out->color_space == ICV_COLOR_SPACE_RGB && out->channels == 4 && out->alpha_channel == 1,
	  "icv_add preserves RGBA layout");
    if (out) {
	CHECK(near_equal(out->data[3], 0.35), "icv_add computes alpha channel with image data");
	icv_destroy(out);
    }
    icv_destroy(rgba1);
    icv_destroy(rgba2);
}

static void
test_saturate_fade_resize_and_fit(void)
{
    icv_image_t *rgb = icv_image_create(1, 1, ICV_COLOR_SPACE_RGB);
    rgb->data[0] = 0.2;
    rgb->data[1] = 0.4;
    rgb->data[2] = 0.8;
    CHECK(icv_saturate(rgb, 0.0) == 0, "icv_saturate accepts RGB images");
    CHECK(near_equal(rgb->data[0], rgb->data[1]) && near_equal(rgb->data[1], rgb->data[2]),
	  "icv_saturate with zero produces monochrome output");
    CHECK(near_equal(rgb->data[0], 0.37), "icv_saturate uses documented luminance weights");
    icv_destroy(rgb);

    icv_image_t *gray = icv_image_create(1, 1, ICV_COLOR_SPACE_GRAY);
    CHECK(icv_saturate(gray, 1.0) == -1, "icv_saturate rejects grayscale images");
    gray->data[0] = 0.8;
    CHECK(icv_fade(gray, 0.25) == 0, "icv_fade accepts valid fraction");
    CHECK(near_equal(gray->data[0], 0.2), "icv_fade scales intensity by requested fraction");
    CHECK(icv_fade(gray, -0.1) == -1, "icv_fade rejects negative fraction");
    CHECK(icv_fade(gray, 1.1) == -1, "icv_fade rejects fraction greater than one");
    icv_destroy(gray);

    gray = make_gray(4, 4);
    CHECK(icv_resize(gray, ICV_RESIZE_UNDERSAMPLE, 0, 0, 2) == 0, "icv_resize undersample succeeds");
    CHECK(gray->width == 2 && gray->height == 2, "undersample output dimensions");
    CHECK(near_equal(gray->data[0], 0.0), "undersample lower-left sample");
    CHECK(near_equal(gray->data[1], 2.0), "undersample lower-right sample");
    CHECK(near_equal(gray->data[2], 20.0), "undersample upper-left sample");
    CHECK(near_equal(gray->data[3], 22.0), "undersample upper-right sample");
    icv_destroy(gray);

    gray = make_gray(4, 4);
    CHECK(icv_resize(gray, ICV_RESIZE_SHRINK, 0, 0, 2) == 0, "icv_resize shrink succeeds");
    CHECK(gray->width == 2 && gray->height == 2, "shrink output dimensions");
    CHECK(near_equal(gray->data[0], 5.5), "shrink lower-left block average");
    CHECK(near_equal(gray->data[1], 7.5), "shrink lower-right block average");
    CHECK(near_equal(gray->data[2], 25.5), "shrink upper-left block average");
    CHECK(near_equal(gray->data[3], 27.5), "shrink upper-right block average");
    icv_destroy(gray);

    gray = make_gray(4, 4);
    CHECK(icv_resize(gray, ICV_RESIZE_SHRINK, 0, 0, 0) == -1, "icv_resize rejects zero shrink factor");
    CHECK(icv_resize(gray, ICV_RESIZE_SHRINK, 0, 0, 5) == -1, "icv_resize rejects shrink factor larger than image");
    CHECK(icv_resize(gray, ICV_RESIZE_NINTERP, 2, 2, 0) == 0, "icv_resize nearest interpolation succeeds");
    CHECK(gray->width == 2 && gray->height == 2, "nearest interpolation output dimensions");
    CHECK(near_equal(gray->data[0], 0.0), "nearest interpolation preserves lower-left endpoint");
    CHECK(near_equal(gray->data[1], 3.0), "nearest interpolation preserves lower-right endpoint");
    CHECK(near_equal(gray->data[2], 30.0), "nearest interpolation preserves upper-left endpoint");
    CHECK(near_equal(gray->data[3], 33.0), "nearest interpolation preserves upper-right endpoint");
    icv_destroy(gray);

    gray = make_gray(2, 2);
    CHECK(icv_resize(gray, ICV_RESIZE_BINTERP, 3, 3, 0) == 0, "icv_resize bilinear interpolation succeeds");
    CHECK(gray->width == 3 && gray->height == 3, "bilinear interpolation output dimensions");
    CHECK(near_equal(gray->data[0], 0.0), "bilinear interpolation preserves lower-left endpoint");
    CHECK(near_equal(gray->data[2], 1.0), "bilinear interpolation preserves lower-right endpoint");
    CHECK(near_equal(gray->data[4], 5.5), "bilinear interpolation computes center sample");
    CHECK(near_equal(gray->data[6], 10.0), "bilinear interpolation preserves upper-left endpoint");
    CHECK(near_equal(gray->data[8], 11.0), "bilinear interpolation preserves upper-right endpoint");
    icv_destroy(gray);

    gray = make_gray(4, 2);
    CHECK(icv_resize(gray, ICV_RESIZE_BINTERP, 2, 4, 0) == 0, "icv_resize supports mixed shrink/stretch bilinear interpolation");
    CHECK(gray->width == 2 && gray->height == 4, "mixed resize output dimensions");
    icv_destroy(gray);

    rgb = make_unit_rgb(4, 4);
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    CHECK(icv_fit(rgb, &msg, 2, 2, 1.0) == 0, "icv_fit accepts square downscale");
    CHECK(rgb->width == 2 && rgb->height == 2, "icv_fit output dimensions");
    CHECK(strlen(bu_vls_cstr(&msg)) > 0, "icv_fit reports placement metadata");
    bu_vls_free(&msg);
    icv_destroy(rgb);
}

static void
test_crop_filters_and_stats(void)
{
    icv_image_t *img = make_gray(4, 4);

    CHECK(icv_crop_rect(img, 1, 2, 2, 1) == 0, "icv_crop_rect accepted valid 2x1 crop");
    CHECK(img->width == 2 && img->height == 1, "icv_crop_rect output dimensions");
    CHECK(near_equal(img->data[0], 21.0), "icv_crop_rect copied x=1,y=2 first pixel");
    CHECK(near_equal(img->data[1], 22.0), "icv_crop_rect copied x=2,y=2 second pixel");
    icv_destroy(img);

    img = make_gray(4, 4);
    CHECK(icv_crop_rect(img, 3, 3, 2, 2) == -1, "icv_crop_rect rejects out-of-bounds crop without terminating");
    icv_destroy(img);

    img = make_gray(4, 4);
    CHECK(icv_crop(img, 0, 3, 3, 3, 3, 0, 0, 0, 4, 4) == 0, "icv_crop accepts full-image quadrilateral");
    CHECK(img->width == 4 && img->height == 4, "icv_crop output dimensions");
    CHECK(near_equal(img->data[0], 0.0), "icv_crop preserves lower-left sample");
    CHECK(near_equal(img->data[3], 3.0), "icv_crop preserves lower-right sample");
    CHECK(near_equal(img->data[12], 30.0), "icv_crop preserves upper-left sample");
    CHECK(near_equal(img->data[15], 33.0), "icv_crop preserves upper-right sample");
    icv_destroy(img);

    img = make_gray(4, 4);
    CHECK(icv_crop(img, 0, 3, 3, 3, 2, 0, 1, 0, 4, 4) == 0, "icv_crop accepts decreasing skew coordinates");
    CHECK(near_equal(img->data[0], 1.0), "skew crop preserves lower-left mapped endpoint");
    CHECK(near_equal(img->data[3], 2.0), "skew crop preserves lower-right mapped endpoint");
    CHECK(near_equal(img->data[12], 30.0), "skew crop preserves upper-left mapped endpoint");
    CHECK(near_equal(img->data[15], 33.0), "skew crop preserves upper-right mapped endpoint");
    icv_destroy(img);

    img = make_gray(2, 2);
    double expected[4] = {0.0, 1.0, 10.0, 11.0};
    CHECK(icv_filter(img, ICV_FILTER_NULL) == 0, "icv_filter null is a pass-through");
    CHECK(img->width == 2 && img->height == 2, "narrow filter keeps dimensions");
    for (size_t i = 0; i < 4; i++) {
	CHECK(near_equal(img->data[i], expected[i]), "null filter leaves image unchanged");
    }
    CHECK(icv_filter(img, (ICV_FILTER)999) == -1, "icv_filter rejects invalid filter type");
    icv_destroy(img);

    img = icv_image_create(1, 1, ICV_COLOR_SPACE_GRAY);
    img->data[0] = 0.25;
    CHECK(icv_filter(img, ICV_FILTER_BOXCAR_AVERAGE) == 0, "icv_filter accepts image smaller than kernel");
    CHECK(near_equal(img->data[0], 0.25), "boxcar filter keeps constant single-pixel image with clamped border");
    icv_destroy(img);

    for (int f = ICV_FILTER_LOW_PASS; f <= ICV_FILTER_BOXCAR_AVERAGE; f++) {
	char msg[160] = {0};
	img = make_unit_gray(5, 5);
	snprintf(msg, sizeof(msg), "icv_filter accepts filter enum %d", f);
	CHECK(icv_filter(img, (ICV_FILTER)f) == 0, msg);
	CHECK(img->width == 5 && img->height == 5 && img->channels == 1, "icv_filter preserves image layout");
	for (size_t s = 0; s < img->width * img->height * img->channels; s++) {
	    CHECK(isfinite(img->data[s]), "icv_filter output is finite");
	}
	icv_destroy(img);
    }

    for (int f = ICV_FILTER3_LOW_PASS; f <= ICV_FILTER3_NULL; f++) {
	char msg[160] = {0};
	icv_image_t *old_img = make_rgb(4, 4);
	icv_image_t *curr_img = make_rgb(4, 4);
	icv_image_t *new_img = make_rgb(4, 4);
	icv_image_t *out = icv_filter3(old_img, curr_img, new_img, (ICV_FILTER3)f);
	snprintf(msg, sizeof(msg), "icv_filter3 accepts filter enum %d", f);
	CHECK(out != NULL, msg);
	if (out) {
	    CHECK(out->width == curr_img->width && out->height == curr_img->height, "icv_filter3 output dimensions");
	    CHECK(out->channels == curr_img->channels, "icv_filter3 output channels");
	    for (size_t s = 0; s < out->width * out->height * out->channels; s++) {
		CHECK(isfinite(out->data[s]), "icv_filter3 output is finite");
	    }
	    icv_destroy(out);
	}
	icv_destroy(old_img);
	icv_destroy(curr_img);
	icv_destroy(new_img);
    }

    {
	icv_image_t *old_rgba = make_rgba(3, 3);
	icv_image_t *curr_rgba = make_rgba(3, 3);
	icv_image_t *new_rgba = make_rgba(3, 3);
	curr_rgba->data[(1 * 3 + 1) * 4 + 3] = 0.95;
	icv_image_t *out = icv_filter3(old_rgba, curr_rgba, new_rgba, ICV_FILTER3_NULL);
	CHECK(out != NULL && out->channels == 4 && out->alpha_channel == 1, "icv_filter3 preserves RGBA output layout");
	if (out) {
	    for (size_t s = 0; s < out->width * out->height * out->channels; s++) {
		CHECK(near_equal(out->data[s], curr_rgba->data[s]), "icv_filter3 null returns current frame data");
	    }
	    icv_destroy(out);
	}
	icv_destroy(old_rgba);
	icv_destroy(curr_rgba);
	icv_destroy(new_rgba);
    }

    icv_image_t *old_img = make_rgb(3, 3);
    icv_image_t *curr_img = make_rgb(4, 3);
    icv_image_t *new_img = make_rgb(3, 3);
    CHECK(icv_filter3(old_img, curr_img, new_img, ICV_FILTER3_NULL) == NULL, "icv_filter3 rejects mismatched dimensions");
    icv_destroy(curr_img);
    curr_img = make_rgb(3, 3);
    CHECK(icv_filter3(old_img, curr_img, new_img, (ICV_FILTER3)999) == NULL, "icv_filter3 rejects invalid filter type");
    icv_destroy(old_img);
    icv_destroy(curr_img);
    icv_destroy(new_img);

    img = icv_image_create(5, 1, ICV_COLOR_SPACE_GRAY);
    img->data[0] = -0.10;
    img->data[1] = 0.00;
    img->data[2] = 0.25;
    img->data[3] = 0.99;
    img->data[4] = 1.00;
    double *minv = icv_min(img);
    double *maxv = icv_max(img);
    double *sumv = icv_sum(img);
    double *meanv = icv_mean(img);
    size_t **bins = icv_hist(img, 4);
    int *mode = icv_mode(img, bins, 4);
    int *median = icv_median(img, bins, 4);
    double *var = icv_var(img, bins, 4);
    double *skew = icv_skew(img, bins, 4);

    CHECK(near_equal(minv[0], -0.10), "icv_min preserves values below zero");
    CHECK(near_equal(maxv[0], 1.00), "icv_max finds max");
    CHECK(near_equal(sumv[0], 2.14), "icv_sum sums all samples");
    CHECK(near_equal(meanv[0], 2.14 / 5.0), "icv_mean averages all samples");
    CHECK(bins[0][0] == 2 && bins[0][1] == 1 && bins[0][2] == 0 && bins[0][3] == 2,
	  "icv_hist clamps negative and maximum values");
    CHECK(mode[0] == 0, "icv_mode returns first most-populated bin");
    CHECK(median[0] == 1, "icv_median is based on sample counts");
    CHECK(var[0] >= 0.0, "icv_var returns non-negative variance");
    CHECK(isfinite(skew[0]), "icv_skew returns finite skewness");

    bu_free(minv, "min values");
    bu_free(maxv, "max values");
    bu_free(sumv, "sum values");
    bu_free(meanv, "mean values");
    bu_free(mode, "mode values");
    bu_free(median, "median values");
    bu_free(var, "variance values");
    bu_free(skew, "skew values");
    free_bins(bins, img->channels);
    icv_destroy(img);
}

static void
test_memory_codecs(void)
{
    struct fmt {
	bu_mime_image_t mime;
	const char *name;
	int gray_ok;
	int roundtrip;
	int raw_size_required;
    } fmts[] = {
	{BU_MIME_IMAGE_PIX, "PIX", 1, 1, 1},
	{BU_MIME_IMAGE_PPM, "PPM", 1, 1, 0},
	{BU_MIME_IMAGE_JPEG, "JPEG", 1, 1, 0},
	{BU_MIME_IMAGE_DPIX, "DPIX", 1, 1, 1},
	{BU_MIME_IMAGE_PNG, "PNG", 1, 1, 0},
	{BU_MIME_IMAGE_BW, "BW", 1, 1, 1},
	{BU_MIME_IMAGE_RLE, "RLE", 0, 1, 0}
    };

    CHECK(icv_write_mem(NULL, NULL, NULL, BU_MIME_IMAGE_PIX) != 0, "icv_write_mem rejects null arguments");
    CHECK(icv_read_mem(NULL, 0, BU_MIME_IMAGE_PIX, 0, 0) == NULL, "icv_read_mem rejects null buffers");
    {
	unsigned char garbage[] = {0, 1, 2, 3, 4, 5};
	unsigned char bad_ppm[] = {'P', '6', '\n', 'x', ' ', 'y', '\n'};
	double dpix_values[] = {-1.0, 1.0, 3.0, 2.0, 2.0, 2.0};
	CHECK(icv_read_mem(garbage, 3, BU_MIME_IMAGE_PIX, 2, 2) == NULL, "icv_read_mem rejects undersized PIX buffers");
	CHECK(icv_read_mem(garbage, 3, BU_MIME_IMAGE_BW, 2, 2) == NULL, "icv_read_mem rejects undersized BW buffers");
	CHECK(icv_read_mem(garbage, 8, BU_MIME_IMAGE_DPIX, 2, 2) == NULL, "icv_read_mem rejects undersized DPIX buffers");
	CHECK(icv_read_mem(bad_ppm, sizeof(bad_ppm), BU_MIME_IMAGE_PPM, 0, 0) == NULL, "icv_read_mem rejects invalid PPM buffers");
	CHECK(icv_read_mem(garbage, sizeof(garbage), BU_MIME_IMAGE_PNG, 0, 0) == NULL, "icv_read_mem rejects invalid PNG buffers");
	CHECK(icv_read_mem(garbage, sizeof(garbage), BU_MIME_IMAGE_JPEG, 0, 0) == NULL, "icv_read_mem rejects invalid JPEG buffers");
	CHECK(icv_read_mem(garbage, sizeof(garbage), BU_MIME_IMAGE_RLE, 0, 0) == NULL, "icv_read_mem rejects invalid RLE buffers");

	icv_image_t *pix_auto = icv_read_mem(garbage, sizeof(garbage), BU_MIME_IMAGE_PIX, 0, 0);
	CHECK(pix_auto != NULL && pix_auto->width == 2 && pix_auto->height == 1, "PIX read_mem auto-deduces one-row width");
	if (pix_auto) icv_destroy(pix_auto);
	icv_image_t *bw_auto = icv_read_mem(garbage, sizeof(garbage), BU_MIME_IMAGE_BW, 0, 0);
	CHECK(bw_auto != NULL && bw_auto->width == sizeof(garbage) && bw_auto->height == 1, "BW read_mem auto-deduces one-row width");
	if (bw_auto) icv_destroy(bw_auto);
	icv_image_t *dpix_norm = icv_read_mem((const unsigned char *)dpix_values, sizeof(dpix_values), BU_MIME_IMAGE_DPIX, 1, 1);
	CHECK(dpix_norm != NULL, "DPIX read_mem accepts double RGB samples");
	if (dpix_norm) {
	    CHECK(near_equal(dpix_norm->data[0], 0.0), "DPIX read_mem normalizes low value");
	    CHECK(near_equal(dpix_norm->data[1], 0.5), "DPIX read_mem normalizes middle value");
	    CHECK(near_equal(dpix_norm->data[2], 1.0), "DPIX read_mem normalizes high value");
	    icv_destroy(dpix_norm);
	}
	dpix_norm = icv_read_mem((const unsigned char *)(dpix_values + 3), 3 * sizeof(double), BU_MIME_IMAGE_DPIX, 1, 1);
	CHECK(dpix_norm != NULL, "DPIX read_mem accepts constant out-of-range samples");
	if (dpix_norm) {
	    CHECK(near_equal(dpix_norm->data[0], 1.0) && near_equal(dpix_norm->data[1], 1.0) && near_equal(dpix_norm->data[2], 1.0),
		  "DPIX read_mem sanitizes constant out-of-range samples");
	    icv_destroy(dpix_norm);
	}
    }

    {
	bu_mime_image_t rgb_fmts[] = {BU_MIME_IMAGE_PIX, BU_MIME_IMAGE_PPM, BU_MIME_IMAGE_DPIX, BU_MIME_IMAGE_JPEG};
	const char *rgb_names[] = {"PIX", "PPM", "DPIX", "JPEG"};
	for (size_t i = 0; i < sizeof(rgb_fmts)/sizeof(rgb_fmts[0]); i++) {
	    icv_image_t *gray = make_unit_gray(2, 1);
	    double before[2] = {gray->data[0], gray->data[1]};
	    unsigned char *buf = NULL;
	    size_t size = 0;
	    char msg[160] = {0};
	    snprintf(msg, sizeof(msg), "%s writer accepts gray input without mutating it", rgb_names[i]);
	    CHECK(icv_write_mem(gray, &buf, &size, rgb_fmts[i]) == 0, msg);
	    CHECK(gray->color_space == ICV_COLOR_SPACE_GRAY && gray->channels == 1 && gray->alpha_channel == 0,
		  "RGB-family writer preserves grayscale layout");
	    CHECK(near_equal(gray->data[0], before[0]) && near_equal(gray->data[1], before[1]),
		  "RGB-family writer preserves grayscale data");
	    if (buf) bu_free(buf, "non-mutating writer test buffer");
	    icv_destroy(gray);
	}

	icv_image_t *rgb = make_unit_rgb(2, 1);
	double before_rgb[6];
	unsigned char *buf = NULL;
	size_t size = 0;
	memcpy(before_rgb, rgb->data, sizeof(before_rgb));
	CHECK(icv_write_mem(rgb, &buf, &size, BU_MIME_IMAGE_BW) == 0, "BW writer accepts RGB input without mutating it");
	CHECK(rgb->color_space == ICV_COLOR_SPACE_RGB && rgb->channels == 3 && rgb->alpha_channel == 0,
	      "BW writer preserves RGB layout");
	CHECK(memcmp(before_rgb, rgb->data, sizeof(before_rgb)) == 0, "BW writer preserves RGB data");
	if (buf) bu_free(buf, "non-mutating BW writer buffer");
	icv_destroy(rgb);

	icv_image_t *rgba = make_rgba(2, 1);
	double before_rgba[8];
	buf = NULL;
	size = 0;
	memcpy(before_rgba, rgba->data, sizeof(before_rgba));
	CHECK(icv_write_mem(rgba, &buf, &size, BU_MIME_IMAGE_PIX) == 0, "PIX writer projects RGBA to RGB without mutating input");
	CHECK(rgba->channels == 4 && rgba->alpha_channel == 1, "PIX writer preserves RGBA layout on input");
	CHECK(memcmp(before_rgba, rgba->data, sizeof(before_rgba)) == 0, "PIX writer preserves RGBA data on input");
	if (buf) bu_free(buf, "non-mutating RGBA writer buffer");
	icv_destroy(rgba);
    }

    for (size_t i = 0; i < sizeof(fmts)/sizeof(fmts[0]); i++) {
	icv_image_t *img = fmts[i].gray_ok ? make_unit_gray(3, 2) : make_unit_rgb(3, 2);
	unsigned char *buf = NULL;
	size_t size = 0;
	int ret = icv_write_mem(img, &buf, &size, fmts[i].mime);
	char msg[160] = {0};
	snprintf(msg, sizeof(msg), "icv_write_mem accepts expected input for %s", fmts[i].name);
	CHECK(ret == 0, msg);
	snprintf(msg, sizeof(msg), "icv_write_mem produced bytes for %s", fmts[i].name);
	CHECK(ret != 0 || (buf != NULL && size > 0), msg);

	if (ret == 0 && buf && fmts[i].roundtrip) {
	    size_t rw = fmts[i].raw_size_required ? img->width : 0;
	    size_t rh = fmts[i].raw_size_required ? img->height : 0;
	    icv_image_t *read_img = icv_read_mem(buf, size, fmts[i].mime, rw, rh);
	    snprintf(msg, sizeof(msg), "icv_read_mem reads %s buffer", fmts[i].name);
	    CHECK(read_img != NULL, msg);
	    if (read_img) {
		snprintf(msg, sizeof(msg), "icv_read_mem preserves width for %s", fmts[i].name);
		CHECK(read_img->width == img->width, msg);
		snprintf(msg, sizeof(msg), "icv_read_mem preserves height for %s", fmts[i].name);
		CHECK(read_img->height == img->height, msg);
		snprintf(msg, sizeof(msg), "icv_read_mem has usable channel count for %s", fmts[i].name);
		CHECK(read_img->channels == 1 || read_img->channels == 3, msg);
		snprintf(msg, sizeof(msg), "icv_read_mem returns initialized image for %s", fmts[i].name);
		CHECK(read_img->magic == ICV_IMAGE_MAGIC, msg);
		if (fmts[i].mime != BU_MIME_IMAGE_JPEG) {
		    CHECK(isfinite(read_img->data[0]), "roundtrip data is finite");
		    CHECK(isfinite(read_img->data[read_img->width * read_img->height * read_img->channels - 1]),
			  "roundtrip final data is finite");
		}
		icv_destroy(read_img);
	    }
	}

	if (buf) bu_free(buf, "icv_write_mem test buffer");
	icv_destroy(img);
    }
}

static int
write_read_temp_image(icv_image_t *img, bu_mime_image_t fmt, const char *name, size_t raw_w, size_t raw_h)
{
    char path[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(path, MAXPATHLEN);
    char msg[160] = {0};

    snprintf(msg, sizeof(msg), "bu_temp_file creates path for %s", name);
    CHECK(fp != NULL && path[0] != '\0', msg);
    if (!fp)
	return -1;
    fclose(fp);

    snprintf(msg, sizeof(msg), "icv_write writes %s file", name);
    CHECK(icv_write(img, path, fmt) == 0, msg);

    icv_image_t *read_img = icv_read(path, fmt, raw_w, raw_h);
    snprintf(msg, sizeof(msg), "icv_read reads %s file", name);
    CHECK(read_img != NULL, msg);
    if (read_img) {
	snprintf(msg, sizeof(msg), "icv_read preserves width for %s", name);
	CHECK(read_img->width == img->width, msg);
	snprintf(msg, sizeof(msg), "icv_read preserves height for %s", name);
	CHECK(read_img->height == img->height, msg);
	snprintf(msg, sizeof(msg), "icv_read returns initialized image for %s", name);
	CHECK(read_img->magic == ICV_IMAGE_MAGIC, msg);
	icv_destroy(read_img);
    }

    bu_file_delete(path);
    return 0;
}

static int
write_temp_bytes(const unsigned char *data, size_t data_len, char *path, size_t path_len)
{
    FILE *fp = bu_temp_file(path, path_len);
    CHECK(fp != NULL && path[0] != '\0', "bu_temp_file creates path for raw bytes");
    if (!fp)
	return -1;
    CHECK(fwrite(data, 1, data_len, fp) == data_len, "wrote raw test bytes");
    fclose(fp);
    return 0;
}

static int
make_temp_path(char *path, size_t path_len)
{
    FILE *fp = bu_temp_file(path, path_len);
    CHECK(fp != NULL && path[0] != '\0', "bu_temp_file creates output path");
    if (!fp)
	return -1;
    fclose(fp);
    return 0;
}

static int
check_file_bytes(const char *path, const unsigned char *expected, size_t expected_len, const char *name)
{
    unsigned char buf[64] = {0};
    FILE *fp = fopen(path, "rb");
    char msg[160] = {0};
    size_t got = 0;

    snprintf(msg, sizeof(msg), "opened %s output", name);
    CHECK(fp != NULL, msg);
    if (!fp)
	return -1;

    got = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    snprintf(msg, sizeof(msg), "%s output byte count", name);
    CHECK(got == expected_len, msg);
    snprintf(msg, sizeof(msg), "%s output byte content", name);
    if (got == expected_len && memcmp(buf, expected, expected_len) != 0) {
	bu_log("%s expected:", name);
	for (size_t i = 0; i < expected_len; i++)
	    bu_log(" %u", (unsigned)expected[i]);
	bu_log(" got:");
	for (size_t i = 0; i < got; i++)
	    bu_log(" %u", (unsigned)buf[i]);
	bu_log("\n");
    }
    CHECK(got == expected_len && memcmp(buf, expected, expected_len) == 0, msg);
    return 0;
}

static void
test_rot_file_api(void)
{
    const unsigned char raw[] = {1, 2, 3, 4};
    const unsigned char reverse_expected[] = {2, 1, 4, 3};
    const unsigned char invert_expected[] = {3, 4, 1, 2};
    char in_path[MAXPATHLEN] = {0};
    char out_path[MAXPATHLEN] = {0};

    if (write_temp_bytes(raw, sizeof(raw), in_path, sizeof(in_path)) != 0)
	return;
    if (make_temp_path(out_path, sizeof(out_path)) != 0) {
	bu_file_delete(in_path);
	return;
    }

    {
	const char *argv[] = {"icv_rot", "-#", "1", "-s", "2", "-o", out_path, in_path};
	CHECK(icv_rot(sizeof(argv)/sizeof(argv[0]), argv) == 0, "icv_rot copies image with no transform");
	check_file_bytes(out_path, raw, sizeof(raw), "icv_rot copy");
    }

    {
	const char *argv[] = {"icv_rot", "-r", "-#", "1", "-s", "2", "-o", out_path, in_path};
	CHECK(icv_rot(sizeof(argv)/sizeof(argv[0]), argv) == 0, "icv_rot reverses scanlines");
	check_file_bytes(out_path, reverse_expected, sizeof(reverse_expected), "icv_rot reverse");
    }

    {
	const char *argv[] = {"icv_rot", "-i", "-#", "1", "-s", "2", "-o", out_path, in_path};
	CHECK(icv_rot(sizeof(argv)/sizeof(argv[0]), argv) == 0, "icv_rot inverts scanline order");
	check_file_bytes(out_path, invert_expected, sizeof(invert_expected), "icv_rot invert");
    }

    {
	const char *argv[] = {"icv_rot", "-a", "360", "-#", "1", "-s", "2", "-o", out_path, in_path};
	CHECK(icv_rot(sizeof(argv)/sizeof(argv[0]), argv) == 0, "icv_rot accepts arbitrary-angle rotation");
	check_file_bytes(out_path, raw, sizeof(raw), "icv_rot 360 degrees");
    }

    bu_file_delete(in_path);
    bu_file_delete(out_path);
}

static void
test_file_codecs_and_pdiff(void)
{
    struct fmt {
	bu_mime_image_t mime;
	const char *name;
	int gray_input;
	int raw_size_required;
    } fmts[] = {
	{BU_MIME_IMAGE_PIX, "PIX", 1, 1},
	{BU_MIME_IMAGE_BW, "BW", 0, 1},
	{BU_MIME_IMAGE_DPIX, "DPIX", 1, 1},
	{BU_MIME_IMAGE_PPM, "PPM", 1, 0},
	{BU_MIME_IMAGE_PNG, "PNG", 1, 0},
	{BU_MIME_IMAGE_JPEG, "JPEG", 1, 0},
	{BU_MIME_IMAGE_RLE, "RLE", 0, 0}
    };

    for (size_t i = 0; i < sizeof(fmts)/sizeof(fmts[0]); i++) {
	icv_image_t *img = fmts[i].gray_input ? make_unit_gray(4, 3) : make_unit_rgb(4, 3);
	size_t rw = fmts[i].raw_size_required ? img->width : 0;
	size_t rh = fmts[i].raw_size_required ? img->height : 0;
	write_read_temp_image(img, fmts[i].mime, fmts[i].name, rw, rh);
	icv_destroy(img);
    }

    {
	unsigned char pix_bytes[] = {10, 20, 30, 40, 50, 60};
	unsigned char bw_bytes[] = {1, 2, 3, 4};
	char pix_path[MAXPATHLEN] = {0};
	char bw_path[MAXPATHLEN] = {0};
	struct bu_vls prefixed = BU_VLS_INIT_ZERO;

	if (write_temp_bytes(pix_bytes, sizeof(pix_bytes), pix_path, sizeof(pix_path)) == 0) {
	    icv_image_t *auto_pix = icv_read(pix_path, BU_MIME_IMAGE_AUTO, 2, 1);
	    CHECK(auto_pix != NULL && auto_pix->width == 2 && auto_pix->height == 1, "icv_read AUTO guesses PIX extension fallback");
	    if (auto_pix) icv_destroy(auto_pix);
	    bu_vls_sprintf(&prefixed, "PIX:%s", pix_path);
	    auto_pix = icv_read(bu_vls_cstr(&prefixed), BU_MIME_IMAGE_AUTO, 2, 1);
	    CHECK(auto_pix != NULL && auto_pix->width == 2 && auto_pix->height == 1, "icv_read AUTO honors PIX: filename prefix");
	    if (auto_pix) icv_destroy(auto_pix);
	    bu_file_delete(pix_path);
	}

	if (write_temp_bytes(bw_bytes, sizeof(bw_bytes), bw_path, sizeof(bw_path)) == 0) {
	    bu_vls_trunc(&prefixed, 0);
	    bu_vls_sprintf(&prefixed, "BW:%s", bw_path);
	    icv_image_t *auto_bw = icv_read(bu_vls_cstr(&prefixed), BU_MIME_IMAGE_AUTO, 0, 0);
	    CHECK(auto_bw != NULL && auto_bw->width == sizeof(bw_bytes) && auto_bw->height == 1, "icv_read AUTO honors BW: filename prefix");
	    if (auto_bw) icv_destroy(auto_bw);
	    bu_file_delete(bw_path);
	}
	bu_vls_free(&prefixed);
    }

    icv_image_t *a = make_unit_rgb(8, 8);
    icv_image_t *b = make_unit_rgb(8, 8);
    fastf_t d0 = icv_adiff(a, b, ICV_DIFF_PHASH);
    char msg[160] = {0};
    snprintf(msg, sizeof(msg), "icv_adiff reports identical images as zero distance, got %g", d0);
    CHECK(NEAR_ZERO(d0, SMALL_FASTF), msg);
    b->data[(7 * 8 + 7) * 3 + 0] = 1.0 - b->data[(7 * 8 + 7) * 3 + 0];
    fastf_t d1 = icv_adiff(a, b, ICV_DIFF_PHASH);
    snprintf(msg, sizeof(msg), "icv_adiff reports changed images as nonzero distance, got %g", d1);
    CHECK(d1 > 0, msg);
    icv_destroy(a);
    icv_destroy(b);

    a = make_unit_gray(8, 8);
    b = make_unit_gray(8, 8);
    CHECK(NEAR_EQUAL(icv_adiff(a, b, ICV_DIFF_PHASH), -1.0, SMALL_FASTF), "icv_adiff rejects non-RGB images");
    icv_destroy(a);
    icv_destroy(b);

    a = make_solid_rgb(3, 5, 0.2, 0.4, 0.6);
    b = make_solid_rgb(19, 11, 0.2, 0.4, 0.6);
    d0 = icv_adiff(a, b, ICV_DIFF_PHASH);
    snprintf(msg, sizeof(msg), "icv_adiff rejects mismatched dimensions, got %g", d0);
    CHECK(NEAR_EQUAL(d0, -1.0, SMALL_FASTF), msg);
    icv_destroy(a);
    icv_destroy(b);

    a = make_solid_rgb(8, 8, 2.0, 2.0, 2.0);
    b = make_solid_rgb(8, 8, 1.0, 1.0, 1.0);
    d0 = icv_adiff(a, b, ICV_DIFF_PHASH);
    snprintf(msg, sizeof(msg), "icv_adiff clamps high sample conversion, got %g", d0);
    CHECK(NEAR_ZERO(d0, SMALL_FASTF), msg);
    icv_destroy(a);
    icv_destroy(b);

    a = make_solid_rgb(8, 8, -1.0, -1.0, -1.0);
    b = make_solid_rgb(8, 8, 0.0, 0.0, 0.0);
    d0 = icv_adiff(a, b, ICV_DIFF_PHASH);
    snprintf(msg, sizeof(msg), "icv_adiff clamps low sample conversion, got %g", d0);
    CHECK(NEAR_ZERO(d0, SMALL_FASTF), msg);
    icv_destroy(a);
    icv_destroy(b);

    a = make_unit_rgb(5, 9);
    b = make_unit_rgb(5, 9);
    for (size_t i = 0; i < b->width * b->height * b->channels; i++) {
	b->data[i] = 1.0 - b->data[i];
    }
    d1 = icv_adiff(a, b, ICV_DIFF_PHASH);
    snprintf(msg, sizeof(msg), "icv_adiff detects rectangular image content changes, got %g", d1);
    CHECK(d1 > 0, msg);
    icv_destroy(a);
    icv_destroy(b);

    CHECK(NEAR_EQUAL(icv_adiff(NULL, NULL, ICV_DIFF_PHASH), -1.0, SMALL_FASTF), "icv_adiff rejects null images");
}

static void
test_ascii_render_info_and_diff(void)
{
    icv_image_t *img1 = make_unit_rgb(2, 1);
    icv_image_t *img2 = make_unit_rgb(2, 1);
    struct icv_ascii_art_params artparams = ICV_ASCII_ART_PARAMS_DEFAULT;
    char *art = icv_ascii_art(img1, &artparams);
    CHECK(art != NULL && strlen(art) > 0, "icv_ascii_art returns non-empty text");
    if (art) bu_free(art, "ascii art");
    CHECK(icv_ascii_art(NULL, &artparams) == NULL, "icv_ascii_art rejects null image");

    CHECK(icv_image_get_render_info(img1) == NULL, "icv_image_get_render_info reports absent metadata");
    struct icv_render_info *ri1 = icv_render_info_create();
    struct icv_render_info *ri2 = icv_render_info_create();
    fill_render_info(ri1, "model.g", "box.s");
    fill_render_info(ri2, "model.g", "box.s");
    CHECK(icv_image_take_render_info(img1, ri1) == 0, "icv_image_take_render_info attaches metadata");
    CHECK(icv_image_take_render_info(img2, ri2) == 0, "icv_image_take_render_info attaches comparison metadata");
    CHECK(icv_image_get_render_info(img1) == ri1, "icv_image_take_render_info stores metadata pointer");
    struct icv_render_info *ri_bad = icv_render_info_create();
    CHECK(icv_image_take_render_info(NULL, ri_bad) == -1, "icv_image_take_render_info rejects null image without taking ownership");
    CHECK(icv_render_info_destroy(ri_bad) == 0, "icv_render_info_destroy releases unattached metadata");

    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    CHECK(icv_diff_render_info(img1, img2, &msgs) == 0, "icv_diff_render_info accepts identical metadata");
    CHECK(strlen(bu_vls_cstr(&msgs)) > 0, "icv_diff_render_info writes comparison report");
    bu_vls_trunc(&msgs, 0);
    img2->render_info->viewsize = 3.0;
    CHECK(icv_diff_render_info(img1, img2, &msgs) == 1, "icv_diff_render_info detects metadata differences");
    bu_vls_free(&msgs);
    CHECK(icv_image_take_render_info(img1, NULL) == 0, "icv_image_take_render_info clears metadata");
    CHECK(icv_image_take_render_info(img2, NULL) == 0, "icv_image_take_render_info clears comparison metadata");
    CHECK(icv_diff_render_info(img1, img2, NULL) == -1, "icv_diff_render_info reports missing metadata");

    int matching = 0;
    int off_by_1 = 0;
    int off_by_many = 0;
    CHECK(icv_diff(&matching, &off_by_1, &off_by_many, img1, img2) == 0, "icv_diff reports identical images");
    CHECK(matching == 6 && off_by_1 == 0 && off_by_many == 0, "icv_diff counts matching channels");
    img2->data[0] += 1.0 / 255.0;
    img2->data[4] += 3.0 / 255.0;
    matching = off_by_1 = off_by_many = 0;
    CHECK(icv_diff(&matching, &off_by_1, &off_by_many, img1, img2) == 1, "icv_diff reports channel differences");
    CHECK(matching == 4 && off_by_1 == 1 && off_by_many == 1, "icv_diff classifies difference magnitudes");

    icv_image_t *diff = icv_diffimg(img1, img2);
    CHECK(diff != NULL, "icv_diffimg returns visual diff image");
    if (diff) {
	CHECK(diff->width == img1->width && diff->height == img1->height && diff->channels == 3,
	      "icv_diffimg preserves dimensions and RGB layout");
	CHECK(diff->data[0] > 0.70 && diff->data[0] < 0.80, "icv_diffimg marks off-by-one channel");
	CHECK(diff->data[4] > 0.99, "icv_diffimg marks larger channel difference");
	icv_destroy(diff);
    }

    icv_image_t *gray = make_gray(2, 1);
    CHECK(icv_diffimg(img1, gray) == NULL, "icv_diffimg rejects channel mismatches");
    icv_destroy(gray);

    icv_destroy(img1);
    icv_destroy(img2);

    img1 = make_unit_rgb(1, 1);
    img2 = make_unit_rgb(1, 1);
    img2->data[0] = 1.0;
    ri1 = icv_render_info_create();
    fill_render_info(ri1, "model.g", "box.s");
    CHECK(icv_image_take_render_info(img1, ri1) == 0, "icv_image_take_render_info attaches nirt metadata");
    FILE *nirt = tmpfile();
    CHECK(nirt != NULL, "tmpfile available for nirt shot test");
    if (nirt) {
	CHECK(icv_diff_nirt_shots(img1, img2, nirt, NULL) == 1, "icv_diff_nirt_shots writes one differing-pixel shot");
	fclose(nirt);
    }
    icv_destroy(img1);
    icv_destroy(img2);
}

static void
test_animation_frame_management(void)
{
    icv_anim_t *anim = icv_anim_create(ICV_ANIM_APNG, 0, 0, 10);
    icv_image_t *a = make_unit_rgb(2, 2);
    icv_image_t *b = make_unit_rgb(2, 2);
    icv_image_t *c = make_unit_rgb(2, 2);
    b->data[0] = 0.75;
    c->data[0] = 0.25;

    CHECK(anim != NULL, "icv_anim_create returns animation");
    CHECK(icv_anim_num_frames(anim) == 0, "new animation starts empty");
    CHECK(icv_anim_add_frame(anim, a) == 0, "icv_anim_add_frame appends frame");
    CHECK(icv_anim_num_frames(anim) == 1, "icv_anim_add_frame updates count");
    CHECK(icv_anim_insert_frame(anim, 0, b) == 0, "icv_anim_insert_frame inserts at beginning");
    CHECK(icv_anim_num_frames(anim) == 2, "icv_anim_insert_frame updates count");
    CHECK(icv_anim_set_frame_delay(anim, 0, 12345) == 0, "icv_anim_set_frame_delay accepts valid index");
    CHECK(icv_anim_set_frame_delay(anim, 10, 12345) == -1, "icv_anim_set_frame_delay rejects invalid index");
    CHECK(icv_anim_add_frame(NULL, a) == -1, "icv_anim_add_frame rejects null animation");
    CHECK(icv_anim_insert_frame(anim, 10, b) == -1, "icv_anim_insert_frame rejects out-of-range insertion");
    CHECK(icv_anim_replace_frame(anim, 10, c) == -1, "icv_anim_replace_frame rejects invalid index");

    icv_image_t *rgba = make_rgba(2, 2);
    CHECK(icv_anim_add_frame(anim, rgba) == 0, "icv_anim_add_frame appends RGBA frame");
    icv_image_t *rgba_copy = icv_anim_get_frame(anim, icv_anim_num_frames(anim) - 1);
    CHECK(rgba_copy != NULL && rgba_copy->channels == 4 && rgba_copy->alpha_channel == 1,
	  "icv_anim_get_frame preserves RGBA frame layout");
    if (rgba_copy) {
	CHECK(near_equal(rgba_copy->data[3], rgba->data[3]), "icv_anim_get_frame preserves alpha data");
	icv_destroy(rgba_copy);
    }
    CHECK(icv_anim_remove_frame(anim, icv_anim_num_frames(anim) - 1) == 0, "icv_anim_remove_frame removes RGBA test frame");
    icv_destroy(rgba);

    icv_image_t *copy = icv_anim_get_frame(anim, 0);
    CHECK(copy != NULL, "icv_anim_get_frame returns frame copy");
    if (copy) {
	CHECK(copy->width == b->width && copy->height == b->height && copy->channels == b->channels,
	      "icv_anim_get_frame preserves frame layout");
	CHECK(near_equal(copy->data[0], 0.75), "icv_anim_get_frame copies frame data");
	b->data[0] = 0.0;
	CHECK(near_equal(copy->data[0], 0.75), "icv_anim_get_frame returns independent copy");
	icv_destroy(copy);
    }

    CHECK(icv_anim_replace_frame(anim, 1, c) == 0, "icv_anim_replace_frame accepts valid index");
    copy = icv_anim_get_frame(anim, 1);
    CHECK(copy != NULL && near_equal(copy->data[0], 0.25), "icv_anim_replace_frame updates frame data");
    if (copy) icv_destroy(copy);
    CHECK(icv_anim_remove_frame(anim, 0) == 0, "icv_anim_remove_frame accepts valid index");
    CHECK(icv_anim_num_frames(anim) == 1, "icv_anim_remove_frame updates count");
    CHECK(icv_anim_get_frame(anim, 5) == NULL, "icv_anim_get_frame rejects invalid index");
    CHECK(icv_anim_remove_frame(anim, 5) == -1, "icv_anim_remove_frame rejects invalid index");
    CHECK(icv_anim_set_fps(anim, 24) == 0, "icv_anim_set_fps accepts positive FPS");
    CHECK(icv_anim_set_fps(anim, 0) == -1, "icv_anim_set_fps rejects invalid FPS");

    char path[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(path, MAXPATHLEN);
    CHECK(fp != NULL && path[0] != '\0', "bu_temp_file creates path for animation");
    if (fp) {
	fclose(fp);
	CHECK(icv_anim_write(anim, path) == 0, "icv_anim_write writes APNG animation");
	icv_anim_t *read_anim = icv_anim_read(path);
	CHECK(read_anim != NULL, "icv_anim_read reads APNG animation");
	if (read_anim) {
	    CHECK(icv_anim_num_frames(read_anim) == 1, "icv_anim_read preserves frame count");
	    icv_image_t *read_frame = icv_anim_get_frame(read_anim, 0);
	    CHECK(read_frame != NULL, "icv_anim_get_frame reads decoded frame");
	    if (read_frame) {
		CHECK(read_frame->width == c->width && read_frame->height == c->height,
		      "decoded animation frame preserves dimensions");
		icv_destroy(read_frame);
	    }
	    icv_anim_destroy(read_anim);
	}
	bu_file_delete(path);
    }

    icv_anim_destroy(anim);

    anim = icv_anim_create(ICV_ANIM_MJPG, 0, 0, 12);
    CHECK(anim != NULL, "icv_anim_create returns MJPG animation");
    if (anim) {
	CHECK(icv_anim_write(anim, "unused.avi") == -1, "icv_anim_write rejects empty animations");
	CHECK(icv_anim_add_frame(anim, a) == 0, "icv_anim_add_frame appends MJPG frame");
	CHECK(icv_anim_add_frame(anim, c) == 0, "icv_anim_add_frame appends second MJPG frame");
	char mjpg_path[MAXPATHLEN] = {0};
	FILE *mjpg_fp = bu_temp_file(mjpg_path, MAXPATHLEN);
	CHECK(mjpg_fp != NULL && mjpg_path[0] != '\0', "bu_temp_file creates path for MJPG animation");
	if (mjpg_fp) {
	    fclose(mjpg_fp);
	    CHECK(icv_anim_write(anim, mjpg_path) == 0, "icv_anim_write writes MJPG animation");
	    icv_anim_t *read_mjpg = icv_anim_read(mjpg_path);
	    CHECK(read_mjpg != NULL, "icv_anim_read reads MJPG animation");
	    if (read_mjpg) {
		CHECK(icv_anim_num_frames(read_mjpg) == 2, "icv_anim_read preserves MJPG frame count");
		icv_image_t *read_frame = icv_anim_get_frame(read_mjpg, 0);
		CHECK(read_frame != NULL, "icv_anim_get_frame extracts MJPG frame");
		if (read_frame) {
		    CHECK(read_frame->width == a->width && read_frame->height == a->height,
			  "decoded MJPG frame preserves dimensions");
		    icv_destroy(read_frame);
		}
		icv_anim_destroy(read_mjpg);
	    }
	    bu_file_delete(mjpg_path);
	}
	icv_anim_destroy(anim);
    }

    anim = icv_anim_create(ICV_ANIM_UNKNOWN, 0, 0, 10);
    CHECK(anim != NULL, "icv_anim_create permits unknown format object");
    if (anim) {
	CHECK(icv_anim_add_frame(anim, a) == 0, "icv_anim_add_frame appends unknown-format frame");
	CHECK(icv_anim_write(anim, "unused.anim") == -1, "icv_anim_write rejects unknown animation format");
	icv_anim_destroy(anim);
    }

    CHECK(icv_anim_read(NULL) == NULL, "icv_anim_read rejects null path");
    CHECK(icv_anim_write(NULL, "unused.anim") == -1, "icv_anim_write rejects null animation");
    icv_destroy(a);
    icv_destroy(b);
    icv_destroy(c);
}

int
main(int argc, char **argv)
{
    (void)argc;
    bu_setprogname(argv[0]);

    test_create_zero_and_pixel_io();
    test_data_conversion_and_size_guessing();
    test_colorspace_conversions();
    test_scalar_and_image_operations();
    test_saturate_fade_resize_and_fit();
    test_crop_filters_and_stats();
    test_memory_codecs();
    test_file_codecs_and_pdiff();
    test_rot_file_api();
    test_ascii_render_info_and_diff();
    test_animation_frame_management();

    if (failures) {
	bu_log("%d libicv robustness checks failed\n", failures);
	return 1;
    }

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
