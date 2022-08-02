/*                         F I L T E R S . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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
/** @addtogroup icv_filters
 *
 * @brief
 * Routines for image filtering.
 *
 * This is done mainly using the convolution of images. Both Gray Scale and RGB
 * images are taken care of.
 *
 */

#ifndef ICV_FILTERS_H
#define ICV_FILTERS_H

#include "common.h"
#include "icv/defines.h"

__BEGIN_DECLS

/** @{ */
/** @file icv/filters.h */

typedef enum {
    ICV_FILTER_LOW_PASS,
    ICV_FILTER_LAPLACIAN,
    ICV_FILTER_HORIZONTAL_GRAD,
    ICV_FILTER_VERTICAL_GRAD,
    ICV_FILTER_HIGH_PASS,
    ICV_FILTER_NULL,
    ICV_FILTER_BOXCAR_AVERAGE
} ICV_FILTER;

typedef enum {
    ICV_FILTER3_LOW_PASS,
    ICV_FILTER3_HIGH_PASS,
    ICV_FILTER3_BOXCAR_AVERAGE,
    ICV_FILTER3_ANIMATION_SMEAR,
    ICV_FILTER3_NULL
} ICV_FILTER3;

/**
 * Filters an image with the specified filter type. Basically
 * convolves kernel with the image.  Does zero_padding for outbound
 * pixels.
 *
 * @param img Image to be filtered.
 * @param filter_type Type of filter to be used.
 *
 */
ICV_EXPORT extern int icv_filter(icv_image_t *img, ICV_FILTER filter_type);


/**
 * Filters a set of three image with the specified filter type.  Does
 * zero_padding for outbound pixels.  Finds the resultant pixel with
 * the help of neighboring pixels in all the three images.
 *
 *
 * @return Resultant image.
 *
 */
ICV_EXPORT extern icv_image_t *icv_filter3(icv_image_t *old_img,
					   icv_image_t *curr_img,
					   icv_image_t *new_img,
					   ICV_FILTER3 filter_type);

/**
 * @brief
 * Fades an image in place.
 *
 * icv_fade will darken a pix by a certain fraction.
 *
 * @param img ICV Image to be faded.
 * @param fraction should be between 0 to 1. Amount by which the image
 * is needed to faded.
 */
ICV_EXPORT extern int icv_fade(icv_image_t *img, double fraction);

/** @} */

__END_DECLS

#endif /* ICV_FILTERS_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
