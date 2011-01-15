/*                       O B J _ P A R S E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 *
 */
/** @file obj_parser.c
 *
 * Handling routines for obj parsing
 *
 */

/* interface header */
#include "./obj_parser.h"

#include <stdio.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#else
#  ifdef HAVE_INTTYPES_H
#    include <inttypes.h>
#  endif
#endif

#include "bu.h"
#include "vmath.h"


#define INITIAL_SIZE 100


extern FILE *yyin;
extern int yyparse (void);

obj_vertices_t obj_global_vertices;
obj_elements_t obj_global_elements;
obj_groups_t obj_global_groups;


int
obj_add_vertex(int type, fastf_t x, fastf_t y, fastf_t z)
{
    int *curr;
    int *max;
    point_t **array;
    switch (type) {
	case 1:
	    curr = &(obj_global_vertices.t_count);
	    max = &(obj_global_vertices.t_max);
	    array = &(obj_global_vertices.texture);
	    break;
	case 2:
	    curr = &(obj_global_vertices.n_count);
	    max = &(obj_global_vertices.n_max);
	    array = &(obj_global_vertices.vertex_norm);
	    break;
   	default:
	    curr = &(obj_global_vertices.v_count);
	    max = &(obj_global_vertices.v_max);
	    array = &(obj_global_vertices.geometric);
    }
    if (*curr == *max - 1) {
	printf("curr: %d\n", *curr);
	printf("max: %d\n", *max);
	*array = (point_t *)bu_realloc(*array, sizeof(point_t) * (*max + INITIAL_SIZE), "realloc geometric vertices");
	*max = *max + INITIAL_SIZE;
    }
    (*array)[*curr][0] = x;
    (*array)[*curr][1] = y;
    (*array)[*curr][2] = z;
    *curr = *curr + 1;
    return *curr;
}


int
obj_add_group(char *grpname)
{
    int curr = obj_global_groups.g_count;
    int max = obj_global_groups.g_max;
    if (curr == max - 1) {
	obj_global_groups.groups = (obj_group_t *)bu_realloc(obj_global_groups.groups, sizeof(obj_group_t) * (max + INITIAL_SIZE), "realloc groups array");
	obj_global_groups.g_max = max + INITIAL_SIZE;
    }
}


int
obj_add_line()
{
    int curr = obj_global_elements.l_count;
    int max = obj_global_elements.l_max;
    if (curr == max - 1) {
	obj_global_elements.lines = (obj_line_t *)bu_realloc(obj_global_elements.lines, sizeof(obj_line_t) * (max + INITIAL_SIZE), "realloc lines array");
	obj_global_elements.l_max = max + INITIAL_SIZE;
    }
}


int
obj_add_face()
{
    int curr = obj_global_elements.f_count;
    int max = obj_global_elements.f_max;
    if (curr == max - 1) {
	obj_global_elements.faces = (obj_face_t *)bu_realloc(obj_global_elements.faces, sizeof(obj_face_t) * (max + INITIAL_SIZE), "realloc lines array");
	obj_global_elements.f_max = max * INITIAL_SIZE;
    }
}


int
main(int argc, char *argv[]) 
{
    int i;
    obj_global_vertices.geometric = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_SIZE, "initial geometric vertices malloc");
    obj_global_vertices.v_count = 0;
    obj_global_vertices.v_max = INITIAL_SIZE;
    obj_global_vertices.texture = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_SIZE, "initial texture vertices malloc");
    obj_global_vertices.t_count = 0;
    obj_global_vertices.t_max = INITIAL_SIZE;
    obj_global_vertices.vertex_norm = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_SIZE, "initial texture vertices malloc");
    obj_global_vertices.v_count = 0;
    obj_global_vertices.v_max = INITIAL_SIZE;
    obj_global_elements.points = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_SIZE, "initial point array malloc");
    obj_global_elements.p_count = 0;
    obj_global_elements.p_max = INITIAL_SIZE;
    obj_global_elements.lines = (obj_line_t *)bu_malloc(sizeof(obj_line_t)*INITIAL_SIZE, "initial line array malloc");
    obj_global_elements.l_count = 0;
    obj_global_elements.l_max = INITIAL_SIZE;
    obj_global_elements.faces = (obj_face_t *)bu_malloc(sizeof(obj_face_t)*INITIAL_SIZE, "initial face array malloc");
    obj_global_elements.f_count = 0;
    obj_global_elements.f_max = INITIAL_SIZE;
    obj_global_groups.groups = (obj_group_t *)bu_malloc(sizeof(obj_group_t)*INITIAL_SIZE, "initial group array malloc");
    obj_global_groups.g_count = 0;
    obj_global_groups.g_max = INITIAL_SIZE;

    if (argc > 0) {
	printf("Reading from %s\n", argv[1]);
	yyin = fopen(argv[1], "r");
	if (!yyin) {
	    perror("Unable to open file");
	    return -1;
	}
	while (!feof(yyin)) {
	    yyparse();
	}
	if (yyin) {
	    fclose(yyin);
	}
    } else {
	printf("Reading from stdin\n");
	yyin = stdin;
	yyparse();
    }

    for (i = 0; i < obj_global_vertices.v_count; i++) {
	printf("added geometric vertex %d: (%f, %f, %f)\n", i+1, obj_global_vertices.geometric[i][0], obj_global_vertices.geometric[i][1], obj_global_vertices.geometric[i][2]);
    }

    for (i = 0; i < obj_global_vertices.t_count; i++) {
	printf("added texture vertex %d: (%f, %f, %f)\n", i+1, obj_global_vertices.texture[i][0], obj_global_vertices.texture[i][1], obj_global_vertices.texture[i][2]);
    }

    for (i = 0; i < obj_global_vertices.n_count; i++) {
	printf("added vertex normal %d: (%f, %f, %f)\n", i+1, obj_global_vertices.vertex_norm[i][0], obj_global_vertices.vertex_norm[i][1], obj_global_vertices.vertex_norm[i][2]);
    }


    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
