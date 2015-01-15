#include "common.h"
#include "bu/ptbl.h"
#include "brep.h"

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
    PLANAR_VOLUME,
    CYLINDER,
    CONE,
    SPHERE,
    ELLIPSOID,
    TORUS,
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
surface_t GetSurfaceType(ON_Surface *surface);



/* Structure for holding parameters corresponding
 * to a csg primitive.  Not all parameters will be
 * used for all primitives - the structure includes
 * enough data slots to describe any primitive that may
 * be matched by the shape recognition logic */
struct csg_object_params {
    point_t origin;
    vect_t hv;
    fastf_t radius;
    fastf_t height;
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

    ON_Brep *brep;
    volume_t type;
    csg_object_params *params;
    subbrep_object_data *parent;
    struct bu_ptbl *children;
};

void subbrep_object_init(struct subbrep_object_data *obj, ON_Brep *brep);
void subbrep_object_free(struct subbrep_object_data *obj);

int cylindrical_loop_planar_vertices(struct subbrep_object_data *data, int face_index);
int cylindrical_planar_vertices(struct subbrep_object_data *data, int face_index);

int subbrep_split(struct subbrep_object_data *data);

struct bu_ptbl *find_subbreps(ON_Brep *brep);
void print_subbrep_object(struct subbrep_object_data *data, const char *offset);
volume_t subbrep_shape_recognize(struct subbrep_object_data *data);


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
