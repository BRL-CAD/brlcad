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

struct partition {
    struct hit inhit;
    struct hit outhit;
    uint inseg;
    uint outseg;
    uint forw_pp;               /* index to the next partition */
    uint back_pp;               /* index to the previous partition */
    uint region_id;             /* id of the "owning" region */
    char inflip;		/* flip inhit->hit_normal */
    char outflip;		/* flip outhit->hit_normal */
};

/**
 * bit expr tree representation
 *
 * node:
 *      uint uop : 3
 *      uint right_child : 29
 *
 * leaf:
 *      uint uop : 3
 *      uint st_bit : 29
 */
struct tree_bit {
    uint val;
};

struct bool_region {
    uint btree_offset;          /* index to the start of the bit tree */
    int reg_aircode;            /* Region ID AIR code */
    int reg_bit;                /* constant index into Regions[] */
    short reg_all_unions;       /* 1=boolean tree is all unions */
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
    global int *iregions;
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
extern double3 MAT4X3PNT(const double16 m, double3 i);
extern constant double rti_tol_dist;

extern bool rt_in_rpp(const double3 pt, const double3 invdir,
		      global const double *min, global const double *max);
extern bool rt_in_rpp2(const double3 pt, const double3 invdir,
		       global const double *min, global const double *max,
		       double *rmin, double *rmax);
extern void do_segp(RESULT_TYPE *res, const uint idx,
		    struct hit *seg_in, struct hit *seg_out);
extern void primitive_hitsort(struct hit *h, int nh);


/* *_shot.cl */
#define RT_DECLARE_INTERFACE(name) \
struct name##_specific; \
extern int name##_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct name##_specific *name); \
extern void name##_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct name##_specific *name)

RT_DECLARE_INTERFACE(arb);
RT_DECLARE_INTERFACE(ehy);
RT_DECLARE_INTERFACE(ell);
RT_DECLARE_INTERFACE(epa);
RT_DECLARE_INTERFACE(eto);
RT_DECLARE_INTERFACE(arbn);
RT_DECLARE_INTERFACE(part);
RT_DECLARE_INTERFACE(rec);
RT_DECLARE_INTERFACE(sph);
RT_DECLARE_INTERFACE(ebm);
RT_DECLARE_INTERFACE(tgc);
RT_DECLARE_INTERFACE(tor);
RT_DECLARE_INTERFACE(rhc);
RT_DECLARE_INTERFACE(rpc);
RT_DECLARE_INTERFACE(hrt);
RT_DECLARE_INTERFACE(superell);
RT_DECLARE_INTERFACE(hyp);

extern int bot_shot(RESULT_TYPE *res, const double3 r_pt, double3 r_dir, const uint idx, global const uchar *args);
extern void bot_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const uchar *args);

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

