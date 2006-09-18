/*                     S H O R T V E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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
/** @addtogroup libbn */
/*@{*/
/** @file shortvect.h
 *@brief
 *  This header file is intended to be include'ed in front of any
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
 *  Absolutely NO pragma's can go in this file;  they spoil backwards
 *  compatability with non-ANSI compilers.
 *  If this file determines that it is being processed by an ANSI
 *  compiler, or one that is known to accept pragma, then it will include
 *  the file "noalias-prag.h", which will contain the various
 *  vendor-specific pragma's.
 *
 *  @authors -
 *	David Becker		Cray
 *	Michael John Muuss	BRL
 *
 *  @par Source
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  @(#)$Header$ (BRL)
 */
#if __STDC__
#	include "shortvect-pr.h"		/* limit 14 chars */
#else
	/* convex? */
#endif
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

