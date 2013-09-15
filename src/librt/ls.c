/*                           L S . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/ls.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"

HIDDEN int
dp_eval_flags(struct directory *dp, int flags)
{
    int flag_eval = 0;

    /* TODO - do we ever write these? */
    if (dp->d_addr == RT_DIR_PHONY_ADDR) return 0;

    /* Unless we're explicitly listing hidden objects, it's game over if the
     * hidden flag is set */
    if (!(flags & DB_LS_HIDDEN) && (dp->d_flags & RT_DIR_HIDDEN)) return 0;

    /* For any other flags that were provided, if we don't match them we don't return
     * true.  If no flags are present, we default to true. */
    if (flags & DB_LS_PRIM)     { flag_eval += (dp->d_flags & RT_DIR_SOLID)    ? 0 : 1; }
    if (flags & DB_LS_COMB)     { flag_eval += (dp->d_flags & RT_DIR_COMB)     ? 0 : 1; }
    if (flags & DB_LS_REGION)   { flag_eval += (dp->d_flags & RT_DIR_REGION)   ? 0 : 1; }
    if (flags & DB_LS_NON_GEOM) { flag_eval += (dp->d_flags & RT_DIR_NON_GEOM) ? 0 : 1; }
    if (flags & DB_LS_TOPS)     { flag_eval += (dp->d_nref == 0)               ? 0 : 1; }
    flag_eval = (flag_eval) ? 0 : 1;

    /* Now that we have handled the filtering flags, check to see if we pass
     * the additive flags - these allow for returning (say) a list of primitives
     * and regions but not combs */
    if (flags & DB_LS_ADD_PRIM)     { flag_eval += (dp->d_flags & RT_DIR_SOLID)    ? 1 : 0; }
    if (flags & DB_LS_ADD_COMB)     { flag_eval += (dp->d_flags & RT_DIR_COMB)     ? 1 : 0; }
    if (flags & DB_LS_ADD_REGION)   { flag_eval += (dp->d_flags & RT_DIR_REGION)   ? 1 : 0; }
    if (flags & DB_LS_ADD_NON_GEOM) { flag_eval += (dp->d_flags & RT_DIR_NON_GEOM) ? 1 : 0; }
    return (flag_eval) ? 1 : 0;
}

int
db_ls(const struct db_i *dbip, int flags, struct directory ***dpv)
{
    int i;
    int objcount = 0;
    struct directory *dp;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    objcount += dp_eval_flags(dp, flags);
	}
    }
    if (objcount > 0) {
	(*dpv) = (struct directory **)bu_malloc(sizeof(struct directory *) * (objcount + 1), "directory pointer array");
	objcount = 0;
	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (dp_eval_flags(dp,flags)) {
		    (*dpv)[objcount] = dp;
		    objcount++;
		}
	    }
	}
	(*dpv)[objcount] = '\0';
    }
    return objcount;
}

struct directory **
db_argv_to_dpv(const struct db_i *dbip, int argc, const char **argv)
{
    int i = 0;
    struct directory **dpv = NULL;
    struct directory *dp = RT_DIR_NULL;
    if (!argv || !argc) return dpv;
    dpv = (struct directory **)bu_malloc(sizeof(struct directory *) * (argc + 1), "directory pointer array");
    for (i = 0; i < argc; i++) {
	if (argv[i]) {
	    dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	    dpv[i] = (dp == RT_DIR_NULL) ? RT_DIR_NULL : dp;
	} else {
	    dpv[i] = RT_DIR_NULL;
	}
    }
    dpv[argc] = RT_DIR_NULL;
    return dpv;
}

char **
db_dpv_to_argv(struct directory **dpv, int argc)
{
    int i = 0;
    char **argv = NULL;
    if (!dpv || !argc) return argv;
    argv = (char **)bu_malloc(sizeof(char *) * (argc + 1), "char pointer array");
    for (i = 0; i < argc; i++) {
	argv[i] = (dpv[i] != RT_DIR_NULL) ? dpv[i]->d_namep : NULL;
    }
    argv[argc] = '\0';
    return argv;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
