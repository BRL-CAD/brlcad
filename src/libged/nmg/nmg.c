/*                             N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @file libged/nmg.c
 *
 * The nmg command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "ged.h"
#include "../ged_private.h"


extern int ged_nmg_cmface_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_collapse_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_fix_normals_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_kill_f_core(struct ged* gedp, int argc, const char* argv[]);
extern int ged_nmg_kill_v_core(struct ged* gedp, int argc, const char* argv[]);
extern int ged_nmg_make_v_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_mm_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_nmg_move_v_core(struct ged* gedp, int argc, const char* argv[]);
extern int ged_nmg_simplify_core(struct ged *gedp, int argc, const char *argv[]);

int
ged_nmg_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "nmg object subcommand [V|F|R|S] [suffix]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* must be wanting help */
    if (argc < 3) {
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n\t%s\n", argv[0], usage);
    bu_vls_printf(gedp->ged_result_str, "commands:\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tmm             -  creates a new "
		  "NMG model structure and fills the appropriate fields. The result "
		  "is an empty model.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tcmface         -  creates a "
		  "manifold face in the first encountered shell of the NMG "
		  "object. Vertices are listed as the suffix and define the "
		  "winding-order of the face.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tkill V         -  removes the "
		  "vertexuse and vertex geometry of the selected vertex (via its "
		  "coordinates) and higher-order topology containing the vertex. "
		  "When specifying vertex to be removed, user generally will display "
		  "vertex coordinates in object via the MGED command labelvert.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tkill F         -  removes the "
		  "faceuse and face geometry of the selected face (via its "
		  "index). When specifying the face to be removed, user generally "
		  "will display face indices in object via the MGED command "
		  "labelface.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tmove V         -  moves an existing "
		  "vertex specified by the coordinates x_initial y_initial "
		  "z_initial to the position with coordinates x_final y_final "
		  "z_final.\n");
    bu_vls_printf(gedp->ged_result_str,
		  "\tmake V         -  creates a new "
		  "vertex in the nmg object.\n");
    return BRLCAD_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* advance CLI arguments for subcommands */
    --argc;
    ++argv;

    const char *subcmd = argv[0];
    if( BU_STR_EQUAL( "mm", subcmd ) ) {
	ged_nmg_mm_core(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "cmface", subcmd ) ) {
	ged_nmg_cmface_core(gedp, argc, argv);
    }
    else if( BU_STR_EQUAL( "kill", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    ged_nmg_kill_v_core(gedp, argc, argv);
	} else if ( BU_STR_EQUAL( "F", opt ) ) {
	    ged_nmg_kill_f_core(gedp, argc, argv);
	}
    }
    else if( BU_STR_EQUAL( "move", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    ged_nmg_move_v_core(gedp, argc, argv);
	}
    }
    else if( BU_STR_EQUAL( "make", subcmd ) ) {
	const char* opt = argv[2];
	if ( BU_STR_EQUAL( "V", opt ) ) {
	    ged_nmg_make_v_core(gedp, argc, argv);
	}
    }
    else {
	bu_vls_printf(gedp->ged_result_str, "%s is not a subcommand.", subcmd );
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl nmg_cmd_impl = {"nmg", ged_nmg_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_cmd = { &nmg_cmd_impl };

struct ged_cmd_impl nmg_cmface_cmd_impl = {"nmg_cmface", ged_nmg_cmface_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_cmface_cmd = { &nmg_cmface_cmd_impl };

struct ged_cmd_impl nmg_collapse_cmd_impl = {"nmg_collapse", ged_nmg_collapse_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_collapse_cmd = { &nmg_collapse_cmd_impl };

struct ged_cmd_impl nmg_fix_normals_cmd_impl = {"nmg_fix_normals", ged_nmg_fix_normals_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_fix_normals_cmd = { &nmg_fix_normals_cmd_impl };

struct ged_cmd_impl nmg_kill_f_cmd_impl = {"nmg_kill_f", ged_nmg_kill_f_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_kill_f_cmd = { &nmg_kill_f_cmd_impl };

struct ged_cmd_impl nmg_kill_v_cmd_impl = {"nmg_kill_v", ged_nmg_kill_v_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_kill_v_cmd = { &nmg_kill_v_cmd_impl };

struct ged_cmd_impl nmg_make_v_cmd_impl = {"nmg_make_v", ged_nmg_make_v_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_make_v_cmd = { &nmg_make_v_cmd_impl };

struct ged_cmd_impl nmg_mm_cmd_impl = {"nmg_mm", ged_nmg_mm_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_mm_cmd = { &nmg_mm_cmd_impl };

struct ged_cmd_impl nmg_move_v_cmd_impl = {"nmg_move_v", ged_nmg_move_v_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_move_v_cmd = { &nmg_move_v_cmd_impl };

struct ged_cmd_impl nmg_simplify_cmd_impl = {"nmg_simplify", ged_nmg_simplify_core, GED_CMD_DEFAULT};
const struct ged_cmd nmg_simplify_cmd = { &nmg_simplify_cmd_impl };

const struct ged_cmd *nmg_cmds[] = {
    &nmg_cmd,
    &nmg_cmface_cmd,
    &nmg_collapse_cmd,
    &nmg_fix_normals_cmd,
    &nmg_kill_f_cmd,
    &nmg_kill_v_cmd,
    &nmg_make_v_cmd,
    &nmg_mm_cmd,
    &nmg_move_v_cmd,
    &nmg_simplify_cmd,
    NULL
};

static const struct ged_plugin pinfo = { GED_API,  nmg_cmds, 10 };

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
