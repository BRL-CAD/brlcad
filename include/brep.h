/*                       B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2016 United States Government as represented by
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
/** @addtogroup libbrep
 * @brief
 * Define surface and curve structures for Non-Uniform Rational
 * B-Spline (NURBS) curves and surfaces. Uses openNURBS library.
 */
/** @{ */
/** @file include/brep.h */
#ifndef BREP_H
#define BREP_H

#include "common.h"

#include "bio.h" /* needed to include windows.h with protections */
#ifdef __cplusplus
extern "C++" {
#define ON_NO_WINDOWS 1 /* don't let opennurbs include windows.h */
/* Note - We aren't (yet) including opennurbs in our Doxygen output. Until we
 * do, use cond to hide the opennurbs header from Doxygen. */
/* @cond */
#include "opennurbs.h"
/* @endcond */
}
#endif

#include "vmath.h"

__BEGIN_DECLS

#include "brep/defines.h"
#include "brep/util.h"
#include "brep/ray.h"
#include "brep/brnode.h"
#include "brep/curvetree.h"
#include "brep/bbnode.h"
#include "brep/surfacetree.h"
#include "brep/pullback.h"
#include "brep/intersect.h"
#include "brep/boolean.h"
#include "brep/csg.h"

__END_DECLS

#endif  /* BREP_H */
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
