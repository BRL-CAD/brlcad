/*                         B E S E T . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file beset.c
 *
 * Program to test functionality of fitness.c (temporary)
 *
 * Author - Ben Poole
 *
 */


#include "common.h"
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>                     /* home of INT_MAX aka MAXINT */

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

#include "fitness.h"


struct fitness_state *fstate;

/* fitness of a given object compared to source */

int main(int argc, char **argv){
    if(argc != 6){
	fprintf(stderr, "Usage: ./fitness database rows cols source_object test_object\n");
	return 1;
    }

    int r = fit_prep(argv[1], atoi(argv[2]), atoi(argv[3]));
    if(r != 0){
       if(r == DB_OPEN_FAILURE){
	   fprintf(stderr, "db_open failure on %s\n", argv[1]);
	   return r;
       }
       else if(r == DB_DIRBUILD_FAILURE){
	   fprintf(stderr, "db_dirbuild failure on %s\n", argv[1]);
	   return r;
       }
   }
    fit_store(argv[4]);
    printf("Linear Difference: %g\n", fit_linDiff(argv[5]));
    //fit_updateRes(atoi(argv[2])*2, atoi(argv[3])*2);
    //printf("Obj Fitness: %g\n", fit_fitness(argv[5]));


    fit_clean();
    return 0;
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
