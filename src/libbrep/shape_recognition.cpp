#include "common.h"

#include <set>

#include "bu/log.h"
#include "bu/malloc.h"
#include "shape_recognition.h"

curve_t
GetCurveType(ON_Curve *curve)
{
    /* First, see if the curve is planar */
/*    if (!curve->IsPlanar()) {
	return CURVE_GENERAL;
    }*/

    /* Check types in order of increasing complexity - simple
     * is better, so try simple first */
    ON_BoundingBox c_bbox;
    curve->GetTightBoundingBox(c_bbox);
    if (c_bbox.IsDegenerate() == 3) return CURVE_POINT;

    if (curve->IsLinear()) return CURVE_LINE;

    ON_Arc arc;
    if (curve->IsArc(NULL, &arc, 0.01)) {
	if (arc.IsCircle()) return CURVE_CIRCLE;
	ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
	bu_log("arc's circle: center %f, %f, %f   radius %f\n", circ.Center().x, circ.Center().y, circ.Center().z, circ.Radius());
       	return CURVE_ARC;
    }

    // TODO - looks like we need a better test for this...
    if (curve->IsEllipse(NULL, NULL, 0.01)) return CURVE_ELLIPSE;

    return CURVE_GENERAL;
}

surface_t
GetSurfaceType(ON_Surface *surface)
{
    if (surface->IsPlanar(NULL, 0.01)) return SURFACE_PLANE;
    if (surface->IsSphere(NULL, 0.01)) return SURFACE_SPHERE;
    if (surface->IsCylinder(NULL, 0.01)) return SURFACE_CYLINDER;
    if (surface->IsCone(NULL, 0.01)) return SURFACE_CONE;
    if (surface->IsTorus(NULL, 0.01)) return SURFACE_TORUS;
    return SURFACE_GENERAL;
}


int
subbrep_is_planar(struct subbrep_object_data *data)
{
    int i = 0;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If all surfaces are planes, then the verdict is yes.
    for (i = 0; i < data->faces_cnt; i++) {
	ON_Plane plane;
	ON_BrepFace *used_face = &(data->brep->m_F[data->faces[i]]);
	ON_Surface *temp_surface = (ON_Surface *)used_face->SurfaceOf();
	if (!temp_surface->IsPlanar(&plane)) return 0;
    }
    data->type = PLANAR_VOLUME;
    return 1;
}


int
subbrep_is_cylinder(struct subbrep_object_data *data)
{
    std::set<int>::iterator f_it;
    std::set<int> planar_surfaces;
    std::set<int> cylindrical_surfaces;
    std::set<int> active_edges;
    // First, check surfaces.  If a surface is anything other than a plane or cylindrical,
    // the verdict is no.  If we don't have at least two planar surfaces and one
    // cylindrical, the verdict is no.
    for (int i = 0; i < data->faces_cnt; i++) {
	int f_ind = data->faces[i];
	ON_BrepFace *used_face = &(data->brep->m_F[f_ind]);
        ON_Surface *temp_surface = (ON_Surface *)used_face->SurfaceOf();
        int surface_type = (int)GetSurfaceType(temp_surface);
        switch (surface_type) {
            case SURFACE_PLANE:
                planar_surfaces.insert(f_ind);
                break;
            case SURFACE_CYLINDER:
                cylindrical_surfaces.insert(f_ind);
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
    for (f_it = cylindrical_surfaces.begin(); f_it != cylindrical_surfaces.end(); f_it++) {
        ON_Cylinder f_cylinder;
        data->brep->m_F[(*f_it)].SurfaceOf()->IsCylinder(&f_cylinder);
        ON_3dPoint fca = f_cylinder.Axis();
        ON_3dPoint ca = cylinder.Axis();
        if (fca.DistanceTo(ca) > 0.01) return 0;
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
    if (p1.Normal().IsParallelTo(p2.Normal(), 0.01) == 0) {
        std::cout << "p1 Normal: " << p1.Normal().x << "," << p1.Normal().y << "," << p1.Normal().z << "\n";
        std::cout << "p2 Normal: " << p2.Normal().x << "," << p2.Normal().y << "," << p2.Normal().z << "\n";
        return 0;
    }

    // Fifth, remove degenerate edge sets. A degenerate edge set is defined as two
    // linear segments having the same two vertices.  (To be sure, we should probably
    // check curve directions in loops in some fashion...)
    std::set<int> degenerate;
    for (int i = 0; i < data->edges_cnt; i++) {
	int e_it = data->edges[i];
	if (degenerate.find(e_it) == degenerate.end()) {
	    ON_BrepEdge& edge = data->brep->m_E[e_it];
	    if (edge.EdgeCurveOf()->IsLinear()) {
		for (int j = 0; j < data->edges_cnt; j++) {
		    int f_ind = data->edges[j];
		    ON_BrepEdge& edge2 = data->brep->m_E[f_ind];
		    if (edge2.EdgeCurveOf()->IsLinear()) {
			if ((edge.Vertex(0)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(1)->Point() == edge2.Vertex(1)->Point()) ||
				(edge.Vertex(1)->Point() == edge2.Vertex(0)->Point() && edge.Vertex(0)->Point() == edge2.Vertex(1)->Point()))
			{
			    degenerate.insert(e_it);
			    degenerate.insert(f_ind);
			    break;
			}
		    }
		}
	    }
	}
    }
    for (int i = 0; i < data->edges_cnt; i++) {
	active_edges.insert(data->edges[i]);
    }
    std::set<int>::iterator e_it;
    for (e_it = degenerate.begin(); e_it != degenerate.end(); e_it++) {
        //std::cout << "erasing " << *e_it << "\n";
        active_edges.erase(*e_it);
    }
    //std::cout << "Active Edge set: ";
#if 0
    for (e_it = active_edges.begin(); e_it != active_edges.end(); e_it++) {
        std::cout << (int)(*e_it);
        f_it = e_it;
        f_it++;
        if (f_it != active_edges.end()) std::cout << ",";
    }
    std::cout << "\n";
#endif

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

    data->type = CYLINDER;

    ON_3dVector hvect(set2_c.Center() - set1_c.Center());

    data->params->origin[0] = set1_c.Center().x;
    data->params->origin[1] = set1_c.Center().y;
    data->params->origin[2] = set1_c.Center().z;
    data->params->hv[0] = hvect.x;
    data->params->hv[1] = hvect.y;
    data->params->hv[2] = hvect.z;
    data->params->radius = set1_c.Radius();

    //std::cout << "rcc: in " << bu_vls_addr(data->key) << " rcc " << set1_c.Center().x << " " << set1_c.Center().y << " " << set1_c.Center().z << " " << hvect.x << " " << hvect.y << " " << hvect.z << " " << set1_c.Radius() << "\n";
    //mk_rcc(data->wdbp, data->key.c_str(), base, hv, set1_c.Radius());

    // TODO - check for different radius values between the two circles - for a pure cylinder test should reject, but we can easily handle it with the tgc...

    return 1;
}


volume_t
subbrep_shape_recognize(struct subbrep_object_data *data)
{
    if (subbrep_is_planar(data)) return PLANAR_VOLUME;
    if (subbrep_is_cylinder(data)) return CYLINDER;
    return BREP;
}

void
subbrep_object_init(struct subbrep_object_data *obj, ON_Brep *brep)
{
    if (!obj) return;
    BU_GET(obj->params, struct csg_object_params);
    BU_GET(obj->key, struct bu_vls);
    BU_GET(obj->children, struct bu_ptbl);
    bu_vls_init(obj->key);
    bu_ptbl_init(obj->children, 8, "children table");
    obj->parent = NULL;
    obj->brep = brep;
    obj->type = BREP;
}

void
subbrep_object_free(struct subbrep_object_data *obj)
{
    if (!obj) return;
    BU_PUT(obj->params, struct csg_object_params);
    bu_vls_free(obj->key);
    BU_PUT(obj->key, struct bu_vls);
    bu_ptbl_free(obj->children);
    BU_GET(obj->children, struct bu_ptbl);
    if (obj->faces) bu_free(obj->faces, "obj faces");
    if (obj->loops) bu_free(obj->loops, "obj loops");
    if (obj->edges) bu_free(obj->edges, "obj edges");
    if (obj->fol) bu_free(obj->fol, "obj fol");
    if (obj->fil) bu_free(obj->fil, "obj fil");
    obj->parent = NULL;
    obj->brep = NULL;
}


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

struct bu_ptbl *
find_subbreps(ON_Brep *brep)
{
    struct bu_ptbl *subbreps;
    std::set<std::string> subbrep_keys;
    BU_GET(subbreps, struct bu_ptbl);
    bu_ptbl_init(subbreps, 8, "subbrep table");

    /* For each face, build the topologically connected subset.  If that
     * subset has not already been seen, add it to the brep's set of
     * subbreps */
    for (int i = 0; i < brep->m_F.Count(); i++) {
	std::string key;
	std::set<int> faces;
	std::set<int> loops;
	std::set<int> edges;
	std::set<int> fol; /* Faces with outer loops in object loop network */
	std::set<int> fil; /* Faces with only inner loops in object loop network */
	std::queue<int> local_loops;
	std::set<int> processed_loops;
	std::set<int>::iterator s_it;

	ON_BrepFace *face = &(brep->m_F[i]);
	faces.insert(i);
	fol.insert(i);
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
	for (s_it = faces.begin(); s_it != faces.end(); s_it++) {
	    if (fol.find(*s_it) == fol.end()) {
		fil.insert(*s_it);
	    }
	}
	key = face_set_key(faces);

	/* If we haven't seen this particular subset before, add it */
	if (subbrep_keys.find(key) == subbrep_keys.end()) {
	    int j = 0;
	    subbrep_keys.insert(key);
	    struct subbrep_object_data *new_obj;
	    BU_GET(new_obj, struct subbrep_object_data);
	    subbrep_object_init(new_obj, brep);
	    bu_vls_sprintf(new_obj->key, "%s", key.c_str());
	    new_obj->faces_cnt = faces.size();
	    if (new_obj->faces_cnt > 0) {
		new_obj->faces = (int *)bu_calloc(new_obj->faces_cnt, sizeof(int), "faces array");
		for (s_it = faces.begin(); s_it != faces.end(); s_it++) {
		    new_obj->faces[j] = *s_it;
		    j++;
		}
	    }
	    j = 0;
	    new_obj->loops_cnt = loops.size();
	    if (new_obj->loops_cnt > 0) {
		new_obj->loops_cnt = loops.size();
		new_obj->loops = (int *)bu_calloc(new_obj->loops_cnt, sizeof(int), "loops array");
		for (s_it = loops.begin(); s_it != loops.end(); s_it++) {
		    new_obj->loops[j] = *s_it;
		    j++;
		}
	    }
	    j = 0;
	    new_obj->edges_cnt = edges.size();
	    if (new_obj->edges_cnt > 0) {
		new_obj->edges_cnt = edges.size();
		new_obj->edges = (int *)bu_calloc(new_obj->edges_cnt, sizeof(int), "edges array");
		for (s_it = edges.begin(); s_it != edges.end(); s_it++) {
		    new_obj->edges[j] = *s_it;
		    j++;
		}
	    }
	    j = 0;
	    new_obj->fol_cnt = fol.size();
	    if (new_obj->fol_cnt > 0) {
		new_obj->fol_cnt = fol.size();
		new_obj->fol = (int *)bu_calloc(new_obj->fol_cnt, sizeof(int), "fol array");
		for (s_it = fol.begin(); s_it != fol.end(); s_it++) {
		    new_obj->fol[j] = *s_it;
		    j++;
		}
	    }
	    j = 0;
	    new_obj->fil_cnt = fil.size();
	    if (new_obj->fil_cnt > 0) {
		new_obj->fil_cnt = fil.size();
		new_obj->fil = (int *)bu_calloc(new_obj->fil_cnt, sizeof(int), "fil array");
		for (s_it = fil.begin(); s_it != fil.end(); s_it++) {
		    new_obj->fil[j] = *s_it;
		    j++;
		}
	    }

	    (void)subbrep_shape_recognize(new_obj);

	    bu_ptbl_ins(subbreps, (long *)new_obj);
	}
    }

    return subbreps;
}

void
print_subbrep_object(struct subbrep_object_data *data, const char *offset)
{
    struct bu_vls log = BU_VLS_INIT_ZERO;
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sObject %s:\n", offset, bu_vls_addr(data->key));
    bu_vls_printf(&log, "%sType: ", offset);
    switch (data->type) {
	case COMB:
	    bu_vls_printf(&log, "comb\n");
	    break;
	case PLANAR_VOLUME:
	    bu_vls_printf(&log, "planar\n");
	    break;
	case CYLINDER:
	    bu_vls_printf(&log, "cylinder\n");
	    break;
	case CONE:
	    bu_vls_printf(&log, "cone\n");
	    break;
	case SPHERE:
	    bu_vls_printf(&log, "sphere\n");
	    break;
	case ELLIPSOID:
	    bu_vls_printf(&log, "ellipsoid\n");
	    break;
	case TORUS:
	    bu_vls_printf(&log, "torus\n");
	    break;
	default:
	    bu_vls_printf(&log, "brep\n");
    }
    bu_vls_printf(&log, "%sFace set: ", offset);
    for (int i = 0; i < data->faces_cnt; i++) {
	bu_vls_printf(&log, "%d", data->faces[i]);
	if (i + 1 != data->faces_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sLoop set: ", offset);
    for (int i = 0; i < data->loops_cnt; i++) {
	bu_vls_printf(&log, "%d", data->loops[i]);
	if (i + 1 != data->loops_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sEdge set: ", offset);
    for (int i = 0; i < data->edges_cnt; i++) {
	bu_vls_printf(&log, "%d", data->edges[i]);
	if (i + 1 != data->edges_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sFaces with outer loop set: ", offset);
    for (int i = 0; i < data->fol_cnt; i++) {
	bu_vls_printf(&log, "%d", data->fol[i]);
	if (i + 1 != data->fol_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");
    bu_vls_printf(&log, "%sFaces with only inner loops set: ", offset);
    for (int i = 0; i < data->fil_cnt; i++) {
	bu_vls_printf(&log, "%d", data->fil[i]);
	if (i + 1 != data->fil_cnt) bu_vls_printf(&log, ",");
    }
    bu_vls_printf(&log, "\n");

    bu_log("%s\n", bu_vls_addr(&log));
    bu_vls_free(&log);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
