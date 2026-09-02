/*                      I G E S _ S U R F . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges/iges_surf.h
 *
 * Read IGES analytic / spline surface entities that the classic
 * importer's Manifold BREP (type 186) face path would otherwise drop,
 * and produce an NMG rational B-spline surface (face_g_snurb) for the
 * face.
 *
 * The surface is built exactly as an OpenNURBS ON_NurbsSurface (through
 * libbrep) and then copied into a face_g_snurb -- the inverse of the
 * face_g_snurb -> ON_NurbsSurface bridge in iges_brep.cpp.  Supported
 * IGES surface types:
 *
 *   118  Ruled Surface
 *   120  Surface of Revolution
 *   122  Tabulated Cylinder
 *   140  Offset Surface
 *   114  Parametric Spline Surface (currently a graceful skip)
 */

#ifndef CONV_IGES_IGES_SURF_H
#define CONV_IGES_IGES_SURF_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct model;		/* NMG model (nmg.h) */
struct face_g_snurb;	/* NMG NURBS surface (nmg.h) */

/**
 * Read the IGES surface entity at directory-array index @p entityno and
 * build an NMG rational B-spline surface (face_g_snurb) for it, allocated
 * from NMG model @p m.
 *
 * Dispatches on the IGES entity type (114/118/120/122/140).  Returns the
 * new face_g_snurb on success, or NULL if the type is unsupported or the
 * conversion fails (the caller should then skip the face gracefully).
 */
extern struct face_g_snurb *Get_iges_nurb_surf(size_t entityno, struct model *m);

#ifdef __cplusplus
}
#endif

#endif /* CONV_IGES_IGES_SURF_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
