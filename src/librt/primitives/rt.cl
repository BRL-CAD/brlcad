#include "common.cl"

constant double rti_tol_dist = 0.0005;

constant double3 lt_color = {1, 1, 1};
constant double3 ambient_color = {1, 1, 1};     /* Ambient white light */

constant double AmbientIntensity = 0.4;


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


#define RT_SINGLE_HIT 1
void do_hitp(global struct hit *res, const uint i, const uint hit_index, const struct hit *hitp)
{
    if (res) {
#ifdef RT_SINGLE_HIT
        if (hitp->hit_dist >= 0.0 && hitp->hit_dist < res->hit_dist) {
            *res = *hitp;
            res->hit_index = hit_index;
        }
#else
        res[i] = *hitp;
#endif
    }
}


inline void gen_ray(double3 *r_pt, double3 *r_dir, double3 *r_idir, const int a_x, const int a_y,
		    const double16 view2model, const double cell_width, const double cell_height, const double aspect)
{
    double3 tmp, dir;
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
    dir = normalize((view2model.s048*tmp.x + view2model.s159*tmp.y + view2model.s26a*tmp.z)*f);

    tmp = (double3){-1.0, -1.0/aspect, 0.0};	// eye plane
    double3 viewbase_model;
    f = 1.0/(dot(view2model.scde, tmp) + view2model.sf);
    viewbase_model = (view2model.s048*tmp.x + view2model.s159*tmp.y + view2model.s26a*tmp.z + view2model.s37b)*f;

    /* our starting point, used for non-jitter */
    *r_pt = viewbase_model + dx_model * a_x + dy_model * a_y;

    double idir[3];
    if (dir.x < -SQRT_SMALL_FASTF) {
        idir[0] = 1.0/dir.x;
    } else if (dir.x > SQRT_SMALL_FASTF) {
        idir[0] = 1.0/dir.x;
    } else {
        dir.x = 0.0;
        idir[0] = INFINITY;
    }
    if (dir.y < -SQRT_SMALL_FASTF) {
        idir[1] = 1.0/dir.y;
    } else if (dir.y > SQRT_SMALL_FASTF) {
        idir[1] = 1.0/dir.y;
    } else {
        dir.y = 0.0;
        idir[1] = INFINITY;
    }
    if (dir.z < -SQRT_SMALL_FASTF) {
        idir[2] = 1.0/dir.z;
    } else if (dir.z > SQRT_SMALL_FASTF) {
        idir[2] = 1.0/dir.z;
    } else {
        dir.z = 0.0;
        idir[2] = INFINITY;
    }
    *r_dir = dir;
    *r_idir = vload3(0, idir);
}


struct bvh_bounds {
    double p_min[3], p_max[3];
};

struct linear_bvh_node {
    struct bvh_bounds bounds;
    union {
        int primitives_offset;		/* leaf */
        int second_child_offset;	/* interior */
    } u;
    ushort n_primitives;		/* 0 -> interior node */
    uchar axis;				/* interior node: xyz */
    uchar pad[1];			/* ensure 32 byte total size */
};

inline int
rt_in_rpp(const double3 pt,
	  const double3 invdir,
	  global const double *min,
	  global const double *max)
{
    /* Start with infinite ray, and trim it down */
    double x0 = (min[0] - pt.x) * invdir.x;
    double y0 = (min[1] - pt.y) * invdir.y;
    double z0 = (min[2] - pt.z) * invdir.z;
    double x1 = (max[0] - pt.x) * invdir.x;
    double y1 = (max[1] - pt.y) * invdir.y;
    double z1 = (max[2] - pt.z) * invdir.z;

    /*
     * Direction cosines along this axis is NEAR 0,
     * which implies that the ray is perpendicular to the axis,
     * so merely check position against the boundaries.
     */
    double rmin = -MAX_FASTF;
    double rmax =  MAX_FASTF;

    rmin = fmax(rmin, fmax(fmax(fmin(x0, x1), fmin(y0, y1)), fmin(z0, z1)));
    rmax = fmin(rmax, fmin(fmin(fmax(x0, x1), fmax(y0, y1)), fmax(z0, z1)));

    /* If equal, RPP is actually a plane */
    return (rmin <= rmax);
}

int
shootray(global struct hit *hitp, const double3 r_pt, const double3 r_dir, const double3 r_idir,
         const uint nprims, global int *ids, global struct linear_bvh_node *nodes,
         global uint *indexes, global uchar *prims)
{
    int ret = 0;

    if (nodes) {
        uchar dir_is_neg[3];
        int to_visit_offset = 0, current_node_index = 0;
        int nodes_to_visit[64];

	vstore3(convert_uchar(r_idir < 0), 0, dir_is_neg);

        /* Follow ray through BVH nodes to find primitive intersections */
        for (;;) {
            const global struct linear_bvh_node *node = &nodes[current_node_index];
            const global struct bvh_bounds *bounds = &node->bounds;

            /* Check ray against BVH node */
            if (rt_in_rpp(r_pt, r_idir, bounds->p_min, bounds->p_max)) {
                if (node->n_primitives > 0) {
                    /* Intersect ray with primitives in leaf BVH node */
                    for (int i = 0; i < node->n_primitives; ++i) {
                        const uint idx = node->u.primitives_offset + i;
                        ret += shot(hitp, r_pt, r_dir, idx, ids[idx], prims + indexes[idx]);
                    }
                    if (to_visit_offset == 0) break;
                    current_node_index = nodes_to_visit[--to_visit_offset];
                } else {
                    /* Put far BVH node on nodes_to_visit stack, advance to near
                     * node
                     */
                    if (dir_is_neg[node->axis]) {
                        nodes_to_visit[to_visit_offset++] = current_node_index + 1;
                        current_node_index = node->u.second_child_offset;
                    } else {
                        nodes_to_visit[to_visit_offset++] = node->u.second_child_offset;
                        current_node_index = current_node_index + 1;
                    }
                }
            } else {
                if (to_visit_offset == 0) break;
                current_node_index = nodes_to_visit[--to_visit_offset];
            }
        }
    } else {
        for (uint idx=0; idx<nprims; idx++) {
            ret += shot(hitp, r_pt, r_dir, idx, ids[idx], prims + indexes[idx]);
        }
    }
    return ret;
}

inline double3
bu_rand0to1(const uint id, global float *bnrandhalftab, const uint randhalftabsize)
{
    double3 ret;
    ret.x = (bnrandhalftab[(id*3+0) % randhalftabsize]+0.5);
    ret.y = (bnrandhalftab[(id*3+1) % randhalftabsize]+0.5);
    ret.z = (bnrandhalftab[(id*3+2) % randhalftabsize]+0.5);
    return ret;
}

inline ulong
bu_cv_htond(const ulong d)
{
    return ((d & 0xFF00000000000000UL) >> 56)
	 | ((d & 0x00FF000000000000UL) >> 40)
	 | ((d & 0x0000FF0000000000UL) >> 24)
	 | ((d & 0x000000FF00000000UL) >>  8)
	 | ((d & 0x00000000FF000000UL) <<  8)
	 | ((d & 0x0000000000FF0000UL) << 24)
	 | ((d & 0x000000000000FF00UL) << 40)
	 | ((d & 0x00000000000000FFUL) << 56);
}

__kernel void
do_pixel(global uchar *pixels, const uchar3 o, global struct hit *hits,
         const int cur_pixel, const int last_pixel, const int width,
	 global float *rand_halftab, const uint randhalftabsize,
	 const uchar3 background, const uchar3 nonbackground,
	 const double16 view2model, const double cell_width, const double cell_height,
	 const double aspect, const int lightmodel, const uint nprims, global int *ids,
	 global struct linear_bvh_node *nodes, global uint *indexes, global uchar *prims)
{
    const size_t id = get_global_id(0);
    const int pixelnum = cur_pixel+id;

    const int a_y = (int)(pixelnum/width);
    const int a_x = (int)(pixelnum - (a_y * width));

    double3 r_pt, r_dir, r_idir;
    gen_ray(&r_pt, &r_dir, &r_idir, a_x,a_y, view2model, cell_width, cell_height, aspect);

    global struct hit *hitp = hits+id;
    hitp->hit_dist = INFINITY;

    int ret = shootray(hitp, r_pt, r_dir, r_idir, nprims, ids, nodes, indexes, prims);

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
        rgb = background;
	a_color = -1e-20;	// background flag
    } else {
        double3 t_color;

	/*
	 * To prevent bad color aliasing, add some color dither.  Be
	 * certain to NOT output the background color here.  Random
	 * numbers in the range 0 to 1 are used, so that integer
	 * valued colors (e.g., from texture maps) retain their original
	 * values.
	 */
        t_color = a_color*255.+bu_rand0to1(id, rand_halftab, randhalftabsize);
	rgb = convert_uchar3_sat(t_color);


	rgb = select(rgb, nonbackground, (uchar3)all(rgb == background));
	// make sure it's never perfect black
	rgb = select(rgb, (uchar3){rgb.xy, 1}, (uchar3)all(!rgb));
    }

    if (o.s0 != o.s1) {
	/* write color */
	global uchar *colorp = pixels+id*o.s2+o.s0;
	vstore3(rgb, 0, colorp);
    }
    if (o.s1 != o.s2) {
	/* write depth */
	ulong depth = bu_cv_htond(as_ulong(hitp->hit_dist));
	global ulong *depthp = pixels+id*o.s2+o.s1;
	*depthp = depth;
    }
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
