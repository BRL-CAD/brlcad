/*                    A P N G M I N I . C P P
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
/** @file apngmini.cpp
 *
 * Direct behavioral checks for the APNG helper used by libicv animation I/O.
 */

#define APNGMINI_IMPLEMENTATION
#include "../apngmini.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "bu/app.h"
#include "bu/file.h"

namespace {

#define CHECK(_expr) do { \
	if (!(_expr)) { \
	    std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #_expr); \
	    return false; \
	} \
    } while (0)

apngmini::Frame
make_frame(uint32_t width, uint32_t height, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    apngmini::Frame f;
    f.width = width;
    f.height = height;
    f.delay.numerator = 1;
    f.delay.denominator = 10;
    f.pixels.resize(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
	f.pixels[i*4 + 0] = r;
	f.pixels[i*4 + 1] = g;
	f.pixels[i*4 + 2] = b;
	f.pixels[i*4 + 3] = a;
    }
    return f;
}

bool
test_compose_modes()
{
    apngmini::Animation anim;
    anim.canvas_width = 2;
    anim.canvas_height = 2;

    apngmini::Frame red = make_frame(2, 2, 200, 0, 0, 255);
    red.dispose_op = apngmini::DisposeOp::Previous;
    red.blend_op = apngmini::BlendOp::Over;
    anim.frames.push_back(red);

    apngmini::Frame blue = make_frame(1, 1, 0, 0, 200, 128);
    blue.x_offset = 1;
    blue.y_offset = 1;
    blue.dispose_op = apngmini::DisposeOp::None;
    blue.blend_op = apngmini::BlendOp::Over;
    anim.frames.push_back(blue);

    apngmini::vector<apngmini::vector<uint8_t>> composed = anim.compose();
    CHECK(composed.size() == 2);
    CHECK(composed[0][0] == 200);
    CHECK(composed[0][3] == 255);

    size_t p = (1 * anim.canvas_width + 1) * 4;
    CHECK(composed[1][p + 0] == 0);
    CHECK(composed[1][p + 2] == 200);
    CHECK(composed[1][p + 3] == 128);

    apngmini::Animation empty;
    CHECK(empty.compose().empty());

    return true;
}

bool
test_read_write_helpers()
{
    apngmini::Animation invalid;
    CHECK(apngmini::write(invalid).empty());
    CHECK(!apngmini::write_file("invalid_empty_apngmini.png", invalid));

    apngmini::Animation anim;
    anim.canvas_width = 2;
    anim.canvas_height = 2;
    anim.num_plays = 3;
    anim.frames.push_back(make_frame(2, 2, 10, 20, 30, 255));

    apngmini::Frame patch = make_frame(1, 1, 240, 10, 20, 255);
    patch.x_offset = 1;
    patch.y_offset = 0;
    patch.delay.numerator = 2;
    patch.delay.denominator = 25;
    anim.frames.push_back(patch);

    apngmini::vector<uint8_t> encoded = apngmini::write(anim, 1);
    CHECK(!encoded.empty());

    apngmini::Animation read_from_ptr = apngmini::read(encoded.data(), encoded.size());
    CHECK(read_from_ptr.canvas_width == 2);
    CHECK(read_from_ptr.canvas_height == 2);
    CHECK(read_from_ptr.frames.size() == 2);

    apngmini::Animation read_from_vec = apngmini::read(encoded);
    CHECK(read_from_vec.frames.size() == 2);

    apngmini::Reader reader(encoded.data(), encoded.size());
    CHECK(!reader.error());
    CHECK(reader.has_next());
    apngmini::Frame first = reader.next_frame();
    CHECK(first.width == 2 && first.height == 2);
    CHECK(reader.has_next());
    apngmini::Frame second = reader.next_frame();
    CHECK(second.width == 1 && second.height == 1);
    CHECK(!reader.has_next());

    std::string path = "test_apngmini_roundtrip.png";
    CHECK(apngmini::write_file(path, anim, 1));
    apngmini::Animation read_from_file = apngmini::read_file(path);
    CHECK(read_from_file.frames.size() == 2);
    bu_file_delete(path.c_str());

    bool missing_threw = false;
    try {
	(void)apngmini::read_file("does_not_exist_apngmini.png");
    } catch (const apngmini::Error&) {
	missing_threw = true;
    }
    CHECK(missing_threw);

    return true;
}

} // namespace

int
main(int ac, const char **av)
{
    if (ac > 0)
	bu_setprogname(av[0]);
    if (!test_compose_modes()) return EXIT_FAILURE;
    if (!test_read_write_helpers()) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
