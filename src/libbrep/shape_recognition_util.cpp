#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
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
GetSurfaceType(const ON_Surface *in_surface, struct filter_obj *obj)
{
    surface_t ret = SURFACE_GENERAL;
    ON_Surface *surface = in_surface->Duplicate();
    if (obj) {
	filter_obj_init(obj);
	if (surface->IsPlanar(obj->plane, BREP_PLANAR_TOL)) {
	    ret = SURFACE_PLANE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsSphere(obj->sphere , BREP_SPHERICAL_TOL)) {
	    ret = SURFACE_SPHERE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCylinder(obj->cylinder , BREP_CYLINDRICAL_TOL)) {
	    ret = SURFACE_CYLINDER;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsCone(obj->cone, BREP_CONIC_TOL)) {
	    ret = SURFACE_CONE;
	    goto st_done;
	}
	delete surface;
	surface = in_surface->Duplicate();
	if (surface->IsTorus(obj->torus, BREP_TOROIDAL_TOL)) {
	    ret = SURFACE_TORUS;
	    goto st_done;
	}
    } else {
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
    return ret;
}


surface_t
highest_order_face(struct subbrep_object_data *data)
{
    int planar = 0;
    int spherical = 0;
    int cylindrical = 0;
    int cone = 0;
    int torus = 0;
    int general = 0;
    int hof = -1;
    surface_t hofo = SURFACE_PLANE;
    std::set<int> faces;
    std::set<int>::iterator f_it;
    array_to_set(&faces, data->faces, data->faces_cnt);
    for (f_it = faces.begin(); f_it != faces.end(); f_it++) {
	int ind = *f_it;
	ON_Surface *s = data->brep->m_F[ind].SurfaceOf()->Duplicate();
	int surface_type = (int)GetSurfaceType(s, NULL);
	switch (surface_type) {
	    case SURFACE_PLANE:
		planar++;
		if (hofo < SURFACE_PLANE) {
		    hof = ind;
		    hofo = SURFACE_PLANE;
		}
		break;
	    case SURFACE_SPHERE:
		spherical++;
		if (hofo < SURFACE_SPHERE) {
		    hof = ind;
		    hofo = SURFACE_SPHERE;
		}
		break;
	    case SURFACE_CYLINDER:
		cylindrical++;
		if (hofo < SURFACE_CYLINDER) {
		    hof = ind;
		    hofo = SURFACE_CYLINDER;
		}
		break;
	    case SURFACE_CONE:
		cone++;
		if (hofo < SURFACE_CONE) {
		    hof = ind;
		    hofo = SURFACE_CONE;
		}
		break;
	    case SURFACE_TORUS:
		torus++;
		if (hofo < SURFACE_TORUS) {
		    hof = ind;
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

volume_t
subbrep_shape_recognize(struct subbrep_object_data *data)
{
    if (subbrep_is_planar(data)) return PLANAR_VOLUME;
    if (subbrep_is_cylinder(data, BREP_CYLINDRICAL_TOL)) return CYLINDER;
    if (subbrep_is_cone(data, BREP_CONIC_TOL)) return CONE;
    return BREP;
}

void
subbrep_object_init(struct subbrep_object_data *obj, const ON_Brep *brep)
{
    if (!obj) return;
    BU_GET(obj->key, struct bu_vls);
    BU_GET(obj->children, struct bu_ptbl);
    BU_GET(obj->params, struct csg_object_params);
    obj->params->planes = NULL;
    obj->planar_obj = NULL;
    bu_vls_init(obj->key);
    bu_ptbl_init(obj->children, 8, "children table");
    obj->parent = NULL;
    obj->brep = brep;
    obj->local_brep = NULL;
    obj->type = BREP;
    obj->is_island = 0;
}

void
subbrep_object_free(struct subbrep_object_data *obj)
{
    if (!obj) return;
    if (obj->params && obj->params->planes) bu_free(obj->params->planes, "csg planes");
    if (obj->params) BU_PUT(obj->params, struct csg_object_params);
    obj->params = NULL;
    if (obj->planar_obj) {
	subbrep_object_free(obj->planar_obj);
	BU_PUT(obj->planar_obj, struct subbrep_object_data);
    }
    obj->planar_obj = NULL;
    bu_vls_free(obj->key);
    if (obj->children) {
	for (unsigned int i = 0; i < BU_PTBL_LEN(obj->children); i++){
	    struct subbrep_object_data *cobj = (struct subbrep_object_data *)BU_PTBL_GET(obj->children, i);
	    subbrep_object_free(cobj);
	    BU_PUT(cobj, struct subbrep_object_data);
	}
	bu_ptbl_free(obj->children);
	BU_PUT(obj->children, struct bu_ptbl);
    }
    obj->children = NULL;
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
    if (obj->planar_obj_vert_map) bu_free(obj->planar_obj_vert_map, "obj fil");
    obj->planar_obj_vert_map = NULL;
    obj->parent = NULL;
    if (obj->local_brep) delete obj->local_brep;
    obj->local_brep = NULL;
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
	    (*array)[i+*array_cnt] = m_it->first;
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


// Remove degenerate edge sets. A degenerate edge set is defined as two
// linear segments having the same two vertices.  (To be sure, we should probably
// check curve directions in loops in some fashion...)
void
subbrep_remove_degenerate_edges(struct subbrep_object_data *data, std::set<int> *edges){
    std::set<int> degenerate;
    std::set<int>::iterator e_it;
    for (e_it = edges->begin(); e_it != edges->end(); e_it++) {
	if (degenerate.find(*e_it) == degenerate.end()) {
	    ON_Curve *ec1 = data->brep->m_E[*e_it].EdgeCurveOf()->Duplicate();
	    if (ec1->IsLinear()) {
		for (int j = 0; j < data->edges_cnt; j++) {
		    int f_ind = data->edges[j];
		    ON_Curve *ec2 = data->brep->m_E[f_ind].EdgeCurveOf()->Duplicate();
		    if (ec2->IsLinear()) {
			if ((data->brep->m_E[*e_it].Vertex(0)->Point() == data->brep->m_E[f_ind].Vertex(0)->Point() && data->brep->m_E[*e_it].Vertex(1)->Point() == data->brep->m_E[f_ind].Vertex(1)->Point()) ||
				(data->brep->m_E[*e_it].Vertex(1)->Point() == data->brep->m_E[f_ind].Vertex(0)->Point() && data->brep->m_E[*e_it].Vertex(0)->Point() == data->brep->m_E[f_ind].Vertex(1)->Point()))
			{
			    degenerate.insert(*e_it);
			    degenerate.insert(f_ind);
			    break;
			}
		    }
		}
	    }
	}
    }
    for (e_it = degenerate.begin(); e_it != degenerate.end(); e_it++) {
	//std::cout << "erasing " << *e_it << "\n";
	edges->erase(*e_it);
    }
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
