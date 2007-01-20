/*                      T E S T F R E E . C
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
/** @file testfree.c
 *
 */

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"

struct whatsit
{
    long	w_magic;
    double	w_d;
};
#define	WHATSIT_MAGIC	0x12345678

void free_whatsit (struct whatsit *wp, char *s)
{
    RT_CKMAG(wp, WHATSIT_MAGIC, "whatsit");

    bu_free((char *) wp, "a whatsit");
}

main (void)
{
    struct whatsit	*wp;

    rt_log("allocating a whatsit...\n");
    wp = (struct whatsit *) bu_malloc(sizeof(struct whatsit), "the whatsit");

    rt_log("Before initializing, the whatsit = <%x> (%x, %g)\n",
	    wp, wp -> w_magic, wp -> w_d);
    wp -> w_magic = WHATSIT_MAGIC;
    wp -> w_d = 4.96962656372528225336310;
    rt_log("After initializing, the whatsit = <%x> (%x, %g)\n",
	    wp, wp -> w_magic, wp -> w_d);

    free_whatsit(wp, "the whatsit once");
    rt_log("Freed it once\n");

    free_whatsit(wp, "the whatsit twice");
    rt_log("Freed it again\n");
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
