/*                G E O M E T R Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @file libged/brep/geometry.cpp
 *
 * NURBS geometry editing support for brep objects
 *
 */

#include "common.h"

#include "wdb.h"
#include "../ged_private.h"
#include "./ged_brep.h"

using namespace brlcad;

struct _ged_brep_igeo {
    struct _ged_brep_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

static int
_brep_geo_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_igeo *gb = (struct _ged_brep_igeo *)bs;
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

static int
_brep_cmd_geo_create_vertex(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> geo create_v <x> <y> <z>";
    const char *purpose_string = "create a new NURBS vertex";
    if (_brep_geo_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_igeo *gib = (struct _ged_brep_igeo *)bs;
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    argc--;argv++;
    if (argc < 3) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, "not enough 	arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", 	usage_string);
	return BRLCAD_ERROR;
    }
    ON_3dPoint position(atof(argv[0]), atof(argv[1]), atof(argv[2]));
    int vertex = brep_vertex_create(b_ip->brep, position);
    // Make the new one
    struct rt_wdb *wdbp = wdb_dbopen(gib->gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create vertex! id = %d", vertex);
    return BRLCAD_OK;
}

static void
_brep_geo_help(struct _ged_brep_igeo *bs, int argc, const char **argv)
{
    struct _ged_brep_igeo *gb = (struct _ged_brep_igeo *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "brep [options] <objname> topo <subcommand> [args]\n");
	bu_vls_printf(gb->vls, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	for (ctp = gb->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gb->vls, "  %s\t\t", ctp->ct_name);
	    helpflag[0] = ctp->ct_name;
	    bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
	}
    }
    else {
	int ret;
	const char *helpflag[2];
	helpflag[0] = argv[0];
	helpflag[1] = HELPFLAG;
	bu_cmd(gb->cmds, 2, helpflag, 0, (void *)gb, &ret);
    }
}

const struct bu_cmdtab _brep_geo_cmds[] = {
    { "create_v",            _brep_cmd_geo_create_vertex},
    { (char *)NULL,          NULL}
};

int brep_geo(struct _ged_brep_info *gb, int argc, const char **argv)
{
    struct _ged_brep_igeo gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _brep_geo_cmds;

    const ON_Brep *brep = ((struct rt_brep_internal *)(gb->intern.idb_ptr))->brep;
    if (brep == NULL) {
	bu_vls_printf(gib.vls, "not a brep object\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	_brep_geo_help(&gib, 0, NULL);
	return BRLCAD_OK;
    }

    // TODO: add help flag
    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--;argv++;
	argc--;argv++;
	_brep_geo_help(&gib, argc, argv);
	return BRLCAD_OK;
    }

    // Must have valid subcommand to process
    if (bu_cmd_valid(_brep_geo_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_brep_geo_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_brep_geo_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }
    return BRLCAD_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
