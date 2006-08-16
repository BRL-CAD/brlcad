/*                     T R A N S F O R M . C
 * BRL-CAD
 *
 * Copyright (c) 2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
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
/** @file transform.c
 *
 */
#include "common.h"

#include "machine.h"
#include "raytrace.h"


/** r t _ m a t r i x _ t r a n s f o r m
 *
 * apply a matrix transformation to a given input object, setting the
 * resultant transformed object as the output solid.  if free is set,
 * the input object will be released.
 *
 * returns zero if matrix transform was applied, non-zero on failure.
 */
int
rt_matrix_transform(struct rt_db_internal *output, const mat_t matrix, struct rt_db_internal *input, int free, struct db_i *dbip, struct resource *resource)
{
    int ret;

    RT_CK_DB_INTERNAL(output);
    RT_CK_DB_INTERNAL(input);
    RT_CK_DBI(dbip);
    RT_CK_RESOURCE(resource);

    ret = rt_functab[input->idb_type].ft_xform( output, matrix, input, free, dbip, resource );

    return ret;
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
