/*                    O B J _ M A K E . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2026 United States Government as represented by
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
rt_obj_make(const char *label, const point_t origin, double scale, struct rt_db_internal *ip)
{
    const struct rt_functab *ftp;

    if (!label || !ip)
	return BRLCAD_ERROR;

    RT_CK_DB_INTERNAL(ip);

    /* minimal gate since there's no actual callers of this yet
     * TODO: variant->base alias table + deny list
     */
    ftp = rt_get_functab_by_label(label);
    if (!ftp || !ftp->ft_make)
	return BRLCAD_ERROR;

    return ftp->ft_make(ftp, ip, label, origin, scale);
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
