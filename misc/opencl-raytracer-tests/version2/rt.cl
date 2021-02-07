/*                            R T . C L
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file rt.cl
 *
 * Brief description
 *
 */

#define MAX_SPHERES 10


typedef struct
{
    float3 position;
    float radius;
    int is_light;
} Sphere;


__kernel void ray_trace(__global int16 *output, __constant Sphere *gspheres)
{
    __local Sphere spheres[MAX_SPHERES];

    spheres[get_global_id(0)] = gspheres[get_global_id(0)];
    barrier(CLK_LOCAL_MEM_FENCE);

    if (get_global_id(0) == 0) {
	for (int i = 0; i < MAX_SPHERES; ++i)
	printf("is_light: %d\n", spheres[i].is_light);
	printf("%d\n", get_local_size(0));
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
