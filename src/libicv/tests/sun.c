/*                     I C V _ S U N _ T E S T . C
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
/** @file sun.c
 *
 * Tests for LIBICV Sun rasterfile output.
 */

#include "common.h"

#include <stdint.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "icv.h"

#define SUN_HEADER_SIZE 32
#define SUN_MAP_SIZE 768
#define SUN_DATA_OFFSET (SUN_HEADER_SIZE + SUN_MAP_SIZE)

static uint32_t
read_be32(const unsigned char *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
	((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

int
main(int argc, char **argv)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    icv_image_t *image;
    int failed = 0;

    (void)argc;
    bu_setprogname(argv[0]);
    image = icv_create(3, 2, ICV_COLOR_SPACE_RGB);
    if (!image)
	return BRLCAD_ERROR;

    image->data[9] = 1.0;
    image->data[13] = 1.0;
    image->data[17] = 1.0;
    if (icv_write_mem(image, &buffer, &size, BU_MIME_IMAGE_SUN) != BRLCAD_OK) {
	icv_destroy(image);
	return BRLCAD_ERROR;
    }

#define CHECK(_condition, _message) do { \
    if (!(_condition)) { \
	bu_log("FAIL: %s\n", _message); \
	failed = 1; \
    } \
} while (0)

    CHECK(size == 808, "odd-width rows are padded to 16 bits");
    CHECK(read_be32(buffer) == 0x59a66a95, "header has the Sun raster magic");
    CHECK(read_be32(buffer + 4) == 3, "header width is big-endian");
    CHECK(read_be32(buffer + 8) == 2, "header height is big-endian");
    CHECK(read_be32(buffer + 12) == 8, "header depth is 8 bits");
    CHECK(read_be32(buffer + 16) == 8, "header includes row padding in image length");
    CHECK(read_be32(buffer + 28) == SUN_MAP_SIZE, "header records the colormap size");
    CHECK(buffer[SUN_DATA_OFFSET] == 5, "top-row red maps to the red palette");
    CHECK(buffer[SUN_DATA_OFFSET + 1] == 30, "top-row green maps to the green palette");
    CHECK(buffer[SUN_DATA_OFFSET + 2] == 180, "top-row blue maps to the blue palette");
    CHECK(buffer[SUN_DATA_OFFSET + 3] == 0, "odd-width padding is zero");
    CHECK(buffer[SUN_DATA_OFFSET + 4] == 0, "bottom row follows the top row");

    bu_free(buffer, "Sun rasterfile test buffer");
    icv_destroy(image);
    return failed ? BRLCAD_ERROR : BRLCAD_OK;
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
