#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"
#include "opennurbs.h"
#include "brep.h"
#include "../libbrep/shape_recognition.h"

int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;
    struct rt_wdb *wdbp;

    if (argc != 3) {
	bu_exit(1, "Usage: %s file.g object", argv[0]);
    }

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
        bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
    }

    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_exit(1, "ERROR: object %s does not appear to be of type BRep\n", argv[2]);
    } else {
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
    }
    RT_BREP_CK_MAGIC(brep_ip);

    ON_Brep *brep = brep_ip->brep;

#if 0
    for (int i = 0; i < brep->m_C3.Count(); i++) {
	int curve_type = (int)GetCurveType(brep->m_C3[i]);
	if (curve_type != 1 && curve_type != 6)
	std::cout << "Curve type: " << GetCurveType(brep->m_C3[i]) << "\n";
    }
#endif

    for (int i = 0; i < brep->m_S.Count(); i++) {
	int surface_type = (int)GetSurfaceType(brep->m_S[i]);
	if (surface_type != 1 && surface_type != 6)
	std::cout << "Surface type: " << GetSurfaceType(brep->m_S[i]) << "\n";
    }

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
