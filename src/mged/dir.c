/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "bio.h"
#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"

#include "./mged.h"
#include "./mged_dm.h"


/*
 * F _ M E M P R I N T
 *
 * Debugging aid:  dump memory maps
 */
int
f_memprint(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    CHECK_DBI_NULL;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls;

	if (argv && argc > 1)
	    bu_log("Unexpected parameter [%s]\n", argv[1]);
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help memprint");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_log("Database free-storage map:\n");
    rt_memprint(&(dbip->dbi_freep));

    return TCL_OK;
}


HIDDEN void
Do_prefix(struct db_i *dbi, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, genptr_t prefix_ptr, genptr_t obj_ptr, genptr_t UNUSED(user_ptr3))
{
    char *prefix, *obj;
    char tempstring_v4[NAMESIZE+1];
    size_t len = NAMESIZE+1;

    RT_CK_DBI(dbi);
    RT_CK_TREE(comb_leaf);

    prefix = (char *)prefix_ptr;
    obj = (char *)obj_ptr;

    if (!BU_STR_EQUAL(comb_leaf->tr_l.tl_name, obj))
	return;

    bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
    if (dbi->dbi_version < 5) {
	bu_strlcpy(tempstring_v4, prefix, len);
	bu_strlcat(tempstring_v4, obj, len);
	comb_leaf->tr_l.tl_name = bu_strdup(tempstring_v4);
    } else {
	len = strlen(prefix)+strlen(obj)+1;
	comb_leaf->tr_l.tl_name = (char *)bu_malloc(len, "Adding prefix");

	bu_strlcpy(comb_leaf->tr_l.tl_name , prefix, len);
	bu_strlcat(comb_leaf->tr_l.tl_name , obj, len);
    }
}


/*
 * F _ P R E F I X
 *
 * Prefix each occurence of a specified object name, both
 * when defining the object, and when referencing it.
 */
int
f_prefix(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int i, k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    char tempstring_v4[NAMESIZE+1];
    struct bu_vls tempstring_v5;
    char *tempstring;
    int len = NAMESIZE+1;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help prefix");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init(&tempstring_v5);

    /* First, check validity, and change node names */
    for (i = 2; i < argc; i++) {
	if ((dp = db_lookup(dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL) {
	    argv[i] = "";
	    continue;
	}

	if (dbip->dbi_version < 5 && (int)(strlen(argv[1]) + strlen(argv[i])) > NAMESIZE) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "'%s%s' too long, must be %d characters or less.\n",
			  argv[1], argv[i], NAMESIZE);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);

	    argv[i] = "";
	    continue;
	}

	if (dbip->dbi_version < 5) {
	    bu_strlcpy(tempstring_v4, argv[1], len);
	    bu_strlcat(tempstring_v4, argv[i], len);
	    tempstring = tempstring_v4;
	} else {
	    bu_vls_trunc(&tempstring_v5, 0);
	    bu_vls_strcpy(&tempstring_v5, argv[1]);
	    bu_vls_strcat(&tempstring_v5, argv[i]);
	    tempstring = bu_vls_addr(&tempstring_v5);
	}

	if (db_lookup(dbip, tempstring, LOOKUP_QUIET) != DIR_NULL) {
	    aexists(tempstring);
	    argv[i] = "";
	    continue;
	}
	/* Change object name in the directory. */
	if (db_rename(dbip, dp, tempstring) < 0) {
	    bu_vls_free(&tempstring_v5);
	    Tcl_AppendResult(interp, "error in rename to ", tempstring,
			     ", aborting\n", (char *)NULL);
	    TCL_ERROR_RECOVERY_SUGGESTION;
	    return TCL_ERROR;
	}
    }

    bu_vls_free(&tempstring_v5);

    /* Examine all COMB nodes */
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (!(dp->d_flags & DIR_COMB))
	    continue;

	if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	    TCL_READ_ERR_return;
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	for (k=2; k<argc; k++)
	    db_tree_funcleaf(dbip, comb, comb->tree, Do_prefix,
			     (genptr_t)argv[1], (genptr_t)argv[k], (genptr_t)NULL);
	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource))
	    TCL_WRITE_ERR_return;
    } FOR_ALL_DIRECTORY_END;
    return TCL_OK;
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
