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
	return -1;
    else if (((struct individual *)p2)->fitness == ((struct individual *)p1)->fitness)
	return 0;
    else
	return 1;
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
	    case 'u':
		opts->keep_upper = atoi(bu_optarg);
		continue;
	    case 'l':
		opts->kill_lower = atoi(bu_optarg);
		continue;
	    default:
		fprintf(stderr, "Unrecognized option: -%c\n", c);
		usage();
	}
    }
    opts->keep_upper *= opts->pop_size/100.0;
    opts->kill_lower *= opts->pop_size/100.0;
    return bu_optind;
}



int main(int argc, char *argv[]){
    int i, g; /* generation and parent counters */
    int parent1, parent2;
    int gop;
    int best,worst;
    fastf_t total_fitness = 0.0f;
    struct fitness_state *fstate = NULL;
    struct population *pop = NULL;
    char dbname[256] = {0}; //name of database
    struct options opts = {DEFAULT_POP_SIZE, DEFAULT_GENS, DEFAULT_RES, 0, 0};
    struct individual *tmp = NULL;
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
   
 
    for(g = 1; g < opts.gens; g++ ){
#ifdef VERBOSE
	printf("\nGeneration %d:\n" 
		"--------------\n", g);
#endif

	total_fitness = 0.0f;
	best = worst = 0; 

	snprintf(dbname, 256, "gen%.3d", g);
	pop->db_c = db_create(dbname, 5);

	/* calculate sum of all fitnesses and find
	 * the most fit individual in the population
	 * note: need to calculate outside of main pop
	 * loop because it's needed for pop_wrand_ind()*/
	for(i = 0; i < pop->size; i++) {
	   fit_diff(pop->parent[i].id, pop->db_p, fstate);
	   pop->parent[i].fitness = fstate->fitness;
	   total_fitness += FITNESS;
	}
	/* sort population - used for keeping top N and dropping bottom M */
	qsort(pop->parent, pop->size, sizeof(struct individual), cmp_ind);

	/* remove lower M of individuals */
	for(i = 0; i < opts.kill_lower > pop->size ? pop->size : opts.kill_lower; i++) {
	    total_fitness -= pop->parent[i].fitness;
	}


	printf("Most fit individual was %s with a fitness of %g\n", pop->parent[pop->size-1].id, pop->parent[pop->size-1].fitness);
	printf("%6.8g\t%6.8g\t%6.8g\n", total_fitness/pop->size, pop->parent[0].fitness, pop->parent[pop->size-1].fitness);

	for(i = 0; i < pop->size; i++){

	    snprintf(pop->child[i].id, 256, "gen%.3dind%.3d", g, i); 

	    /* keep upper N */
	    if(i >= pop->size - opts.keep_upper){
		pop_gop(REPRODUCE, pop->parent[i].id, NULL, pop->child[i].id, NULL,
			pop->db_p, pop->db_c, &rt_uniresource);
		continue;
	    }

	    /* Choose a random genetic operation and
	     * a parent which the op will be performed on*/
	    gop = pop_wrand_gop();
	    parent1 = pop_wrand_ind(pop->parent, pop->size, total_fitness, opts.kill_lower);
	    //printf("selected %g\n", pop->parent[parent1].fitness);
	    /* If we're performing crossover, we need a second parent */
	    if(gop == CROSSOVER && i >= pop->size-opts.keep_upper-1)gop=REPRODUCE; //cannot cross, so reproduce
	    if(gop & (REPRODUCE | MUTATE)){
#ifdef VERBOSE
		printf("r(%s)\t ---------------> (%s)\n", pop->parent[parent1].id, pop->child[i].id);
#endif
		/* perform the genetic operation and output the child to the child database */
		pop_gop(gop, pop->parent[parent1].id, NULL, pop->child[i].id, NULL,
			pop->db_p, pop->db_c, &rt_uniresource);
	    } else {
	    
		//while(parent2 == parent1) -- needed? can crossover be done on same 2 ind?
		parent2 = pop_wrand_ind(pop->parent, pop->size, total_fitness, opts.kill_lower);
//		printf("selected: %g\n", pop->parent[parent2].fitness);
		snprintf(pop->child[i].id, 256, "gen%.3dind%.3d", g, ++i); //name the child and increase pop count

#ifdef VERBOSE 
		printf("x(%s, %s) --> (%s, %s)\n", pop->parent[parent1].id, pop->parent[parent2].id, pop->child[i-1].id, pop->child[i].id);
#endif
		/* perform the genetic operation and output the children to the cihld database */
		pop_gop(gop, pop->parent[parent1].id, pop->parent[parent2].id, pop->child[i-1].id, pop->child[i].id,
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
