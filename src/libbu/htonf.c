/*                         H T O N F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup hton */
/*@{*/
/** @file htonf.c
 *
 *@brief convert floats to host/network format
 *
 * Host to Network Floats  +  Network to Host Floats.
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */


#ifndef lint
static const char libbu_htond_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#include "machine.h"
#include "bu.h"

#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif
#include <stdio.h>

/**
 *			H T O N F
 *
 *  Host to Network Floats
 */
void
htonf(register unsigned char *out, register const unsigned char *in, int count)
{
#if	defined(NATURAL_IEEE)
	/*
	 *  First, the case where the system already operates in
	 *  IEEE format internally, using big-endian order.
	 *  These are the lucky ones.
	 */
#	ifdef HAVE_MEMORY_H
		memcpy( out, in, count*SIZEOF_NETWORK_FLOAT );
#	else
		bcopy( in, out, count*SIZEOF_NETWORK_FLOAT );
#	endif
	return;
#	define	HTONF	yes1
#endif

#if	defined(REVERSE_IEEE)
	/* This machine uses IEEE, but in little-endian byte order */
	register int	i;
	for( i=count-1; i >= 0; i-- )  {
		*out++ = in[3];
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
		in += SIZEOF_NETWORK_FLOAT;
	}
	return;
#	define	HTONF	yes2
#endif

	/* Now, for the machine-specific stuff. */

#ifndef HTONF
# include "ntohf.c:  ERROR, no NtoHD conversion for this machine type"
#endif
}


/**
 *			N T O H F
 *
 *  Network to Host Floats
 */
void
ntohf(register unsigned char *out, register const unsigned char *in, int count)
{
#ifdef NATURAL_IEEE
	/*
	 *  First, the case where the system already operates in
	 *  IEEE format internally, using big-endian order.
	 *  These are the lucky ones.
	 */
	if( sizeof(float) != SIZEOF_NETWORK_FLOAT )
		bu_bomb("ntohf:  sizeof(float) != SIZEOF_NETWORK_FLOAT\n");
#	ifdef HAVE_MEMORY_H
		memcpy( out, in, count*SIZEOF_NETWORK_FLOAT );
#	else
		bcopy( in, out, count*SIZEOF_NETWORK_FLOAT );
#	endif
	return;
#	define	NTOHF	yes1
#endif
#if	defined(REVERSE_IEEE)
	/* This machine uses IEEE, but in little-endian byte order */
	register int	i;
	for( i=count-1; i >= 0; i-- )  {
		*out++ = in[3];
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
		in += SIZEOF_NETWORK_FLOAT;
	}
	return;
#	define	NTOHF	yes2
#endif

#ifndef NTOHF
# include "ntohf.c:  ERROR, no NtoHD conversion for this machine type"
#endif
}
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
