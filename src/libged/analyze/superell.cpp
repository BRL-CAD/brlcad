/*                    S U P E R E L L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file superell.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include "vmath.h"

#include "rt/geom.h"
#include "ged/defines.h"
#include "../ged_private.h"
#include "./ged_analyze.h"

#define PROLATE 1
#define OBLATE 2

void
analyze_superell(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;

    RT_SUPERELL_CK_MAGIC(superell);

    fastf_t ma = MAGNITUDE(superell->a);
    fastf_t mb = MAGNITUDE(superell->b);
    fastf_t mc = MAGNITUDE(superell->c);
    fastf_t vol = 4.0 * M_PI * ma * mb * mc / 3.0;
    fastf_t ecc = 0.0;
    fastf_t major_mag = 0.0;
    fastf_t minor_mag = 0.0;
    fastf_t sur_area = 0.0;
    int type = 0;

    if (fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001) {
	/* have a sphere */
	sur_area = 4.0 * M_PI * ma * ma;
	goto print_results;
    }
    if (fabs(ma-mb) < .00001) {
	/* A == B */
	if (mc > ma) {
	    /* oblate spheroid */
	    type = OBLATE;
	    major_mag = mc;
	    minor_mag = ma;
	} else {
	    /* prolate spheroid */
	    type = PROLATE;
	    major_mag = ma;
	    minor_mag = mc;
	}
    } else
	if (fabs(ma-mc) < .00001) {
	    /* A == C */
	    if (mb > ma) {
		/* oblate spheroid */
		type = OBLATE;
		major_mag = mb;
		minor_mag = ma;
	    } else {
		/* prolate spheroid */
		type = PROLATE;
		major_mag = ma;
		minor_mag = mb;
	    }
	} else
	    if (fabs(mb-mc) < .00001) {
		/* B == C */
		if (ma > mb) {
		    /* oblate spheroid */
		    type = OBLATE;
		    major_mag = ma;
		    minor_mag = mb;
		} else {
		    /* prolate spheroid */
		    type = PROLATE;
		    major_mag = mb;
		    minor_mag = ma;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   Cannot find surface area\n");
		return;
	    }
    ecc = sqrt(major_mag*major_mag - minor_mag*minor_mag) / major_mag;
    if (type == PROLATE) {
	sur_area = M_2PI * minor_mag * minor_mag +
	(M_2PI * (major_mag*minor_mag/ecc) * asin(ecc));
    } else {
	/* type == OBLATE */
	sur_area = M_2PI * major_mag * major_mag +
	(M_PI * (minor_mag*minor_mag/ecc) * log((1.0+ecc)/(1.0-ecc)));
    }

print_results:
    print_volume_table(gedp,
		       vol
		      * gedp->dbip->dbi_base2local
		      * gedp->dbip->dbi_base2local
		      * gedp->dbip->dbi_base2local,
		       sur_area
		      * gedp->dbip->dbi_base2local
		      * gedp->dbip->dbi_base2local,
		       vol/GALLONS_TO_MM3
		      );
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
