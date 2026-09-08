/*                         T E S T _ B O . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file test_bo.c
 *
 * Check bo host/network input type parsing and host-order export.
 */

#include "common.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"


static int failures = 0;


static void
record_failure(const char *message)
{
    bu_log("FAIL: %s\n", message);
    failures++;
}


static int
write_input_file(char *path, const void *data, size_t size)
{
    FILE *fp = bu_temp_file(path, MAXPATHLEN);

    if (!fp)
	return -1;
    if (fwrite(data, 1, size, fp) != size) {
	fclose(fp);
	return -1;
    }
    return fclose(fp);
}


static void
check_object(struct ged *gedp, const char *name, const uint16_t *expected, size_t count)
{
    struct rt_db_internal intern;
    struct rt_binunif_internal *bip;
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) {
	record_failure("bo did not create the expected object");
	return;
    }
    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0) {
	record_failure("could not read object created by bo");
	return;
    }

    bip = (struct rt_binunif_internal *)intern.idb_ptr;
    if (bip->type != DB5_MINORTYPE_BINU_16BITINT_U ||
	bip->count != count ||
	memcmp(bip->u.uint16, expected, count * sizeof(*expected)) != 0)
	record_failure("bo imported incorrect host-order values");

    rt_db_free_internal(&intern);
}


int
main(int argc, char **argv)
{
    static const uint16_t values[] = {0x0001, 0x0102, 0x1234, 0xff00};
    static const unsigned char network_values[] = {
	0x00, 0x01, 0x01, 0x02, 0x12, 0x34, 0xff, 0x00
    };
    char database_path[MAXPATHLEN] = {0};
    char host_path[MAXPATHLEN] = {0};
    char network_path[MAXPATHLEN] = {0};
    char output_path[MAXPATHLEN] = {0};
    struct rt_wdb *wdbp;
    struct ged *gedp;
    FILE *fp;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    fp = bu_temp_file(database_path, MAXPATHLEN);
    if (!fp)
	bu_exit(1, "Could not create temporary database path\n");
    fclose(fp);
    wdbp = wdb_fopen(database_path);
    if (!wdbp)
	bu_exit(1, "Could not create temporary database\n");
    db_close(wdbp->dbip);

    if (write_input_file(host_path, values, sizeof(values)) != 0 ||
	write_input_file(network_path, network_values, sizeof(network_values)) != 0)
	bu_exit(1, "Could not create bo input files\n");
    fp = bu_temp_file(output_path, MAXPATHLEN);
    if (!fp)
	bu_exit(1, "Could not create bo output path\n");
    fclose(fp);

    gedp = ged_open("db", database_path, 1);
    if (!gedp)
	bu_exit(1, "Could not open temporary database\n");

    {
	const char *av[] = {"bo", "-i", "u", "S", "default_host", host_path};
	if (ged_exec(gedp, 6, av) != BRLCAD_OK)
	    record_failure("bo rejected default host-order type syntax");
	check_object(gedp, "default_host", values, sizeof(values) / sizeof(values[0]));
    }
    {
	const char *av[] = {"bo", "-i", "u", "hS", "explicit_host", host_path};
	if (ged_exec(gedp, 6, av) != BRLCAD_OK)
	    record_failure("bo rejected explicit host-order type syntax");
	check_object(gedp, "explicit_host", values, sizeof(values) / sizeof(values[0]));
    }
    {
	const char *av[] = {"bo", "-i", "u", "nS", "network", network_path};
	if (ged_exec(gedp, 6, av) != BRLCAD_OK)
	    record_failure("bo rejected network-order type syntax");
	check_object(gedp, "network", values, sizeof(values) / sizeof(values[0]));
    }
    {
	const char *av[] = {"bo", "-i", "u", "nnS", "invalid", network_path};
	if (ged_exec(gedp, 6, av) != BRLCAD_ERROR)
	    record_failure("bo accepted an invalid input type");
    }
    {
	const char *av[] = {"bo", "-o", output_path, "network"};
	uint16_t exported[sizeof(values) / sizeof(values[0])] = {0};
	if (ged_exec(gedp, 4, av) != BRLCAD_OK) {
	    record_failure("bo failed to export a BINUNIF object");
	} else {
	    fp = fopen(output_path, "rb");
	    if (!fp || fread(exported, sizeof(exported[0]),
			      sizeof(exported) / sizeof(exported[0]), fp) !=
		       sizeof(exported) / sizeof(exported[0]) ||
		memcmp(exported, values, sizeof(values)) != 0)
		record_failure("bo export did not produce host-order values");
	    if (fp)
		fclose(fp);
	}
    }

    ged_close(gedp);
    bu_file_delete(database_path);
    bu_file_delete(host_path);
    bu_file_delete(network_path);
    bu_file_delete(output_path);

    return failures ? 1 : 0;
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
