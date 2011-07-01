/*                         P U T M A T . C
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
/** @file libged/putmat.c
 *
 * The putmat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_getmat(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    char name1[RT_MAXLINE];
    char name2[RT_MAXLINE];
    static const char *usage = "a/b";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    {
	const char *begin;
	const char *first_fs;
	const char *last_fs;
	const char *end;

	/* skip leading slashes */
	begin = argv[1];
	while (*begin == '/')
	    ++begin;

	if (*begin == '\0' ||
	    !(first_fs = strchr(begin, '/')) ||
	    !(last_fs = strrchr(begin, '/')) ||
	    first_fs != last_fs) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad path specification '%s'", argv[0], argv[1]);
	    return GED_ERROR;
	}

	/* Note: At this point first_fs == last_fs */

	end = strrchr(begin, '\0');
	if (last_fs == end-1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad path specification '%s'", argv[0], argv[1]);
	    return GED_ERROR;
	}
	strncpy(name1, begin, (size_t)(last_fs-begin));
	name1[last_fs-begin] = '\0';
	strncpy(name2, last_fs+1, (size_t)(end-last_fs)); /* This includes the EOS */
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, name1, LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not found in database.\n", argv[0], name1);
	return GED_ERROR;
    }

    if (!(dp->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not a combination\n", argv[0], name1);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    if (!comb->tree) {
	bu_vls_printf(gedp->ged_result_str, "%s: empty combination", dp->d_namep);
	goto fail;
    }

    /* Search for first mention of arc */
    if ((tp = db_find_named_leaf(comb->tree, name2)) == TREE_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to find instance of '%s' in combination '%s', error",
		      name2, name1);
	goto fail;
    }

    if (!tp->tr_l.tl_mat) {
	bu_vls_printf(gedp->ged_result_str, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1");
	rt_db_free_internal(&intern);

	return GED_OK;
    } else {
	register int i;

	for (i=0; i<16; i++)
	    bu_vls_printf(gedp->ged_result_str, "%lf ", tp->tr_l.tl_mat[i]);

	rt_db_free_internal(&intern);

	return GED_OK;
    }

fail:
    rt_db_free_internal(&intern);
    return GED_ERROR;
}


/*
 * Replace the matrix on an arc in the database from the command line,
 * when NOT in an edit state.  Used mostly to facilitate writing shell
 * scripts.  There are two valid syntaxes, each of which is
 * implemented as an appropriate call to f_arced.  Commands of the
 * form:
 *
 * putmat a/b m0 m1 ... m15
 *
 * are converted to:
 *
 * arced a/b matrix rarc m0 m1 ... m15,
 *
 * while commands of the form:
 *
 * putmat a/b I
 *
 * are converted to:
 *
 * arced a/b matrix rarc 1 0 0 0   0 1 0 0   0 0 1 0   0 0 0 1
 *
 */
int
ged_putmat(struct ged *gedp, int argc, const char *argv[])
{
    int result = GED_OK;	/* Return code */
    char *newargv[20+2];
    struct bu_vls *avp;
    int got;
    static const char *usage = "a/b I|m0 m1 ... m15";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc == 2)
	return ged_getmat(gedp, argc, argv);

    if (argc < 3 || 18 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (!strchr(argv[1], '/')) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad path spec '%s'\n", argv[0], argv[1]);
	return GED_ERROR;
    }
    switch (argc) {
	case 18:
	    avp = bu_vls_vlsinit();
	    bu_vls_from_argv(avp, 16, (const char **)argv + 2);
	    break;
	case 3:
	    if ((argv[2][0] == 'I') && (argv[2][1] == '\0')) {
		avp = bu_vls_vlsinit();
		bu_vls_printf(avp, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 ");
		break;
	    }
	    /* Sometimes the matrix is sent through Tcl as one long string.
	     * Copy it so we can crack it, below.
	     */
	    avp = bu_vls_vlsinit();
	    bu_vls_strcat(avp, argv[2]);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s: error in matrix specification (wrong number of args)\n", argv[0]);
	    return GED_ERROR;
    }
    newargv[0] = "arced";
    newargv[1] = (char *)argv[1];
    newargv[2] = "matrix";
    newargv[3] = "rarc";

    got = bu_argv_from_string(&newargv[4], 16, bu_vls_addr(avp));
    if (got != 16) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s:%d: bad matrix, only got %d elements\n",
		      argv[0], __FILE__, __LINE__, got);
	result = GED_ERROR;
    }

    if (result != GED_ERROR)
	result = ged_arced(gedp, 20, (const char **)newargv);

    bu_vls_vlsfree(avp);
    return result;
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
