#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "brep.h"
#include "shape_recognition.h"

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
GetSurfaceType(const ON_Surface *orig_surface)
{
    ON_Surface *surface;
    surface_t ret = SURFACE_GENERAL;
    ON_Surface *in_surface = orig_surface->Duplicate();
    // Make things a bit larger so small surfaces can be identified
    ON_Xform sf(1000);
    in_surface->Transform(sf);
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
st_done:
    delete surface;
    delete in_surface;
    return ret;
}


surface_t
subbrep_highest_order_face(struct subbrep_island_data *data)
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

#if 0
volume_t
subbrep_shape_recognize(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    if (subbrep_is_planar(msgs, data)) return PLANAR_VOLUME;
    if (subbrep_is_cylinder(msgs, data, BREP_CYLINDRICAL_TOL)) return CYLINDER;
    //if (subbrep_is_cone(msgs, data, BREP_CONIC_TOL)) return CONE;
    return BREP;
}
#endif

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
}

void
csg_object_params_init(struct csg_object_params *csg)
{
    csg->type = 0;
    csg->negative = 0;
    csg->id = -1;
    csg->bool_op = '\0';
    csg->planes = NULL;
    csg->faces = NULL;
    csg->verts = NULL;
}

void
csg_object_params_free(struct csg_object_params *csg)
{
    if (!csg) return;
    if (csg->planes) bu_free(csg->planes, "free planes");
    if (csg->faces) bu_free(csg->faces , "free faces");
    if (csg->verts) bu_free(csg->verts , "free verts");
}


void
subbrep_shoal_init(struct subbrep_shoal_data *data, struct subbrep_island_data *i)
{
    data->i = i;
    BU_GET(data->params, struct csg_object_params);
    csg_object_params_init(data->params);
    BU_GET(data->sub_params, struct bu_ptbl);
    bu_ptbl_init(data->sub_params, 8, "sub_params table");
    data->loops = NULL;
    data->loops_cnt = 0;
}

void
subbrep_shoal_free(struct subbrep_shoal_data *data)
{
    if (!data) return;
    csg_object_params_free(data->params);
    BU_PUT(data->params, struct csg_object_params);
    data->params = NULL;
    for (unsigned int i = 0; i < BU_PTBL_LEN(data->sub_params); i++) {
	struct csg_object_params *c = (struct csg_object_params *)BU_PTBL_GET(data->sub_params, i);
	csg_object_params_free(c);
	BU_PUT(c, struct csg_object_params);
    }
    bu_ptbl_free(data->sub_params);
    BU_PUT(data->sub_params, struct bu_ptbl);
    if (data->loops) bu_free(data->loops, "free loop array");
}


void
subbrep_island_init(struct subbrep_island_data *obj, const ON_Brep *brep)
{
    if (!obj) return;

    /* We're a B-Rep until proven otherwise */
    obj->type = BREP;

    obj->brep = brep;
    obj->local_brep = NULL;

    obj->bbox = new ON_BoundingBox();
    ON_MinMaxInit(&(obj->bbox->m_min), &(obj->bbox->m_max));

    BU_GET(obj->nucleus, struct subbrep_shoal_data);
    subbrep_shoal_init(obj->nucleus, obj);

    BU_GET(obj->children, struct bu_ptbl);
    bu_ptbl_init(obj->children, 8, "children table");

    BU_GET(obj->subtractions, struct bu_ptbl);
    bu_ptbl_init(obj->subtractions, 8, "children table");

    BU_GET(obj->key, struct bu_vls);
    bu_vls_init(obj->key);

    obj->obj_cnt = NULL;
    obj->faces = NULL;
    obj->loops = NULL;
    obj->edges = NULL;
    obj->fol = NULL;
    obj->fil = NULL;
    obj->null_verts = NULL;
    obj->null_edges = NULL;
}

void
subbrep_island_free(struct subbrep_island_data *obj)
{
    if (!obj) return;

    obj->brep = NULL;
    if (obj->local_brep) delete obj->local_brep;
    obj->local_brep = NULL;

    delete obj->bbox;

    subbrep_shoal_free(obj->nucleus);
    BU_PUT(obj->nucleus, struct csg_obj_params);
    obj->nucleus = NULL;

    bu_vls_free(obj->key);
    BU_PUT(obj->key, struct bu_vls);
    obj->key = NULL;

    for (unsigned int i = 0; i < BU_PTBL_LEN(obj->children); i++){
	struct subbrep_shoal_data *cobj = (struct subbrep_shoal_data *)BU_PTBL_GET(obj->children, i);
	subbrep_shoal_free(cobj);
	BU_PUT(cobj, struct subbrep_shoal_data);
    }
    bu_ptbl_free(obj->children);
    BU_PUT(obj->children, struct bu_ptbl);
    obj->children = NULL;

    /* Anything in here will be freed elsewhere */
    if (obj->subtractions) {
	bu_ptbl_free(obj->subtractions);
	BU_PUT(obj->subtractions, struct bu_ptbl);
    }
    obj->subtractions = NULL;

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
    if (obj->null_edges) bu_free(obj->null_edges, "ignore edges");
    obj->null_edges = NULL;
}

void subbrep_tree_init(struct subbrep_tree_node *node)
{
    BU_GET(node->children, struct bu_ptbl);
    bu_ptbl_init(node->children, 8, "init table");
    node->parent = NULL;
    node->island = NULL;
}

void subbrep_tree_free(struct subbrep_tree_node *node)
{
    bu_ptbl_free(node->children);
    BU_PUT(node->children, struct bu_ptbl);
    node->parent = NULL;
    node->island = NULL;
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
    if (*array) bu_free((*array), "free old array");
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
    if (*array) bu_free((*array), "free old array");
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
       surface_t stype = ((surface_t *)data->face_surface_types)[face->m_face_index];
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


int
subbrep_make_brep(struct bu_vls *UNUSED(msgs), struct subbrep_island_data *data)
{
    if (data->local_brep) return 0;
    data->local_brep = ON_Brep::New();
    const ON_Brep *brep = data->brep;
    ON_Brep *nbrep = data->local_brep;

    // We only want the subset of data that is part of this particular
    // subobject - to do that, we need to map elements from their indices in
    // the old Brep to their locations in the new.
    std::map<int, int> face_map;
    std::map<int, int> surface_map;
    std::map<int, int> edge_map;
    std::map<int, int> vertex_map;
    std::map<int, int> c3_map;
    std::map<int, int> trim_map;

    std::set<int> faces;
    std::set<int> fil;
    array_to_set(&faces, data->faces, data->faces_cnt);
    array_to_set(&fil, data->fil, data->fil_cnt);

    // Use the set of loops to collect loops, trims, vertices, edges, faces, 2D
    // and 3D curves
    for (int i = 0; i < data->loops_cnt; i++) {
	const ON_BrepLoop *loop = &(brep->m_L[data->loops[i]]);
	const ON_BrepFace *face = loop->Face();
	// Face
	if (face_map.find(face->m_face_index) == face_map.end()) {
	    // Get Surface of Face
	    ON_Surface *ns = face->SurfaceOf()->Duplicate();
	    int nsid = nbrep->AddSurface(ns);
	    surface_map[face->SurfaceIndexOf()] = nsid;
	    // Get Face
	    ON_BrepFace &new_face = nbrep->NewFace(nsid);
	    face_map[face->m_face_index] = new_face.m_face_index;
	    if (fil.find(face->m_face_index) != fil.end()) nbrep->FlipFace(new_face);
	    if (face->m_bRev) nbrep->FlipFace(new_face);
	}
	// Loop
	ON_BrepLoop &nl = nbrep->NewLoop(ON_BrepLoop::outer, nbrep->m_F[face_map[face->m_face_index]]);
	if (loop->m_type != ON_BrepLoop::outer && loop->m_type != ON_BrepLoop::inner)
	    nl.m_type = loop->m_type;
	// Trims, etc.
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    int v0i, v1i;
	    int c2i, c3i;
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    const ON_BrepEdge *nedge = NULL;

	    // Get the 2D curve from the trim.
	    ON_Curve *nc = trim->TrimCurveOf()->Duplicate();
	    c2i = nbrep->AddTrimCurve(nc);

	    // Edges, etc.
	    if (trim->m_ei != -1 && edge) {
		// Get the 3D curve from the edge
		if (c3_map.find(edge->EdgeCurveIndexOf()) == c3_map.end()) {
		    ON_Curve *nec = edge->EdgeCurveOf()->Duplicate();
		    c3i = nbrep->AddEdgeCurve(nec);
		    c3_map[edge->EdgeCurveIndexOf()] = c3i;
		} else {
		    c3i = c3_map[edge->EdgeCurveIndexOf()];
		}

		// Get the vertices from the edges
		if (vertex_map.find(edge->Vertex(0)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newv0 = nbrep->NewVertex(edge->Vertex(0)->Point(), edge->Vertex(0)->m_tolerance);
		    v0i = newv0.m_vertex_index;
		    vertex_map[edge->Vertex(0)->m_vertex_index] = v0i;
		} else {
		    v0i = vertex_map[edge->Vertex(0)->m_vertex_index];
		}
		if (vertex_map.find(edge->Vertex(1)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newv1 = nbrep->NewVertex(edge->Vertex(1)->Point(), edge->Vertex(1)->m_tolerance);
		    v1i = newv1.m_vertex_index;
		    vertex_map[edge->Vertex(1)->m_vertex_index] = v1i;
		} else {
		    v1i = vertex_map[edge->Vertex(1)->m_vertex_index];
		}

		// Edge
		if (edge_map.find(edge->m_edge_index) == edge_map.end()) {
		    ON_BrepEdge& new_edge = nbrep->NewEdge(nbrep->m_V[v0i], nbrep->m_V[v1i], c3i, NULL ,0);
		    edge_map[edge->m_edge_index] = new_edge.m_edge_index;
		}
		nedge = &(nbrep->m_E[edge_map[edge->m_edge_index]]);
	    }

	    // Now set up the Trim
	    if (trim->m_ei != -1 && nedge) {
		ON_BrepEdge &n_edge = nbrep->m_E[nedge->m_edge_index];
		ON_BrepTrim &nt = nbrep->NewTrim(n_edge, trim->m_bRev3d, nl, c2i);
		nt.m_tolerance[0] = trim->m_tolerance[0];
		nt.m_tolerance[1] = trim->m_tolerance[1];
		nt.m_type = trim->m_type;
		nt.m_iso = trim->m_iso;
	    } else {
		if (vertex_map.find(trim->Vertex(0)->m_vertex_index) == vertex_map.end()) {
		    ON_BrepVertex& newvs = nbrep->NewVertex(trim->Vertex(0)->Point(), trim->Vertex(0)->m_tolerance);
		    vertex_map[trim->Vertex(0)->m_vertex_index] = newvs.m_vertex_index;
		    ON_BrepTrim &nt = nbrep->NewSingularTrim(newvs, nl, trim->m_iso, c2i);
		    nt.m_type = trim->m_type;
		    nt.m_tolerance[0] = trim->m_tolerance[0];
		    nt.m_tolerance[1] = trim->m_tolerance[1];
		} else {
		    ON_BrepTrim &nt = nbrep->NewSingularTrim(nbrep->m_V[vertex_map[trim->Vertex(0)->m_vertex_index]], nl, trim->m_iso, c2i);
		    nt.m_type = trim->m_type;
		    nt.m_tolerance[0] = trim->m_tolerance[0];
		    nt.m_tolerance[1] = trim->m_tolerance[1];
		}

	    }
	}
    }

    // Make sure all the loop directions and types are correct
    for (int f = 0; f < nbrep->m_F.Count(); f++) {
	ON_BrepFace *face = &(nbrep->m_F[f]);
	if (face->m_li.Count() == 1) {
	    ON_BrepLoop& loop = nbrep->m_L[face->m_li[0]];
	    if (nbrep->LoopDirection(loop) != 1) {
		nbrep->FlipLoop(loop);
	    }
	    loop.m_type = ON_BrepLoop::outer;
	} else {
	    int i1 = 0;
	    int tmp;
	    ON_BoundingBox o_bbox, c_bbox;
	    int outer_loop_ind = face->m_li[0];
	    nbrep->m_L[outer_loop_ind].GetBoundingBox(o_bbox);
	    for (int l = 1; l < face->m_li.Count(); l++) {
		ON_BrepLoop& loop = nbrep->m_L[face->m_li[l]];
		loop.GetBoundingBox(c_bbox);

		if (c_bbox.Includes(o_bbox)) {
		    if (nbrep->m_L[outer_loop_ind].m_type == ON_BrepLoop::outer) {
			nbrep->m_L[outer_loop_ind].m_type = ON_BrepLoop::inner;
		    }
		    o_bbox = c_bbox;
		    outer_loop_ind = face->m_li[l];
		    i1 = l;
		}
	    }
	    if (nbrep->m_L[outer_loop_ind].m_type != ON_BrepLoop::outer)
		nbrep->m_L[outer_loop_ind].m_type = ON_BrepLoop::outer;
	    tmp = face->m_li[0];
	    face->m_li[0] = face->m_li[i1];
	    face->m_li[i1] = tmp;
	    for (int l = 1; l < face->m_li.Count(); l++) {
		if (nbrep->m_L[face->m_li[l]].m_type != ON_BrepLoop::inner && nbrep->m_L[face->m_li[l]].m_type != ON_BrepLoop::slit) {
		    nbrep->m_L[face->m_li[l]].m_type = ON_BrepLoop::inner;
		}
	    }
	}
    }

    nbrep->ShrinkSurfaces();
    nbrep->CullUnusedSurfaces();

    //std::cout << "new brep done: " << bu_vls_addr(data->key) << "\n";

    return 1;
}





// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
