/*                    B R E P _ L O C A L . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2019 United States Government as represented by
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
/** @file brep_local.h
 *
 * Local data structures for brep primitive
 */

#ifndef LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H
#define LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H


/**
 * The b-rep specific data structure for caching the prepared
 * acceleration data structure.
 */
struct brep_specific {
    ON_Brep* brep;
    BrepBoundingVolume* bvh;
};


#ifdef __cplusplus
#include "brep.h"
/* poly2tri interface functions */
extern void poly2tri_CDT(struct bu_list *vhead, ON_BrepFace &face, const struct rt_tess_tol *ttol, const struct bn_tol *tol, const struct rt_view_info *info, bool watertight = false, int plottype = 0, int num_points = -1.0);

int brep_facecdt_plot(struct bu_vls *vls, const char *solid_name,
	const struct rt_tess_tol *ttol, const struct bn_tol *tol,
	struct brep_specific* bs, struct rt_brep_internal*UNUSED(bi),
	struct bn_vlblock *vbp, int index, int plottype, int num_points = -1);

#endif

#endif /* LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
