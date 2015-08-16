#ifndef COMMON_CL
#define COMMON_CL

#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
    #error "Double precision floating point not supported by OpenCL implementation."
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


/* solver.cl */
extern int rt_poly_roots(double *eqn, uint dgr, bn_complex_t *roots);

/* table.cl */
extern constant double rti_tol_dist;

extern void do_hitp(global struct hit *res, const uint i, const uint hit_index, const struct hit *hitp);

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

