/*
 *			P L O T 3 . H
 *
 *
 * This is a ANSI C header for LIBPLOT3 giving function prototypes.
 * This header file will also work if called by a "traditional" C
 * compiler.
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

#endif /* PLOT3_H */
