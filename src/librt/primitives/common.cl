#ifndef COMMON_CL
#define COMMON_CL

#if __OPENCL_VERSION__ < 120
    #error "OpenCL 1.2 required."
#endif

#ifdef CLT_SINGLE_PRECISION
#define double float
#define double3 float3
#define double4 float4
#define double16 float16

#define convert_double3 convert_float3
#define as_ulong (ulong)

#define DOUBLE_C(value) \
	value ## f
#else
#define DOUBLE_C(value) \
	value
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
};

struct seg {
    struct hit seg_in;
    struct hit seg_out;
    uint seg_sti;
};

struct region;
#if RT_SINGLE_HIT
struct accum {
    struct seg s;
    double3 r_pt;
    double3 r_dir;
    double3 lt_pos;
    double3 a_color;
    double a_total;
    global uchar *ids;
    global uint *indexes;
    global uchar *prims;
    global struct region *regions;
    int lightmodel;
};

typedef struct accum *RESULT_TYPE;
#else
typedef global struct seg *RESULT_TYPE;
#endif

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


/* solver.cl */
extern int rt_poly_roots(double *eqn, uint dgr, bn_complex_t *roots);


/* rt.cl */
extern double3 MAT3X3VEC(global const double *m, double3 i);
extern double3 MAT4X3VEC(global const double *m, double3 i);

extern double3 bu_rand0to1(const uint id, global float *bnrandhalftab, const uint randhalftabsize);
extern ulong bu_cv_htond(const ulong d);

extern constant double rti_tol_dist;

extern bool rt_in_rpp(const double3 pt, const double3 invdir,
		      global const double *min, global const double *max);
extern void do_segp(RESULT_TYPE *res, const uint idx,
		    struct hit *seg_in, struct hit *seg_out);


/* *_shot.cl */
struct arb_specific;
struct ehy_specific;
struct ell_specific;
struct epa_specific;
struct eto_specific;
struct rec_specific;
struct sph_specific;
struct tgc_specific;
struct tor_specific;

extern int arb_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct arb_specific *arb);
extern int bot_shot(RESULT_TYPE *res, const double3 r_pt, double3 r_dir, const uint idx, global const uchar *args);
extern int ehy_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct ehy_specific *ehy);
extern int ell_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct ell_specific *ell);
extern int epa_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct epa_specific *epa);
extern int eto_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct eto_specific *eto);
extern int rec_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct rec_specific *rec);
extern int sph_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct sph_specific *sph);
extern int tgc_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct tgc_specific *tgc);
extern int tor_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct tor_specific *tor);

extern void arb_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct arb_specific *arb);
extern void bot_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const uchar *args);
extern void ehy_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct ehy_specific *ehy);
extern void ell_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct ell_specific *ell);
extern void epa_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct epa_specific *epa);
extern void eto_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct eto_specific *eto);
extern void rec_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct rec_specific *rec);
extern void sph_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct sph_specific *sph);
extern void tgc_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct tgc_specific *tgc);
extern void tor_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct tor_specific *tor);


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

