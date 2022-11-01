/*                        E D I T I T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/editit.c
 *
 * The editit function.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "bio.h"
#include "bresource.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/path.h"
#include "ged.h"
#include "../ged_private.h"


int
ged_editit_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = 0;
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* FIXME: this should NOT assume that argv[2] and argv[4] are the
     * edit string and temp file.  should use bu_getopt().
     */
    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Internal Error: \"%s -e editstring -f tmpfile\" is malformed (argc == %d)", argv[0], argc);
	return GED_ERROR;
    } else {
	char *edstr = bu_strdup((char *)argv[2]);
	ret = _ged_editit(edstr, argv[4]);
	bu_free(edstr, "free tmp editstring copy");
	return ret;
    }
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl editit_cmd_impl = {
    "editit",
    ged_editit_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd editit_cmd = { &editit_cmd_impl };
const struct ged_cmd *editit_cmds[] = { &editit_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  editit_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
