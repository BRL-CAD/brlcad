/*                        D M _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "dm.h"

int
main(int UNUSED(argc), const char **argv)
{

    bu_setprogname(argv[0]);

    bu_log("load msgs: %s\n", dm_init_msgs());

    struct bu_vls blist = BU_VLS_INIT_ZERO;
    dm_list_types(&blist, "\n	");
    bu_log("Available types:\n	%s\n", bu_vls_cstr(&blist));
    bu_vls_free(&blist);

    int vtype;
    vtype = dm_valid_type("nu", NULL);
    bu_log("nu valid: %d\n", vtype);
    vtype = dm_valid_type("plot", NULL);
    bu_log("plot valid: %d\n", vtype);
    vtype = dm_valid_type("X", NULL);
    bu_log("X valid: %d\n", vtype);
    vtype = dm_valid_type("ogl", NULL);
    bu_log("ogl valid: %d\n", vtype);
    vtype = dm_valid_type("osgl", NULL);
    bu_log("osgl valid: %d\n", vtype);
    vtype = dm_valid_type("wgl", NULL);
    bu_log("wgl valid: %d\n", vtype);

    const char *av0 = "attach";
    struct dm *dmp;
    dmp = dm_open(NULL, "nu", 1, &av0);
    const char *name = dm_get_name(dmp);
    bu_log("dmp name: %s\n", (name)?name:"(NULL)");
    dm_close(dmp);
    dmp = dm_open(NULL, "txt", 1, &av0);
    name = dm_get_name(dmp);
    bu_log("dmp name: %s\n", (name)?name:"(NULL)");
    dm_close(dmp);

    bu_log("recommended type: %s\n", dm_bestXType(NULL));

    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
