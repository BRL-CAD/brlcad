/*
 *			P L O T 3 . H
 *
 *
 * This is a ANSI C header for LIBPLOT3 giving function prototypes.
 * This header file will also work if called by a "traditional" C
 * compiler.
 *
 *  For best results, before the #include "plot3.h", there should be:
 *
 *		#include "machine.h"
 *    and	#include "vmath.h"
 *
 *  although not doing this should not be fatal.
 *
 *	$Header$
 */
#ifndef	PLOT3_H
#define	PLOT3_H

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 *  The setting of USE_PROTOTYPES is done in machine.h
 */
#if USE_PROTOTYPES
#	define	PL_EXTERN(type_and_name,args)	extern type_and_name args
#	define	PL_ARGS(args)			args
#else
#	define	PL_EXTERN(type_and_name,args)	extern type_and_name()
#	define	PL_ARGS(args)			()
#endif

#define	pl_mat_idn( _mat )		MAT_IDN( _mat )
#define pl_mat_zero( _mat )		MAT_ZERO( _mat )
#define pl_mat_copy( _mat1, _mat2 )	MAT_COPY( _mat1, _mat2 )

/*
 *  The basic UNIX-plot routines.
 *  The calling sequence is the same as the original Bell Labs routines,
 *  with the exception of the pl_ prefix on the name.
 */
PL_EXTERN(void pl_point, (FILE *plotfp, int x, int y));
PL_EXTERN(void pl_line, (FILE *plotfp, int fx, int fy, int tx, int ty));
PL_EXTERN(void pl_linmod, (FILE *plotfp, char *s));
PL_EXTERN(void pl_move, (FILE *plotfp, int x, int y));
PL_EXTERN(void pl_cont, (FILE *plotfp, int x, int y));
PL_EXTERN(void pl_label, (FILE *plotfp, char *s));
PL_EXTERN(void pl_space, (FILE *plotfp, int x1, int y1, int x2, int y2));
PL_EXTERN(void pl_erase, (FILE *plotfp));
PL_EXTERN(void pl_circle, (FILE *plotfp, int x, int y, int r));
PL_EXTERN(void pl_arc, (FILE *plotfp, int xc, int yc, int x1, int y1, int x2, int y2));
PL_EXTERN(void pl_box, (FILE *plotfp, int x1, int y1, int x2, int y2));

/*
 * BRL extensions to the UNIX-plot file format.
 */
PL_EXTERN(void pl_color, (FILE *plotfp, int r, int g, int b));
PL_EXTERN(void pl_flush, (FILE *plotfp));
PL_EXTERN(void pl_3space, (FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2));
PL_EXTERN(void pl_3point, (FILE *plotfp, int x, int y, int z));
PL_EXTERN(void pl_3move, (FILE *plotfp, int x, int y, int z));
PL_EXTERN(void pl_3cont, (FILE *plotfp, int x, int y, int z));
PL_EXTERN(void pl_3line, (FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2));
PL_EXTERN(void pl_3box, (FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2));

/* Double floating point versions */
PL_EXTERN(void pd_point, (FILE *plotfp, double x, double y));
PL_EXTERN(void pd_line, (FILE *plotfp, double fx, double fy, double tx, double ty));
PL_EXTERN(void pd_move, (FILE *plotfp, double x, double y));
PL_EXTERN(void pd_cont, (FILE *plotfp, double x, double y));
PL_EXTERN(void pd_space, (FILE *plotfp, double x1, double y1, double x2, double y2));
PL_EXTERN(void pd_circle, (FILE *plotfp, double x, double y, double r));
PL_EXTERN(void pd_arc, (FILE *plotfp, double xc, double yc, double x1, double y1, double x2, double y2));
PL_EXTERN(void pd_box, (FILE *plotfp, double x1, double y1, double x2, double y2));

/* Double 3-D both in vector and enumerated versions */
#ifdef VMATH_H
PL_EXTERN(void pdv_3space, (FILE *plotfp, vect_t min, vect_t max));
PL_EXTERN(void pdv_3point, (FILE *plotfp, vect_t pt));
PL_EXTERN(void pdv_3move, (FILE *plotfp, vect_t pt));
PL_EXTERN(void pdv_3cont, (FILE *plotfp, vect_t pt));
PL_EXTERN(void pdv_3line, (FILE *plotfp, vect_t a, vect_t b));
PL_EXTERN(void pdv_3box, (FILE *plotfp, vect_t a, vect_t b));
#endif /* VMATH_H */
PL_EXTERN(void pd_3space, (FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2));
PL_EXTERN(void pd_3point, (FILE *plotfp, double x, double y, double z));
PL_EXTERN(void pd_3move, (FILE *plotfp, double x, double y, double z));
PL_EXTERN(void pd_3cont, (FILE *plotfp, double x, double y, double z));
PL_EXTERN(void pd_3line, (FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2));
PL_EXTERN(void pd_3box, (FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2));

/*
 *  The following routines are taken from the BRL TIG-PACK
 *  (Terminal Independent Plotting Package).
 *  These routines create plots by using the pl_() and pd_() routines
 *  declared above.
 */

/*
 *			P L _ F O R T R A N
 *
 *  This macro is used to take the 'C' function name,
 *  and convert it at compile time to the
 *  FORTRAN calling convention used for this particular system.
 *
 *  Both lower-case and upper-case alternatives have to be provided
 *  because there is no way to get the C preprocessor to change the
 *  case of a token.
 */
#if CRAY
#	define	PL_FORTRAN(lc,uc)	uc
#endif
#if defined(apollo) || defined(mips) || defined(aux)
	/* Lower case, with a trailing underscore */
#ifdef __STDC__
#	define	PL_FORTRAN(lc,uc)	lc ## _
#else
#	define	PL_FORTRAN(lc,uc)	lc/**/_
#endif
#endif
#if !defined(PL_FORTRAN)
#	define	PL_FORTRAN(lc,uc)	lc
#endif

PL_EXTERN(void tp_i2list, (FILE *fp, int *x, int *y, int npoints));
PL_EXTERN(void tp_2list, (FILE *fp, double *x, double *y, int npoints));
PL_EXTERN(void PL_FORTRAN(f2list, F2LIST), (FILE **fpp, float *x, float *y,
			int *n));
PL_EXTERN(void tp_3list, (FILE *fp, double *x, double *y, double *z,
			int npoints));
PL_EXTERN(void PL_FORTRAN(f3list, F3LIST), (FILE **fpp, float *x, float *y,
			float *z, int *n));
PL_EXTERN(void tp_2mlist, (FILE *fp, double *x, double *y, int npoints,
			int flag, int mark, int interval, double size));
PL_EXTERN(void PL_FORTRAN(f2mlst, F2MLST), (FILE **fp, float *x, float *y,
			int *np, int *flag, int *mark, int *interval,

			float *size));
PL_EXTERN(void tp_2marker, (FILE *fp, int c, double x, double y, double scale));
PL_EXTERN(void PL_FORTRAN(f2mark, F2MARK), (FILE **fp, int *c, float *x,
			float *y, float *scale));
PL_EXTERN(void tp_3marker, (FILE *fp, int c, double x, double y, double z,
			double scale));
PL_EXTERN(void PL_FORTRAN(f3mark, F3MARK), (FILE **fp, int *c, float *x,
			float *y, float *z, float *scale));
PL_EXTERN(void tp_2number, (FILE *fp, double input, int x, int y, int cscale,
			double theta, int digits));
PL_EXTERN(void PL_FORTRAN(f2numb, F2NUMB), (FILE **fp, float *input,
			int *x, int *y, float *cscale, float *theta, int *digits));
PL_EXTERN(void tp_scale, (int idata[], int elements, int mode, int length,
			int odata[], double *min, double *dx));

PL_EXTERN(void PL_FORTRAN(fscale, FSCALE), (int	idata[], int *elements,
			char *mode, int *length, int odata[], double *min,
			double *dx));
PL_EXTERN(void tp_2symbol, (FILE *fp, char *string, double x, double y,
			double scale, double theta));
PL_EXTERN(void PL_FORTRAN(f2symb, F2SYMB), (FILE **fp, char *string,
			float *x, float *y, float *scale, float *theta));
PL_EXTERN(void tp_plot, (FILE *fp, int xp, int yp, int xl, int yl,
			char xtitle[], char ytitle[], float x[], float y[],
			int n, double cscale));
PL_EXTERN(void PL_FORTRAN(fplot, FPLOT), (FILE **fp, int *xp, int *yp,
			int *xl, int *yl, char *xtitle, char *ytitle,
			float *x, float *y, int *n, float *cscale));
#ifdef VMATH_H
PL_EXTERN(void tp_3axis, (FILE *fp, char *string, point_t origin,
			mat_t rot, double length, int ccw, int ndigits,
			double label_start, double label_incr,
			double tick_separation, double char_width));
PL_EXTERN(void PL_FORTRAN(f3axis, F3AXIS), (FILE **fp, char *string,
			float *x, float *y, float *z,
			float *length, float *theta,
			int *ccw, int *ndigits,
			float *label_start, float *label_incr,
			float *tick_separation, float *char_width));
PL_EXTERN(void tp_3symbol, (FILE *fp, char *string, point_t origin,
			mat_t rot, double scale));
PL_EXTERN(void tp_3vector, (FILE *plotfp, point_t from, point_t to,
			double fromheadfract, double toheadfract));
PL_EXTERN(void PL_FORTRAN(f3vect, F3VECT), (FILE **fp,
			float *fx, float *fy, float *fz,
			float *tx, float *ty, float *tz,
			float *fl, float *tl));
#endif /* VMATH_H */

#endif /* PLOT3_H */
