/*                           V L B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include "common.h"

#include <string.h>

#include "bu.h"


#define VLB_BLOCK_SIZE 512


void
bu_vlb_init(struct bu_vlb *vlb)
{
    vlb->buf = bu_calloc(1, VLB_BLOCK_SIZE, "bu_vlb");
    vlb->bufCapacity = VLB_BLOCK_SIZE;
    vlb->nextByte = 0;
    vlb->magic = BU_VLB_MAGIC;
}


void
bu_vlb_initialize(struct bu_vlb *vlb, size_t initialSize)
{
    if (UNLIKELY(initialSize <= 0)) {
	bu_log("bu_vlb_initialize: WARNING - illegal initial size (%zu), ignored\n", initialSize);
	bu_vlb_init(vlb);
	return;
    }
    vlb->buf = bu_calloc(1, initialSize, "bu_vlb");
    vlb->bufCapacity = initialSize;
    vlb->nextByte = 0;
    vlb->magic = BU_VLB_MAGIC;
}


void
bu_vlb_write(struct bu_vlb *vlb, unsigned char *start, size_t len)
{
    size_t addBlocks = 0;
    size_t currCapacity;

    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    currCapacity = vlb->bufCapacity;
    while (currCapacity <= (vlb->nextByte + len)) {
	addBlocks++;
	currCapacity += VLB_BLOCK_SIZE;
    }

    if (addBlocks) {
	vlb->buf = bu_realloc(vlb->buf, currCapacity, "enlarging vlb");
	vlb->bufCapacity = currCapacity;
    }

    memcpy(&vlb->buf[vlb->nextByte], start, len);
    vlb->nextByte += len;
}


void
bu_vlb_reset(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    vlb->nextByte = 0;
}


unsigned char *
bu_vlb_addr(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    return vlb->buf;
}


size_t
bu_vlb_buflen(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    return vlb->nextByte;
}


void
bu_vlb_free(struct bu_vlb *vlb)
{
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    if (vlb->buf != NULL) {
	bu_free(vlb->buf, "vlb");
    }
    vlb->buf = NULL;
    vlb->bufCapacity = 0;
    vlb->nextByte = -1;
    vlb->magic = 0;
}


void
bu_vlb_print(struct bu_vlb *vlb, FILE *fd)
{
    int ret;
    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");
    ret = fwrite(vlb->buf, 1, vlb->nextByte, fd);
}


void
bu_pr_vlb(const char *title, const struct bu_vlb *vlb)
{
    size_t i;
    unsigned char *c;
    struct bu_vls v;

    BU_CKMAG(vlb, BU_VLB_MAGIC, "magic for bu_vlb");

    bu_vls_init(&v);

    /* optional title */
    if (title) {
	bu_vls_strcat(&v, title);
	bu_vls_strcat(&v, ": ");
    }

    /* print each byte to string buffer */
    c = vlb->buf;
    for (i=0; i<vlb->nextByte; i++) {
	bu_vls_printf(&v, "%02x ", *c);
	c++;
    }

    /* print as one call */
    bu_log("%V", &v);

    bu_vls_free(&v);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
