/*                        M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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

const char *basic_density =
"15   0.920000        Rubber\n"
"16   1.230000        Rubber, Neoprene\n";

const char *nist_to_basic =
"\"Rubber, Butyl\" Rubber\n"
"\"Rubber, Natural\" Rubber\n";

const char *basic_to_nist =
"\"Rubber\" \"Rubber, Natural\"\n";


int
check_for_data_exported(const char *filename, const char *key)
{
    struct bu_mapped_file *efile = bu_open_mapped_file(filename, "exported densities data");
    if (!strstr((char *)efile->buf, key)) {
	bu_log("Error: 'mater -d export' file %s does not contain all expected data\n", filename);
	bu_close_mapped_file(efile);
	return -1;
    }
    bu_close_mapped_file(efile);
    return 0;
}

int
check_for_data_present(struct ged *gedp, const char *key, const char *expected)
{
    const char *mater_cmd[10] = {"mater", "-d", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    mater_cmd[1] = "-d";
    mater_cmd[2] = "get";
    mater_cmd[3] = "--name";
    mater_cmd[4] = key;
    if (ged_mater(gedp, 5, (const char **)mater_cmd) != GED_OK) {
	bu_log("Error: 'mater -d get --name %s' failed\n", key);
	return -1;
    }
    if (!BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), expected)) {
	bu_log("Error: expected '%s', got: '%s'\n", expected, bu_vls_cstr(gedp->ged_result_str));
	return -1;
    }
    return 0;
}


int
check_for_data_not_present(struct ged *gedp, const char *key)
{
    const char *mater_cmd[10] = {"mater", "-d", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    mater_cmd[1] = "-d";
    mater_cmd[2] = "get";
    mater_cmd[3] = "--name";
    mater_cmd[4] = key;
    if (ged_mater(gedp, 5, (const char **)mater_cmd) != GED_OK) {
	bu_log("Error: 'mater -d get --name %s' failed\n", key);
	return -1;
    }
    if (bu_vls_strlen(gedp->ged_result_str)) {
	bu_log("Error: unexpected result found for key %s: '%s'\n", key, bu_vls_cstr(gedp->ged_result_str));
	return -1;
    }
    return 0;
}

int
attr_val_check(struct ged *gedp, const char *obj, const char *key, const char *expected)
{
    const char *attr_cmd[5] = {"attr", "get", NULL, NULL, NULL};
    attr_cmd[2] = obj;
    attr_cmd[3] = key;
    if (ged_attr(gedp, 4, (const char **)attr_cmd) != GED_OK) {
	bu_log("Error: 'attr get %s %s' failed\n", obj, key);
	return -1;
    }
    if (!BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), expected)) {
	bu_log("Error: expected '%s', got: '%s'\n", expected, bu_vls_cstr(gedp->ged_result_str));
	return -1;
    }
    return 0;
}


int
main(int ac, char *av[]) {
    struct ged *gedp;
    const char *gname = "ged_mater_test.g";
    const char *exp_data = "ged_mater_density_export.txt";
    const char *d_data = "ged_mater_density_data.txt";
    const char *m_data = "ged_mater_density_map.txt";
    const char *b_data = "ged_mater_density_reverse_map.txt";
    char mdata[MAXPATHLEN];
    const char *mater_cmd[10] = {"mater", "-d", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const char *make_cmd[10] = {"make", NULL, "sph", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const char *reg_cmd[10] = {"r", NULL, "u", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const char *attr_cmd[10] = {"attr", "set", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const char *mat1 = "1,0.0001,Material01";
    const char *mat2 = "2,1.1e-1,Material02";
    const char *mat1_reassign = "1,2.0,Material 03";

    /* Need this for bu_dir to work correctly */
    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s test_name\n", av[0]);
	return 1;
    }

    if (bu_file_exists(gname, NULL)) {
	printf("Error: %s already exists\n", gname);
	return 1;
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
	if (check_for_data_exported(exp_data, "Xylene")) {
	    goto ged_test_fail;
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "clear";
	if (ged_mater(gedp, 3, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d clear' failed\n");
	    goto ged_test_fail;
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "set";
	mater_cmd[3] = mat1;
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d set %s' failed\n", mat1);
	    goto ged_test_fail;
	}
	if (check_for_data_present(gedp, "Material01", "1	0.0001	Material01\n")) {
	    goto ged_test_fail;
	}
	if (check_for_data_not_present(gedp, "Xylene")) {
	    goto ged_test_fail;
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "set";
	mater_cmd[3] = mat2;
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d set %s' failed\n", mat2);
	    goto ged_test_fail;
	}
	if (check_for_data_present(gedp, "Material01", "1	0.0001	Material01\n")) {
	    goto ged_test_fail;
	}
	if (check_for_data_present(gedp, "Material02", "2	0.11	Material02\n")) {
	    goto ged_test_fail;
	}

	mater_cmd[1] = "-d";
	mater_cmd[2] = "set";
	mater_cmd[3] = mat1_reassign;
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d set %s' failed\n", mat1_reassign);
	    goto ged_test_fail;
	}
	if (check_for_data_present(gedp, "Material02", "2	0.11	Material02\n")) {
	    goto ged_test_fail;
	}
	if (check_for_data_present(gedp, "Material 03", "1	2	Material 03\n")) {
	    goto ged_test_fail;
	}
	if (check_for_data_not_present(gedp, "Material01")) {
	    goto ged_test_fail;
	}

	// Now, test the mapping logic

	mater_cmd[1] = "-d";
	mater_cmd[2] = "clear";
	if (ged_mater(gedp, 3, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d clear' failed\n");
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

	// Make the initial geometry objects on which the mappings will
	// operate.
	make_cmd[1] = "sph1.s";
	if (ged_make(gedp, 3, (const char **)make_cmd) != GED_OK) {
	    bu_log("Error: failed to make object\n");
	    goto ged_test_fail;
	}
	make_cmd[1] = "sph2.s";
	if (ged_make(gedp, 3, (const char **)make_cmd) != GED_OK) {
	    bu_log("Error: failed to make object\n");
	    goto ged_test_fail;
	}
	make_cmd[1] = "sph3.s";
	if (ged_make(gedp, 3, (const char **)make_cmd) != GED_OK) {
	    bu_log("Error: failed to make object\n");
	    goto ged_test_fail;
	}
	reg_cmd[1] = "reg1.r";
	reg_cmd[3] = "sph1.s";
	if (ged_region(gedp, 4, (const char **)reg_cmd) != GED_OK) {
	    bu_log("Error: failed to make region\n");
	    goto ged_test_fail;
	}
	reg_cmd[1] = "reg2.r";
	reg_cmd[3] = "sph2.s";
	if (ged_region(gedp, 4, (const char **)reg_cmd) != GED_OK) {
	    bu_log("Error: failed to make region\n");
	    goto ged_test_fail;
	}
	reg_cmd[1] = "reg3.r";
	reg_cmd[3] = "sph3.s";
	if (ged_region(gedp, 4, (const char **)reg_cmd) != GED_OK) {
	    bu_log("Error: failed to make region\n");
	    goto ged_test_fail;
	}

	// Set the seed attributes.
	attr_cmd[2] = "reg1.r";
	attr_cmd[3] = "material_id";
	attr_cmd[4] = "12242";
	if (ged_attr(gedp, 5, (const char **)attr_cmd) != GED_OK) {
	    bu_log("Error: failed to set attribute\n");
	    goto ged_test_fail;
	}
	attr_cmd[2] = "reg2.r";
	attr_cmd[3] = "material_name";
	attr_cmd[4] = "Rubber, Natural";
	if (ged_attr(gedp, 5, (const char **)attr_cmd) != GED_OK) {
	    bu_log("Error: failed to set attribute\n");
	    goto ged_test_fail;
	}
	attr_cmd[2] = "reg3.r";
	attr_cmd[3] = "material_id";
	attr_cmd[4] = "12244";
	if (ged_attr(gedp, 5, (const char **)attr_cmd) != GED_OK) {
	    bu_log("Error: failed to set attribute\n");
	    goto ged_test_fail;
	}

	// In-memory name updates.
	mater_cmd[1] = "-d";
	mater_cmd[2] = "map";
	mater_cmd[3] = "--names-from-ids";
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d map --names-from-ids' failed to run\n");
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg1.r", "material_name", "Rubber, Butyl")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg2.r", "material_name", "Rubber, Natural")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg3.r", "material_name", "Rubber, Neoprene")) {
	    goto ged_test_fail;
	}

	// In-memory id updates.
	mater_cmd[1] = "-d";
	mater_cmd[2] = "map";
	mater_cmd[3] = "--ids-from-names";
	if (ged_mater(gedp, 4, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d map --ids-from-names' failed to run\n");
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg1.r", "material_id", "12242")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg2.r", "material_id", "12243")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg3.r", "material_id", "12244")) {
	    goto ged_test_fail;
	}

	// Create files to use for mapping inputs
	{
	    FILE *dd, *dm, *db;
	    if (((dd = fopen(d_data, "wb")) == NULL) ||
		    ((dm = fopen(m_data, "wb")) == NULL) || 
		    ((db = fopen(b_data, "wb")) == NULL)) {
		bu_log("Error: could not open mapping input files for writing\n");
		goto ged_test_fail;
	    }
	    fprintf(dd, "%s", basic_density);
	    fprintf(dm, "%s", nist_to_basic);
	    fprintf(db, "%s", basic_to_nist);
	    (void)fclose(dd);
	    (void)fclose(dm);
	    (void)fclose(db);
	}

	// File-based id updates.
	mater_cmd[1] = "-d";
	mater_cmd[2] = "map";
	mater_cmd[3] = "--ids-from-names";
	mater_cmd[4] = d_data;
	if (ged_mater(gedp, 5, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d map --ids-from-names %s' failed to run\n", d_data);
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg1.r", "material_id", "12242")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg2.r", "material_id", "12243")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg3.r", "material_id", "16")) {
	    goto ged_test_fail;
	}


	// File-based id updates, with map file.
	mater_cmd[1] = "-d";
	mater_cmd[2] = "map";
	mater_cmd[3] = "--ids-from-names";
	mater_cmd[4] = d_data;
	mater_cmd[5] = m_data;
	if (ged_mater(gedp, 6, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d map --ids-from-names %s %s' failed to run\n", d_data, m_data);
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg1.r", "material_id", "15")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg2.r", "material_id", "15")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg3.r", "material_id", "16")) {
	    goto ged_test_fail;
	}

	// File-based name updates.
	mater_cmd[1] = "-d";
	mater_cmd[2] = "map";
	mater_cmd[3] = "--names-from-ids";
	mater_cmd[4] = d_data;
	if (ged_mater(gedp, 5, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d map --names-from-ids %s' failed to run\n", d_data);
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg1.r", "material_name", "Rubber")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg2.r", "material_name", "Rubber")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg3.r", "material_name", "Rubber, Neoprene")) {
	    goto ged_test_fail;
	}


	// File-based id updates, with map file, after name update.
	mater_cmd[1] = "-d";
	mater_cmd[2] = "map";
	mater_cmd[3] = "--ids-from-names";
	mater_cmd[4] = d_data;
	mater_cmd[5] = m_data;
	if (ged_mater(gedp, 5, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d map --ids-from-names %s %s' failed to run\n", d_data, m_data);
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg1.r", "material_id", "15")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg2.r", "material_id", "15")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg3.r", "material_id", "16")) {
	    goto ged_test_fail;
	}


	// Map file only id updates.
	mater_cmd[1] = "-d";
	mater_cmd[2] = "map";
	mater_cmd[3] = "--ids-from-names";
	mater_cmd[4] = b_data;
	if (ged_mater(gedp, 5, (const char **)mater_cmd) != GED_OK) {
	    bu_log("Error: 'mater -d map --ids-from-names %s' failed to run\n", b_data);
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg1.r", "material_id", "12243")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg2.r", "material_id", "12243")) {
	    goto ged_test_fail;
	}
	if (attr_val_check(gedp, "reg3.r", "material_id", "12244")) {
	    goto ged_test_fail;
	}


    }

    ged_close(gedp);
    return 0;

ged_test_fail:
    ged_close(gedp);
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
