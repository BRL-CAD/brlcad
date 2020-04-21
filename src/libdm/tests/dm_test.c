/*                        D M _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2020 United States Government as represented by
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
main(int UNUSED(argc), const char **UNUSED(argv))
{
    dm_list_backends(NULL);
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
    dmp = dm_popen(NULL, "nu", 1, &av0);
    bu_log("dmp name: %s\n", dm_get_name(dmp));
    dm_close(dmp);
    dmp = dm_popen(NULL, "txt", 1, &av0);
    bu_log("dmp name: %s\n", dm_get_name(dmp));
    dm_close(dmp);


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
