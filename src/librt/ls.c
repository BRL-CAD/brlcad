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


#define DB_LS_HIDE_SOLID	0x1
#define DB_LS_HIDE_COMB		0x2
#define DB_LS_HIDE_REGION	0x4
#define DB_LS_SHOW_HIDDEN	0x8
#define DB_LS_HIDE_NON_GEOM	0x10
#define DB_LS_HIDE_NON_TOPS	0x20

HIDDEN int
dp_eval_flags(struct directory *dp, int flags)
{
    int add_obj = 1;
    if (dp->d_addr == RT_DIR_PHONY_ADDR) add_obj = 0; /* TODO - do we ever write these? */
    if ((flags & DB_LS_HIDE_SOLID) && (dp->d_flags & RT_DIR_SOLID)) add_obj = 0;
    if ((flags & DB_LS_HIDE_COMB) && (dp->d_flags & RT_DIR_COMB)) add_obj = 0;
    if ((flags & DB_LS_HIDE_REGION) && (dp->d_flags & RT_DIR_COMB)) add_obj = 0;
    if (!(flags & DB_LS_SHOW_HIDDEN) && (dp->d_flags & RT_DIR_HIDDEN)) add_obj = 0;
    if ((flags & DB_LS_HIDE_NON_GEOM) && (dp->d_flags & RT_DIR_NON_GEOM)) add_obj = 0;
    if ((flags & DB_LS_HIDE_NON_TOPS) && (!(dp->d_nref == 0))) add_obj = 0;
    return add_obj;
}

struct directory **
db_ls(const struct db_i *dbip, int flags)
{
    int i;
    int objcount = 0;
    struct directory *dp;
    struct directory **dpv;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    objcount += dp_eval_flags(dp, flags);
	}
    }
    if (objcount > 0) {
	dpv = (struct directory **)bu_malloc(sizeof(struct directory *) * (objcount + 1), "directory pointer array");
	objcount = 0;
	for (i = 0; i < RT_DBNHASH; i++) {
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (dp_eval_flags(dp,flags)) {
		    dpv[objcount] = dp;
		    objcount++;
		}
	    }
	}
	dpv[objcount] = '\0';
    }
    return dpv;
}

struct directory **
db_argv_to_dpv(const struct db_i *dbip, const char **argv)
{
    struct directory **dpv = NULL;
    struct directory *dp = RT_DIR_NULL;
    int argv_cnt = 0;
    const char *arg = argv[0];
    if (!argv) return dpv;
    while (arg) {
	argv_cnt++;
	arg = argv[argv_cnt];
    }
    if (argv_cnt > 0) {
	dpv = (struct directory **)bu_malloc(sizeof(struct directory *) * (argv_cnt + 1), "directory pointer array");
	argv_cnt = 0;
	arg = argv[0];
	while (arg) {
	    dp = db_lookup(dbip, arg, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		dpv[argv_cnt] = RT_DIR_NULL;
	    } else {
		dpv[argv_cnt] = dp;
	    }
	    argv_cnt++;
	    arg = argv[argv_cnt];
	}
    }
    return dpv;
}

char **
db_dpv_to_argv(struct directory **dpv)
{
    char **argv = NULL;
    int dpv_cnt = 0;
    struct directory *dp = dpv[dpv_cnt];
    if (!dpv) return argv;
    while (dp) {
	dpv_cnt++;
	dp = dpv[dpv_cnt];
    }
    if (dpv_cnt > 0) {
	argv = (char **)bu_malloc(sizeof(char *) * (dpv_cnt + 1), "char pointer array");
	dpv_cnt = 0;
	dp = dpv[0];
	while (dpv) {
	    argv[dpv_cnt] = dpv[dpv_cnt]->d_namep;
	    dpv_cnt++;
	}
    }
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
