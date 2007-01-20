/*                       B S P H E R E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file bsphere.c
 *
 */

/*	BSPHERE.C	*/
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include "common.h"



#include	<stdio.h>
#include	<math.h>

#include	"machine.h"
#include	"vmath.h"
#include	"raytrace.h"
#include	"./nirt.h"
#include	"./usrfmt.h"

fastf_t	bsphere_diameter;

void set_diameter(struct rt_i *rtip)
{
    vect_t	diag;

    VSUB2(diag, rtip -> mdl_max, rtip -> mdl_min);
    bsphere_diameter = MAGNITUDE(diag);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
