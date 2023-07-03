/*                  S U R F A C E . C P P
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
/** @file libged/brep/surface.cpp
 *
 * NURBS surfaces editing support for brep objects
 *
 */

#include "common.h"

#include "wdb.h"
#include "../ged_private.h"
#include "./ged_brep.h"

using namespace brlcad;

struct _ged_brep_isurface {
    struct _ged_brep_info *gb;
    struct bu_vls *vls;
    const struct bu_cmdtab *cmds;
};

static int
_brep_surface_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_brep_isurface *gb = (struct _ged_brep_isurface *)bs;
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
_brep_cmd_surface_create(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> surface create <x> <y> <z>";
    const char *purpose_string = "create a new NURBS surface";
    if (_brep_surface_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_isurface *gib = (struct _ged_brep_isurface *)bs;
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    argc--;argv++;
    ON_3dPoint position(0, 0, 0);
    if (argc >= 3) {
	position = ON_3dPoint(atof(argv[0]), atof(argv[1]), atof(argv[2]));
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

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create surface! id = %d", surfcode);
    return BRLCAD_OK;
}

static int
_brep_cmd_surface_birail(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> surface birail <curve_id_1> <curve_id_2>";
    const char *purpose_string = "create a new NURBS surface using two curves";
    if (_brep_surface_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_brep_isurface *gib = (struct _ged_brep_isurface *)bs;
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    argc--;argv++;
    if (argc != 2) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }
    int curve_id_1 = atoi(argv[0]);
    int curve_id_2 = atoi(argv[1]);
    int surfcode = brep_surface_create_ruled(b_ip->brep, curve_id_1, curve_id_2);
    if (surfcode < 0) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to create surface\n");
	return BRLCAD_ERROR;
    }

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

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gib->gb->gedp->ged_result_str, "create surface! id = %d", surfcode);
    return BRLCAD_OK;
}

static int
_brep_cmd_surface_remove(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> surface remove <surface_id>";
    const char *purpose_string = "remove a NURBS surface";
    if (_brep_surface_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;
    struct _ged_brep_isurface *gib = (struct _ged_brep_isurface *)bs;
    if (argc < 1) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    bool flag = brep_surface_remove(b_ip->brep, atoi(argv[0]));
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to remove surface %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

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

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_surface_move(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> surface move <surface_id> <x> <y> <z>";
    const char *purpose_string = "move a NURBS surface to a specified position";
    if (_brep_surface_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;
    struct _ged_brep_isurface *gib = (struct _ged_brep_isurface *)bs;
    if (argc < 4) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    ON_3dPoint p = ON_3dPoint(atof(argv[1]), atof(argv[2]), atof(argv[3]));
    bool flag = brep_surface_move(b_ip->brep, atoi(argv[0]), p);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to move surface %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

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

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_set_cv(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> surface set_cv <surface_id> <cv_id_u> <cv_id_v> <x> <y> <z> [<w>]";
    const char *purpose_string = "set a control vertex of a NURBS surface to a specified position";
    if (_brep_surface_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--;argv++;
    struct _ged_brep_isurface *gib = (struct _ged_brep_isurface *)bs;
    if (argc < 6) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    ON_4dPoint p = ON_4dPoint(atof(argv[3]), atof(argv[4]), atof(argv[5]), argc == 7 ? atof(argv[6]) : 1);
    bool flag = brep_surface_set_cv(b_ip->brep, atoi(argv[0]), atoi(argv[1]), atoi(argv[2]), p);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to move surface cv \n");
	return BRLCAD_ERROR;
    }

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

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
_brep_cmd_surface_trim(void *bs, int argc, const char **argv)
{
    const char *usage_string = "brep [options] <objname> trim <surface_id> <dir> <start_param> <end_param>";
    const char *purpose_string = "trim a NURBS surface using start and end parameters.";
    if (_brep_surface_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }
    argc--;argv++;
    struct _ged_brep_isurface *gib = (struct _ged_brep_isurface *)bs;
    if (argc < 4) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, " not enough arguments\n");
	bu_vls_printf(gib->gb->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    int surface_id = atoi(argv[0]);
    int dir = atoi(argv[1]);
    double start_param = atof(argv[2]);
    double end_param = atof(argv[3]);
    struct rt_brep_internal *b_ip = (struct rt_brep_internal *)gib->gb->intern.idb_ptr;
    bool flag = brep_surface_trim(b_ip->brep, surface_id, dir, start_param, end_param);
    if (!flag) {
	bu_vls_printf(gib->gb->gedp->ged_result_str, ": failed to trim surface %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

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

    if (mk_brep(wdbp, gib->gb->solid_name.c_str(), (void *)b_ip->brep)) {
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static void
_brep_surface_help(struct _ged_brep_isurface *bs, int argc, const char **argv)
{
    struct _ged_brep_isurface *gb = (struct _ged_brep_isurface *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gb->vls, "brep [options] <objname> surface <subcommand> [args]\n");
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

const struct bu_cmdtab _brep_surface_cmds[] = {
    { "create",              _brep_cmd_surface_create},
    { "birail",              _brep_cmd_surface_birail},
    { "remove",              _brep_cmd_surface_remove},
    { "move",                _brep_cmd_surface_move},
    { "set_cv",              _brep_cmd_set_cv},
    { "trim",                _brep_cmd_surface_trim},
    { (char *)NULL,          NULL}
};

int brep_surface(struct _ged_brep_info *gb, int argc, const char **argv)
{
    struct _ged_brep_isurface gib;
    gib.gb = gb;
    gib.vls = gb->gedp->ged_result_str;
    gib.cmds = _brep_surface_cmds;

    const ON_Brep *brep = ((struct rt_brep_internal *)(gb->intern.idb_ptr))->brep;
    if (brep == NULL) {
	bu_vls_printf(gib.vls, "not a brep object\n");
	return BRLCAD_ERROR;
    }

    if (!argc) {
	_brep_surface_help(&gib, 0, NULL);
	return BRLCAD_OK;
    }

    // TODO: add help flag
    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
