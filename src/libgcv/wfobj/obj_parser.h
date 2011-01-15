/*                   O B J _ P A R S E R . H
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef OBJ_PARSER_H
#define OBJ_PARSER_H
#endif

#include "common.h" 
#include "vmath.h"


/* Freeform curve and surface types */
#define BEZIER 1
#define BASISMATRIX 2
#define BSPLINE 3
#define CARDINAL 4
#define TAYLOR 5

/* Basic 3D vertices */

typedef struct ctrl_point {
    point_t pt;
    fastf_t w;
} ctrl_point_t;

typedef struct obj_vertices {
    int v_count;
    int v_max;
    point_t *geometric;
    int t_count;
    int t_max; 
    point_t *texture;
    int n_count;
    int n_max;
    vect_t *vertex_norm;
    int c_count;
    int c_max; 
    ctrl_point_t *control;
} obj_vertices_t;

/* Elements */

typedef struct obj_line {
    int v_count;
    int v_max;
    int t_count;
    int t_max;
    point_t **vertex;
    point_t **texture;
} obj_line_t;

typedef struct obj_face {
    int v_count;
    int v_max;
    int t_count;
    int t_max;
    int n_count;
    int n_max;
    point_t **vertex;
    point_t **texture;
    vect_t **normal;
} obj_face_t;

typedef struct obj_elements {
    int p_count;
    int p_max;
    point_t *points;
    int l_count;
    int l_max;
    obj_line_t *lines;
    int f_count;
    int f_max;
    obj_face_t *faces;
} obj_elements_t;

/* Curves and Surfaces */

typedef struct obj_freeform_curve {
    int type;
    int rational;
    int degree_u;
    int step_u;
    fastf_t *basis_matrix;
    fastf_t u0;
    fastf_t u1;
    ctrl_point_t **control;
    fastf_t *param_u;
} obj_freeform_curve_t;

typedef struct obj_trim {
    int type; /* 0 = hole, 1 = trim */
    obj_freeform_curve_t *curve;
    fastf_t u0;
    fastf_t u1;
} obj_trim_t;

typedef struct obj_trim_loop {
    obj_trim_t **trims;
} obj_trim_loop_t;

typedef struct obj_freeform_surface {
    int type;
    int rational;
    int degree_u;
    int degree_v;
    int step_u;
    int step_v;
    fastf_t *basis_matrix;
    fastf_t s0;
    fastf_t s1;
    fastf_t t0;
    fastf_t t1;
    ctrl_point_t **control;
    point_t **texture;
    vect_t **normal;
    fastf_t *param_u;
    fastf_t *param_v;
    obj_trim_loop_t *loops;
} obj_freeform_surface_t;

typedef struct obj_group {
    int active;
    int l_count;
    int l_max;
    obj_line_t *lines;
    int f_count;
    int f_max;
    obj_face_t *faces;
} obj_group_t;

typedef struct obj_groups {
    int g_count;
    int g_max;
    obj_group_t *groups;
} obj_groups_t;

typedef struct obj_data {
    float real;
    int integer;
    int reference[3];
    int toggle;
    size_t index;
} obj_data_t;

#undef YYSTYPE
#define YYSTYPE obj_data_t

#ifndef YY_STACK_USED
#  define YY_STACK_USED 0
#endif
#ifndef YY_STACK_UNUSED
#  define YY_STACK_UNUSED 0
#endif
#ifndef YY_ALWAYS_INTERACTIVE
#  define YY_ALWAYS_INTERACTIVE 0
#endif
#ifndef YY_NEVER_INTERACTIVE
#  define YY_NEVER_INTERACTIVE 0
#endif
#ifndef YY_MAIN
#  define YY_MAIN 0
#endif
										      

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
