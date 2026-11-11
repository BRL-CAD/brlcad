/*              D B _ O P E N _ M O D E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#include "common.h"

#include <vector>

#include "bio.h"
#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/log.h"
#include "raytrace.h"
#include "wdb.h"

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    char filename[MAXPATHLEN];
    FILE *temporary = bu_temp_file(filename, sizeof(filename));
    if (!temporary)
	return 1;
    fclose(temporary);
    struct rt_wdb *writer = wdb_fopen(filename);
    if (!writer)
	return 1;
    wdb_close(writer);

    std::vector<struct db_i *> opened;
    auto remember = [&opened](struct db_i *db) {
	opened.push_back(db);
	return db;
    };
    bool ok = bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "0", 1) == 0;
    auto *enabled = remember(db_open_ex(filename, DB_OPEN_READONLY, DB_OPEN_COMB_INSTANCES));
    auto *enabled_write = remember(db_open_ex(filename, DB_OPEN_READWRITE, DB_OPEN_COMB_INSTANCES));
    ok = ok && enabled && db_uses_instance_specifiers(enabled) &&
	enabled_write && db_uses_instance_specifiers(enabled_write);

    ok = bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1) == 0 && ok;
    auto *disabled = remember(db_open_ex(filename, DB_OPEN_READONLY, DB_OPEN_DEFAULT));
    auto *disabled_write = remember(db_open_ex(filename, DB_OPEN_READWRITE, DB_OPEN_DEFAULT));
    auto *legacy = remember(db_open(filename, DB_OPEN_READONLY));
    ok = ok && disabled && !db_uses_instance_specifiers(disabled) &&
	disabled_write && !db_uses_instance_specifiers(disabled_write) &&
	legacy && db_uses_instance_specifiers(legacy);

    ok = bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "0", 1) == 0 && ok;
    auto *enabled_again = remember(db_open_ex(filename, DB_OPEN_READONLY, DB_OPEN_COMB_INSTANCES));
    auto *disabled_again = remember(db_open_ex(filename, DB_OPEN_READONLY, DB_OPEN_DEFAULT));
    auto *legacy_again = remember(db_open(filename, DB_OPEN_READONLY));
    ok = ok && enabled_again == enabled && disabled_again == disabled &&
	legacy_again == legacy && db_uses_instance_specifiers(legacy_again) &&
	enabled != disabled && enabled != legacy && disabled != legacy;

    char title[MAXPATHLEN];
    struct rt_i *indexed = rt_dirbuild_ex(filename, title, sizeof(title), DB_OPEN_COMB_INSTANCES);
    ok = ok && indexed && db_uses_instance_specifiers(indexed->rti_dbip) && title[0];
    struct rt_i *plain = rt_dirbuild_ex(filename, NULL, 0, DB_OPEN_DEFAULT);
    ok = ok && plain && !db_uses_instance_specifiers(plain->rti_dbip);
    auto *invalid = remember(db_open_ex(filename, DB_OPEN_READONLY, DB_OPEN_COMB_INSTANCES << 1));
    struct rt_i *invalid_rt = rt_dirbuild_ex(filename, NULL, 0, DB_OPEN_COMB_INSTANCES << 1);
    ok = ok && !invalid && !invalid_rt;

    if (invalid_rt) rt_i_destroy(invalid_rt);
    if (plain) rt_i_destroy(plain);
    if (indexed) rt_i_destroy(indexed);
    for (auto *db : opened)
	if (db) db_close(db);
    if (!bu_file_delete(filename)) {
	bu_log("Could not remove temporary database\n");
	ok = false;
    }
    if (!ok)
	bu_log("Explicit database instance policy or cache isolation was not honored\n");
    return ok ? 0 : 1;
}
