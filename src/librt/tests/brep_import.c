/*                    B R E P _ I M P O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/malloc.h"
#include "raytrace.h"


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 2;

    /* A failed OpenNURBS archive read can leave an empty component manifest.
     * The importer must reject that state without dereferencing a nonexistent
     * geometry item, and its partially initialized internal must be freeable. */
    struct bu_external invalid_external = BU_EXTERNAL_INIT_ZERO;
    invalid_external.ext_nbytes = 32;
    invalid_external.ext_buf = (uint8_t *)bu_calloc(
	invalid_external.ext_nbytes, 1, "invalid BRep archive");

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    const int import_result = OBJ[ID_BREP].ft_import5(&intern,
	&invalid_external, bn_mat_identity, NULL);
    if (intern.idb_ptr)
	rt_db_free_internal(&intern);
    bu_free_external(&invalid_external);

    return import_result < 0 ? 0 : 1;
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
