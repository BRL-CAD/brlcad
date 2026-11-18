/*                    V 4 _ R E G I O N . C
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

#include <limits.h>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"

int
main(int UNUSED(argc), const char **argv)
{
    struct rt_db_internal object;
    struct rt_comb_internal combination;
    long *fields[] = {&combination.region_id, &combination.aircode, &combination.GIFTmater, &combination.los};
    const long valid[] = {SHRT_MIN, SHRT_MAX};
    const long invalid[] = {(long)SHRT_MIN - 1, (long)SHRT_MAX + 1};
    bu_setprogname(argv[0]);
    RT_DB_INTERNAL_INIT(&object);
    RT_COMB_INTERNAL_INIT(&combination);
    combination.region_flag = 1;
    object.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    object.idb_type = ID_COMBINATION;
    object.idb_meth = &OBJ[ID_COMBINATION];
    object.idb_ptr = &combination;
    for (size_t field = 0; field < sizeof(fields) / sizeof(fields[0]); ++field) {
	for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
	    struct bu_external output = BU_EXTERNAL_INIT_ZERO;
	    *fields[field] = valid[i];
	    int status = object.idb_meth->ft_export4(&output, &object, 1.0, NULL);
	    bu_free_external(&output);
	    if (status < 0) {
		bu_log("V4 export rejected representable field %zu value %ld\n", field, valid[i]);
		return 1;
	    }
	}
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
	    struct bu_external output = BU_EXTERNAL_INIT_ZERO;
	    *fields[field] = invalid[i];
	    int status = object.idb_meth->ft_export4(&output, &object, 1.0, NULL);
	    bu_free_external(&output);
	    if (status >= 0) {
		bu_log("V4 export accepted unrepresentable field %zu value %ld\n", field, invalid[i]);
		return 1;
	    }
	}
	*fields[field] = 0;
    }
    bu_vls_free(&combination.shader);
    bu_vls_free(&combination.material);
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
