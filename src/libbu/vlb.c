/*                           V L B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @addtogroup vlb */
/** @{ */
/** @file vlb.c
 *
 * @brief The variable length buffer package.
 *
 * The variable length buffer package.
 *
 */

#include "common.h"
#include "bu.h"
#include <string.h>

/**
 * Initialize the specified bu_vlb structure and mallocs the initial
 * block of memory.
 *
 * @param vlb Pointer to an unitialized bu_vlb structure
 */
void
bu_vlb_init(struct bu_vlb *vlb)
{
    vlb->buf = bu_calloc(1, BU_VLB_BLOCK_SIZE, "bu_vlb");
    vlb->bufCapacity = BU_VLB_BLOCK_SIZE;
    vlb->nextByte = 0;
    vlb->magic = BU_VLB_MAGIC;
}

/**
 * Initialize the specified bu_vlb structure and mallocs the initial
 * block of memory with the specified size
 *
 * @param vlb Pointer to an unitialized bu_vlb structure
 * @param initialSize The desired initial size of the buffer
 */
void
bu_vlb_initialize(struct bu_vlb *vlb, int initialSize)
{
    if( initialSize <= 0 ) {
        bu_log( "bu_vlb_initialize: WARNING - illegal initial size (%d), ignored\n", initialSize);
        bu_vlb_init(vlb);
        return;
    }
    vlb->buf = bu_calloc(1, initialSize, "bu_vlb");
    vlb->bufCapacity = initialSize;
    vlb->nextByte = 0;
    vlb->magic = BU_VLB_MAGIC;
}

/**
 * Write some bytes to the end of the bu_vlb structure. If necessary,
 * additional memory will be allocated.
 *
 * @param vlb Pointer to the bu_vlb structure to receive the bytes
 * @param start Pointer to the first byte to be copied to the bu_vlb structure
 * @param len The number of bytes to copy to the bu_vlb structure
 */
void
bu_vlb_write(struct bu_vlb *vlb, unsigned char *start, int len)
{
    int addBlocks = 0;
    int currCapacity;
    
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    currCapacity = vlb->bufCapacity;
    while (currCapacity <= (vlb->nextByte + len)) {
        addBlocks++;
        currCapacity += BU_VLB_BLOCK_SIZE;
    }
    
    if (addBlocks) {
        vlb->buf = bu_realloc(vlb->buf, currCapacity, "enlarging vlb");
        vlb->bufCapacity = currCapacity;
    }
    
    memcpy(&vlb->buf[vlb->nextByte], start, len);
    vlb->nextByte += len;
}

/**
 * Reset the bu_vlb counter to the start of its byte array. This essentially
 * ignores any bytes currenty in the buffer, but does not free any memory.
 *
 * @param vlb Pointer to the bu_vlb structure to be reset
 */
void
bu_vlb_reset(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    vlb->nextByte = 0;
}
 /**
  * Get a pointer to the byte array held by the bu_vlb structure
  *
  * @param vlb Pointer to the bu_vlb structure
  * @return A pointer to the byte array contained by the bu_vlb structure
  */
unsigned char *
bu_vlb_getBuffer(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    return vlb->buf;
}

/**
 * Return the number of bytes used in the bu_vlb structure
 *
 * @param vlb Pointer to the bu_vlb structure
 * @return The number of bytes written to the bu_vlb structure
 */
int
bu_vlb_getBufferLength(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    return vlb->nextByte;
}

/**
 * Free the memory allocated for the byte array in the bu_vlb structure.
 * Also unitializes the structure.
 *
 *  @param vlb Pointer to the bu_vlb structure
 */
void
bu_vlb_free(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    if( vlb->buf != NULL ) {
        bu_free(vlb->buf, "vlb");
    }
    vlb->buf = NULL;
    vlb->bufCapacity = 0;
    vlb->nextByte = -1;
    vlb->magic = 0;
}

/**
 * Write the current byte array from the bu_vlb structure to a file
 *
 * @param vlb Pointer to the bu_vlb structure that is the source of the bytes
 * @param fd Pointer to a FILE to receive the bytes
 */
void
bu_vlb_print(struct bu_vlb *vlb, FILE *fd)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    fwrite(vlb->buf, 1, vlb->nextByte, fd);
}
