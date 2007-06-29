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
 * Ben's Evolutionary Shape Tool
 *
 * CURRENT STATUS: single spheres with a whole radius work
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

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

#include "fitness.h"
#include "population.h"
#include "beset.h"


#define _L pop->individual[0]

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
    struct fitness_state *fstate;

    if(argc != 5){
	fprintf(stderr, "Usage: %s database rows cols source_object\n", argv[0]);
	return 1;
    }

    fstate = fit_prep(argv[1], atoi(argv[2]), atoi(argv[3]));
    fstate->db->dbi_wdbp = wdb_dbopen(fstate->db, RT_WDB_TYPE_DB_DISK);

    printf("____STORED MODEL____\n");
    fit_store(argv[4], fstate);

    pop_init(&pop, 1);
    pop_spawn(pop, fstate->db->dbi_wdbp);
    printf("___POPULATION_MODEL___\n");

    for(g = 0; g < 1; g++){
	for(i = 0; i < pop->size; i++) {
	   pop->individual[i].fitness = 1.0-fit_linDiff(pop->individual[i].id, fstate);
	}
	qsort(pop->individual, pop->size, sizeof(struct individual), cmp_ind);

	/* calculate total fitness */
	total_fitness = 0;

	for (i = 0; i < pop->size; i++) {

	    total_fitness += pop->individual[i].fitness;
	}
	for(i = 0; i < pop->size; i++){
	    ind = pop_wrand_ind(pop->individual, pop->size, total_fitness);
	    gop = pop_wrand_gop(); // to be implemented...
	    //going to need random node calculation for these too ...

	    pop->offspring[i] = pop->individual[ind];

	    //printf("ind: %g -- %g\n", pop->offspring[i].r, pop->individual[ind].r);
	    switch(CROSSOVER){
		case MUTATE_MOD:  // mutate but do not replace values, modify them by +- MOD_RATE
		    pop->offspring[i].r += -.1 + (.2 * (pop_rand()));
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



    //fit_updateRes(atoi(argv[2])*2, atoi(argv[3])*2);

    fit_clean(fstate);
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
