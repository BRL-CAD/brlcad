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

int obj_add_vertex(fastf_t x, fastf_t y, fastf_t  z) {
    int curr = obj_global_vertices.v_count;
    if (curr == obj_global_vertices.v_max - 1) {
	obj_global_vertices.geometric = (point_t *)bu_realloc(obj_global_vertices.geometric, sizeof(point_t) * obj_global_vertices.v_max * 2, "realloc geometric vertices");
	obj_global_vertices.v_max = obj_global_vertices.v_max * 2;
    }
    printf("x: %f  y: %f  z: %f \n", x, y ,z);
    obj_global_vertices.geometric[curr][0] = x;
    obj_global_vertices.geometric[curr][1] = y;
    obj_global_vertices.geometric[curr][2] = z;
    obj_global_vertices.v_count++;
    printf("added vertex %d: (%f,%f,%f)\n", obj_global_vertices.v_count, obj_global_vertices.geometric[curr][0], obj_global_vertices.geometric[curr][1], obj_global_vertices.geometric[curr][2]);
    return obj_global_vertices.v_count;
}

int main(int argc, char *argv[]) 
{
  obj_global_vertices.geometric = (point_t *)bu_malloc(sizeof(point_t)*INITIAL_VERT_MAX, "initial geometric vertices malloc");
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
