/*
 *			H T O N F . C
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Release Status -
 *	Public Domain, Distribution Unlimited
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

/*
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

#if	defined(REVERSED_IEEE)
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


/*
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
#if	defined(REVERSED_IEEE)
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
