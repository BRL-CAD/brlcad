/*                       F I T N E S S . H
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
/** @file fitness.h
 *
 * fitness prototypes and structures
 *
 * Author -
 *   Ben Poole
 */

#ifndef __FITNESS_H__
#define __FITNESS_H__

#define U_AXIS 0
#define V_AXIS 1
#define I_AXIS 2

/* Used for comparing rays */
#define STATUS_PP 1
#define STATUS_MP 2
#define STATUS_EMPTY 0

#define SEM_WORK RT_SEM_LAST
#define SEM_DIFF RT_SEM_LAST+1
#define SEM_SAME RT_SEM_LAST+2
#define TOTAL_SEMAPHORES SEM_SAME+1


struct part {
    struct bu_list  l;
    fastf_t	    inhit_dist;
    fastf_t	    outhit_dist;
};

struct fitness_state {
    char *name;
    struct part **ray; /* internal representation of raytraced source */
    struct rt_i *rtip; /* current objects to be raytraced */

    struct resource resource[MAX_PSW]; /* memory resource for multi-cpu processing */
    int ncpu;
    int max_cpus;
    int nodes;
    fastf_t a_len;

    int res[2]; /*  ray resolution on u and v axes */
    double gridSpacing[2]; /* grid spacing on u and v axes */
    int row; /* current v axis index */

    int capture; /* flags whether to store the object */
    fastf_t diff; /* linear difference between source and object */
    fastf_t same;

    fastf_t mdl_min[3];
    fastf_t min[3];
    fastf_t max[3];
    fastf_t fitness;
    fastf_t volume;
};

/* store a ray that hit */
int capture_hit(register struct application *ap, struct partition *partHeadp, struct seg *segs);

/* store a ray that missed */
int capture_miss(register struct application *ap);

/* compare a ray that hit to the same ray from source */
int compare_hit(register struct application *ap, struct partition *partHeadp, struct seg *segs);

/* compare a ray that missed to the same ray from source */
int compare_miss(register struct application *ap);

/* grab the next row of rays to be evaluated */
int get_next_row(struct fitness_state *fstate);

/* raytrace an object stored in fstate  either storing the rays or comparing them to the source */
void rt_worker(int cpu, genptr_t g);

/* prep for raytracing object, and call rt_worker for parallel processing */
void fit_rt (char *obj, struct db_i *db, struct fitness_state *fstate);

/* load database and prepare fstate for work */
void fit_prep(struct fitness_state *fstate, int rows, int cols);

/* cleanup */
void fit_clean(struct fitness_state *fstate);

/* store a given object as the source  */
void fit_store(char *obj, char *dbname, struct fitness_state *fstate);

/* update grid resolution */
void fit_updateRes(int rows, int cols, struct fitness_state *fstate);

/* calculates difference between object and source */
void fit_diff(char *obj, struct db_i *db, struct fitness_state *fstate);

/* clear the stored rays */
void free_rays (struct fitness_state *fstate);

#endif /* __FITNESS_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
