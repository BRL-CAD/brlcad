/*              A R T _ F R A M E B U F F E R _ T E S T . C P P
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
#include "common.h"

#include <cmath>
#include <exception>
#include <string>

#include "bu/app.h"
#include "bu/log.h"
#include "icv.h"

static int
check_framebuffer(const char *path, const char *image_path, size_t width,
    size_t height, size_t framebuffer_width, size_t framebuffer_height)
{
    icv_image_t *expected = icv_read(image_path, BU_MIME_IMAGE_AUTO, width, height);
    icv_image_t *actual = icv_read(path, BU_MIME_IMAGE_PIX, framebuffer_width, framebuffer_height);
    if (!expected || !actual) {
	if (expected)
	    icv_destroy(expected);
	if (actual)
	    icv_destroy(actual);
	return 1;
    }
    constexpr double MAX_DISPLAY_ERROR = 2.0 / 255.0;
    bool matches = actual->channels == 3 && expected->channels >= 3 &&
	expected->width == width && expected->height == height &&
	width <= actual->width && height <= actual->height;
    for (size_t y = 0; matches && y < height; ++y) {
	for (size_t x = 0; matches && x < width; ++x) {
	    for (size_t channel = 0; channel < actual->channels; ++channel) {
		const double value = actual->data[(y * actual->width + x) * actual->channels + channel];
		const double reference = expected->data[(y * width + x) * expected->channels + channel];
		if (!std::isfinite(value) || !std::isfinite(reference) ||
		    std::abs(value - reference) > MAX_DISPLAY_ERROR)
		    matches = false;
	    }
	}
    }
    icv_destroy(expected);
    icv_destroy(actual);
    return matches ? 0 : 1;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 7)
	return 1;
    try {
	return check_framebuffer(argv[1], argv[2], std::stoul(argv[3]), std::stoul(argv[4]),
	    std::stoul(argv[5]), std::stoul(argv[6]));
    } catch (const std::exception &error) {
	bu_log("Invalid framebuffer dimensions: %s\n", error.what());
    }
    return 1;
}
