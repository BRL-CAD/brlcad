/*                         D U P . C
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
/** @file libged/dup.c
 *
 * The dup command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


struct dir_check_stuff {
    struct db_i *main_dbip;
    struct rt_wdb *wdbp;
    struct directory **dup_dirp;
};


static void
dup_dir_check5(struct db_i *input_dbip,
	       const struct db5_raw_internal *rip,
	       off_t addr,
	       genptr_t ptr)
{
    char *name;
    struct directory *dupdp;
    struct bu_vls local;
    struct dir_check_stuff *dcsp = (struct dir_check_stuff *)ptr;

    if (dcsp->main_dbip == DBI_NULL)
	return;

    RT_CK_DBI(input_dbip);
    RT_CK_RIP(rip);

    if (rip->h_dli == DB5HDR_HFLAGS_DLI_HEADER_OBJECT) return;
    if (rip->h_dli == DB5HDR_HFLAGS_DLI_FREE_STORAGE) return;

    name = (char *)rip->name.ext_buf;

    if (name == (char *)NULL) return;
    if (addr == 0) return;

    /* do not compare _GLOBAL */
    if (rip->major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
	rip->minor_type == 0)
	return;

    /* Add the prefix, if any */
    bu_vls_init(&local);
    if (db_version(dcsp->main_dbip) < 5) {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    bu_vls_strncpy(&local, bu_vls_addr(&dcsp->wdbp->wdb_prestr), dcsp->wdbp->wdb_ncharadd);
	    bu_vls_strcat(&local, name);
	} else {
	    bu_vls_strncpy(&local, name, _GED_V4_MAXNAME);
	}
	bu_vls_trunc(&local, _GED_V4_MAXNAME);
    } else {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    (void)bu_vls_vlscat(&local, &dcsp->wdbp->wdb_prestr);
	    (void)bu_vls_strcat(&local, name);
	} else {
	    (void)bu_vls_strcat(&local, name);
	}
    }

    /* Look up this new name in the existing (main) database */
    if ((dupdp = db_lookup(dcsp->main_dbip, bu_vls_addr(&local), LOOKUP_QUIET)) != RT_DIR_NULL) {
	/* Duplicate found, add it to the list */
	dcsp->wdbp->wdb_num_dups++;
	*dcsp->dup_dirp++ = dupdp;
    }

    bu_vls_free(&local);

    return;
}


/**
 * G E D _ D I R _ C H E C K
 *@brief
 * Check a name against the global directory.
 */
static int
dup_dir_check(struct db_i *input_dbip, const char *name, off_t UNUSED(laddr), size_t UNUSED(len), int UNUSED(flags), genptr_t ptr)
{
    struct directory *dupdp;
    struct bu_vls local;
    struct dir_check_stuff *dcsp = (struct dir_check_stuff *)ptr;

    if (dcsp->main_dbip == DBI_NULL)
	return 0;

    RT_CK_DBI(input_dbip);

    /* Add the prefix, if any */
    bu_vls_init(&local);
    if (db_version(dcsp->main_dbip) < 5) {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    bu_vls_strncpy(&local, bu_vls_addr(&dcsp->wdbp->wdb_prestr), dcsp->wdbp->wdb_ncharadd);
	    bu_vls_strcat(&local, name);
	} else {
	    bu_vls_strncpy(&local, name, _GED_V4_MAXNAME);
	}
	bu_vls_trunc(&local, _GED_V4_MAXNAME);
    } else {
	if (dcsp->wdbp->wdb_ncharadd > 0) {
	    bu_vls_vlscat(&local, &dcsp->wdbp->wdb_prestr);
	    bu_vls_strcat(&local, name);
	} else {
	    bu_vls_strcat(&local, name);
	}
    }

    /* Look up this new name in the existing (main) database */
    if ((dupdp = db_lookup(dcsp->main_dbip, bu_vls_addr(&local), LOOKUP_QUIET)) != RT_DIR_NULL) {
	/* Duplicate found, add it to the list */
	dcsp->wdbp->wdb_num_dups++;
	*dcsp->dup_dirp++ = dupdp;
    }
    bu_vls_free(&local);
    return 0;
}


int
ged_dup(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i *newdbp = DBI_NULL;
    struct directory **dirp0 = (struct directory **)NULL;
    struct dir_check_stuff dcs;
    static const char *usage = "file.g prefix";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bu_vls_trunc(&gedp->ged_wdbp->wdb_prestr, 0);
    if (argc == 3)
	(void)bu_vls_strcpy(&gedp->ged_wdbp->wdb_prestr, argv[2]);

    gedp->ged_wdbp->wdb_num_dups = 0;
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	if ((gedp->ged_wdbp->wdb_ncharadd = bu_vls_strlen(&gedp->ged_wdbp->wdb_prestr)) > 12) {
	    gedp->ged_wdbp->wdb_ncharadd = 12;
	    bu_vls_trunc(&gedp->ged_wdbp->wdb_prestr, 12);
	}
    } else {
	gedp->ged_wdbp->wdb_ncharadd = bu_vls_strlen(&gedp->ged_wdbp->wdb_prestr);
    }

    /* open the input file */
    if ((newdbp = db_open(argv[1], "r")) == DBI_NULL) {
	perror(argv[1]);
	bu_vls_printf(gedp->ged_result_str, "dup: Can't open %s", argv[1]);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str,
		  "\n*** Comparing %s with %s for duplicate names\n",
		  gedp->ged_wdbp->dbip->dbi_filename, argv[1]);
    if (gedp->ged_wdbp->wdb_ncharadd) {
	bu_vls_printf(gedp->ged_result_str,
		      "  For comparison, all names in %s were prefixed with: %s\n",
		      argv[1], bu_vls_addr(&gedp->ged_wdbp->wdb_prestr));
    }

    /* Get array to hold names of duplicates */
    if ((dirp0 = _ged_getspace(gedp->ged_wdbp->dbip, 0)) == (struct directory **) 0) {
	bu_vls_printf(gedp->ged_result_str, "f_dup: unable to get memory\n");
	db_close(newdbp);
	return GED_ERROR;
    }

    /* Scan new database for overlaps */
    dcs.main_dbip = gedp->ged_wdbp->dbip;
    dcs.wdbp = gedp->ged_wdbp;
    dcs.dup_dirp = dirp0;
    if (db_version(newdbp) < 5) {
	if (db_scan(newdbp, dup_dir_check, 0, (genptr_t)&dcs) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "dup: db_scan failure");
	    bu_free((genptr_t)dirp0, "_ged_getspace array");
	    db_close(newdbp);
	    return GED_ERROR;
	}
    } else {
	if (db5_scan(newdbp, dup_dir_check5, (genptr_t)&dcs) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "dup: db_scan failure");
	    bu_free((genptr_t)dirp0, "_ged_getspace array");
	    db_close(newdbp);
	    return GED_ERROR;
	}
    }
    rt_mempurge(&(newdbp->dbi_freep));        /* didn't really build a directory */

    _ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)(dcs.dup_dirp - dirp0), 0);
    bu_vls_printf(gedp->ged_result_str, "\n -----  %d duplicate names found  -----", gedp->ged_wdbp->wdb_num_dups);
    bu_free((genptr_t)dirp0, "_ged_getspace array");
    db_close(newdbp);

    return GED_OK;
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
