/*                  D I S P L A Y _ L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/display_list.c
 *
 * Collect display list manipulation logic here as it is refactored.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "solid.h"

#include "./ged_private.h"

void
headsolid_split(struct bu_list *hdlp, struct db_i *dbip, struct solid *sp, int newlen)
{
    int savelen;
    struct display_list *new_gdlp;
    char *pathname;

    savelen = sp->s_fullpath.fp_len;
    sp->s_fullpath.fp_len = newlen;
    pathname = db_path_to_string(&sp->s_fullpath);
    sp->s_fullpath.fp_len = savelen;

    new_gdlp = dl_addToDisplay(hdlp, dbip, pathname);
    bu_free((void *)pathname, "headsolid_split pathname");

    BU_LIST_DEQUEUE(&sp->l);
    BU_LIST_INSERT(&new_gdlp->dl_headSolid, &sp->l);
}



int
headsolid_splitGDL(struct bu_list *hdlp, struct db_i *dbip, struct display_list *gdlp, struct db_full_path *path)
{
    struct solid *sp;
    struct solid *nsp;
    int newlen = path->fp_len + 1;

    if (BU_LIST_IS_EMPTY(&gdlp->dl_headSolid)) return 0;

    if (newlen < 3) {
	while (BU_LIST_WHILE(sp, solid, &gdlp->dl_headSolid)) {
	    headsolid_split(hdlp, dbip, sp, newlen);
	}
    } else {
	sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
	while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
	    nsp = BU_LIST_PNEXT(solid, sp);
	    if (db_full_path_match_top(path, &sp->s_fullpath)) {
		headsolid_split(hdlp, dbip, sp, newlen);
	    }
	    sp = nsp;
	}

	--path->fp_len;
	headsolid_splitGDL(hdlp, dbip, gdlp, path);
	++path->fp_len;
    }

    return 1;
}



/*
 * Erase/remove the display list item from headDisplay if path matches the list item's path.
 *
 */
void
dl_erasePathFromDisplay(struct bu_list *hdlp,
	struct db_i *dbip, void (*callback)(unsigned int, int),
       	const char *path, int allow_split)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct display_list *last_gdlp;
    struct solid *sp;
    struct directory *dp;
    struct db_full_path subpath;
    int found_subpath;

    if (db_string_to_path(&subpath, dbip, path) == 0)
	found_subpath = 1;
    else
	found_subpath = 0;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    last_gdlp = BU_LIST_LAST(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	if (BU_STR_EQUAL(path, bu_vls_addr(&gdlp->dl_path))) {
	    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL) {

		/* We can't assume the display lists are contiguous */
		FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		    (*callback)(BU_LIST_FIRST(solid, &gdlp->dl_headSolid)->s_dlist, 1);
		}
	    }

	    /* Free up the solids list associated with this display list */
	    while (BU_LIST_WHILE(sp, solid, &gdlp->dl_headSolid)) {
		dp = FIRST_SOLID(sp);
		RT_CK_DIR(dp);
		if (dp->d_addr == RT_DIR_PHONY_ADDR) {
		    (void)db_dirdelete(dbip, dp);
		}

		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp, &_FreeSolid.l);
	    }

	    BU_LIST_DEQUEUE(&gdlp->l);
	    bu_vls_free(&gdlp->dl_path);
	    free((void *)gdlp);

	    break;
	} else if (found_subpath) {
	    int need_split = 0;
	    struct solid *nsp;

	    sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
	    while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
		nsp = BU_LIST_PNEXT(solid, sp);

		if (db_full_path_match_top(&subpath, &sp->s_fullpath)) {
		    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL)
			(*callback)(sp->s_dlist, 1);

		    BU_LIST_DEQUEUE(&sp->l);
		    FREE_SOLID(sp, &_FreeSolid.l);
		    need_split = 1;
		}

		sp = nsp;
	    }

	    if (BU_LIST_IS_EMPTY(&gdlp->dl_headSolid)) {
		BU_LIST_DEQUEUE(&gdlp->l);
		bu_vls_free(&gdlp->dl_path);
		free((void *)gdlp);
	    } else if (allow_split && need_split) {
		BU_LIST_DEQUEUE(&gdlp->l);

		--subpath.fp_len;
		(void)headsolid_splitGDL(hdlp, dbip, gdlp, &subpath);
		++subpath.fp_len;

		/* Free up the display list */
		bu_vls_free(&gdlp->dl_path);
		free((void *)gdlp);
	    }
	}

	if (gdlp == last_gdlp)
	    gdlp = (struct display_list *)hdlp;
	else
	    gdlp = next_gdlp;
    }

    if (found_subpath)
	db_free_full_path(&subpath);
}


HIDDEN void
eraseAllSubpathsFromSolidList(struct display_list *gdlp,
			      struct db_full_path *subpath,
			      void (*callback)(unsigned int, int),
			      const int skip_first)
{
    struct solid *sp;
    struct solid *nsp;

    sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, subpath, skip_first)) {
	    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL)
		(*callback)(sp->s_dlist, 1);

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
_dl_eraseAllNamesFromDisplay(struct bu_list *hdlp, struct db_i *dbip,
       	void (*callback)(unsigned int, int), const char *name, const int skip_first)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	char *dup_path;
	char *tok;
	int first = 1;
	int found = 0;

	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	dup_path = strdup(bu_vls_addr(&gdlp->dl_path));
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
		_dl_freeDisplayListItem(dbip, callback, gdlp);
		found = 1;

		break;
	    }

	    tok = strtok((char *)NULL, "/");
	}

	/* Look for name in solids list */
	if (!found) {
	    struct db_full_path subpath;

	    if (db_string_to_path(&subpath, dbip, name) == 0) {
		eraseAllSubpathsFromSolidList(gdlp, &subpath, callback, skip_first);
		db_free_full_path(&subpath);
	    }
	}

	free((void *)dup_path);
	gdlp = next_gdlp;
    }
}


int
_dl_eraseFirstSubpath(struct bu_list *hdlp, struct db_i *dbip,
       	               void (*callback)(unsigned int, int),
		       struct display_list *gdlp,
		       struct db_full_path *subpath,
		       const int skip_first)
{
    struct solid *sp;
    struct solid *nsp;
    struct db_full_path dup_path;

    db_full_path_init(&dup_path);

    sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
    while (BU_LIST_NOT_HEAD(sp, &gdlp->dl_headSolid)) {
	nsp = BU_LIST_PNEXT(solid, sp);
	if (db_full_path_subset(&sp->s_fullpath, subpath, skip_first)) {
	    int ret;
	    int full_len = sp->s_fullpath.fp_len;

	    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL)
		(*callback)(sp->s_dlist, 1);

	    sp->s_fullpath.fp_len = full_len - 1;
	    db_dup_full_path(&dup_path, &sp->s_fullpath);
	    sp->s_fullpath.fp_len = full_len;
	    BU_LIST_DEQUEUE(&sp->l);
	    FREE_SOLID(sp, &_FreeSolid.l);

	    BU_LIST_DEQUEUE(&gdlp->l);

	    ret = headsolid_splitGDL(hdlp, dbip, gdlp, &dup_path);

	    db_free_full_path(&dup_path);

	    /* Free up the display list */
	    bu_vls_free(&gdlp->dl_path);
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
_dl_eraseAllPathsFromDisplay(struct bu_list *hdlp, struct db_i *dbip,
       	                      void (*callback)(unsigned int, int),
			      const char *path,
			      const int skip_first)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct db_full_path fullpath, subpath;

    if (db_string_to_path(&subpath, dbip, path) == 0) {
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    gdlp->dl_wflag = 0;
	    gdlp = BU_LIST_PNEXT(display_list, gdlp);
	}

	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    /* This display list has already been visited. */
	    if (gdlp->dl_wflag) {
		gdlp = next_gdlp;
		continue;
	    }

	    /* Mark as being visited. */
	    gdlp->dl_wflag = 1;

	    if (db_string_to_path(&fullpath, dbip, bu_vls_addr(&gdlp->dl_path)) == 0) {
		if (db_full_path_subset(&fullpath, &subpath, skip_first)) {
		    _dl_freeDisplayListItem(dbip, callback, gdlp);
		} else if (_dl_eraseFirstSubpath(hdlp, dbip, callback, gdlp, &subpath, skip_first)) {
		    gdlp = BU_LIST_NEXT(display_list, hdlp);
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
_dl_freeDisplayListItem (struct db_i *dbip,
       	void (*callback)(unsigned int, int),
	struct display_list *gdlp)
{
    struct solid *sp;
    struct directory *dp;

    if (callback != GED_FREE_VLIST_CALLBACK_PTR_NULL) {

	/* We can't assume the display lists are contiguous */
	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    (*callback)(BU_LIST_FIRST(solid, &gdlp->dl_headSolid)->s_dlist, 1);
	}
    }

    /* Free up the solids list associated with this display list */
    while (BU_LIST_WHILE(sp, solid, &gdlp->dl_headSolid)) {
	dp = FIRST_SOLID(sp);
	RT_CK_DIR(dp);
	if (dp->d_addr == RT_DIR_PHONY_ADDR) {
	    (void)db_dirdelete(dbip, dp);
	}

	BU_LIST_DEQUEUE(&sp->l);
	FREE_SOLID(sp, &_FreeSolid.l);
    }

    /* Free up the display list */
    BU_LIST_DEQUEUE(&gdlp->l);
    bu_vls_free(&gdlp->dl_path);
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
