/*                           I C V . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2013 United States Government as represented by
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
/** @file icv.h
 *
 * Functions provided by the LIBICV image conversion library.
 *
 */

#ifndef __ICV_H__
#define __ICV_H__

__BEGIN_DECLS

#ifndef ICV_EXPORT
#  if defined(ICV_DLL_EXPORTS) && defined(ICV_DLL_IMPORTS)
#    error "Only ICV_DLL_EXPORTS or ICV_DLL_IMPORTS can be defined, not both."
#  elif defined(ICV_DLL_EXPORTS)
#    define ICV_EXPORT __declspec(dllexport)
#  elif defined(ICV_DLL_IMPORTS)
#    define ICV_EXPORT __declspec(dllimport)
#  else
#    define ICV_EXPORT
#  endif
#endif

/**
 * I C V _ R O T
 *
 * Rotate an image.
 * %s [-rifb | -a angle] [-# bytes] [-s squaresize] [-w width] [-n height] [-o outputfile] inputfile [> outputfile]
 *
 */
ICV_EXPORT extern int icv_rot(int argv, char **argc);


/** @addtogroup image */
/** @ingroup data */
/** @{ */
/** @file libicv/fileformat.c
 *
 * image save/load routines
 *
 * save or load images in a variety of formats.
 *
 */

enum {
    ICV_IMAGE_AUTO,
    ICV_IMAGE_AUTO_NO_PIX,
    ICV_IMAGE_PIX,
    ICV_IMAGE_BW,
    ICV_IMAGE_ALIAS,
    ICV_IMAGE_BMP,
    ICV_IMAGE_CI,
    ICV_IMAGE_ORLE,
    ICV_IMAGE_PNG,
    ICV_IMAGE_PPM,
    ICV_IMAGE_PS,
    ICV_IMAGE_RLE,
    ICV_IMAGE_SPM,
    ICV_IMAGE_SUN,
    ICV_IMAGE_YUV,
    ICV_IMAGE_UNKNOWN
};


struct icv_image_file {
    uint32_t magic;
    char *filename;
    int fd;
    int format;			/* ICV_IMAGE_* */
    int width, height, depth;	/* pixel, pixel, byte */
    unsigned char *data;	/* this should be considered private */
    unsigned long flags;
};
typedef struct icv_image_file icv_image_file_t;
#define ICV_IMAGE_FILE_NULL ((struct icv_image_file *)0)

/**
 * asserts the integrity of a icv_image_file struct.
 */
#define ICV_CK_IMAGE_FILE(_i) ICV_CKMAG(_i, ICV_IMAGE_FILE_MAGIC, "icv_image_file")

/**
 * initializes a icv_image_file struct without allocating any memory.
 */
#define ICV_IMAGE_FILE_INIT(_i) { \
	(_i)->magic = ICV_IMAGE_FILE_MAGIC; \
	(_i)->filename = NULL; \
	(_i)->fd = (_i)->format = (_i)->width = (_i)->height = (_i)->depth = 0; \
	(_i)->data = NULL; \
	(_i)->flags = 0; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * icv_image_file struct.  does not allocate memory.
 */
#define ICV_IMAGE_FILE_INIT_ZERO { ICV_IMAGE_FILE_MAGIC, NULL, 0, 0, 0, 0, 0, NULL, 0 }

/**
 * returns truthfully whether a icv_image_file has been initialized.
 */
#define ICV_IMAGE_FILE_IS_INITIALIZED(_i) (((struct icv_image_file *)(_i) != ICV_IMAGE_FILE_NULL) && LIKELY((_i)->magic == ICV_IMAGE_FILE_MAGIC))

/**
 * Finds the Image format based on heuristics depending on the file name.
 * @param filename Filename of the image whose format is to be know.
 * @param trimmedname Buffer for storing filename after removing extensions
 * @return File Format
 */
ICV_EXPORT extern int icv_guess_file_format(const char *filename, char *trimmedname);

/**
 * This function opens the file. Allocates memory for ICV Struct and the data part of ICV Struct
 * Image File is opened/created for writing.
 * The file descriptor of the file is added to ICV struct. The size of data is governed by the
 * width, height and depth.
 * @param filename Filename of the image file to be opened.
 * @param format File format to be opened ICV_IMAGE_ . for most cases this is ICV_IMAGE_AUTO
 * @param width Width when passed as parameter by calling function
 * @param height Height when passed as parameter by calling function
 * @param depth Depth when passed as parameter by calling function
 * @return ICV Struct which contains information regarding the geometry of the file
 * filename, file-format, file descriptor of the opened file and allocated data
 * array
 */
ICV_EXPORT extern struct icv_image_file *icv_image_save_open(const char *filename,
							  int format,
							  int width,
							  int height,
							  int depth);

/**
 * Write an image line to the data of ICV struct.
 * @param bif ICV struct where data is to be written
 * @param y Index of the line at which data is to be written. 0 for the first line
 * @data Line Data to be written
 * @return on success 0, on failure -1
 */
ICV_EXPORT extern int icv_image_save_writeline(struct icv_image_file *bif,
					     int y,
					     unsigned char *data);

/**
 * Writes a pixel to the specified coordinates in the data of ICV struct.
 * @param bif ICV struct where data is to be written
 * @param x x-dir coordinate of the pixel
 * @param y y-dir coordinate of the pixel. (0,0) coordinate is taken as bottom left
 * @data Data to be written
 * @return on success 0, on failure -1
 */
ICV_EXPORT extern int icv_image_save_writepixel(struct icv_image_file *bif,
					      int x,
					      int y,
					      unsigned char *data);

/**
 * This  function writes the data from the ICV Struct to the respective files.
 * This assumes that the ICV struct contains all the necessary information
 * Geometry information, file descriptor, format and data.
 * @param bif ICV struct to be saved.
 * @return 0.
 */
ICV_EXPORT extern int icv_image_save_close(struct icv_image_file *bif);

/**
 * This function is used to save an image when ICV struct of the image is not available.
 * This creates the ICV struct for image, adds data to the ICV Struct
 * opens/creates file to be saved and finally saves the image by calling icv_image_save_close().
 * @param data Image array to be saved, a one dimensional array with 24Bit rgb pixels.
 * @param width Width of the Image to be saved
 * @param height Height of the Image to be saved
 * @param depth Depth of the Image to be saved.
 * @param filename Filename to be saved
 * @param filetype Format of the file to be saved.
 * @return 0, logs error messages.
 */
ICV_EXPORT extern int icv_image_save(unsigned char *data,
				   int width,
				   int height,
				   int depth,
				   char *filename,
				   int filetype);

/**
 * Load a file into an ICV struct. For most formats, this will be called with
 * format=ICV_IMAGE_AUTO, hint_format=0, hint_width=0, hint_height=0 and
 * hint_depth=0 for default values. At the moment, the data is packed into the
 * data field as rgb24 (raw pix style).
 *
 * For pix and bw files, having width and height set to 0 will trigger a
 * heuristic sizing algorithm based on file size, assuming that the image is
 * square at first, then looking through a set of common sizes, finally assuming
 * 512x512.
 *
 * @param filename File to load
 * @param hint_format Probable format of the file, typically ICV_IMAGE_AUTO
 * @param hint_width Width when passed as parameter from calling program. 0 for default
 * @param hint_height Height when passed as parameter from calling program. 0 for default
 * @param hint_depth Default depth field, 0 for default.
 * @return A newly allocated struct holding the loaded image info.
 */
ICV_EXPORT struct icv_image_file *icv_image_load(const char *filename, int hint_format, int hint_width, int hint_height, int hint_depth);

/** @} */
/* end image utilities */

__END_DECLS

#endif /* __ICV_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
