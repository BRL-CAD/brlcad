/*                      T I M E R H E P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup timer */
/** @{ */
/** @file librt/timerhep.c
 *
 * To provide timing information for RT.
 * THIS VERSION FOR Denelcor HEP/UPX (System III-like)
 */


#include <stdio.h>

/* Standard System V stuff */
extern long time(time_t *);
static long time0;


/**
 *
 */
void
rt_prep_timer(void)
{
    (void)time(&time0);
    (void)intime_();
}


/**
 *
 */
double
rt_read_timer(char *str, int len)
{
    long now;
    double usert;
    long htime[6];
    char line[132];

    (void)stats_(htime);
    (void)time(&now);
    usert = ((double)htime[0]) / 10000000.0;
    if (usert < 0.00001) usert = 0.00001;
    sprintf(line, "%f secs: %ld wave, %ld fp, %ld dmem, %ld other",
	    usert,
	    htime[0], htime[1], htime[2], htime[3], htime[4]);
    bu_strlcpy(str, line, len);
    return usert;
}


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
