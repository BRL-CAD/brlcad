/*                           I C V . H
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
/** @addtogroup icv_ops
 *
 * Various routines to perform operations on images.
 *
 */

#ifndef ICV_OPS_H
#define ICV_OPS_H

#include "common.h"
#include <stddef.h> /* for size_t */
#include "icv/defines.h"

__BEGIN_DECLS

/** @{ */
/** @file icv/ops.h */

/**
 * This function sanitizes the image.
 *
 * It forces the image pixels to be in the prescribed range.
 *
 * All the pixels higher than the max range are set to MAX (1.0).
 * All the pixels lower than the min range are set to MIN (0.0).
 *
 * Note if an image(bif) is sanitized then,
 * (bif->flags&&ICV_SANITIZED)  is true.
 *
 */
ICV_EXPORT int icv_sanitize(icv_image_t* img);

/**
 * This adds a constant value to all the pixels of the image.  Also if
 * the flag ICV_OPERATIONS_MODE is set this doesn't sanitize the
 * image.
 *
 * Note to set the flag for a bif (icv_image struct);
 * bif->flags |= ICV_OPERATIONS_MODE;
 *
 */
ICV_EXPORT int icv_add_val(icv_image_t* img, double val);

/**
 * This multiplies all the pixels of the image with a constant Value.
 * Also if the flag ICV_OPERATIONS_MODE is set this doesn't sanitize
 * the image.
 */
ICV_EXPORT int icv_multiply_val(icv_image_t* img, double val);

/**
 * This divides all the pixels of the image with a constant Value.
 * Also if the flag ICV_OPERATIONS_MODE is set this doesn't sanitize
 * the image.
 */
ICV_EXPORT int icv_divide_val(icv_image_t* img, double val);

/**
 * This raises all the pixels of the image to a constant exponential
 * power.  Also if the flag ICV_OPERATIONS_MODE is set this doesn't
 * sanitize the image.
 */
ICV_EXPORT int icv_pow_val(icv_image_t* img, double val);

/**
 * This routine adds pixel value of one image to pixel value of other
 * pixel and inserts in the same index of the output image.
 *
 * Also it sanitizes the image.
 */
ICV_EXPORT icv_image_t *icv_add(icv_image_t *img1, icv_image_t *img2);

/**
 * This routine subtracts pixel value of one image from pixel value of
 * other pixel and inserts the result at the same index of the output
 * image.
 *
 * Also it sanitizes the image.
 *
 * @param img1 First Image.
 * @param img2 Second Image.
 * @return New icv_image (img1 - img2)
 *
 */
ICV_EXPORT icv_image_t *icv_sub(icv_image_t *img1, icv_image_t *img2);

/**
 * This routine multiplies pixel value of one image to pixel value of
 * other pixel and inserts the result at the same index of the output
 * image.
 *
 * Also it sanitizes the image.
 *
 * @param img1 First Image.
 * @param img2 Second Image.
 * @return New icv_image (img1 * img2)
 *
 */
ICV_EXPORT icv_image_t *icv_multiply(icv_image_t *img1, icv_image_t *img2);

/**
 * This routine divides pixel value of one image from pixel value of
 * other pixel and inserts the result at the same index of the output
 * image.
 *
 * Also it sanitizes the image.
 *
 * @param img1 First Image.
 * @param img2 Second Image.
 * @return New icv_image (img1 / img2)
 *
 */
ICV_EXPORT icv_image_t *icv_divide(icv_image_t *img1, icv_image_t *img2);

/**
 * Change the saturation of image pixels.  If sat is set to 0.0 the
 * result will be monochromatic; if sat is made 1.0, the color will
 * not change; if sat is made greater than 1.0, the amount of color is
 * increased.
 *
 * @param img RGB Image to be saturated.
 * @param sat Saturation value.
 */
ICV_EXPORT int icv_saturate(icv_image_t* img, double sat);

typedef enum {
    ICV_RESIZE_UNDERSAMPLE,
    ICV_RESIZE_SHRINK,
    ICV_RESIZE_NINTERP,
    ICV_RESIZE_BINTERP
} ICV_RESIZE_METHOD;

/**
 * This function resizes the given input image.
 * Mode of usage:
 * a) ICV_RESIZE_UNDERSAMPLE : This method undersamples the said image
 * e.g. icv_resize(bif, ICV_RESIZE_UNDERSAMPLE, 0, 0, 2);
 *  undersamples the image with a factor of 2.
 *
 * b) ICV_RESIZE_SHRINK : This Shrinks the image, keeping the light
 * energy per square area as constant.
 * e.g. icv_resize(bif, ICV_RESIZE_SHRINK,0,0,2);
 *  shrinks the image with a factor of 2.
 *
 * c) ICV_RESIZE_NINTERP : This interpolates using nearest neighbor
 * method.
 * e.g. icv_resize(bif, ICV_RESIZE_NINTERP,1024,1024,0);
 *  interpolates the output image to have the size of 1024X1024.
 *
 * d) ICV_RESIZE_BINTERP : This interpolates using bilinear
 * Interpolation Method.
 * e.g. icv_resize(bif, ICV_RESIZE_BINTERP,1024,1024,0);
 *  interpolates the output image to have the size of 1024X1024.
 *
 * resizes the image inplace.
 *
 * @param bif Image (packed in icv_image struct)
 * @param method One of the modes.
 * @param out_width Out Width.
 * @param out_height Out Height.
 * @param factor Integer type data representing the factor to be
 * shrunken
 * @return 0 on success and -1 on failure.
 */
ICV_EXPORT int icv_resize(icv_image_t *bif, ICV_RESIZE_METHOD method, size_t out_width, size_t out_height, size_t factor);

/**
 * Rotate an image.
 * %s [-rifb | -a angle] [-# bytes] [-s squaresize] [-w width] [-n height] [-o outputfile] inputfile [> outputfile]
 *
 */
ICV_EXPORT extern int icv_rot(size_t argc, const char *argv[]);

/** @} */

__END_DECLS

#endif /* ICV_OPS_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
