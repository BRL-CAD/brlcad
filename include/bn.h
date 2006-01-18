/*                            B N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file bn.h
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
 *  Include Sequencing -
 *	#include "common.h"
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

#ifndef __BN_H__
#define __BN_H__

__BEGIN_DECLS

#ifndef BN_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef BN_EXPORT_DLL
#      define BN_EXPORT __declspec(dllexport)
#    else
#      define BN_EXPORT __declspec(dllimport)
#    endif
#  else
#    define BN_EXPORT
#  endif
#endif

#define BN_H_VERSION	"@(#)$Header$ (BRL)"

/* interface headers */
#include "vmath.h"

#define BN_AZIMUTH 0
#define BN_ELEVATION 1
#define BN_TWIST 2

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
 *  Note:
 *	tol->dist_sq = tol->dist * tol->dist;
 *	tol->para = 1 - tol->perp;
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

/* asize.c */
BN_EXPORT BU_EXTERN(int bn_common_file_size,
		    (unsigned long int *width,
		     unsigned long int *height,
		     const char *filename,
		     int pixel_size));
BN_EXPORT BU_EXTERN(int bn_common_name_size,
		    (unsigned long int *width,
		     unsigned long int *height,
		     const char *name));
BN_EXPORT BU_EXTERN(int bn_common_image_size,
		    (unsigned long int *width,
		     unsigned long int *height,
		     unsigned long int num_pixels));

/*----------------------------------------------------------------------*/
/* anim.c */
/* XXX These should all have bn_ prefixes */
BN_EXPORT BU_EXTERN(void anim_v_permute,
		    (mat_t m));
BN_EXPORT BU_EXTERN(void anim_v_unpermute,
		    (mat_t m));
BN_EXPORT BU_EXTERN(void anim_tran,
		    (mat_t m));
BN_EXPORT BU_EXTERN(int anim_mat2zyx,
		    (const mat_t viewrot,
		     vect_t angle));
BN_EXPORT BU_EXTERN(int anim_mat2ypr,
		    (mat_t viewrot,
		     vect_t angle));
BN_EXPORT BU_EXTERN(int anim_mat2quat,
		    (quat_t quat,
		     const mat_t viewrot));
BN_EXPORT BU_EXTERN(void anim_ypr2mat,
		    (mat_t m,
		     const vect_t a));
BN_EXPORT BU_EXTERN(void anim_ypr2vmat,
		    (mat_t m,
		     const vect_t a));
BN_EXPORT BU_EXTERN(void anim_y_p_r2mat,
		    (mat_t m,
		     double y,
		     double p,
		     double r));
BN_EXPORT BU_EXTERN(void anim_dy_p_r2mat,
		    (mat_t m,
		     double y,
		     double p,
		     double r));
BN_EXPORT BU_EXTERN(void anim_dy_p_r2vmat,
		    (mat_t m,
		     double yaw,
		     double pch,
		     double rll));
BN_EXPORT BU_EXTERN(void anim_x_y_z2mat,
		    (mat_t m,
		     double x,
		     double y,
		     double z));
BN_EXPORT BU_EXTERN(void anim_dx_y_z2mat,
		    (mat_t m,
		     double x,
		     double y,
		     double z));
BN_EXPORT BU_EXTERN(void anim_zyx2mat,
		    (mat_t m,
		     const vect_t a));
BN_EXPORT BU_EXTERN(void anim_z_y_x2mat,
		    (mat_t m,
		     double x,
		     double y,
		     double z));
BN_EXPORT BU_EXTERN(void anim_dz_y_x2mat,
		    (mat_t m,
		     double x,
		     double y,
		     double z));
BN_EXPORT BU_EXTERN(void anim_quat2mat,
		    (mat_t m,
		     const quat_t qq));
BN_EXPORT BU_EXTERN(void anim_dir2mat,
		    (mat_t m,
		     const vect_t d,
		     const vect_t d2));
BN_EXPORT BU_EXTERN(void anim_dirn2mat,
		    (mat_t m,
		     const vect_t dx,
		     const vect_t dn));
BN_EXPORT BU_EXTERN(int anim_steer_mat,
		    (mat_t  mat,
		     vect_t point,
		     int end));
BN_EXPORT BU_EXTERN(void anim_add_trans,
		    (mat_t m,
		     const vect_t post,
		     const vect_t pre));
BN_EXPORT BU_EXTERN(void anim_rotatez,
		    (fastf_t a,
		     vect_t d));
BN_EXPORT BU_EXTERN(void anim_mat_print,
		    (FILE *fp,
		     const mat_t m,
		     int s_colon));
BN_EXPORT BU_EXTERN(void anim_mat_printf,
		    (FILE *fp,
		     const mat_t m,
		     const char *formstr,
		     const char *linestr,
		     const char *endstr));
BN_EXPORT BU_EXTERN(void anim_view_rev,
		    (mat_t m));


/*----------------------------------------------------------------------*/
/* bn_tcl.c */
BN_EXPORT BU_EXTERN(int bn_decode_mat,
		    (mat_t m,
		     const char *str));
BN_EXPORT BU_EXTERN(int bn_decode_quat,
		    (quat_t q,
		     const char *str));
BN_EXPORT BU_EXTERN(int bn_decode_vect,
		    (vect_t v,
		     const char *str));
BN_EXPORT BU_EXTERN(int bn_decode_hvect,
		    (hvect_t v,
		     const char *str));
BN_EXPORT BU_EXTERN(void bn_encode_mat,
		    (struct bu_vls *vp,
		     const mat_t m));
BN_EXPORT BU_EXTERN(void bn_encode_quat,
		    (struct bu_vls *vp,
		     const quat_t q));
BN_EXPORT BU_EXTERN(void bn_encode_vect,
		    (struct bu_vls *vp,
		     const vect_t v));
BN_EXPORT BU_EXTERN(void bn_encode_hvect,
		    (struct bu_vls *vp,
		     const hvect_t v));

/* The presence of Tcl_Interp as an arg prevents giving arg list */
BN_EXPORT BU_EXTERN(void bn_tcl_setup,
		    ());
#ifdef BRLCAD_DEBUG
BN_EXPORT BU_EXTERN(int Bn_d_Init,
		    ());
#else
BN_EXPORT BU_EXTERN(int Bn_Init,
		    ());
#endif
BN_EXPORT BU_EXTERN(void bn_tcl_mat_print,
		    ());


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

BN_EXPORT BU_EXTERN(void bn_cx_div,
		    (bn_complex_t *ap,
		     const bn_complex_t *bp));
BN_EXPORT BU_EXTERN(void bn_cx_sqrt,
		    (bn_complex_t *op,
		     const bn_complex_t *ip));

/*----------------------------------------------------------------------*/
/* mat.c */
/*
 * 4x4 Matrix math
 */
BN_EXPORT extern const mat_t 	bn_mat_identity;

BN_EXPORT BU_EXTERN(void bn_mat_print,
		    (const char *title,
		     const mat_t m));
BN_EXPORT BU_EXTERN(void bn_mat_print_guts,
		    (const char *title,
		     const mat_t m,
		     char *buf));
BN_EXPORT BU_EXTERN(double bn_atan2,
		    (double x, double y));

#if 0 /* deprecated for macros below (which were deprecated for vmath.h) */
BN_EXPORT BU_EXTERN(void bn_mat_zero,
		    (mat_t m));
BN_EXPORT BU_EXTERN(void bn_mat_idn,
		    (mat_t m));
BN_EXPORT BU_EXTERN(void bn_mat_copy,
		    (register mat_t dest,
		     register const mat_t src));
#else
#define bn_mat_zero( _m )	{ \
	bu_log("%s:%d bn_mat_zero() is deprecated, use MAT_ZERO()\n", \
			__FILE__, __LINE__); \
	(_m)[0] = (_m)[1] = (_m)[2] = (_m)[3] = \
	(_m)[4] = (_m)[5] = (_m)[6] = (_m)[7] = \
	(_m)[8] = (_m)[9] = (_m)[10] = (_m)[11] = \
	(_m)[12] = (_m)[13] = (_m)[14] = (_m)[15] = 0.0; }
  /*
#define	bn_mat_zero( _m )	(void)memset( (void *)_m, 0, sizeof(mat_t))
  */
#define bn_mat_idn( _m )	{ \
	bu_log("%s:%d bn_mat_idn() is deprecated, use MAT_IDN()\n", \
			__FILE__, __LINE__); \
	(_m)[1] = (_m)[2] = (_m)[3] = (_m)[4] = \
	(_m)[6] = (_m)[7] = (_m)[8] = (_m)[9] = \
	(_m)[11] = (_m)[12] = (_m)[13] = (_m)[14] = 0.0; \
	(_m)[0] = (_m)[5] = (_m)[10] = (_m)[15] = 1.0; }
  /*
#define bn_mat_idn( _m )	(void)memcpy( (void *)_m, (const void *)bn_mat_identity, sizeof(mat_t))
  */

#define bn_mat_copy( _d, _s )	{ \
	bu_log("%s:%d bn_mat_copy() is deprecated, use MAT_COPY()\n", \
			__FILE__, __LINE__); \
	(_d)[0] = (_s)[0];\
	(_d)[1] = (_s)[1];\
	(_d)[2] = (_s)[2];\
	(_d)[3] = (_s)[3];\
	(_d)[4] = (_s)[4];\
	(_d)[5] = (_s)[5];\
	(_d)[6] = (_s)[6];\
	(_d)[7] = (_s)[7];\
	(_d)[8] = (_s)[8];\
	(_d)[9] = (_s)[9];\
	(_d)[10] = (_s)[10];\
	(_d)[11] = (_s)[11];\
	(_d)[12] = (_s)[12];\
	(_d)[13] = (_s)[13];\
	(_d)[14] = (_s)[14];\
	(_d)[15] = (_s)[15]; }
  /*
#define bn_mat_copy(_d,_s)	(void)memcpy( (void *)_d, (const void *)(_s), sizeof(mat_t))
  */
#endif /* deprecated */

BN_EXPORT BU_EXTERN(void bn_mat_mul,
		    (register mat_t o,
		     register const mat_t a,
		     register const mat_t b));
BN_EXPORT BU_EXTERN(void bn_mat_mul2,
		    (register const mat_t i,
		     register mat_t o));
BN_EXPORT BU_EXTERN(void bn_mat_mul3,
		    (mat_t o,
		     const mat_t a,
		     const mat_t b,
		     const mat_t c));
BN_EXPORT BU_EXTERN(void bn_mat_mul4,
		    (mat_t		o,
		     const mat_t	a,
		     const mat_t	b,
		     const mat_t	c,
		     const mat_t	d));
BN_EXPORT BU_EXTERN(void bn_matXvec,
		    (register hvect_t ov,
		     register const mat_t im,
		     register const hvect_t iv));
BN_EXPORT BU_EXTERN(void bn_mat_inv,
		    (register mat_t output,
		     const mat_t input));
BN_EXPORT BU_EXTERN(void bn_vtoh_move,
		    (register vect_t h,
		     register const vect_t v));
BN_EXPORT BU_EXTERN(void bn_htov_move,
		    (register vect_t v,
		     register const vect_t h));
BN_EXPORT BU_EXTERN(void bn_mat_trn,
		    (mat_t om,
		     register const mat_t im));
BN_EXPORT BU_EXTERN(void bn_mat_ae,
		    (register mat_t m,
		     double azimuth,
		     double elev));
BN_EXPORT BU_EXTERN(void bn_ae_vec,
		    (fastf_t *azp,
		     fastf_t *elp,
		     const vect_t v));
BN_EXPORT BU_EXTERN(void  bn_aet_vec,
		    (fastf_t *az,
		     fastf_t *el,
		     fastf_t *twist,
		     vect_t vec_ae,
		     vect_t vec_twist,
		     fastf_t accuracy));

BN_EXPORT BU_EXTERN(void bn_mat_angles,
		    (register mat_t mat,
		     double alpha,
		     double beta, double ggamma));
BN_EXPORT BU_EXTERN(void bn_mat_angles_rad,
		    (register mat_t mat,
		     double alpha,
		     double beta,
		     double ggamma));

BN_EXPORT BU_EXTERN(void bn_eigen2x2,
		    (fastf_t *val1,
		     fastf_t *val2,
		     vect_t vec1,
		     vect_t vec2,
		     fastf_t a,
		     fastf_t b,
		     fastf_t c));

BN_EXPORT BU_EXTERN(void bn_vec_perp,
		    (vect_t new_vec,
		     const vect_t old_vec));
BN_EXPORT BU_EXTERN(void bn_mat_fromto,
		    (mat_t m,
		     const vect_t from,
		     const vect_t to));
BN_EXPORT BU_EXTERN(void bn_mat_xrot,
		    (mat_t m,
		     double sinx,
		     double cosx));
BN_EXPORT BU_EXTERN(void bn_mat_yrot,
		    (mat_t m,
		     double siny,
		     double cosy));
BN_EXPORT BU_EXTERN(void bn_mat_zrot,
		    (mat_t m,
		     double sinz,
		     double cosz));
BN_EXPORT BU_EXTERN(void bn_mat_lookat,
		    (mat_t rot,
		     const vect_t dir,
		     int yflip));
BN_EXPORT BU_EXTERN(void bn_vec_ortho,
		    (register vect_t out,
		     register const vect_t in));
BN_EXPORT BU_EXTERN(int bn_mat_scale_about_pt,
		    (mat_t mat,
		     const point_t pt,
		     const double scale));
BN_EXPORT BU_EXTERN(void bn_mat_xform_about_pt,
		    (mat_t mat,
		     const mat_t xform,
		     const point_t pt));
BN_EXPORT BU_EXTERN(int bn_mat_is_equal,
		    (const mat_t a,
		     const mat_t b,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_mat_is_identity,
		    (const mat_t m));
BN_EXPORT BU_EXTERN(void bn_mat_arb_rot,
		    (mat_t m,
		     const point_t pt,
		     const vect_t dir,
		     const fastf_t ang));
BN_EXPORT BU_EXTERN(matp_t bn_mat_dup,
		    (const mat_t in));
BN_EXPORT BU_EXTERN(int bn_mat_ck,
		    (const char *title,
		     const mat_t m));
BN_EXPORT BU_EXTERN(fastf_t bn_mat_det3,
		    (const mat_t m));
BN_EXPORT BU_EXTERN(fastf_t bn_mat_determinant,
		    (const mat_t m));

BN_EXPORT BU_EXTERN(int bn_mat_is_non_unif,
		    (const mat_t m));
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

BN_EXPORT BU_EXTERN(struct bn_unif *bn_unif_init,
		    (long setseed,
		     int method));
BN_EXPORT BU_EXTERN(void bn_unif_free,
		    (struct bn_unif *p));
BN_EXPORT BU_EXTERN(long bn_unif_long_fill,
		    (struct bn_unif *p));
BN_EXPORT BU_EXTERN(double bn_unif_double_fill,
		    (struct bn_unif *p));
BN_EXPORT BU_EXTERN(struct bn_gauss *bn_gauss_init,
		    (long setseed,
		     int method));
BN_EXPORT BU_EXTERN(void bn_gauss_free,
		    (struct bn_gauss *p));
BN_EXPORT BU_EXTERN(double bn_gauss_fill,
		    (struct bn_gauss *p));

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

BN_EXPORT BU_EXTERN(void bn_noise_init,
		    ());
BN_EXPORT BU_EXTERN(double bn_noise_perlin,
		    (point_t pt));
/* XXX Why isn't the result listed first? */
BN_EXPORT BU_EXTERN(void bn_noise_vec,
		    (point_t point,
		     point_t result));
BN_EXPORT BU_EXTERN(double bn_noise_fbm,
		    (point_t point,
		     double h_val,
		     double lacunarity,
		     double octaves));
BN_EXPORT BU_EXTERN(double bn_noise_turb,
		    (point_t point,
		     double h_val,
		     double lacunarity,
		     double octaves));
BN_EXPORT BU_EXTERN(double bn_noise_mf,
		    (point_t point,
		     double h_val,
		     double lacunarity,
		     double octaves,
		     double offset));
BN_EXPORT BU_EXTERN(double bn_noise_ridged,
		    (point_t point,
		     double h_val,
		     double lacunarity,
		     double octaves,
		     double offset));

/*----------------------------------------------------------------------*/
/* plane.c */
/*
 * Plane/line/point calculations
 */


BN_EXPORT BU_EXTERN(int bn_distsq_line3_line3,
		    (fastf_t dist[3],
		     point_t P,
		     vect_t d,
		     point_t Q,
		     vect_t e,
		     point_t pt1,
		     point_t pt2));

BN_EXPORT BU_EXTERN(int bn_dist_pt3_lseg3,
		    (fastf_t *dist,
		     point_t pca,
		     const point_t a,
		     const point_t b,
		     const point_t p,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_3pts_collinear,
		    (point_t a,
		     point_t b,
		     point_t c,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_pt3_pt3_equal,
		    ( const point_t a,
		      const point_t b,
		      const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_dist_pt2_lseg2,
		    (fastf_t *dist_sq,
		     fastf_t pca[2],
		     const point_t a,
		     const point_t b,
		     const point_t p,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_lseg3_lseg3,
		    (fastf_t *dist,
		     const point_t p, const vect_t pdir,
		     const point_t q, const vect_t qdir,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_line3_line3,
		    (fastf_t *t, fastf_t *u,
		     const point_t p,
		     const vect_t d,
		     const point_t a,
		     const vect_t c,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_2line3_colinear,
		    (const point_t p1,
		     const vect_t d1,
		     const point_t p2,
		     const vect_t d2,
		     double range,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_pt2_lseg2,
		    (fastf_t *dist,
		     const point_t a,
		     const point_t b,
		     const point_t p,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_line2_lseg2,
		    (fastf_t *dist,
		     const point_t p,
		     const vect_t d,
		     const point_t a,
		     const vect_t c,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_lseg2_lseg2,
		    (fastf_t *dist,
		     const point_t p,
		     const vect_t pdir,
		     const point_t q,
		     const vect_t qdir,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_line2_line2,
		    (fastf_t *dist,
		     const point_t p,
		     const vect_t d,
		     const point_t a,
		     const vect_t c,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(double bn_dist_pt3_pt3,
		    (const point_t a,
		     const point_t b));
BN_EXPORT BU_EXTERN(int bn_3pts_distinct,
		    (const point_t a,
		     const point_t b,
		     const point_t c,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_mk_plane_3pts,
		    (plane_t plane,
		     const point_t a,
		     const point_t b,
		     const point_t c,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_mkpoint_3planes,
		    (point_t pt,
		     const plane_t a,
		     const plane_t b,
		     const plane_t c));
BN_EXPORT BU_EXTERN(int bn_isect_line3_plane,
		    (fastf_t *dist,
		     const point_t pt,
		     const vect_t dir,
		     const plane_t plane,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_2planes,
		    (point_t pt,
		     vect_t dir,
		     const plane_t a,
		     const plane_t b,
		     const vect_t rpp_min,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_2lines,
		    (fastf_t *t,
		     fastf_t *u,
		     const point_t p,
		     const vect_t d,
		     const point_t a,
		     const vect_t c,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_isect_line_lseg,
		    (fastf_t *t, const point_t p,
		     const vect_t d,
		     const point_t a,
		     const point_t b,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(double bn_dist_line3_pt3,
		    (const point_t pt,
		     const vect_t dir,
		     const point_t a));
BN_EXPORT BU_EXTERN(double bn_distsq_line3_pt3,
		    (const point_t pt,
		     const vect_t dir,
		     const point_t a));
BN_EXPORT BU_EXTERN(double bn_dist_line_origin,
		    (const point_t pt,
		     const vect_t dir));
BN_EXPORT BU_EXTERN(double bn_dist_line2_point2,
		    (const point_t pt,
		     const vect_t dir,
		     const point_t a));
BN_EXPORT BU_EXTERN(double bn_distsq_line2_point2,
		    (const point_t pt,
		     const vect_t dir,
		     const point_t a));
BN_EXPORT BU_EXTERN(double bn_area_of_triangle,
		    (const point_t a,
		     const point_t b,
		     const point_t c));
BN_EXPORT BU_EXTERN(int bn_isect_pt_lseg,
		    (fastf_t *dist,
		     const point_t a,
		     const point_t b,
		     const point_t p,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(double bn_dist_pt_lseg,
		    (point_t pca,
		     const point_t a,
		     const point_t b,
		     const point_t p,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(void bn_rotate_bbox,
		    (point_t omin,
		     point_t omax,
		     const mat_t mat,
		     const point_t imin,
		     const point_t imax));
BN_EXPORT BU_EXTERN(void bn_rotate_plane,
		    (plane_t oplane,
		     const mat_t mat,
		     const plane_t iplane));
BN_EXPORT BU_EXTERN(int bn_coplanar,
		    (const plane_t a,
		     const plane_t b,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(double bn_angle_measure,
		    (vect_t vec,
		     const vect_t x_dir,
		     const vect_t y_dir));
BN_EXPORT BU_EXTERN(double bn_dist_pt3_along_line3,
		    (const point_t	p,
		     const vect_t d,
		     const point_t x));
BN_EXPORT BU_EXTERN(double bn_dist_pt2_along_line2,
		    (const point_t p,
		     const vect_t d,
		     const point_t x));
BN_EXPORT BU_EXTERN(int bn_between,
		    (double left,
		     double mid,
		     double right,
		     const struct bn_tol *tol));
BN_EXPORT BU_EXTERN(int bn_does_ray_isect_tri,
		    (const point_t pt,
		     const vect_t dir,
		     const point_t V,
		     const point_t A,
		     const point_t B,
		     point_t	inter));
BN_EXPORT BU_EXTERN(int bn_hlf_class,
		    (const plane_t half_eqn,
		     const vect_t min, const vect_t max,
		     const struct bn_tol *tol));

#define BN_CLASSIFY_UNIMPLEMENTED	0x0000
#define BN_CLASSIFY_OVERLAPPING		0x0002
#define BN_CLASSIFY_INSIDE		0x0001
#define BN_CLASSIFY_OUTSIDE		0x0003

BN_EXPORT BU_EXTERN(int bn_isect_planes,
		    (point_t pt,
		     const plane_t planes[],
		     const int pl_count));

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

BN_EXPORT BU_EXTERN(struct bn_poly *bn_poly_mul,
		    (struct bn_poly *product,
		     const struct bn_poly *m1,
		     const struct bn_poly *m2));
BN_EXPORT BU_EXTERN(struct bn_poly *bn_poly_scale,
		    (struct bn_poly *eqn,
		     double factor));
BN_EXPORT BU_EXTERN(struct bn_poly *bn_poly_add,
		    (struct bn_poly *sum,
		     const struct bn_poly *poly1,
		     const struct bn_poly *poly2));
BN_EXPORT BU_EXTERN(struct bn_poly *bn_poly_sub,
		    (struct bn_poly *diff,
		     const struct bn_poly	*poly1,
		     const struct bn_poly	*poly2));
BN_EXPORT BU_EXTERN(void bn_poly_synthetic_division,
		    (struct bn_poly *quo,
		     struct bn_poly *rem,
		     const struct bn_poly	*dvdend,
		     const struct bn_poly	*dvsor));
BN_EXPORT BU_EXTERN(int bn_poly_quadratic_roots,
		    (struct bn_complex	roots[],
		     const struct bn_poly	*quadrat));
BN_EXPORT BU_EXTERN(int bn_poly_cubic_roots,
		    (struct bn_complex	roots[],
		     const struct bn_poly	*eqn));
BN_EXPORT BU_EXTERN(int bn_poly_quartic_roots,
		    (struct bn_complex	roots[],
		     const struct bn_poly	*eqn));
BN_EXPORT BU_EXTERN(void bn_pr_poly,
		    (const char *title,
		     const struct bn_poly	*eqn));
BN_EXPORT BU_EXTERN(void bn_pr_roots,
		    (const char *title,
		     const struct bn_complex roots[],
		     int n));

/*----------------------------------------------------------------------*/
/* qmath.c */
/*
 * Quaternion support
 */

BN_EXPORT BU_EXTERN(void quat_mat2quat,
		    (quat_t quat,
		     const mat_t mat));
BN_EXPORT BU_EXTERN(void quat_quat2mat,
		    (mat_t mat,
		     const quat_t quat));
BN_EXPORT BU_EXTERN(double quat_distance,
		    (const quat_t q1,
		     const quat_t q2));
BN_EXPORT BU_EXTERN(void quat_double,
		    (quat_t qout,
		     const quat_t q1,
		     const quat_t q2));
BN_EXPORT BU_EXTERN(void quat_bisect,
		    (quat_t qout,
		     const quat_t q1,
		     const quat_t q2));
BN_EXPORT BU_EXTERN(void quat_slerp,
		    (quat_t qout,
		     const quat_t q1,
		     const quat_t q2,
		     double f));
BN_EXPORT BU_EXTERN(void quat_sberp,
		    (quat_t qout,
		     const quat_t q1,
		     const quat_t qa,
		     const quat_t qb,
		     const quat_t q2,
		     double f));
BN_EXPORT BU_EXTERN(void quat_make_nearest,
		    (quat_t q1,
		     const quat_t q2));
BN_EXPORT BU_EXTERN(void quat_print,
		    (const char *title,
		     const quat_t quat));
BN_EXPORT BU_EXTERN(void quat_exp,
		    (quat_t out,
		     const quat_t in));
BN_EXPORT BU_EXTERN(void quat_log,
		    (quat_t out,
		     const quat_t in));
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
BN_EXPORT extern const float bn_rand_table[BN_RAND_TABSIZE];

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
BN_EXPORT extern int bn_randhalftabsize;
BN_EXPORT extern float bn_rand_halftab[BN_RANDHALFTABSIZE];

#define bn_rand_half(_p)	\
	( (++(_p) >= &bn_rand_halftab[bn_randhalftabsize] || \
	     (_p) < bn_rand_halftab) ? \
		*((_p) = bn_rand_halftab) : *(_p))

#define bn_rand_init(_p, _seed)	\
	(_p) = &bn_rand_halftab[ \
		(int)( \
		      (bn_rand_halftab[(_seed)%bn_randhalftabsize] + 0.5) * \
		      (bn_randhalftabsize-1)) ]

/* random numbers 0..1 except when benchmarking, when this is always 0.5 */
#define bn_rand0to1(_q)	(bn_rand_half(_q)+0.5)

#define	BN_SINTABSIZE		2048
BN_EXPORT extern double bn_sin_scale;
#define bn_tab_sin(_a)	(((_a) > 0) ? \
	( bn_sin_table[(int)((0.5+ (_a)*bn_sin_scale))&(BN_SINTABSIZE-1)] ) :\
	(-bn_sin_table[(int)((0.5- (_a)*bn_sin_scale))&(BN_SINTABSIZE-1)] ))
BN_EXPORT extern const float bn_sin_table[BN_SINTABSIZE];

BN_EXPORT extern void bn_mathtab_constant();



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
		}\
}

BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_double_decompose,
		    (double *tbuf,
		     double *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_double_reconstruct,
		    (double *tbuf,
		     double *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_float_decompose,
		    (float *tbuf,
		     float *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_float_reconstruct,
		    (float *tbuf,
		     float *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_char_decompose,
		    (char *tbuf,
		     char *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_char_reconstruct,
		    (char *tbuf, char *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_short_decompose,
		    (short *tbuf, short *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_short_reconstruct,
		    (short *tbuf, short *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_int_decompose,
		    (int *tbuf, int *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_int_reconstruct,
		    (int *tbuf,
		     int *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_long_decompose,
		    (long *tbuf, long *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_1d_long_reconstruct,
		    (long *tbuf, long *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));



BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_double_decompose,
		    (double *tbuf,
		     double *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_double_reconstruct,
		    (double *tbuf,
		     double *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_float_decompose,
		    (float *tbuf,
		     float *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_float_reconstruct,
		    (float *tbuf,
		     float *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_char_decompose,
		    (char *tbuf,
		     char *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_char_reconstruct,
		    (char *tbuf,
		     char *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_short_decompose,
		    (short *tbuf,
		     short *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_short_reconstruct,
		    (short *tbuf,
		     short *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_int_decompose,
		    (int *tbuf,
		     int *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_int_reconstruct,
		    (int *tbuf,
		     int *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_long_decompose,
		    (long *tbuf,
		     long *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_long_reconstruct,
		    (long *tbuf,
		     long *buf,
		     unsigned long dimen,
		     unsigned long depth,
		     unsigned long subimage_size,
		     unsigned long limit));



BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_double_decompose2,
		    (double *tbuf,
		     double *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_double_reconstruct2,
		    (double *tbuf,
		     double *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_float_decompose2,
		    (float *tbuf,
		     float *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_float_reconstruct2,
		    (float *tbuf,
		     float *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_char_decompose2,
		    (char *tbuf,
		     char *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_char_reconstruct2,
		    (char *tbuf,
		     char *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_short_decompose2,
		    (short *tbuf,
		     short *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_short_reconstruct2,
		    (short *tbuf,
		     short *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_int_decompose2,
		    (int *tbuf,
		     int *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_int_reconstruct2,
		    (int *tbuf,
		     int *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long subimage_size,
		     unsigned long limit));

BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_long_decompose2,
		    (long *tbuf,
		     long *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long limit));
BN_EXPORT BU_EXTERN(void bn_wlt_haar_2d_long_reconstruct2,
		    (long *tbuf,
		     long *buf,
		     unsigned long dimen,
		     unsigned long width,
		     unsigned long height,
		     unsigned long subimage_size,
		     unsigned long limit));


/*----------------------------------------------------------------------*/
/* const.c */
BN_EXPORT extern const double bn_pi;
BN_EXPORT extern const double bn_twopi;
BN_EXPORT extern const double bn_halfpi;
BN_EXPORT extern const double bn_invpi;
BN_EXPORT extern const double bn_inv2pi;
BN_EXPORT extern const double bn_inv255;
BN_EXPORT extern const double bn_degtorad;
BN_EXPORT extern const double bn_radtodeg;

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
 *	spectrum.h, spectrum.c
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
#ifndef NO_BOMBING_MACROS
#  define BN_GET_TABLE(_table, _nx)  { \
	if( (_nx) < 1 )  bu_bomb("RT_GET_TABLE() _nx < 1\n"); \
	_table = (struct bn_table *)bu_calloc( 1, \
		sizeof(struct bn_table) + sizeof(fastf_t)*(_nx), \
		"struct bn_table" ); \
	_table->magic = BN_TABLE_MAGIC; \
	_table->nx = (_nx);  }
#else
#  define BN_GET_TABLE(_table, _nx)  { \
	_table = (struct bn_table *)bu_calloc( 1, \
		sizeof(struct bn_table) + sizeof(fastf_t)*(_nx), \
		"struct bn_table" ); \
	_table->magic = BN_TABLE_MAGIC; \
	_table->nx = (_nx);  }
#endif

struct bn_tabdata {
	long		magic;
	int		ny;
	const struct bn_table *table;	/* Up pointer to definition of X axis */
	fastf_t		y[1];		/* array of ny samples, dynamically sized */
};
#define BN_TABDATA_MAGIC	0x53736d70
#define BN_CK_TABDATA(_p)	BU_CKMAG(_p, BN_TABDATA_MAGIC, "bn_tabdata")
#define BN_TABDATA_NULL		((struct bn_tabdata *)NULL)

#define BN_SIZEOF_TABDATA_Y(_tabdata)	sizeof(fastf_t)*((_tabdata)->ny)
#define BN_SIZEOF_TABDATA(_table)	( sizeof(struct bn_tabdata) + \
			sizeof(fastf_t)*((_table)->nx-1))

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

BN_EXPORT BU_EXTERN(void bn_table_free,
		    (struct bn_table *tabp));
BN_EXPORT BU_EXTERN(void bn_tabdata_free,
		    (struct bn_tabdata *data));
BN_EXPORT BU_EXTERN(void bn_ck_table,
		    (const struct bn_table *tabp));
BN_EXPORT BU_EXTERN(struct bn_table *bn_table_make_uniform,
		    (int num,
		     double first,
		     double last));
BN_EXPORT BU_EXTERN(void bn_tabdata_add,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     const struct bn_tabdata *in2));
BN_EXPORT BU_EXTERN(void bn_tabdata_mul,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     const struct bn_tabdata *in2));
BN_EXPORT BU_EXTERN(void bn_tabdata_mul3,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     const struct bn_tabdata *in2,
		     const struct bn_tabdata *in3));
BN_EXPORT BU_EXTERN(void bn_tabdata_incr_mul3_scale,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     const struct bn_tabdata *in2,
		     const struct bn_tabdata *in3,
		     double scale));
BN_EXPORT BU_EXTERN(void bn_tabdata_incr_mul2_scale,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     const struct bn_tabdata *in2,
		     double scale));
BN_EXPORT BU_EXTERN(void bn_tabdata_scale,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     double scale));
BN_EXPORT BU_EXTERN(void bn_table_scale,
		    (struct bn_table *tabp,
		     double scale));
BN_EXPORT BU_EXTERN(void bn_tabdata_join1,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     double scale,
		     const struct bn_tabdata *in2));
BN_EXPORT BU_EXTERN(void bn_tabdata_join2,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in1,
		     double scale2,
		     const struct bn_tabdata *in2,
		     double scale3,
		     const struct bn_tabdata *in3));
BN_EXPORT BU_EXTERN(void bn_tabdata_blend2,
		    (struct bn_tabdata *out,
		     double scale1,
		     const struct bn_tabdata *in1,
		     double scale2,
		     const struct bn_tabdata *in2));
BN_EXPORT BU_EXTERN(void bn_tabdata_blend3,
		    (struct bn_tabdata *out,
		     double scale1,
		     const struct bn_tabdata *in1,
		     double scale2,
		     const struct bn_tabdata *in2,
		     double scale3,
		     const struct bn_tabdata *in3));
BN_EXPORT BU_EXTERN(double bn_tabdata_area1,
		    (const struct bn_tabdata *in));
BN_EXPORT BU_EXTERN(double bn_tabdata_area2,
		    (const struct bn_tabdata *in));
BN_EXPORT BU_EXTERN(double bn_tabdata_mul_area1,
		    (const struct bn_tabdata *in1,
		     const struct bn_tabdata *in2));
BN_EXPORT BU_EXTERN(double bn_tabdata_mul_area2,
		    (const struct bn_tabdata *in1,
		     const struct bn_tabdata *in2));
BN_EXPORT BU_EXTERN(fastf_t bn_table_lin_interp,
		    (const struct bn_tabdata *samp,
		     double wl));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_resample_max,
		    (const struct bn_table *newtable,
		     const struct bn_tabdata *olddata));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_resample_avg,
		    (const struct bn_table *newtable,
		     const struct bn_tabdata *olddata));
BN_EXPORT BU_EXTERN(int bn_table_write,
		    (const char *filename,
		     const struct bn_table *tabp));
BN_EXPORT BU_EXTERN(struct bn_table *bn_table_read,
		    (const char *filename));
BN_EXPORT BU_EXTERN(void bn_pr_table,
		    (const char *title,
		     const struct bn_table *tabp));
BN_EXPORT BU_EXTERN(void bn_pr_tabdata,
		    (const char *title,
		     const struct bn_tabdata *data));
BN_EXPORT BU_EXTERN(int bn_print_table_and_tabdata,
		    (const char *filename,
		     const struct bn_tabdata *data));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_read_table_and_tabdata,
		    (const char *filename));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_binary_read,
		    (const char *filename,
		     int num,
		     const struct bn_table *tabp));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_malloc_array,
		    (const struct bn_table *tabp,
		     int num));
BN_EXPORT BU_EXTERN(void bn_tabdata_copy,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_dup,
		    (const struct bn_tabdata *in));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_get_constval,
		    (double val,
		     const struct bn_table *tabp));
BN_EXPORT BU_EXTERN(void bn_tabdata_constval,
		    (struct bn_tabdata *data,
		     double val));
BN_EXPORT BU_EXTERN(void bn_tabdata_to_tcl,
		    (struct bu_vls *vp,
		     const struct bn_tabdata *data));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_from_array,
		    (const double *array));
BN_EXPORT BU_EXTERN(void bn_tabdata_freq_shift,
		    (struct bn_tabdata *out,
		     const struct bn_tabdata *in,
		     double offset));
BN_EXPORT BU_EXTERN(int bn_table_interval_num_samples,
		    (const struct bn_table *tabp,
		     double low,
		     double hi));
BN_EXPORT BU_EXTERN(int bn_table_delete_sample_pts,
		    (struct bn_table *tabp,
		     int i,
		     int j));
BN_EXPORT BU_EXTERN(struct bn_table *bn_table_merge2,
		    (const struct bn_table *a,
		     const struct bn_table *b));
BN_EXPORT BU_EXTERN(struct bn_tabdata *bn_tabdata_mk_linear_filter,
		    (const struct bn_table *spectrum,
		     double lower_wavelen,
		     double upper_wavelen));

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

BN_EXPORT BU_EXTERN(void bn_vlist_3string,
		    (struct bu_list *vhead,
		     struct bu_list *free_hd,
		     const char *string,
		     const point_t origin,
		     const mat_t rot,
		     double scale));
BN_EXPORT BU_EXTERN(void bn_vlist_2string,
		    (struct bu_list *vhead,
		     struct bu_list *free_hd,
		     const char *string,
		     double x,
		     double y,
		     double scale,
		     double theta));


/*----------------------------------------------------------------------*/
/* vert_tree.c */
/*
 * vertex tree support
 */
/* packaging structure
 * holds all the required info for a single vertex tree
 */
struct vert_root {
	long magic;
	int tree_type;			/* vertices or vertices with normals */
	union vert_tree *the_tree;	/* the actual vertex tree */
	fastf_t *the_array;		/* the array of vertices */
	unsigned long curr_vert;	/* the number of vertices currently in the array */
	unsigned long max_vert;		/* the current maximum capacity of the array */
};

#define TREE_TYPE_VERTS			1
#define TREE_TYPE_VERTS_AND_NORMS	2

#define VERT_BLOCK 512			/* number of vertices to malloc per call when building the array */

#define VERT_TREE_MAGIC	0x56455254	/* "VERT" */
#define BN_CK_VERT_TREE(_p)	BU_CKMAG(_p, VERT_TREE_MAGIC, "vert_tree")

BN_EXPORT BU_EXTERN(struct vert_root *create_vert_tree,
		    ());
BN_EXPORT BU_EXTERN(struct vert_root *create_vert_tree_w_norms,
		    ());
BN_EXPORT BU_EXTERN(void free_vert_tree,
		    (struct vert_root *tree_root));
BN_EXPORT BU_EXTERN(int Add_vert,
		    (double x,
		     double y,
		     double z,
		     struct vert_root *tree_root,
		     fastf_t local_tol_sq));
BN_EXPORT BU_EXTERN(int Add_vert_and_norm,
		    (double x,
		     double y,
		     double z,
		     double nx,
		     double ny,
		     double nz,
		     struct vert_root *tree_root,
		     fastf_t local_tol_sq));
BN_EXPORT BU_EXTERN(void clean_vert_tree,
		    (struct vert_root *tree_root));

/*----------------------------------------------------------------------*/
/* vectfont.c */
BN_EXPORT BU_EXTERN(void tp_setup,
		    ());


/*----------------------------------------------------------------------*/
/* vers.c (created by the Cakefile) */
BN_EXPORT extern const char		bn_version[];
/*----------------------------------------------------------------------*/

__END_DECLS

#endif /* SEEN_BN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
