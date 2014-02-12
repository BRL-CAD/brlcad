/*                         V L B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/** @defgroup container Data Containers */
/**   @defgroup vlb Variable-length Byte Buffers */

#ifndef BU_VLB_H
#define BU_VLB_H

#include "common.h"

#include "stdio.h" /* For FILE - TODO, do we want to use bio.h here?*/

#include "./defines.h"

/*----------------------------------------------------------------------*/
/** @addtogroup vlb */
/** @ingroup container */
/** @{ */
/** @file libbu/vlb.c
 *
 * The variable length buffer package.
 *
 * The variable length buffer package.
 *
 */

/**
 * Variable Length Buffer: bu_vlb support
 */
struct bu_vlb {
    uint32_t magic;
    unsigned char *buf;     /**< Dynamic memory for the buffer */
    size_t bufCapacity;     /**< Current capacity of the buffer */
    size_t nextByte;        /**< Number of bytes currently used in the buffer */
};
typedef struct bu_vlb bu_vlb_t;
#define BU_VLB_NULL ((struct bu_vlb *)0)

/**
 * assert the integrity of a bu_vlb struct.
 */
#define BU_CK_VLB(_vp) BU_CKMAG(_vp, BU_VLB_MAGIC, "bu_vlb")

/**
 * initializes a bu_vlb struct without allocating any memory.
 */
#define BU_VLB_INIT(_vp) { \
	(_vp)->magic = BU_VLB_MAGIC; \
	(_vp)->buf = NULL; \
	(_vp)->bufCapacity = (_vp)->nextByte = 0; \
    }

/**
 * macro suitable for declaration statement initialization of a bu_vlb
 * struct.  does not allocate memory.
 */
#define BU_VLB_INIT_ZERO { BU_VLB_MAGIC, NULL, 0, 0 }

/**
 * returns truthfully whether a bu_vlb struct has been initialized.
 * is not reliable unless the struct has been allocated with
 * BU_ALLOC(), bu_calloc(), or a previous call to bu_vlb_init() or
 * BU_VLB_INIT() has been made.
 */
#define BU_VLB_IS_INITIALIZED(_vp) (((struct bu_vlb *)(_vp) != BU_VLB_NULL) && ((_vp)->magic == BU_VLB_MAGIC))

/**
 * Initialize the specified bu_vlb structure and mallocs the initial
 * block of memory.
 *
 * @param vlb Pointer to an uninitialized bu_vlb structure
 */
BU_EXPORT extern void bu_vlb_init(struct bu_vlb *vlb);

/**
 * Initialize the specified bu_vlb structure and mallocs the initial
 * block of memory with the specified size
 *
 * @param vlb Pointer to an uninitialized bu_vlb structure
 * @param initialSize The desired initial size of the buffer
 */
BU_EXPORT extern void bu_vlb_initialize(struct bu_vlb *vlb,
					size_t initialSize);

/**
 * Write some bytes to the end of the bu_vlb structure. If necessary,
 * additional memory will be allocated.
 *
 * @param vlb Pointer to the bu_vlb structure to receive the bytes
 * @param start Pointer to the first byte to be copied to the bu_vlb structure
 * @param len The number of bytes to copy to the bu_vlb structure
 */
BU_EXPORT extern void bu_vlb_write(struct bu_vlb *vlb,
				   unsigned char *start,
				   size_t len);

/**
 * Reset the bu_vlb counter to the start of its byte array. This
 * essentially ignores any bytes currently in the buffer, but does not
 * free any memory.
 *
 * @param vlb Pointer to the bu_vlb structure to be reset
 */
BU_EXPORT extern void bu_vlb_reset(struct bu_vlb *vlb);

/**
 * Get a pointer to the byte array held by the bu_vlb structure
 *
 * @param vlb Pointer to the bu_vlb structure
 * @return A pointer to the byte array contained by the bu_vlb structure
 */
BU_EXPORT extern unsigned char *bu_vlb_addr(struct bu_vlb *vlb);

/**
 * Return the number of bytes used in the bu_vlb structure
 *
 * @param vlb Pointer to the bu_vlb structure
 * @return The number of bytes written to the bu_vlb structure
 */
BU_EXPORT extern size_t bu_vlb_buflen(struct bu_vlb *vlb);

/**
 * Free the memory allocated for the byte array in the bu_vlb
 * structure.  Also uninitializes the structure.
 *
 * @param vlb Pointer to the bu_vlb structure
 */
BU_EXPORT extern void bu_vlb_free(struct bu_vlb *vlb);
/**
 * Write the current byte array from the bu_vlb structure to a file
 *
 * @param vlb Pointer to the bu_vlb structure that is the source of the bytes
 * @param fd Pointer to a FILE to receive the bytes
 */
BU_EXPORT extern void bu_vlb_print(struct bu_vlb *vlb,
				   FILE *fd);

/**
 * Print the bytes set in a variable-length byte array.
 */
BU_EXPORT extern void bu_pr_vlb(const char *title, const struct bu_vlb *vlb);


#endif  /* BU_VLB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
