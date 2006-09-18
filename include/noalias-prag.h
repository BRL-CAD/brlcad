/*                  N O A L I A S - P R A G . H
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
/** @file noalias-prag.h
 *
 *  This header file is intended to be included by "noalis.h",
 *  and may contain vendor-specific vectorization directives which
 *  use the C preprocessor pragma directive and other ANSI-C constructions.
 *
 *  This file will never be included when the compilation is not being
 *  processed by an ANSI-C compiler.
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
#	if defined(CRAY)
#		pragma ivdep
#	endif
#	if defined(alliant)
#		pragma noeqvchk
#	endif
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

