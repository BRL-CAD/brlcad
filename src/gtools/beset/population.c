/*                    P O P U L A T I O N . C
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
/** @file population.c
 *
 * routines to manipulate the population
 *
 * Author -
 *   Ben Poole
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
#include <limits.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

#include "population.h"
#include "beset.h" // GOP options

//cut when pop_random() updated
unsigned int idx;

/**
 *	P O P _ I N I T --- initialize a population of a given size
 */
void 
pop_init (struct population **p, int size)
{
    *p = bu_malloc(sizeof(struct population), "population");

    (*p)->individual = bu_malloc(sizeof(struct individual) * size, "individuals");
    (*p)->offspring  = bu_malloc(sizeof(struct individual) * size, "offspring");
    (*p)->size = size;

    //remove when pop_rand() is updated
    //-------8<--------
#define SEED 33
    BN_RANDSEED(idx, SEED);
    //-----------------
}

/**
 *	P O P _ C L E A N  --- cleanup population struct
 */
void 
pop_clean (struct population *p)
{
    bu_free(p->individual, "individuals");
    bu_free(p->offspring, "offspring");
    bu_free(p, "population");
}

/**
 *	P O P _ S P A W N --- spawn a new population
 *	TODO: generalize/modularize somehow to allow adding more shapes and primitives
 *	also use variable/defined rates, intersection with bounding box, etc...
 */
void 
pop_spawn (struct population *p, struct rt_wdb *db_fp)
{
    int i;
    point_t p1 = {0.0, 0.0, 0.0};
    for(i = 0; i < p->size; i++)
    {
	VSET(p->individual[i].p, 1.0, 5.0, 0.0);
	snprintf(p->individual[i].id, 256, "i%d.s", i);
	p->individual[i].fitness = 0.0;
	p->individual[i].r = 1 + (10.0 * BN_RANDOM(idx));
	p->individual[i].type = GEO_SPHERE;
	mk_sph(db_fp, p->individual[i].id, p->individual[i].p, p->individual[i].r);
    }
}

/**
 *	P O P _ A D D --- add an individual to othe database
 *	TODO: Don't overwrite previous individuals, one .g file per generation
 */
void 
pop_add(struct individual *i, struct rt_wdb *db_fp)
{
    switch(i->type)
    {
    case GEO_SPHERE:
	mk_sph(db_fp, i->id, i->p, i->r);
    }
}


/**
 *	P O P _ W R A N D -- weighted random index of individual
 */
int
pop_wrand_ind(struct individual *i, int size, fastf_t total_fitness)
{
    int n = 0;
    fastf_t rindex, psum = 0;
    rindex = BN_RANDOM(idx) * total_fitness;

    /* EEEEEK WHAT IF RAND() IS ZEROOOOO!?!? */
    psum += i[n].fitness;
    for(n = 1; n < size; n++) {
	psum += i[n].fitness;
	if( rindex <= psum )
	    return n-1;
    }
    return size-1;
}

/**
 *	P O P _ R A N D --- random number (0,1)
 *	TODO: change to bn_rand0t01() or bn_rand_half()
 */
fastf_t
pop_rand (void)
{
    return BN_RANDOM(idx);
}

/**
 *	P O P _ R A N D _ G O P --- return a random genetic operation
 *	TODO: implement other operations, weighted (like wrand) op selection
 */
int 
pop_wrand_gop(void)
{
    float i = BN_RANDOM(idx);
    //if(i < 0.5)
	return MUTATE_MOD;
    //return MUTATE_RAND;
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
