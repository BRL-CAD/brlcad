/*
 *			S H O R T V E C T . H
 *
 *  This header file is intended to be #include'ed in front of any
 *  loop which is known to never involve more than 32,
 *  to permit vectorizing compilers to omit extra overhead.
 *
 *  Vector register file lengths:
 *	Alliant		32
 *	Cray XMP	64
 *	Cray 2		64
 *	Convex		128
 *	ETA-10		Unlimited
 *
 *  This file contains various definitions which can be safely seen
 *  by both ANSI and non-ANSI compilers and preprocessors.
 *  Absolutely NO #pragma's can go in this file;  they spoil backwards
 *  compatability with non-ANSI compilers.
 *  If this file determines that it is being processed by an ANSI
 *  compiler, or one that is known to accept #pragma, then it will #include
 *  the file "noalias-prag.h", which will contain the various
 *  vendor-specific #pragma's.
 *
 *  Authors -
 *	David Becker		Cray
 *	Michael John Muuss	BRL
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  @(#)$Header$ (BRL)
 */
#if __STDC__
#	include "shortvect-pr.h"		/* limit 14 chars */
#else
	/* convex? */
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
