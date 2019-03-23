/*                           C A C H E . C X X
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

/* Stress tests for the librt cache mechanism. */

#include "common.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/avs.h"
#include "bu/env.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"

/* TODO - basic single object serial API test.  Check that the cache object
 * is created, that only one object is created, that rt_shootray
 * succeeds both after initial creation and in a subsequent cache
 * read. */

/* TODO - basic single object parallel API test.  Check that the cache object
 * is created, that only one object is created, that rt_shootray
 * succeeds both after initial creation and in a subsequent cache
 * read. Shouldn't do much different than the serial API, but should work too. */

/* TODO - basic multi object, same content serial test.  Check that the cache object
 * is created, that only one object is created, that rt_shootray succeeds both
 * after initial creation and in a subsequent cache read. */

/* TODO - basic multi object, same content parallel test.  Check that the cache object
 * is created, that only one object is created, that rt_shootray succeeds both
 * after initial creation and in a subsequent cache read. */

/* TODO - basic multi object, different content serial test.  Check that the cache objects
 * are created, that the correct number of objects are created, that rt_shootray succeeds both
 * after initial creation and in a subsequent cache read. */

/* TODO - basic multi object, different content parallel test.  Check that the cache objects
 * are created, that the correct number of objects are created, that rt_shootray succeeds both
 * after initial creation and in a subsequent cache read. */



void
add_brep_sph(struct db_i *dbip, const char *name, point_t *v, double r)
{
    struct bu_external ext;

    struct directory *dp;
    struct rt_db_internal intern;
    ON_Sphere sph(*v, r);
    struct rt_brep_internal* bi;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BREP;
    intern.idb_meth = &OBJ[ID_BREP];

    BU_ALLOC(intern.idb_ptr, struct rt_brep_internal);
    bi = (struct rt_brep_internal *)intern.idb_ptr;
    bi->magic = RT_BREP_INTERNAL_MAGIC;
    bi->brep =  ON_BrepSphere(sph);

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Error: Cannot add %s to directory\n", name);
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Error: Database write error, aborting\n");
    }
}

void
cp_brep_sph(struct db_i *dbip, const char *oname, const char *nname)
{
    struct directory *odp, *ndp;
    struct bu_external external;
    int idb_type = ID_BREP;

    odp = db_lookup(dbip, oname, LOOKUP_QUIET);
    if (odp == RT_DIR_NULL) {
	bu_exit(1, "Error: Cannot copy %s - object not found\n", oname);
    }

    if (db_get_external(&external, odp, dbip)) {
	bu_exit(1, "Error: db_get_external cannot get bu_external form of %s\n", oname);
    }

    ndp = db_diradd(dbip, nname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&idb_type);

    ndp->d_flags = odp->d_flags;

    if (db_put_external(&external, ndp, dbip) < 0) {
	bu_exit(1, "Error: db_put_external cannot write %s to  %s\n", oname, nname);
    }
}

int
main(int UNUSED(argc), char *argv[])
{
    struct application ap;
    struct rt_i *rtip;
    struct db_i *dbip = DBI_NULL;
    point_t v = VINIT_ZERO;
    const char *gfile = "librt_cache_test_1.g";
    const char *cache_dir = "rt_test_cache_dir";

    bu_setenv("LIBRT_CACHE", bu_dir(NULL, 0, BU_DIR_CURR, cache_dir, NULL), 1);

    if (bu_file_exists(getenv("LIBRT_CACHE"), NULL)) {
	bu_exit(1, "Stale test cache: directory %s exists\n", getenv("LIBRT_CACHE"));
    }

    dbip = db_create(gfile, 5);

    if (dbip == DBI_NULL) {
	bu_exit(1, "Unable to create test file %s\n", gfile);
    }

    RT_CK_DBI(dbip);
    rt_init_resource(&rt_uniresource, 0, NULL);

    add_brep_sph(dbip, "0", &v, 1);
    cp_brep_sph(dbip, "0", "1");

    db_close(dbip);

    rtip = rt_dirbuild(gfile, NULL, 0);
    if (rtip == RTI_NULL) {
	bu_exit(1, "Failed to rt_dirbuild %s\n", gfile);
    }

    if (rt_gettree(rtip, "0") < 0) {
	bu_exit(1, "rt_gettree failed for object 0\n");
    }
#if 0
    if (rt_gettree(rtip, "1") < 0) {
	bu_exit(1, "rt_gettree failed for object 1\n");
    }
#endif
    rt_prep_parallel(rtip, 1);

    bu_file_delete("librt_cache_test_1.g");

    /**** Cache cleanup ****/
    {
	struct bu_vls wpath = BU_VLS_INIT_ZERO;

	/* Zap the format file first (that's the easy one) */
	bu_vls_sprintf(&wpath, "%s/format", getenv("LIBRT_CACHE"));
	bu_file_delete(bu_vls_cstr(&wpath));

	/* Now, we need to find and eliminate any cache objects */
	char **obj_dirs = NULL;
	bu_vls_sprintf(&wpath, "%s/objects", getenv("LIBRT_CACHE"));
	size_t objdir_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &obj_dirs);
	for (size_t i = 0; i < objdir_cnt; i++) {
	    /* Find and remove all files in the obj dir */
	    char **objs = NULL;
	    bu_vls_sprintf(&wpath, "%s/objects/%s", getenv("LIBRT_CACHE"), obj_dirs[i]);
	    size_t objs_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &objs);
	    for (size_t j = 0; j < objs_cnt; j++) {
		bu_vls_sprintf(&wpath, "%s/objects/%s/%s", getenv("LIBRT_CACHE"), obj_dirs[i], objs[j]);
		if (!bu_file_delete(bu_vls_cstr(&wpath))) {
		    bu_exit(1, "Unable to remove the object %s\n", bu_vls_cstr(&wpath));
		}
	    }
	    bu_argv_free(objs_cnt, objs);

	    /* Emptied the dir, now remove it */
	    bu_vls_sprintf(&wpath, "%s/objects/%s", getenv("LIBRT_CACHE"), obj_dirs[i]);
	    if (!bu_file_delete(bu_vls_cstr(&wpath))) {
		bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
	    }
	}
	bu_argv_free(objdir_cnt, obj_dirs);

	/* That should be everything - remove the objects dir and the cache dir */
	bu_vls_sprintf(&wpath, "%s/objects", getenv("LIBRT_CACHE"));
	if (!bu_file_delete(bu_vls_cstr(&wpath))) {
	    bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
	}
	bu_vls_sprintf(&wpath, "%s", getenv("LIBRT_CACHE"));
	if (!bu_file_delete(bu_vls_cstr(&wpath))) {
	    bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
	}
    }

    return 0;
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
