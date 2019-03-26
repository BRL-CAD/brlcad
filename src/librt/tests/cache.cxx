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

const char *RTC_PREFIX = "rt_cache_test";

static void
add_brep_sph(struct db_i *dbip, const char *name, point_t *v, double r, long int test_num)
{
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
	bu_exit(1, "Test %ld: cannot add %s to directory\n", test_num, name);
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: database write error, aborting\n", test_num);
    }
    rt_db_free_internal(&intern);
}

static void
cp_brep_sph(struct db_i *dbip, const char *oname, const char *nname, long int test_num)
{
    struct directory *odp, *ndp;
    struct bu_external external;
    int idb_type = ID_BREP;

    odp = db_lookup(dbip, oname, LOOKUP_QUIET);
    if (odp == RT_DIR_NULL) {
	bu_exit(1, "Test %ld: cannot copy %s - object not found\n", test_num, oname);
    }

    if (db_get_external(&external, odp, dbip)) {
	bu_exit(1, "Test %ld: db_get_external cannot get bu_external form of %s\n", test_num, oname);
    }

    ndp = db_diradd(dbip, nname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&idb_type);

    ndp->d_flags = odp->d_flags;

    if (db_put_external(&external, ndp, dbip) < 0) {
	bu_exit(1, "Test %ld: db_put_external cannot write %s to  %s\n", test_num, oname, nname);
    }

    bu_free_external(&external);
}

/* Make a comb with all the objects in obj_argv */
static void
add_comb(struct db_i *dbip, const char *name, int obj_argc, const char **obj_argv, long int test_num)
{
    int i;
    struct rt_db_internal intern;
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_tree_array *tree_list;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_meth = &OBJ[ID_COMBINATION];
    intern.idb_type = ID_COMBINATION;
    BU_ALLOC(intern.idb_ptr, struct rt_comb_internal);
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_COMB_INTERNAL_INIT(comb);
    tree_list = (struct rt_tree_array *)bu_calloc(obj_argc, sizeof(struct rt_tree_array), "tree list");
    for (i = 0; i < obj_argc; i++) {
	union tree *tp;
	BU_GET(tp, union tree);
	RT_TREE_INIT(tp);
	tree_list[i].tl_op = OP_UNION;
	tree_list[i].tl_tree = tp;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(obj_argv[i]);
	tp->tr_l.tl_mat = (matp_t)NULL;

    }
    comb->tree = (union tree *)db_mkgift_tree(tree_list, obj_argc, &rt_uniresource);
    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_COMB, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: cannot add %s to directory\n", test_num, name);
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_exit(1, "Test %ld: Database write error creating comb, aborting\n", test_num);
    }
}

static int
cache_count(const char *cache_dir)
{
    int cache_objects = 0;
    struct bu_vls wpath = BU_VLS_INIT_ZERO;

    /* We need to find all cache objects */
    char **obj_dirs = NULL;
    bu_vls_sprintf(&wpath, "%s/objects", cache_dir);
    size_t objdir_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &obj_dirs);
    for (size_t i = 0; i < objdir_cnt; i++) {
	/* Find and remove all files in the obj dir */
	bu_vls_sprintf(&wpath, "%s/objects/%s", cache_dir, obj_dirs[i]);
	cache_objects += bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", NULL);
    }
    bu_argv_free(objdir_cnt, obj_dirs);
    bu_vls_free(&wpath);
    return cache_objects;
}


static void
cache_cleanup(struct bu_vls *cache_dir_vls)
{
    const char *cache_dir = bu_vls_cstr(cache_dir_vls);
    struct bu_vls wpath = BU_VLS_INIT_ZERO;

    /* Zap the format file first (that's the easy one) */
    bu_vls_sprintf(&wpath, "%s/format", cache_dir);
    bu_file_delete(bu_vls_cstr(&wpath));

    /* Now, we need to find and eliminate any cache objects */
    char **obj_dirs = NULL;
    bu_vls_sprintf(&wpath, "%s/objects", cache_dir);
    size_t objdir_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &obj_dirs);
    for (size_t i = 0; i < objdir_cnt; i++) {
	/* Find and remove all files in the obj dir */
	char **objs = NULL;
	bu_vls_sprintf(&wpath, "%s/objects/%s", cache_dir, obj_dirs[i]);
	size_t objs_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &objs);
	for (size_t j = 0; j < objs_cnt; j++) {
	    bu_vls_sprintf(&wpath, "%s/objects/%s/%s", cache_dir, obj_dirs[i], objs[j]);
	    if (!bu_file_delete(bu_vls_cstr(&wpath))) {
		bu_exit(1, "Unable to remove the object %s\n", bu_vls_cstr(&wpath));
	    }
	}
	bu_argv_free(objs_cnt, objs);

	/* Emptied the dir, now remove it */
	bu_vls_sprintf(&wpath, "%s/objects/%s", cache_dir, obj_dirs[i]);
	if (!bu_file_delete(bu_vls_cstr(&wpath))) {
	    bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
	}
    }
    bu_argv_free(objdir_cnt, obj_dirs);

    /* That should be everything - remove the objects dir and the cache dir */
    bu_vls_sprintf(&wpath, "%s/objects", cache_dir);
    if (!bu_file_delete(bu_vls_cstr(&wpath))) {
	bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
    }
    if (!bu_file_delete(cache_dir)) {
	bu_exit(1, "Unable to remove the directory %s\n", bu_vls_cstr(&wpath));
    }

    bu_vls_free(&wpath);
}

static struct db_i *
create_test_g_file(long int test_num, const char *gfile)
{
    struct db_i *dbip = DBI_NULL;

    if (bu_file_exists(gfile, NULL)) {
	bu_exit(1, "Test %ld: Stale .g file %s exists\n", test_num, gfile);
    }

    dbip = db_create(gfile, 5);

    if (dbip == DBI_NULL) {
	bu_exit(1, "Test %ld: unable to create test file %s\n", test_num, gfile);
    }

    RT_CK_DBI(dbip);

    return dbip;
}

static struct rt_i *
build_rtip(long int test_num, struct bu_vls *gfile, const char *objname, int stage_num, int do_parallel, int ncpus)
{
    struct rt_i *rtip = RTI_NULL;

    rtip = rt_dirbuild(bu_vls_cstr(gfile), NULL, 0);

    if (rtip == RTI_NULL) {
	bu_exit(1, "Test %ld: failed to rt_dirbuild in stage %d\n", test_num, stage_num);
    }

    if (!do_parallel) {
	if (rt_gettree(rtip, objname) < 0) {
	    bu_exit(1, "Test %ld: rt_getree in stage %d failed\n", test_num, stage_num);
	}
	rt_prep(rtip);
    } else {
	RTG.rtg_parallel =  (ncpus > 1) ? 1 : 0;
	if (rt_gettrees(rtip, 1, (const char **)&objname, ncpus) < 0) {
	    bu_exit(1, "Test %ld: rt_getrees in stage %d failed\n", test_num, stage_num);
	}
	rt_prep_parallel(rtip, ncpus);
    }

    return rtip;
}

/* Basic single object test.  Check that the cache object is created, that only
 * one object is created, that rt_shootray succeeds both after initial creation
 * and in a subsequent cache read. */
static int
test_cache_single_object(long int test_num, int do_parallel)
{
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct rt_i *rtip_stage_1, *rtip_stage_2;
    struct db_i *dbip;
    const char *oname = "sph.s";
    point_t v = VINIT_ZERO;
    size_t ncpus = 1;
    if (do_parallel) {
	ncpus =	(bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();
    }

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_1", RTC_PREFIX, test_num);
    bu_vls_sprintf(&gfile, "%s_%ld_1.g", RTC_PREFIX, test_num);

    bu_setenv("LIBRT_CACHE", bu_dir(NULL, 0, BU_DIR_CURR, bu_vls_cstr(&cache_dir), NULL), 1);

    if (bu_file_exists(getenv("LIBRT_CACHE"), NULL)) {
	bu_exit(1, "Test %ld: Stale test cache directory %s exists\n", test_num, getenv("LIBRT_CACHE"));
    }

    dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));

    // Unit sphere at the origin
    add_brep_sph(dbip, "sph.s", &v, 1, test_num);

    db_close(dbip);

    rtip_stage_1 = build_rtip(test_num, &gfile, oname, 1, do_parallel, ncpus);

    // Confirm there is exactly 1 file in the cache
    int cc = cache_count(bu_vls_cstr(&cache_dir));
    if (cc != 1) {
	bu_exit(1, "Test %ld: expected 1 cache object, found %d\n", test_num, cc);
    }

    rt_free_rti(rtip_stage_1);

    /*** Now, do it again with the cache in place */
    rtip_stage_2 = build_rtip(test_num, &gfile, oname, 2, do_parallel, ncpus);
    rt_free_rti(rtip_stage_2);

    /* All done - scrub out the temporary cache */
    cache_cleanup(&cache_dir);

    /* Clear the .g file */
    bu_file_delete(bu_vls_cstr(&gfile));

    bu_vls_free(&cache_dir);
    bu_vls_free(&gfile);
    return 0;
}

/* Basic multi object, same content test.  Check that the cache object is
 * created, that only one object is created, that rt_shootray succeeds both
 * after initial creation and in a subsequent cache read. */
static int
test_cache_multiple_object_same_content(long int test_num, long int obj_cnt, int do_parallel)
{
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct rt_i *rtip_stage_1, *rtip_stage_2;
    struct db_i *dbip;
    const char *oname_root = "sph_";
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    point_t v = VINIT_ZERO;
    int oc = obj_cnt;
    char **ov = (char **)bu_calloc(oc+1, sizeof(char  *), "object array");
    size_t ncpus = 1;
    if (do_parallel) {
	ncpus =	(bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();
    }

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_%ld", RTC_PREFIX, test_num, obj_cnt);
    bu_vls_sprintf(&gfile, "%s_%ld_%ld.g", RTC_PREFIX, test_num, obj_cnt);

    bu_setenv("LIBRT_CACHE", bu_dir(NULL, 0, BU_DIR_CURR, bu_vls_cstr(&cache_dir), NULL), 1);

    if (bu_file_exists(getenv("LIBRT_CACHE"), NULL)) {
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n", test_num, getenv("LIBRT_CACHE"));
    }

    dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));

    // Unit sphere at the origin
    add_brep_sph(dbip, "sph_0.s", &v, 1, test_num);
    ov[0] = bu_strdup("sph_0.s");

    for (long int i = 1; i < obj_cnt; i++) {
	bu_vls_sprintf(&cname, "%s%ld.s", oname_root, i);
	cp_brep_sph(dbip, ov[0], bu_vls_cstr(&cname), test_num);
	ov[i] = bu_strdup(bu_vls_cstr(&cname));
    }
    bu_vls_sprintf(&cname, "%s%ld.c", oname_root, test_num);

    add_comb(dbip, bu_vls_cstr(&cname), oc, (const char **)ov, test_num);

    db_close(dbip);

    for (long int i = 0; i < obj_cnt; i++) {
	bu_free(ov[i], "free string");
    }
    bu_free(ov, "free string array");
    ov = NULL;

    rtip_stage_1 = build_rtip(test_num, &gfile, bu_vls_cstr(&cname), 1, do_parallel, ncpus);

    // Confirm there is exactly 1 file in the cache
    int cc = cache_count(bu_vls_cstr(&cache_dir));
    if (cc != 1) {
	bu_exit(1, "Test %ld: expected 1 cache object, found %d\n", test_num, cc);
    }

    rt_free_rti(rtip_stage_1);

    /*** Now, do it again with the cache in place */
    rtip_stage_2 = build_rtip(test_num, &gfile, bu_vls_cstr(&cname), 2, do_parallel, ncpus);
    rt_free_rti(rtip_stage_2);

    /* All done - scrub out the temporary cache */
    cache_cleanup(&cache_dir);

    /* Clear the .g file */
    bu_file_delete(bu_vls_cstr(&gfile));

    bu_vls_free(&cache_dir);
    bu_vls_free(&gfile);
    bu_vls_free(&cname);
    return 0;
}

/* Basic multi object, different content test.  Check that the cache objects
 * are created, that the correct number of objects are created, and that we
 * can create a second rtip after the cache is initialized. */
static int
test_cache_multiple_object_different_content(long int test_num, long int obj_cnt, int do_parallel)
{
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct rt_i *rtip_stage_1, *rtip_stage_2;
    struct db_i *dbip;
    const char *oname_root = "sph_";
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    point_t v = VINIT_ZERO;
    double r = 1;
    int oc = obj_cnt;
    char **ov = (char **)bu_calloc(oc+1, sizeof(char  *), "object array");
    size_t ncpus = 1;
    if (do_parallel) {
	ncpus =	(bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();
    }

    bu_vls_sprintf(&cache_dir, "%s_dir_%ld_%ld", RTC_PREFIX, test_num, obj_cnt);
    bu_vls_sprintf(&gfile, "%s_%ld_%ld.g", RTC_PREFIX, test_num, obj_cnt);

    bu_setenv("LIBRT_CACHE", bu_dir(NULL, 0, BU_DIR_CURR, bu_vls_cstr(&cache_dir), NULL), 1);

    if (bu_file_exists(getenv("LIBRT_CACHE"), NULL)) {
	bu_exit(1, "Test %ld: stale test cache directory %s exists\n", test_num, getenv("LIBRT_CACHE"));
    }

    dbip = create_test_g_file(test_num, bu_vls_cstr(&gfile));

    // Unit sphere at the origin
    add_brep_sph(dbip, "sph_0.s", &v, r, test_num);
    ov[0] = bu_strdup("sph_0.s");

    for (long int i = 1; i < obj_cnt; i++) {
	r = 2 * r;
	v[1] = v[1] + (2*r);
	bu_vls_sprintf(&cname, "%s%ld.s", oname_root, i);
	add_brep_sph(dbip, bu_vls_cstr(&cname), &v, r, test_num);
	ov[i] = bu_strdup(bu_vls_cstr(&cname));
    }
    bu_vls_sprintf(&cname, "all.g");

    add_comb(dbip, bu_vls_cstr(&cname), oc, (const char **)ov, test_num);

    db_close(dbip);

    for (long int i = 0; i < obj_cnt; i++) {
	bu_free(ov[i], "free string");
    }
    bu_free(ov, "free string array");
    ov = NULL;

    rtip_stage_1 = build_rtip(test_num, &gfile, bu_vls_cstr(&cname), 1, do_parallel, ncpus);

    // Confirm there are exactly obj_cnt files in the cache
    long int cc = cache_count(bu_vls_cstr(&cache_dir));
    if (cc != obj_cnt) {
	bu_exit(1, "Test %ld: expected %ld cache object, found %ld\n", test_num, obj_cnt, cc);
    }

    rt_free_rti(rtip_stage_1);

    /*** Now, do it again with the cache in place */
    rtip_stage_2 = build_rtip(test_num, &gfile, bu_vls_cstr(&cname), 2, do_parallel, ncpus);
    rt_free_rti(rtip_stage_2);

    /* All done - scrub out the temporary cache */
    cache_cleanup(&cache_dir);

    /* Clear the .g file */
    bu_file_delete(bu_vls_cstr(&gfile));

    bu_vls_free(&cache_dir);
    bu_vls_free(&gfile);
    bu_vls_free(&cname);
    return 0;
}

const char *rt_cache_test_usage =
"Usage: rt_cache 1             (Single object serial test)\n"
"       rt_cache 2             (Single object parallel test)\n"
"       rt_cache 3 [obj_count] (Multiple identical object serial test)\n"
"       rt_cache 4 [obj_count] (Multiple identical object parallel test)\n"
"       rt_cache 5 [obj_count] (Multiple distinct object serial test)\n"
"       rt_cache 6 [obj_count] (Multiple distinct object parallel test)\n"
"       rt_cache 7 [gfile]     (Multiple process identical objects test)\n"
"       rt_cache 8 [gfile]     (Multiple process distinct objects test)\n";

int
main(int ac, char *av[])
{
    long int obj_cnt = 1;
    long int test_num = 0;

    if (ac < 2 || ac > 4) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    sscanf(av[1], "%ld", &test_num);

    if (test_num < 3 && ac > 2) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    if (test_num > 2 && test_num < 7 && ac == 3) {
	sscanf(av[2], "%ld", &obj_cnt);
    }

    /* Just get this done up front - all tests need it */
    rt_init_resource(&rt_uniresource, 0, NULL);

    switch (test_num) {
	case 1:
	    /* Serial prep API */
	    return test_cache_single_object(1, 0);
	    break;
	case 2:
	    /* Parallel prep API */
	    return test_cache_single_object(2, 1);
	    break;
	case 3:
	    /* Serial prep API */
	    return test_cache_multiple_object_same_content(3, obj_cnt, 0);
	    break;
	case 4:
	    /* Parallel prep API */
	    return test_cache_multiple_object_same_content(4, obj_cnt, 1);
	    break;
	case 5:
	    /* Serial prep API */
	    return test_cache_multiple_object_different_content(5, obj_cnt, 0);
	    break;
	case 6:
	    /* Parallel prep API */
	    return test_cache_multiple_object_different_content(6, obj_cnt, 1);
	default:
	    break;
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
