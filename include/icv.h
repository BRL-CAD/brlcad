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
 * Functions provided by the LIBICV image processing library.
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

typedef enum {
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
}ICV_IMAGE_FORMAT;

typedef enum {
    ICV_COLOR_SPACE_RGB,
    ICV_COLOR_SPACE_GRAY
    /* Add here for format addition like CMYKA, HSV, others  */
}ICV_COLOR_SPACE;

typedef enum {
    ICV_DATA_DOUBLE,
    ICV_DATA_UCHAR
}ICV_DATA;

struct icv_image {
    uint32_t magic;
    ICV_COLOR_SPACE color_space;
    double *data;
    float gamma_corr;
    int width, height, channels, alpha_channel;
    unsigned long flags;
};

typedef struct icv_image icv_image_t;
#define ICV_IMAGE_NULL ((struct icv_image *)0)

/**
 * asserts the integrity of a icv_image_file struct.
 */
#define ICV_CK_IMAGE(_i) ICV_CKMAG(_i, ICV_IMAGE_FILE_MAGIC, "icv_image")

/**
 * initializes a icv_image_file struct without allocating any memory.
 */
#define ICV_IMAGE_INIT(_i) { \
	    (_i)->magic = ICV_IMAGE_FILE_MAGIC; \
	    (_i)->width = (_i)->height = (_i)->channels = (_i)->alpha_channel = 0; \
	    (_i)->gamma_corr = 0.0; \
	    (_i)->data = NULL; \
	    (_i)->flags = 0; \
    }

/**
 * returns truthfully whether a icv_image_file has been initialized.
 */
#define ICV_IMAGE_IS_INITIALIZED(_i) (((struct icv_image *)(_i) != ICV_IMAGE_NULL) && LIKELY((_i)->magic == ICV_IMAGE_FILE_MAGIC))

/**
 * Finds the Image format based on heuristics depending on the file name.
 * @param filename Filename of the image whose format is to be know.
 * @param trimmedname Buffer for storing filename after removing extensions
 * @return File Format
 */
ICV_EXPORT extern int icv_guess_file_format(const char *filename, char *trimmedname);

ICV_EXPORT extern icv_image_t* icv_image_create(int width, int height, ICV_COLOR_SPACE color_space);

ICV_EXPORT int icv_image_writeline(icv_image_t *bif, int y, void *data, ICV_DATA type);

ICV_EXPORT int icv_image_writepixel(icv_image_t *bif, int x, int y, double *data);

ICV_EXPORT extern int icv_image_save(icv_image_t* bif, const char*filename, ICV_IMAGE_FORMAT format);

ICV_EXPORT extern icv_image_t* icv_image_load(const char *filename, int format, int width, int height);

ICV_EXPORT extern icv_image_t* icv_image_zero(icv_image_t* bif);

ICV_EXPORT extern void icv_image_free(icv_image_t* bif);

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
