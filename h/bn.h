/*
 *			B N . H
 *
 *  Header file for the BRL-CAD Numerical Computation Library, LIBBN.
 *
 *  The library provides a broad assortment of numerical algorithms
 *  and computational routines, including random number generation,
 *  vector math, matrix math, quaternion math, complex math,
 *  synthetic division, root finding, etc.
 *
 *  This header file depends on vmath.h
 *  This header file depends on bu.h and LIBBU;  it is safe to use
 *  bu.h macros (e.g. BU_EXTERN) here.
 *
 *  ??Should complex.h and plane.h and polyno.h get absorbed in here??
 *	??absorbed/included??
 *
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A Butler
 *	Douglas A Gwyn
 *	Jeff Hanes
 *
 *  Modifications & Additions -
 *      Christopher Sean Morrison
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  Include Sequencing -
 *	#include "conf.h"
 *	#include <stdio.h>
 *	#include <math.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "bu.h"
 *	#include "vmath.h"
 *	#include "bn.h"
 *
 *  Libraries Used -
 *	-lm -lc
 *
 *  $Header$
 */

#ifndef SEEN_BN_H
#define SEEN_BN_H seen
#ifdef __cplusplus
extern "C" {
#endif

#define BN_H_VERSION	"@(#)$Header$ (BRL)"

/*			B N _ T O L
 *
 *  A handy way of passing around the tolerance information needed to
 *  perform approximate floating-point calculations on geometry.
 *
 *  dist & dist_sq establish the distance tolerance.
 *
 *	If two points are closer together than dist, then they are to
 *	be considered the same point.
 *	For example:
 *		point_t	a,b;
 *		vect_t	diff;
 *		VSUB2( diff, a, b );
 *		if( MAGNITUDE(diff) < tol->dist )	a & b are the same.
 *	or, more efficiently:
 *		if( MAQSQ(diff) < tol->dist_sq )
 *
 *  perp & para establish the angular tolerance.
 *
 *	If two rays emanate from the same point, and their dot product
 *	is nearly one, then the two rays are the same, while if their
 *	dot product is nearly zero, then they are perpendicular.
 *	For example:
 *		vect_t	a,b;
 *		if( fabs(VDOT(a,b)) >= tol->para )	a & b are parallel
 *		if( fabs(VDOT(a,b)) <= tol->perp )	a & b are perpendicular
 *
 */
struct bn_tol {
	unsigned long	magic;
	double		dist;			/* >= 0 */
	double		dist_sq;		/* dist * dist */
	double		perp;			/* nearly 0 */
	double		para;			/* nearly 1 */
};
#define BN_TOL_MAGIC	0x98c734bb
#define BN_CK_TOL(_p)	BU_CKMAG(_p, BN_TOL_MAGIC, "bn_tol")

#define	BN_VECT_ARE_PARALLEL(_dot,_tol)		\
	(((_dot) < 0) ? ((-(_dot))>=(_tol)->para) : ((_dot) >= (_tol)->para))
#define BN_VECT_ARE_PERP(_dot,_tol)		\
	(((_dot) < 0) ? ((-(_dot))<=(_tol)->perp) : ((_dot) <= (_tol)->perp))

#define BN_APPROXEQUAL(_a, _b, _tol) (fabs( (_a) - (_b) ) <= _tol->dist)

/*----------------------------------------------------------------------*/
/* complex.c */
/*
 *  Complex numbers
 */

/* "complex number" data type: */
typedef struct bn_complex {
	double		re;		/* real part */
	double		im;		/* imaginary part */
}  bn_complex_t;

/* functions that are efficiently done as macros: */

#define	bn_cx_copy( ap, bp )		{*(ap) = *(bp);}
#define	bn_cx_neg( cp )			{ (cp)->re = -((cp)->re);(cp)->im = -((cp)->im);}
#define	bn_cx_real( cp )		(cp)->re
#define	bn_cx_imag( cp )		(cp)->im

#define bn_cx_add( ap, bp )		{ (ap)->re += (bp)->re; (ap)->im += (bp)->im;}
#define bn_cx_ampl( cp )		hypot( (cp)->re, (cp)->im )
#define bn_cx_amplsq( cp )		( (cp)->re * (cp)->re + (cp)->im * (cp)->im )
#define bn_cx_conj( cp )		{ (cp)->im = -(cp)->im; }
#define bn_cx_cons( cp, r, i )		{ (cp)->re = r; (cp)->im = i; }
#define bn_cx_phas( cp )		atan2( (cp)->im, (cp)->re )
#define bn_cx_scal( cp, s )		{ (cp)->re *= (s); (cp)->im *= (s); }
#define bn_cx_sub( ap, bp )		{ (ap)->re -= (bp)->re; (ap)->im -= (bp)->im;}

#define bn_cx_mul( ap, bp )	 	\
	{ FAST fastf_t a__re, b__re; \
	(ap)->re = ((a__re=(ap)->re)*(b__re=(bp)->re)) - (ap)->im*(bp)->im; \
	(ap)->im = a__re*(bp)->im + (ap)->im*b__re; }

/* Output variable "ap" is different from input variables "bp" or "cp" */
#define bn_cx_mul2( ap, bp, cp )	{ \
	(ap)->re = (cp)->re * (bp)->re - (cp)->im * (bp)->im; \
	(ap)->im = (cp)->re * (bp)->im + (cp)->im * (bp)->re; }

BU_EXTERN(void			bn_cx_div, (bn_complex_t *ap, CONST bn_complex_t *bp) );
BU_EXTERN(void			bn_cx_sqrt, (bn_complex_t *op, CONST bn_complex_t *ip) );

/*----------------------------------------------------------------------*/
/* mat.c */
/*
 * 4x4 Matrix math
 */
extern CONST mat_t 	bn_mat_identity;

BU_EXTERN(void		bn_mat_print, (CONST char *title, CONST mat_t m));
BU_EXTERN(double	bn_atan2, (double x, double y));

#if 0 /* deprecated for macros below */
BU_EXTERN(void		bn_mat_zero, (mat_t m));
BU_EXTERN(void		bn_mat_idn, (mat_t m));
BU_EXTERN(void		bn_mat_copy, (register mat_t dest,register CONST mat_t src));
#else
#define	bn_mat_zero( _m )	(void)memset( (void *)_m, 0, sizeof(mat_t))
#define bn_mat_idn( _m )	(void)memcpy( (void *)_m, (CONST void *)bn_mat_identity, sizeof(mat_t))
#define bn_mat_copy(_d,_s)	(void)memcpy( (void *)_d, (CONST void *)(_s), sizeof(mat_t))
#endif /* deprecated */

BU_EXTERN(void		bn_mat_mul, (register mat_t o, register CONST mat_t a,
					register CONST mat_t b));
BU_EXTERN(void		bn_mat_mul2, (register CONST mat_t i, register mat_t o));
BU_EXTERN(void		bn_mat_mul3, (mat_t o, CONST mat_t a, CONST mat_t b,
					CONST mat_t c));
BU_EXTERN(void		bn_matXvec, (register hvect_t ov,
					register CONST mat_t im,
					register CONST hvect_t iv));
BU_EXTERN(void		bn_mat_inv, (register mat_t output, CONST mat_t input));
BU_EXTERN(void		bn_vtoh_move, (register vect_t h, 
					register CONST vect_t v));
BU_EXTERN(void		bn_htov_move, (register vect_t v, 
					register CONST vect_t h));
BU_EXTERN(void		bn_mat_trn, (mat_t om, register CONST mat_t im));
BU_EXTERN(void		bn_mat_ae, (register mat_t m, double azimuth,
					double elev));
BU_EXTERN(void		bn_ae_vec, (fastf_t *azp, fastf_t *elp, 
					CONST vect_t v));
BU_EXTERN(void 		bn_aet_vec, ( fastf_t *az, fastf_t *el, 
					fastf_t *twist, vect_t vec_ae,
					vect_t vec_twist, fastf_t accuracy));

BU_EXTERN(void		bn_mat_angles, (register mat_t mat, double alpha,
					double beta, double ggamma ));

BU_EXTERN(void		bn_eigen2x2, ( fastf_t	*val1, fastf_t *val2,
					vect_t	vec1, vect_t vec2, fastf_t a,
					fastf_t b, fastf_t c) );

BU_EXTERN(void		bn_vec_perp, (vect_t new_vec, CONST vect_t old_vec));
BU_EXTERN(void		bn_mat_fromto, ( mat_t m, CONST vect_t from,
					CONST vect_t to));
BU_EXTERN(void		bn_mat_xrot, (mat_t m, double sinx, double cosx));
BU_EXTERN(void		bn_mat_yrot, (mat_t m, double siny, double cosy));
BU_EXTERN(void		bn_mat_zrot, (mat_t m, double sinz, double cosz));
BU_EXTERN(void		bn_mat_lookat, (mat_t rot, CONST vect_t dir, int yflip));
BU_EXTERN(void		bn_vec_ortho, (register vect_t out, 
					register CONST vect_t in));
BU_EXTERN(int		bn_mat_scale_about_pt, (mat_t mat, CONST point_t pt,
					CONST double scale));
BU_EXTERN(void		bn_mat_xform_about_pt, (mat_t mat, 
					CONST mat_t xform,
					CONST point_t pt));
BU_EXTERN(int		bn_mat_is_equal, (CONST mat_t a, CONST mat_t b, 
					CONST struct bn_tol *tol));
BU_EXTERN(int		bn_mat_is_identity, (CONST mat_t m));
BU_EXTERN(void		bn_mat_arb_rot, ( mat_t m, CONST point_t pt,
					CONST vect_t dir, CONST fastf_t ang));
BU_EXTERN(matp_t	bn_mat_dup, (CONST mat_t in));
BU_EXTERN(int		bn_mat_ck, (CONST char *title, CONST mat_t m));
BU_EXTERN(fastf_t	bn_mat_det3, (CONST mat_t m));
BU_EXTERN(fastf_t	bn_mat_determinant, (CONST mat_t m));

/*----------------------------------------------------------------------*/
/* msr.c */
/*
 * Define data structures and constants for the "MSR" random number package.
 *
 * Also define a set of macros to access the random number tables
 * and to limit the area/volume that a set of random numbers inhabit.
 */

#define BN_UNIF_MAGIC	12481632
#define BN_GAUSS_MAGIC 512256128

#define BN_CK_UNIF(_p) BU_CKMAG(_p, BN_UNIF_MAGIC, "bn_unif")
#define BN_CK_GAUSS(_p) BU_CKMAG(_p, BN_GAUSS_MAGIC, "bn_gauss")

struct bn_unif {
	long	magic;
	long	msr_seed;
	int	msr_double_ptr;
	double	*msr_doubles;
	int	msr_long_ptr;
	long	*msr_longs;
};
/*
 * NOTE!!! The order of msr_gauss and msr_unif MUST match in the
 * first three entries as msr_gauss is passed as a msr_unif in
 * msr_gauss_fill.
 */
struct bn_gauss {
	long	magic;
	long	msr_gauss_seed;
	int	msr_gauss_dbl_ptr;
	double	*msr_gauss_doubles;
	int	msr_gauss_ptr;
	double	*msr_gausses;
};

BU_EXTERN(struct bn_unif *	bn_unif_init, (long setseed, int method));
BU_EXTERN(void			bn_unif_free, (struct bn_unif *p));
BU_EXTERN(long			bn_unif_long_fill, (struct bn_unif *p));
BU_EXTERN(double		bn_unif_double_fill, (struct bn_unif *p));
BU_EXTERN(struct bn_gauss *	bn_gauss_init, (long setseed, int method));
BU_EXTERN(void			bn_gauss_free, (struct bn_gauss *p));
BU_EXTERN(double		bn_gauss_fill, (struct bn_gauss *p));

#define	BN_UNIF_LONG(_p)	\
	 (((_p)->msr_long_ptr ) ? \
		(_p)->msr_longs[--(_p)->msr_long_ptr] : \
		bn_unif_long_fill(_p))
#define BN_UNIF_DOUBLE(_p)	\
	(((_p)->msr_double_ptr) ? \
		(_p)->msr_doubles[--(_p)->msr_double_ptr] : \
		bn_unif_double_fill(_p))

#define BN_UNIF_CIRCLE(_p,_x,_y,_r) { \
	do { \
		(_x) = 2.0*BN_UNIF_DOUBLE((_p)); \
		(_y) = 2.0*BN_UNIF_DOUBLE((_p)); \
		(_r) = (_x)*(_x)+(_y)*(_y); \
	} while ((_r) >= 1.0);  }

#define	BN_UNIF_SPHERE(_p,_x,_y,_z,_r) { \
	do { \
		(_x) = 2.0*BN_UNIF_DOUBLE(_p); \
		(_y) = 2.0*BN_UNIF_DOUBLE(_p); \
		(_z) = 2.0*BN_UNIF_DOUBLE(_p); \
		(_r) = (_x)*(_x)+(_y)*(_y)+(_z)*(_z);\
	} while ((_r) >= 1.0) }

#define	BN_GAUSS_DOUBLE(_p)	\
	(((_p)->msr_gauss_ptr) ? \
		(_p)->msr_gausses[--(_p)->msr_gauss_ptr] : \
		bn_gauss_fill(_p))


/*----------------------------------------------------------------------*/
/* noise.c */
/*
 * fractal noise support
 */

BU_EXTERN(void		bn_noise_init, () );
BU_EXTERN(double	bn_noise_perlin, (CONST point_t pt) );
/* XXX Why isn't the result listed first? */
BU_EXTERN(void		bn_noise_vec, (CONST point_t point, point_t result) );
BU_EXTERN(double	bn_noise_fbm, (point_t point, double h_val,
				double lacunarity, double octaves) );
BU_EXTERN(double	bn_noise_turb, (point_t point, double h_val,
				double lacunarity, double octaves ) );
BU_EXTERN(double	bn_noise_mf, (point_t point, double h_val,
				double lacunarity, double octaves,
				double offset) );
BU_EXTERN(double	bn_noise_ridged, (point_t point, double h_val,
				double lacunarity, double octaves,
				double offset) );

/*----------------------------------------------------------------------*/
/* plane.c */
/*
 * Plane/line/point calculations
 */


BU_EXTERN(int		bn_dist_pt3_lseg3, (fastf_t *dist, point_t pca,
				CONST point_t a, CONST point_t b,
				CONST point_t p, CONST struct bn_tol *tol));
BU_EXTERN(int		bn_3pts_collinear, ( point_t a, point_t b, point_t c,
				CONST struct bn_tol *tol));
BU_EXTERN(int		bn_pt3_pt3_equal, ( CONST point_t a, CONST point_t b,
				CONST struct bn_tol *tol));
BU_EXTERN(int		bn_dist_pt2_lseg2, ( fastf_t *dist_sq, 
				fastf_t pca[2], CONST point_t a,
				CONST point_t b, CONST point_t p,
				CONST struct bn_tol *tol));
BU_EXTERN(int		bn_isect_lseg3_lseg3, ( fastf_t *dist,
				CONST point_t p, CONST vect_t pdir,
				CONST point_t q, CONST vect_t qdir,
				CONST struct bn_tol *tol));
BU_EXTERN(int		bn_isect_line3_line3, (fastf_t *t, fastf_t *u, 	
				CONST point_t p, CONST vect_t d,
				CONST point_t a, CONST vect_t c,
				CONST struct bn_tol *tol));
BU_EXTERN(int		bn_2line3_colinear, ( CONST point_t p1,
				CONST vect_t d1, CONST point_t p2,
				CONST vect_t d2, double range,
				CONST struct bn_tol *tol));
BU_EXTERN(int		bn_isect_pt2_lseg2, ( fastf_t *dist, CONST point_t a,
				CONST point_t b, CONST point_t p,
				CONST struct bn_tol *tol));
BU_EXTERN(int		bn_isect_line2_lseg2, (fastf_t *dist, CONST point_t p,
				CONST vect_t d, CONST point_t a,
				CONST vect_t c, CONST struct bn_tol *tol));
BU_EXTERN(int		bn_isect_lseg2_lseg2, (fastf_t *dist, CONST point_t p,
				CONST vect_t pdir, CONST point_t q,
				CONST vect_t qdir, CONST struct bn_tol *tol));
BU_EXTERN(int		bn_isect_line2_line2, ( fastf_t *dist,
				CONST point_t p, CONST vect_t d,
				CONST point_t a, CONST vect_t c,
				CONST struct bn_tol *tol));
BU_EXTERN(double	bn_dist_pt3_pt3, (CONST point_t a, CONST point_t b));
BU_EXTERN(int		bn_3pts_distinct, (CONST point_t a, CONST point_t b,
				CONST point_t c, CONST struct bn_tol *tol) );
BU_EXTERN(int		bn_mk_plane_3pts, (plane_t plane, CONST point_t a,
				CONST point_t b, CONST point_t c,
				CONST struct bn_tol *tol) );
BU_EXTERN(int		bn_mkpoint_3planes, (point_t pt, CONST plane_t a,
				CONST plane_t b, CONST plane_t c) );
BU_EXTERN(int		bn_isect_line3_plane, (fastf_t *dist,
				CONST point_t pt,
				CONST vect_t dir,
				CONST plane_t plane,
				CONST struct bn_tol *tol) );
BU_EXTERN(int		bn_isect_2planes, (point_t pt, vect_t dir,
				CONST plane_t a, CONST plane_t b,
				CONST vect_t rpp_min,
				CONST struct bn_tol *tol) );
BU_EXTERN(int		bn_isect_2lines, (fastf_t *t, fastf_t *u,
				CONST point_t p, CONST vect_t d, 
				CONST point_t a, CONST vect_t c,
				CONST struct bn_tol *tol) );
BU_EXTERN(int		bn_isect_line_lseg, (fastf_t *t, CONST point_t p,
				CONST vect_t d, CONST point_t a,
				CONST point_t b, CONST struct bn_tol *tol) );
BU_EXTERN(double	bn_dist_line3_pt3, (CONST point_t pt,
				CONST vect_t dir, CONST point_t a) );
BU_EXTERN(double	bn_distsq_line3_pt3, (CONST point_t pt,
				CONST vect_t dir, CONST point_t a));
BU_EXTERN(double	bn_dist_line_origin, (CONST point_t pt,
				CONST vect_t dir) );
BU_EXTERN(double	bn_dist_line2_point2, (CONST point_t pt,
				CONST vect_t dir, CONST point_t a));
BU_EXTERN(double	bn_distsq_line2_point2, (CONST point_t pt,
				CONST vect_t dir, CONST point_t a));
BU_EXTERN(double	bn_area_of_triangle, (CONST point_t a,
				CONST point_t b, CONST point_t c) );
BU_EXTERN(int		bn_isect_pt_lseg, (fastf_t *dist, CONST point_t a,
				CONST point_t b, CONST point_t p,
				CONST struct bn_tol *tol) );
BU_EXTERN(double	bn_dist_pt_lseg, (point_t pca, CONST point_t a,
				CONST point_t b, CONST point_t p,
				CONST struct bn_tol *tol) );
BU_EXTERN(void		bn_rotate_bbox, (point_t omin, point_t omax,
				CONST mat_t mat, CONST point_t imin,
				CONST point_t imax));
BU_EXTERN(void		bn_rotate_plane, (plane_t oplane, CONST mat_t mat,
				CONST plane_t iplane));
BU_EXTERN(int		bn_coplanar, (CONST plane_t a, CONST plane_t b,
				CONST struct bn_tol *tol));
BU_EXTERN(double	bn_angle_measure, (vect_t vec, CONST vect_t x_dir,
				CONST vect_t y_dir));
BU_EXTERN(double	bn_dist_pt3_along_line3, (CONST point_t	p,
				CONST vect_t d, CONST point_t x));
BU_EXTERN(double	bn_dist_pt2_along_line2, (CONST point_t p,
				CONST vect_t d, CONST point_t x));
BU_EXTERN(int		bn_between, (double left, double mid,
				double right, CONST struct bn_tol *tol));
BU_EXTERN(int		bn_hlf_class, (CONST plane_t half_eqn,
				       CONST vect_t min, CONST vect_t max,
				       CONST struct bn_tol *tol));

#define BN_CLASSIFY_UNIMPLEMENTED	0x0000
#define BN_CLASSIFY_OVERLAPPING		0x0002
#define BN_CLASSIFY_INSIDE		0x0001
#define BN_CLASSIFY_OUTSIDE		0x0003

BU_EXTERN(int			bn_isect_planes, (point_t pt,
				CONST plane_t planes[], CONST int pl_count));

/*----------------------------------------------------------------------*/
/* poly.c */
/*
 *  Polynomial data type
 */

			/* This could be larger, or even dynamic... */
#define BN_MAX_POLY_DEGREE	4	/* Maximum Poly Order */
typedef  struct bn_poly {
	long		magic;
	int		dgr;
	double		cf[BN_MAX_POLY_DEGREE+1];
}  bn_poly_t;
#define BN_POLY_MAGIC	0x506f4c79	/* 'PoLy' */
#define BN_CK_POLY(_p)	BU_CKMAG(_p, BN_POLY_MAGIC, "struct bn_poly")
#define BN_POLY_NULL	((struct bn_poly *)NULL)

BU_EXTERN(struct bn_poly *	bn_poly_mul, (struct bn_poly *product,
				CONST struct bn_poly *m1, CONST struct bn_poly *m2));
BU_EXTERN(struct bn_poly *	bn_poly_scale, (struct bn_poly *eqn,
				double factor));
BU_EXTERN(struct bn_poly *	bn_poly_add, (struct bn_poly *sum,
				CONST struct bn_poly *poly1, CONST struct bn_poly *poly2));
BU_EXTERN(struct bn_poly *	bn_poly_sub, (struct bn_poly *diff,
				CONST struct bn_poly	*poly1,
				CONST struct bn_poly	*poly2));
BU_EXTERN(void			bn_poly_synthetic_division, (
				struct bn_poly *quo, struct bn_poly *rem,
				CONST struct bn_poly	*dvdend,
				CONST struct bn_poly	*dvsor));
BU_EXTERN(int			bn_poly_quadratic_roots, (
				struct bn_complex	roots[],
				CONST struct bn_poly	*quadrat));
BU_EXTERN(int			bn_poly_cubic_roots, (
				struct bn_complex	roots[],
				CONST struct bn_poly	*eqn));
BU_EXTERN(int			bn_poly_quartic_roots, (
				struct bn_complex	roots[],
				CONST struct bn_poly	*eqn));
BU_EXTERN(void			bn_pr_poly, (CONST char *title,
				CONST struct bn_poly	*eqn));
BU_EXTERN(void			bn_pr_roots, (CONST char *title,
				CONST struct bn_complex	roots[], int n));

/*----------------------------------------------------------------------*/
/* qmath.c */
/*
 * Quaternion support 
 */

BU_EXTERN(void quat_mat2quat, (quat_t quat, CONST mat_t mat));
BU_EXTERN(void quat_quat2mat, (mat_t mat, CONST quat_t quat));
BU_EXTERN(double quat_distance, (CONST quat_t q1, CONST quat_t q2));
BU_EXTERN(void quat_double, (quat_t qout, CONST quat_t q1, CONST quat_t q2));
BU_EXTERN(void quat_bisect, (quat_t qout, CONST quat_t q1, CONST quat_t q2));
BU_EXTERN(void quat_slerp, (quat_t qout, CONST quat_t q1, CONST quat_t q2, double f));
BU_EXTERN(void quat_sberp, (quat_t qout, CONST quat_t q1, CONST quat_t qa, CONST quat_t qb,
			    CONST quat_t q2, double f));
BU_EXTERN(void quat_make_nearest, (quat_t q1, CONST quat_t q2));
BU_EXTERN(void quat_print, (CONST char *title, CONST quat_t quat));
BU_EXTERN(void quat_exp, (quat_t out, CONST quat_t in));
BU_EXTERN(void quat_log, (quat_t out, CONST quat_t in));
/*----------------------------------------------------------------------*/
/* rand.c */

/*  A supply of fast pseudo-random numbers from table in bn/rand.c.
 *  The values are in the range 0..1
 *
 * Usage:
 *	unsigned idx;
 *	float f;
 *
 *	BN_RANDSEED( idx, integer_seed );
 *
 *	while (NEED_MORE_RAND_NUMBERS) {
 *		f = BN_RANDOM( idx );
 *	}
 *
 * Note that the values from bn_rand_half() become all 0.0 when the benchmark
 * flag is set (bn_rand_halftab is set to all 0's).  The numbers from 
 * bn_rand_table do not change, because the procedural noise would cease to
 * exist.
 */
#define BN_RAND_TABSIZE 4096
#define BN_RAND_TABMASK 0xfff
#define BN_RANDSEED( _i, _seed )  _i = ((unsigned)_seed) % BN_RAND_TABSIZE
extern CONST float bn_rand_table[BN_RAND_TABSIZE];

/* BN_RANDOM always gives numbers between 0.0 and 1.0 */
#define BN_RANDOM( _i )	bn_rand_table[ _i = (_i+1) % BN_RAND_TABSIZE ]

/* BN_RANDHALF always gives numbers between -0.5 and 0.5 */
#define BN_RANDHALF( _i ) (bn_rand_table[ _i = (_i+1) % BN_RAND_TABSIZE ]-0.5)
#define BN_RANDHALF_INIT(_p) _p = bn_rand_table

/* XXX This should move to compat4 */
/* #define rand_half bn_rand_half */
/* #define rand_init bn_rand_init */
/* #define rand0to1  bn_rand0to1 */

/* random numbers between -0.5 and 0.5, except when benchmark flag is set, 
 * when this becomes a constant 0.0
 */
#define BN_RANDHALFTABSIZE	16535	/* Powers of two give streaking */
extern int bn_randhalftabsize;
extern float bn_rand_halftab[BN_RANDHALFTABSIZE];

#define bn_rand_half(_p)	\
	( (++(_p) >= &bn_rand_halftab[bn_randhalftabsize] || \
	     (_p) < bn_rand_halftab) ? \
		*((_p) = bn_rand_halftab) : *(_p))

#define bn_rand_init(_p, _seed)	\
	(_p) = &bn_rand_halftab[ \
		(int)( \
		      (bn_rand_halftab[(_seed)%bn_randhalftabsize] + 0.5) * \
		      (bn_randhalftabsize-1) ) ]

/* random numbers 0..1 except when benchmarking, when this is always 0.5 */
#define bn_rand0to1(_q)	(bn_rand_half(_q)+0.5)


#define	BN_SINTABSIZE		2048
extern double bn_sin_scale;
#define bn_tab_sin(_a)	(((_a) > 0) ? \
	( bn_sin_table[(int)((0.5+ (_a)*bn_sin_scale))&(BN_SINTABSIZE-1)] ) :\
	(-bn_sin_table[(int)((0.5- (_a)*bn_sin_scale))&(BN_SINTABSIZE-1)] ) )
extern CONST float bn_sin_table[BN_SINTABSIZE];



/*----------------------------------------------------------------------*/
/* wavelet.c */

#define CK_POW_2(dimen) { register unsigned long j; register int ok;\
	for (ok=0, j=0 ; j < sizeof(unsigned long) * 8 ; j++) { \
		if ( (1<<j) == dimen) { ok = 1;  break; } \
	} \
	if ( ! ok ) { \
		bu_log("%s:%d value %d should be power of 2 (2^%d)\n", \
			__FILE__, __LINE__, dimen, j); \
		bu_bomb("CK_POW_2"); \
	} \
}

BU_EXTERN(void	bn_wlt_haar_1d_double_decompose, (double *tbuf, double *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_1d_double_reconstruct, (double *tbuf, double *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_1d_float_decompose, (float *tbuf, float *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_1d_float_reconstruct, (float *tbuf, float *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_1d_char_decompose, (char *tbuf, char *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_1d_char_reconstruct, (char *tbuf, char *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_1d_short_decompose, (short *tbuf, short *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_1d_short_reconstruct, (short *tbuf, short *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_1d_int_decompose, (int *tbuf, int *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_1d_int_reconstruct, (int *tbuf, int *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_1d_long_decompose, (long *tbuf, long *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_1d_long_reconstruct, (long *tbuf, long *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));



BU_EXTERN(void	bn_wlt_haar_2d_double_decompose, (double *tbuf, double *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_double_reconstruct, (double *tbuf, double *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_float_decompose, (float *tbuf, float *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_float_reconstruct, (float *tbuf, float *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_char_decompose, (char *tbuf, char *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_char_reconstruct, (char *tbuf, char *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_short_decompose, (short *tbuf, short *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_short_reconstruct, (short *tbuf, short *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_int_decompose, (int *tbuf, int *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_int_reconstruct, (int *tbuf, int *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_long_decompose, (long *tbuf, long *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_long_reconstruct, (long *tbuf, long *buf, \
			unsigned long dimen, unsigned long depth, \
			unsigned long subimage_size, unsigned long limit ));



BU_EXTERN(void	bn_wlt_haar_2d_double_decompose2, (double *tbuf, double *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_double_reconstruct2, (double *tbuf, double *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long subimage_size, \
                        unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_float_decompose2, (float *tbuf, float *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_float_reconstruct2, (float *tbuf, float *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long subimage_size, \
                        unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_char_decompose2, (char *tbuf, char *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_char_reconstruct2, (char *tbuf, char *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long subimage_size, \
                        unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_short_decompose2, (short *tbuf, short *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_short_reconstruct2, (short *tbuf, short *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long subimage_size, \
		        unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_int_decompose2, (int *tbuf, int *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_int_reconstruct2, (int *tbuf, int *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long subimage_size, \
                        unsigned long limit ));

BU_EXTERN(void	bn_wlt_haar_2d_long_decompose2, (long *tbuf, long *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long limit ));
BU_EXTERN(void	bn_wlt_haar_2d_long_reconstruct2, (long *tbuf, long *buf, \
			unsigned long dimen, unsigned long width, \
			unsigned long height, unsigned long subimage_size, \
			unsigned long limit ));


/*----------------------------------------------------------------------*/
/* const.c */
extern CONST double bn_pi;
extern CONST double bn_twopi;
extern CONST double bn_halfpi;
extern CONST double bn_invpi;
extern CONST double bn_inv2pi;
extern CONST double bn_inv255;
extern CONST double bn_degtorad;
extern CONST double bn_radtodeg;

/*----------------------------------------------------------------------*/
/* tabdata.c */
/*
 *			T A B D A T A
 *
 *  Data structures to assist with
 *  recording many sets of data sampled along the same set of independent
 *  variables.
 *  The overall notion is that each sample should be
 *  as compact as possible (an array of measurements),
 *  with all the context stored in one place.
 *
 *  These structures and support routines apply to
 *  any measured "curve" or "function" or "table" with one independent
 *  variable and one or more scalar dependent variable(s).
 *
 *  The context is kept in an 'bn_table' structure, and
 *  the data for one particular sample are kept in an 'bn_tabdata'
 *  structure.
 *
 *  The contents of the sample in val[j] are interpreted
 *  in the interval (wavel[j]..wavel[j+1]).
 *  This value could be power, albedo, absorption, refractive index,
 *  or any other wavelength-specific parameter.
 *
 *  For example, if the val[] array contains power values, then
 *  val[j] contains the integral of the power from wavel[j] to wavel[j+1]
 *
 *  As an exmple, assume nwave=2, wavel[0]=500, wavel[1]=600, wavel[2]=700.
 *  Then val[0] would contain data for the 500 to 600nm interval,
 *  and val[1] would contain data for the 600 to 700nm interval.
 *  There would be no storage allocated for val[2] -- don't use it!
 *  There are several interpretations of this:
 *	1)  val[j] stores the total (integral, area) value for the interval, or
 *	2)  val[j] stores the average value across the interval.
 *
 *  The intervals need not be uniformly spaced; it is acceptable to
 *  increase wavelength sampling density around "important" frequencies.
 *
 *  See Also -
 *	h/spectrum.h, spectrum.c
 */
struct bn_table {
	long		magic;
	int		nx;
	fastf_t		x[1];	/* array of nx+1 wavelengths, dynamically sized */
};
#define BN_TABLE_MAGIC	0x53706374
#define BN_CK_TABLE(_p)	BU_CKMAG(_p, BN_TABLE_MAGIC, "bn_table")
#define BN_TABLE_NULL	((struct bn_table *)NULL)

/* Gets an bn_table, with x[] having size _nx+1 */
#define BN_GET_TABLE(_table, _nx)  { \
	if( (_nx) < 1 )  bu_bomb("RT_GET_TABLE() _nx < 1\n"); \
	_table = (struct bn_table *)bu_calloc( 1, \
		sizeof(struct bn_table) + sizeof(fastf_t)*(_nx), \
		"struct bn_table" ); \
	_table->magic = BN_TABLE_MAGIC; \
	_table->nx = (_nx);  }


struct bn_tabdata {
	long		magic;
	int		ny;
	CONST struct bn_table *table;	/* Up pointer to definition of X axis */
	fastf_t		y[1];		/* array of ny samples, dynamically sized */
};
#define BN_TABDATA_MAGIC	0x53736d70
#define BN_CK_TABDATA(_p)	BU_CKMAG(_p, BN_TABDATA_MAGIC, "bn_tabdata")
#define BN_TABDATA_NULL		((struct bn_tabdata *)NULL)

#define BN_SIZEOF_TABDATA_Y(_tabdata)	sizeof(fastf_t)*((_tabdata)->ny)
#define BN_SIZEOF_TABDATA(_table)	( sizeof(struct bn_tabdata) + \
			sizeof(fastf_t)*((_table)->nx-1) )

/* Gets an bn_tabdata, with y[] having size _ny */
#define BN_GET_TABDATA(_data, _table)  { \
	BN_CK_TABLE(_table);\
	_data = (struct bn_tabdata *)bu_calloc( 1, \
		BN_SIZEOF_TABDATA(_table), "struct bn_tabdata" ); \
	_data->magic = BN_TABDATA_MAGIC; \
	_data->ny = (_table)->nx; \
	_data->table = (_table); }

/*
 * Routines
 */

BU_EXTERN( void			bn_table_free, (struct bn_table	*tabp));
BU_EXTERN( void			bn_tabdata_free, (struct bn_tabdata *data));
BU_EXTERN( void			bn_ck_table, (CONST struct bn_table *tabp));
BU_EXTERN( struct bn_table	*bn_table_make_uniform, (int num, double first,
					double last));
BU_EXTERN( void			bn_tabdata_add, (struct bn_tabdata *out,
					CONST struct bn_tabdata *in1,
					CONST struct bn_tabdata *in2));
BU_EXTERN( void			bn_tabdata_mul, (struct bn_tabdata *out,
					CONST struct bn_tabdata *in1,
					CONST struct bn_tabdata *in2));
BU_EXTERN( void			bn_tabdata_scale, (struct bn_tabdata *out,
					CONST struct bn_tabdata *in1,
					double scale));
BU_EXTERN( void			bn_table_scale, (struct bn_table *tabp,
					double scale));
BU_EXTERN( void			bn_tabdata_join1, (struct bn_tabdata *out,
					CONST struct bn_tabdata *in1,
					double scale,
					CONST struct bn_tabdata *in2));
BU_EXTERN( void			bn_tabdata_blend3, (struct bn_tabdata *out,
					double scale1,
					CONST struct bn_tabdata *in1,
					double scale2,
					CONST struct bn_tabdata *in2,
					double scale3,
					CONST struct bn_tabdata *in3));
BU_EXTERN( double		bn_tabdata_area1, (CONST struct bn_tabdata *in));
BU_EXTERN( double		bn_tabdata_area2, (CONST struct bn_tabdata *in));
BU_EXTERN( double		bn_tabdata_mul_area1, (CONST struct bn_tabdata *in1,
					CONST struct bn_tabdata	*in2));
BU_EXTERN( double		bn_tabdata_mul_area2, (CONST struct bn_tabdata *in1,
					CONST struct bn_tabdata	*in2));
BU_EXTERN( fastf_t		bn_table_lin_interp, (CONST struct bn_tabdata *samp,
					double wl));
BU_EXTERN( struct bn_tabdata	*bn_tabdata_resample_max, (
					CONST struct bn_table *newtable,
					CONST struct bn_tabdata *olddata));
BU_EXTERN( struct bn_tabdata	*bn_tabdata_resample_avg, (
					CONST struct bn_table *newtable,
					CONST struct bn_tabdata *olddata));
BU_EXTERN( int			bn_table_write, (CONST char *filename,
					CONST struct bn_table *tabp));
BU_EXTERN( struct bn_table	*bn_table_read, (CONST char *filename));
BU_EXTERN( int			bn_print_table_and_tabdata, (CONST char *filename,
					CONST struct bn_tabdata *data));
BU_EXTERN( struct bn_tabdata	*bn_read_table_and_tabdata, (
					CONST char *filename));
BU_EXTERN( struct bn_tabdata	*bn_tabdata_binary_read, (CONST char *filename,
					int num,
					CONST struct bn_table *tabp));
BU_EXTERN( struct bn_tabdata	*bn_tabdata_malloc_array, (
					CONST struct bn_table *tabp,
					int num));
BU_EXTERN( void			bn_tabdata_copy, (struct bn_tabdata *out,
					CONST struct bn_tabdata *in));
BU_EXTERN(struct bn_tabdata	*bn_tabdata_dup, (CONST struct bn_tabdata *in));
BU_EXTERN(struct bn_tabdata	*bn_tabdata_get_constval, (double val,
					CONST struct bn_table	*tabp));
BU_EXTERN(void			bn_tabdata_constval, (struct bn_tabdata	*data, double val));
BU_EXTERN(struct bn_tabdata	*bn_tabdata_from_array, (CONST double *array));
BU_EXTERN(struct bn_table	*bn_table_merge2, (CONST struct bn_table *a,
				CONST struct bn_table *b));
BU_EXTERN(struct bn_tabdata	*bn_tabdata_mk_linear_filter,
					(CONST struct bn_table *spectrum,
					double lower_wavelen, double upper_wavelen));

/*----------------------------------------------------------------------*/
/* vlist.c */
/*
 *			B N _ V L I S T
 *
 *  Definitions for handling lists of vectors (really verticies, or points)
 *  and polygons in 3-space.
 *  Intented for common handling of wireframe display information,
 *  in the full resolution that is calculated in.
 *
 *  On 32-bit machines, BN_VLIST_CHUNK of 35 results in bn_vlist structures
 *  just less than 1k bytes.
 *
 *  The head of the doubly linked list can be just a "struct bu_list" head.
 *
 *  To visit all the elements in the vlist:
 *	for( BU_LIST_FOR( vp, rt_vlist, hp ) )  {
 *		register int	i;
 *		register int	nused = vp->nused;
 *		register int	*cmd = vp->cmd;
 *		register point_t *pt = vp->pt;
 *		for( i = 0; i < nused; i++,cmd++,pt++ )  {
 *			access( *cmd, *pt );
 *			access( vp->cmd[i], vp->pt[i] );
 *		}
 *	}
 */
#define BN_VLIST_CHUNK	35		/* 32-bit mach => just less than 1k */
struct bn_vlist  {
	struct bu_list	l;			/* magic, forw, back */
	int		nused;			/* elements 0..nused active */
	int		cmd[BN_VLIST_CHUNK];	/* VL_CMD_* */
	point_t		pt[BN_VLIST_CHUNK];	/* associated 3-point/vect */
};
#define BN_VLIST_NULL	((struct bn_vlist *)0)
#define BN_VLIST_MAGIC	0x98237474
#define BN_CK_VLIST(_p) BU_CKMAG((_p), BN_VLIST_MAGIC, "bn_vlist")
#define BN_CK_VLIST_TCL(_interp,_p) BU_CKMAG_TCL(_interp,(_p), BN_VLIST_MAGIC, "bn_vlist")

/* Values for cmd[] */
#define BN_VLIST_LINE_MOVE	0
#define BN_VLIST_LINE_DRAW	1
#define BN_VLIST_POLY_START	2	/* pt[] has surface normal */
#define BN_VLIST_POLY_MOVE	3	/* move to first poly vertex */
#define BN_VLIST_POLY_DRAW	4	/* subsequent poly vertex */
#define BN_VLIST_POLY_END	5	/* last vert (repeats 1st), draw poly */
#define BN_VLIST_POLY_VERTNORM	6	/* per-vertex normal, for interpoloation */

/*
 *  Applications that are going to use BN_ADD_VLIST and BN_GET_VLIST
 *  are required to execute this macro once, on their _free_hd:
 *		BU_LIST_INIT( &_free_hd );
 *
 * Note that BN_GET_VLIST and BN_FREE_VLIST are non-PARALLEL.
 */
#define BN_GET_VLIST(_free_hd,p) {\
		(p) = BU_LIST_FIRST( bn_vlist, (_free_hd) ); \
		if( BU_LIST_IS_HEAD( (p), (_free_hd) ) )  { \
			(p) = (struct bn_vlist *)bu_malloc(sizeof(struct bn_vlist), "bn_vlist"); \
			(p)->l.magic = BN_VLIST_MAGIC; \
		} else { \
			BU_LIST_DEQUEUE( &((p)->l) ); \
		} \
		(p)->nused = 0; \
	}

/* Place an entire chain of bn_vlist structs on the freelist _free_hd */
#define BN_FREE_VLIST(_free_hd,hd)	{ \
	BU_CK_LIST_HEAD( (hd) ); \
	BU_LIST_APPEND_LIST( (_free_hd), (hd) ); \
	}

#define BN_ADD_VLIST(_free_hd,_dest_hd,pnt,draw)  { \
	register struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD( _dest_hd ); \
	_vp = BU_LIST_LAST( bn_vlist, (_dest_hd) ); \
	if( BU_LIST_IS_HEAD( _vp, (_dest_hd) ) || _vp->nused >= BN_VLIST_CHUNK )  { \
		BN_GET_VLIST(_free_hd, _vp); \
		BU_LIST_INSERT( (_dest_hd), &(_vp->l) ); \
	} \
	VMOVE( _vp->pt[_vp->nused], (pnt) ); \
	_vp->cmd[_vp->nused++] = (draw); \
	}

/*
 *			B N _ V L B L O C K
 *
 *  For plotting, a way of separating plots into separate color vlists:
 *  blocks of vlists, each with an associated color.
 */
struct bn_vlblock {
	long		magic;
	int		nused;
	int		max;
	long		*rgb;		/* rgb[max] variable size array */
	struct bu_list	*head;		/* head[max] variable size array */
	struct bu_list	*free_vlist_hd;	/* where to get/put free vlists */
};
#define BN_VLBLOCK_MAGIC	0x981bd112
#define BN_CK_VLBLOCK(_p)	BU_CKMAG((_p), BN_VLBLOCK_MAGIC, "bn_vlblock")

BU_EXTERN(void	bn_vlist_3string, (struct bu_list *vhead,
				struct bu_list *free_hd, CONST char *string,
				CONST point_t origin, CONST mat_t rot,
				double scale));
BU_EXTERN(void	bn_vlist_2string, (struct bu_list *vhead,
				struct bu_list *free_hd, CONST char *string,
				double x, double y, double scale, double theta));

/*----------------------------------------------------------------------*/
/* vectfont.c */
extern void tp_setup();


/*----------------------------------------------------------------------*/
/* vers.c (created by the Cakefile) */
extern CONST char		bn_version[];
/*----------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
#endif /* SEEN_BN_H */
