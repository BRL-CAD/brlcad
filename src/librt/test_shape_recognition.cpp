#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

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

class object_data {
    public:
	object_data(int face, ON_Brep *brep);
	~object_data();

	bool operator=(const object_data &a);
	bool operator<(const object_data &a);

	std::string key;
	int negative_object; /* Concave (1) or convex (0)? */
	std::set<int> faces;
	std::set<int> loops;
	std::set<int> edges;
	std::set<int> fol; /* Faces with outer loops in object loop network */
	std::set<int> fil; /* Faces with only inner loops in object loop network */

	bool operator<(const object_data &a) const
	{
	    return key < a.key;
	}

	bool operator=(const object_data &a) const
	{
	    return key == a.key;
	}
};

object_data::object_data(int face_index, ON_Brep *brep)
    : negative_object(false)
{
    std::queue<int> local_loops;
    std::set<int> processed_loops;
    ON_BrepFace *face = &(brep->m_F[face_index]);
    faces.insert(face_index);
    fol.insert(face_index);
    local_loops.push(face->OuterLoop()->m_loop_index);
    processed_loops.insert(face->OuterLoop()->m_loop_index);
    while(!local_loops.empty()) {
	ON_BrepLoop* loop = &(brep->m_L[local_loops.front()]);
	loops.insert(local_loops.front());
	local_loops.pop();
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];
	    ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
	    edges.insert(trim.m_ei);
	    if (edge.TrimCount() > 1) {
		for (int j = 0; j < edge.TrimCount(); j++) {
		    int fio = edge.Trim(j)->FaceIndexOf();
		    if (edge.m_ti[j] != ti && fio != -1) {
			int li = edge.Trim(j)->Loop()->m_loop_index;
			faces.insert(fio);
			if (processed_loops.find(li) == processed_loops.end()) {
			    local_loops.push(li);
			    processed_loops.insert(li);
			}
			if (li == brep->m_F[fio].OuterLoop()->m_loop_index) {
			    fol.insert(fio);
			}
		    }
		}
	    }
	}
    }
    key = face_set_key(faces);
}

object_data::~object_data()
{
}

void
print_objects(std::set<object_data> *object_set)
{
    std::set<object_data>::iterator o_it;
    for (o_it = object_set->begin(); o_it != object_set->end(); o_it++) {
	std::set<int>::iterator s_it;
	std::set<int>::iterator s_it2;
	std::cout << "Face set for object " << (*o_it).key.c_str() << ": ";
	for (s_it = (*o_it).faces.begin(); s_it != (*o_it).faces.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*o_it).faces.end()) std::cout << ",";
	}
	std::cout << "\n";
	std::cout << "Outer Face set for object " << (*o_it).key.c_str() << ": ";
	for (s_it = (*o_it).fol.begin(); s_it != (*o_it).fol.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*o_it).fol.end()) std::cout << ",";
	}
	std::cout << "\n";
	std::cout << "Edge set for object " << (*o_it).key.c_str() << ": ";
	for (s_it = (*o_it).edges.begin(); s_it != (*o_it).edges.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*o_it).edges.end()) std::cout << ",";
	}

	std::cout << "\n";
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

    std::set<object_data> object_set;
    for (int i = 0; i < brep->m_F.Count(); i++) {
	object_data face_object(i, brep);
	object_set.insert(face_object);
    }


    print_objects(&object_set);

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
