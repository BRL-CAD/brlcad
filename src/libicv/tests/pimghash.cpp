/*                     P I M G H A S H . C P P
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
/** @file pimghash.cpp
 *
 * Behavioral checks for the small PImgHash subset used by libicv.
 */

#include "common.h"

#include "../PImgHash.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

#include "bu/app.h"

namespace {

#define CHECK(_expr) do { \
	if (!(_expr)) { \
	    std::fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #_expr); \
	    return false; \
	} \
    } while (0)

bool
nearly_equal(float a, float b)
{
    return std::fabs(a - b) < 1.0e-6f;
}

class ManualHasher : public imghash::Hasher
{
public:
    hash_type apply(const imghash::Image<float>&) override
    {
	clear();
	append_bit(true);
	append_bit(false);
	append_bit(true);
	return bytes;
    }

    void reset()
    {
	clear();
    }
};

class ProbeDCTHasher : public imghash::DCTHasher
{
public:
    ProbeDCTHasher(unsigned M, bool even) : imghash::DCTHasher(M, even) {}

    static std::vector<float> matrix(unsigned N, unsigned M, bool even)
    {
	return mat(N, M, even);
    }
};

bool
test_image_ownership()
{
    imghash::Image<float> img(2, 3, 2);
    for (size_t i = 0; i < img.size; ++i)
	img[i] = static_cast<float>(i);

    CHECK(img.index(1, 2, 1) == 11);
    CHECK(nearly_equal(img(1, 2, 1), 11.0f));

    imghash::Image<float> copy(img);
    img[0] = 99.0f;
    CHECK(nearly_equal(copy[0], 0.0f));
    CHECK(copy.height == 2 && copy.width == 3 && copy.channels == 2);

    imghash::Image<float> assigned(1, 1, 1);
    assigned = img;
    assigned[1] = 77.0f;
    CHECK(nearly_equal(img[1], 1.0f));
    CHECK(assigned.height == img.height && assigned.row_size == img.row_size);

    imghash::Image<float> moved(std::move(assigned));
    CHECK(moved.size == img.size);
    CHECK(assigned.data == nullptr);

    imghash::Image<float> move_assigned(1, 1, 1);
    move_assigned = std::move(moved);
    CHECK(move_assigned.size == img.size);
    CHECK(moved.data == nullptr);

    return true;
}

bool
test_resize()
{
    imghash::Image<uint8_t> in(2, 2, 3);
    for (size_t i = 0; i < in.size; ++i)
	in[i] = static_cast<uint8_t>(i * 20);

    std::vector<size_t> hist;
    imghash::Image<float> same(2, 2, 3);
    imghash::resize<uint8_t, float, float>(in, same, hist);
    CHECK(hist.size() == 3 * 256);
    CHECK(nearly_equal(same[0], 0.0f));
    CHECK(nearly_equal(same[11], static_cast<float>(220) / 255.0f));

    imghash::Image<float> up(4, 4, 3);
    imghash::resize<uint8_t, float, float>(in, up, hist);
    CHECK(up.height == 4 && up.width == 4);
    CHECK(nearly_equal(up[0], 0.0f));
    CHECK(nearly_equal(up[up.size - 1], static_cast<float>(220) / 255.0f));

    imghash::Image<float> down(1, 1, 3);
    imghash::resize<uint8_t, float, float>(in, down, hist);
    CHECK(down.height == 1 && down.width == 1);
    CHECK(down[0] > 0.0f && down[0] < 1.0f);

    bool threw = false;
    imghash::Image<float> bad_channels(2, 2, 1);
    try {
	imghash::resize<uint8_t, float, float>(in, bad_channels, hist);
    } catch (const std::runtime_error&) {
	threw = true;
    }
    CHECK(threw);

    return true;
}

bool
test_preprocess()
{
    uint8_t row0[] = {0, 10, 20, 30, 40, 50};
    uint8_t row1[] = {60, 70, 80, 90, 100, 110};

    imghash::Preprocess up(4, 4);
    up.start(2, 2, 3);
    CHECK(up.add_row(row0));
    CHECK(!up.add_row(row1));
    imghash::Image<float> up_img = up.stop();
    CHECK(up_img.height == 4 && up_img.width == 4 && up_img.channels == 1);
    CHECK(up_img[0] >= 0.0f && up_img[0] <= 3.0f);

    uint8_t rows[4][6] = {
	{0, 10, 20, 30, 40, 50},
	{60, 70, 80, 90, 100, 110},
	{120, 130, 140, 150, 160, 170},
	{180, 190, 200, 210, 220, 230}
    };
    imghash::Preprocess down(2, 2);
    down.start(4, 2, 3);
    CHECK(down.add_row(rows[0]));
    CHECK(down.add_row(rows[1]));
    CHECK(down.add_row(rows[2]));
    CHECK(!down.add_row(rows[3]));
    imghash::Image<float> down_img = down.stop();
    CHECK(down_img.height == 2 && down_img.width == 2 && down_img.channels == 1);
    CHECK(down_img[0] >= 0.0f && down_img[0] <= 3.0f);

    return true;
}

bool
test_hashing()
{
    imghash::Image<float> img(16, 16, 1);
    for (size_t y = 0; y < img.height; ++y) {
	for (size_t x = 0; x < img.width; ++x) {
	    img(y, x, 0) = (x < y) ? 0.75f : 0.25f;
	}
    }

    ProbeDCTHasher even_hasher(32, true);
    imghash::Hasher::hash_type even_hash = even_hasher.apply(img);
    CHECK(even_hash.size() == 8);

    ProbeDCTHasher full_hasher(32, false);
    imghash::Hasher::hash_type full_hash = full_hasher.apply(img);
    CHECK(full_hash.size() == 32);

    ProbeDCTHasher short_hasher(1, true);
    imghash::Hasher::hash_type short_hash = short_hasher.apply(img);
    CHECK(short_hash.empty());

    imghash::Image<float> nonsquare(16, 8, 1);
    bool threw = false;
    try {
	(void)full_hasher.apply(nonsquare);
    } catch (const std::runtime_error&) {
	threw = true;
    }
    CHECK(threw);

    std::vector<float> even_matrix = ProbeDCTHasher::matrix(16, 32, true);
    std::vector<float> full_matrix = ProbeDCTHasher::matrix(16, 32, false);
    CHECK(even_matrix.size() == 16 * 8);
    CHECK(full_matrix.size() == 16 * 16);

    ManualHasher manual;
    imghash::Hasher::hash_type manual_hash = manual.apply(img);
    CHECK(manual_hash.size() == 1);
    CHECK(manual_hash[0] == 5);
    manual.reset();
    CHECK(manual.apply(img)[0] == 5);

    imghash::Hasher::hash_type h1 = {0xff};
    imghash::Hasher::hash_type h2 = {0x0f, 0xff};
    CHECK(imghash::Hasher::hamming_distance(h1, h2) == 4);

    return true;
}

} // namespace

int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    if (!test_image_ownership()) return EXIT_FAILURE;
    if (!test_resize()) return EXIT_FAILURE;
    if (!test_preprocess()) return EXIT_FAILURE;
    if (!test_hashing()) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
