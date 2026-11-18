/*                    D B _ I D E N T . C
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

#include <math.h>
#include <stdio.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "rt/db_attr.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/wdb.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("Database identification check failed at line %d: %s\n", __LINE__, #condition); \
	failed = 1; \
	goto done; \
    } \
} while (0)

static int
test_version(int version)
{
    char path[MAXPATHLEN] = {0};
    struct db_i *database = NULL;
    struct rt_wdb *writer = NULL;
    FILE *temporary = NULL;
    const char *title = "Database identification test";
    const double invalid_scales[] = {0, -1, INFINITY, NAN};
    int failed = 0;
    int close_status;

    temporary = bu_temp_file(path, sizeof(path));
    CHECK(temporary);
    close_status = fclose(temporary);
    temporary = NULL;
    CHECK(!close_status);
    writer = wdb_fopen_v(path, version);
    CHECK(writer);
    database = writer->dbip;
    CHECK(db_update_ident(database, title, 10) == 0);
    CHECK(BU_STR_EQUAL(database->dbi_title, title));
    CHECK(EQUAL(database->dbi_local2base, 10) && EQUAL(database->dbi_base2local, 0.1));
    for (size_t i = 0; i < sizeof(invalid_scales) / sizeof(invalid_scales[0]); ++i) {
	CHECK(db_update_ident(database, "invalid scale", invalid_scales[i]) < 0);
	CHECK(BU_STR_EQUAL(database->dbi_title, title) && EQUAL(database->dbi_local2base, 10));
    }
    db_close(database);
    database = db_open(path, DB_OPEN_READONLY);
    CHECK(database && !db_dirbuild(database));
    CHECK(BU_STR_EQUAL(database->dbi_title, title));
    CHECK(EQUAL(database->dbi_local2base, 10) && EQUAL(database->dbi_base2local, 0.1));
    CHECK(db_update_ident(database, "failed write", 1000) < 0);
    CHECK(BU_STR_EQUAL(database->dbi_title, title));
    CHECK(EQUAL(database->dbi_local2base, 10) && EQUAL(database->dbi_base2local, 0.1));

done:
    if (temporary) fclose(temporary);
    if (database) db_close(database);
    if (path[0]) bu_file_delete(path);
    return failed;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    return test_version(4) || test_version(5);
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
