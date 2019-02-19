/*                        M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2019 United States Government as represented by
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
/** @file mater.c
 *
 * Brief description
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <bu.h>
#include <ged.h>

int
check_for_data(const char *filename, const char *key)
{
    struct bu_mapped_file *efile = bu_open_mapped_file(filename, "exported densities data");
    if (!strstr((char *)efile->buf, key)) {
	bu_log("Error: 'mater -d export' file %s does not contain all expected data\n", filename);
	bu_close_mapped_file(efile);
	return -1;
    }
    bu_close_mapped_file(efile);
    if (!bu_file_delete(filename)) {
	bu_log("Error deleting %s\n", filename);
	return -1;
    }
    return 0;
}



int
main(int ac, char *av[]) {
    struct ged *gedp;
    const char *gname = "ged_mater_test.g";
    const char *exp_data = "ged_mater_density_export.txt";
    char mdata[MAXPATHLEN];
    const char *mater_cmd[10] = {"mater", "-d", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

    if (ac != 2) {
	printf("Usage: %s test_name\n", av[0]);
	return 1;
    }

    if (bu_file_exists(gname, NULL)) {
	printf("ERROR: %s already exists, aborting\n", gname);
	return 2;
    }


    gedp = ged_open("db", gname, 0);

    if (BU_STR_EQUAL(av[1], "dnull")) {
	if (ged_mater(gedp, 2, (const char **)mater_cmd) != GED_HELP) {
	    bu_log("Error: bare 'mater -d' doesn't return GED_HELP\n");
	    goto ged_test_fail;
	}
    }

    if (BU_STR_EQUAL(av[1], "dstd")) {

	if (bu_file_exists(exp_data, NULL)) {
	    printf("ERROR: %s already exists, aborting\n", exp_data);
	    return 2;
	}

       	(void)bu_dir(mdata, MAXPATHLEN, BU_DIR_DATA, "data", "NIST_DENSITIES", NULL);
	if (!bu_file_exists(mdata, NULL)) {
	    bu_log("Error: density file %s not found.\n", mdata);
	    goto ged_test_fail;
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "validate";
	mater_cmd[3] = mdata;
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d import' failed to validate %s\n", mdata);
	    goto ged_test_fail;
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "import";
	mater_cmd[3] = "-v";
	mater_cmd[4] = mdata;
	if (ged_mater(gedp, 5, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d import' failed to load %s\n", mdata);
	    goto ged_test_fail;
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "source";
	mater_cmd[3] = NULL;
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d source' failed to run correctly\n");
	    goto ged_test_fail;
	} else {
	    if (bu_strncmp(bu_vls_cstr(gedp->ged_result_str), gedp->ged_wdbp->dbip->dbi_filename, strlen(gedp->ged_wdbp->dbip->dbi_filename))) {
		bu_log("Error: 'mater -d source' reports a location of %s instead of %s\n", bu_vls_cstr(gedp->ged_result_str), gedp->ged_wdbp->dbip->dbi_filename);
		goto ged_test_fail;
	    }
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "export";
	mater_cmd[3] = exp_data;
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK || !bu_file_exists(exp_data, NULL)) {
	    bu_log("Error: 'mater -d export' failed to export to %s\n", exp_data);
	    goto ged_test_fail;
	}
	if (check_for_data(exp_data, "Xylene")) {
	    goto ged_test_fail;
	}
    }

    ged_close(gedp);
    BU_PUT(gedp, struct ged);
    bu_file_delete(gname);

    return 0;

ged_test_fail:
    ged_close(gedp);
    BU_PUT(gedp, struct ged);
    bu_file_delete(gname);
    return 1;
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
