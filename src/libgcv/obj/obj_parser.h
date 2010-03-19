/*                   O B J _ P A R S E R . H
 * BRL-CAD
 *
 * Copyright (c) 2010 United States Government as represented by
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

#include "common.h" 
#include "vmath.h"

#undef YYSTYPE
#define YYSTYPE point_line_t

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

#include "./obj_grammar.h"

/* Freeform curve and surface types */
#define BEZIER 1
#define BASISMATRIX 2
#define BSPLINE 3
#define CARDINAL 4
#define TAYLOR 5

/* Basic 3D vertices */

struct obj_vertices {
    point_t *geometric;
    point_t *texture;
    vect_t *vertex_norm;
    point_t *control;
}

/* Elements */

struct obj_point {
    point_t *vertex; 
}

struct obj_line {
    int v_count;
    int t_count;
    point_t **vertex;
    point_t **texture;
}

struct obj_face {
    int v_count;
    int t_count;
    int n_count;
    point_t **vertex;
    point_t **texture;
    vect_t **normal;
}


/* Curves and Surfaces */

struct obj_freeform_curve {
    int type;
    int rational;
    int degree_u;
    int step_u;
    fastf_t *basis_matrix;
}

struct obj_freeform_surface {
    int type;
    int rational;
    int degree_u;
    int degree_v;
    int step_u;
    int step_v;
    fastf_t *basis_matrix;
}

/* Elements */

struct obj_point {
}

struct obj_line {
}

struct obj_face {
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
