/*                    P O P U L A T I O N . H
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

struct name{
    char n[256];
    int i;
};

struct individual {
    char id[256];//the nameeeeee
    fastf_t fitness;
};

struct population {
    struct individual *parent;
    struct individual *child;

    struct db_i *db_p; // parent database
    struct db_i *db_c; // chld database




    int size;
};

void pop_init	    (struct population **p, int size);
void pop_spawn	    (struct population *p, struct rt_wdb *db_fp);
void pop_clean	    (struct population *p);
void pop_add	    (struct individual *i, struct rt_wdb *db);
int  pop_wrand_ind  (struct individual *i, int size, fastf_t total_fitness);
int  pop_wrand_gop  (void);
fastf_t pop_rand    (void);

//void pop_dup_functree(struct db_i *dbi_p, struct db_i *dbi_c,
//		      union tree *tp, struct resource *resp, 
//		      char *name );
void pop_dup(char *parent, char * child, struct db_i *dbi_p, 
	     struct db_i *dbi_c, struct resource *resp);





#endif /* __POPULATION_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
