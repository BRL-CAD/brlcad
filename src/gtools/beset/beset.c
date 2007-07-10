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
 * CURRENT STATUS: single spheres work
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


#define _L pop->parent[0]
void usage(){bu_bomb("Usage: ./beset [options] database rows cols");}


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

void
parse_args (int argc, char **argv)
{
    int c;

    bu_setprogname(argv[0]);
    /* handle options */
    bu_opterr = 0;
    bu_optind = 0;
    argv++; argc--;
    while (argv[0][0] == '-') {
	c = bu_getopt(argc, argv, "x:");
	switch (c) {
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.debug );
		argc -= 2;
		argv += 2;
		continue;
	    case EOF:
		break;
	    default:
		fprintf(stderr, "Unrecognized option: -%c\n", c);
		usage();
	//	return 1;
	}
    }
}





int main(int argc, char *argv[]){
    int i, g; /* generation and parent counters */
    int ind;
    int gop;
    fastf_t total_fitness;
    struct fitness_state *fstate;
    int c;
    struct population *pop;
    char dbname[256]; //name of database
    char indname[256];


//    parse_args(argc, argv);
    
    argc--;
    argv++;
    if (argc != 4) {
	fprintf(stderr, "Usage: %s [options] database rows cols source_object\n", bu_getprogname());
	return 1;
    }

    rt_init_resource(&rt_uniresource,1,NULL);

    fstate = fit_prep(atoi(argv[1]), atoi(argv[2])); //ERROR CHECK

    /* read source model into fstate.rays */
    fit_store(argv[3], argv[0], fstate); 

    pop_init(&pop, 10);

    pop_spawn(pop, pop->db_p->dbi_wdbp);
    

    struct directory *dp;
    //pop_spawn = gen00
    for(g = 1; g < 10; g++){
	if((dp = db_lookup(pop->db_p, "gen000ind005", LOOKUP_NOISY)) == DIR_NULL)
	    printf("ERROR LOOKING UP PARENT");

	//create a new db for each generation
	snprintf(dbname, 256, "gen%.2d", g);
	pop->db_c = db_create(dbname, 5);
	pop->db_c->dbi_wdbp = wdb_dbopen(pop->db_c,RT_WDB_TYPE_DB_DISK);

	//evaluate fitness of parents
	total_fitness = 0;
	
	for(i = 0; i < pop->size; i++) {
	   pop->parent[i].fitness = 1.0-fit_linDiff(pop->parent[i].id, pop->db_p, fstate);
	   total_fitness += pop->parent[i].fitness;
	}

	//qsort(pop->parent, pop->size, sizeof(struct individual), cmp_ind);

	for(i = 0; i < pop->size; i++){
	    ind = pop_wrand_ind(pop->parent, pop->size, total_fitness);
	    gop = pop_wrand_gop(); // to be implemented...
	    //going to need random node calculation for these too ...

	    pop->child[i] = pop->parent[ind]; //struct copy
	    snprintf(pop->child[i].id, 256, "gen%.3dind%.3d", g, i);
	    printf("copying %s to %s\n", pop->parent[ind].id, pop->child[i].id);

	    switch(REPRODUCE){
		case MUTATE_MOD:  // mutate but do not replace values, modify them by +- MOD_RATE
		    break;
		case MUTATE_RAND:
		    break;
		case MUTATE_OP:
		    //modify op in tree
		    break;
		case CROSSOVER:
		    //perform crossover on tree
		    break;
		case REPRODUCE:
		    pop_dup(pop->parent[ind].id, pop->child[i].id, pop->db_p, pop->db_c, &rt_uniresource );
	    }
	  	 //   snprintf(pop->child[i].id, 256, "g%di%d.s", g, i);
	//    pop_add(&pop->child[i], fstate->db->dbi_wdbp);
	}
  wdb_close(pop->db_p->dbi_wdbp);
	    pop->db_p = pop->db_c;


	struct individual *tmp = pop->child;
pop->child = pop->parent;
	pop->parent = tmp;
	printf("GENERATION %d COMPLETE\n", g);

    }
    printf("\nFINAL POPULATION\n----------------\n");
    for(i = 0; i < pop->size; i++)
	printf("%s\tf:%.5g\n", pop->child[i].id, 
		pop->child[i].fitness);

    //fit_updateRes(atoi(argv[1])*2, atoi(argv[2])*2);

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
