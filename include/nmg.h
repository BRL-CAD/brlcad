/*                           N M G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @addtogroup libnmg
 *
 * @brief
 * "Non-Manifold Geometry" (NMG) models are BRL-CAD's primary mechanism
 * for storing and manipulating planar mesh geometry.  The design is
 * based on work from the 1980s by Kevin J. Weiler.
 *
 * Original development was based on "Non-Manifold Geometric Boundary
 * Modeling" by Kevin Weiler, 5/7/87 (SIGGraph 1989 Course #20 Notes)
 *
 * See also "Topological Structures for Geometric Modeling"
 * by Kevin J. Weiler - RPI Phd thesis from 1986:
 * https://www.scorec.rpi.edu/REPORTS/1986-1.pdf
 *
 * Documentation of BRL-CAD's implementation was published as part of
 * Combinatorial Solid Geometry, Boundary Representations, and Non-Manifold
 * Geometry by Michael John Muuss and Lee A. Butler, originally published in
 * State of the Art in Computer Graphics: Visualization and Modeling D. F.
 * Rogers, R. A. Earnshaw editors, Springer-Verlag, New York, 1991, pages
 * 185-223:
 * https://ftp.arl.army.mil/~mike/papers/90nmg/joined.html
 */
/** @{ */
/** @brief Main header file for the BRL-CAD Non-Manifold Geometry Library, LIBNMG. */
/** @file nmg.h */
/** @} */

/*
 * TODO: since this original work, there have been a number of subsequent
 * papers published on ways of representing non-manifold geometry.  In
 * particular, the "partial entity" structure is purported to have essentially
 * the same representation power as Weiler's radial edge approach but with
 * more compact storage:
 *
 *  Sang Hun Lee and Kunwoo Lee, Partial Entity Structure: A Compact Boundary
 *  Representation for Non-Manifold Geometric Modeling, J. Comput. Inf. Sci.
 *  Eng 1(4), 356-365 (Nov 01, 2001)
 *
 *  Sang Hun Lee and Kunwoo Lee, Partial entity structure: a compact
 *  non-manifold boundary representation based on partial topological entities,
 *  SMA '01 Proceedings of the sixth ACM symposium on Solid modeling and
 *  applications Pages 159-170
 *
 * Also, from a design perspective, it would be nice if we could set this up so
 * that the more general non-manifold data structures include less general
 * containers and just extend them with the necessary extra information...
 * Don't know if that's possible, but it would be really nice from a data
 * conversion standpoint...
 *
 * TODO:  This paper may be worth a look from an API design perspective:
 * Topological Operators for Non-Manifold Modeling (1995) 
 * http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.50.1961
 *
 * also potentially useful:
 * https://www.cs.purdue.edu/homes/cmh/distribution/books/geo.html
 * https://cs.nyu.edu/faculty/yap/book/egc/
 *
 */

#ifndef NMG_H
#define NMG_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "vmath.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/magic.h"
#include "bu/ptbl.h"
#include "bg/plane.h"
#include "bn/tol.h"
#include "bv/vlist.h"
#include "vmath.h"

__BEGIN_DECLS

/* Standard BRL-CAD definitions for libraries, and common definitions used
 * broadly in libnmg logic. */
#include "nmg/defines.h"

/* Fundamental data structures used to represent NMG information */
#include "nmg/topology.h"

#include "nmg/debug.h"
#include "nmg/globals.h"
#include "nmg/vertex.h"
#include "nmg/edge.h"
#include "nmg/loop.h"
#include "nmg/face.h"
#include "nmg/shell.h"
#include "nmg/region.h"
#include "nmg/model.h"
#include "nmg/nurb.h"
#include "nmg/ray.h"
#include "nmg/plot.h"
#include "nmg/print.h"
#include "nmg/index.h"
#include "nmg/radial.h"
#include "nmg/visit.h"
#include "nmg/isect.h"
#include "nmg/check.h"

__END_DECLS

#endif /* NMG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
