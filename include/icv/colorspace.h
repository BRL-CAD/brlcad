/*                   C O L O R S P A C E . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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
/** @addtogroup icv_colorspace
 *
 * @brief
 * Functions to change an image from one color space to another.
 *
 */

#ifndef ICV_COLORSPACE_H
#define ICV_COLORSPACE_H

#include "common.h"
#include "icv/defines.h"

__BEGIN_DECLS

/** @{ */
/** @file icv/colorspace.h */


typedef enum {
    ICV_COLOR_RGB,
    ICV_COLOR_R,
    ICV_COLOR_G,
    ICV_COLOR_B,
    ICV_COLOR_RG,
    ICV_COLOR_RB,
    ICV_COLOR_BG
} ICV_COLOR;


/**
 * Converts a single channel image to three channel image.  Replicates
 * the pixel as done by bw-pix utility returns a three channel image.
 * If a three channel image is passed, this function returns the same
 * image.
 */
ICV_EXPORT int icv_gray2rgb(icv_image_t *img);

/**
 * converts image to single channel image by combining three weights
 * based on NTSC primaries and 6500 white.
 */
#define icv_rgb2gray_ntsc(_a) icv_rgb2gray(_a, ICV_COLOR_RGB, 0.30, 0.59, 0.11)

/**
 * converts image to single channel image by combining three weights
 * based on CRT phosphor and D6500 white.
 */
#define icv_rgb2gray_crt(_a) icv_rgb2gray(_a, ICV_COLOR_RGB, 0.26, 0.66, 0.08)

/**
 * converts a three channel rgb image to single channel gray-image.
 * This function will combine or select planes of the image based on
 * the input arguments.
 *
 * A normal calling of this functions is as follows:
 * icv_image_rgb2gray(bif, 0, 0, 0, 0); where bif is the rgb image
 * to be converted.
 *
 * @param[in,out] img - image
 * @param[in] color Chooses color planes to be selected for combination.
 * This function will need color to be specified from
 *              ICV_COLOR_R
 *              ICV_COLOR_G
 *              ICV_COLOR_B
 *              ICV_COLOR_RG
 *              ICV_COLOR_RB
 *              ICV_COLOR_BG
 *              ICV_COLOR_RGB
 * @param[in] rweight Weight for r-plane
 * @param[in] gweight Weight for g-plane
 * @param[in] bweight Weight for b-plane
 * @return 0 on success; on failure return 1
 *
 * User can specify weights in the arguments, for the selected color
 * planes. If 0 weight is chosen this utility assigns equal weights.
 *
 */
ICV_EXPORT int icv_rgb2gray(icv_image_t *img,
			    ICV_COLOR color,
			    double rweight,
			    double gweight,
			    double bweight);

/** @} */

__END_DECLS

#endif /* ICV_COLORSPACE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
