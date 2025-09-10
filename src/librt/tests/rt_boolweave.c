/*                  R T _ B O O L W E A V E . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/**
 * Test the rt_boolweave() function.  For simplicity, we assume
 * single-threaded and have hard-coded usage of rt_uniresource.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for ap init */

#include "bu.h"
#include "rt/boolweave.h"
#include "rt/rt_instance.h"
#include "rt/resource.h"
#include "rt/db_io.h"
#include "rt/seg.h"


static struct seg*
create_segment(double in_dist, double out_dist)
{
    struct seg* segment;
    RT_GET_SEG(segment, (&rt_uniresource));
    segment->seg_in.hit_dist = in_dist;
    segment->seg_out.hit_dist = out_dist;
    return segment;
}



static void
free_segment(struct seg* segment)
{
    RT_FREE_SEG(segment, (&rt_uniresource))
}


static void
test_rt_boolweave(void)
{
    struct seg in_hd;
    struct seg out_hd;
    struct application ap;
    struct rt_i *rtip;
    struct partition partp;

    /* alas, raytrace instance is required for weaving */
    RT_APPLICATION_INIT(&ap);
    rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    ap.a_rt_i = rtip;
    ap.a_resource = &rt_uniresource;

    BU_LIST_INIT(&in_hd.l);
    BU_LIST_INIT(&out_hd.l);

    /* init partition list segments will be weaved into */
    partp.pt_forw = partp.pt_back = &partp;
    partp.pt_magic = PT_HD_MAGIC;

    /* Some segments to weave:
     *
     * seg1 |------------------|
     * seg2          |-------------------|
     *      0       10        20        30
     */
    struct seg* seg1 = create_segment(0.0, 20.0);
    struct seg* seg2 = create_segment(10.0, 30.0);

    BU_LIST_INSERT(&(in_hd.l), &(seg1->l));
    BU_LIST_INSERT(&(in_hd.l), &(seg2->l));

    // Weave it.

    rt_boolweave(&out_hd, &in_hd, &partp, &ap);

    // Check it.

    /* ... */

    // Clean up
    BU_LIST_DEQUEUE(&seg1->l);
    BU_LIST_DEQUEUE(&seg2->l);
    free_segment(seg1);
    free_segment(seg2);
}


int
main(int ac, char *av[])
{
    bu_setprogname(av[0]);
    if (ac && av)
	test_rt_boolweave();

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
