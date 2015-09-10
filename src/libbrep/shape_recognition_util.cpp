#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "brep.h"
#include "shape_recognition.h"

ON_3dPoint
ON_LinePlaneIntersect(ON_Line &line, ON_Plane &plane)
{
    ON_3dPoint result;
    result.x = ON_DBL_MAX;
    result.y = ON_DBL_MAX;
    result.z = ON_DBL_MAX;
    ON_3dVector n = plane.Normal();
    ON_3dVector l = line.Direction();
    ON_3dPoint l0 = line.PointAt(0.5);
    ON_3dPoint p0 = plane.Origin();

    ON_3dVector p0l0 = p0 - l0;
    double p0l0n = ON_DotProduct(p0l0, n);
    double ln = ON_DotProduct(l, n);
    if (NEAR_ZERO(ln, 0.000001) && NEAR_ZERO(p0l0n, 0.000001)) {
	result.x = -ON_DBL_MAX;
	result.y = -ON_DBL_MAX;
	result.z = -ON_DBL_MAX;
	return result;
    }
    if (NEAR_ZERO(ln, 0.000001)) return result;

    double d = p0l0n/ln;

    result = d*l + l0;
    return result;
}

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
    if (curve->IsArc(NULL, &arc, BREP_CYLINDRICAL_TOL)) {
	if (arc.IsCircle()) return CURVE_CIRCLE;
	ON_Circle circ(arc.StartPoint(), arc.MidPoint(), arc.EndPoint());
	bu_log("arc's circle: center %f, %f, %f   radius %f\n", circ.Center().x, circ.Center().y, circ.Center().z, circ.Radius());
       	return CURVE_ARC;
    }

    // TODO - looks like we need a better test for this...
    if (curve->IsEllipse(NULL, NULL, BREP_ELLIPSOIDAL_TOL)) return CURVE_ELLIPSE;

    return CURVE_GENERAL;
}

surface_t
GetSurfaceType(const ON_Surface *orig_surface, struct filter_obj *obj)
{
    ON_Surface *surface;
    surface_t ret = SURFACE_GENERAL;
    ON_Surface *in_surface = orig_surface->Duplicate();
    // Make things a bit larger so small surfaces can be identified
    ON_Xform sf(1000);
    in_surface->Transform(sf);
    if (obj) {
	filter_obj_init(obj);
	surface = in_surface->Duplicate();
	if (surface->IsPlanar(obj->plane, BREP_PLANAR_TOL)) {
	    ret = SURFACE_PLANE;
	    obj->stype = SURFACE_PLANE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsSphere(obj->sphere , BREP_SPHERICAL_TOL)) {
	    ret = SURFACE_SPHERE;
	    obj->stype = SURFACE_SPHERE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCylinder(obj->cylinder , BREP_CYLINDRICAL_TOL)) {
	    ret = SURFACE_CYLINDER;
	    obj->stype = SURFACE_CYLINDER;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCone(obj->cone, BREP_CONIC_TOL)) {
	    ret = SURFACE_CONE;
	    obj->stype = SURFACE_CONE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsTorus(obj->torus, BREP_TOROIDAL_TOL)) {
	    ret = SURFACE_TORUS;
	    obj->stype = SURFACE_TORUS;
	    goto st_done;
	}
    } else {
	surface = in_surface->Duplicate();
	if (surface->IsPlanar(NULL, BREP_PLANAR_TOL)) {
	    ret = SURFACE_PLANE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsSphere(NULL, BREP_SPHERICAL_TOL)) {
	    ret = SURFACE_SPHERE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCylinder(NULL, BREP_CYLINDRICAL_TOL)) {
	    ret = SURFACE_CYLINDER;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCone(NULL, BREP_CONIC_TOL)) {
	    ret = SURFACE_CONE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsTorus(NULL, BREP_TOROIDAL_TOL)) {
	    ret = SURFACE_TORUS;
	    goto st_done;
	}
    }
st_done:
    delete surface;
    delete in_surface;
    return ret;
}


surface_t
highest_order_face(struct subbrep_island_data *data)
{
    int planar = 0;
    int spherical = 0;
    int cylindrical = 0;
    int cone = 0;
    int torus = 0;
    int general = 0;
    surface_t *fstypes = (surface_t *)data->face_surface_types;
    surface_t hofo = SURFACE_PLANE;
    for (int i = 0; i < data->faces_cnt; i++) {
	int ind = data->faces[i];
	int surface_type = (int)fstypes[ind];
	switch (surface_type) {
	    case SURFACE_PLANE:
		planar++;
		if (hofo < SURFACE_PLANE) {
		    hofo = SURFACE_PLANE;
		}
		break;
	    case SURFACE_SPHERE:
		spherical++;
		if (hofo < SURFACE_SPHERE) {
		    hofo = SURFACE_SPHERE;
		}
		break;
	    case SURFACE_CYLINDER:
		cylindrical++;
		if (hofo < SURFACE_CYLINDER) {
		    hofo = SURFACE_CYLINDER;
		}
		break;
	    case SURFACE_CONE:
		cone++;
		if (hofo < SURFACE_CONE) {
		    hofo = SURFACE_CONE;
		}
		break;
	    case SURFACE_TORUS:
		torus++;
		if (hofo < SURFACE_TORUS) {
		    hofo = SURFACE_TORUS;
		}
		break;
	    default:
		general++;
		hofo = SURFACE_GENERAL;
		//std::cout << "general surface(" << used_face.m_face_index << "): " << used_face.SurfaceIndexOf() << "\n";
		break;
	}
    }
    return hofo;
}

void
filter_obj_init(struct filter_obj *obj)
{
    if (!obj) return;
    if (!obj->plane) obj->plane = new ON_Plane;
    if (!obj->sphere) obj->sphere = new ON_Sphere;
    if (!obj->cylinder) obj->cylinder = new ON_Cylinder;
    if (!obj->cone) obj->cone = new ON_Cone;
    if (!obj->torus) obj->torus = new ON_Torus;
    obj->type = BREP;
}

void
filter_obj_free(struct filter_obj *obj)
{
    if (!obj) return;
    delete obj->plane;
    delete obj->sphere;
    delete obj->cylinder;
    delete obj->cone;
    delete obj->torus;
}

int
filter_objs_equal(struct filter_obj *obj1, struct filter_obj *obj2)
{
    ON_Line l1;
    ON_3dPoint p1, p2;
    double d1, d2;
    int ret = 0;
    if (obj1->stype != obj2->stype) return 0;
    switch (obj1->stype) {
	case SURFACE_PLANE:
	    break;
	case SURFACE_CYLINDRICAL_SECTION:
	case SURFACE_CYLINDER:
	    if (NEAR_ZERO(obj1->cylinder->circle.Radius() - obj2->cylinder->circle.Radius(), 0.001)) {
		l1 = ON_Line(obj1->cylinder->Center(), obj1->cylinder->Center() + obj1->cylinder->Axis());
		d1 = l1.DistanceTo(obj2->cylinder->Center());
		d2 = l1.DistanceTo(obj2->cylinder->Center() + obj2->cylinder->Axis());
		if (NEAR_ZERO(d1, 0.001) && NEAR_ZERO(d2, 0.001)) ret = 1;
	    }
	    break;
	case SURFACE_CONE:
	    break;
	case SURFACE_SPHERICAL_SECTION:
	case SURFACE_SPHERE:
	    break;
	case SURFACE_ELLIPSOIDAL_SECTION:
	case SURFACE_ELLIPSOID:
	    break;
	case SURFACE_TORUS:
	    break;
	default:
	    break;
    }
    return ret;
}

volume_t
subbrep_shape_recognize(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    if (subbrep_is_planar(msgs, data)) return PLANAR_VOLUME;
    if (subbrep_is_cylinder(msgs, data, BREP_CYLINDRICAL_TOL)) return CYLINDER;
    //if (subbrep_is_cone(msgs, data, BREP_CONIC_TOL)) return CONE;
    return BREP;
}

void
ON_MinMaxInit(ON_3dPoint *min, ON_3dPoint *max)
{
    min->x = ON_DBL_MAX;
    min->y = ON_DBL_MAX;
    min->z = ON_DBL_MAX;
    max->x = -ON_DBL_MAX;
    max->y = -ON_DBL_MAX;
    max->z = -ON_DBL_MAX;
}

void
subbrep_bbox(struct subbrep_island_data *obj)
{
    for (int i = 0; i < obj->fol_cnt; i++) {
	ON_3dPoint min, max;
	ON_MinMaxInit(&min, &max);
	const ON_BrepFace *face = &(obj->brep->m_F[obj->fol[i]]);
	// Bounding intervals of outer loop
	for (int ti = 0; ti < face->OuterLoop()->TrimCount(); ti++) {
	    ON_BrepTrim *trim = face->OuterLoop()->Trim(ti);
	    trim->GetBoundingBox(min, max, true);
	}
	ON_Interval u(min.x, max.x);
	ON_Interval v(min.y, max.y);
	surface_GetBoundingBox(face->SurfaceOf(), u, v, *(obj->bbox), true);
    }
    std::set<int> loops;
    array_to_set(&loops, obj->loops, obj->loops_cnt);
    for (int i = 0; i < obj->fil_cnt; i++) {
	ON_3dPoint min, max;
	ON_MinMaxInit(&min, &max);
	const ON_BrepFace *face = &(obj->brep->m_F[obj->fil[i]]);
	int loop_ind = -1;
	for (int li = 0; li < face->LoopCount(); li++) {
	    int loop_index = face->Loop(li)->m_loop_index;
	    if (loops.find(loop_index) != loops.end()) {
		loop_ind = loop_index;
		break;
	    }
	}
	if (loop_ind == -1) {
	    bu_log("Error - could not find fil loop!\n");
	}
	const ON_BrepLoop *loop= &(obj->brep->m_L[loop_ind]);
	for (int ti = 0; ti < loop->TrimCount(); ti++) {
	    ON_BrepTrim *trim = loop->Trim(ti);
	    trim->GetBoundingBox(min, max, true);
	}
	ON_Interval u(min.x, max.x);
	ON_Interval v(min.y, max.y);
	surface_GetBoundingBox(face->SurfaceOf(), u, v, *(obj->bbox), true);
    }
    obj->bbox_set = 1;
}

void
subbrep_object_init(struct subbrep_island_data *obj, const ON_Brep *brep)
{
    if (!obj) return;
    BU_GET(obj->key, struct bu_vls);
    BU_GET(obj->id, struct bu_vls);
    BU_GET(obj->children, struct bu_ptbl);
    BU_GET(obj->subtraction_candidates, struct bu_ptbl);
    BU_GET(obj->params, struct csg_object_params);
    BU_GET(obj->obj_name, struct bu_vls);
    obj->params->planes = NULL;
    obj->params->bool_op = '\0';
    obj->nucleus = NULL;
    bu_vls_init(obj->key);
    bu_vls_init(obj->id);
    bu_vls_init(obj->obj_name);
    bu_ptbl_init(obj->children, 8, "children table");
    bu_ptbl_init(obj->subtraction_candidates, 8, "children table");
    obj->parent = NULL;
    obj->brep = brep;
    obj->local_brep = NULL;
    obj->type = BREP;
    obj->is_island = 0;
    obj->negative_shape = 0;
    obj->bbox = new ON_BoundingBox();
    ON_MinMaxInit(&(obj->bbox->m_min), &(obj->bbox->m_max));
    obj->bbox_set = 0;
    obj->obj_cnt = NULL;
}

void
subbrep_object_free(struct subbrep_island_data *obj)
{
    if (!obj) return;
    if (obj->params && obj->params->planes) bu_free(obj->params->planes, "csg planes");
    if (obj->params) BU_PUT(obj->params, struct csg_object_params);
    obj->params = NULL;
    if (obj->nucleus) {
	BU_PUT(obj->nucleus, struct csg_obj_params);
    }
    obj->nucleus = NULL;
    if (obj->key) {
	bu_vls_free(obj->key);
	BU_PUT(obj->key, struct bu_vls);
    }
    if (obj->id) {
	bu_vls_free(obj->id);
	BU_PUT(obj->id, struct bu_vls);
    }
    if (obj->children) {
	for (unsigned int i = 0; i < BU_PTBL_LEN(obj->children); i++){
	    struct subbrep_island_data *cobj = (struct subbrep_island_data *)BU_PTBL_GET(obj->children, i);
	    subbrep_object_free(cobj);
	    BU_PUT(cobj, struct subbrep_island_data);
	}
	bu_ptbl_free(obj->children);
	BU_PUT(obj->children, struct bu_ptbl);
    }
    obj->children = NULL;
    if (obj->subtraction_candidates) {
	bu_ptbl_free(obj->subtraction_candidates);
	BU_PUT(obj->subtraction_candidates, struct bu_ptbl);
    }
    obj->subtraction_candidates = NULL;

    if (obj->faces) bu_free(obj->faces, "obj faces");
    obj->faces = NULL;
    if (obj->loops) bu_free(obj->loops, "obj loops");
    obj->loops = NULL;
    if (obj->edges) bu_free(obj->edges, "obj edges");
    obj->edges = NULL;
    if (obj->fol) bu_free(obj->fol, "obj fol");
    obj->fol = NULL;
    if (obj->fil) bu_free(obj->fil, "obj fil");
    obj->fil = NULL;
    if (obj->null_verts) bu_free(obj->null_verts, "ignore verts");
    obj->null_verts = NULL;
    if (obj->parent && (obj->parent->local_brep == obj->local_brep)) obj->parent->local_brep = NULL;
    if (obj->local_brep) delete obj->local_brep;
    obj->local_brep = NULL;

    if (obj->obj_name) {
	bu_vls_free(obj->obj_name);
	BU_PUT(obj->obj_name, struct bu_vls);
    }

    obj->parent = NULL;
}


std::string
set_key(std::set<int> intset)
{
    std::set<int>::iterator s_it;
    std::set<int>::iterator s_it2;
    std::string key;
    struct bu_vls vls_key = BU_VLS_INIT_ZERO;
    for (s_it = intset.begin(); s_it != intset.end(); s_it++) {
	bu_vls_printf(&vls_key, "%d", (*s_it));
	s_it2 = s_it;
	s_it2++;
	if (s_it2 != intset.end()) bu_vls_printf(&vls_key, ",");
    }
    key.append(bu_vls_addr(&vls_key));
    bu_vls_free(&vls_key);
    return key;
}

void
set_to_array(int **array, int *array_cnt, std::set<int> *set)
{
    std::set<int>::iterator s_it;
    int i = 0;
    (*array_cnt) = set->size();
    if ((*array_cnt) > 0) {
	(*array) = (int *)bu_calloc((*array_cnt), sizeof(int), "array");
	for (s_it = set->begin(); s_it != set->end(); s_it++) {
	    (*array)[i] = *s_it;
	    i++;
	}
    }
}

void
array_to_set(std::set<int> *set, int *array, int array_cnt)
{
    for (int i = 0; i < array_cnt; i++) {
	set->insert(array[i]);
    }
}


void
map_to_array(int **array, int *array_cnt, std::map<int,int> *map)
{
    std::map<int,int>::iterator m_it;
    int i = 0;
    (*array_cnt) = map->size();
    if ((*array_cnt) > 0) {
	(*array) = (int *)bu_calloc((*array_cnt)*2, sizeof(int), "array");
	for (m_it = map->begin(); m_it != map->end(); m_it++) {
	    (*array)[i] = m_it->first;
	    (*array)[i+*array_cnt] = m_it->second;
	    i++;
	}
    }
}

void
array_to_map(std::map<int,int> *map, int *array, int array_cnt)
{
    for (int i = 0; i < array_cnt; i++) {
	(*map)[array[i]] = array[array_cnt+i];
    }
}

void
print_subbrep_object(struct subbrep_island_data *data, const char *offset)
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

/* This routine is used when there is uncertainty about whether a
 * particular solid or combination is to be subtracted or unioned
 * into a parent.  For planar on planar cases, the check is whether every
 * vertex of the candidate solid is either on or one of above/below
 * the shared face plane.  If some vertices are above and some
 * are below, the candidate solid is self intersecting and must be
 * further subdivided.
 *
 * For non-planar situations, we're probably looking at a surface
 * surface intersection test.  A possible test short of that might
 * be vertices of bounding boxes, but that will result in situations
 * reporting subdivision necessity when there isn't any unless we
 * follow up with a more rigorous test.  This needs more thought. */
int
subbrep_determine_boolean(struct subbrep_island_data *data)
{
   int pos_cnt = 0;
   int neg_cnt = 0;

   if (data->fil_cnt == 0) return 1;

   for (int i = 0; i < data->fil_cnt; i++) {
       // Get face with inner loop
       const ON_BrepFace *face = &(data->brep->m_F[data->fil[i]]);
       const ON_Surface *surf = face->SurfaceOf();
       surface_t stype = GetSurfaceType(surf, NULL);
       ON_Plane face_plane;
       if (stype == SURFACE_PLANE) {
	   ON_Surface *ts = surf->Duplicate();
	   (void)ts->IsPlanar(&face_plane, BREP_PLANAR_TOL);
	   delete ts;
	   if (face->m_bRev) face_plane.Flip();
       }
       std::set<int> verts;
       for (int j = 0; j < data->edges_cnt; j++) {
	   verts.insert(data->brep->m_E[data->edges[j]].Vertex(0)->m_vertex_index);
	   verts.insert(data->brep->m_E[data->edges[j]].Vertex(1)->m_vertex_index);
       }
       for (std::set<int>::iterator s_it = verts.begin(); s_it != verts.end(); s_it++) {
	   // For each vertex in the subbrep, determine
	   // if it is above, below or on the face
	   if (stype == SURFACE_PLANE) {
	       ON_3dPoint p = data->brep->m_V[*s_it].Point();
	       double distance = face_plane.DistanceTo(p);
	       if (distance > BREP_PLANAR_TOL) pos_cnt++;
	       if (distance < -1*BREP_PLANAR_TOL) neg_cnt++;
	   } else {
	       // TODO - this doesn't seem to work...
	       if (face->m_bRev) {
		   // do something...
	       }
	       double current_distance = 0;
	       ON_3dPoint p = data->brep->m_V[*s_it].Point();
	       ON_3dPoint p3d;
	       ON_2dPoint p2d;
	       if (surface_GetClosestPoint3dFirstOrder(surf,p,p2d,p3d,current_distance)) {
		   if (current_distance > 0.001) pos_cnt++;
	       } else {
		   neg_cnt++;
	       }
	   }
       }
   }

   // Determine what we have.  If we have both pos and neg counts > 0,
   // the proposed brep needs to be broken down further.  all pos
   // counts is a union, all neg counts is a subtraction
   if (pos_cnt && neg_cnt) return -2;
   if (pos_cnt) return 1;
   if (neg_cnt) return -1;
   if (!pos_cnt && !neg_cnt) return 2;

   return -1;
}

/* Find corners that can be used to construct a planar face
 *
 * Collect all of the vertices in the data that are connected
 * to one and only one non-linear edge in the set.
 *
 * Failure cases are:
 *
 * More than four vertices that are mated with exactly one
 * non-linear edge in the data set
 * Four vertices meeting previous criteria that are non-planar
 * Any vertex on a linear edge that is not coplanar with the
 * plane described by the vertices meeting the above criteria
 *
 * return -1 if failed, number of corner verts if successful
 */
int
subbrep_find_corners(struct subbrep_island_data *data, int **corner_verts_array, ON_Plane *pcyl)
{
    std::set<int> candidate_verts;
    std::set<int> corner_verts;
    std::set<int> linear_verts;
    std::set<int>::iterator v_it, e_it;
    std::set<int> edges;
    if (!data || !corner_verts_array || !pcyl) return -1;
    array_to_set(&edges, data->edges, data->edges_cnt);
    // collect all candidate vertices
    for (int i = 0; i < data->edges_cnt; i++) {
	int ei = data->edges[i];
	const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
	candidate_verts.insert(edge->Vertex(0)->m_vertex_index);
	candidate_verts.insert(edge->Vertex(1)->m_vertex_index);
    }
    for (v_it = candidate_verts.begin(); v_it != candidate_verts.end(); v_it++) {
        const ON_BrepVertex *vert = &(data->brep->m_V[*v_it]);
        int curve_cnt = 0;
        int line_cnt = 0;
        for (int i = 0; i < vert->m_ei.Count(); i++) {
            int ei = vert->m_ei[i];
            const ON_BrepEdge *edge = &(data->brep->m_E[ei]);
            if (edges.find(edge->m_edge_index) != edges.end()) {
                ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
                if (ecv->IsLinear()) {
                    line_cnt++;
                } else {
                    curve_cnt++;
                }
                delete ecv;
            }
        }
        if (curve_cnt == 1) {
            corner_verts.insert(*v_it);
            //std::cout << "found corner vert: " << *v_it << "\n";
        }
        if (line_cnt > 1 && curve_cnt == 0) {
            linear_verts.insert(*v_it);
            //std::cout << "found linear vert: " << *v_it << "\n";
        }
    }

    // First, check corner count
    if (corner_verts.size() > 4) {
        //std::cout << "more than 4 corners - complex\n";
        return -1;
    }
    if (corner_verts.size() == 0) return 0;
    // Second, create the candidate face plane.  Verify coplanar status of points if we've got 4.
    std::set<int>::iterator s_it = corner_verts.begin();
    ON_3dPoint p1 = data->brep->m_V[*s_it].Point();
    s_it++;
    ON_3dPoint p2 = data->brep->m_V[*s_it].Point();
    s_it++;
    ON_3dPoint p3 = data->brep->m_V[*s_it].Point();
    ON_Plane tmp_plane(p1, p2, p3);
    if (corner_verts.size() == 4) {
        s_it++;
        ON_3dPoint p4 = data->brep->m_V[*s_it].Point();
        if (tmp_plane.DistanceTo(p4) > BREP_PLANAR_TOL) {
            //std::cout << "planar tol fail\n";
            return -1;
        } else {
            (*pcyl) = tmp_plane;
        }
    } else {
        // TODO - If we have less than four corner points and no additional curve planes, we
        // must have a face subtraction that tapers to a point at the edge of the
        // cylinder.  Pull the linear edges from the two corner points to find the third point -
        // this is a situation where a simpler arb (arb6?) is adequate to make the subtraction.
	(*pcyl) = tmp_plane;
    }

    // Third, if we had vertices with only linear edges, check to make sure they are in fact
    // on the candidate plane for the face (if not, we've got something more complex going on).
    if (linear_verts.size() > 0) {
        std::set<int>::iterator ls_it;
        for (ls_it = linear_verts.begin(); ls_it != linear_verts.end(); ls_it++) {
            ON_3dPoint pnt = data->brep->m_V[*ls_it].Point();
            if ((*pcyl).DistanceTo(pnt) > BREP_PLANAR_TOL) {
                //std::cout << "stray verts fail\n";
                return -1;
            }
        }
    }

    // If we've made it here, package up the verts and return
    int verts_cnt = 0;
    if (corner_verts.size() > 0)
	set_to_array(corner_verts_array, &verts_cnt, &corner_verts);
    return verts_cnt;
}

/* Return 1 if determination fails, else return 0 and top_pnts and bottom_pnts */
int
subbrep_top_bottom_pnts(struct subbrep_island_data *data, std::set<int> *corner_verts, ON_Plane *top_plane, ON_Plane *bottom_plane, ON_SimpleArray<const ON_BrepVertex *> *top_pnts, ON_SimpleArray<const ON_BrepVertex *> *bottom_pnts)
{
    double offset = 0.0;
    double pdist = INT_MAX;
    ON_SimpleArray<const ON_BrepVertex *> corner_pnts(4);
    std::set<int>::iterator s_it;
    /* Get a sense of how far off the planes the vertices are, and how
     * far it is from one plane to the other */
    for (s_it = corner_verts->begin(); s_it != corner_verts->end(); s_it++) {
	ON_3dPoint p = data->brep->m_V[*s_it].Point();
	corner_pnts.Append(&(data->brep->m_V[*s_it]));
	double d1 = fabs(bottom_plane->DistanceTo(p));
	double d2 = fabs(top_plane->DistanceTo(p));
	double d = (d1 > d2) ? d2 : d1;
	if (d > offset) offset = d;
	double dp = (d1 > d2) ? d1 : d2;
	if (dp < pdist) pdist = dp;
    }
    for (int p = 0; p < corner_pnts.Count(); p++) {
	double poffset1 = bottom_plane->DistanceTo(corner_pnts[p]->Point());
	double poffset2 = top_plane->DistanceTo(corner_pnts[p]->Point());
	int identified = 0;
	if (NEAR_ZERO(poffset1, 0.01 * pdist + offset)) {
	    bottom_pnts->Append(corner_pnts[p]);
	    identified = 1;
	}
	if (NEAR_ZERO(poffset2, 0.01 * pdist + offset)) {
	    top_pnts->Append(corner_pnts[p]);
	    identified = 1;
	}
	if (!identified) return 1;
    }
    // TODO - probably doesn't always have to be 2 and 2...
    if (bottom_pnts->Count() < 2) return 1;
    if (top_pnts->Count() < 2) return 1;
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
