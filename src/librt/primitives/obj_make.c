/*                    O B J _ M A K E . C
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
rt_obj_make(const struct rt_functab *ftp, struct rt_db_internal *ip)
{
    int id;
    const struct rt_functab *ft;

    if (!ftp || !ip)
	return -1;

    RT_CK_FUNCTAB(ftp);
    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_minor_type;
    if (id < 0)
	return -2;

    ft = &rt_functab[id];
    if (!ft)
	return -3;
    if (!ft->ft_make)
	return -4;

    /* extra insanity checking */
    if (ft->ft_make != ftp->ft_make)
	return -5;

    ft->ft_make(ftp, ip);
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
