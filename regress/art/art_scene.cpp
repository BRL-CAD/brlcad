/*              A R T _ S C E N E . C P P
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

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "icv.h"
#include "raytrace.h"
#include "wdb.h"

static int
create_scene(const char *path)
{
    struct rt_wdb *db = wdb_fopen(path);
    if (!db)
	return 1;

    point_t center = {-15, 0, 0};
    point_t box_min = {5, -10, -10};
    point_t box_max = {25, 10, 10};
    point_t bore_base = {15, -15, 0};
    vect_t bore_height = {0, 30, 0};
    const unsigned char red[] = {255, 32, 16};
    const unsigned char blue[] = {32, 64, 255};
    struct wmember members;
    bool success = mk_sph(db, "ball.s", center, 10) == 0 &&
	mk_rpp(db, "box.s", box_min, box_max) == 0 &&
	mk_rcc(db, "bore.s", bore_base, bore_height, 4) == 0;

    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ball.s", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "ball.r", &members, 1, "plastic", "", red, 0) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("box.s", &members.l, NULL, WMOP_UNION) &&
	mk_addmember("bore.s", &members.l, NULL, WMOP_SUBTRACT) &&
	mk_lcomb(db, "box.r", &members, 1, "plastic", "", NULL, 0) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("box.r", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "blue.g", &members, 0, NULL, NULL, blue, 1) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ball.r", &members.l, NULL, WMOP_UNION) &&
	mk_addmember("blue.g", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "all.g", &members, 0, NULL, NULL, NULL, 0) == 0;
    wdb_close(db);
    return success ? 0 : 1;
}

static int
check_image(const char *path, size_t width, size_t height)
{
    icv_image_t *image = icv_read(path, BU_MIME_IMAGE_AUTO, width, height);
    if (!image)
	return 1;
    if (image->width != width || image->height != height || image->channels < 3) {
	bu_log("Unexpected image dimensions or channel count: %s\n", path);
	icv_destroy(image);
	return 1;
    }

    const double COLOR_DOMINANCE = 2.0;
    const double ASPECT_TOLERANCE = 0.2;
    size_t min_x = width, min_y = height, max_x = 0, max_y = 0;
    size_t red_pixels = 0, blue_pixels = 0;
    bool finite = true;
    for (size_t y = 0; y < height; ++y) {
	for (size_t x = 0; x < width; ++x) {
	    const double *pixel = image->data + (y * width + x) * image->channels;
	    finite = finite && std::isfinite(pixel[0]) && std::isfinite(pixel[1]) && std::isfinite(pixel[2]);
	    if (pixel[0] > COLOR_DOMINANCE * pixel[1] && pixel[0] > COLOR_DOMINANCE * pixel[2]) {
		++red_pixels;
		min_x = std::min(min_x, x);
		min_y = std::min(min_y, y);
		max_x = std::max(max_x, x);
		max_y = std::max(max_y, y);
	    }
	    if (pixel[2] > COLOR_DOMINANCE * pixel[0] && pixel[2] > COLOR_DOMINANCE * pixel[1])
		++blue_pixels;
	}
    }
    icv_destroy(image);
    if (!finite || !red_pixels || !blue_pixels) {
	bu_log("Missing finite red and inherited blue geometry: %s\n", path);
	return 1;
    }

    // A sphere must stay round when the frame aspect or projection changes.
    const double span_x = static_cast<double>(max_x - min_x + 1);
    const double span_y = static_cast<double>(max_y - min_y + 1);
    if (std::abs(span_x - span_y) > ASPECT_TOLERANCE * std::max(span_x, span_y)) {
	bu_log("Distorted sphere in %s: %g x %g pixels\n", path, span_x, span_y);
	return 1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "create"))
	return create_scene(argv[2]);
    if (argc == 5 && BU_STR_EQUAL(argv[1], "check")) {
	try {
	    return check_image(argv[2], std::stoul(argv[3]), std::stoul(argv[4]));
	} catch (const std::exception &error) {
	    bu_log("Invalid image dimensions: %s\n", error.what());
	}
    }
    return 1;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
