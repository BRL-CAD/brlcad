/*                       T R I _ R A Y . C
 * BRL-CAD
 *
 * Published in 2015 by the United States Government.
 * This work is in the public domain.
 *
 */
/* Ray/triangle intersection test routine,
 * by Moller & Trumbore, 1997.
 * See article "Fast, Minimal Storage Ray-Triangle Intersection",
 * Journal of Graphics Tools, 2(1):21-28, 1997
 *
 * License:  public domain (from Moller's collection of public domain code at
 * http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/)
 *
 * The changes made for BRL-CAD integration were to use the point_t
 * data type instead of fastf_t arrays and use vmath's vector macros
 * instead of the locally defined versions.  The function name was
 * changed to bg_isect_tri_ray.
 */

#include "common.h"
#include <math.h>
#include "vmath.h"
#include "bg/tri_ray.h"

#define EPSILON 0.000001

int bg_isect_tri_ray(const point_t orig, const point_t dir,
		     const point_t vert0, const point_t vert1, const point_t vert2,
		     point_t *isect)
{
    point_t edge1, edge2, tvec, pvec, qvec, D;
    fastf_t det,inv_det, u, v, t;

    /* find vectors for two edges sharing vert0 */
    VSUB2(edge1, vert1, vert0);
    VSUB2(edge2, vert2, vert0);

    /* begin calculating determinant - also used to calculate U parameter */
    VCROSS(pvec, dir, edge2);

    /* if determinant is near zero, ray lies in plane of triangle */
    det = VDOT(edge1, pvec);

    if (det > -EPSILON && det < EPSILON)
	return 0;
    inv_det = 1.0 / det;

    /* calculate distance from vert0 to ray origin */
    VSUB2(tvec, orig, vert0);

    /* calculate U parameter and test bounds */
    u = VDOT(tvec, pvec) * inv_det;
    if (u < 0.0 || u > 1.0)
	return 0;

    /* prepare to test V parameter */
    VCROSS(qvec, tvec, edge1);

    /* calculate V parameter and test bounds */
    v = VDOT(dir, qvec) * inv_det;
    if (v < 0.0 || u + v > 1.0)
	return 0;

    /* calculate point, ray intersects triangle */
    t = VDOT(edge2, qvec) * inv_det;

    /* This is a ray, not a line - if t is negative, it's a miss */
    if (t < 0) return 0;

    if (isect) {
	VSCALE(D, dir, t);
	VADD2((*isect), orig, D);
    }

    return 1;
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
