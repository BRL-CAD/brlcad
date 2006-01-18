/*                  S H O R T V E C T - P R . H
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
/** @file shortvect-pr.h
 *			S H O R T V E C T - P R A G . H
 *
 *  This header file is intended to be #include'ed by "shortvect.h",
 *  and may contain vendor-specific vectorization directives which
 *  use #pragma and other ANSI-C constructions.
 *
 *  This file will never be included when the compilation is not being
 *  processed by an ANSI-C compiler.
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
 *  @(#)$Header$ (BRL)
 */
#if __STDC__
#	if defined(CRAY)
#		pragma shortloop
#	endif
#	if defined(alliant)
		/* ??? */
#	endif
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
