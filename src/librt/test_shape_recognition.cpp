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

    for (int i = 0; i < brep->m_S.Count(); i++) {
	int surface_type = (int)GetSurfaceType(brep->m_S[i]);
	if (surface_type != 1 && surface_type != 6)
	std::cout << "Surface type: " << GetSurfaceType(brep->m_S[i]) << "\n";
    }
#endif
#if 0
    for (int i = 0; i < brep->m_F.Count(); i++) {
	ON_BrepFace *face = &(brep->m_F[i]);
	ON_Surface *temp_surface = (ON_Surface *)face->SurfaceOf();
	int surface_type = (int)GetSurfaceType(temp_surface);
	std::cout << "Surface type: " << surface_type << "\n";
	for (int li = 0; li < face->LoopCount(); li++) {
	    bool innerLoop = (li > 0) ? true : false;
	    ON_BrepLoop* loop = face->Loop(li);
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
		ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		//const ON_Curve* trimCurve = trim.TrimCurveOf();
		const ON_Curve* edgeCurve = edge.EdgeCurveOf();
		int curve_type = (int)GetCurveType((ON_Curve *)edgeCurve);
		if (innerLoop) {
		std::cout << "  Inner Curve type: " << curve_type << "\n";
		} else {
		std::cout << "  Outer Curve type: " << curve_type << "\n";
		}
	    }
	}
    }
#endif

    std::map<int, std::set<int> > face_sets;
    std::map<int, std::set<int> > loop_sets;

    for (int i = 0; i < brep->m_F.Count(); i++) {
	ON_BrepFace *face = &(brep->m_F[i]);
	face_sets[i].insert(i);
	loop_sets[i].insert(face->OuterLoop()->m_loop_index);
	ON_BrepLoop* loop = face->OuterLoop();
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
	    ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
	    if (edge.TrimCount() > 1) {
		for (int j = 0; j < edge.TrimCount(); j++) {
		    if (edge.m_ti[j] != ti && edge.Trim(j)->FaceIndexOf()!= -1) {
			face_sets[i].insert(edge.Trim(j)->FaceIndexOf());
			loop_sets[i].insert(edge.Trim(j)->Loop()->m_loop_index);
			std::cout << "Face " << i <<  ": shares loop " << edge.Trim(j)->Loop()->m_loop_index << " with face " << edge.Trim(j)->FaceIndexOf() << "\n";
		    }
		}
	    }
	}
    }
    for (int i = 0; i < brep->m_F.Count(); i++) {
	std::set<int>::iterator s_it;
	std::set<int>::iterator s_it2;
	ON_BrepFace *face = &(brep->m_F[i]);
	std::cout << "Face set for face " << i << ": ";
	for (s_it = face_sets[i].begin(); s_it != face_sets[i].end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != face_sets[i].end()) std::cout << ",";
	}
	std::cout << "\n";
	for (s_it = face_sets[i].begin(); s_it != face_sets[i].end(); s_it++) {
	    ON_BrepFace *used_face = &(brep->m_F[(*s_it)]);
	    ON_Surface *temp_surface = (ON_Surface *)used_face->SurfaceOf();
	    int surface_type = (int)GetSurfaceType(temp_surface);
	    std::cout << "Face " << (*s_it) << " surface type: " << surface_type << "\n";
	}
	for (s_it = loop_sets[i].begin(); s_it != loop_sets[i].end(); s_it++) {
	    ON_BrepLoop* loop = &(brep->m_L[(*s_it)]);
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
		ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		if (edge.TrimCount() > 1) {
		    for (int j = 0; j < edge.TrimCount(); j++) {
			if (edge.m_ti[j] != ti) {
			    const ON_Curve* edgeCurve = edge.EdgeCurveOf();
			    int curve_type = (int)GetCurveType((ON_Curve *)edgeCurve);
			    std::cout << "Loop " << (*s_it) << " curve type: " << curve_type << "\n";
			}
		    }
		}
	    }
	}
	std::cout << "\n";
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
