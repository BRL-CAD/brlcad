/*                            E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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
    bu_setprogname(av[0]);

    long int all_mem = bu_mem(BU_MEM_ALL);
    long int avail_mem = bu_mem(BU_MEM_AVAIL);
    long int page_mem = bu_mem(BU_MEM_PAGE_SIZE);

    bu_log("MEM report: all(%ld) avail(%ld) page_size(%ld)\n", all_mem, avail_mem, page_mem);

    if (all_mem < 0 || avail_mem < 0 || page_mem < 0)
	return -1;

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
