/*                         B E S E T . H
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

#ifndef __BESET_H__
#define __BESET_H__

#define MUTATE 1
#define MUTATE_RAND 2
#define MUTATE_OP 4
#define CROSSOVER 8
#define REPRODUCE 16

#define SCALE 10

#define DEFAULT_POP_SIZE 20
#define DEFAULT_GENS 50
#define DEFAULT_RES 10
#define OPTIONS ":c:m:x:p:g:r:u:l:"
#define NL(a) pop.name[(a)]
#define NL_P(a) p->name[(a)]

struct beset_options{
    int pop_size;
    int gens;
    int res;
    int kill_lower;
    int keep_upper;
    int mut_rate;
    int cross_rate;
};


/*
 * MACROS TO IMPROVE READABILITY
 * OF FITNESS FUNCTION
 */
#define FITNESS (pop.parent[i].fitness)
#define DIFF fit_linDiff(pop->parent[i].id, pop->db_p, fstate)
#define NODES (fstate->nodes)
#define INDEX (pop.size-i)

#endif /* __BESET_H__ */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
