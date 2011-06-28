/*                         S U B T Y P E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/subtype.c
 *
 * Given an arb, ell or tgc determine if the values contained in
 * the data structure actually represent a less general shape.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


char *
_ged_arb8_subtype(struct ged *gedp, struct rt_arb_internal *arb)
{
    int arb_type;
    RT_ARB_CK_MAGIC(arb);
    arb_type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);
    switch (arb_type) {
	case ARB4:
	    return "arb4";
	    break;
	case ARB5:
	    return "arb5";
	    break;
	case ARB6:
	    return "arb6";
	    break;
	case ARB7:
	    return "arb7";
	    break;
	default:
	    return 0;
	    break;
    }
    return 0;
}

char *
_ged_ell_subtype(struct ged *gedp, struct rt_ell_internal *ell)
{
    fastf_t mag_diff;
    if (NEAR_EQUAL(MAGNITUDE(ell->a), MAGNITUDE(ell-b), 2*RT_LEN_TOL)) {
	if (NEAR_EQUAL(MAGNITUDE(ell->a), MAGNITUDE(ell-c), 2*RT_LEN_TOL)) {
	    return "sph";
	}
	return 0;
    }

    char *
	_ged_tgc_subtype(struct ged *gedp, struct rt_tgc_internal *tgc)
    {
	return 0;
    }

    char *
	_ged_eto_subtype(struct ged *gedp, struct rt_eto_internal *tgc)
    {
	return 0;
    }

/*
 *  			_ G E D _ S U B T Y P E
 *
 *   Examine the numerical properties of an instance of a general primitive type and return
 *   the simplest geometric type capable of representing the given values (if no simpler
 *   type can describe the shape, return nothing.
 */
    char *
	_ged_subtype(struct ged	*gedp)
    {
	int id;
	struct rt_db_internal intern;
	rt_db_get_internal(&intern, DB_FULL_PATH_CUR_DIR(entry), gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource);
	if(intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) return 0;
	switch (intern.idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_ARB8:
		return _ged_arb8_subtype(gedp, (struct rt_arb_internal *)intern.idb_ptr);
		break;
	    case DB5_MINORTYPE_BRLCAD_ELL:
		return _ged_ell_subtype(gedp, (struct rt_ell_internal *)intern.idb_ptr);
		break;
	    case DB5_MINORTYPE_BRLCAD_TGC:
		return _ged_tgc_subtype(gedp, (struct rt_ell_internal *)intern.idb_ptr);
		break;
	    case DB5_MINORTYPE_BRLCAD_ETO:
		return _ged_eto_subtype(gedp, (struct rt_ell_internal *)intern.idb_ptr);
		break;

	    default:
		return 0;
		break;
	}
	return 0;
    }


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
