/*                    G R I D R O T A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/gridrotate.c
 *
 */

#include "common.h"

#include <math.h>

#include "vmath.h"


/*
  void gridRotate(fastf_t azim, fastf_t elev, fastf_t roll,
  fastf_t *des_H, fastf_t *des_V)

  Creates the unit vectors H and V which are the horizontal
  and vertical components of the grid in target coordinates.
  The vectors are found from the azimuth and elevation of the
  viewing angle according to a simplification of the rotation
  matrix from grid coordinates to target coordinates.
  To see that the vectors are, indeed, unit vectors, recall
  the trigonometric relation:

  sin(A)^2 + cos(A)^2  =  1 .

*/
void
gridRotate(fastf_t azim, fastf_t elev, fastf_t roll, fastf_t *des_H, fastf_t *des_V)
{
    fastf_t sn_azm = sin(azim);
    fastf_t cs_azm = cos(azim);
    fastf_t sn_elv = sin(elev);

    des_H[0] = -sn_azm;
    des_H[1] =  cs_azm;
    des_H[2] =  0.0;
    des_V[0] = -sn_elv*cs_azm;
    des_V[1] = -sn_elv*sn_azm;
    des_V[2] =  cos(elev);

    if (!ZERO(roll)) {
	fastf_t tmp_V[3], tmp_H[3], prime_V[3];
	fastf_t sn_roll = sin(roll);
	fastf_t cs_roll = cos(roll);

	VSCALE(tmp_V, des_V, cs_roll);
	VSCALE(tmp_H, des_H, sn_roll);
	VADD2(prime_V, tmp_V, tmp_H);
	VSCALE(tmp_V, des_V, -sn_roll);
	VSCALE(tmp_H, des_H, cs_roll);
	VADD2(des_H, tmp_V, tmp_H);
	VMOVE(des_V, prime_V);
    }
    return;
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
