/*                           I O . H
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
/** @addtogroup icv_io
 *
 * @brief
 * Functions provided by the LIBICV image processing library for reading
 * and writing of images.
 *
 */

#ifndef ICV_IO_H
#define ICV_IO_H

#include "common.h"
#include <stddef.h> /* for size_t */
#include "bu/mime.h"
#include "icv/defines.h"

__BEGIN_DECLS

/** @{ */
/** @file icv/io.h */

/**
 * This function allocates memory for an image and returns the
 * resultant image.
 *
 * @param width Width of the image to be created
 * @param height Height of the image to be created
 * @param color_space Color space of the image (RGB, grayscale)
 * @return Image structure with allocated space and zeroed data array
 */
ICV_EXPORT extern icv_image_t *icv_create(size_t width, size_t height, ICV_COLOR_SPACE color_space);

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

/**
 * Function to calculate (or make an educated guess) about the
 * dimensions of an image, when the image doesn't supply such
 * information.
 *
 * Standard image sizes may be hinted using the label parameter.  Many
 * standard print and display sizes (e.g., "A4" and "SVGA") are
 * recognized and used in concert with the dpi and data_size
 * parameters.
 *
 * @param[in] label      String hinting at a size  (pass NULL if not using)
 * @param[in] dpi        Dots per inch of image (pass 0 if not using)
 * @param[in] data_size  Number of uncompressed image bytes (necessary if deducing an unspecified image size)
 * @param[in] type       Image type (necessary if deducing an unspecified image size)
 *
 * @param[out] widthp    Pointer to variable that will hold image width
 * @param[out] heightp   Pointer to variable that will hold image height
 *
 * @return
 * Returns 1 if an image size was identified, zero otherwise.
 *
 */
ICV_EXPORT extern int icv_image_size(const char *label, size_t dpi, size_t data_size, bu_mime_image_t type, size_t *widthp, size_t *heightp);

/**
 * Load a file into an ICV struct. For most formats, this will be
 * called with format=ICV_IMAGE_AUTO.
 *
 * The data is packed in icv_image struct in double format with varied
 * channels as per the specification of image to be loaded.
 *
 * To read stream from stdin pass NULL pointer for filename.
 *
 * In case of bw and pix image if size is unknown pass 0 for width and
 * height. This will read the image till EOF is reached. The image
 * size of the output image will be : height = 1; width = size; where
 * size = total bytes read
 *
 * @param filename File to read
 * @param format Probable format of the file, typically
 * ICV_IMAGE_AUTO
 * @param width Width when passed as parameter from calling
 * program.
 * @param height Height when passed as parameter from calling
 * program.
 * @return A newly allocated struct holding the loaded image info.
 */
ICV_EXPORT extern icv_image_t *icv_read(const char *filename, bu_mime_image_t format, size_t width, size_t height);

/**
 * Saves Image to a file or streams to stdout in respective format
 *
 * To stream it to stdout pass NULL pointer for filename.
 *
 * @param bif Image structure of file.
 * @param filename Filename of the file to be written.
 * @param format Specific format of the file to be written.
 * @return on success 0, on failure -1 with log messages.
 */
ICV_EXPORT extern int icv_write(icv_image_t *bif, const char*filename, bu_mime_image_t format);

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


/** @} */

__END_DECLS

#endif /* ICV_IO_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
