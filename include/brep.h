/*                       B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2022 United States Government as represented by
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
#include "brep/cdt.h"
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
