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

#if __STDC__ || USE_PROTOTYPES
extern void pl_point(  FILE *plotfp, int x, int y);
extern void pl_line(   FILE *plotfp, int fx, int fy, int tx, int ty);
extern void pl_linmod( FILE *plotfp, char *s);
extern void pl_move(   FILE *plotfp, int x, int y);
extern void pl_cont(   FILE *plotfp, int x, int y);
extern void pl_label(  FILE *plotfp, char *s);
extern void pl_space(  FILE *plotfp, int x1, int y1, int x2, int y2);
extern void pl_erase(  FILE *plotfp);
extern void pl_circle( FILE *plotfp, int x, int y, int r);
extern void pl_arc(    FILE *plotfp, int xc, int yc, int x1, int y1, int x2, int y2);
extern void pl_box(    FILE *plotfp, int x1, int y1, int x2, int y2);
/* BRL extensions. */
extern void pl_color(  FILE *plotfp, int r, int g, int b);
extern void pl_flush(  FILE *plotfp);
extern void pl_3space( FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2);
extern void pl_3point( FILE *plotfp, int x, int y, int z);
extern void pl_3move(  FILE *plotfp, int x, int y, int z);
extern void pl_3cont(  FILE *plotfp, int x, int y, int z);
extern void pl_3line(  FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2);
extern void pl_3box(   FILE *plotfp, int x1, int y1, int z1, int x2, int y2, int z2);
/* Double floating point versions */
extern void pd_point(  FILE *plotfp, double x, double y);
extern void pd_line(   FILE *plotfp, double fx, double fy, double tx, double ty);
extern void pd_move(   FILE *plotfp, double x, double y);
extern void pd_cont(   FILE *plotfp, double x, double y);
extern void pd_space(  FILE *plotfp, double x1, double y1, double x2, double y2);
extern void pd_circle( FILE *plotfp, double x, double y, double r);
extern void pd_arc(    FILE *plotfp, double xc, double yc, double x1, double y1, double x2, double y2);
extern void pd_box(    FILE *plotfp, double x1, double y1, double x2, double y2);
/* Double 3-D both in vector and enumerated versions */
#ifdef VMATH_H
extern void pdv_3space(FILE *plotfp, vect_t min, vect_t max);
extern void pdv_3point(FILE *plotfp, vect_t pt);
extern void pdv_3move( FILE *plotfp, vect_t pt);
extern void pdv_3cont( FILE *plotfp, vect_t pt);
extern void pdv_3line( FILE *plotfp, vect_t a, vect_t b);
extern void pdv_3box(  FILE *plotfp, vect_t a, vect_t b);
#endif /* VMATH_H */
extern void pd_3space( FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2);
extern void pd_3point( FILE *plotfp, double x, double y, double z);
extern void pd_3move(  FILE *plotfp, double x, double y, double z);
extern void pd_3cont(  FILE *plotfp, double x, double y, double z);
extern void pd_3line(  FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2);
extern void pd_3box(   FILE *plotfp, double x1, double y1, double z1, double x2, double y2, double z2);

#else /* __STDC__ */

extern void pl_point();
extern void pl_line();
extern void pl_linmod();
extern void pl_move();
extern void pl_cont();
extern void pl_label();
extern void pl_space();
extern void pl_erase();
extern void pl_circle();
extern void pl_arc();
extern void pl_box();
/* BRL extensions. */
extern void pl_color();
extern void pl_flush();
extern void pl_3space();
extern void pl_3point();
extern void pl_3move();
extern void pl_3cont();
extern void pl_3line();
extern void pl_3box();
/* Double floating point versions */
extern void pd_point();
extern void pd_line();
extern void pd_move();
extern void pd_cont();
extern void pd_space();
extern void pd_circle();
extern void pd_arc();
extern void pd_box();
/* Double 3-D both in vector and enumerated versions */
extern void pdv_3space();
extern void pdv_3point();
extern void pdv_3move();
extern void pdv_3cont();
extern void pdv_3line();
extern void pdv_3box();
extern void pd_3space();
extern void pd_3point();
extern void pd_3move();
extern void pd_3cont();
extern void pd_3line();
extern void pd_3box();
#endif /* __STDC__ */
#endif /* VMATH_H */
