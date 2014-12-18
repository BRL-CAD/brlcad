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

std::string
face_set_key(std::set<int> fset)
{
    std::set<int>::iterator s_it;
    std::set<int>::iterator s_it2;
    std::string key;
    struct bu_vls vls_key = BU_VLS_INIT_ZERO;
    for (s_it = fset.begin(); s_it != fset.end(); s_it++) {
	bu_vls_printf(&vls_key, "%d", (*s_it));
	s_it2 = s_it;
	s_it2++;
	if (s_it2 != fset.end()) bu_vls_printf(&vls_key, "_");
    }
    bu_vls_printf(&vls_key, ".s");
    key.append(bu_vls_addr(&vls_key));
    bu_vls_free(&vls_key);
    return key;
}

void
build_face_sets(std::map<int, std::set<int> > *face_sets, ON_Brep *brep)
{
    for (int i = 0; i < brep->m_F.Count(); i++) {
	std::queue<int> local_loops;
	ON_BrepFace *face = &(brep->m_F[i]);
	(*face_sets)[i].insert(i);
	local_loops.push(face->OuterLoop()->m_loop_index);
	while(!local_loops.empty()) {
	    ON_BrepLoop* loop = &(brep->m_L[local_loops.front()]);
	    local_loops.pop();
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
		ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		if (edge.TrimCount() > 1) {
		    for (int j = 0; j < edge.TrimCount(); j++) {
			if (edge.m_ti[j] != ti && edge.Trim(j)->FaceIndexOf()!= -1) {
			    (*face_sets)[i].insert(edge.Trim(j)->FaceIndexOf());
			}
		    }
		}
	    }
	}
    }
}

void
build_object_face_sets(std::map<std::string, std::set<int> > *face_sets, ON_Brep *brep)
{
    std::map<int, std::set<int> > int_face_sets;
    std::set<int>::iterator s_it;
    build_face_sets(&int_face_sets, brep);
    for (int i = 0; i < brep->m_F.Count(); i++) {
	struct bu_vls key = BU_VLS_INIT_ZERO;
	for (s_it = int_face_sets[i].begin(); s_it != int_face_sets[i].end(); s_it++) {
	    bu_vls_printf(&key, "%d", (*s_it));
	}
	//bu_log("key: %s\n", bu_vls_addr(&key));
	(*face_sets).insert(std::make_pair(face_set_key(int_face_sets[i]), int_face_sets[i]));
	bu_vls_free(&key);
    }
}


void
build_loop_sets(std::map<int, std::set<int> > *loop_sets, ON_Brep *brep)
{
    for (int i = 0; i < brep->m_F.Count(); i++) {
	std::queue<int> local_loops;
	std::set<int> processed_loops;
	ON_BrepFace *face = &(brep->m_F[i]);
	local_loops.push(face->OuterLoop()->m_loop_index);
	processed_loops.insert(face->OuterLoop()->m_loop_index);
	//std::cout << "Face " << i <<  " outer loop " << face->OuterLoop()->m_loop_index << "\n";
	while(!local_loops.empty()) {
	    (*loop_sets)[i].insert(local_loops.front());
	    ON_BrepLoop* loop = &(brep->m_L[local_loops.front()]);
	    local_loops.pop();
	    for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
		ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
		ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		if (edge.TrimCount() > 1) {
		    for (int j = 0; j < edge.TrimCount(); j++) {
			if (edge.m_ti[j] != ti && edge.Trim(j)->FaceIndexOf()!= -1) {
			    if (processed_loops.find(edge.Trim(j)->Loop()->m_loop_index) == processed_loops.end()) {
				local_loops.push(edge.Trim(j)->Loop()->m_loop_index);
				processed_loops.insert(edge.Trim(j)->Loop()->m_loop_index);
				//std::cout << "Face " << i <<  " network pulls in loop " << edge.Trim(j)->Loop()->m_loop_index << " in face " << edge.Trim(j)->FaceIndexOf() << "\n";
			    }
			}
		    }
		}
	    }
	}
    }
}

void
print_face_set(std::map<int, std::set<int> > *face_sets, int i)
{
    std::set<int>::iterator s_it;
    std::set<int>::iterator s_it2;
    std::cout << "Face set for face " << i << ": ";
    for (s_it = (*face_sets)[i].begin(); s_it != (*face_sets)[i].end(); s_it++) {
	std::cout << (int)(*s_it);
	s_it2 = s_it;
	s_it2++;
	if (s_it2 != (*face_sets)[i].end()) std::cout << ",";
    }
    std::cout << "\n";
}

void
print_object_face_sets(std::map<std::string, std::set<int> > *face_sets)
{
    std::map<std::string, std::set<int> >::iterator f_it;
    for (f_it = face_sets->begin(); f_it != face_sets->end(); f_it++) {
	std::set<int>::iterator s_it;
	std::set<int>::iterator s_it2;
	std::cout << "Face set for object " << (*f_it).first.c_str() << ": ";
	for (s_it = (*f_it).second.begin(); s_it != (*f_it).second.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*f_it).second.end()) std::cout << ",";
	}
	std::cout << "\n";
    }
}

void
print_face_set_surface_types(std::map<int, std::set<int> > *face_sets, ON_Brep *brep, int i)
{
    std::set<int>::iterator s_it;
    std::cout << "Surface types for face set " << i << ": \n";
    for (s_it = (*face_sets)[i].begin(); s_it != (*face_sets)[i].end(); s_it++) {
	ON_BrepFace *used_face = &(brep->m_F[(*s_it)]);
	ON_Surface *temp_surface = (ON_Surface *)used_face->SurfaceOf();
	int surface_type = (int)GetSurfaceType(temp_surface);
	std::cout << "  Face " << (*s_it) << " surface type: " << surface_type << "\n";
    }
}

void
build_edge_set(std::set<std::pair<int, int> > *edge_set, std::map<int, std::set<int> > *loop_sets, ON_Brep *brep, int i)
{
    std::set<int>::iterator s_it;
    ON_BrepFace *face = &(brep->m_F[i]);
    for (s_it = (*loop_sets)[i].begin(); s_it != (*loop_sets)[i].end(); s_it++) {
	ON_BrepLoop* loop = &(brep->m_L[(*s_it)]);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
	    ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
	    if (edge.TrimCount() > 1) {
		for (int j = 0; j < edge.TrimCount(); j++) {
		    if (edge.m_ti[j] != ti) {
			(*edge_set).insert(std::make_pair(loop->Face()->m_face_index, edge.m_edge_index));
		    }
		}
	    }
	}
    }
}

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

    std::map<int, std::set<int> > face_sets;
    std::map<std::string, std::set<int> > object_face_sets;
    std::map<int, std::set<int> > loop_sets;
    build_face_sets(&face_sets, brep);
    build_object_face_sets(&object_face_sets, brep);
    build_loop_sets(&loop_sets, brep);

    ///*
    for (int i = 0; i < brep->m_F.Count(); i++) {
	print_face_set(&face_sets, i);
	print_face_set_surface_types(&face_sets, brep, i);
    }
    //*/
    print_object_face_sets(&object_face_sets);
#if 0
    for (int i = 0; i < brep->m_F.Count(); i++) {
	std::set<std::pair<int, int> > edge_set;
	std::set<std::pair<int, int> >::iterator c_it;
	build_edge_set(&edge_set, &loop_sets, brep, i);
	for (c_it = edge_set.begin(); c_it != edge_set.end(); c_it++) {
	    ON_Curve *curve = (ON_Curve *)(brep->m_C3[brep->m_E[(*c_it).second].EdgeCurveIndexOf()]);
	    int curve_type = (int)GetCurveType(curve);
	    std::cout << "  Curve " << (*c_it).second << " type: " << curve_type << " in face " << (*c_it).first << "\n";
	    if (curve_type == CURVE_ARC) {
		ON_Arc arc;
		(void)curve->IsArc(NULL, &arc, 0.01);
		ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
		bu_log("    arc's circle: center %0.2f, %0.2f, %0.2f   radius %f\n", circ.Center().x, circ.Center().y, circ.Center().z, circ.Radius());
	    }
	}
	std::cout << "\n";
    }
#endif

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
