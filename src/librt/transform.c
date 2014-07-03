/*                     T R A N S F O R M . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2014 United States Government as represented by
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

#include "vmath.h"
#include "raytrace.h"


int
rt_matrix_transform(struct rt_db_internal *output, const mat_t matrix, struct rt_db_internal *input, int freeflag, struct db_i *dbip, struct resource *resource)
{
    int ret;

    RT_CK_DB_INTERNAL(output);
    RT_CK_DB_INTERNAL(input);
    RT_CK_DBI(dbip);
    RT_CK_RESOURCE(resource);

    ret = -1;
    if (OBJ[input->idb_type].ft_xform) {
	ret = OBJ[input->idb_type].ft_xform(output, matrix, input, freeflag, dbip, resource);
    }

    return ret;
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
