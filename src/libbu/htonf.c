/*                         H T O N F . C
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

#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif
#include <stdio.h>
#include <assert.h>

#include "bu.h"


void
htonf(register unsigned char *out, register const unsigned char *in, size_t count)
{
    register size_t i;

    assert(sizeof(float) == SIZEOF_NETWORK_FLOAT);

    switch (bu_byteorder()) {
	case BU_BIG_ENDIAN:
	    /*
	     * First, the case where the system already operates in
	     * IEEE format internally, using big-endian order.  These
	     * are the lucky ones.
	     */
	    memcpy(out, in, count*SIZEOF_NETWORK_FLOAT);
	    return;
	case BU_LITTLE_ENDIAN:
	    /*
	     * This machine uses IEEE, but in little-endian byte order
	     */
	    for (i=count; i > 0; i--) {
		*out++ = in[3];
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
		in += SIZEOF_NETWORK_FLOAT;
	    }
	    return;
	default:
	    /* nada */
	    break;
    }

    bu_bomb("ntohf.c:  ERROR, no NtoHD conversion for this machine type\n");
}


void
ntohf(register unsigned char *out, register const unsigned char *in, size_t count)
{
    register size_t i;

    assert(sizeof(float) == SIZEOF_NETWORK_FLOAT);

    switch (bu_byteorder()) {
	case BU_BIG_ENDIAN:
	    /*
	     * First, the case where the system already operates in
	     * IEEE format internally, using big-endian order.  These
	     * are the lucky ones.
	     */
	    memcpy(out, in, count*SIZEOF_NETWORK_FLOAT);
	    return;
	case BU_LITTLE_ENDIAN:
	    /*
	     * This machine uses IEEE, but in little-endian byte order
	     */
	    for (i=count; i > 0; i--) {
		*out++ = in[3];
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
		in += SIZEOF_NETWORK_FLOAT;
	    }
	    return;
	default:
	    /* nada */
	    break;
    }

    bu_bomb("ntohf.c:  ERROR, no NtoHD conversion for this machine type\n");
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
