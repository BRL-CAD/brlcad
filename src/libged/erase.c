/*                         E R A S E . C
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
/** @file libged/erase.c
 *
 * The erase command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "solid.h"

#include "./ged_private.h"


void ged_splitGDL(struct ged *gedp, struct ged_display_list *gdlp, struct db_full_path *path);

/*
 * Erase objects from the display.
 *
 */
int
ged_erase(struct ged *gedp, int argc, const char *argv[])
{
    size_t i;
    int flag_A_attr=0;
    int flag_o_nonunique=1;
    int last_opt=0;
    struct bu_vls vls;
    static const char *usage = "[[-r] | [[-o] -A attribute=value]] [object(s)]";
    const char *cmdName = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    /* skip past cmd */
    --argc;
    ++argv;

    /* check args for options */
    bu_vls_init(&vls);
    for (i=0; i<(size_t)argc; i++) {
	char *ptr_A=NULL;
	char *ptr_o=NULL;

	if (*argv[i] != '-')
	    break;

	if (strchr(argv[i], 'r')) {
	    /* Erase all and quit (ignore other options) */
	    for (i = 1; i < (size_t)argc; ++i)
		_ged_eraseAllPathsFromDisplay(gedp, argv[i], 0);
	    return GED_OK;
	}

	ptr_A=strchr(argv[i], 'A');
	if (ptr_A)
	    flag_A_attr = 1;

	ptr_o=strchr(argv[i], 'o');
	if (ptr_o)
	    flag_o_nonunique = 2;

	last_opt = i;

	if (!ptr_A && !ptr_o) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	    return GED_ERROR;
	}

	if (strlen(argv[i]) == ((size_t)1 + (ptr_A != NULL) + (ptr_o != NULL))) {
	    /* argv[i] is just a "-A" or "-o" */
	    continue;
	}

	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_ERROR;
    }

    if (flag_A_attr) {
	/* args are attribute name/value pairs */
	struct bu_attribute_value_set avs;
	int max_count=0;
	int remaining_args=0;
	int new_argc=0;
	char **new_argv=NULL;
	struct bu_ptbl *tbl;

	remaining_args = argc - last_opt - 1;
	if (remaining_args < 2 || remaining_args%2) {
	    bu_vls_printf(gedp->ged_result_str, "Error: must have even number of arguments (name/value pairs)\n");
	    bu_vls_free(&vls);
	    return GED_ERROR;
	}

	bu_avs_init(&avs, (argc - last_opt)/2, "ged_erase avs");
	i = 0;
	while (i < (size_t)argc) {
	    if (*argv[i] == '-') {
		i++;
		continue;
	    }

	    /* this is a name/value pair */
	    if (flag_o_nonunique == 2) {
		bu_avs_add_nonunique(&avs, argv[i], argv[i+1]);
	    } else {
		bu_avs_add(&avs, argv[i], argv[i+1]);
	    }
	    i += 2;
	}

	tbl = db_lookup_by_attr(gedp->ged_wdbp->dbip, RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB, &avs, flag_o_nonunique);
	bu_avs_free(&avs);
	if (!tbl) {
	    bu_log("Error: db_lookup_by_attr() failed!!\n");
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}
	if (BU_PTBL_LEN(tbl) < 1) {
	    /* nothing matched, just return */
	    bu_vls_free(&vls);
	    return TCL_OK;
	}
	for (i=0; i<BU_PTBL_LEN(tbl); i++) {
	    struct directory *dp;

	    dp = (struct directory *)BU_PTBL_GET(tbl, i);
	    bu_vls_putc(&vls, ' ');
	    bu_vls_strcat(&vls, dp->d_namep);
	}

	max_count = BU_PTBL_LEN(tbl) + last_opt + 1;
	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "ged_erase ptbl");
	new_argv = (char **)bu_calloc(max_count+1, sizeof(char *), "ged_erase new_argv");
	new_argc = bu_argv_from_string(new_argv, max_count, bu_vls_addr(&vls));

	for (i = 0; i < (size_t)new_argc; ++i) {
	    /* Skip any options */
	    if (new_argv[i][0] == '-')
		continue;

	    ged_erasePathFromDisplay(gedp, new_argv[i], 1);
	}
    } else {
	for (i = 0; i < (size_t)argc; ++i)
	    ged_erasePathFromDisplay(gedp, argv[i], 1);
    }

    return GED_OK;
}


void
ged_splitGDL(struct ged *gedp,
	     struct ged_display_list *gdlp,
	     struct db_full_path *path)
{
    struct solid *sp;
    struct solid *nsp;
    struct ged_display_list *new_gdlp;
    char *pathname;
    int savelen;
    int newlen = path->fp_len + 1;

    if (newlen < 3) {
	while (BU_LIST_WHILE(sp, solid, &gdlp->gdl_headSolid)) {
	    savelen = sp->s_fullpath.fp_len;
	    sp->s_fullpath.fp_len = newlen;
	    pathname = db_path_to_string(&sp->s_fullpath);
	    sp->s_fullpath.fp_len = savelen;

	    new_gdlp = ged_addToDisplay(gedp, pathname);
	    bu_free((genptr_t)pathname, "ged_splitGDL pathname");

	    BU_LIST_DEQUEUE(&sp->l);
	    BU_LIST_INSERT(&new_gdlp->gdl_headSolid, &sp->l);
	}
    } else {
	sp = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);
	while (BU_LIST_NOT_HEAD(sp, &gdlp->gdl_headSolid)) {
	    nsp = BU_LIST_PNEXT(solid, sp);

	    if (db_full_path_match_top(path, &sp->s_fullpath)) {
		savelen = sp->s_fullpath.fp_len;
		sp->s_fullpath.fp_len = newlen;
		pathname = db_path_to_string(&sp->s_fullpath);
		sp->s_fullpath.fp_len = savelen;

		new_gdlp = ged_addToDisplay(gedp, pathname);
		bu_free((genptr_t)pathname, "ged_splitGDL pathname");

		BU_LIST_DEQUEUE(&sp->l);
		BU_LIST_INSERT(&new_gdlp->gdl_headSolid, &sp->l);
	    }

	    sp = nsp;
	}

	--path->fp_len;
	ged_splitGDL(gedp, gdlp, path);
	++path->fp_len;
    }
}


/*
 * Erase/remove the display list item from headDisplay if path matches the list item's path.
 *
 */
void
ged_erasePathFromDisplay(struct ged *gedp,
			 const char *path,
			 int allow_split)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct ged_display_list *last_gdlp;
    struct solid *sp;
    struct directory *dp;
    struct db_full_path subpath;
    int found_subpath;

    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, path) == 0)
	found_subpath = 1;
    else
	found_subpath = 0;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    last_gdlp = BU_LIST_LAST(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	if (BU_STR_EQUAL(path, bu_vls_addr(&gdlp->gdl_path))) {
	    /* Free up the solids list associated with this display list */
	    while (BU_LIST_WHILE(sp, solid, &gdlp->gdl_headSolid)) {
		dp = FIRST_SOLID(sp);
		RT_CK_DIR(dp);
		if (dp->d_addr == RT_DIR_PHONY_ADDR) {
		    if (db_dirdelete(gedp->ged_wdbp->dbip, dp) < 0) {
			bu_vls_printf(gedp->ged_result_str, "ged_erasePathFromDisplay: db_dirdelete failed\n");
		    }
		}

		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp, &_FreeSolid.l);
	    }

	    BU_LIST_DEQUEUE(&gdlp->l);
	    bu_vls_free(&gdlp->gdl_path);
	    free((void *)gdlp);

	    break;
	} else if (found_subpath) {
	    int need_split = 0;
	    struct solid *nsp;

	    sp = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);
	    while (BU_LIST_NOT_HEAD(sp, &gdlp->gdl_headSolid)) {
		nsp = BU_LIST_PNEXT(solid, sp);

		if (db_full_path_match_top(&subpath, &sp->s_fullpath)) {
		    BU_LIST_DEQUEUE(&sp->l);
		    FREE_SOLID(sp, &_FreeSolid.l);
		    need_split = 1;
		}

		sp = nsp;
	    }

	    if (BU_LIST_IS_EMPTY(&gdlp->gdl_headSolid)) {
		BU_LIST_DEQUEUE(&gdlp->l);
		bu_vls_free(&gdlp->gdl_path);
		free((void *)gdlp);
	    } else if (allow_split && need_split) {
		BU_LIST_DEQUEUE(&gdlp->l);

		--subpath.fp_len;
		ged_splitGDL(gedp, gdlp, &subpath);
		++subpath.fp_len;

		/* Free up the display list */
		bu_vls_free(&gdlp->gdl_path);
		free((void *)gdlp);
	    }
	}

	if (gdlp == last_gdlp)
	    gdlp = (struct ged_display_list *)&gedp->ged_gdp->gd_headDisplay;
	else
	    gdlp = next_gdlp;
    }

    if (found_subpath)
	db_free_full_path(&subpath);
}


HIDDEN void
eraseAllSubpathsFromSolidList(struct ged_display_list *gdlp,
			      struct db_full_path *subpath,
			      const int skip_first)
{
    struct solid *sp;
    struct solid *nsp;

    sp = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gdlp->gdl_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, subpath, skip_first)) {
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &_FreeSolid.l);
	}
	sp = nsp;
    }
}


/*
 * Erase/remove display list item from headDisplay if name is found anywhere along item's path with
 * the exception that the first path element is skipped if skip_first is true.
 *
 * Note - name is not expected to contain path separators.
 *
 */
void
_ged_eraseAllNamesFromDisplay(struct ged *gedp,
			      const char *name,
			      const int skip_first)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	char *dup_path;
	char *tok;
	int first = 1;
	int found = 0;

	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	dup_path = strdup(bu_vls_addr(&gdlp->gdl_path));
	tok = strtok(dup_path, "/");
	while (tok) {
	    if (first) {
		first = 0;

		if (skip_first) {
		    tok = strtok((char *)NULL, "/");
		    continue;
		}
	    }

	    if (BU_STR_EQUAL(tok, name)) {
		_ged_freeDisplayListItem(gedp, gdlp);
		found = 1;

		break;
	    }

	    tok = strtok((char *)NULL, "/");
	}

	/* Look for name in solids list */
	if (!found) {
	    struct db_full_path subpath;

	    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, name) == 0) {
		eraseAllSubpathsFromSolidList(gdlp, &subpath, skip_first);
		db_free_full_path(&subpath);
	    }
	}

	free((void *)dup_path);
	gdlp = next_gdlp;
    }
}


int
_ged_eraseFirstSubpath(struct ged *gedp,
		       struct ged_display_list *gdlp,
		       struct db_full_path *subpath,
		       const int skip_first)
{
    struct solid *sp;
    struct solid *nsp;
    struct db_full_path dup_path;

    db_full_path_init(&dup_path);

    sp = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gdlp->gdl_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, subpath, skip_first)) {
	    int ret;

	    db_dup_full_path(&dup_path, &sp->s_fullpath);
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &_FreeSolid.l);

	    BU_LIST_DEQUEUE(&gdlp->l);

	    if (!BU_LIST_IS_EMPTY(&gdlp->gdl_headSolid)) {
		ged_splitGDL(gedp, gdlp, &dup_path);
		ret = 1;
	    } else {
		ret = 0;
	    }

	    db_free_full_path(&dup_path);

	    /* Free up the display list */
	    bu_vls_free(&gdlp->gdl_path);
	    free((void *)gdlp);

	    return ret;
	}
	sp = nsp;
    }

    return 0;
}


/*
 * Erase/remove display list item from headDisplay if path is a subset of item's path.
 */
void
_ged_eraseAllPathsFromDisplay(struct ged *gedp,
			      const char *path,
			      const int skip_first)
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    struct db_full_path fullpath, subpath;

    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, path) == 0) {
	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    gdlp->gdl_wflag = 0;
	    gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);
	}

	gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    /* This display list has already been visited. */
	    if (gdlp->gdl_wflag) {
		gdlp = next_gdlp;
		continue;
	    }

	    /* Mark as being visited. */
	    gdlp->gdl_wflag = 1;

	    if (db_string_to_path(&fullpath, gedp->ged_wdbp->dbip, bu_vls_addr(&gdlp->gdl_path)) == 0) {
		if (db_full_path_subset(&fullpath, &subpath, skip_first)) {
		    _ged_freeDisplayListItem(gedp, gdlp);
		} else if (_ged_eraseFirstSubpath(gedp, gdlp, &subpath, skip_first)) {
		    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
		    db_free_full_path(&fullpath);
		    continue;
		}

		db_free_full_path(&fullpath);
	    }

	    gdlp = next_gdlp;
	}

	db_free_full_path(&subpath);
    }
}


void
_ged_freeDisplayListItem (struct ged *gedp,
			  struct ged_display_list *gdlp)
{
    struct solid *sp;
    struct directory *dp;

    /* Free up the solids list associated with this display list */
    while (BU_LIST_WHILE(sp, solid, &gdlp->gdl_headSolid)) {
	dp = FIRST_SOLID(sp);
	RT_CK_DIR(dp);
	if (dp->d_addr == RT_DIR_PHONY_ADDR) {
	    if (db_dirdelete(gedp->ged_wdbp->dbip, dp) < 0) {
		bu_vls_printf(gedp->ged_result_str, "_ged_freeDisplayListItem: db_dirdelete failed\n");
	    }
	}

	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp, &_FreeSolid.l);
    }

    /* Free up the display list */
    BU_LIST_DEQUEUE(&gdlp->l);
    bu_vls_free(&gdlp->gdl_path);
    free((void *)gdlp);
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
