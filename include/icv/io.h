/*                           I O . H
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
#include "bu/mime.h"
#include "icv/codec.h"

__BEGIN_DECLS

/** @{ */
/** @file icv/io.h */

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
 * DEPRECATED (7.48): The generated-enum size interface.
 * Use icv_image_size_with_context, icv_raw_size, or icv_named_size.
 */
ICV_EXPORT extern int icv_image_size(const char *label, size_t dpi, size_t data_size, bu_mime_image_t type, size_t *widthp, size_t *heightp);

/**
 * Load a file into an ICV struct. For most formats, this will be
 * called with format=BU_MIME_IMAGE_AUTO.
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
 * BU_MIME_IMAGE_AUTO
 * @param width Width when passed as parameter from calling
 * program.
 * @param height Height when passed as parameter from calling
 * program.
 * @return A newly allocated struct holding the loaded image info.
 *
 * DEPRECATED (7.48): Use icv_read_with_context and a
 * canonical MIME name (NULL for automatic recognition).
 */
ICV_EXPORT extern icv_image_t *icv_read(const char *filename, bu_mime_image_t format, size_t width, size_t height);

/**
 * Load an image from a memory buffer.
 *
 * @param buffer Pointer to the memory buffer.
 * @param size Size of the memory buffer in bytes.
 * @param format Probable format of the image.
 * @param width Width when passed as parameter from calling program.
 * @param height Height when passed as parameter from calling program.
 * @return A newly allocated struct holding the loaded image info, or NULL on failure.
 *
 * DEPRECATED (7.48): Use icv_read_mem_with_context.
 */
ICV_EXPORT extern icv_image_t *icv_read_mem(const unsigned char *buffer, size_t size, bu_mime_image_t format, size_t width, size_t height);

/**
 * Saves Image to a file or streams to stdout in respective format
 *
 * To stream it to stdout pass NULL pointer for filename.
 * Writers do not modify the input image. If a format requires a different
 * channel layout, conversion is performed on a temporary image; alpha is
 * dropped only for formats that cannot represent it.
 *
 * @param bif Image structure of file.
 * @param filename Filename of the file to be written.
 * @param format Specific format of the file to be written.
 * @return on success 0, on failure -1 with log messages.
 *
 * DEPRECATED (7.48): Use icv_write_with_context.
 */
ICV_EXPORT extern int icv_write(icv_image_t *bif, const char*filename, bu_mime_image_t format);

/**
 * Saves Image to a dynamically allocated memory buffer in the respective format.
 * The input image is not modified. If a format requires a different channel
 * layout, conversion is performed on a temporary image; alpha is dropped only
 * for formats that cannot represent it.
 *
 * @param bif Image structure.
 * @param buffer Pointer to an unsigned char pointer to hold the allocated memory.
 * @param size Pointer to a size_t to hold the size of the written buffer.
 * @param format Specific format of the file to be written.
 * @return on success 0, on failure -1 with log messages.
 *
 * Note: The caller is responsible for freeing *buffer using bu_free().
 *
 * DEPRECATED (7.48): Use icv_write_mem_with_context.
 */
ICV_EXPORT extern int icv_write_mem(icv_image_t *bif, unsigned char **buffer, size_t *size, bu_mime_image_t format);

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
