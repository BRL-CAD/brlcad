/*                    I N M E M _ W R I T E . C
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/avs.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


static int
write_sphere(struct directory *dp, struct db_i *dbip, fastf_t radius, const char *note)
{
    struct rt_db_internal intern;
    struct rt_ell_internal *sph;

    RT_DB_INTERNAL_INIT(&intern);
    BU_ALLOC(sph, struct rt_ell_internal);
    sph->magic = RT_ELL_INTERNAL_MAGIC;
    VSETALL(sph->v, 0.0);
    VSET(sph->a, radius, 0.0, 0.0);
    VSET(sph->b, 0.0, radius, 0.0);
    VSET(sph->c, 0.0, 0.0, radius);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_SPH;
    intern.idb_meth = &OBJ[ID_SPH];
    intern.idb_ptr = sph;
    if (note)
	bu_avs_add(&intern.idb_avs, "note", note);

    return rt_db_put_internal(dp, dbip, &intern);
}


static int
check_sphere(struct directory *dp, struct db_i *dbip, fastf_t radius, const char *note)
{
    struct rt_db_internal intern;
    const struct rt_ell_internal *sph;
    const char *actual_note;
    int id;
    int ret = 0;

    id = rt_db_get_internal(&intern, dp, dbip, NULL);
    if (id != ID_SPH && id != ID_ELL) {
	bu_log("%s: expected a sphere, got type %d\n", dp->d_namep, id);
	if (id >= 0)
	    rt_db_free_internal(&intern);
	return 1;
    }

    sph = (const struct rt_ell_internal *)intern.idb_ptr;
    actual_note = bu_avs_get(&intern.idb_avs, "note");
    if (!EQUAL(sph->a[0], radius) || !EQUAL(sph->b[1], radius) ||
	!EQUAL(sph->c[2], radius) ||
	(note && (!actual_note || !BU_STR_EQUAL(actual_note, note))) ||
	(!note && actual_note)) {
	bu_log("%s: stored geometry or attributes differ\n", dp->d_namep);
	ret = 1;
    }

    rt_db_free_internal(&intern);
    return ret;
}


static int
check_disk_write(void)
{
    char path[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(path, sizeof(path));
    struct db_i *dbip = NULL;
    struct directory *dp;
    unsigned char type = ID_SPH;
    int ret = 1;

    if (!fp)
	return 1;
    if (fclose(fp) != 0)
	goto done;

    dbip = db_create(path, 5);
    if (!dbip)
	goto done;
    dp = db_diradd(dbip, "disk.o", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, &type);
    if (!dp || write_sphere(dp, dbip, 7.0, NULL))
	goto done;
    db_close(dbip);
    dbip = db_open(path, "r");
    if (!dbip || db_dirbuild(dbip) < 0)
	goto done;
    dp = db_lookup(dbip, "disk.o", LOOKUP_QUIET);
    if (dp && !check_sphere(dp, dbip, 7.0, NULL))
	ret = 0;

done:
    if (dbip)
	db_close(dbip);
    if (!bu_file_delete(path))
	ret = 1;
    return ret;
}

int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;
    struct directory *copy_dp;
    struct rt_wdb *wdbp;
    struct bu_external ext;
    unsigned char type = ID_SPH;
    const char *note = "in-memory replacement with attributes";
    size_t initial_len;
    size_t wide_len;
    point_t center = VINIT_ZERO;
    enum { REWRITE_COUNT = 32, NOTE_BYTES = 512, WIDE_NOTE_BYTES = 65536 };
    char long_note[NOTE_BYTES + 1];
    char wide_note[WIDE_NOTE_BYTES + 1];
    int ret = 1;

    if (argc != 1)
	return 1;
    bu_setprogname(argv[0]);
    dbip = db_create_inmem();
    if (!dbip)
	return 1;
    BU_EXTERNAL_INIT(&ext);

    dp = db_diradd(dbip, "sph.o", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, &type);
    if (!dp || write_sphere(dp, dbip, 1.0, NULL) ||
	!(dp->d_flags & RT_DIR_INMEM) || !dp->d_un.ptr ||
	check_sphere(dp, dbip, 1.0, NULL))
	goto done;

    initial_len = dp->d_len;
    if (write_sphere(dp, dbip, 2.0, NULL) || dp->d_len != initial_len ||
	check_sphere(dp, dbip, 2.0, NULL))
	goto done;

    if (write_sphere(dp, dbip, 3.0, note) ||
	dp->d_len <= initial_len ||
	check_sphere(dp, dbip, 3.0, note))
	goto done;

    if (write_sphere(dp, dbip, 4.0, NULL) || dp->d_len != initial_len ||
	check_sphere(dp, dbip, 4.0, NULL))
	goto done;

    if (db_get_external(&ext, dp, dbip) < 0)
	goto done;
    copy_dp = db_diradd(dbip, "copy.o", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, &type);
    if (!copy_dp || db_put_external(&ext, copy_dp, dbip) < 0 ||
	check_sphere(copy_dp, dbip, 4.0, NULL))
	goto done;

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp || mk_sph(wdbp, "wdb.o", center, 5.0))
	goto done;
    dp = db_lookup(dbip, "wdb.o", LOOKUP_QUIET);
    if (!dp || check_sphere(dp, dbip, 5.0, NULL))
	goto done;

    if (db_dirdelete(dbip, copy_dp) < 0)
	goto done;
    copy_dp = db_diradd(dbip, "reused.o", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, &type);
    if (!copy_dp || write_sphere(copy_dp, dbip, 6.0, NULL) ||
	check_sphere(copy_dp, dbip, 6.0, NULL))
	goto done;

    memset(long_note, 'x', NOTE_BYTES);
    long_note[NOTE_BYTES] = '\0';
    dp = db_lookup(dbip, "sph.o", LOOKUP_QUIET);
    if (!dp)
	goto done;
    for (int i = 0; i < REWRITE_COUNT; i++) {
	const char *current_note = (i % 2) ? NULL : long_note;
	fastf_t radius = (fastf_t)(i + 1);

	if (write_sphere(dp, dbip, radius, current_note) ||
	    check_sphere(dp, dbip, radius, current_note)) {
	    bu_log("rewrite %d failed\n", i);
	    goto done;
	}
    }

    memset(wide_note, 'x', WIDE_NOTE_BYTES);
    wide_note[WIDE_NOTE_BYTES] = '\0';
    if (write_sphere(dp, dbip, (fastf_t)(REWRITE_COUNT + 1), wide_note) ||
	check_sphere(dp, dbip, (fastf_t)(REWRITE_COUNT + 1), wide_note) ||
	dp->d_len <= UINT16_MAX)
	goto done;
    wide_len = dp->d_len;
    if (write_sphere(dp, dbip, (fastf_t)(REWRITE_COUNT + 2), NULL) ||
	check_sphere(dp, dbip, (fastf_t)(REWRITE_COUNT + 2), NULL) ||
	dp->d_len >= wide_len)
	goto done;

    ret = 0;

done:
    if (ret)
	bu_log("in-memory database write test failed\n");
    bu_free_external(&ext);
    db_close(dbip);
    if (!ret && check_disk_write()) {
	bu_log("file-backed database control test failed\n");
	ret = 1;
    }
    return ret;
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
