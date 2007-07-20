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
#include "wdb.h"

#include "fitness.h"
#include "population.h"
#include "beset.h"


void usage(){fprintf(stderr, "Usage: %s [options] db.g object\nOptions:\n -p #\t\tPopulation size\n -g #\t\tNumber of generations\n -r #\t\tResolution \n",bu_getprogname());exit(1);}


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

int
parse_args (int ac, char *av[], struct options *opts)
{
    int c;

    bu_setprogname(av[0]);
    /* handle options */
    bu_opterr = 0;
    bu_optind = 0;
    av++; ac--;

    while ((c=bu_getopt(ac,av,OPTIONS)) != EOF) {
	switch (c) {
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.debug );
		continue;
	    case 'p':
		opts->pop_size = atoi(bu_optarg);
		continue;
	    case 'g':
		opts->gens = atoi(bu_optarg);
		continue;
	    case 'r':
		opts->res = atoi(bu_optarg);
		continue;
	    default:
		fprintf(stderr, "Unrecognized option: -%c\n", c);
		usage();
	}
    }
    return bu_optind;
}



int main(int argc, char *argv[]){
    int i, g; /* generation and parent counters */
    int parent1, parent2;
    int gop;
    int best;
    fastf_t total_fitness;
    struct fitness_state *fstate;
    struct population *pop;
    char dbname[256]; //name of database
    struct options opts = {DEFAULT_POP_SIZE, DEFAULT_GENS, DEFAULT_RES};
    struct individual *tmp;
    int  ac;


    ac = parse_args(argc, argv, &opts);
    if(argc - ac != 3)
	usage();
    

    rt_init_resource(&rt_uniresource,1,NULL);

    /* read source model into fstate.rays */
    fstate = fit_prep(opts.res, opts.res);
    fit_store(argv[ac+2], argv[ac+1], fstate); 

    /* initialize population and spawn initial individuals */
    pop_init(&pop, opts.pop_size);
    pop_spawn(pop, pop->db_p->dbi_wdbp);

    
    for(g = 1; g < opts.gens; g++){
#ifdef VERBOSE
	printf("\nGeneration %d:\n" 
		"--------------\n", g);
#endif

	total_fitness = best = 0; 

	snprintf(dbname, 256, "gen%.3d", g);
	pop->db_c = db_create(dbname, 5);

	/* calculate sum of all fitnesses and find
	 * the most fit individual in the population
	 * note: need to calculate outside of main pop
	 * loop because it's needed for pop_wrand_ind()*/
	for(i = 0; i < pop->size; i++) {
	   pop->parent[i].fitness = 2.0/(1+fit_linDiff(pop->parent[i].id, pop->db_p, fstate));
	   if(pop->parent[i].fitness > pop->parent[best].fitness) best = i;
	   total_fitness += pop->parent[i].fitness;
	}
	printf("Most fit from generation %3d was: %s, fitness of %g\n", g-1, pop->parent[best].id, pop->parent[best].fitness);


	for(i = 0; i < pop->size; i++){

	    /* Choose a random genetic operation and
	     * a parent which the op will be performed on*/
	    gop = pop_wrand_gop();
	    parent1 = pop_wrand_ind(pop->parent, pop->size, total_fitness);
	    snprintf(pop->child[i].id, 256, "gen%.3dind%.3d", g, i); 

	    /* If we're performing crossover, we need a second parent */
	    if(gop == CROSSOVER && i < pop->size-1){
		//while(parent2 == parent1) -- needed? can crossover be done on same 2 ind?
		parent2 = pop_wrand_ind(pop->parent, pop->size, total_fitness);
		snprintf(pop->child[i].id, 256, "gen%.3dind%.3d", g, ++i); //name the child and increase pop count

#ifdef VERBOSE 
		printf("x(%s, %s) --> (%s, %s)\n", pop->parent[parent1].id, pop->parent[parent2].id, pop->child[i-1].id, pop->child[i].id);
#endif
		/* perform the genetic operation and output the children to the cihld database */
		pop_gop(gop, pop->parent[parent1].id, pop->parent[parent2].id, pop->child[i-1].id, pop->child[i].id,
			pop->db_p, pop->db_c, &rt_uniresource);
	    } else {
#ifdef VERBOSE
		printf("r(%s)\t ---------------> (%s)\n", pop->parent[parent1].id, pop->child[i].id);
#endif
		/* perform the genetic operation and output the child to the child database */
		pop_gop(REPRODUCE, pop->parent[parent1].id, NULL, pop->child[i].id, NULL,
			pop->db_p, pop->db_c, &rt_uniresource);
	    }
	    
	}

	/* Close parent db and move children 
	 * to parent database and population
	 * Note: pop size is constant so we
	 * can keep the storage from the previous 
	 * pop->parent for the next pop->child*/
	db_close(pop->db_p);
	pop->db_p = pop->db_c;
	tmp = pop->child;
	pop->child = pop->parent;
	pop->parent = tmp;

    }

#ifdef VERBOSE
    printf("\nFINAL POPULATION\n"
	    "----------------\n");
    for(i = 0; i < pop->size; i++)
	printf("%s\tf:%.5g\n", pop->child[i].id, 
		pop->child[i].fitness);
#endif


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
