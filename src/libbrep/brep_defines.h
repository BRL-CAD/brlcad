/*                  B R E P _ D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file brep_defines.h
 *
 * Private defines.
 *
 */

// The maximal depth for subdivision - trade-off between accuracy and
// performance.
#define NR_MAX_DEPTH 8
#define MAX_PCI_DEPTH NR_MAX_DEPTH
#define MAX_PSI_DEPTH NR_MAX_DEPTH
#define MAX_CCI_DEPTH NR_MAX_DEPTH
#define MAX_CSI_DEPTH NR_MAX_DEPTH
#define MAX_SSI_DEPTH NR_MAX_DEPTH

// Used to prevent an infinite loop in the unlikely event that we
// can't provide a good starting point for the Newton-Raphson
// Iteration.
#define NR_MAX_ITERATIONS 100
#define PCI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define PSI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define CCI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define CSI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define SSI_MAX_ITERATIONS NR_MAX_ITERATIONS

// We make the default tolerance for PSI the same as that of curve and
// surface intersections defined by openNURBS (see opennurbs_curve.h
// and opennurbs_surface.h).
#define NR_DEFAULT_TOLERANCE 0.001
#define PCI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define PSI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define CCI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define CSI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define SSI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE

// tol value used in ON_Intersect()s. We use a smaller tolerance than the
// default one 0.001.
#define INTERSECTION_TOL 1e-4

// tol value used in ON_3dVector::IsParallelTo(). We use a smaller tolerance
// than the default one ON_PI/180.
#define ANGLE_TOL ON_PI/1800.0

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
