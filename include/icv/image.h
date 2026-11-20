/*                           I M A G E . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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
/** @file icv/image.h
 * Image allocation, pixel access and render metadata, independent of file types.
 */
#ifndef ICV_IMAGE_H
#define ICV_IMAGE_H

#include "common.h"
#include <stddef.h>
#include "vmath.h"
#include "icv/defines.h"

__BEGIN_DECLS

/**
 * This function allocates memory for an image and returns the
 * resultant image.
 *
 * @param width Width of the image to be created
 * @param height Height of the image to be created
 * @param color_space Color space of the image (RGB, grayscale)
 * @return Image structure with allocated space and zeroed data array, or
 * NULL if the arguments are invalid or allocation fails.
 */
ICV_EXPORT extern icv_image_t *icv_create(size_t width, size_t height, ICV_COLOR_SPACE color_space);

/** Allocate an image with an explicit channel layout. Grayscale supports
 * one channel or gray plus alpha; RGB supports three channels or RGBA.
 * Data is zero-filled. Invalid layouts, dimensions or allocation failures
 * return NULL. Free the returned image with icv_destroy. */
ICV_EXPORT extern icv_image_t *icv_create_with_channels(size_t width, size_t height,
    ICV_COLOR_SPACE color_space, size_t channels);


/**
 * This function zeroes all the data entries of an image
 * @param bif Image Structure
 */
ICV_EXPORT extern icv_image_t *icv_zero(icv_image_t *bif);

/**
 * This function frees the allocated memory for a ICV Structure and
 * data.
 */
ICV_EXPORT extern int icv_destroy(icv_image_t *bif);

/** Resolve a named paper/display size. Return 1 on success, 0 otherwise. */
ICV_EXPORT extern int icv_named_size(const char *label, size_t dpi, size_t *width, size_t *height);
/** Infer fixed-layout raw dimensions from an exact byte count. Encoded
 * file sizes must not be supplied. pixel_size is bytes per pixel. */
ICV_EXPORT extern int icv_raw_size(const char *label, size_t dpi, size_t data_size,
    size_t pixel_size, size_t *width, size_t *height);

/**
 * Write an image line to the data of ICV struct. Can handle unsigned
 * char buffers.
 *
 * Note : This function requires memory allocation for ICV_UCHAR_DATA,
 * which in turn acquires BU_SEM_SYSCALL semaphore.
 *
 * @param bif ICV struct where data is to be written
 * @param y Index of the line at which data is to be written. 0 for
 * the first line
 * @param data Line Data to be written
 * @param type Type of data, e.g., uint8 data specify ICV_DATA_UCHAR or 1
 * @return on success 0, on failure -1
 */
ICV_EXPORT int icv_writeline(icv_image_t *bif, size_t y, void *data, ICV_DATA type);

/**
 * Writes a pixel to the specified coordinates in the data of ICV
 * struct.
 *
 * @param bif ICV struct where data is to be written
 * @param x x-dir coordinate of the pixel
 * @param y y-dir coordinate of the pixel. (0,0) coordinate is taken
 * as bottom left
 * @param data Data to be written
 * @return on success 0, on failure -1
 */
ICV_EXPORT int icv_writepixel(icv_image_t *bif, size_t x, size_t y, double *data);

/**
 * Converts double data of icv_image to unsigned char data.
 * This function also does gamma correction using the gamma_corr
 * parameter of the image structure.
 *
 * Gamma correction prevents bad color aliasing.
 *
 * @param bif ICV struct where data is to be read from
 * @return array of unsigned char converted data, or NULL on failure
 */
ICV_EXPORT unsigned char *icv_data2uchar(const icv_image_t *bif);

/**
 * Converts unsigned char array to double array.
 * This function returns array of double data.
 *
 * Used to convert data from pix, bw, ppm type images for icv_image
 * struct.
 *
 * This does not free the char data.
 *
 * @param data pointer to the array to be converted.
 * @param size Size of the array.
 * @return double array.
 *
 */
ICV_EXPORT double *icv_uchar2double(unsigned char *data, size_t size);


/**
 * Options that can be passed to icv_ascii_art to modify its output.
 */
struct icv_ascii_art_params {
    int output_color;
    int invert_color;
    fastf_t brightness_multiplier;
};
#define ICV_ASCII_ART_PARAMS_DEFAULT {0, 0, 1.0}

/**
 * Converts the image to an ASCII art text string.  This function returns a
 * char array - it is the caller's responsibility to free it.
 *
 * Note that this function does not attempt to correct for the aspect ratio
 * problem (character printing in terminals often will result in the image
 * being "stretched" in height.)  If the use case calls for that, the
 * application should use icv_resize to adjust the image before calling
 * icv_ascii_art. (See the ascii libicv test code for an example.)
 *
 * @param i ICV image data.
 * @param p ASCII text generation parameters.
 * @return char array.
 *
 */
ICV_EXPORT extern char *icv_ascii_art(icv_image_t *i, struct icv_ascii_art_params *p);

/**
 * Allocate and zero-initialise a new icv_render_info struct.
 * Caller is responsible for filling in the fields and eventually
 * calling icv_render_info_destroy().
 */
ICV_EXPORT extern struct icv_render_info *icv_render_info_create(void);

/**
 * Release all memory owned by an icv_render_info, including the struct itself.
 *
 * @return 0 on success.
 */
ICV_EXPORT extern int icv_render_info_destroy(struct icv_render_info *info);

/**
 * Attach render metadata to an image.  Any existing render_info on the
 * image is first freed.  Passing NULL clears the metadata.  On success,
 * ownership of info transfers to img.
 *
 * @return 0 on success, -1 if img is NULL.
 */
ICV_EXPORT extern int icv_image_set_render_info(icv_image_t *img, struct icv_render_info *info);

/**
 * Return the render metadata attached to an image, or NULL if none.
 */
ICV_EXPORT extern struct icv_render_info *icv_image_get_render_info(const icv_image_t *img);

__END_DECLS

#endif /* ICV_IMAGE_H */
