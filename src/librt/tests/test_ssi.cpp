/*                   T E S T _ S S I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>
#include "vmath.h"
#include "brep.h"
#include "raytrace.h"

#include "rtgeom.h"
#include "wdb.h"

int
get_surface(const char *name, struct db_i *dbip, struct rt_db_internal *intern, struct rt_brep_internal **brep_ip) {

    struct directory *dp;
    (*brep_ip) = NULL;


    dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_log("ERROR: Unable to look up object %s\n", name);
	return -1;
    }

    if (rt_db_get_internal(intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_log("ERROR: Unable to get internal representation of %s\n", name);
	return -1;
    }

    RT_CK_DB_INTERNAL(intern);

    if (intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_log("ERROR: object %s does not appear to be of type BRep\n", name);
	return -1;
    } else {
	(*brep_ip) = (struct rt_brep_internal *)(intern->idb_ptr);
    }

    RT_BREP_CK_MAGIC(*brep_ip);
    return 0;
}

int
main(int argc, char** argv)
{
    int ret = 0;
    struct bu_vls intersect_name, name;

    if (argc != 6 && argc != 7) {
	bu_log("Usage: %s %s\n", argv[0], "<file> <obj1> <obj2> <surf_i> <surf_j> [curve_name]");
	return -1;
    }

    // Read the file
    struct db_i *dbip;
    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_log("ERROR: Unable to read from geometry database file %s\n", argv[1]);
	return -1;
    }

    if (db_dirbuild(dbip) < 0) {
	bu_log("ERROR: Unable to read from %s\n", argv[1]);
	return -1;
    }

    // Get surfaces
    struct rt_db_internal obj1_intern, obj2_intern;
    struct rt_brep_internal *obj1_brep_ip = NULL, *obj2_brep_ip = NULL;

    RT_DB_INTERNAL_INIT(&obj1_intern);
    RT_DB_INTERNAL_INIT(&obj2_intern);
    if (get_surface(argv[2], dbip, &obj1_intern, &obj1_brep_ip)) return -1;
    if (get_surface(argv[3], dbip, &obj2_intern, &obj2_brep_ip)) return -1;

    // Manipulate the openNURBS objects
    ON_Brep *brep1 = obj1_brep_ip->brep;
    ON_Brep *brep2 = obj2_brep_ip->brep;

    ON_NurbsSurface surf1;
    ON_NurbsSurface surf2;

    int a = atoi(argv[4]), b = atoi(argv[5]);
    if (a < 0 || a >= brep1->m_S.Count() || b < 0 || b >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_S.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    brep1->m_S[a]->GetNurbForm(surf1);
    brep2->m_S[b]->GetNurbForm(surf2);

    // Run the intersection
    ON_ClassArray<ON_SSX_EVENT> events;
    if (ON_Intersect(&surf1, &surf2, events) < 0) {
	bu_log("Intersection failed\n");
	return -1;
    }

    bu_log("%d intersection segments.\n", events.Count());
    if (events.Count() == 0)
	return 0;

    bu_vls_init(&intersect_name);
    if (argc == 7) {
	bu_vls_sprintf(&intersect_name, "%s_", argv[6]);
    } else {
	bu_vls_sprintf(&intersect_name, "%s_%s_", argv[2], argv[3]);
    }

    // Print the information of the intersection curves
    bu_log("*** Intersection Events: ***\n");
    for (int i = 0; i < events.Count(); i++) {
	ON_wString wstr;
	ON_TextLog textlog(wstr);
	DumpSSXEvent(events[i], textlog);
	ON_String str = ON_String(wstr);
	bu_log("Intersection event %d:\n %s", i + 1, str.Array());
    }

    for (int i = 0; i < events.Count() * 2; i++) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	switch (events[i%events.Count()].m_type) {
	case ON_SSX_EVENT::ssx_overlap:
	case ON_SSX_EVENT::ssx_tangent:
	case ON_SSX_EVENT::ssx_transverse:
	    {
		// curves
		ON_Curve *curve2d = i < events.Count() ? events[i].m_curveA :
		    events[i - events.Count()].m_curveB;
		ON_NurbsCurve *nurbscurve2d = ON_NurbsCurve::New();
		curve2d->GetNurbForm(*nurbscurve2d);
		// Use a sketch primitive to represent a 2D curve (polyline curve).
		// The CVs of the curve are used as vertexes of the sketch.
		struct rt_sketch_internal *sketch;
		int vert_count = nurbscurve2d->CVCount();
		intern.idb_type = ID_SKETCH;
		intern.idb_meth = &OBJ[ID_SKETCH];
		BU_ALLOC(intern.idb_ptr, struct rt_sketch_internal);
		sketch = (struct rt_sketch_internal *)intern.idb_ptr;
		sketch->magic = RT_SKETCH_INTERNAL_MAGIC;
		VSET(sketch->V, 0.0, 0.0, 0.0);
		VSET(sketch->u_vec, 1.0, 0.0, 0.0);
		VSET(sketch->v_vec, 0.0, 1.0, 0.0);
		sketch->vert_count = vert_count;
		sketch->curve.count = vert_count - 1;

		// The memory must be dynamic allocated since they are bu_free'd
		// in rt_db_free_internal()
		sketch->verts = (point2d_t *)bu_calloc(vert_count, sizeof(point2d_t), "sketch->verts");
		sketch->curve.reverse = (int *)bu_calloc(vert_count - 1, sizeof(int), "sketch->crv->reverse");
		sketch->curve.segment = (genptr_t *)bu_calloc(vert_count - 1, sizeof(genptr_t), "sketch->crv->segments");

		for (int j = 0; j < vert_count; j++) {
		    ON_3dPoint CV3d;
		    nurbscurve2d->GetCV(j, CV3d);
		    sketch->verts[j][0] = CV3d.x;
		    sketch->verts[j][1] = CV3d.y;
		    if (j != 0) {
			struct line_seg *lsg;
			BU_ALLOC(lsg, struct line_seg);
			lsg->magic = CURVE_LSEG_MAGIC;
			lsg->start = j - 1;
			lsg->end = j;
			sketch->curve.segment[j - 1] = (genptr_t)lsg;
			sketch->curve.reverse[j - 1] = 0;
		    }
		}
		break;
	    }
	case ON_SSX_EVENT::ssx_tangent_point:
	case ON_SSX_EVENT::ssx_transverse_point:
	    {
		// use a sphere to display points
		ON_3dPoint center = i < events.Count() ? events[i].m_pointA :
		    events[i - events.Count()].m_pointB;
		double radius = 0.1;
		struct rt_ell_internal* sph;
		intern.idb_type = ID_SPH;
		intern.idb_meth = &OBJ[ID_SPH];
		BU_ALLOC(intern.idb_ptr, struct rt_ell_internal);
		sph = (struct rt_ell_internal *)intern.idb_ptr;
		sph->magic = RT_ELL_INTERNAL_MAGIC;
		VSET(sph->v, center.x, center.y, center.z);
		VSET(sph->a, radius, 0.0, 0.0);
		VSET(sph->b, 0.0, radius, 0.0);
		VSET(sph->c, 0.0, 0.0, radius);
		break;
	    }
	default:
	    break;
	}

	bu_vls_init(&name);
	bu_vls_sprintf(&name, "%s2d%c_%d", bu_vls_addr(&intersect_name),
	    i < events.Count() ? 'A' : 'B', i % events.Count());

	struct directory *dp;
	dp = db_diradd(dbip, bu_vls_addr(&name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern.idb_type);
	ret = rt_db_put_internal(dp, dbip, &intern, &rt_uniresource);
	if (ret)
	    bu_log("ERROR: failure writing [%s] to disk\n", dp->d_namep);
	else
	    bu_log("%s is written to file.\n", dp->d_namep);
	bu_vls_free(&name);
    }

    for (int i = 0; i < events.Count(); i++) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	switch (events[i].m_type) {
	case ON_SSX_EVENT::ssx_overlap:
	case ON_SSX_EVENT::ssx_tangent:
	case ON_SSX_EVENT::ssx_transverse:
	    {
		// Use a pipe primitive to represent a curve.
		// The CVs of the curve are used as vertexes of the pipe.
		ON_Curve *curve3d = events[i].m_curve3d;
		ON_NurbsCurve *nurbscurve3d = ON_NurbsCurve::New();
		curve3d->GetNurbForm(*nurbscurve3d);
		intern.idb_type = ID_PIPE;
		intern.idb_meth = &OBJ[ID_PIPE];
		BU_ALLOC(intern.idb_ptr, struct rt_pipe_internal);
		struct rt_pipe_internal *pi;
		pi = (struct rt_pipe_internal *)intern.idb_ptr;
		pi->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
		pi->pipe_count = nurbscurve3d->CVCount();
		BU_LIST_INIT(&(pi->pipe_segs_head));
		struct wdb_pipept *ps;

		fastf_t od = nurbscurve3d->BoundingBox().Diagonal().Length() * 0.05;
		for (int j = 0; j < nurbscurve3d->CVCount(); j++) {
		    BU_ALLOC(ps, struct wdb_pipept);
		    ps->l.magic = WDB_PIPESEG_MAGIC;
		    ps->l.back = NULL;
		    ps->l.forw = NULL;
		    ON_3dPoint p;
		    nurbscurve3d->GetCV(j, p);
		    VSET(ps->pp_coord, p.x, p.y, p.z);
		    ps->pp_id = 0.0;
		    ps->pp_od = od;
		    ps->pp_bendradius = 0;
		    BU_LIST_INSERT(&pi->pipe_segs_head, &ps->l);
		}
		break;
	    }
	case ON_SSX_EVENT::ssx_tangent_point:
	case ON_SSX_EVENT::ssx_transverse_point:
	    {
		// use a sphere to display points
		ON_3dPoint center = events[i].m_point3d;
		double radius = 0.1;
		struct rt_ell_internal* sph;
		intern.idb_type = ID_SPH;
		intern.idb_meth = &OBJ[ID_SPH];
		BU_ALLOC(intern.idb_ptr, struct rt_ell_internal);
		sph = (struct rt_ell_internal *)intern.idb_ptr;
		sph->magic = RT_ELL_INTERNAL_MAGIC;
		VSET(sph->v, center.x, center.y, center.z);
		VSET(sph->a, radius, 0.0, 0.0);
		VSET(sph->b, 0.0, radius, 0.0);
		VSET(sph->c, 0.0, 0.0, radius);
		break;
	    }
	default:
	    break;
	}

	bu_vls_init(&name);
	bu_vls_sprintf(&name, "%s3d_%d", bu_vls_addr(&intersect_name), i);

	struct directory *dp;
	dp = db_diradd(dbip, bu_vls_addr(&name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern.idb_type);
	ret = rt_db_put_internal(dp, dbip, &intern, &rt_uniresource);
	if (ret)
	    bu_log("ERROR: failure writing [%s] to disk\n", dp->d_namep);
	else
	    bu_log("%s is written to file.\n", dp->d_namep);
	bu_vls_free(&name);
    }

    bu_vls_free(&intersect_name);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
