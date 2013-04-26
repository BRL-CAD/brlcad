/*                   T E S T _ S S I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>
#include "common.h"
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

    if (argc != 6 && argc != 7) {
	bu_log("Usage: %s %s\n", argv[0], "<file> <obj1> <obj2> <surf_i> <surf_j> [curve_name]");
	return -1;
    }

    // Read the file
    struct db_i *dbip;
    dbip = db_open(argv[1], "r+w");
    if (dbip == DBI_NULL) {
	bu_log("ERROR: Unable to read from %s\n", argv[1]);
	return -1;
    }

    if (db_dirbuild(dbip) < 0){
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
    ON_SimpleArray<ON_NurbsCurve*> curve;
    ON_SimpleArray<ON_NurbsCurve*> curve_uv;
    ON_SimpleArray<ON_NurbsCurve*> curve_st;

    int i = atoi(argv[4]), j = atoi(argv[5]);
    if (i < 0 || i >= brep1->m_S.Count() || j < 0 || j >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_S.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    brep1->m_S[i]->GetNurbForm(surf1);
    brep2->m_S[j]->GetNurbForm(surf2);

    // Run the intersection (max_dis = 0)
    if (brlcad::surface_surface_intersection(&surf1, &surf2, curve, curve_uv, curve_st, 0)) {
	bu_log("Intersection failed\n");
	return -1;
    }

    bu_log("%d intersection segments.\n", curve.Count());
    if (curve.Count() == 0)
	return 0;

    // ON_SSX_EVENT cannot give us enough information on the intersections.
    /*
    ON_SSX_EVENT ssi_event;
    ssi_event.m_curve3d = curve[0];
    ssi_event.m_curveA = curve_uv[0];
    ssi_event.m_curveB = curve_st[0];
    ssi_event.m_type = ON_SSX_EVENT::TYPE::ssx_transverse;
    ON_wString wstr;
    ON_TextLog textlog(wstr);
    ssi_event.Dump(textlog);
    char *str = new char [wstr.Length() + 1];
    for (int k = 0; k < wstr.Length(); k++) {
	str[k] = wstr[k];
    }
    str[wstr.Length()] = 0;
    bu_log("%s", str);
    delete str; */

    char *intersect_name;
    if (argc == 7) {
	intersect_name = argv[6];
    } else {
	intersect_name = new char [strlen(argv[2]) + strlen(argv[3]) + 3];
	strcpy(intersect_name, argv[2]);
	strcat(intersect_name, "_");
	strcat(intersect_name, argv[3]);
	strcat(intersect_name, "_");
    }

    for (int i = 0; i < curve.Count(); i++) {
	// Do sampling, and use a pipe primitive to represent a curve
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_PIPE;
	intern.idb_meth = &rt_functab[ID_PIPE];
	BU_ALLOC(intern.idb_ptr, struct rt_pipe_internal);
	struct rt_pipe_internal *pi;
	pi = (struct rt_pipe_internal *)intern.idb_ptr;
	pi->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	pi->pipe_count = curve.Count();
	BU_LIST_INIT(&(pi->pipe_segs_head));
	struct wdb_pipept *ps;
	ON_Interval dom = curve[i]->Domain();
	// use different plotres for different accuracy (resolution)
	int plotres = 100;
	for (int j = 0; j <= plotres; j++) {
	    BU_ALLOC(ps, struct wdb_pipept);
	    ps->l.magic = WDB_PIPESEG_MAGIC;
	    ps->l.back = NULL;
	    ps->l.forw = NULL;
	    ON_3dPoint p = curve[i]->PointAt(dom.ParameterAt((double) j
							     / (double) plotres));
	    VSET(ps->pp_coord, p.x, p.y, p.z);
	    bu_log("%lf %lf %lf\n", p.x, p.y, p.z);
	    ps->pp_id = 50;
	    ps->pp_od = 100;
	    ps->pp_bendradius = 0;
	    BU_LIST_INSERT(&pi->pipe_segs_head, &ps->l);
	}

	char *name = new char [strlen(intersect_name) + 10];
	strcpy(name, intersect_name);
	char number[10];
	itoa(i, number, 10);
	strcat(name, number);

	struct bu_external ext;
	BU_CK_EXTERNAL(&ext);
	int flags = db_flags_internal(&intern);
	struct directory *dp;
	dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern.idb_type);
	int ret = rt_db_put_internal(dp, dbip, &intern, &rt_uniresource);
	if (ret)
	    bu_log("ERROR: failure writing [%s] to disk\n", name);
	else
	    bu_log("%s is written to file.\n", name);
    }

    // Free the memory.
    for (int i = 0; i < curve.Count(); i++)
	delete curve[i];
    for (int i = 0; i < curve_uv.Count(); i++)
	delete curve_uv[i];
    for (int i = 0; i < curve_st.Count(); i++)
	delete curve_st[i];

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
