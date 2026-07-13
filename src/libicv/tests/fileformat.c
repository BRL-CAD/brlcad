/*              I C V _ F I L E F O R M A T _ T E S T . C
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
/** @file fileformat.c
 *
 * Tests for LIBICV's automatic file format handling.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "icv.h"

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(_cond, _msg) do { \
    tests_run++; \
    if (_cond) { \
	tests_passed++; \
    } else { \
	bu_log("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, _msg); \
    } \
} while (0)


static icv_image_t *
make_pixel(void)
{
    icv_image_t *img = icv_create(1, 1, ICV_COLOR_SPACE_RGB);
    if (!img)
	return NULL;

    img->data[0] = 1.0;
    img->data[1] = 128.0 / 255.0;
    img->data[2] = 0.0;
    return img;
}


static void
check_pixel_file(const char *path, const char *msg)
{
    icv_image_t *img = icv_read(path, BU_MIME_IMAGE_AUTO, 1, 1);
    unsigned char *data = NULL;

    CHECK(img != NULL, msg);
    if (!img)
	return;

    CHECK(img->width == 1, "round-tripped image width is 1");
    CHECK(img->height == 1, "round-tripped image height is 1");
    CHECK(img->channels == 3, "round-tripped image has RGB channels");

    data = icv_data2uchar(img);
    CHECK(data != NULL, "round-tripped image converts to uchar data");
    if (data) {
	CHECK(data[0] == 255, "round-tripped red channel is preserved");
	CHECK(data[1] == 128, "round-tripped green channel is preserved");
	CHECK(data[2] == 0, "round-tripped blue channel is preserved");
	bu_free(data, "uchar data");
    }

    icv_destroy(img);
}


static void
write_auto_pix(const char *path, const char *msg)
{
    icv_image_t *img = make_pixel();
    int ret = BRLCAD_ERROR;

    CHECK(img != NULL, "created source pixel image");
    if (!img)
	return;

    (void)remove(path);
    ret = icv_write(img, path, BU_MIME_IMAGE_AUTO);
    CHECK(ret == BRLCAD_OK, msg);
    CHECK(bu_file_exists(path, NULL), "AUTO write created the target file");
    icv_destroy(img);

    if (ret == BRLCAD_OK)
	check_pixel_file(path, "AUTO-written PIX file reads back");

    (void)remove(path);
}


static void
test_auto_write_short_filename(void)
{
    write_auto_pix("x", "AUTO write handles a one-character filename");
}


static void
test_auto_write_percent_filename(void)
{
    write_auto_pix("icv_percent_%s.pix", "AUTO write handles percent characters in filenames");
}


static void
test_auto_write_prefixed_filename(void)
{
    icv_image_t *img = make_pixel();
    int ret = BRLCAD_ERROR;

    CHECK(img != NULL, "created source pixel image for prefixed write");
    if (!img)
	return;

    (void)remove("icv_prefixed.pix");
    ret = icv_write(img, "PIX:icv_prefixed.pix", BU_MIME_IMAGE_AUTO);
    CHECK(ret == BRLCAD_OK, "AUTO write honors FMT:filename prefixes");
    CHECK(bu_file_exists("icv_prefixed.pix", NULL), "prefixed AUTO write created trimmed target path");
    icv_destroy(img);

    if (ret == BRLCAD_OK)
	check_pixel_file("icv_prefixed.pix", "prefixed AUTO-written PIX file reads back");

    (void)remove("icv_prefixed.pix");
}


int
main(int argc, char **argv)
{
    (void)argc;

    bu_setprogname(argv[0]);

    test_auto_write_short_filename();
    test_auto_write_percent_filename();
    test_auto_write_prefixed_filename();

    bu_log("icv_fileformat_test: %d/%d checks passed\n", tests_passed, tests_run);

    return (tests_run == tests_passed) ? BRLCAD_OK : BRLCAD_ERROR;
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
