/*                     B O T _ D U M P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/bot_dump.cpp
 *
 * The bot_dump command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"
#include "bnetwork.h"

#include "bu/app.h" // bu_mkdir
#include "bu/cv.h"
#include "bu/getopt.h"
#include "bu/units.h"
#include "bu/path.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"

#include "raytrace.h"
#include "wdb.h"

#include "brlcad_version.h"

#include "raytrace.h"

#include "bv/defines.h"
#include "dm.h"

#include "../../ged_private.h"
#include "../ged_bot.h"
#include "./ged_bot_dump.h"

void
_ged_bot_dump(struct _ged_bot_dump_client_data *d, struct directory *dp, const struct db_full_path *pathp, struct rt_bot_internal *bot)
{
    const char *db_g_name = d->gedp->dbip->dbi_filename;
    FILE *fp = d->fp;
    int fd = d->fd;
    const char *file_ext = d->file_ext;

    if (bu_vls_strlen(&d->output_directory)) {
	char *cp;
	struct bu_vls file_name = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&file_name, bu_vls_cstr(&d->output_directory));
	bu_vls_putc(&file_name, '/');
	cp = dp->d_namep;
	while (*cp != '\0') {
	    if (*cp == '/') {
		bu_vls_putc(&file_name, '@');
	    } else if (*cp == '.' || isspace((int)*cp)) {
		bu_vls_putc(&file_name, '_');
	    } else {
		bu_vls_putc(&file_name, *cp);
	    }
	    cp++;
	}
	bu_vls_strcat(&file_name, file_ext);

	bu_mkdir(bu_vls_cstr(&d->output_directory));

	switch (d->output_type) {
	    case OTYPE_GLB:
	    case OTYPE_GLTF:
		if (gltf_setup(d, bu_vls_cstr(&file_name)) != BRLCAD_OK)
		    return;
		gltf_write_bot(d, bot, dp->d_namep);
		gltf_finish(d);
		break;
	    case OTYPE_STL:
		if (stl_setup(d, bu_vls_cstr(&file_name)) != BRLCAD_OK)
		    return;

		if (d->binary) {
		    fd = d->fd;
		    stl_write_bot_binary(d, bot, fd, dp->d_namep);
		} else {
		    fp = d->fp;
		    stl_write_bot(d, bot, fp, dp->d_namep);
		}

		stl_finish(d);
		break;
	    case OTYPE_DXF:
		if (dxf_setup(d, bu_vls_cstr(&file_name), dp->d_namep, db_g_name) != BRLCAD_OK)
		    return;

		fp = d->fp;
		dxf_write_bot(d, bot, fp, dp->d_namep);
		dxf_finish(d);
		break;
	    case OTYPE_OBJ:
		if (obj_setup(d, bu_vls_cstr(&file_name), 0) != BRLCAD_OK)
		    return;

		d->obj.v_offset = 1;

		fprintf(d->fp, "mtllib %s\n", bu_vls_addr(&d->obj.obj_materials_file));
		fp = d->fp;
		if (!pathp) {
		    obj_write_bot(d, bot, d->fp, dp->d_namep);
		} else {
		    char *pathstr = db_path_to_string(pathp);
		    obj_write_bot(d, bot, d->fp, pathstr);
		    bu_free(pathstr, "free path");
		}

		obj_finish(d);

		break;
	    case OTYPE_SAT:
		d->sat.curr_line_num = 0;
		if (sat_setup(d, bu_vls_cstr(&file_name)) != BRLCAD_OK)
		    return;
		fp = d->fp;
		sat_write_bot(d, bot, fp, dp->d_namep);
		sat_finish(d);
		break;
	    default:
		bu_log("Unsupported output type!\n");
		bu_vls_free(&file_name);
		return;
	}

	bu_vls_free(&file_name);

    } else {

	switch (d->output_type) {
	    case OTYPE_GLB:
	    case OTYPE_GLTF:
		gltf_write_bot(d, bot, dp->d_namep);
		break;
	    case OTYPE_STL:
		if (d->binary) {
		    d->total_faces += bot->num_faces;
		    stl_write_bot_binary(d, bot, fd, dp->d_namep);
		} else {
		    /* Need fp - check for it */
		    if (!fp) {
			bu_log("_ged_bot_dump: non-binay file requested but fp is NULL!\n");
			return;
		    }
		    stl_write_bot(d, bot, fp, dp->d_namep);
		}
		break;
	    case OTYPE_DXF:
		/* Need fp - check for it */
		if (!fp) {
		    bu_log("_ged_bot_dump: non-binay file requested but fp is NULL!\n");
		    return;
		}
		dxf_write_bot(d, bot, fp, dp->d_namep);
		break;
	    case OTYPE_OBJ:
		/* Need fp - check for it */
		if (!fp) {
		    bu_log("_ged_bot_dump: non-binay file requested but fp is NULL!\n");
		    return;
		}
		if (!pathp) {
		    obj_write_bot(d, bot, fp, dp->d_namep);
		} else {
		    char *pathstr = db_path_to_string(pathp);
		    obj_write_bot(d, bot, fp, pathstr);
		    bu_free(pathstr, "free path");
		}
		break;
	    case OTYPE_SAT:
		/* Need fp - check for it */
		if (!fp) {
		    bu_log("_ged_bot_dump: non-binay file requested but fp is NULL!\n");
		    return;
		}
		sat_write_bot(d, bot, fp, dp->d_namep);
		break;
	    default:
		/* Need fp - check for it */
		if (!fp) {
		    bu_log("_ged_bot_dump: non-binay file requested but fp is NULL!\n");
		    return;
		}
		stl_write_bot(d, bot, fp, dp->d_namep);
		break;
	}
    }
}


static union tree *
bot_dump_leaf(struct db_tree_state *UNUSED(tsp),
	const struct db_full_path *pathp,
	struct rt_db_internal *UNUSED(ip),
	void *client_data)
{
    int ret;
    union tree *curtree;
    mat_t mat;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct _ged_bot_dump_client_data *gbdcdp = (struct _ged_bot_dump_client_data *)client_data;

    /* Indicate success by returning something other than TREE_NULL */
    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;

    dp = pathp->fp_names[pathp->fp_len-1];

    /* we only dump BOT primitives, so skip some obvious exceptions */
    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD || dp->d_flags & RT_DIR_COMB)
	return curtree;

    MAT_IDN(mat);

    /* get the internal form */
    ret = rt_db_get_internal(&intern, dp, gbdcdp->gedp->dbip, mat, &rt_uniresource);

    if (ret < 0) {
	bu_log("ged_bot_leaf: rt_get_internal failure %d on %s\n", ret, dp->d_namep);
	return curtree;
    }

    if (ret != ID_BOT) {
	bu_log("ged_bot_leaf: %s is not a bot (ignored)\n", dp->d_namep);
	rt_db_free_internal(&intern);
	return curtree;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    _ged_bot_dump(gbdcdp, dp, pathp, bot);
    rt_db_free_internal(&intern);

    return curtree;
}

static char usage[] = "bot dump [-b] [-n] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] [bot1 bot2 ...]\n\n";

enum otype
bot_fmt_type(const char *t)
{
    if (!t)
	return OTYPE_UNSET;

    if (BU_STR_EQUAL("dxf", t))
	return OTYPE_DXF;
    if (BU_STR_EQUAL("glb", t))
	return OTYPE_GLB;
    if (BU_STR_EQUAL("gltf", t))
	return OTYPE_GLTF;
    if (BU_STR_EQUAL("obj", t))
	return OTYPE_OBJ;
    if (BU_STR_EQUAL("sat", t))
	return OTYPE_SAT;
    if (BU_STR_EQUAL("stl", t))
	return OTYPE_STL;

    return OTYPE_UNSET;
}

const char *
bot_fmt_ext(enum otype o, int UNUSED(binary))
{
    static const char *edxf  = ".dxf";
    static const char *eglb  = ".glb";
    static const char *egltf = ".gltf";
    static const char *eobj  = ".obj";
    static const char *esat  = ".sat";
    static const char *estl  = ".stl";

    if (o == OTYPE_DXF)
	return edxf;
    if (o == OTYPE_GLB)
	return eglb;
    if (o == OTYPE_GLTF)
	return egltf;
    if (o == OTYPE_OBJ)
	return eobj;
    if (o == OTYPE_SAT)
	return esat;
    if (o == OTYPE_STL)
	return estl;

    return NULL;
}

static int
bot_opt_fmt(struct bu_vls *UNUSED(msg), size_t argc, const char **argv, void *set_var)
{
    enum otype *otype = (enum otype *)set_var;
    if (!otype)
	return 0;

    if (!argc) {
	(*otype) = OTYPE_UNSET;
	return 0;
    }

    (*otype) = bot_fmt_type(argv[0]);

    if ((*otype) == OTYPE_UNSET)
	return -1;

    return 1;
}

static int
bot_opt_unit(struct bu_vls *UNUSED(msg), size_t argc, const char **argv, void *set_var)
{
    double *cfactor = (double *)set_var;
    if (!cfactor)
	return 0;

    if (!argc) {
	(*cfactor) = 1.0;
	return 0;
    }

    (*cfactor) = bu_units_conversion(argv[0]);
    if (ZERO(*cfactor)) {
	(*cfactor) = 1.0;
    } else {
	(*cfactor) = 1.0 / (*cfactor);
    }

    return 1;
}

// TODO - right now this is not at all general, and in fact will only write out a few
// Tcl specific data containers.  Needs to be rethought.  Probably should be revisited
// after we switch to the new drawing path, which uses bview scene objects - that will
// likely make writing out a scene simpler overall.
static int
viewdata_dump(struct _ged_bot_dump_client_data *d, struct ged *gedp, FILE *fp)
{
    switch (d->output_type) {
	case OTYPE_DXF:
	    break;
	case OTYPE_OBJ:
	    if (bu_vls_strlen(&d->output_directory)) {
		char *odir = bu_strdup(bu_vls_cstr(&d->output_directory));
		char *cp;
		struct bu_vls filepath = BU_VLS_INIT_ZERO;
		FILE *data_fp;

		cp = strrchr(odir, '/');
		if (!cp)
		    cp = odir;
		else
		    ++cp;

		if (*cp == '\0') {
		    bu_vls_printf(gedp->ged_result_str, "viewdata_dump: bad dirname - %s\n", bu_vls_cstr(&d->output_directory));
		    bu_free(odir, "odir");
		    return BRLCAD_ERROR;
		}

		bu_vls_printf(&filepath, "%s/%s_data.obj", bu_vls_cstr(&d->output_directory), cp);

		data_fp = fopen(bu_vls_addr(&filepath), "wb+");
		if (data_fp == NULL) {
		    bu_vls_printf(gedp->ged_result_str, "viewdata_dump: failed to open %s\n", bu_vls_addr(&filepath));
		    bu_vls_free(&filepath);
		    bu_free(odir, "odir");
		    return BRLCAD_ERROR;
		}

		bu_free(odir, "odir");
		bu_vls_free(&filepath);
		obj_write_data(d, gedp, data_fp);
		fclose(data_fp);
	    } else
		if (fp) {
		    obj_write_data(d, gedp, fp);
		} else {
		    bu_vls_printf(gedp->ged_result_str, "viewdata_dump: bad FILE fp\n");
		    return BRLCAD_ERROR;
		}

	    break;
	case OTYPE_SAT:
	    break;
	case OTYPE_STL:
	default:
	    break;
    }

    return BRLCAD_OK;
}

static void
dl_botdump(struct _ged_bot_dump_client_data *d)
{
    struct bu_list *hdlp = d->gedp->ged_gdp->gd_headDisplay;
    struct db_i *dbip = d->gedp->dbip;
    int ret;
    mat_t mat;
    struct display_list *gdlp;

    MAT_IDN(mat);

    for (BU_LIST_FOR(gdlp, display_list, hdlp)) {
	struct bv_scene_obj *sp;

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    struct directory *dp;
	    struct rt_db_internal intern;
	    struct rt_bot_internal *bot;

	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    dp = bdata->s_fullpath.fp_names[bdata->s_fullpath.fp_len-1];

	    /* get the internal form */
	    ret = rt_db_get_internal(&intern, dp, dbip, mat, &rt_uniresource);

	    if (ret < 0) {
		bu_log("rt_get_internal failure %d on %s\n", ret, dp->d_namep);
		continue;
	    }

	    if (ret != ID_BOT) {
		bu_log("%s is not a bot (ignored)\n", dp->d_namep);
		rt_db_free_internal(&intern);
		continue;
	    }

	    /* Write out object color */
	    if (d->output_type == OTYPE_OBJ) {
		d->obj.curr_obj_red = sp->s_color[0];
		d->obj.curr_obj_green = sp->s_color[1];
		d->obj.curr_obj_blue = sp->s_color[2];
		d->obj.curr_obj_alpha = sp->s_os->transparency;
	    }

	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    _ged_bot_dump(d, dp, NULL, bot);
	    rt_db_free_internal(&intern);
	}
    }

}

void
bot_client_data_init(struct _ged_bot_dump_client_data *d)
{
    if (!d)
	return;
    d->gedp = NULL;
    d->binary = 0;
    d->cfactor = 1.0;
    d->full_precision = 0;
    d->material_info = 0;
    d->normals = 0;
    d->output_type = OTYPE_UNSET;
    d->total_faces = 0;
    d->using_dbot_dump = 0;
    d->view_data = 0;
    bu_vls_init(&d->output_file);
    bu_vls_init(&d->output_directory);
}

void
bot_client_data_cleanup(struct _ged_bot_dump_client_data *d)
{
    if (!d)
	return;
    bu_vls_free(&d->output_file);
    bu_vls_free(&d->output_directory);
}

extern "C" int
ged_bot_dump_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    int i;
    int print_help = 0;
    int write_displayed = 0;
    const char *cmd_name;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    struct _ged_bot_dump_client_data ld;
    struct _ged_bot_dump_client_data *d = &ld;
    bot_client_data_init(d);
    d->gedp = gedp;

    /* save the command name and skip */
    cmd_name = argv[0];
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc od[13];
    BU_OPT(od[ 0], "h", "help",             "",      NULL,          &print_help,          "Print help and exit");
    BU_OPT(od[ 1], "?", "",                 "",      NULL,          &print_help,          "");
    BU_OPT(od[ 2], "b", "",                 "",      NULL,          &d->binary,           "Use binary version of output format");
    BU_OPT(od[ 3], "n", "normals",          "",      NULL,          &d->normals,          "If supported, write out normals");
    BU_OPT(od[ 4], "m", "output-directory", "dir",   &bu_opt_vls,   &d->output_directory, "Output multiple files into this directory");
    BU_OPT(od[ 5], "o", "output-file",      "file",  &bu_opt_vls,   &d->output_file,      "Specify an output filename");
    BU_OPT(od[ 6], "t", "",                 "fmt",   &bot_opt_fmt,  &d->output_type,      "Specify an output format type");
    BU_OPT(od[ 7], "u", "",                 "unit",  &bot_opt_unit, &d->cfactor,          "Specify an output unit");
    BU_OPT(od[ 8], "F", "full-precision",   "",      NULL,          &d->full_precision,   "Write full floating point precision (non-standard for glTF).");
    // TODO - use these options to fold dbot variations into the core function, and then
    // rework dbot version to just construct a new argc/argv array and call this.
    BU_OPT(od[ 9], "",  "displayed",        "",      NULL,          &write_displayed,     "Write out displayed geometry");
    BU_OPT(od[10], "",  "viewdata",         "",      NULL,          &d->view_data,        "Write out non-geometry view data");
    BU_OPT(od[11], "",  "materials",        "",      NULL,          &d->material_info,    "Write out material info");
    BU_OPT_NULL(od[12]);

    /* must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, od);
	bot_client_data_cleanup(d);
	return GED_HELP;
    }

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, od);

    if (print_help) {
	_ged_cmd_help(gedp, usage, od);
	bot_client_data_cleanup(d);
	return BRLCAD_OK;
    }

    argc = opt_ret;

    if (bu_vls_strlen(&d->output_file) && bu_vls_strlen(&d->output_directory)) {
	fprintf(stderr, "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n");
	bot_client_data_cleanup(d);
	return BRLCAD_ERROR;
    }

    if (d->view_data || write_displayed) {
	GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
	GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    }

    if (!bu_vls_strlen(&d->output_file) && !bu_vls_strlen(&d->output_directory)) {
	d->fp = stdout;
	bu_vls_sprintf(&d->output_file, "stdout");
    }

    // If we don't have an explicit format, see if we can deduce it from the
    // filename.  If not, go with STL.
    if (d->output_type == OTYPE_UNSET) {
	if (bu_vls_strlen(&d->output_file)) {
	    struct bu_vls ext = BU_VLS_INIT_ZERO;
	    if (bu_path_component(&ext, bu_vls_cstr(&d->output_file), BU_PATH_EXT))
		d->output_type = bot_fmt_type(bu_vls_cstr(&ext));
	    bu_vls_free(&ext);
	}
	if (d->output_type == OTYPE_UNSET) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING: no format type '-t' specified, defaulting to stl\n");
	    d->output_type = OTYPE_STL;
	}
    }

    // GLB files are inherently binary
    if (d->output_type == OTYPE_GLB)
	d->binary = 1;

    // Sanity
    if (d->binary && d->fp == stdout) {
	bu_vls_printf(gedp->ged_result_str, "Can't output binary format to stdout\n");
	bot_client_data_cleanup(d);
	return BRLCAD_ERROR;
    }

    // Type is set - assign extension
    d->file_ext = bot_fmt_ext(d->output_type, d->binary);

    if (bu_vls_strlen(&d->output_file)) {

	switch (d->output_type) {
	    case OTYPE_DXF:
		if (dxf_setup(d, bu_vls_cstr(&d->output_file), argv[argc-1], NULL) != BRLCAD_OK) {
		    bot_client_data_cleanup(d);
		    return BRLCAD_ERROR;
		}
		break;
	    case OTYPE_GLB:
	    case OTYPE_GLTF:
		if (gltf_setup(d, bu_vls_cstr(&d->output_file)) != BRLCAD_OK) {
		    bot_client_data_cleanup(d);
		    return BRLCAD_ERROR;
		}
		break;
	    case OTYPE_OBJ:
		if (obj_setup(d, bu_vls_cstr(&d->output_file), 0) != BRLCAD_OK) {
		    bot_client_data_cleanup(d);
		    return BRLCAD_ERROR;
		}
		break;
	    case OTYPE_SAT:
		if (sat_setup(d, bu_vls_cstr(&d->output_file)) != BRLCAD_OK) {
		    bot_client_data_cleanup(d);
		    return BRLCAD_ERROR;
		}
		break;
	    case OTYPE_STL:
		if (stl_setup(d, bu_vls_cstr(&d->output_file)) != BRLCAD_OK) {
		    bot_client_data_cleanup(d);
		    return BRLCAD_ERROR;
		}
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Error: don't know how to set up output file for type %s\n", d->file_ext);
		bot_client_data_cleanup(d);
		return BRLCAD_ERROR;
	}
    }


    if (bu_vls_strlen(&d->output_directory)) {
	switch (d->output_type) {
	    case OTYPE_OBJ:
		if (obj_setup(d, bu_vls_cstr(&d->output_file), 1) != BRLCAD_OK) {
		    bot_client_data_cleanup(d);
		    return BRLCAD_ERROR;
		}
	    default:
		break;
	}
    }


    if (argc < 1 && !write_displayed) {
	mat_t mat;
	MAT_IDN(mat);
	/* dump all the bots */
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {

	    /* we only dump BOT primitives, so skip some obvious exceptions */
	    if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
	    if (dp->d_flags & RT_DIR_COMB) continue;

	    /* get the internal form */
	    i = rt_db_get_internal(&intern, dp, gedp->dbip, mat, &rt_uniresource);
	    if (i < 0) {
		fprintf(stderr, "%s: rt_get_internal failure %d on %s\n", cmd_name, i, dp->d_namep);
		continue;
	    }

	    if (i != ID_BOT) {
		continue;
	    }

	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    _ged_bot_dump(d, dp, NULL, bot);
	    rt_db_free_internal(&intern);

	} FOR_ALL_DIRECTORY_END;
    } else if (write_displayed) {
	dl_botdump(d);
    } else {
	int ac = 1;
	int ncpu = 1;
	char *av[2];
	av[1] = (char *)0;

	struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	for (i = 0; i < argc; ++i) {
	    av[0] = (char *)argv[i];
	    ret = db_walk_tree(gedp->dbip,
		    ac,
		    (const char **)av,
		    ncpu,
		    &wdbp->wdb_initial_tree_state,
		    0,
		    0,
		    bot_dump_leaf,
		    (void *)d);
	    if (ret < 0) {
		perror("db_walk_tree");
	    }
	}
    }

    if (d->view_data && d->fp) {
	viewdata_dump(d, gedp, d->fp);
    }

    if (!bu_vls_strlen(&d->output_directory)) {

	switch (d->output_type) {
	    case OTYPE_DXF:
		dxf_finish(d);
		break;
	    case OTYPE_GLB:
	    case OTYPE_GLTF:
		gltf_finish(d);
		break;
	    case OTYPE_OBJ:
		obj_finish(d);
		break;
	    case OTYPE_SAT:
		sat_finish(d);
		break;
	    case OTYPE_STL:
		stl_finish(d);
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Error - don't know how to finalize file!\n");
		bot_client_data_cleanup(d);
		return BRLCAD_ERROR;
	}
    }

    bot_client_data_cleanup(d);
    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_dump(void* bs, int argc, const char** argv)
{
    struct _ged_bot_info* gb = (struct _ged_bot_info*)bs;
    struct ged* gedp = gb->gedp;
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    const char *usage_string = "bot dump [-b] [-n] [-m directory] [-o file] [-t dxf|glb|gltf|obj|sat|stl] [-u units] [bot1 bot2 ...]\n\n";
    const char* purpose_string = "Export raw BoT information, without any processing.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_bot_dump_core(gedp, argc, argv);
}

extern "C" int
ged_dbot_dump_core(struct ged *gedp, int argc, const char *argv[])
{
    int ac = argc + 3;
    const char **av = (const char **)bu_calloc(ac, sizeof(char *), "av");
    av[0] = argv[0];
    av[1] = "--displayed";
    av[2] = "--viewdata";
    av[3] = "--materials";
    for (int i = 1; i < argc; i++)
	av[i+3] = argv[i];

    int ret = ged_bot_dump_core(gedp, ac, (const char **)av);

    bu_free(av, "av");

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
