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

#include <strings.h>
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
#include "population.h"
#include "beset.h"


#define _L pop->individual[0]

struct fitness_state *fstate;
struct population *pop;

/* fitness of a given object compared to source */
static int cmp_ind(const void *p1, const void *p2)
{
    if(((struct individual *)p2)->fitness > ((struct individual *)p1)->fitness)
	return 1;
    else if (((struct individual *)p2)->fitness == ((struct individual *)p1)->fitness)
	return 0;
    else
	return -1;
}



int main(int argc, char **argv){
    /* general inint stuff, parallel stuff, break into parallel function */
    point_t p1;
    int i, g; /* generation and individual counters */
    int ind;
    int gop;
    fastf_t total_fitness;

    if(argc != 6){
	fprintf(stderr, "Usage: ./fitness database rows cols source_object test_object\n");
	return 1;
    }

    fit_prep(argv[1], atoi(argv[2]), atoi(argv[3]));
    fstate->db->dbi_wdbp = wdb_dbopen(fstate->db, RT_WDB_TYPE_DB_DISK);

    fit_store(argv[4]);

    pop_init(&pop, 50);
    pop_spawn(pop, fstate->db->dbi_wdbp);

    for(g = 0; g < 2; g++){
	printf("%d: ", g);
	for(i = 0; i < pop->size; i++) {
	   pop->individual[i].fitness = fit_linDiff(pop->individual[i].id);
	}
	qsort(pop->individual, pop->size, sizeof(struct individual), cmp_ind);

	printf("%s:\t%.5g\t%.5g\n", _L.id, _L.fitness, _L.r);
	/* calculate total fitness */
	total_fitness = 0;

	    printf("%d: %g\n", 0, pop->individual[0].r);
	for (i = 0; i < pop->size; i++) {

	    total_fitness += pop->individual[i].fitness;
	}
	for(i = 0; i < pop->size; i++){
	    printf("%d: r:%g\tf:%g\n", i, pop->individual[i].r, pop->individual[i].fitness);
	    ind = pop_wrand_ind(pop->individual, pop->size, total_fitness);
	    gop = pop_wrand_gop(); // to be implemented...
	    //going to need random node calculation for these too ...

	    pop->offspring[i] = pop->individual[ind];

	    //printf("ind: %g -- %g\n", pop->offspring[i].r, pop->individual[ind].r);
	    switch(MUTATE_RAND){
		case MUTATE_MOD:  // mutate but do not replace values, modify them by +- MOD_RATE
		    pop->offspring[i].r += -.1 + (.2 * (pop_rand()));
		    printf("off: %g\n", pop->offspring[i].r);
		    break;
		case MUTATE_RAND:
		    pop->offspring[i].r = 1+pop_rand() * SCALE;
		    break;
		case MUTATE_OP:
		    //modify op in tree
		    break;
		case CROSSOVER:
		    //perform crossover on tree
		    break;
	    }
	    pop_add(&pop->offspring[i], fstate->db->dbi_wdbp);
	}
	struct individual *tmp = pop->offspring;
	pop->offspring = pop->individual;
	pop->individual = tmp;
	printf("GENERATION %d COMPLETE\n", g);

    }


    fit_store(argv[4]);

    //fit_updateRes(atoi(argv[2])*2, atoi(argv[3])*2);

    fit_clean();
    pop_clean(pop);
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
