/*                          I G E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef CONV_IGES_IGES_H
#define CONV_IGES_IGES_H

#define CSG_MODE 1
#define FACET_MODE 2
#define TRIMMED_SURF_MODE 3

#define NAMESIZE 16 /* from db.h */

struct iges_properties
{
    char name[NAMESIZE+1];
    char material_name[32];
    char material_params[60];
    char region_flag;
    short ident;
    short air_code;
    short material_code;
    short los_density;
    short inherit;
    short color_defined;
    unsigned char color[3];

};

/* Low-level IGES entity writers used by the faithful brep (OpenNURBS)
 * exporter.  Defined in iges.c (which owns the directory/parameter section
 * sequence counters); declared here with C linkage so the C++ ON_Brep
 * walker in brep_iges.cpp can call them.  Each returns the directory entry
 * (DE) sequence number of the entity written.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* IGES 128 Rational B-Spline Surface.  k1/k2 are the upper control-point
 * indices (n_u_cv-1, n_v_cv-1); m1/m2 the u/v degrees.  Knot arrays hold
 * k+m+2 clamped values; weights and euclidean xyz control points are in
 * IGES order (u index fastest, then v). */
extern int write_nurb_surface_entity(int k1, int k2, int m1, int m2,
				     int rational,
				     const double *uknots, const double *vknots,
				     const double *weights, const double *ctlpts,
				     double u0, double u1, double v0, double v1,
				     FILE *fp_dir, FILE *fp_param);

/* IGES 126 Rational B-Spline Curve.  k is the upper control-point index
 * (n_cv-1), m the degree; knots hold k+m+2 values, ctlpts are euclidean
 * xyz triples (z=0 for 2d parameter-space curves). */
extern int write_nurb_curve_entity(int k, int m, int rational, int planar,
				   const double *knots, const double *weights,
				   const double *ctlpts, double v0, double v1,
				   double nx, double ny, double nz,
				   FILE *fp_dir, FILE *fp_param);

/* IGES 102 Composite Curve referencing @p n member curve DEs. */
extern int write_composite_curve_entity(const int *members, int n,
					FILE *fp_dir, FILE *fp_param);

/* IGES 142 Curve on a Parametric Surface (surface DE, parameter-space
 * curve DE, model-space curve DE). */
extern int write_curve_on_surface_entity(int surf_de, int bcurve_de,
					 int ccurve_de,
					 FILE *fp_dir, FILE *fp_param);

/* IGES 144 Trimmed (Parametric) Surface (surface DE, outer boundary 142
 * DE, and @p n_inner inner boundary 142 DEs). */
extern int write_trimmed_surface_entity(int surf_de, int outer_de,
					const int *inner_des, int n_inner,
					FILE *fp_dir, FILE *fp_param);

#ifdef __cplusplus
}
#endif

#endif /* CONV_IGES_IGES_H */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
