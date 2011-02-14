/*                    P O P U L A T I O N . H
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
/** @file population.h
 *
 * prototypes and population structs
 *
 * Author -
 *   Ben Poole
 */

#ifndef __POPULATION_H__
#define __POPULATION_H__

#define GEO_SPHERE 1

#define VSCALE_SELF(a, c) { (a)[X] *= (c); (a)[Y] *= (c); (a)[Z]*=(c); }
#define VMUTATE(a) {VMUT(a, -MUT_STEP/2+MUT_STEP*pop_rand())}
#define VMUT(a, c) {(a)[X] += (ZERO((a)[X]))?0.0:(c); (a)[Y] += (ZERO((a)[Y]))?0.0:(c); (a)[Z]+=(ZERO((a)[Z]))?0:(c);}
#define MUT_STEP 0.8



struct name{
    char n[256];
    int i;
};

struct individual {
    int id;
    fastf_t fitness;
};

struct population {
    struct individual *parent;
    struct individual *child;

    struct db_i *db_p;
    struct db_i *db_c;

    char **name;
    int size;
};

struct node{
    struct bu_list l;
    union tree **s_parent;
    union tree *s_child;
};

void pop_init	    (struct population *p, int size);
void pop_spawn	    (struct population *p);
void pop_clean	    (struct population *p);
void pop_add	    (struct individual *i, struct rt_wdb *db);
int  pop_wrand_ind  (struct individual *i, int size, fastf_t total_fitness, int offset);
int  pop_wrand_gop  (void);
fastf_t pop_rand    (void);
int pop_find_nodes(union tree *tp);

void pop_gop(int gop, char *parent1, char *parent2, char * child1, char *child2,  struct db_i *dbi_p,
	     struct db_i *dbi_c, struct resource *resp);
int pop_put_internal(const char *n, struct directory *dp, struct db_i *dbip, struct rt_db_internal *ip,
		     struct resource *resp);





#endif /* __POPULATION_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
