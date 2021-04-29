#include "common.cl"

constant double rti_tol_dist = 0.0005;

constant double3 lt_color = {1, 1, 1};
constant double3 ambient_color = {1, 1, 1};     /* Ambient white light */

constant double AmbientIntensity = 0.4;


double3 MAT3X3VEC(global const double *m, double3 i)
{
    double3 o;
    o.x = dot(vload3(0, &m[0]), i);
    o.y = dot(vload3(0, &m[4]), i);
    o.z = dot(vload3(0, &m[8]), i);
    return o;
}

double3 MAT4X3VEC(global const double *m, double3 i)
{
    double3 o;
    o = MAT3X3VEC(m, i) * (DOUBLE_C(1.0)/m[15]);
    return o;
}


static double3
bu_rand0to1(const uint id, global float *bnrandhalftab, const uint randhalftabsize)
{
    double3 ret;
    ret.x = (bnrandhalftab[(id*3+0) % randhalftabsize]+0.5);
    ret.y = (bnrandhalftab[(id*3+1) % randhalftabsize]+0.5);
    ret.z = (bnrandhalftab[(id*3+2) % randhalftabsize]+0.5);
    return ret;
}

static ulong
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


bool
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

bool
rt_in_rpp2(const double3 pt, const double3 invdir,
	   global const double *min, global const double *max,
	   double *rmin, double *rmax)
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
    *rmin = fmax(-MAX_FASTF, fmax(fmax(fmin(x0, x1), fmin(y0, y1)), fmin(z0, z1)));
    *rmax = fmin( MAX_FASTF, fmin(fmin(fmax(x0, x1), fmax(y0, y1)), fmax(z0, z1)));

    /* If equal, RPP is actually a plane */
    return (*rmin <= *rmax);
}


/*
 * Values for Solid ID.
 */
#define ID_TOR          1       /**< @brief Toroid */
#define ID_TGC          2       /**< @brief Generalized Truncated General Cone */
#define ID_ELL          3       /**< @brief Ellipsoid */
#define ID_ARB8         4       /**< @brief Generalized ARB.  V + 7 vectors */
#define ID_ARS          5       /**< @brief ARS */
#define ID_REC          7       /**< @brief Right Elliptical Cylinder [TGC special] */
#define ID_SPH          10      /**< @brief Sphere */
#define ID_EBM          12      /**< @brief Extruded bitmap solid */
#define ID_PARTICLE     16      /**< @brief Particle system solid */
#define ID_RPC          17      /**< @brief Right Parabolic Cylinder [TGC special] */
#define ID_RHC          18      /**< @brief Right Hyperbolic Cylinder [TGC special] */
#define ID_EPA          19      /**< @brief Elliptical Paraboloid */
#define ID_EHY          20      /**< @brief Elliptical Hyperboloid  */
#define ID_ETO          21      /**< @brief Elliptical Torus  */
#define ID_BOT          30      /**< @brief Bag o' triangles */
#define ID_SUPERELL	35      /**< @brief Superellipsoid */
#define ID_HYP		38      /**< @brief Hyperboloid */
#define ID_HRT          43      /**< @brief Heart */


inline int shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, const int id, global const void *args)
{
    switch (id) {
    case ID_TOR:	return tor_shot(res, r_pt, r_dir, idx, args);
    case ID_TGC:	return tgc_shot(res, r_pt, r_dir, idx, args);
    case ID_ELL:	return ell_shot(res, r_pt, r_dir, idx, args);
    case ID_ARB8:	return arb_shot(res, r_pt, r_dir, idx, args);
    case ID_REC:	return rec_shot(res, r_pt, r_dir, idx, args);
    case ID_SPH:	return sph_shot(res, r_pt, r_dir, idx, args);
    case ID_EBM:	return ebm_shot(res, r_pt, r_dir, idx, args);
    case ID_PARTICLE:	return part_shot(res, r_pt, r_dir, idx, args);
    case ID_EHY:	return ehy_shot(res, r_pt, r_dir, idx, args);
    case ID_ARS:
    case ID_BOT:	return bot_shot(res, r_pt, r_dir, idx, args);
    case ID_EPA:	return epa_shot(res, r_pt, r_dir, idx, args);
    case ID_ETO:	return eto_shot(res, r_pt, r_dir, idx, args);
    case ID_RHC:	return rhc_shot(res, r_pt, r_dir, idx, args);
    case ID_RPC:	return rpc_shot(res, r_pt, r_dir, idx, args);
    case ID_HRT:	return hrt_shot(res, r_pt, r_dir, idx, args);
    case ID_SUPERELL:	return superell_shot(res, r_pt, r_dir, idx, args);
    case ID_HYP:	return hyp_shot(res, r_pt, r_dir, idx, args);
    default:		return 0;
    };
}

inline void norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, const int id, global const void *args)
{
    switch (id) {
    case ID_TOR:	tor_norm(hitp, r_pt, r_dir, args);	break;
    case ID_TGC:	tgc_norm(hitp, r_pt, r_dir, args);	break;
    case ID_ELL:	ell_norm(hitp, r_pt, r_dir, args);	break;
    case ID_ARB8:	arb_norm(hitp, r_pt, r_dir, args);	break;
    case ID_REC:	rec_norm(hitp, r_pt, r_dir, args);	break;
    case ID_EHY:	ehy_norm(hitp, r_pt, r_dir, args);	break;
    case ID_SPH:	sph_norm(hitp, r_pt, r_dir, args);	break;
    case ID_EBM:	ebm_norm(hitp, r_pt, r_dir, args);	break;
    case ID_PARTICLE:	part_norm(hitp, r_pt, r_dir, args);	break;
    case ID_ARS:
    case ID_BOT:	bot_norm(hitp, r_pt, r_dir, args);	break;
    case ID_EPA:	epa_norm(hitp, r_pt, r_dir, args);	break;
    case ID_ETO:	eto_norm(hitp, r_pt, r_dir, args);	break;
    case ID_RHC:	rhc_norm(hitp, r_pt, r_dir, args);	break;
    case ID_RPC:	rpc_norm(hitp, r_pt, r_dir, args);	break;
    case ID_HRT:	hrt_norm(hitp, r_pt, r_dir, args);	break;
    case ID_SUPERELL:	superell_norm(hitp, r_pt, r_dir, args);	break;
    case ID_HYP:	hyp_norm(hitp, r_pt, r_dir, args);	break;
    default:							break;
    };
}


inline void gen_ray(double3 *r_pt, double3 *r_dir, const int a_x, const int a_y,
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
    *r_dir = dir;
}

int
shootray(RESULT_TYPE *res, double3 r_pt, double3 r_dir,
         const uint nprims, global uchar *ids, global struct linear_bvh_node *nodes,
         global uint *indexes, global uchar *prims)
{
    int ret = 0;

    const double3 r_idir = select(INFINITY, DOUBLE_C(1.0)/r_dir, isgreaterequal(fabs(r_dir), SQRT_SMALL_FASTF));
    r_dir = select(0, r_dir, isgreaterequal(fabs(r_dir), SQRT_SMALL_FASTF));

    if (nodes) {
        uchar dir_is_neg[3];
        int to_visit_offset = 0, current_node_index = 0;
        int nodes_to_visit[64];

	vstore3(convert_uchar3(r_idir < 0), 0, dir_is_neg);

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
                        ret += shot(res, r_pt, r_dir, idx, ids[idx], prims + indexes[idx]);
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
            ret += shot(res, r_pt, r_dir, idx, ids[idx], prims + indexes[idx]);
        }
    }
    return ret;
}


struct shade_work {
    double3 sw_color;		/**< @brief  shaded color */
    double3 sw_basecolor;	/**< @brief  base color */
};

/*
 * Values for Shader Function ID.
 */
#define SH_NONE 	0
#define SH_PHONG	1

struct phong_specific {
    double wgt_specular;
    double wgt_diffuse;
    int shine;
};

/**
 * The region structure.
 */
struct region {
    float color[3];		/**< @brief explicit color:  0..1  */
    int mf_id;
    union {
	struct phong_specific phg_spec;
    }udata;
};

static inline
double3 reflect(double3 I, double3 N)
{
    return I - dot(N, I) * N * 2;
}

void
phong_render(struct shade_work *swp, double3 r_dir, double3 normal, double3 to_light, global const struct phong_specific *sp)
{
    double3 matcolor;

    matcolor = swp->sw_color;

    double3 L = normalize(to_light);

    /* Diffuse reflectance from "Ambient" light source (at eye) */
    double ambient = max(-dot(normal, r_dir), DOUBLE_C(0.0));
    double diffuse = max(dot(L, normal), DOUBLE_C(0.0));
    double specular = 0;

    if (diffuse > 0.0) {
        double3 R = reflect(-r_dir, normal);
        double NH = max(dot(R, r_dir), DOUBLE_C(0.0));
        specular = pown(NH, sp->shine);
    }
    swp->sw_color = matcolor * AmbientIntensity * ambient
        + matcolor * lt_color * sp->wgt_diffuse * diffuse
        + matcolor * lt_color * sp->wgt_specular * specular;
}


inline void
viewshade(struct shade_work *swp, const double3 r_dir, const double3 normal, const double3 to_light, global const struct region *rp)
{
    swp->sw_color = convert_double3(vload3(0, rp->color));
    swp->sw_basecolor = swp->sw_color;

    switch (rp->mf_id) {
	case SH_PHONG:
		phong_render(swp, r_dir, normal, to_light, &rp->udata.phg_spec);
		break;
	default:
		break;
    }
}

double3 MAT4X3PNT(const double16 m, double3 i) {
    double4 j;

    j.xyz = i;
    j.w = (1.0/(m.sc*i.x+m.sd*i.y+m.se*i.z+m.sf));

    double3 o;
    o.x = dot(m.s0123, j);
    o.y = dot(m.s4567, j);
    o.z = dot(m.s98ab, j);
    return o;
}

inline double3
shade(const double3 r_pt, const double3 r_dir, struct hit *hitp, const double3 lt_pos, const uint idx, global struct region *regions)
{
    double3 color;
    double3 normal;

    if (hitp->hit_dist < 0.0) {
	/* Eye inside solid, orthoview */
	normal = -r_dir;
    } else {
	normal = hitp->hit_normal;
    }

    /*
     * Diffuse reflectance from each light source
     */
    const double3 to_light = lt_pos - hitp->hit_point;

    struct shade_work sw;
    sw.sw_color = 1.0;
    sw.sw_basecolor = 1.0;

    viewshade(&sw, r_dir, normal, to_light, &regions[idx]);
    color = sw.sw_color;
    return color;
}


#if RT_SINGLE_HIT
/**
  * Single-hit.
  */
void do_segp(RESULT_TYPE *res, const uint idx,
	     struct hit *seg_in, struct hit *seg_out)
{
    struct accum *acc = *res;
    struct seg *segp = &acc->s;

    if (seg_in->hit_dist < segp->seg_in.hit_dist) {
	segp->seg_in = *seg_in;
	segp->seg_sti = idx;
    }

    if (acc->lightmodel == 4) {
	if (seg_in->hit_dist >= 0.0) {
	    norm(seg_in, acc->r_pt, acc->r_dir, acc->ids[idx], acc->prims + acc->indexes[idx]);
	    const double3 color = shade(acc->r_pt, acc->r_dir, seg_in, acc->lt_pos, acc->iregions[idx], acc->regions);
	    double f = exp(-seg_in->hit_dist*1e-10);
	    acc->a_color += color * f;
	    acc->a_total += f;
	}
    }
}


__kernel void
do_pixel(global uchar *pixels, const uchar2 o, const int cur_pixel,
	 const int last_pixel, const int width, global float *rand_halftab,
	 const uint randhalftabsize, const uchar3 background,const uchar3 nonbackground,
	 const double airdensity, const double3 haze, const double gamma,
	 const double16 view2model, const double cell_width, const double cell_height,
	 const double aspect, const int lightmodel, const uint nprims, global uchar *ids,
	 global struct linear_bvh_node *nodes, global uint *indexes,global uchar *prims,
	 global struct region *regions, global int *iregions)
{
    const size_t id = get_global_size(0)*get_global_id(1)+get_global_id(0);

    if (id >= (last_pixel-cur_pixel+1))
      return;

    const int pixelnum = cur_pixel+id;

    const int a_y = (int)(pixelnum/width);
    const int a_x = (int)(pixelnum - (a_y * width));

    double3 r_pt, r_dir;
    gen_ray(&r_pt, &r_dir, a_x, a_y, view2model, cell_width, cell_height, aspect);

    /* Determine the Light location(s) in view space */
    /* 0:  At left edge, 1/2 high */
    const double3 lt_pos = MAT4X3PNT(view2model, (double3){-1,0,1});

    struct accum a;
    a.lightmodel = lightmodel;
    a.r_pt = r_pt;
    a.r_dir = r_dir;
    a.lt_pos = lt_pos;
    a.a_color = 0.0;
    a.a_total = 0.0;
    a.ids = ids;
    a.indexes = indexes;
    a.prims = prims;
    a.regions = regions;
    a.iregions = iregions;

    struct seg *segp = &a.s;
    segp->seg_in.hit_dist = INFINITY;
    segp->seg_out.hit_dist = -INFINITY;
    segp->seg_sti = 0;

    int ret = shootray(&segp, r_pt, r_dir, nprims, ids, nodes, indexes, prims);

    double3 a_color;
    uchar3 rgb;

    struct hit *hitp;
    hitp = &segp->seg_in;

    if (ret != 0) {
        double diffuse0 = 0;
        double cosI0 = 0;
        double3 work0, work1;
        double3 normal;
	const uint idx = segp->seg_sti;

	if (lightmodel != 4) {
	    if (hitp->hit_dist < 0.0) {
		/* Eye inside solid, orthoview */
		normal = -r_dir;
	    } else {
		norm(hitp, r_pt, r_dir, ids[idx], prims + indexes[idx]);
		normal = hitp->hit_normal;
	    }
	}

        /*
         * Diffuse reflectance from each light source
         */
        switch (lightmodel) {
	    default:
		a_color = shade(r_pt, r_dir, hitp, lt_pos, iregions[idx], regions);
	    	break;
            case 1:
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
                a_color = (normal * DOUBLE_C(-.5)) + DOUBLE_C(.5);
                break;
	    case 4:
	    	a_color = a.a_color * (1.0/a.a_total);
		break;
        }

        /*
         * e ^(-density * distance)
         */
        if (!ZERO(airdensity)) {
            double g;
            double f = exp(-hitp->hit_dist * airdensity);
            g = (1.0 - f);
            a_color = a_color * f + haze * g;
        }

        double3 t_color;

        /*
         * To prevent bad color aliasing, add some color dither.  Be
         * certain to NOT output the background color here.  Random
         * numbers in the range 0 to 1 are used, so that integer
         * valued colors (e.g., from texture maps) retain their original
         * values.
         */
        if (!ZERO(gamma)) {
            /*
             * Perform gamma correction in floating-point space, and
             * avoid nasty mach bands in dark areas from doing it in
             * 0..255 space later.
             */
            const double ex = 1.0/gamma;
	    t_color = floor(pow(a_color, ex) * DOUBLE_C(255.) +
			bu_rand0to1(id, rand_halftab, randhalftabsize) + DOUBLE_C(0.5));
        } else {
	    t_color = a_color * DOUBLE_C(255.) + bu_rand0to1(id, rand_halftab, randhalftabsize);
        }
	rgb = convert_uchar3_sat(t_color);

	rgb = select(rgb, nonbackground, (uchar3)all(rgb == background));
	// make sure it's never perfect black
	rgb = select(rgb, (uchar3){rgb.x, rgb.y, 1}, (uchar3)all(!rgb));
    } else {
	/* shot missed the model, don't dither */
        rgb = background;
	a_color = -1e-20;	// background flag
    }

    if (o.s0 != o.s1) {
	/* write color */
	global uchar *colorp = (global uchar*)pixels+id*o.s1+o.s0;
	vstore3(rgb, 0, colorp);
    }
}
#else
/**
  * Multi-hit.
  */
void do_segp(RESULT_TYPE *res, const uint idx,
	     struct hit *seg_in, struct hit *seg_out)
{
    if (res) {
	RESULT_TYPE segp = *res;
	segp->seg_in = *seg_in;
	segp->seg_out = *seg_out;
	segp->seg_sti = idx;
	++*res;
    }
}


__kernel void
count_hits(global int *counts,
         const int cur_pixel, const int last_pixel, const int width,
	 const double16 view2model, const double cell_width, const double cell_height,
	 const double aspect, const uint nprims, global uchar *ids,
	 global struct linear_bvh_node *nodes, global uint *indexes, global uchar *prims)
{
    const size_t id = get_global_size(0)*get_global_id(1)+get_global_id(0);

    if (id >= (last_pixel-cur_pixel+1))
      return;

    const int pixelnum = cur_pixel+id;

    const int a_y = (int)(pixelnum/width);
    const int a_x = (int)(pixelnum - (a_y * width));

    double3 r_pt, r_dir;
    gen_ray(&r_pt, &r_dir, a_x, a_y, view2model, cell_width, cell_height, aspect);

    int ret = shootray(0, r_pt, r_dir, nprims, ids, nodes, indexes, prims);

    counts[id] = ret;
}

__kernel void
store_segs(RESULT_TYPE segs, global uint *h,
         const int cur_pixel, const int last_pixel, const int width,
	 const double16 view2model, const double cell_width, const double cell_height,
	 const double aspect, const uint nprims, global uchar *ids,
	 global struct linear_bvh_node *nodes, global uint *indexes, global uchar *prims)
{
    const size_t id = get_global_size(0)*get_global_id(1)+get_global_id(0);

    if (id >= (last_pixel-cur_pixel+1))
      return;

    const int pixelnum = cur_pixel+id;

    const int a_y = (int)(pixelnum/width);
    const int a_x = (int)(pixelnum - (a_y * width));

    double3 r_pt, r_dir;
    gen_ray(&r_pt, &r_dir, a_x, a_y, view2model, cell_width, cell_height, aspect);

    if (h[id] != h[id+1]) {
        RESULT_TYPE start, end;
        start = segs+h[id];
	end = start;
        int ret = shootray(&end, r_pt, r_dir, nprims, ids, nodes, indexes, prims);
    }
}

__kernel void
shade_segs(global uchar *pixels, const uchar2 o, RESULT_TYPE segs, global uint *head_partition,
         const int cur_pixel, const int last_pixel, const int width,
	 global float *rand_halftab, const uint randhalftabsize,
	 const uchar3 background, const uchar3 nonbackground,
	 const double airdensity, const double3 haze, const double gamma,
	 const double16 view2model, const double cell_width, const double cell_height,
	 const double aspect, const int lightmodel, global uchar *ids,
	 global uint *indexes, global uchar *prims, global struct region *regions,
         global struct partition *partitions)
{
    const size_t id = get_global_size(0)*get_global_id(1)+get_global_id(0);

    if (id >= (last_pixel-cur_pixel+1))
      return;

    double3 a_color;
    uchar3 rgb;
    struct hit hitp;

    if (lightmodel >= 0 && head_partition[id] != UINT_MAX) {
	const int pixelnum = cur_pixel+id;

	const int a_y = (int)(pixelnum/width);
	const int a_x = (int)(pixelnum - (a_y * width));

	double3 r_pt, r_dir;
	gen_ray(&r_pt, &r_dir, a_x, a_y, view2model, cell_width, cell_height, aspect);

	/* Determine the Light location(s) in view space */
	/* 0:  At left edge, 1/2 high */
	const double3 lt_pos = MAT4X3PNT(view2model, (double3){-1,0,1});

	bool flipflag;
	uint region_id;
	uint idx;

	double diffuse0 = 0;
	double cosI0 = 0;
	double3 work0, work1;

	a_color = 0.0;
	hitp.hit_dist = INFINITY;
	region_id = 0;

	idx = UINT_MAX;

	/* Get first partition of the ray */
	flipflag = 0;
	for (uint k = head_partition[id];
	     k != UINT_MAX;
	     k = partitions[k].forw_pp) {
	    global struct partition *pp = &partitions[k];
            RESULT_TYPE segp = &segs[pp->inseg];

            if (pp->inhit.hit_dist < hitp.hit_dist) {
                hitp = pp->inhit;
                idx = segp->seg_sti;
                region_id = pp->region_id;
                flipflag = pp->inflip;
            }
	}

        double3 normal;

        if (hitp.hit_dist < 0.0) {
	    /* Eye inside solid, orthoview */
	    normal = -r_dir;
        } else {
	    norm(&hitp, r_pt, r_dir, ids[idx], prims + indexes[idx]);
            hitp.hit_normal = flipflag ? -hitp.hit_normal : hitp.hit_normal;
	    normal = hitp.hit_normal;
        }

        /*
         * Diffuse reflectance from each light source
         */
        switch (lightmodel) {
	    default:
		a_color = shade(r_pt, r_dir, &hitp, lt_pos, region_id, regions);
	    	break;
            case 1:
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
                a_color = (normal * DOUBLE_C(-.5)) + DOUBLE_C(.5);
                break;
        }

        /*
         * e ^(-density * distance)
         */
        if (!ZERO(airdensity)) {
            double g;
            double f = exp(-hitp.hit_dist * airdensity);
            g = (1.0 - f);
            a_color = a_color * f + haze * g;
        }

        double3 t_color;

        /*
         * To prevent bad color aliasing, add some color dither.  Be
         * certain to NOT output the background color here.  Random
         * numbers in the range 0 to 1 are used, so that integer
         * valued colors (e.g., from texture maps) retain their original
         * values.
         */
        if (!ZERO(gamma)) {
            /*
             * Perform gamma correction in floating-point space, and
             * avoid nasty mach bands in dark areas from doing it in
             * 0..255 space later.
             */
            const double ex = 1.0/gamma;
	    t_color = floor(pow(a_color, ex) * DOUBLE_C(255.) +
			bu_rand0to1(id, rand_halftab, randhalftabsize) + DOUBLE_C(0.5));
        } else {
	    t_color = a_color * DOUBLE_C(255.) + bu_rand0to1(id, rand_halftab, randhalftabsize);
        }
	rgb = convert_uchar3_sat(t_color);

	rgb = select(rgb, nonbackground, (uchar3)all(rgb == background));
	// make sure it's never perfect black
	rgb = select(rgb, (uchar3){rgb.x, rgb.y, 1}, (uchar3)all(!rgb));
    } else {
	/* shot missed the model, don't dither */
        rgb = background;
	a_color = -1e-20;	// background flag
        hitp.hit_dist = INFINITY;
    }

    if (o.s0 != o.s1) {
	/* write color */
	global uchar *colorp = (global uchar*)pixels+id*o.s1+o.s0;
	vstore3(rgb, 0, colorp);
    }
}
#endif


/**
 * Sort an array of hits into ascending order.
 */
void
primitive_hitsort(struct hit *h, int nh)
{
    int i, j;
    struct hit temp;

    for (i = 0; i < nh-1; i++) {
	for (j = i + 1; j < nh; j++) {
	    if (h[i].hit_dist <= h[j].hit_dist)
		continue;
	    temp = h[j];		/* struct copy */
	    h[j] = h[i];		/* struct copy */
	    h[i] = temp;		/* struct copy */
	}
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
