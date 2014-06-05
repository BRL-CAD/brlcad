#include "common.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>
#include <iomanip>

#include "brep.h"
#include "raytrace.h"


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
nurbs_test(long int test_number, struct db_i *dbip)
{
    /* Have various specific surface structures - there are potentially
     * multiple surfaces in the .g, but multiple tests may re-use a single
     * surface.*/

    /* openNURBS pointers */
    ON_Brep *brep = NULL;

    switch (test_number)
    {
	case 1: /* 3d -> 2d pull-back function get_closest_point */
	    {
		struct rt_db_internal case_1_intern;
		struct rt_brep_internal *case_1_brep_ip = NULL;
		RT_DB_INTERNAL_INIT(&case_1_intern);
		if (get_surface("case_1_surface.s", dbip, &case_1_intern, &case_1_brep_ip)) return -1;
		brep = case_1_brep_ip->brep;
		ON_BrepFace& c1_face = brep->m_F[0];

		ON_2dPoint pt_2d_1, pt_2d_2;
		ON_3dPoint pt_3d_1(11204.05366897583,-16726.489562988281, 3358.7263298034668);
		ON_3dPoint pt_3d_2(11204.007682800293, -16726.479568481445, 3358.8327312469482);
		ON_3dPoint p3d_pullback_1 = ON_3dPoint::UnsetPoint;
		ON_3dPoint p3d_pullback_2 = ON_3dPoint::UnsetPoint;
		bool p1_result = surface_GetClosestPoint3dFirstOrder(c1_face.SurfaceOf(),pt_3d_1,pt_2d_1,p3d_pullback_1,0,BREP_EDGE_MISS_TOLERANCE);
		bool p2_result = surface_GetClosestPoint3dFirstOrder(c1_face.SurfaceOf(),pt_3d_2,pt_2d_2,p3d_pullback_2,0,BREP_EDGE_MISS_TOLERANCE);
	        rt_db_free_internal(&case_1_intern);
                if (pt_2d_1 == pt_2d_2) {
		    std::cout << "NURBS test case 1 failure (surface_GetClosestPoint3dFirstOrder):  Unexpectedly identical 2D pullbacks from different 3D points\n";
		    std::cout << std::setprecision(std::numeric_limits<double>::digits10) << "Inputs:  3d Point 1: " << pt_3d_1.x << "," << pt_3d_1.y << "," << pt_3d_1.z << "\n";
		    std::cout << std::setprecision(std::numeric_limits<double>::digits10) << "         3d Point 2: " << pt_3d_2.x << "," << pt_3d_2.y << "," << pt_3d_2.z << "\n";
		    std::cout << std::setprecision(std::numeric_limits<double>::digits10) << "Output:  2d Point 1: " << pt_2d_1.x << "," << pt_2d_1.y << "\n";
		    std::cout << std::setprecision(std::numeric_limits<double>::digits10) << "         2d Point 2: " << pt_2d_2.x << "," << pt_2d_2.y << "\n";
		    return -1;
		}
		if (p1_result == false) std::cout << "Warning - p1 pullback failed\n";
		if (p2_result == false) std::cout << "Warning - p2 pullback failed\n";
                return 0;
	    }
	default:
	    return -1;
    }

}


int
main(int argc, char **argv)
{
    long int test_number = 0;
    int retval = 0;
    long int all_tests = 1;
    struct db_i *dbip;
    char *endptr;

    /* Set up the .g file */
    if (argc > 3 || argc < 2) {
	bu_log("Usage: %s file.g [test_number]", argv[0]);
	return -1;
    }

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_log("ERROR: Unable to read from geometry database file %s\n", argv[1]);
	return -1;
    }

    if (db_dirbuild(dbip) < 0) {
	bu_log("ERROR: Unable to read from %s\n", argv[1]);
	return -1;
    }

    if (argc == 3) {
	test_number = strtol(argv[2], &endptr, 10);
	if (*endptr) return -1;
    }

    if (!test_number) {
	int current_test = 1;
	while (current_test <= all_tests && retval != -1) {
	    retval = nurbs_test(current_test, dbip);
	    current_test++;
	}
    } else {
	retval = nurbs_test(test_number, dbip);
    }
    return retval;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
