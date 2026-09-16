/*                    P D I F F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include <cmath>
#include <cstdint>

#include "PImgHash.h"

#include "icv.h"

#include "bu/log.h"

static constexpr size_t ICV_PDIFF_PREPROCESS_SIZE = 128;
static constexpr unsigned ICV_PDIFF_DCT_COMPONENTS = 32;

static uint8_t
sample_to_u8(double sample)
{
    if (!std::isfinite(sample) || sample <= 0.0)
	return 0;
    if (sample >= 1.0)
	return 255;
    return static_cast<uint8_t>(lrint(sample * 255.0));
}

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
	    size_t offset = ((rows - 1) * cols * 3) - (i * cols * 3);
	    row.push_back(sample_to_u8(img->data[offset + j*3+0]));
	    row.push_back(sample_to_u8(img->data[offset + j*3+1]));
	    row.push_back(sample_to_u8(img->data[offset + j*3+2]));
	    //std::cout << "rgb: " << (int)row[row.size()-3] << " " << (int)row[row.size()-2] << " " << (int)row[row.size()-1] << "\n";
	}
	prep->add_row(row.data());
    }
}


static fastf_t
pdiff_dct(icv_image_t *img1, icv_image_t *img2)
{
    if (!img1 || !img2 || !img1->data || !img2->data ||
	img1->channels != 3 || img2->channels != 3 ||
	img1->width == 0 || img1->height == 0 ||
	img2->width == 0 || img2->height == 0) {
	bu_log("ERROR: pdiff_dct requires exactly 3 channels per image\n");
	return -1.0;
    }

    imghash::DCTHasher hasher(ICV_PDIFF_DCT_COMPONENTS, true);

    imghash::Preprocess prep1(ICV_PDIFF_PREPROCESS_SIZE, ICV_PDIFF_PREPROCESS_SIZE);
    load_icv(img1, &prep1);
    imghash::Image<float> pimg1 = prep1.stop();

    imghash::Preprocess prep2(ICV_PDIFF_PREPROCESS_SIZE, ICV_PDIFF_PREPROCESS_SIZE);
    load_icv(img2, &prep2);
    imghash::Image<float> pimg2 = prep2.stop();

    auto hash1 = hasher.apply(pimg1);
    auto hash2 = hasher.apply(pimg2);

    return imghash::Hasher::hamming_distance(hash1, hash2);
}


// Compute Structural Similarity Index (SSIM) - returns the mean
// the SSIM for the various channels (e.g. R, G, B)
static size_t
ssim_clamped_index(ptrdiff_t coord, size_t limit)
{
    if (coord < 0)
	return 0;
    if ((size_t)coord >= limit)
	return limit - 1;
    return (size_t)coord;
}

static double
ssim_calc(icv_image_t *img1, icv_image_t *img2)
{
    /* SSIM Constants */
    const double K1 = 0.01;
    const double K2 = 0.03;
    const double L = 1.0; /* dynamic range for doubles is 0.0 to 1.0 */
    const double C1 = (K1 * L) * (K1 * L);
    const double C2 = (K2 * L) * (K2 * L);
    const int W_SIZE = 11;
    const int OFFSET = 5;

    /* 1D Gaussian kernel for an 11-tap filter (sigma = 1.5) */
    const double g1d[11] = {0.001028, 0.007597, 0.035994, 0.109340, 0.213445, 0.265192,
	0.213445, 0.109340, 0.035994, 0.007597, 0.001028};
    double kern[121]; /* Precomputed 11x11 2D Gaussian */

    size_t width, height, channels;
    size_t y, x, c;
    int ky, kx;
    double total_ssim = 0.0;
    double *data1, *data2;
    long long window_count = 0;

    /* Validate structures */
    ICV_IMAGE_VAL_INT(img1);
    ICV_IMAGE_VAL_INT(img2);

    /* Validate compatibility */
    if (img1->width != img2->width || img1->height != img2->height || img1->channels != img2->channels) {
	bu_log("icv_ssim : Image dimensions or channels do not match.\n");
	return -1.0;
    }

    width = img1->width;
    height = img1->height;
    channels = img1->channels;
    data1 = img1->data;
    data2 = img2->data;

    /* Precompute normalized 2D Gaussian kernel */
    for (ky = 0; ky < W_SIZE; ky++) {
	for (kx = 0; kx < W_SIZE; kx++) {
	    kern[ky * W_SIZE + kx] = g1d[ky] * g1d[kx];
	}
    }

    /* Iterate over every pixel, channel by channel */
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    for (c = 0; c < channels; c++) {
		double mu1 = 0.0, mu2 = 0.0;
		double val1_sq = 0.0, val2_sq = 0.0, val12 = 0.0;
		double sigma1_sq, sigma2_sq, sigma12;
		double numerator, denominator;

		/* 11x11 sliding window */
		for (ky = 0; ky < W_SIZE; ky++) {
		    /* Leverage existing filter.c ssim_clamped_index for edge boundary protection */
		    size_t sy = ssim_clamped_index((ptrdiff_t)y + (ptrdiff_t)ky - (ptrdiff_t)OFFSET, height);

		    for (kx = 0; kx < W_SIZE; kx++) {
			size_t sx = ssim_clamped_index((ptrdiff_t)x + (ptrdiff_t)kx - (ptrdiff_t)OFFSET, width);
			size_t idx = (sy * width + sx) * channels + c;
			double weight = kern[ky * W_SIZE + kx];

			double p1 = data1[idx];
			double p2 = data2[idx];

			/* Accumulate means (E[X]) */
			mu1 += p1 * weight;
			mu2 += p2 * weight;

			/* Accumulate squares for variance (E[X^2]) */
			val1_sq += p1 * p1 * weight;
			val2_sq += p2 * p2 * weight;
			val12   += p1 * p2 * weight;
		    }
		}

		/* Calculate Variances algebraically: Var(X) = E[X^2] - E[X]^2 */
		sigma1_sq = val1_sq - (mu1 * mu1);
		sigma2_sq = val2_sq - (mu2 * mu2);
		sigma12   = val12 - (mu1 * mu2);

		/* Floating point inaccuracy guard */
		if (sigma1_sq < 0.0) sigma1_sq = 0.0;
		if (sigma2_sq < 0.0) sigma2_sq = 0.0;

		/* Compute SSIM for this window */
		numerator = (2.0 * mu1 * mu2 + C1) * (2.0 * sigma12 + C2);
		denominator = (mu1 * mu1 + mu2 * mu2 + C1) * (sigma1_sq + sigma2_sq + C2);

		total_ssim += (numerator / denominator);
		window_count++;
	    }
	}
    }

    if (window_count == 0)
	return -1.0;

    /* Return the Mean SSIM */
    return total_ssim / (double)window_count;
}

extern "C" fastf_t
icv_adiff(icv_image_t *img1, icv_image_t *img2, int m)
{
    if (!img1 || !img2 || !img1->data || !img2->data ||
	    img1->channels != img2->channels ||
	    img1->width != img2->width || img1->height != img2->height) {
	// Return a failing score if images are invalid or dimensions mismatched
	return -1.0;
    }

    if (m == ICV_DIFF_PHASH)
	return pdiff_dct(img1, img2);

    if (m == ICV_DIFF_SSIM)
	return ssim_calc(img1, img2);

    bu_log("Unknown ICV approximate diff method: %d\n", m);
    return -1;
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
