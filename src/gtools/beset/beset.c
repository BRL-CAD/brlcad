/*                         B E S E T . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
 */

#include "common.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"
#include "wdb.h"

#include "fitness.h"
#include "population.h"
#include "beset.h"
struct rt_db_internal source_obj;


void usage() {
    bu_exit(1, "Usage: %s [options] db.g object\nOptions:\n -p #\t\tPopulation size\n -g #\t\tNumber of generations\n -r #\t\tResolution \n -u #\t\tUpper percent of individuals to keep\n -l #\t\tLower percent of individuals to kill\n", bu_getprogname());
}


/* fitness of a given object compared to source */
static int cmp_ind(const void *p1, const void *p2)
{
    if (((struct individual *)p2)->fitness > ((struct individual *)p1)->fitness)
	return -1;
    else if (EQUAL(((struct individual *)p2)->fitness, ((struct individual *)p1)->fitness))
	return 0;
    else
	return 1;
}

int
parse_args (int ac, char *av[], struct beset_options *opts)
{
    int c;

    bu_setprogname(av[0]);
    /* handle options */
    bu_opterr = 0;
    bu_optind = 0;
    av++; ac--;

    while ((c=bu_getopt(ac, av, OPTIONS)) != -1) {
	switch (c) {
	    case 'm':
		opts->mut_rate = atoi(bu_optarg);
		continue;
	    case 'c':
		opts->cross_rate = atoi(bu_optarg);
		continue;
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



int main(int argc, char *argv[]) {
    int i, g; /* generation and parent counters */
    int parent1, parent2;
    int gop;
    fastf_t total_fitness = 0.0f;
    struct fitness_state fstate;
    struct population pop = {NULL, NULL, NULL, NULL, NULL, 0};
    char dbname[256] = {0};
    struct beset_options opts = {DEFAULT_POP_SIZE, DEFAULT_GENS, DEFAULT_RES, 0, 0, 0, 0};
    struct individual *tmp = NULL;
    int  ac;
    struct db_i *source_db;


    ac = parse_args(argc, argv, &opts);
    if (argc - ac != 3)
	usage();


    /* read source model into fstate.rays */
    fit_prep(&fstate, opts.res, opts.res);
    fit_store(argv[ac+2], argv[ac+1], &fstate);


    /* initialize population and spawn initial individuals */
    pop_init(&pop, opts.pop_size);
    pop_spawn(&pop);

    source_db = db_open(argv[ac+1], "r+w");
    db_dirbuild(source_db);
    pop.db_c = db_create("testdb", 5);
    db_close(pop.db_c);


    for (g = 1; g < opts.gens; g++ ) {
#ifdef VERBOSE
	printf("\nGeneration %d:\n"
	       "--------------\n", g);
#endif

	total_fitness = 0.0f;

	snprintf(dbname, 256, "gen%.3d", g);
	pop.db_c = db_create(dbname, 5);

	pop_gop(REPRODUCE, argv[ac+2], NULL, argv[ac+2], NULL, source_db, pop.db_c, &rt_uniresource);



	/* calculate sum of all fitnesses and find
	 * the most fit individual in the population
	 * note: need to calculate outside of main pop
	 * loop because it's needed for pop_wrand_ind()*/
	for (i = 0; i < pop.size; i++) {
	    fit_diff(NL(pop.parent[i].id), pop.db_p, &fstate);
	    pop.parent[i].fitness = fstate.fitness;
	    total_fitness += FITNESS;
	}
	/* sort population - used for keeping top N and dropping bottom M */
	qsort(pop.parent, pop.size, sizeof(struct individual), cmp_ind);

	/* remove lower M of individuals */
	for (i = 0; i < opts.kill_lower; i++) {
	    total_fitness -= pop.parent[i].fitness;
	}


	printf("Most fit from %s was %s with a fitness of %g\n", dbname, NL(pop.parent[pop.size-1].id), pop.parent[pop.size-1].fitness);
	printf("%6.8g\t%6.8g\t%6.8g\n", total_fitness/pop.size, pop.parent[0].fitness, pop.parent[pop.size-1].fitness);
	for (i = 0; i < pop.size; i++) {

	    pop.child[i].id = i;

	    /* keep upper N */
	    if (i >= pop.size - opts.keep_upper) {
		pop_gop(REPRODUCE, NL(pop.parent[i].id), NULL, NL(pop.child[i].id), NULL,
			pop.db_p, pop.db_c, &rt_uniresource);
		continue;
	    }

	    /* Choose a random genetic operation and
	     * a parent which the op will be performed on*/
	    gop = pop_wrand_gop();
	    parent1 = pop_wrand_ind(pop.parent, pop.size, total_fitness, opts.kill_lower);
	    /* only need 1 more individual, can't crossover, so reproduce */
	    if (gop == CROSSOVER && i >= pop.size-opts.keep_upper-1)gop=REPRODUCE;

	    if (gop & (REPRODUCE | MUTATE)) {
#ifdef VERBOSE
		printf("r(%s)\t ---------------> (%s)\n", NL(pop.parent[parent1].id), NL(pop.child[i].id));
#endif
		/* perform the genetic operation and output the child to the child database */
		pop_gop(gop, NL(pop.parent[parent1].id), NULL, NL(pop.child[i].id), NULL,
			pop.db_p, pop.db_c, &rt_uniresource);
	    } else {
		/* If we're performing crossover, we need a second parent */
		parent2 = pop_wrand_ind(pop.parent, pop.size, total_fitness, opts.kill_lower);
		++i;
		pop.child[i].id = i;

#ifdef VERBOSE
		printf("x(%s, %s) --> (%s, %s)\n", NL(pop.parent[parent1].id), NL(pop.parent[parent2].id), pop.child[i-1].id, pop.child[i].id);
#endif
		/* perform the genetic operation and output the children to the cihld database */
		pop_gop(gop, NL(pop.parent[parent1].id), NL(pop.parent[parent2].id), NL(pop.child[i-1].id), NL(pop.child[i].id),
			pop.db_p, pop.db_c, &rt_uniresource);
	    }

	}



	/* Close parent db and move children
	 * to parent database and population
	 * Note: pop size is constant so we
	 * can keep the storage from the previous
	 * pop.parent for the next pop.child*/
	db_close(pop.db_p);
	pop.db_p = pop.db_c;
	tmp = pop.child;
	pop.child = pop.parent;
	pop.parent = tmp;

    }
    db_close(pop.db_p);

#ifdef VERBOSE
    printf("\nFINAL POPULATION\n"
	   "----------------\n");
    for (i = 0; i < pop.size; i++)
	printf("%s\tf:%.5g\n", NL(pop.child[i].id),
	       pop.child[i].fitness);
#endif


    pop_clean(&pop);
    fit_clean(&fstate);
    return 0;
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
