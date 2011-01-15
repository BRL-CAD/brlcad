/*                      C A T T R A C K . H
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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
/** @file cattrack.h
 *
 * private functions used by anim_track
 *
 */

#include "common.h"

#include "bu.h"


/**
 * get x value of a point which is a given distance along caternary
 * curve:  x(s) = arcsinh(a*s-sinh(a*c))/a + c
 *
 * Left to calling routine to avoid dividing by zero.
 */
fastf_t hyper_get_x(fastf_t a, fastf_t c, fastf_t s);


/**
 * calculate the arclength parameter of a caternary curve
 * corresponding to the given value of x.
 *
 * s(x) = (sinh(a(x-c))+sinh(ac))/a
 *
 * Left to calling routime to avoid dividing by zero.
 */
fastf_t hyper_get_s(fastf_t a, fastf_t c, fastf_t x);


/**
 * calculate point on the caternary curve: z(x) = cosh(a*(x-c))/a + b
 * Left to calling routine to avoid dividing by zero.
 */
fastf_t hyper_get_z(fastf_t a, fastf_t b, fastf_t c, fastf_t x);


/**
 * calculate angle corresponding to the slope of caternary curve:
 * z'(x) = sinh(a*(x-c))
 */
fastf_t hyper_get_ang(fastf_t a, fastf_t c, fastf_t x);


/**
 * Find the constants a, b, and c such that the curve
 *
 * z = cosh(a*(x-c))/a + b
 *
 * is tangent to circles of radii r0 and r1 located at (x0, z0) and
 * (x1, z1) and such that the curve has arclength delta_s between
 * circles. Also find the angle where the curve touches each
 * circle. When called successively, It uses the values of a, b, and c
 * from the last call as a start.
 *
 * pa, pb, pc: curve parameters
 * pth0, pth1: angle where curve contacts circle0, circle1
 * delta_s: desired arclength
 * p_zero, p_one: center of circle0 and circle1
 * r_zero, r_one: radii of circle0 and circle1
 *
 */
int getcurve(fastf_t *pa, fastf_t *pb, fastf_t *pc, fastf_t *pth0, fastf_t *pth1, fastf_t delta_s, fastf_t *p_zero, fastf_t *p_one, fastf_t r_zero, fastf_t r_one);


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
