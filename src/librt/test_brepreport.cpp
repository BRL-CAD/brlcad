/*              T E S T _ B R E P R E P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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

#include <set>
#include <map>

#include "vmath.h"
#include "raytrace.h"
#include "brep.h"
#include "../libbrep/shape_recognition.h"

struct type_counts {
    int planar;
    int spherical;
    int cylindrical;
    int cone;
    int torus;
    int general;
};

/* 0 - no general surfaces, 1 ratio of general surfaces to other types is < 1/10, 2 other */
int
characterize_faces(struct type_counts *cnts, const char *name, ON_Brep *brep)
{
    int planar = 0;
    int spherical = 0;
    int cylindrical = 0;
    int cone = 0;
    int torus = 0;
    int general = 0;
    int hof = -1;
    surface_t hofo = SURFACE_PLANE;
    double ratio;
    int status = 0;
    struct bu_vls status_msg = BU_VLS_INIT_ZERO;

    for (int i = 0; i < brep->m_F.Count(); i++) {
	ON_Surface *s = brep->m_F[i].SurfaceOf()->Duplicate();
	int surface_type = (int)GetSurfaceType(s);
        switch (surface_type) {
            case SURFACE_PLANE:
                planar++;
                if (hofo < SURFACE_PLANE) {
                    hof = i;
                    hofo = SURFACE_PLANE;
                }
                break;
            case SURFACE_SPHERE:
                spherical++;
                if (hofo < SURFACE_SPHERE) {
                    hof = i;
                    hofo = SURFACE_SPHERE;
                }
                break;
            case SURFACE_CYLINDER:
                cylindrical++;
                if (hofo < SURFACE_CYLINDER) {
                    hof = i;
                    hofo = SURFACE_CYLINDER;
                }
                break;
            case SURFACE_CONE:
                cone++;
                if (hofo < SURFACE_CONE) {
                    hof = i;
                    hofo = SURFACE_CONE;
                }
                break;
            case SURFACE_TORUS:
                torus++;
                if (hofo < SURFACE_TORUS) {
                    hof = i;
                    hofo = SURFACE_TORUS;
                }
                break;
            default:
                general++;
                hofo = SURFACE_GENERAL;
                break;
        }
	delete s;
    }


    cnts->planar += planar;
    cnts->spherical += spherical;
    cnts->cylindrical += cylindrical;
    cnts->cone += cone;
    cnts->torus += torus;
    cnts->general += general;

    if (hofo != SURFACE_GENERAL) {
	status = 0;
    } else {
	if ((planar + spherical + cylindrical + cone + torus) == 0) status = 3;

	ratio = (double)general/(double)(planar + spherical + cylindrical + cone + torus);

	status = (ratio < .25) ? 1 : 2;
	if (ratio > .5) status = 3;
    }
    switch (status) {
	case 0:
	    bu_vls_sprintf(&status_msg, "OK");
	    break;
	case 1:
	    bu_vls_sprintf(&status_msg, "Partial");
	    break;
	case 2:
	    bu_vls_sprintf(&status_msg, "Maybe");
	    break;
	case 3:
	    bu_vls_sprintf(&status_msg, "NO");
	    break;
    }

    bu_log("%40s\t%5d\t%5d\t%5d\t%5d\t%5d\t%5d\t%5s\n", name, planar, spherical, cylindrical, cone, torus, general, bu_vls_addr(&status_msg));
    bu_vls_free(&status_msg);
    return status;
}




int
main(int argc, char *argv[])
{
    size_t i;
    struct db_i *dbip;
    struct directory *dp;
    struct rt_wdb *wdbp;
    int brep_cnt = 0;
    int convertable = 0;
    int maybeconvertable = 0;
    int unlikelyconvertable = 0;
    int nonconvertable = 0;
    struct type_counts cnts;

    bu_setprogname(argv[0]);

    cnts.planar = 0;
    cnts.spherical = 0;
    cnts.cylindrical = 0;
    cnts.cone = 0;
    cnts.torus = 0;
    cnts.general = 0;

    struct bu_ptbl unique_breps = BU_PTBL_INIT_ZERO;
    struct bu_ptbl full_path_breps = BU_PTBL_INIT_ZERO;


    if (argc != 2) {
	bu_exit(1, "Usage: %s file.g", argv[0]);
    }

    dbip = db_open(argv[1], DB_OPEN_READONLY);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    db_update_nref(dbip, &rt_uniresource);

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);


    brep_cnt = db_search(&unique_breps, DB_SEARCH_RETURN_UNIQ_DP, "-type brep", 0, NULL, dbip);

    bu_log("brep object count: %d\n", brep_cnt);
    /*
       brep_path_cnt = db_search(&full_path_breps, DB_SEARCH_TREE, "-type brep", 0, NULL, dbip);

       bu_log("brep path count: %d\n", brep_path_cnt);
    */

    bu_log("%40s\t%5s\t%5s\t%5s\t%5s\t%5s\t%5s\t%s\n", "Totals", "Plane", "Sph", "Cyl", "Cone", "Torus", "NURB", "Convertable");
    int valid_breps = 0;
    for (i = 0; i < BU_PTBL_LEN(&unique_breps); i++) {
	struct rt_db_internal intern;
	struct rt_brep_internal* bi;
	ON_Brep *brep;
	RT_DB_INTERNAL_INIT(&intern);
	dp = (struct directory *)BU_PTBL_GET(&unique_breps, i);
	if (rt_db_get_internal(&intern, dp, dbip, bn_mat_identity, &rt_uniresource) < 0) {
	    bu_log("Error getting internal: %s\n", dp->d_namep);
	    continue;
	}
	RT_CK_DB_INTERNAL(&intern);
	if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	    bu_log("%s is not a B-Rep, skipping\n", dp->d_namep);
	    continue;
	}
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	brep = bi->brep;
	int have_boundary = 0;
	for (int j = 0; j < brep->m_T.Count(); j++) {
	    if (brep->m_T[j].m_type == ON_BrepTrim::boundary) {
		have_boundary = 1;
		break;
	    }
	}
	if (have_boundary) continue;
	if (!brep->IsValid()) continue;
	valid_breps++;

	int cf = characterize_faces(&cnts, dp->d_namep, brep);

	switch (cf) {
	    case 3:
		nonconvertable++;
		break;
	    case 2:
		unlikelyconvertable++;
		break;

	    case 1:
		maybeconvertable++;
		break;
	    case 0:
		convertable++;
		break;
	}
    }

    bu_log("%40s\t%5s\t%5s\t%5s\t%5s\t%5s\t%5s\n", "Objects", "Plane", "Sph", "Cyl", "Cone", "Torus", "NURB");
    bu_log("%40s\t%5d\t%5d\t%5d\t%5d\t%5d\t%5d\n", " ", cnts.planar, cnts.spherical, cnts.cylindrical, cnts.cone, cnts.torus, cnts.general);
    bu_log("Convertable: %d of %d (%.1f percent)\n", convertable, valid_breps, 100*(double)convertable/(double)valid_breps);
    bu_log("Maybe convertable: %d of %d (%.1f percent)\n", maybeconvertable, valid_breps, 100*(double)maybeconvertable/(double)valid_breps);
    bu_log("Unlikely convertable: %d of %d (%.1f percent)\n", unlikelyconvertable, valid_breps, 100*(double)unlikelyconvertable/(double)valid_breps);
    bu_log("Non-convertable: %d of %d (%.1f percent)\n", nonconvertable, valid_breps, 100*(double)nonconvertable/(double)valid_breps);
#if 0



    RT_DB_INTERNAL_INIT(&intern)
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
    }

    if (intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
	bu_exit(1, "ERROR: object %s does not appear to be of type BoT\n", argv[2]);
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(brep_ip);
    }

#endif
    db_search_free(&unique_breps);
    db_search_free(&full_path_breps);
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
