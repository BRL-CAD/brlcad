/*                     I G E S _ B R E P . H
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
/** @file iges/iges_brep.h
 *
 * Conversion of the assembled NMG boundary representation (built by the
 * classic IGES importer from IGES entities 186/514/510/508/504/502 and
 * 144/128/126/142) into an OpenNURBS ON_Brep, stored as a BRL-CAD
 * rt_brep object.
 *
 * The NMG the importer builds already carries the original analytic
 * surfaces (NURBS as face_g_snurb, planes as face_g_plane) and the
 * parameter-space trim curves (edge_g_cnurb).  Converting that NMG to an
 * ON_Brep therefore preserves the original representation faithfully,
 * rather than tessellating to a triangle mesh (BoT) or down-converting.
 */

#ifndef CONV_IGES_IGES_BREP_H
#define CONV_IGES_IGES_BREP_H

#include "common.h"

#include "vmath.h"
#include "bn/tol.h"

__BEGIN_DECLS

struct model;		/* NMG model (nmg.h) */
struct rt_wdb;		/* output database (wdb.h) */
struct face_g_snurb;	/* NMG NURBS surface (nmg.h) */

/**
 * Convert an assembled NMG @p m (whose faces may be planar and/or NURBS)
 * to an OpenNURBS ON_Brep and write it to @p wdbp as an rt_brep object
 * named @p name.
 *
 * The brep is validated with ON_Brep::IsValid() before it is written.
 *
 * Returns 1 on success (a valid brep was written), 0 on failure.  On
 * failure nothing is written and the caller should fall back to an NMG
 * or BoT (mesh) representation of the same model.
 */
extern int iges_nmg_to_brep(struct rt_wdb *wdbp, const char *name,
			    struct model *m, const struct bn_tol *tol);

/**
 * Convert a collection of untrimmed NURBS surfaces (NMG face_g_snurb, e.g.
 * from IGES 128 entities gathered by the '-n' path) to an ON_Brep, with one
 * natural-boundary ON_BrepFace per surface, and write it to @p wdbp as an
 * rt_brep object named @p name.  The result is a surface model (not
 * necessarily a closed solid).
 *
 * @p surfs is an array of @p count surface pointers (NULL entries skipped).
 * The surfaces are copied; the caller retains ownership.
 *
 * Returns 1 on success, 0 on failure (caller should fall back).
 */
extern int iges_snurbs_to_brep(struct rt_wdb *wdbp, const char *name,
			       struct face_g_snurb **surfs, int count);

__END_DECLS

#endif /* CONV_IGES_IGES_BREP_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
