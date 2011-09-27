/*                           T O L . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file tol.h
 *
 * These routines provide access to the default tolerance values
 * available within LIBRT.  These routines assume working units of
 * 'mm' and are idealized to only attempt to account for
 * cross-platform hardware and floating point instability.  That is to
 * say that the default tolerance values are tight.
 *
 */

#include "raytrace.h" /* FIXME: need to reverse dependency but need
		       * RT_EXPORT broken out to do that.
		       */
#include "bn.h"


/**
 * Fills in the provided bn_tol structure with compile-time default
 * tolerance values.  These presently correspond to a distance
 * tolerance of 5e-5 (assuming default working units is 1/2000th a mm)
 * and a perpendicularity tolerance of 1e-6.  If tol is NULL, a
 * structure is allocated, initialized, and returned.
 */
RT_EXPORT extern struct bn_tol *rt_tol_default(struct bn_tol *tol);



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
