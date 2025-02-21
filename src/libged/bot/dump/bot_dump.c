/*                         B O T _ D U M P . C
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
/** @file libged/bot_dump.c
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
#include "./ged_bot_dump.h"


static char usage[] = "[-b] [-n] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] [bot1 bot2 ...]";


/* Byte swaps a four byte value */
static void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}


void
_ged_bot_dump(struct _ged_bot_dump_client_data *d, struct directory *dp, const struct db_full_path *pathp, struct rt_bot_internal *bot)
{
    const char *db_g_name = d->gedp->dbip->dbi_filename;
    FILE *fp = d->fp;
    int fd = d->fd;
    const char *file_ext = d->file_ext;
    int ret;

    if (d->output_directory) {
	char *cp;
	struct bu_vls file_name = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&file_name, d->output_directory);
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


	switch (d->output_type) {
	    case OTYPE_STL:
		if (d->binary) {
		    char buf[81];	/* need exactly 80 chars for header */
		    unsigned char tot_buffer[4];

		    fd = open(bu_vls_addr(&file_name), O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		    if (fd < 0) {
			perror(bu_vls_addr(&file_name));
			bu_log("Cannot open binary output file (%s) for writing\n", bu_vls_addr(&file_name));
			bu_vls_free(&file_name);
			return;
		    }

		    /* Write out STL header */
		    memset(buf, 0, sizeof(buf));
		    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
		    ret = write(fd, &buf, 80);
		    if (ret < 0) {
			perror("write");
		    }

		    /* write a place keeper for the number of triangles */
		    memset(buf, 0, 4);
		    ret = write(fd, &buf, 4);
		    if (ret < 0) {
			perror("write");
		    }

		    stl_write_bot_binary(d, bot, fd, dp->d_namep);

		    /* Re-position pointer to 80th byte */
		    bu_lseek(fd, 80, SEEK_SET);

		    /* Write out number of triangles */
		    *(uint32_t *)tot_buffer = htonl((unsigned long)d->total_faces);
		    lswap((unsigned int *)tot_buffer);
		    ret = write(fd, tot_buffer, 4);
		    if (ret < 0) {
			perror("write");
		    }

		    close(fd);
		} else {
		    fp = fopen(bu_vls_addr(&file_name), "wb+");
		    if (fp == NULL) {
			perror(bu_vls_addr(&file_name));
			bu_log("Cannot open STL ASCII output file (%s) for writing\n", bu_vls_addr(&file_name));
			bu_vls_free(&file_name);
			return;
		    }
		    stl_write_bot(d, bot, fp, dp->d_namep);
		    fclose(fp);
		}
		break;

	    case OTYPE_DXF:
		fp = fopen(bu_vls_addr(&file_name), "wb+");
		if (fp == NULL) {
		    perror(bu_vls_addr(&file_name));
		    bu_log("Cannot open DXF output file (%s) for writing\n", bu_vls_addr(&file_name));
		    bu_vls_free(&file_name);
		    return;
		}
		fprintf(fp,
			"0\nSECTION\n2\nHEADER\n999\n%s (BOT from %s)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
			dp->d_namep, db_g_name);
		dxf_write_bot(d, bot, fp, dp->d_namep);
		fprintf(fp, "0\nENDSEC\n0\nEOF\n");
		fclose(fp);
		break;
	    case OTYPE_OBJ:
		fp = fopen(bu_vls_addr(&file_name), "wb+");
		if (fp == NULL) {
		    perror(bu_vls_addr(&file_name));
		    bu_log("Cannot open OBJ output file (%s) for writing\n", bu_vls_addr(&file_name));
		    bu_vls_free(&file_name);
		    return;
		}
		d->v_offset = 1;
		fprintf(fp, "mtllib %s\n", bu_vls_addr(&d->obj.obj_materials_file));
		if (!pathp) {
		    obj_write_bot(d, bot, fp, dp->d_namep);
		} else {
		    char *pathstr = db_path_to_string(pathp);
		    obj_write_bot(d, bot, fp, pathstr);
		    bu_free(pathstr, "free path");
		}
		fclose(fp);
		break;
	    case OTYPE_SAT:
		fp = fopen(bu_vls_addr(&file_name), "wb+");
		if (fp == NULL) {
		    perror(bu_vls_addr(&file_name));
		    bu_log("Cannot open SAT output file (%s) for writing\n", bu_vls_addr(&file_name));
		    bu_vls_free(&file_name);
		    return;
		}

		d->sat.curr_line_num = 0;

		sat_write_header(fp);

		sat_write_bot(d, bot, fp, dp->d_namep);
		fprintf(fp, "End-of-ACIS-data\n");
		fclose(fp);
		break;
	    default:
		fp = fopen(bu_vls_addr(&file_name), "wb+");
		if (fp == NULL) {
		    perror(bu_vls_addr(&file_name));
		    bu_log("Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(&file_name));
		    bu_vls_free(&file_name);
		    return;
		}
		stl_write_bot(d, bot, fp, dp->d_namep);
		fclose(fp);
		break;
	}

	bu_vls_free(&file_name);

    } else {
	/* If we get to this point and need fp - check for it */
	if (!d->binary && !fp) {
	    bu_log("_ged_bot_dump: non-binay file requested but fp is NULL!\n");
	    return;
	}
	switch (d->output_type) {
	    case OTYPE_STL:
		if (d->binary) {
		    d->total_faces += bot->num_faces;
		    stl_write_bot_binary(d, bot, fd, dp->d_namep);
		} else {
		    stl_write_bot(d, bot, fp, dp->d_namep);
		}
		break;
	    case OTYPE_DXF:
		dxf_write_bot(d, bot, fp, dp->d_namep);
		break;
	    case OTYPE_OBJ:
		if (!pathp) {
		    obj_write_bot(d, bot, fp, dp->d_namep);
		} else {
		    char *pathstr = db_path_to_string(pathp);
		    obj_write_bot(d, bot, fp, pathstr);
		    bu_free(pathstr, "free path");
		}
		break;
	    case OTYPE_SAT:
		sat_write_bot(d, bot, fp, dp->d_namep);
		break;
	    default:
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


static int
bot_dump_get_args(struct _ged_bot_dump_client_data *d, struct ged *gedp, int argc, const char *argv[])
{
    int c;
    int specified_t = 0;

    d->output_type = OTYPE_STL;
    d->binary = 0;
    d->normals = 0;
    d->cfactor = 1.0;
    d->output_file = NULL;
    d->output_directory = NULL;
    d->total_faces = 0;
    d->v_offset = 1;
    bu_optind = 1;

    d->sat.curr_line_num = 0;

    /* Get command line options. */
    while ((c = bu_getopt(argc, (char * const *)argv, "bno:m:t:u:")) != -1) {
	switch (c) {
	    case 'b':		/* Binary output file */
		d->binary=1;
		break;
	    case 'n':		/* Binary output file */
		d->normals=1;
		break;
	    case 'm':
		d->output_directory = bu_optarg;
		break;
	    case 'o':		/* Output file name. */
		d->output_file = bu_optarg;
		break;
	    case 't':
		if (BU_STR_EQUAL("dxf", bu_optarg))
		    d->output_type = OTYPE_DXF;
		else if (BU_STR_EQUAL("obj", bu_optarg))
		    d->output_type = OTYPE_OBJ;
		else if (BU_STR_EQUAL("sat", bu_optarg))
		    d->output_type = OTYPE_SAT;
		else if (BU_STR_EQUAL("stl", bu_optarg))
		    d->output_type = OTYPE_STL;
		else {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
		    return BRLCAD_ERROR;
		}
		specified_t = 1;
		break;
	    case 'u':
		d->cfactor = bu_units_conversion(bu_optarg);
		if (ZERO(d->cfactor))
		    d->cfactor = 1.0;
		else
		    d->cfactor = 1.0 / d->cfactor;

		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
		return BRLCAD_ERROR;
	}
    }

    /* try to be smart with the specified file extension, but warn
     * when we default to stl
     */
    if (!specified_t) {
	struct bu_vls ext = BU_VLS_INIT_ZERO;
	if (!bu_path_component(&ext, d->output_file, BU_PATH_EXT)) {
	    bu_vls_printf(gedp->ged_result_str,
			  "WARNING: no format type '-t' specified, defaulting to stl\n");
	} else {
	    if (BU_STR_EQUAL("dxf", bu_vls_cstr(&ext)))
		d->output_type = OTYPE_DXF;
	    else if (BU_STR_EQUAL("obj", bu_vls_cstr(&ext)))
		d->output_type = OTYPE_OBJ;
	    else if (BU_STR_EQUAL("sat", bu_vls_cstr(&ext)))
		d->output_type = OTYPE_SAT;
	    else if (BU_STR_EQUAL("stl", bu_vls_cstr(&ext)))
		d->output_type = OTYPE_STL;
	    else {
		bu_vls_printf(gedp->ged_result_str,
			      "WARNING: unrecognized filename type, defaulting to stl\n");
	    }
	}
    }

    return BRLCAD_OK;
}


int
ged_bot_dump_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    char *file_ext = NULL;
    FILE *fp = (FILE *)0;
    int fd = -1;
    mat_t mat;
    int i;
    const char *cmd_name;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    struct _ged_bot_dump_client_data ld;
    struct _ged_bot_dump_client_data *d = &ld;
    d->gedp = gedp;
    d->using_dbot_dump = 0;

    /* save the command name */
    cmd_name = argv[0];

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", cmd_name, usage);
	return GED_HELP;
    }

    d->using_dbot_dump = 0;

    if (bot_dump_get_args(d, gedp, argc, argv) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    if (bu_optind > argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_ERROR;
    }

    if (d->output_file && d->output_directory) {
	fprintf(stderr, "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n");
	return BRLCAD_ERROR;
    }

    if (!d->output_file && !d->output_directory) {
	if (d->binary) {
	    bu_vls_printf(gedp->ged_result_str, "Can't output binary to stdout\nUsage: %s %s\n", argv[0], usage);
	    return BRLCAD_ERROR;
	}
	fp = stdout;

	/* Set this to something non-null in order to possibly write eof */
	d->output_file = "stdout";
    }


    if (d->output_file) {

	switch (d->output_type) {
	    case OTYPE_STL:
		if (d->binary) {
		    char buf[81];	/* need exactly 80 chars for header */

		    /* Open binary output file */
		    fd = open(d->output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		    if (fd < 0) {
			perror(cmd_name);
			bu_vls_printf(gedp->ged_result_str, "Cannot open binary output file (%s) for writing\n", d->output_file);
			return BRLCAD_ERROR;
		    }

		    /* Write out STL header if output file is binary */
		    memset(buf, 0, sizeof(buf));
		    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
		    ret = write(fd, &buf, 80);
		    if (ret < 0) {
			perror("write");
		    }

		    /* write a place keeper for the number of triangles */
		    memset(buf, 0, 4);
		    ret = write(fd, &buf, 4);
		    if (ret < 0) {
			perror("write");
		    }
		} else {
		    fp = fopen(d->output_file, "wb+");
		    if (fp == NULL) {
			perror(argv[0]);
			bu_vls_printf(gedp->ged_result_str, "Cannot open STL ascii output file (%s) for writing\n", d->output_file);
			return BRLCAD_ERROR;
		    }
		}
		break;
	    case OTYPE_DXF:
		fp = fopen(d->output_file, "wb+");
		if (fp == NULL) {
		    perror(cmd_name);
		    bu_vls_printf(gedp->ged_result_str, "Cannot open DXF ascii output file (%s) for writing\n", d->output_file);
		    return BRLCAD_ERROR;
		}

		/* output DXF header and start of TABLES section */
		fprintf(fp,
			"0\nSECTION\n2\nHEADER\n999\n%s (All Bots)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
			argv[argc-1]);
		break;
	    case OTYPE_SAT:
		fp = fopen(d->output_file, "wb+");
		if (fp == NULL) {
		    perror(cmd_name);
		    bu_vls_printf(gedp->ged_result_str, "Cannot open SAT ascii output file (%s) for writing\n", d->output_file);
		    return BRLCAD_ERROR;
		}

		sat_write_header(fp);
		break;
	    default:
		fp = fopen(d->output_file, "wb+");
		if (fp == NULL) {
		    perror(cmd_name);
		    bu_vls_printf(gedp->ged_result_str, "Cannot open ascii output file (%s) for writing\n", d->output_file);
		    return BRLCAD_ERROR;
		}
		break;
	}
    }

    if (d->output_directory) {
	switch (d->output_type) {
	    case OTYPE_DXF:
		file_ext = ".dxf";
		break;
	    case OTYPE_OBJ:
		file_ext = ".obj";
		break;
	    case OTYPE_SAT:
		file_ext = ".sat";
		break;
	    case OTYPE_STL:
	    default:
		file_ext = ".stl";
		break;
	}
    }


    /* skip past the command name and optional args */
    argc -= bu_optind;
    argv += bu_optind;


    MAT_IDN(mat);

    d->file_ext = file_ext;
    d->fp = fp;
    d->fd = fd;

    if (argc < 1) {
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


    if (d->output_file) {

	switch (d->output_type) {
	    case OTYPE_STL:
		if (d->binary) {
		    unsigned char tot_buffer[4];

		    /* Re-position pointer to 80th byte */
		    bu_lseek(fd, 80, SEEK_SET);

		    /* Write out number of triangles */
		    *(uint32_t *)tot_buffer = htonl((unsigned long)d->total_faces);
		    lswap((unsigned int *)tot_buffer);
		    ret = write(fd, tot_buffer, 4);
		    if (ret < 0) {
			perror("write");
		    }

		    close(fd);
		}
		break;
	    case OTYPE_DXF:
		fprintf(fp, "0\nENDSEC\n0\nEOF\n");
		fclose(fp);
		break;
	    case OTYPE_SAT:
		fprintf(fp, "End-of-ACIS-data\n");
		fclose(fp);
		break;
	    default:
		fclose(fp);
		break;
	}
    }

    return BRLCAD_OK;
}

static int
data_dump(struct _ged_bot_dump_client_data *d, struct ged *gedp, FILE *fp)
{
    switch (d->output_type) {
	case OTYPE_DXF:
	    break;
	case OTYPE_OBJ:
	    if (d->output_directory) {
		char *cp;
		struct bu_vls filepath = BU_VLS_INIT_ZERO;
		FILE *data_fp;

		cp = strrchr(d->output_directory, '/');
		if (!cp)
		    cp = (char *)d->output_directory;
		else
		    ++cp;

		if (*cp == '\0') {
		    bu_vls_printf(gedp->ged_result_str, "data_dump: bad dirname - %s\n", d->output_directory);
		    return BRLCAD_ERROR;
		}

		bu_vls_printf(&filepath, "%s/%s_data.obj", d->output_directory, cp);

		data_fp = fopen(bu_vls_addr(&filepath), "wb+");
		if (data_fp == NULL) {
		    bu_vls_printf(gedp->ged_result_str, "data_dump: failed to open %s\n", bu_vls_addr(&filepath));
		    bu_vls_free(&filepath);
		    return BRLCAD_ERROR;
		}

		bu_vls_free(&filepath);
		obj_write_data(d, gedp, data_fp);
		fclose(data_fp);
	    } else
		if (fp) {
		  obj_write_data(d, gedp, fp);
		} else {
		  bu_vls_printf(gedp->ged_result_str, "data_dump: bad FILE fp\n");
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


int
ged_dbot_dump_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char *file_ext = NULL;
    FILE *fp = (FILE *)0;
    int fd = -1;

    /* save the command name */
    const char *cmd_name = argv[0];


    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, usage, cmd_name);
	return GED_HELP;
    }

    struct _ged_bot_dump_client_data ld;
    struct _ged_bot_dump_client_data *d = &ld;
    d->gedp = gedp;
    d->using_dbot_dump = 0;

    if (bot_dump_get_args(d, gedp, argc, argv) == BRLCAD_ERROR)
	return BRLCAD_ERROR;

    if (bu_optind != argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_ERROR;
    }

    if (d->output_file && d->output_directory) {
	fprintf(stderr, "ERROR: options \"-o\" and \"-m\" are mutually exclusive\n");
	return BRLCAD_ERROR;
    }

    if (!d->output_file && !d->output_directory) {
	if (d->binary) {
	    bu_vls_printf(gedp->ged_result_str, "Can't output binary to stdout\nUsage: %s %s", cmd_name, usage);
	    return BRLCAD_ERROR;
	}
	fp = stdout;

	/* Set this to something non-null in order to possibly write eof */
	d->output_file = "stdout";
    }

    if (d->output_file) {
	switch (d->output_type) {
	    case OTYPE_STL:
		if (d->binary) {
		    char buf[81];	/* need exactly 80 chars for header */

		    /* Open binary output file */
		    fd = open(d->output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		    if (fd < 0) {
			perror(cmd_name);
			bu_vls_printf(gedp->ged_result_str, "Cannot open binary output file (%s) for writing\n", d->output_file);
			return BRLCAD_ERROR;
		    }

		    /* Write out STL header if output file is binary */
		    memset(buf, 0, sizeof(buf));
		    bu_strlcpy(buf, "BRL-CAD generated STL FILE", sizeof(buf));
		    ret = write(fd, &buf, 80);
		    if (ret < 0) {
			perror("write");
		    }

		    /* write a place keeper for the number of triangles */
		    memset(buf, 0, 4);
		    ret = write(fd, &buf, 4);
		    if (ret < 0) {
			perror("write");
		    }
		} else {
		    fp = fopen(d->output_file, "wb+");
		    if (fp == NULL) {
			perror(argv[0]);
			bu_vls_printf(gedp->ged_result_str, "Cannot open STL ascii output file (%s) for writing\n", d->output_file);
			return BRLCAD_ERROR;
		    }
		}
		break;
	    case OTYPE_DXF:
		/* Open ASCII output file */
		fp = fopen(d->output_file, "wb+");
		if (fp == NULL) {
		    perror(argv[0]);
		    bu_vls_printf(gedp->ged_result_str, "Cannot open DXF output file (%s) for writing\n", d->output_file);
		    return BRLCAD_ERROR;
		}

		/* output DXF header and start of TABLES section */
		fprintf(fp,
			"0\nSECTION\n2\nHEADER\n999\n%s (All Bots)\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n",
			argv[argc-1]);
		break;
	    case OTYPE_OBJ:
		{
		    fp = fopen(d->output_file, "wb+");
		    if (fp == NULL) {
			perror(argv[0]);
			bu_vls_printf(gedp->ged_result_str, "Cannot open SAT output file (%s) for writing\n", d->output_file);
			return BRLCAD_ERROR;
		    }

		    char *cp;

		    bu_vls_trunc(&d->obj.obj_materials_file, 0);

		    cp = strrchr(d->output_file, '.');
		    if (!cp)
			bu_vls_printf(&d->obj.obj_materials_file, "%s.mtl", d->output_file);
		    else {
			/* ignore everything after the last '.' */
			*cp = '\0';
			bu_vls_printf(&d->obj.obj_materials_file, "%s.mtl", d->output_file);
			*cp = '.';
		    }

		    BU_LIST_INIT(&d->obj.HeadObjMaterials);

		    d->obj.obj_materials_fp = fopen(bu_vls_cstr(&d->obj.obj_materials_file), "wb+");
		    if (d->obj.obj_materials_fp == NULL) {
			bu_vls_printf(gedp->ged_result_str, "%s: failed to open %s\n", cmd_name, bu_vls_cstr(&d->obj.obj_materials_file));
			bu_vls_free(&d->obj.obj_materials_file);
			fclose(fp);
			return BRLCAD_ERROR;
		    }

		    d->obj.num_obj_materials = 0;

		    fprintf(fp, "mtllib %s\n", bu_vls_cstr(&d->obj.obj_materials_file));
		}
		break;
	    case OTYPE_SAT:
		fp = fopen(d->output_file, "wb+");
		if (fp == NULL) {
		    perror(argv[0]);
		    bu_vls_printf(gedp->ged_result_str, "Cannot open SAT output file (%s) for writing\n", d->output_file);
		    return BRLCAD_ERROR;
		}

		sat_write_header(fp);
		break;
	    default:
		/* Open ASCII output file */
		fp = fopen(d->output_file, "wb+");
		if (fp == NULL) {
		    perror(argv[0]);
		    bu_vls_printf(gedp->ged_result_str, "Cannot open ascii output file (%s) for writing\n", d->output_file);
		    return BRLCAD_ERROR;
		}
		break;
	}
    }

    if (d->output_directory) {
	switch (d->output_type) {
	    case OTYPE_DXF:
		d->file_ext = ".dxf";
		break;
	    case OTYPE_OBJ:
		d->file_ext = ".obj";

		BU_LIST_INIT(&d->obj.HeadObjMaterials);

		{
		    char *cp;
		    struct bu_vls filepath = BU_VLS_INIT_ZERO;

		    cp = strrchr(d->output_directory, '/');
		    if (!cp)
			cp = (char *)d->output_directory;
		    else
			++cp;

		    if (*cp == '\0') {
			bu_vls_printf(gedp->ged_result_str, "%s: bad dirname - %s\n", cmd_name, d->output_directory);
			return BRLCAD_ERROR;
		    }

		    bu_vls_trunc(&d->obj.obj_materials_file, 0);
		    bu_vls_printf(&d->obj.obj_materials_file, "%s.mtl", cp);

		    bu_vls_printf(&filepath, "%s/%s", d->output_directory, bu_vls_cstr(&d->obj.obj_materials_file));


		    d->obj.obj_materials_fp = fopen(bu_vls_cstr(&filepath), "wb+");
		    if (d->obj.obj_materials_fp == NULL) {
			bu_vls_printf(gedp->ged_result_str, "%s: failed to open %s\n", cmd_name, bu_vls_cstr(&filepath));
			bu_vls_free(&d->obj.obj_materials_file);
			bu_vls_free(&filepath);
			return BRLCAD_ERROR;
		    }

		    bu_vls_free(&filepath);
		}

		d->obj.num_obj_materials = 0;

		break;
	    case OTYPE_SAT:
		d->file_ext = ".sat";
		break;
	    case OTYPE_STL:
	    default:
		d->file_ext = ".stl";
		break;
	}
    }

    d->fp = fp;
    d->fd = fd;
    d->file_ext = file_ext;

    dl_botdump(d);

    data_dump(d, gedp, fp);

    if (d->output_file) {
	switch (d->output_type) {
	    case OTYPE_STL:
		if (d->binary) {
		    unsigned char tot_buffer[4];

		    /* Re-position pointer to 80th byte */
		    bu_lseek(fd, 80, SEEK_SET);

		    /* Write out number of triangles */
		    *(uint32_t *)tot_buffer = htonl((unsigned long)d->total_faces);
		    lswap((unsigned int *)tot_buffer);
		    ret = write(fd, tot_buffer, 4);
		    if (ret < 0) {
			perror("write");
		    }

		    close(fd);
		}
		break;
	    case OTYPE_DXF:
		fprintf(fp, "0\nENDSEC\n0\nEOF\n");
		fclose(fp);
		break;
	    case OTYPE_SAT:
		fprintf(fp, "End-of-ACIS-data\n");
		fclose(fp);
		break;
	    default:
		fclose(fp);
		break;
	}
    }

    if (d->output_type == OTYPE_OBJ) {
	bu_vls_free(&d->obj.obj_materials_file);
	obj_free_materials(&d->obj);
	fclose(d->obj.obj_materials_fp);
    }

    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
