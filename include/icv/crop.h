/*                           C R O P . H
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
/** @addtogroup icv_crop
 *
 * @brief
 * Functions for cropping images.
 *
 * There are currently two types of cropping: rectangular and skewed.
 *
 */

#ifndef ICV_CROP_H
#define ICV_CROP_H

#include "common.h"
#include <stddef.h> /* for size_t */
#include "icv/defines.h"

__BEGIN_DECLS

/** @{ */
/** @file icv/crop.h */

/**
 * This function crops an input image.
 * Note : (0,0) corresponds to the Bottom Left of an Image.
 *
 * @param img Input image struct to be cropped.
 * @param xorig X-Coordinate of offset of image to be extracted from.
 * @param yorig Y-Coordinate of offset of image to be extracted from.
 * @param xnum Length of the output image to be extracted from input
 * data in horizontal direction.
 * @param ynum Length of the output image to be extracted from input
 * data in vertical direction.
 * @return 0 on success.
 */
ICV_EXPORT extern int icv_rect(icv_image_t *img, size_t xorig, size_t yorig, size_t xnum, size_t ynum);

/**
 * This function crops an input image.
 *
 * This can do a skewed cropping, i.e. given any four points of
 * quadrilateral in an image, map it to a rectangle of xnumXynum
 * dimension.
 *
 * @verbatim
 *        (ulx,uly)         (urx,ury)
 *             __________________
 *            /                 |
 *           /                  |
 *          /                   |
 *         /                    |
 *        /                     |
 *       /______________________|
 *     (llx,lly)             (lrx,lry)
 * @endverbatim
 *
 * @return 0 on success; on failure -1; and logs the error message.
 */
ICV_EXPORT extern int icv_crop(icv_image_t *img,
			       size_t ulx, size_t uly,
			       size_t urx, size_t ury,
			       size_t lrx, size_t lry,
			       size_t llx, size_t lly,
			       size_t ynum,
			       size_t xnum);
/** @} */

__END_DECLS

#endif /* ICV_CROP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
