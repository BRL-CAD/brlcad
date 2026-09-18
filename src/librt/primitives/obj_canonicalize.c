/*                 O B J _ C A N O N I C A L I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#include "rt/func.h"


int
rt_obj_canonicalize(struct rt_db_internal *canonical,
		    mat_t canonical_to_input,
		    const struct rt_db_internal *input,
		    const struct bn_tol *tol,
		    enum rt_canonicalize_mode mode)
{
    const struct rt_functab *ft;

    if (!canonical || !canonical_to_input || !input || !tol)
	return RT_CANONICALIZE_ERROR;

    RT_CK_DB_INTERNAL(canonical);
    RT_CK_DB_INTERNAL(input);
    BN_CK_TOL(tol);

    if (canonical->idb_ptr || canonical->idb_meth ||
	canonical->idb_major_type != -1 || canonical->idb_minor_type != -1)
	return RT_CANONICALIZE_ERROR;

    if (mode < RT_CANONICALIZE_RIGID || mode > RT_CANONICALIZE_AFFINE)
	return RT_CANONICALIZE_ERROR;

    if (input->idb_minor_type < 0 || input->idb_minor_type > ID_MAX_SOLID)
	return RT_CANONICALIZE_ERROR;

    ft = &OBJ[input->idb_minor_type];
    if (!ft->ft_canonicalize)
	return RT_CANONICALIZE_UNSUPPORTED;

    return ft->ft_canonicalize(canonical, canonical_to_input, input, tol, mode);
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
