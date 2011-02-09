/*                  O B J _ M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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

#include "raytrace.h"


int
rt_obj_mirror(struct rt_db_internal *ip, const plane_t *plane)
{
    int id;
    const struct rt_functab *ft;

    if (!ip || !plane)
	return -1;

    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &rt_functab[id];
    if (!ft)
	return -3;

#if 0
    /* !!! Does not yet exist, need to push rt_mirror() internals down */
    if (!ft->ft_mirror)
	return -4;

    ft->ft_mirror(ip, plane);
#endif
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
