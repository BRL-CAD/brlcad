/*                    P D I F F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2023 United States Government as represented by
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
/** @file libicv/pdiff.cpp
 *
 * This file implements perceptual image hashing based difference
 * checking - as opposed to icv_diff, icv_pdiff does an approximate
 * comparison of two images to determine how similar they are.
 */

#include "common.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <math.h>

#include "PImgHash.h"

#include "icv.h"

#include "bio.h"
#include "bu/log.h"
#include "bu/magic.h"
#include "bu/malloc.h"
#include "vmath.h"

#if 0
std::string format_hash(const std::vector<uint8_t>& hash)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : hash) oss << std::setw(2) << int(b);
    return oss.str();
}
#endif

void
load_icv(icv_image_t *img, imghash::Preprocess *prep)
{
    size_t rows, cols;
    prep->start(img->height, img->width, 3);
    rows = img->height;
    cols = img->width;
    for (size_t i = 0; i < rows; i++) {
	std::vector<uint8_t> row;
	for (size_t j = 0; j < cols ; j++) {
	    long l;
	    int offset = ((rows - 1) * cols * 3) - (i * cols * 3);
	    l = lrint(img->data[offset + j*3+0]*255.0);
	    row.push_back((uint8_t)l);
	    l = lrint(img->data[offset + j*3+1]*255.0);
	    row.push_back((uint8_t)l);
	    l = lrint(img->data[offset + j*3+2]*255.0);
	    row.push_back((uint8_t)l);
	    //std::cout << "rgb: " << (int)row[row.size()-3] << " " << (int)row[row.size()-2] << " " << (int)row[row.size()-1] << "\n";
	}
	prep->add_row(row.data());
    }
}


extern "C" uint32_t
icv_pdiff(icv_image_t *img1, icv_image_t *img2)
{
    if (!img1 || !img2)
	return -1;

    std::unique_ptr<imghash::Hasher> hasher;
    int dct_size = 4; // 1024 bits
    hasher = std::make_unique<imghash::DCTHasher>(8 * dct_size, true);

    int d1 = (img1->width < img1->height) ? img1->width : img1->height;
    imghash::Preprocess prep1(d1, d1);
    load_icv(img1, &prep1);
    imghash::Image<float> pimg1 = prep1.stop();

    int d2 = (img2->width < img2->height) ? img2->width : img2->height;
    imghash::Preprocess prep2(d2, d2);
    load_icv(img2, &prep2);
    imghash::Image<float> pimg2 = prep2.stop();

    auto hash1 = hasher->apply(pimg1);
    auto hash2 = hasher->apply(pimg2);

    //std::cout << "hash1:" << format_hash(hash1) << "\n";
    //std::cout << "hash2:" << format_hash(hash2) << "\n";

    return hasher->hamming_distance(hash1, hash2);
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
