#include "common.h"
#include "bu/ptbl.h"
#include "brep.h"

#ifndef SHAPE_RECOGNITION_H
#define SHAPE_RECOGNITION_H

#define BREP_PLANAR_TOL 0.05
#define BREP_CYLINDRICAL_TOL 0.05
#define BREP_CONIC_TOL 0.05
#define BREP_SPHERICAL_TOL 0.05
#define BREP_ELLIPSOIDAL_TOL 0.05
#define BREP_TOROIDAL_TOL 0.05

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

struct filter_obj {
    ON_Plane* plane;
    ON_Sphere* sphere;
    ON_Cylinder* cylinder;
    ON_Cone* cone;
    ON_Torus* torus;
    surface_t stype;
    volume_t type;
};
void filter_obj_init(struct filter_obj *);
void filter_obj_free(struct filter_obj *);

curve_t GetCurveType(ON_Curve *curve);
surface_t GetSurfaceType(const ON_Surface *surface, struct filter_obj *obj);


void set_to_array(int **array, int *array_cnt, std::set<int> *set);
void array_to_set(std::set<int> *set, int *array, int array_cnt);


/* Structure for holding parameters corresponding
 * to a csg primitive.  Not all parameters will be
 * used for all primitives - the structure includes
 * enough data slots to describe any primitive that may
 * be matched by the shape recognition logic */
struct csg_object_params {
    volume_t type;
    char bool_op; /* Boolean operator - u = union (default), - = subtraction, + = intersection */
    struct csg_object_params *sub_object; // if a comb needs to be subtracted...
    point_t origin;
    vect_t hv;
    fastf_t radius;
    fastf_t r2;
    fastf_t height;
    point_t p[8];
};

struct subbrep_object_data {
    struct bu_vls *key;
    int *faces;
    int *loops;
    int *edges;
    int *fol; /* Faces with outer loops in object loop network */
    int *fil; /* Faces with only inner loops in object loop network */
    int faces_cnt;
    int loops_cnt;
    int edges_cnt;
    int fol_cnt;
    int fil_cnt;

    const ON_Brep *brep;
    ON_Brep *local_brep;
    volume_t type;
    struct bu_ptbl *objs;
    subbrep_object_data *parent;
    struct bu_ptbl *children;
};

void subbrep_object_init(struct subbrep_object_data *obj, const ON_Brep *brep);
void subbrep_object_free(struct subbrep_object_data *obj);

int subbrep_split(struct subbrep_object_data *data);
int subbrep_make_brep(struct subbrep_object_data *data);

struct bu_ptbl *find_subbreps(const ON_Brep *brep);
void print_subbrep_object(struct subbrep_object_data *data, const char *offset);
volume_t subbrep_shape_recognize(struct subbrep_object_data *data);

void
subbrep_remove_degenerate_edges(struct subbrep_object_data *data, std::set<int> *edges);

int subbrep_is_planar(struct subbrep_object_data *data);

int cylindrical_loop_planar_vertices(ON_BrepFace *face, int loop_index);
int subbrep_is_cylinder(struct subbrep_object_data *data, fastf_t cyl_tol);
int cylinder_csg(struct subbrep_object_data *data, fastf_t cyl_tol);

int subbrep_is_cone(struct subbrep_object_data *data, fastf_t cone_tol);
int cone_csg(struct subbrep_object_data *data, fastf_t cone_tol);

std::string face_set_key(std::set<int> fset);
surface_t highest_order_face(struct subbrep_object_data *data);
void set_to_array(int **array, int *array_cnt, std::set<int> *set);
void array_to_set(std::set<int> *set, int *array, int array_cnt);


#endif /* SHAPE_RECOGNITION_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
