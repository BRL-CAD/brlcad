/*                          K N O B . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
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
/** @file libdm/knob.c
 *
 * Utilities for dealing with knobs.
 *
 */

#include "common.h"
#include "bio.h"

#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"

#ifdef IR_KNOBS

/*
 * Because the dials are so sensitive, setting them exactly to
 * zero is very difficult.  This function can be used to extend the
 * location of "zero" into "the dead zone".
 */

int
dm_limit(int i)
{
    if ( i > NOISE )
	return i-NOISE;
    if ( i < -NOISE )
	return i+NOISE;
    return 0;
}

/*
 * This function reverses the effects of dm_limit.
 */
int
dm_unlimit(int i)
{
    if ( i > 0 )
	return i + NOISE;
    if ( i < 0 )
	return i - NOISE;
    return 0;
}

/*
 * Wrap the given value "f" to a new value in the range (-1.0, 1.0).
 */
fastf_t
dm_wrap(fastf_t f)
{
    int i;
    fastf_t tmp_f;

    /* This way makes no assumption about the size of f */
    if (f > 1.0) {
	tmp_f = (f - 1.0) / 2.0;
	i = tmp_f;
	tmp_f = (tmp_f - i) * 2.0;

	if (tmp_f == 0)
	    return 1.0;
	else
	    return -1.0 + tmp_f;
    }

    if (f < -1.0) {
	tmp_f = (f + 1.0) / 2.0;
	i = tmp_f;
	tmp_f = (tmp_f - i) * 2.0;

	if (tmp_f == 0)
	    return -1.0;
	else
	    return 1.0 + tmp_f;
    }

    return f;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
