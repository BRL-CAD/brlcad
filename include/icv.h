/*                           I C V . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
    ICV_IMAGE_YUV
};


struct icv_image_file {
    uint32_t magic;
    char *filename;
    int fd;
    int format;			/* ICV_IMAGE_* */
    int width, height, depth;	/* pixel, pixel, byte */
    unsigned char *data;
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
 * icv_image_file struct.  does not allocate mmeory.
 */
#define ICV_IMAGE_FILE_INIT_ZERO { ICV_IMAGE_FILE_MAGIC, NULL, 0, 0, 0, 0, 0, NULL, 0 }

/**
 * returns truthfully whether a icv_image_file has been initialized.
 */
#define ICV_IMAGE_FILE_IS_INITIALIZED(_i) (((struct icv_image_file *)(_i) != ICV_IMAGE_FILE_NULL) && LIKELY((_i)->magic == ICV_IMAGE_FILE_MAGIC))


ICV_EXPORT extern struct icv_image_file *icv_image_save_open(const char *filename,
							  int format,
							  int width,
							  int height,
							  int depth);

ICV_EXPORT extern int icv_image_save_writeline(struct icv_image_file *bif,
					     int y,
					     unsigned char *data);

ICV_EXPORT extern int icv_image_save_writepixel(struct icv_image_file *bif,
					      int x,
					      int y,
					      unsigned char *data);

ICV_EXPORT extern int icv_image_save_close(struct icv_image_file *bif);

ICV_EXPORT extern int icv_image_save(unsigned char *data,
				   int width,
				   int height,
				   int depth,
				   char *filename,
				   int filetype);

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
