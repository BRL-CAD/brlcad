/*                  S U R F A C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file libged/brep/surface.cpp
 *
 * NURBS surfaces editing support for brep objects
 *
 */

#include "common.h"

#include <set>
#include <vector>

#include "opennurbs.h"
#include "wdb.h"
#include "bu/cmd.h"
#include "../ged_private.h"
#include "./ged_brep.h"

using namespace brlcad;

struct _ged_brep_isurf {
    struct _ged_brep_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

static int
_brep_surface_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_isurf *gb = (struct _ged_brep_isurf *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->vls, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->vls, "%s\n", ps);
	return 1;
    }
    return 0;
}

extern "C" int
_brep_cmd_surface_create(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> surface create <x> <y> <z>";
    const char *purpose_string = "create a new NURBS surface";
    if (_brep_surface_msgs(bs, argc, argv, usage_string, purpose_string))
    {
        return BRLCAD_OK;
    }

    struct _ged_brep_isurf *gib = (struct _ged_brep_isurf *)bs;
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    argc--;argv++;
    std::vector<double> position;
    if(argc != 3)
    {
        position.push_back(0.0);
        position.push_back(0.0);
        position.push_back(0.0);
    }
    else
    {
        position.push_back(atof(argv[0]));
        position.push_back(atof(argv[1]));
        position.push_back(atof(argv[2]));
    }
    int surfcode = brep_surface_make(b_ip->brep, position);

    // Delete the old object
    const char *av[3];
    char *ncpy = bu_strdup(gib->gb->solid_name.c_str());
    av[0] = "kill";
    av[1] = ncpy;
    av[2] = NULL;
    (void)ged_exec(gib->gb->gedp, 2, (const char **)av);
    bu_free(ncpy, "free name cpy");

    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep))
    {
        return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create surface! id = %d", surfcode);
    return BRLCAD_OK;
}

static void
_brep_surface_help(struct _ged_brep_isurf *bs, int argc, const char **argv)
{
    struct _ged_brep_isurf *gb = (struct _ged_brep_isurf *)bs;
    if (!argc || !argv)
    {
    bu_vls_printf(gb->vls, "brep [options] <objname> surface <subcommand> [args]\n");
    bu_vls_printf(gb->vls, "Available subcommands:\n");
    const struct bu_cmdtab *ctp = NULL;
    int ret;
    const char *helpflag[2];
    helpflag[1] = PURPOSEFLAG;
    for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++)
    {
    bu_vls_printf(gb->vls, "  %s\t\t", ctp->ct_name);
    helpflag[0] = ctp->ct_name;
    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
    }
    else
    {
    int ret;
    const char *helpflag[2];
    helpflag[0] = argv[0];
    helpflag[1] = HELPFLAG;
    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
}

const struct bu_cmdtab _brep_surface_cmds[] = {
    {"create", _brep_cmd_surface_create},
    {(char *)NULL, NULL}
};

int
brep_surface(struct _ged_brep_info *gb, int argc, const char **argv)
{
    struct _ged_brep_isurf gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _brep_surface_cmds;

    const ON_Brep *brep = ((struct rt_brep_internal *)(gb->intern.idb_ptr))->brep;
    if (brep == NULL)
    {
    bu_vls_printf(gib.vls, "not a brep object\n");
    return BRLCAD_ERROR;
    }

    if (!argc)
    {
    _brep_surface_help(&gib, 0, NULL);
    return BRLCAD_OK;
    }

    // TODO: add help flag
    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG))
    {
    argc--;argv++;
    argc--;argv++;
    _brep_surface_help(&gib, argc, argv);
    return BRLCAD_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_surface_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_brep_surface_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_surface_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }
    return BRLCAD_ERROR;
}