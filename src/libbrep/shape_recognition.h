#include "common.h"
#include "bu/ptbl.h"
#include "brep.h"

#ifndef SHAPE_RECOGNITION_H
#define SHAPE_RECOGNITION_H

#define CSG_BREP_MAX_OBJS 1500

#define BREP_PLANAR_TOL 0.05
#define BREP_CYLINDRICAL_TOL 0.05
#define BREP_CONIC_TOL 0.05
#define BREP_SPHERICAL_TOL 0.05
#define BREP_ELLIPSOIDAL_TOL 0.05
#define BREP_TOROIDAL_TOL 0.05

#define pout(p)  p.x << " " << p.y << " " << p.z

void set_to_array(int **array, int *array_cnt, std::set<int> *set);
void array_to_set(std::set<int> *set, int *array, int array_cnt);
void set_key(struct bu_vls *key, int k, int *karray);

#define ON_VMOVE(on, bn) { \
    on.x = bn[0]; \
    on.y = bn[1]; \
    on.z = bn[2]; \
}
#define BN_VMOVE(bn, on) { \
    bn[0] = on.x; \
    bn[1] = on.y; \
    bn[2] = on.z; \
}

typedef enum {
    CURVE_POINT = 0,
    CURVE_LINE,
    CURVE_ARC,
    CURVE_CIRCLE,
    CURVE_PARABOLA,
    CURVE_ELLIPSE,
    //Insert any new types here
    CURVE_GENERAL  /* A curve that does not fit in any of the previous categories */
} curve_t;

typedef enum {
    SURFACE_PLANE = 0,
    SURFACE_CYLINDRICAL_SECTION,
    SURFACE_CYLINDER,
    SURFACE_CONE,
    SURFACE_SPHERICAL_SECTION,
    SURFACE_SPHERE,
    SURFACE_ELLIPSOIDAL_SECTION,
    SURFACE_ELLIPSOID,
    SURFACE_TORUS,
    //Insert any new types here
    SURFACE_GENERAL  /* A surface that does not fit in any of the previous categories */
} surface_t;

typedef enum {
    COMB = 0,  /* A comb is a boolean combination of other solids */
    ARB4,
    ARB5,
    ARB6,
    ARB7,
    ARB8,
    ARBN,
    PLANAR_VOLUME, /* NMG */
    CYLINDER,      /* RCC */
    CONE,          /* TRC */
    SPHERE,        /* SPH */
    ELLIPSOID,     /* ELL */
    TORUS,         /* TOR */
    //Insert any new types here
    BREP /* A brep is a complex solid that cannot be represented by CSG */
} volume_t;

curve_t GetCurveType(ON_Curve *curve);
surface_t GetSurfaceType(const ON_Surface *surface);
surface_t subbrep_highest_order_face(struct subbrep_island_data *data);
void subbrep_bbox(struct subbrep_island_data *obj);

void ON_MinMaxInit(ON_3dPoint *min, ON_3dPoint *max);
ON_3dPoint ON_LinePlaneIntersect(ON_Line &line, ON_Plane &plane);

void convex_plane_usage(ON_SimpleArray<ON_Plane> *planes, int **pu);
int island_nucleus(struct bu_vls *msgs, struct subbrep_island_data *data);

int subbrep_make_brep(struct bu_vls *msgs, struct subbrep_island_data *data);


struct subbrep_tree_node {
    struct subbrep_tree_node *parent;
    struct subbrep_island_data *island;
    /* subbrep_tree_node */
    struct bu_ptbl *children;
};

void subbrep_tree_init(struct subbrep_tree_node *node);
void subbrep_tree_free(struct subbrep_tree_node *node);
void subbrep_island_init(struct subbrep_island_data *obj, const ON_Brep *brep);
void subbrep_island_free(struct subbrep_island_data *obj);
void subbrep_shoal_init(struct subbrep_shoal_data *obj, struct subbrep_island_data *island);
void subbrep_shoal_free(struct subbrep_shoal_data *obj);
void csg_object_params_init(struct csg_object_params *obj, struct subbrep_shoal_data *shoal);
void csg_object_params_free(struct csg_object_params *obj);

// TODO - will this become a general pattern with primitive specific sub-functions?
int cylinder_csg(struct bu_vls *msgs, struct subbrep_shoal_data *data, fastf_t cyl_tol);


int subbrep_brep_boolean(struct subbrep_island_data *data);


//int subbrep_split(struct bu_vls *msgs, struct subbrep_island_data *data);
//int subbrep_make_planar(struct subbrep_island_data *data);




// Functions for defining a simplified planar subvolume
//void subbrep_planar_init(struct subbrep_island_data *data);
//void subbrep_planar_close_obj(struct subbrep_island_data *data);
//void subbrep_add_planar_face(struct subbrep_island_data *data, ON_Plane *pcyl, ON_SimpleArray<const ON_BrepVertex *> *vert_loop, int neg_surf);

//struct bu_ptbl *find_subbreps(const ON_Brep *brep);
//struct bu_ptbl *find_top_level_hierarchy(struct bu_ptbl *subbreps);
void print_subbrep_object(struct subbrep_island_data *data, const char *offset);

//volume_t subbrep_shape_recognize(struct bu_vls *msgs, struct subbrep_island_data *data);

//int subbrep_is_planar(struct bu_vls *msgs, struct subbrep_island_data *data);

//int cylindrical_loop_planar_vertices(ON_BrepFace *face, int loop_index);
//int subbrep_is_cylinder(struct bu_vls *msgs, struct subbrep_island_data *data, fastf_t cyl_tol);

//int subbrep_is_cone(struct bu_vls *msgs, struct subbrep_island_data *data, fastf_t cone_tol);
//int cone_csg(struct bu_vls *msgs, struct subbrep_island_data *data, fastf_t cone_tol);

//int sphere_csg(struct subbrep_island_data *data, fastf_t cone_tol);

//int torus_csg(struct subbrep_island_data *data, fastf_t cone_tol);


//int subbrep_find_corners(struct subbrep_island_data *data, int **corner_verts_array, ON_Plane *pcyl);
//int subbrep_top_bottom_pnts(struct subbrep_island_data *data, std::set<int> *corner_verts, ON_Plane *top_plane, ON_Plane *bottom_plane, ON_SimpleArray<const ON_BrepVertex *> *top_pnts, ON_SimpleArray<const ON_BrepVertex *> *bottom_pnts);


//int subbrep_polygon_tri(const ON_Brep *brep, const point_t *all_verts, int *loops, int loop_cnt, int **ffaces);

#endif /* SHAPE_RECOGNITION_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
