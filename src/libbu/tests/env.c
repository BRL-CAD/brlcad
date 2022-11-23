/*                            E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu.h"

int
main(int UNUSED(ac), char *av[])
{
    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    ssize_t all_mem = bu_mem(BU_MEM_ALL, NULL);
    if (all_mem < 0)
	return -1;
    ssize_t avail_mem = bu_mem(BU_MEM_AVAIL, NULL);
    if (avail_mem < 0)
	return -2;
    ssize_t page_mem = bu_mem(BU_MEM_PAGE_SIZE, NULL);
    if (page_mem < 0)
	return -3;

    /* make sure passing works too */
    size_t all_mem2 = 0;
    size_t avail_mem2 = 0;
    size_t page_mem2 = 0;
    (void)bu_mem(BU_MEM_ALL, &all_mem2);
    if (all_mem2 != (size_t)all_mem)
	return -4;
    (void)bu_mem(BU_MEM_AVAIL, &avail_mem2);
    if (avail_mem2 != (size_t)avail_mem)
	return -5;
    (void)bu_mem(BU_MEM_PAGE_SIZE, &page_mem2);
    if (page_mem2 != (size_t)page_mem)
	return -6;

    char all_buf[6] = {'\0'};
    char avail_buf[6] = {'\0'};
    char p_buf[6] = {'\0'};
    bu_humanize_number(all_buf, 5, all_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
    bu_humanize_number(avail_buf, 5, avail_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
    bu_humanize_number(p_buf, 5, page_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);

    bu_log("MEM report: all: %s(%zd) avail: %s(%zd) page_size: %s(%zd)\n",
	    all_buf, all_mem,
	    avail_buf, avail_mem,
	    p_buf, page_mem);

    return 0;
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
