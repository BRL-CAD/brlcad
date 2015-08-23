#ifndef COMMON_CL
#define COMMON_CL

#if __OPENCL_VERSION__ < 120
    #error "OpenCL 1.2 required."
#endif

#define RT_PCOEF_TOL            (1.0e-10)
#define RT_DOT_TOL              (0.001)

#define MAX_FASTF               (1.0e73)        /* Very close to the largest number */
#define SQRT_MAX_FASTF		(1.0e36)	/* This squared just avoids overflow */
#define SMALL_FASTF		(1.0e-77)
#define SQRT_SMALL_FASTF	(1.0e-39)	/* This squared gives zero */

#define M_PI_3			(1.04719755119659774615421446109316763)   /**< pi/3 */
#define M_SQRT3			(1.73205080756887729352744634150587237)   /**< sqrt(3) */

#define NEAR_ZERO(val, epsilon)		(((val) > -epsilon) && ((val) < epsilon))
#define ZERO(_a)        		NEAR_ZERO((_a), SMALL_FASTF)
#define NEAR_EQUAL(_a, _b, _tol)	NEAR_ZERO((_a) - (_b), (_tol))


typedef union {
    struct { double x, y; };
    struct { double re, im; };
} bn_complex_t;


struct hit {
  double3 hit_point;
  double3 hit_normal;
  double3 hit_vpriv;
  double hit_dist;
  int hit_surfno;
  uint hit_index;
};

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


inline double3 MAT3X3VEC(global const double *m, double3 i) {
    double3 o;
    o.x = dot(vload3(0, &m[0]), i);
    o.y = dot(vload3(0, &m[4]), i);
    o.z = dot(vload3(0, &m[8]), i);
    return o;
}

inline double3 MAT4X3VEC(global const double *m, double3 i) {
    double3 o;
    o = MAT3X3VEC(m, i) * (1.0/m[15]);
    return o;
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


/* solver.cl */
extern int rt_poly_roots(double *eqn, uint dgr, bn_complex_t *roots);

/* table.cl */
extern constant double rti_tol_dist;

extern void do_hitp(global struct hit **res, const uint hit_index, const struct hit *hitp);

#endif	/* COMMON_CL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

