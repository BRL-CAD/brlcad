/*                         M O V E _ A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/move_all.c
 *
 * The move_all command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"

#include "./ged_private.h"

HIDDEN int
move_all_func(struct ged *gedp, int nflag, const char *old, const char *new)
{
    int i;
    struct ged_display_list *gdlp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_ptbl stack;

    /* rename the record itself */
    if ((dp = db_lookup(gedp->ged_wdbp->dbip, old, LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (db_lookup(gedp->ged_wdbp->dbip, new, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: already exists", new);
	return GED_ERROR;
    }

    /* if this was a sketch, we need to look for all the extrude
     * objects that might use it.
     *
     * This has to be done here, before we rename the (possible) sketch object
     * because the extrude will do a rt_db_get on the sketch when we call
     * rt_db_get_internal on it.
     */
    if (dp->d_major_type == DB5_MAJORTYPE_BRLCAD && \
	dp->d_minor_type == DB5_MINORTYPE_BRLCAD_SKETCH) {

	struct directory *dirp;

	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dirp = gedp->ged_wdbp->dbip->dbi_Head[i]; dirp != RT_DIR_NULL; dirp = dirp->d_forw) {

		if (dirp->d_major_type == DB5_MAJORTYPE_BRLCAD && \
		    dirp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE) {
		    struct rt_extrude_internal *extrude;

		    if (rt_db_get_internal(&intern, dirp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
			bu_log("Can't get extrude %s?\n", dirp->d_namep);
			continue;
		    }
		    extrude = (struct rt_extrude_internal *)intern.idb_ptr;
		    RT_EXTRUDE_CK_MAGIC(extrude);

		    if (BU_STR_EQUAL(extrude->sketch_name, old)) {
			if (nflag) {
			    bu_vls_printf(&gedp->ged_result_str, "%s ", dirp->d_namep);
			    rt_db_free_internal(&intern);
			} else {
			    bu_free(extrude->sketch_name, "sketch name");
			    extrude->sketch_name = bu_strdup(new);

			    if (rt_db_put_internal(dirp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
				bu_log("oops\n");
			    }
			}
		    } else
			rt_db_free_internal(&intern);
		}
	    }
	}
    }

    if (!nflag) {
	/* Change object name in the directory. */
	if (db_rename(gedp->ged_wdbp->dbip, dp, new) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "error in rename to %s, aborting", new);
	    return GED_ERROR;
	}

	/* Change name in the file */
	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting");
	    return GED_ERROR;
	}

	if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "Database write error, aborting");
	    return GED_ERROR;
	}
    }

    bu_ptbl_init(&stack, 64, "combination stack for wdb_mvall_cmd");


    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    union tree *comb_leaf;
	    int done=0;
	    int changed=0;


	    if (!(dp->d_flags & RT_DIR_COMB))
		continue;

	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
		continue;
	    comb = (struct rt_comb_internal *)intern.idb_ptr;

	    bu_ptbl_reset(&stack);
	    /* visit each leaf in the combination */
	    comb_leaf = comb->tree;
	    if (comb_leaf) {
		while (!done) {
		    while (comb_leaf->tr_op != OP_DB_LEAF) {
			bu_ptbl_ins(&stack, (long *)comb_leaf);
			comb_leaf = comb_leaf->tr_b.tb_left;
		    }

		    if (BU_STR_EQUAL(comb_leaf->tr_l.tl_name, old)) {
			if (nflag)
			    bu_vls_printf(&gedp->ged_result_str, "%s ", dp->d_namep);
			else {
			    bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
			    comb_leaf->tr_l.tl_name = bu_strdup(new);
			    changed = 1;
			}
		    }

		    if (BU_PTBL_END(&stack) < 1) {
			done = 1;
			break;
		    }
		    comb_leaf = (union tree *)BU_PTBL_GET(&stack, BU_PTBL_END(&stack)-1);
		    if (comb_leaf->tr_op != OP_DB_LEAF) {
			bu_ptbl_rm(&stack, (long *)comb_leaf);
			comb_leaf = comb_leaf->tr_b.tb_right;
		    }
		}
	    }

	    if (changed) {
		if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource)) {
		    bu_ptbl_free(&stack);
		    bu_vls_printf(&gedp->ged_result_str, "Database write error, aborting");
		    return GED_ERROR;
		}
	    } else
		rt_db_free_internal(&intern);
	}
    }

    bu_ptbl_free(&stack);

    if (!nflag) {
	/* Change object name anywhere in the display list path. */
	for (BU_LIST_FOR(gdlp, ged_display_list, &gedp->ged_gdp->gd_headDisplay)) {
	    int first = 1;
	    int found = 0;
	    struct bu_vls new_path;
	    char *dupstr = strdup(bu_vls_addr(&gdlp->gdl_path));
	    char *tok = strtok(dupstr, "/");

	    bu_vls_init(&new_path);

	    while (tok) {
		if (BU_STR_EQUAL(tok, old)) {
		    found = 1;

		    if (first) {
			first = 0;
			bu_vls_printf(&new_path, "%s", new);
		    } else
			bu_vls_printf(&new_path, "/%s", new);
		} else {
		    if (first) {
			first = 0;
			bu_vls_printf(&new_path, "%s", tok);
		    } else
			bu_vls_printf(&new_path, "/%s", tok);
		}

		tok = strtok((char *)NULL, "/");
	    }

	    if (found) {
		bu_vls_free(&gdlp->gdl_path);
		bu_vls_printf(&gdlp->gdl_path, "%s", bu_vls_addr(&new_path));
	    }

	    free((void *)dupstr);
	    bu_vls_free(&new_path);
	}
    }

    return GED_OK;
}


HIDDEN int
move_all_file(struct ged *gedp, int nflag, const char *file)
{
    FILE *fp;
    char line[512];

    if ((fp=fopen(file, "r")) == NULL) {
	bu_vls_printf(&gedp->ged_result_str, "cannot open %s\n", file);
	return GED_ERROR;
    }

    while (bu_fgets(line, sizeof(line), fp) != NULL) {
	char *cp;
	char *new_av[3];

	/* Skip comments */
	if ((cp = strchr(line, '#')) != NULL)
	    *cp = '\0';

	if (bu_argv_from_string(new_av, 2, line) != 2)
	    continue;

	move_all_func(gedp, nflag, (const char *)new_av[0], (const char *)new_av[1]);
    }

    fclose(fp);

    return GED_OK;
}


int
ged_move_all(struct ged *gedp, int argc, const char *argv[])
{
    char c;
    int fflag = 0;
    int nflag = 0;
    static const char *usage = "[-n] {-f <mapping_file>|<from> <to>}";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (db_version(gedp->ged_wdbp->dbip) < 5 && (int)strlen(argv[2]) > NAMESIZE) {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: name length limited to %zu characters in v4 databases\n", strlen(argv[2]));
	return GED_ERROR;
    }

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "fn")) != -1) {
	switch (c) {
	    case 'f':
		fflag = 1;
		break;
	    case 'n':
		nflag = 1;
		break;
	    default:
		bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	}
    }

    argc -= bu_optind;
    argv += bu_optind;

    if (fflag) {
	if (argc != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

	return move_all_file(gedp, nflag, argv[0]);
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    return move_all_func(gedp, nflag, argv[0], argv[1]);
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
