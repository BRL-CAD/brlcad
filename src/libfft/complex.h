/*                       C O M P L E X . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file complex.h
 *
 * The COMPLEX type used throughout.
 */

typedef struct {
	double	re;	/* Real Part */
	double	im;	/* Imaginary Part */
} COMPLEX;

#define	CMAG(c)	(hypot( c.re, c.im ))

/* Sometimes found in <math.h> */
#if !defined(PI)
#	define	PI	3.141592653589793238462643
#endif

#define	TWOPI	6.283185307179586476925286

/* Degree <-> Radian conversions */
#define	RtoD(x)	((x)*57.29577951308232157827)
#define	DtoR(x) ((x)*0.01745329251994329555)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
