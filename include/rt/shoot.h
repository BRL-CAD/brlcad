/*                        S H O O T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file rt/shoot.h
 *
 */

#ifndef RT_SHOOT_H
#define RT_SHOOT_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"
#include "rt/application.h"
#include "rt/xray.h"

__BEGIN_DECLS

/* Shoot a ray */
/**
 * Note that the direction vector r_dir must have unit length; this is
 * mandatory, and is not ordinarily checked, in the name of
 * efficiency.
 *
 * Input:  Pointer to an application structure, with these mandatory fields:
 * a_ray.r_pt ==> Starting point of ray to be fired
 * a_ray.r_dir => UNIT VECTOR with direction to fire in (dir cosines)
 * a_hit =======> Routine to call when something is hit
 * a_miss ======> Routine to call when ray misses everything
 *
 * Calls user's a_miss() or a_hit() routine as appropriate.  Passes
 * a_hit() routine list of partitions, with only hit_dist fields
 * valid.  Normal computation deferred to user code, to avoid needless
 * computation here.
 *
 * Formal Return: whatever the application function returns (an int).
 *
 * NOTE: The application functions may call rt_shootray() recursively.
 * Thus, none of the local variables may be static.
 *
 * To prevent having to lock the statistics variables in a PARALLEL
 * environment, all the statistics variables have been moved into the
 * 'resource' structure, which is allocated per-CPU.
 */
RT_EXPORT extern int rt_shootray(struct application *ap);

/* Shoot a bundle of rays */

/*
 * Function for shooting a bundle of rays. Iteratively walks list of
 * rays contained in the application bundles xrays field 'b_rays'
 * passing each single ray to r_shootray().
 *
 * Input:
 *
 * bundle -  Pointer to an application_bundle structure.
 *
 * b_ap - Members in this single ray application structure should be
 * set in a similar fashion as when used with rt_shootray() with the
 * exception of a_hit() and a_miss(). Default implementations of these
 * routines are provided that simple update hit/miss counters and
 * attach the hit partitions and segments to the partition_bundle
 * structure. Users can still override this default functionality but
 * have to make sure to move the partition and segment list to the new
 * partition_bundle structure.
 *
 * b_hit() Routine to call when something is hit by the ray bundle.
 *
 * b_miss() Routine to call when ray bundle misses everything.
 *
 */
RT_EXPORT extern int rt_shootrays(struct application_bundle *bundle);


/**
 * Shoot a single ray and return the partition list. Handles callback
 * issues.
 *
 * Note that it calls malloc(), therefore should NOT be used if
 * performance matters.
 */
RT_EXPORT extern struct partition *rt_shootray_simple(struct application *ap,
                                                      point_t origin,
                                                      vect_t direction);


/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int rt_shootray_bundle(register struct application *ap, struct xray *rays, int nrays);



__END_DECLS

#endif /* RT_SHOOT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
