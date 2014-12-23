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

	ON_Brep *brep;
};

object_data::object_data(int face_index, ON_Brep *in_brep)
    : negative_object(false)
{
    std::queue<int> local_loops;
    std::set<int> processed_loops;
    std::set<int>::iterator s_it;
    brep = in_brep;
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
	    if (trim.m_ei != -1 && edge.TrimCount() > 1) {
		edges.insert(trim.m_ei);
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
    for (s_it = faces.begin(); s_it != faces.end(); s_it++) {
	if (fol.find(*s_it) == fol.end()) {
	    fil.insert(*s_it);
	}
    }
}

object_data::~object_data()
{
}

int
is_arbn(const object_data *data)
{
    int ret = 1;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If all surfaces are planes, then the verdict is yes.
    std::set<int>::iterator f_it;
    for (f_it = data->faces.begin(); f_it != data->faces.end(); f_it++) {
	ON_Plane plane;
	ON_BrepFace *used_face = &(data->brep->m_F[(*f_it)]);
	ON_Surface *temp_surface = (ON_Surface *)used_face->SurfaceOf();
	if (!temp_surface->IsPlanar(&plane)) return 0;
    }

    // BRL-CAD's arbn primitive does not support concave shapes, and we want to use
    // simpler primitives than the generic nmg if the opportunity arises.  A heuristic
    // along the following lines may help:
    //
    // Step 1.  Check the number of vertices.  If the number is small enough that the
    //          volume might conceivably be expressed by an arb4-arb8, try that first.
    //
    // Step 2.  If the arb4-arb8 test fails, do the convex hull and see if all points
    //          are used by the hull.  If so, the shape is convex and may be expressed
    //          as an arbn.
    //
    // Step 3.  If the arbn test fails, construct sets of contiguous concave faces using
    //          the set of edges with one or more vertices not in the convex hull.  If
    //          those shapes are all convex, construct an arbn tree (or use simpler arb
    //          shapes if the subtractions may be so expressed).  If the subtraction
    //          volumes are still not convex, cut our losses and proceed to the nmg
    //          primitive.  It may conceivably be worth some additional searches to spot
    //          subsets of shapes that are convex, but that is not particularly simple
    //          and should wait until it is clear we would get a benefit from it.

    return ret;
}

int
is_cylinder(const object_data *data)
{
    int ret = 1;
    std::set<int>::iterator f_it;
    std::set<int> planar_surfaces;
    std::set<int> cylindrical_surfaces;
    std::set<int> active_edges;
    // First, check surfaces.  If a surface is anything other than a plane or cylindrical,
    // the verdict is no.  If we don't have at least two planar surfaces and one
    // cylindrical, the verict is no.
    for (f_it = data->faces.begin(); f_it != data->faces.end(); f_it++) {
	ON_BrepFace *used_face = &(data->brep->m_F[(*f_it)]);
	ON_Surface *temp_surface = (ON_Surface *)used_face->SurfaceOf();
	int surface_type = (int)GetSurfaceType(temp_surface);
	switch (surface_type) {
	    case SURFACE_PLANE:
		planar_surfaces.insert(*f_it);
		break;
	    case SURFACE_CYLINDER:
		cylindrical_surfaces.insert(*f_it);
		break;
	    default:
		return 0;
		break;
	}
    }
    if (planar_surfaces.size() < 2) return 0;
    if (cylindrical_surfaces.size() < 1) return 0;

    // Second, check if all cylindrical surfaces share the same axis.
    ON_Cylinder cylinder;
    data->brep->m_F[*cylindrical_surfaces.begin()].SurfaceOf()->IsCylinder(&cylinder);
    //std::cout << "cylinder: " << cylinder.Axis().x << "," << cylinder.Axis().y << "," << cylinder.Axis().z << "\n";
    for (f_it = cylindrical_surfaces.begin(); f_it != cylindrical_surfaces.end(); f_it++) {
	ON_Cylinder f_cylinder;
	data->brep->m_F[(*f_it)].SurfaceOf()->IsCylinder(&f_cylinder);
	//std::cout << "f_cylinder: " << f_cylinder.Axis().x << "," << f_cylinder.Axis().y << "," << f_cylinder.Axis().z << "\n";
	if (f_cylinder.Axis() != cylinder.Axis()) return 0;
    }

    // Third, see if all planes are coplanar with two and only two planes.
    ON_Plane p1, p2;
    int p2_set = 0;
    data->brep->m_F[*planar_surfaces.begin()].SurfaceOf()->IsPlanar(&p1);
    for (f_it = planar_surfaces.begin(); f_it != planar_surfaces.end(); f_it++) {
	ON_Plane f_p;
	data->brep->m_F[(*f_it)].SurfaceOf()->IsPlanar(&f_p);
	if (!p2_set && f_p != p1) {
	    p2 = f_p;
	    continue;
	};
	if (f_p != p1 && f_p != p2) return 0;
    }

    // Fourth, check that the two planes are parallel to each other.
    if (p1.Normal().IsParallelTo(p2.Normal()) == 0) return 0;

    // Fifth, remove degenerate edge sets. A degenerate edge set is defined as two
    // linear segments having the same two vertices.  (To be sure, we should probably
    // check curve directions in loops in some fashion...)
    std::set<int>::iterator e_it;
    std::set<int> degenerate;
    for (e_it = data->edges.begin(); e_it != data->edges.end(); e_it++) {
	if (degenerate.find(*e_it) == degenerate.end()) {
	    ON_BrepEdge& edge = data->brep->m_E[*e_it];
	    if (edge.EdgeCurveOf()->IsLinear()) {
		for (f_it = data->edges.begin(); f_it != data->edges.end(); f_it++) {
		    ON_BrepEdge& edge2 = data->brep->m_E[*f_it];
		    if (edge2.EdgeCurveOf()->IsLinear()) {
			if ((edge.Vertex(0)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(1)->Point() == edge2.Vertex(1)->Point()) ||
				(edge.Vertex(1)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(0)->Point() == edge2.Vertex(1)->Point()))
			{
			    degenerate.insert(*e_it);
			    degenerate.insert(*f_it);
			    break;
			}
		    }
		}
	    }
	}
    }
    active_edges = data->edges;
    for (e_it = degenerate.begin(); e_it != degenerate.end(); e_it++) {
	std::cout << "erasing " << *e_it << "\n";
	active_edges.erase(*e_it);
    }
    std::cout << "Active Edge set: ";
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
	std::cout << (int)(*e_it);
	f_it = e_it;
	f_it++;
	if (f_it != active_edges.end()) std::cout << ",";
    }
    std::cout << "\n";

    // Sixth, check for any remaining linear segments.  For rpc primitives
    // those are expected, but for a true cylinder the linear segments should
    // all wash out in the degenerate pass.
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
	ON_BrepEdge& edge = data->brep->m_E[*e_it];
	if (edge.EdgeCurveOf()->IsLinear()) return 0;
    }

    // Seventh, sort the curved edges into one of two circles.  Again, in more
    // general cases we might have other curves but a true cylinder should have
    // all of its arcs on these two circles.  We don't need to check for closed
    // loops because we are assuming that in the input Brep; any curve except
    // arc curves that survived the degeneracy test has already resulted in an
    // earlier rejection.
    std::set<int> arc_set_1, arc_set_2;
    ON_Circle set1_c, set2_c;
    int arc1_circle_set= 0;
    int arc2_circle_set = 0;
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
	ON_BrepEdge& edge = data->brep->m_E[*e_it];
	ON_Arc arc;
	if (edge.EdgeCurveOf()->IsArc(NULL, &arc, 0.01)) {
	    int assigned = 0;
	    ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
	    //std::cout << "circ " << circ.Center().x << " " << circ.Center().y << " " << circ.Center().z << "\n";
	    if (!arc1_circle_set) {
		arc1_circle_set = 1;
		set1_c = circ;
		//std::cout << "center 1 " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << "\n";
	    } else {
		if (!arc2_circle_set) {
		    if (!(NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), 0.01))){
			arc2_circle_set = 1;
			set2_c = circ;
			//std::cout << "center 2 " << set2_c.Center().x << " " << set2_c.Center().y << " " << set2_c.Center().z << "\n";
		    }
		}
	    }
	    if (NEAR_ZERO(circ.Center().DistanceTo(set1_c.Center()), 0.01)){
		arc_set_1.insert(*e_it);
		assigned = 1;
	    }
	    if (arc2_circle_set) {
		if (NEAR_ZERO(circ.Center().DistanceTo(set2_c.Center()), 0.01)){
		    arc_set_2.insert(*e_it);
		    assigned = 1;
		}
	    }
	    if (!assigned) {
		std::cout << "found extra circle - no go\n";
		return 0;
	    }
	}
    }
    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

    std::cout << "rcc: in " << data->key.c_str() << " rcc " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << " " << hvect.x << " " << hvect.y << " " << hvect.z << " " << set1_c.Radius() << "\n";


    // TODO - check for different radius values between the two circles - for a pure cylinder test should reject, but we can easily handle it with the tgc...

    return ret;
}


void
print_objects(std::set<object_data> *object_set)
{
    int cnt = 0;
    std::set<object_data>::iterator o_it;
    for (o_it = object_set->begin(); o_it != object_set->end(); o_it++) {
	std::set<int>::iterator s_it;
	std::set<int>::iterator s_it2;
	std::cout << "\n";
	std::cout << "Face set for object " << cnt << ": ";
	for (s_it = (*o_it).faces.begin(); s_it != (*o_it).faces.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*o_it).faces.end()) std::cout << ",";
	}
	std::cout << "\n";
	std::cout << "Outer Face set for object " << cnt << ": ";
	for (s_it = (*o_it).fol.begin(); s_it != (*o_it).fol.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*o_it).fol.end()) std::cout << ",";
	}
	std::cout << "\n";
	std::cout << "Inner Face set for object " << cnt << ": ";
	for (s_it = (*o_it).fil.begin(); s_it != (*o_it).fil.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*o_it).fil.end()) std::cout << ",";
	}
	std::cout << "\n";
	std::cout << "Edge set for object " << cnt << ": ";
	for (s_it = (*o_it).edges.begin(); s_it != (*o_it).edges.end(); s_it++) {
	    std::cout << (int)(*s_it);
	    s_it2 = s_it;
	    s_it2++;
	    if (s_it2 != (*o_it).edges.end()) std::cout << ",";
	}
	std::cout << "\n";
	if (is_arbn(&(*o_it))) std::cout << "Object is arbn\n";
	if (is_cylinder(&(*o_it))) std::cout << "Object is rcc\n";
	cnt++;
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
