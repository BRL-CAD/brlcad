/*                        V E C T F O N T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file vectfont.h */
/** @addtogroup plot */
/** @{ */

/**
 *
 *	Terminal Independent Graphics Display Package.
 *		Mike Muuss  July 31, 1978
 *
 *	This routine is used to plot a string of ASCII symbols
 *  on the plot being generated, using a built-in set of fonts
 *  drawn as vector lists.
 *
 *	Internally, the basic font resides in a 10x10 unit square.
 *  Externally, each character can be thought to occupy one square
 *  plotting unit;  the 'scale'
 *  parameter allows this to be changed as desired, although scale
 *  factors less than 10.0 are unlikely to be legible.
 *
 *  The vector font table here was provided courtesy of Dr. Bruce
 *  Henrikson and Dr. Stephen Wolff, US Army Ballistic Research
 *  Laboratory, Summer of 1978.  They had developed it for their
 *  remote Houston Instruments pen plotter package for the
 *  GE Tymeshare system.
 *
 */

#ifndef BN_VECTFONT_H
#define BN_VECTFONT_H

#include "common.h"
#include "bn/defines.h"

__BEGIN_DECLS

/**
 * @brief
 *  Once-only setup routine
 *  Used by libplot3/symbol.c, so it can't be static.
 *  DEPRECATED: libplot3 has been merged into libbn, so this no longer needs to be public.
 */
BN_EXPORT extern void tp_setup(void);

__END_DECLS

#endif  /* BN_VECTFONT_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
