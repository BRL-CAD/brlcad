/*                           F F T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
/** @file fft.h
 *
 */

#include "common.h"

#include <math.h>

#ifndef M_PI
#  define M_PI 3.141592653589793238462643
#endif
#ifndef INVSQ2
#  define INVSQ2 0.70710678118654752440084436210
#endif
#ifndef SQRT2
#  define SQRT2 1.41421356237309504880168872421
#endif

extern void splitdit(int N, int M);
extern void ditsplit(int n /* length */, int m /* n = 2^m */);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
