/*                     T R I _ R A Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
/** @file tri_ray.cpp
 *
 * Triangle ray and line intersection queries backed by Geometric Tools.
 */

#include "common.h"

#include "Mathematics/IntrLine3Triangle3.h"
#include "Mathematics/IntrRay3Triangle3.h"
#include "Mathematics/Line.h"
#include "Mathematics/Ray.h"
#include "Mathematics/Triangle.h"
#include "Mathematics/Vector3.h"

#include "bg/plane.h"
#include "bg/tri_ray.h"


using GTF = fastf_t;
using Vec3 = gte::Vector3<GTF>;
using Triangle = gte::Triangle3<GTF>;


static Vec3
vec3_from_array(const fastf_t values[3])
{
    return Vec3{values[0], values[1], values[2]};
}


static Triangle
make_triangle(const point_t vert0, const point_t vert1, const point_t vert2)
{
    Triangle triangle;
    triangle.v[0] = vec3_from_array(vert0);
    triangle.v[1] = vec3_from_array(vert1);
    triangle.v[2] = vec3_from_array(vert2);
    return triangle;
}


template <typename Primitive>
static int
intersect_triangle(const point_t origin, const vect_t direction,
		   const point_t vert0, const point_t vert1, const point_t vert2,
		   fastf_t *intersection)
{
    Vec3 gte_direction = vec3_from_array(direction);
    if (gte::Normalize(gte_direction, true) == (GTF)0)
	return 0;

    Primitive primitive(vec3_from_array(origin), gte_direction);
    gte::FIQuery<GTF, Primitive, Triangle> query;
    auto result = query(primitive, make_triangle(vert0, vert1, vert2));
    if (!result.intersect)
	return 0;

    if (intersection) {
	intersection[X] = result.point[0];
	intersection[Y] = result.point[1];
	intersection[Z] = result.point[2];
    }

    return 1;
}


extern "C" {

int
bg_isect_tri_ray(const point_t orig, const point_t dir,
		 const point_t vert0, const point_t vert1, const point_t vert2,
		 point_t *isect)
{
    return intersect_triangle<gte::Ray3<GTF>>(
	orig, dir, vert0, vert1, vert2, isect ? &(*isect)[0] : NULL);
}


int
bg_isect_tri_line(point_t *isect, const point_t orig, const vect_t dir,
		  const point_t vert0, const point_t vert1, const point_t vert2)
{
    return intersect_triangle<gte::Line3<GTF>>(
	orig, dir, vert0, vert1, vert2, isect ? &(*isect)[0] : NULL);
}


int
bg_does_ray_isect_tri(const point_t pt, const vect_t dir,
		      const point_t vert0, const point_t vert1, const point_t vert2,
		      point_t inter)
{
    point_t intersection;
    int hit = bg_isect_tri_line(inter ? &intersection : NULL,
				pt, dir, vert0, vert1, vert2);
    if (hit && inter)
	VMOVE(inter, intersection);

    return hit;
}

} /* extern "C" */


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
