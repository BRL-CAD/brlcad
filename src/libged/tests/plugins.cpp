/*                        P L U G I N S . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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

#include <set>
#include <map>

#include <stdio.h>
#include <bu.h>
#include <ged.h>

int
main(int ac, char *av[]) {
    struct ged *gbp;

    bu_setprogname(av[0]);

    if (ac < 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }

    ac--; av++;

    if (BU_STR_EQUAL(av[0], "-l")) {
	const char *imsgs = ged_init_msgs();
	bu_log("%s\n", ged_init_msgs());
	return (strlen(imsgs) > 0) ? BRLCAD_ERROR : BRLCAD_OK;
    }

    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    gbp = ged_open("db", av[1], 1);

    const char * const *cmds = NULL;
    size_t cmds_cnt = ged_cmd_list(&cmds);

    bu_log("%s\n", ged_init_msgs());

    for (size_t i = 0; i < cmds_cnt; i++) {
	bu_log("%s\n", cmds[i]);
    }

#if 0
    for (size_t i = 0; i < cmds_cnt; i++) {
	bu_log("ged_execing %s\n", cmds[i]);
	ged_exec(gbp, 1, (const char **)&cmds[i]);
    }
#endif

    /* These are the commands from the MGED and libtclcad command tables, the
     * latter being limited to the ones with a non-null ged function pointer.*/
    const char *app_cmds[] = {
	    "3ptarb",
	    "adc",
	    "adjust",
	    "ae",
	    "ae2dir",
	    "aet",
	    "analyze",
	    "annotate",
	    "arb",
	    "arced",
	    "arot",
	    "attr",
	    "bb",
	    "bev",
	    "bo",
	    "bot",
	    "bot_condense",
	    "bot_decimate",
	    "bot_dump",
	    "bot_face_fuse",
	    "bot_face_sort",
	    "bot_flip",
	    "bot_fuse",
	    "bot_merge",
	    "bot_smooth",
	    "bot_split",
	    "bot_sync",
	    "bot_vertex_fuse",
	    "brep",
	    "c",
	    "cat",
	    "cc",
	    "center",
	    "check",
	    "clear",
	    "clone",
	    "coil",
	    "color",
	    "comb",
	    "comb_color",
	    "combmem",
	    "constraint",
	    "copyeval",
	    "copymat",
	    "cp",
	    "cpi",
	    "d",
	    "dbconcat",
	    "dbfind",
	    "db_glob",
	    "dbip",
	    "dbot_dump",
	    "dbversion",
	    "debug",
	    "debugbu",
	    "debugdir",
	    "debuglib",
	    "debugnmg",
	    "decompose",
	    "delay",
	    "dir2ae",
	    "draw",
	    "dsp",
	    "dump",
	    "dup",
	    "e",
	    "E",
	    "eac",
	    "echo",
	    "edarb",
	    "edcodes",
	    "edcolor",
	    "edcomb",
	    "edit",
	    "edmater",
	    "env",
	    "erase",
	    "ev",
	    "exists",
	    "expand",
	    "eye",
	    "eye_pos",
	    "eye_pt",
	    "facetize",
	    "fb2pix",
	    "fbclear",
	    "find_arb_edge",
	    "find_bot_edge",
	    "find_bot_pnt",
	    "find_pipe_pnt",
	    "form",
	    "fracture",
	    "g",
	    "gdiff",
	    "get",
	    "get_autoview",
	    "get_bot_edges",
	    "get_comb",
	    "get_dbip",
	    "get_eyemodel",
	    "get_type",
	    "glob",
	    "gqa",
	    "graph",
	    "grid",
	    "grid2model_lu",
	    "grid2view_lu",
	    "heal",
	    "hide",
	    "how",
	    "human",
	    "i",
	    "idents",
	    "illum",
	    "importFg4Section",
	    "in",
	    "inside",
	    "isize",
	    "item",
	    "joint",
	    "joint2",
	    "keep",
	    "keypoint",
	    "kill",
	    "killall",
	    "killrefs",
	    "killtree",
	    "l",
	    "lc",
	    "lint",
	    "listeval",
	    "loadview",
	    "lod",
	    "log",
	    "lookat",
	    "ls",
	    "lt",
	    "m2v_point",
	    "make_name",
	    "make_pnts",
	    "mat4x3pnt",
	    "mat_ae",
	    "match",
	    "mater",
	    "mat_mul",
	    "mat_scale_about_pnt",
	    "metaball_delete_pnt",
	    "metaball_move_pnt",
	    "mirror",
	    "model2grid_lu",
	    "model2view",
	    "model2view_lu",
	    "mouse_add_metaball_pnt",
	    "mouse_append_pipe_pnt",
	    "mouse_move_metaball_pnt",
	    "mouse_move_pipe_pnt",
	    "mouse_prepend_pipe_pnt",
	    "move_arb_edge",
	    "move_arb_face",
	    "mv",
	    "mvall",
	    "nirt",
	    "nmg",
	    "nmg_collapse",
	    "nmg_fix_normals",
	    "nmg_simplify",
	    "ocenter",
	    "open",
	    "orient",
	    "orientation",
	    "orotate",
	    "oscale",
	    "otranslate",
	    "overlay",
	    "pathlist",
	    "paths",
	    "perspective",
	    "pipe_append_pnt",
	    "pipe_delete_pnt",
	    "pipe_move_pnt",
	    "pipe_prepend_pnt",
	    "pix2fb",
	    "plot",
	    "pmat",
	    "pmodel2view",
	    "png",
	    "png2fb",
	    "pngwf",
	    "pnts",
	    "postscript",
	    "prcolor",
	    "prefix",
	    "preview",
	    "process",
	    "protate",
	    "pscale",
	    "pset",
	    "ptranslate",
	    "pull",
	    "push",
	    "put",
	    "put_comb",
	    "putmat",
	    "qray",
	    "quat",
	    "qvrot",
	    "r",
	    "rcodes",
	    "rect",
	    "red",
	    "regdef",
	    "regions",
	    "rfarb",
	    "rm",
	    "rmap",
	    "rmat",
	    "rmater",
	    "rot",
	    "rot_about",
	    "rotate_arb_face",
	    "rot_point",
	    "rrt",
	    "rselect",
	    "rt",
	    "rtabort",
	    "rtarea",
	    "rtcheck",
	    "rtedge",
	    "rtweight",
	    "rtwizard",
	    "savekey",
	    "saveview",
	    "sca",
	    "screengrab",
	    "search",
	    "select",
	    "set_output_script",
	    "set_transparency",
	    "set_uplotOutputMode",
	    "setview",
	    "shaded_mode",
	    "shader",
	    "shells",
	    "showmats",
	    "simulate",
	    "size",
	    "slew",
	    "solid_report",
	    "solids",
	    "solids_on_ray",
	    "summary",
	    "sv",
	    "sync",
	    "t",
	    "tire",
	    "title",
	    "tol",
	    "tops",
	    "tra",
	    "track",
	    "tree",
	    "unhide",
	    "units",
	    "v2m_point",
	    "vdraw",
	    "version",
	    "view",
	    "view2grid_lu",
	    "view2model",
	    "view2model_lu",
	    "view2model_vec",
	    "viewdir",
	    "vnirt",
	    "voxelize",
	    "wcodes",
	    "whatid",
	    "whichair",
	    "whichid",
	    "which_shader",
	    "who",
	    "wmater",
	    "x",
	    "xpush",
	    "ypr",
	    "zap",
	    "zoom",
	    NULL
    };

    {
	int app_cmd_cnt = 0;
	const char *ccmd = app_cmds[app_cmd_cnt];
	while (ccmd) {
	    bu_log("\nTest %d: ged_execing app command %s\n", app_cmd_cnt, ccmd);
	    int ret = ged_exec(gbp, 1, (const char **)&ccmd);
	    if (ret != GED_OK && ret != GED_HELP) {
		bu_log("%s\n", bu_vls_cstr(gbp->ged_result_str));
	    }
	    bu_vls_trunc(gbp->ged_result_str, 0);
	    app_cmd_cnt++;
	    ccmd = app_cmds[app_cmd_cnt];
	}
    }

    /* Deliberately call a ged function with invalid argv[0] */
    {
	const char *wav0 = "wrong_name";
	const char *wav[2];
	wav[0] = wav0;
	wav[1] = NULL;
	int ret = ged_ls(gbp, 1, (const char **)wav);
	if (ret & GED_UNKNOWN && wav[0] != wav0) {
	    bu_log("\nged_ls called with command name \"%s\" correctly returned with GED_UNKNOWN set\n", wav0);
	} else {
	    if (!(ret & GED_UNKNOWN)) {
		bu_log("\nged_ls called with command name \"%s\" did not return with GED_UNKNOWN set\n", wav0);
	    }
	    if (wav[0] == wav0) {
		bu_log("\nged_ls called with command name \"%s\" did not override wav[0]\n", wav0);
	    }
	}
    }

    /* Deliberately call a ged function with wrong argv[0] (which is a valid
     * command, just not one matching the function) */
    {
	const char *wav1 = "search";
	const char *wav[2];
	wav[0] = wav1;
	wav[1] = NULL;
	int ret = ged_ls(gbp, 1, (const char **)wav);
	if (ret & GED_UNKNOWN && wav[0] != wav1) {
	    bu_log("\nged_ls called with command name \"%s\" correctly returned with GED_UNKNOWN set\n", wav1);
	} else {
	    if (!(ret & GED_UNKNOWN)) {
		bu_log("\nged_ls called with command name \"%s\" did not return with GED_UNKNOWN set\n", wav1);
	    }
	    if (wav[0] == wav1) {
		bu_log("\nged_ls called with command name \"%s\" did not override wav[0]\n", wav1);
	    }
	}
    }

    /* Find functions sets that are aliases */
    {
	std::set<std::string> cmd_set;
	// Start with the app commands
	int app_cmd_cnt = 0;
	const char *ccmd = app_cmds[app_cmd_cnt];
	while (ccmd) {
	    cmd_set.insert(std::string(ccmd));
	    app_cmd_cnt++;
	    ccmd = app_cmds[app_cmd_cnt];
	}
	// Also add libged's list in case there are any
	// we missed calling out explicitly above...
	for (size_t i = 0; i < cmds_cnt; i++) {
	    cmd_set.insert(std::string(cmds[i]));
	}

	std::set<std::string>::iterator cs_it;
	std::map<std::string, std::set<std::string>> alias_sets;
	while (cmd_set.size()) {
	    std::string cmd = *cmd_set.begin();
	    cmd_set.erase(cmd);
	    std::set<std::string> cdone;
	    for (cs_it = cmd_set.begin(); cs_it != cmd_set.end(); cs_it++) {
		std::string cs = *cs_it;
		int cval = ged_cmd_valid(cmd.c_str(), cs.c_str());
		if (cval == 0) {
		    // Aliases - use cmd as the key
		    alias_sets[cmd].insert(cmd);
		    alias_sets[cmd].insert(cs);
		    cdone.insert(cs);
		}
	    }
	    for (cs_it = cdone.begin(); cs_it != cdone.end(); cs_it++) {
		cmd_set.erase(*cs_it);
	    }
	}
	std::map<std::string, std::set<std::string>>::iterator a_it;
	for (a_it = alias_sets.begin(); a_it != alias_sets.end(); a_it++) {
	    std::set<std::string>::iterator s_it;
	    bu_log("Found set: ");
	    for (s_it = a_it->second.begin(); s_it != a_it->second.end(); s_it++) {
		bu_log("%s ", s_it->c_str());
	    }
	    bu_log("\n");
	}
    }

    ged_close(gbp);
    return 0;
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

