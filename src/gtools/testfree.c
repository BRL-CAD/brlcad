/*                      T E S T F R E E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 */
/** @file testfree.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


struct whatsit
{
    long	w_magic;
    double	w_d;
};
#define	WHATSIT_MAGIC	0x12345678

void free_whatsit (struct whatsit *wp, char *s)
{
    BU_CKMAG(wp, WHATSIT_MAGIC, "whatsit");

    bu_free((char *) wp, "a whatsit");
}

main (void)
{
    struct whatsit	*wp;

    bu_log("allocating a whatsit...\n");
    wp = (struct whatsit *) bu_malloc(sizeof(struct whatsit), "the whatsit");

    bu_log("Before initializing, the whatsit = <%x> (%x, %g)\n",
	    wp, wp -> w_magic, wp -> w_d);
    wp -> w_magic = WHATSIT_MAGIC;
    wp -> w_d = 4.96962656372528225336310;
    bu_log("After initializing, the whatsit = <%x> (%x, %g)\n",
	    wp, wp -> w_magic, wp -> w_d);

    free_whatsit(wp, "the whatsit once");
    bu_log("Freed it once\n");

    free_whatsit(wp, "the whatsit twice");
    bu_log("Freed it again\n");
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
