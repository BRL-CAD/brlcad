/*                       N O A L I A S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup libbn */
/** @{ */
/** @file noalias.h
 *
 *  This header file is intended to be included in front of any
 *  loop that involves pointers that do not alias each other,
 *  to permit vectorizing compilers to proceed.
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
 *
 *  @author	David Becker		Cray
 *  @author	Michael John Muuss	BRL
 *
 *  @par Source
 *	SECAD/VLD Computing Consortium, Bldg 394
 *@n	The U. S. Army Ballistic Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  @(#)$Header$ (BRL)
 */
#if __STDC__
#	include "noalias-prag.h"
#else
#	if defined(convex) || defined(__convex__)
		/*$dir no_recurrence */
#	endif
#endif
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
