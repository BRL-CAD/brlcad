/*                      B I N U N I F . C
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
/** @file binunif.c
 *
 * Exercise BINUNIF host/network conversion and typed file import.
 */

#include "common.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "raytrace.h"
#include "rt/binunif.h"
#include "wdb.h"


#define TEST_UNKNOWN_BINUNIF_FLAG 0x0200u

static int failures = 0;


static void
record_failure(const char *message)
{
    bu_log("FAIL: %s\n", message);
    failures++;
}


static void
check_object(struct db_i *dbip, const char *name, unsigned int minor_type,
	     const void *expected, size_t count, const unsigned char *external,
	     size_t external_size)
{
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    struct rt_db_internal intern;
    struct rt_binunif_internal *bip;
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    size_t item_size = db5_type_sizeof_h_binu(minor_type);

    if (dp == RT_DIR_NULL) {
	record_failure("expected BINUNIF object is missing");
	return;
    }
    if (dp->d_major_type != DB5_MAJORTYPE_BINARY_UNIF)
	record_failure("BINUNIF directory entry has the wrong major type");
    if (!(dp->d_flags & RT_DIR_NON_GEOM))
	record_failure("BINUNIF directory entry is marked as geometry");

    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
	record_failure("could not import BINUNIF object");
	return;
    }

    bip = (struct rt_binunif_internal *)intern.idb_ptr;
    if (bip->type != (int)minor_type)
	record_failure("BINUNIF minor type changed during round trip");
    if (bip->count != count)
	record_failure("BINUNIF element count changed during round trip");
    if (bip->count == count && memcmp(bip->u.uint8, expected, count * item_size) != 0)
	record_failure("BINUNIF host-order values changed during round trip");

    if (external) {
	if (!intern.idb_meth->ft_export5 ||
	    intern.idb_meth->ft_export5(&ext, &intern, 1.0, dbip) != 0) {
	    record_failure("could not export BINUNIF object");
	} else if (ext.ext_nbytes != external_size ||
		   memcmp(ext.ext_buf, external, external_size) != 0) {
	    record_failure("BINUNIF external data is not in network order");
	}
	bu_free_external(&ext);
    }

    rt_db_free_internal(&intern);
}


static int
write_test_files(char *host_path, char *network_path,
		 const uint16_t *values, size_t count,
		 const unsigned char *network_values)
{
    FILE *host_file = bu_temp_file(host_path, MAXPATHLEN);
    FILE *network_file = bu_temp_file(network_path, MAXPATHLEN);
    int host_close;
    int network_close;

    if (!host_file || !network_file) {
	if (host_file)
	    fclose(host_file);
	if (network_file)
	    fclose(network_file);
	return -1;
    }

    if (fwrite(values, sizeof(*values), count, host_file) != count ||
	fputc(0x5a, host_file) == EOF ||
	fwrite(network_values, sizeof(*network_values),
	       count * sizeof(*values), network_file) != count * sizeof(*values) ||
	fputc(0x5a, network_file) == EOF) {
	fclose(host_file);
	fclose(network_file);
	return -1;
    }

    host_close = fclose(host_file);
    network_close = fclose(network_file);
    if (host_close != 0 || network_close != 0)
	return -1;

    return 0;
}


int
main(int argc, char **argv)
{
    static const uint8_t uint8_values[] = {0x01, 0x80, 0xff};
    static const int8_t int8_values[] = {1, -2, 127};
    static const uint16_t uint16_values[] = {0x0001, 0x0102, 0x1234, 0xff00};
    static const int16_t int16_values[] = {1, -2, 0x1234, -256};
    static const uint32_t uint32_values[] = {0x00000001, 0x01020304, 0xff000001};
    static const int32_t int32_values[] = {1, -2, 0x12345678, -65536};
    static const uint64_t uint64_values[] = {
	UINT64_C(0x0000000000000001),
	UINT64_C(0x0102030405060708),
	UINT64_C(0xff00000000000001)
    };
    static const int64_t int64_values[] = {
	INT64_C(1), -INT64_C(2), INT64_C(0x0123456789abcdef), -INT64_C(65536)
    };
    static const float float_values[] = {1.25f, -2.5f, 4096.0f};
    static const double double_values[] = {1.25, -2.5, 4096.0};
    static const unsigned char uint16_network[] = {
	0x00, 0x01, 0x01, 0x02, 0x12, 0x34, 0xff, 0x00
    };
    struct test_case {
	const char *name;
	const void *values;
	size_t count;
	wdb_binunif wdb_type;
	unsigned int minor_type;
    } cases[] = {
	{"uint8", uint8_values, sizeof(uint8_values) / sizeof(uint8_values[0]),
	    WDB_BINUNIF_UINT8, DB5_MINORTYPE_BINU_8BITINT_U},
	{"int8", int8_values, sizeof(int8_values) / sizeof(int8_values[0]),
	    WDB_BINUNIF_INT8, DB5_MINORTYPE_BINU_8BITINT},
	{"uint16", uint16_values, sizeof(uint16_values) / sizeof(uint16_values[0]),
	    WDB_BINUNIF_UINT16, DB5_MINORTYPE_BINU_16BITINT_U},
	{"int16", int16_values, sizeof(int16_values) / sizeof(int16_values[0]),
	    WDB_BINUNIF_INT16, DB5_MINORTYPE_BINU_16BITINT},
	{"uint32", uint32_values, sizeof(uint32_values) / sizeof(uint32_values[0]),
	    WDB_BINUNIF_UINT32, DB5_MINORTYPE_BINU_32BITINT_U},
	{"int32", int32_values, sizeof(int32_values) / sizeof(int32_values[0]),
	    WDB_BINUNIF_INT32, DB5_MINORTYPE_BINU_32BITINT},
	{"uint64", uint64_values, sizeof(uint64_values) / sizeof(uint64_values[0]),
	    WDB_BINUNIF_UINT64, DB5_MINORTYPE_BINU_64BITINT_U},
	{"int64", int64_values, sizeof(int64_values) / sizeof(int64_values[0]),
	    WDB_BINUNIF_INT64, DB5_MINORTYPE_BINU_64BITINT},
	{"float", float_values, sizeof(float_values) / sizeof(float_values[0]),
	    WDB_BINUNIF_FLOAT, DB5_MINORTYPE_BINU_FLOAT},
	{"double", double_values, sizeof(double_values) / sizeof(double_values[0]),
	    WDB_BINUNIF_DOUBLE, DB5_MINORTYPE_BINU_DOUBLE}
    };
    char host_path[MAXPATHLEN] = {0};
    char network_path[MAXPATHLEN] = {0};
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    size_t i;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    dbip = db_open_inmem();
    if (!dbip)
	bu_exit(1, "Could not create an in-memory database\n");
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	bu_exit(1, "Could not create a database writer\n");

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
	if (mk_binunif(wdbp, cases[i].name, cases[i].values,
			cases[i].wdb_type, (long)cases[i].count) != 0) {
	    record_failure("mk_binunif failed for host-order memory");
	    continue;
	}
	check_object(dbip, cases[i].name, cases[i].minor_type,
		     cases[i].values, cases[i].count,
		     cases[i].minor_type == DB5_MINORTYPE_BINU_16BITINT_U ? uint16_network : NULL,
		     sizeof(uint16_network));
    }

    if (write_test_files(host_path, network_path, uint16_values,
			 sizeof(uint16_values) / sizeof(uint16_values[0]),
			 uint16_network) != 0)
	bu_exit(1, "Could not create BINUNIF test files\n");

    if (rt_mk_binunif(wdbp, "host_file", host_path,
		      DB5_MINORTYPE_BINU_16BITINT_U, 0) != 0)
	record_failure("rt_mk_binunif rejected host-order file input");
    check_object(dbip, "host_file", DB5_MINORTYPE_BINU_16BITINT_U,
		 uint16_values, sizeof(uint16_values) / sizeof(uint16_values[0]),
		 uint16_network, sizeof(uint16_network));

    if (rt_mk_binunif(wdbp, "network_file", network_path,
		      DB5_MINORTYPE_BINU_16BITINT_U | RT_BINUNIF_NETWORK_ORDER, 0) != 0)
	record_failure("rt_mk_binunif rejected network-order file input");
    check_object(dbip, "network_file", DB5_MINORTYPE_BINU_16BITINT_U,
		 uint16_values, sizeof(uint16_values) / sizeof(uint16_values[0]),
		 uint16_network, sizeof(uint16_network));

    if (mk_binunif(wdbp, "wdb_network_file", network_path,
		   (wdb_binunif)(WDB_BINUNIF_FILE_UINT16 | WDB_BINUNIF_NETWORK_ORDER), 0) != 0)
	record_failure("mk_binunif rejected network-order file input");
    check_object(dbip, "wdb_network_file", DB5_MINORTYPE_BINU_16BITINT_U,
		 uint16_values, sizeof(uint16_values) / sizeof(uint16_values[0]),
		 uint16_network, sizeof(uint16_network));

    if (rt_mk_binunif(wdbp, "limited_file", host_path,
		      DB5_MINORTYPE_BINU_16BITINT_U, 3) != 0)
	record_failure("rt_mk_binunif rejected a file count limit");
    check_object(dbip, "limited_file", DB5_MINORTYPE_BINU_16BITINT_U,
		 uint16_values, 3, uint16_network, 3 * sizeof(uint16_t));

    if (mk_binunif(wdbp, "invalid_memory_order", uint16_values,
		   (wdb_binunif)(WDB_BINUNIF_UINT16 | WDB_BINUNIF_NETWORK_ORDER),
		   (long)(sizeof(uint16_values) / sizeof(uint16_values[0]))) == 0)
	record_failure("mk_binunif accepted network-order memory input");

    if (rt_mk_binunif(wdbp, "invalid_file_flags", host_path,
		      DB5_MINORTYPE_BINU_16BITINT_U | TEST_UNKNOWN_BINUNIF_FLAG, 0) == 0)
	record_failure("rt_mk_binunif accepted an unknown input flag");

    bu_file_delete(host_path);
    bu_file_delete(network_path);
    db_close(dbip);

    if (failures)
	bu_log("BINUNIF tests failed: %d\n", failures);

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
