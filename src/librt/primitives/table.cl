#include "common.cl"

constant double rti_tol_dist = 0.0005;

constant double3 lt_color = {1, 1, 1};
constant double3 ambient_color = {1, 1, 1};     /* Ambient white light */

constant double AmbientIntensity = 0.4;

constant uchar3 ibackground = {0, 0, 50};	/* integer 0..255 version */
constant uchar3 inonbackground = {0, 0, 51};	/* integer non-background */
constant uchar3 iblack = {0, 0, 0};		/* integer black */


extern inline double3 MAT3X3VEC(global const double *m, double3 i);
extern inline double3 MAT4X3VEC(global const double *m, double3 i);


/*
 * Values for Solid ID.
 */
#define ID_TOR          1       /**< @brief Toroid */
#define ID_TGC          2       /**< @brief Generalized Truncated General Cone */
#define ID_ELL          3       /**< @brief Ellipsoid */
#define ID_ARB8         4       /**< @brief Generalized ARB.  V + 7 vectors */
#define ID_REC          7       /**< @brief Right Elliptical Cylinder [TGC special] */
#define ID_SPH          10      /**< @brief Sphere */
#define ID_EHY          20      /**< @brief Elliptical Hyperboloid  */


inline int shot(global struct hit *res, const double3 r_pt, const double3 r_dir, const uint idx, const int id, global const void *args)
{
    switch (id) {
    case ID_TOR:	return tor_shot(res, r_pt, r_dir, idx, args);
    case ID_TGC:	return tgc_shot(res, r_pt, r_dir, idx, args);
    case ID_ELL:	return ell_shot(res, r_pt, r_dir, idx, args);
    case ID_ARB8:	return arb_shot(res, r_pt, r_dir, idx, args);
    case ID_REC:	return rec_shot(res, r_pt, r_dir, idx, args);
    case ID_SPH:	return sph_shot(res, r_pt, r_dir, idx, args);
    case ID_EHY:	return ehy_shot(res, r_pt, r_dir, idx, args);
    default:		return 0;
    };
}

inline void norm(global struct hit *hitp, const double3 r_pt, const double3 r_dir, const int id, global const void *args)
{
    switch (id) {
    case ID_TOR:	tor_norm(hitp, r_pt, r_dir, args);	break;
    case ID_TGC:	tgc_norm(hitp, r_pt, r_dir, args);	break;
    case ID_ELL:	ell_norm(hitp, r_pt, r_dir, args);	break;
    case ID_ARB8:	arb_norm(hitp, r_pt, r_dir, args);	break;
    case ID_REC:	rec_norm(hitp, r_pt, r_dir, args);	break;
    case ID_EHY:	ehy_norm(hitp, r_pt, r_dir, args);	break;
    case ID_SPH:	sph_norm(hitp, r_pt, r_dir, args);	break;
    default:							break;
    };
}


__kernel void
solid_norm(global struct hit *hitp, const double3 r_pt, const double3 r_dir, const int id, global const void *args)
{
    norm(hitp, r_pt, r_dir, id, args);
}

__kernel void
solid_shot(global int *len, global struct hit *res, const double3 r_pt, const double3 r_dir, const int id, global const void *args)
{
    *len = shot(res, r_pt, r_dir, UINT_MAX, id, args);
}

__kernel void
db_solid_shot(global int *len, global struct hit *res, const double3 r_pt, const double3 r_dir, const uint idx, global int *ids, global uint *indexes, global char *prims)
{
    global const void *args;
    args = prims + indexes[idx];
    *len = shot(res, r_pt, r_dir, idx, ids[idx], args);
}


void do_hitp(global struct hit *res, const uint i, const uint hit_index, const struct hit *hitp)
{
    if (res) {
#ifdef RT_SINGLE_HIT
        if (hitp->hit_dist < res->hit_dist) {
            *res = *hitp;
            res->hit_index = hit_index;
        }
#else
        res[i] = *hitp;
#ednfi
    }
}


inline void gen_ray(double3 *r_pt, double3 *r_dir, const int a_x, const int a_y,
		    const double16 view2model, const double cell_width, const double cell_height, const double aspect)
{
    double3 tmp;
    double f;

    /* create basis vectors dx and dy for emanation plane (grid) */
    tmp = (double3){1.0, 0.0, 0.0};
    const double3 dx_unit = view2model.s048*tmp.x + view2model.s159*tmp.y + view2model.s26a*tmp.z;
    const double3 dx_model = dx_unit * cell_width;

    tmp = (double3){0.0, 1.0, 0.0};
    const double3 dy_unit = view2model.s048*tmp.x + view2model.s159*tmp.y + view2model.s26a*tmp.z;
    const double3 dy_model = dy_unit * cell_height;

    /* "lower left" corner of viewing plane.  all rays go this direction */
    tmp = (double3){0.0, 0.0, -1.0};
    f = 1.0/view2model.sf;
    *r_dir = normalize((view2model.s048*tmp.x + view2model.s159*tmp.y + view2model.s26a*tmp.z)*f);

    tmp = (double3){-1.0, -1.0/aspect, 0.0};	// eye plane
    double3 viewbase_model;
    f = 1.0/(dot(view2model.scde, tmp) + view2model.sf);
    viewbase_model = (view2model.s048*tmp.x + view2model.s159*tmp.y + view2model.s26a*tmp.z + view2model.s37b)*f;

    /* our starting point, used for non-jitter */
    *r_pt = viewbase_model + dx_model * a_x + dy_model * a_y;
}


__kernel void
do_pixel(global unsigned char *pixels, global struct hit *hits, 
         const uint pwidth, const int cur_pixel, const int last_pixel, const int width,
         const double16 view2model, const double cell_width, const double cell_height, const double aspect,
         const int lightmodel,
         const uint nprims, global int *ids, global uint *indexes, global char *prims)
{
    const int pixelnum = cur_pixel+get_global_id(0);

    const int a_y = (int)(pixelnum/width);
    const int a_x = (int)(pixelnum - (a_y * width));

    double3 r_pt, r_dir;
    gen_ray(&r_pt, &r_dir, a_x, a_y, view2model, cell_width, cell_height, aspect);

    global struct hit *hitp = hits+(a_y*width+a_x);
    hitp->hit_dist = INFINITY;

    int ret = 0;
    for (uint idx=0; idx<nprims; idx++) {
        ret += shot(hitp, r_pt, r_dir, idx, ids[idx], prims + indexes[idx]);
    }

    double3 a_color;

    if (ret != 0) {
        double diffuse0 = 0;
        double cosI0 = 0;
        double3 work0, work1;
        double3 normal;

        const uint idx = hitp->hit_index;
        if (idx<nprims) {
            norm(hitp, r_pt, r_dir, ids[idx], prims + indexes[idx]);
            normal = hitp->hit_normal;
        }

        /*
         * Diffuse reflectance from each light source
         */
        switch (lightmodel) {
            default:
                /* Light from the "eye" (ray source).  Note sign change */
                diffuse0 = 0;
                if ((cosI0 = -dot(normal, r_dir)) >= 0.0)
                    diffuse0 = cosI0 * (1.0 - AmbientIntensity);
                work0 = lt_color * diffuse0;

                /* Add in contribution from ambient light */
                work1 = ambient_color * AmbientIntensity;
                a_color = work0 + work1;
                break;
            case 2:
                /* Store surface normals pointing inwards */
                /* (For Spencer's moving light program) */
                a_color = (normal * (-.5)) + .5;
                break;
        }
    }

    uchar3 rgb;
    if (ret <= 0) {
	/* shot missed the model, don't dither */
        rgb = ibackground;
	a_color = -1e-20;	// background flag
    } else {
        rgb = convert_uchar3_sat(a_color*255);

	if (all(rgb == ibackground)) {
            rgb = inonbackground;
	} else if (all(rgb == iblack)) { // make sure it's never perfect black
            rgb.z = 1;
	}
    }

    global unsigned char *pixelp = pixels+pwidth*(a_y*width+a_x);
    vstore3(rgb, 0, pixelp);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
