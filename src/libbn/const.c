/*                         C O N S T . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2008 United States Government as represented by
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
/** @addtogroup const */
/** @{ */
/** @file const.c
 *
 *  Constants provided by the numerics library.
 *
 */

#include "common.h"

#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


const fastf_t bn_pi	= M_PI;				/** pi */
const fastf_t bn_twopi	= 6.28318530717958647692;	/** pi*2 */

const fastf_t bn_halfpi		= M_PI_2;		/** pi/2 */
const fastf_t bn_quarterpi	= M_PI_4;		/** pi/4 */

const fastf_t bn_invpi	= M_1_PI;			/** 1/pi */
const fastf_t bn_inv2pi	= 0.159154943091895335769;	/** 1/(pi*2) */
const fastf_t bn_inv4pi	= 0.07957747154594766788;	/** 1/(pi*4) */

const fastf_t bn_inv255	= 0.003921568627450980392156862745; /** 1.0/255.0; */

const fastf_t bn_degtorad = DEG2RAD;	/** (pi*2)/360 */
const fastf_t bn_radtodeg = RAD2DEG;	/** 360/(pi*2) */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
