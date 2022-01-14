/*                     D E F I N E S . H
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
/** @addtogroup icv_defines
 *
 * @brief
 * Definitions used in the LIBICV image processing library.
 *
 */

#ifndef ICV_DEFINES_H
#define ICV_DEFINES_H

#include "common.h"
#include <stddef.h> /* for size_t */

/** @{ */
/** @file icv/defines.h */

__BEGIN_DECLS

#ifndef ICV_EXPORT
#  if defined(ICV_DLL_EXPORTS) && defined(ICV_DLL_IMPORTS)
#    error "Only ICV_DLL_EXPORTS or ICV_DLL_IMPORTS can be defined, not both."
#  elif defined(ICV_DLL_EXPORTS)
#    define ICV_EXPORT COMPILER_DLLEXPORT
#  elif defined(ICV_DLL_IMPORTS)
#    define ICV_EXPORT COMPILER_DLLIMPORT
#  else
#    define ICV_EXPORT
#  endif
#endif

typedef enum {
    ICV_COLOR_SPACE_RGB,
    ICV_COLOR_SPACE_GRAY
    /* Add here for format addition like CMYKA, HSV, others  */
} ICV_COLOR_SPACE;

typedef enum {
    ICV_DATA_DOUBLE,
    ICV_DATA_UCHAR
} ICV_DATA;

/* Define Various Flags */
#define ICV_NULL_IMAGE 0X0001
#define ICV_SANITIZED 0X0002
#define ICV_OPERATIONS_MODE 0x0004
#define ICV_UNDEFINED_1 0x0008

struct icv_image {
    uint32_t magic;
    ICV_COLOR_SPACE color_space;
    double *data;
    float gamma_corr;
    size_t width, height, channels, alpha_channel;
    uint16_t flags;
};


typedef struct icv_image icv_image_t;
#define ICV_IMAGE_NULL ((struct icv_image *)0)

/**
 * asserts the integrity of a icv_image_file struct.
 */
#define ICV_CK_IMAGE(_i) ICV_CKMAG(_i, ICV_IMAGE_MAGIC, "icv_image")

/**
 * initializes a icv_image_file struct without allocating any memory.
 */
#define ICV_IMAGE_INIT(_i) { \
	(_i)->magic = ICV_IMAGE_MAGIC; \
	(_i)->width = (_i)->height = (_i)->channels = (_i)->alpha_channel = 0; \
	(_i)->gamma_corr = 0.0; \
	(_i)->data = NULL; \
    }

/**
 * returns truthfully whether a icv_image_file has been initialized.
 */
#define ICV_IMAGE_IS_INITIALIZED(_i) (((struct icv_image *)(_i) != ICV_IMAGE_NULL) && LIKELY((_i)->magic == ICV_IMAGE_MAGIC))

/* Validation Macros */
/**
 * Validates input icv_struct, if failure (in validation) returns -1
 */
#define ICV_IMAGE_VAL_INT(_i)  if (!ICV_IMAGE_IS_INITIALIZED(_i)) return -1

/**
 * Validates input icv_struct, if failure (in validation) returns NULL
 */
#define ICV_IMAGE_VAL_PTR(_i) if (!ICV_IMAGE_IS_INITIALIZED(_i)) return NULL


/* Data conversion MACROS  */
/**
 * Converts to double (icv data) type from unsigned char(8bit).
 */
#define ICV_CONV_8BIT(data) ((double)(data))/255.0

__END_DECLS

/** @} */

#endif /* ICV_DEFINES_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
