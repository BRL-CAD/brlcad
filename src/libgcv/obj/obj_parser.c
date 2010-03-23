/*                       O B J _ P A R S E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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
 *  Handling routines for obj parsing
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

#define INITIAL_VERT_MAX 100

extern FILE *yyin;
extern int yyparse (void);

obj_vertices_t obj_global_vertices;

int obj_add_vertex(int type, fastf_t x, fastf_t y, fastf_t  z) {
    int *curr;
    int *max;
    point_t *array;
    switch (type) {
	case 1:
	    curr = &(obj_global_vertices.t_count);
	    max = &(obj_global_vertices.t_max);
	    array = obj_global_vertices.texture;
	    break;
	case 2:
	    curr = &(obj_global_vertices.n_count);
	    max = &(obj_global_vertices.n_max);
	    array = obj_global_vertices.vertex_norm;
	    break;
   	default:
	    curr = &(obj_global_vertices.v_count);
	    max = &(obj_global_vertices.v_max);
	    array = obj_global_vertices.geometric;
    }
    if (*curr == *max - 1) {
	printf("reallocing\n");
	array = (point_t *)bu_realloc(array, sizeof(point_t) * INITIAL_VERT_MAX, "realloc geometric vertices");
	*max = *max * 2;
    }
    array[*curr][0] = x;
    array[*curr][1] = y;
    array[*curr][2] = z;
    printf("added vertex %d(type %d): (%f,%f,%f)\n", *curr, type, array[*curr][0], array[*curr][1], array[*curr][2]);
    *curr = *curr + 1;
    return *curr;
}

int main(int argc, char *argv[]) 
{
  obj_global_vertices.geometric = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_VERT_MAX, "initial geometric vertices malloc");
  obj_global_vertices.v_count = 0;
  obj_global_vertices.v_max = INITIAL_VERT_MAX;
  obj_global_vertices.texture = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_VERT_MAX, "initial texture vertices malloc");
  obj_global_vertices.t_count = 0;
  obj_global_vertices.t_max = INITIAL_VERT_MAX;
  obj_global_vertices.vertex_norm = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_VERT_MAX, "initial texture vertices malloc");
  obj_global_vertices.v_count = 0;
  obj_global_vertices.v_max = INITIAL_VERT_MAX;
  if (argc > 0) {
     printf("Reading from %s\n", argv[1]);
     yyin = fopen(argv[1], "r");
     if (!yyin)
     {
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
