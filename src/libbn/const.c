/*                         C O N S T . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup const */
/*@{*/
/** @file const.c
 * @brief
 *  Constants used by the ray tracing library.
 *
 * @par Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *@n	The U. S. Army Ballistic Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/

#ifndef lint
static const char RCSmat[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


const double bn_pi	= 3.14159265358979323846;	/**< @brief pi */
const double bn_twopi	= 6.28318530717958647692;	/**< @brief pi*2 */
const double bn_halfpi	= 1.57079632679489661923;	/**< @brief pi/2 */
const double bn_quarterpi=0.78539816339744830961;	/**< @brief  pi/4 */
const double bn_invpi	= 0.318309886183790671538;	/**< @brief  1/pi */
const double bn_inv2pi	= 0.159154943091895335769;	/**< @brief  1/(pi*2) */
const double bn_inv4pi	= 0.07957747154594766788;	/**< @brief  1/(pi*4) */

const double bn_inv255 = 1.0/255.0;

const double bn_degtorad =  0.0174532925199433;		/**< @brief  (pi*2)/360 */
const double bn_radtodeg = 57.29577951308230698802;	/**< @brief  360/(pi*2) */
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
