/*                           C A C H E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
#include "bu/process.h"
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

static size_t
cache_count(const char *cache_dir, int ignore_temp)
{
    size_t cache_objects = 0;
    struct bu_vls wpath = BU_VLS_INIT_ZERO;

    /* We need to find all cache objects */
    char **obj_dirs = NULL;
    bu_vls_sprintf(&wpath, "%s/objects", cache_dir);
    size_t objdir_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &obj_dirs);
    for (size_t i = 0; i < objdir_cnt; i++) {
	/* Find and remove all files in the obj dir */
	bu_vls_sprintf(&wpath, "%s/objects/%s", cache_dir, obj_dirs[i]);
	if (!ignore_temp) {
	    cache_objects += bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", NULL);
	} else {
	    char **objs = NULL;
	    size_t objs_cnt = bu_file_list(bu_vls_cstr(&wpath), "[a-zA-z0-9]*", &objs);
	    for (size_t j = 0; j < objs_cnt; j++) {
		// Temporary objects will have '.' chars in the name - skip them,
		// since another process might be working on one
		if (!strchr(objs[j], '.')) {
		    cache_objects++;
		}
	    }
	    bu_argv_free(objs_cnt, objs);
	}
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
build_rtip(long int test_num, const char *gfile, const char *objname, int stage_num, int do_parallel, int ncpus)
{
    struct rt_i *rtip = RTI_NULL;

    rtip = rt_dirbuild(gfile, NULL, 0);

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

static int
test_subprocess(int ac, char *av[])
{
    long int test_num;
    long int process_num;
    long int expected;
    struct rt_i *rtip_stage_1, *rtip_stage_2;
    const char *gfile;
    const char *cache_dir;
    const char *cname;
    size_t ncpus = (bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();

    if (ac != 6) {
	bu_exit(1, "rt_cache subprocess command invoked incorrectly");
    }

    sscanf(av[0], "%ld", &test_num);
    sscanf(av[1], "%ld", &process_num);
    sscanf(av[2], "%ld", &expected);
    gfile = av[3];
    cache_dir = av[4];
    cname = av[5];

    bu_setenv("LIBRT_CACHE", bu_dir(NULL, 0, BU_DIR_CURR, cache_dir, NULL), 1);

    rtip_stage_1 = build_rtip(test_num, gfile, cname, process_num*1000 + 1, 1, (int)ncpus);

    // Confirm the presence of the expected number of file(s) in the cache
    size_t cc = cache_count(cache_dir, 1);
    if (cc != (size_t)expected) {
	bu_exit(1, "Test %ld(process %ld): expected %ld cache object(s), found %zu\n", test_num, process_num, expected, cc);
    }

    rt_clean(rtip_stage_1);
    rt_free_rti(rtip_stage_1);

    /*** Now, do it again with the cache definitely in place */
    rtip_stage_2 = build_rtip(test_num, gfile, cname, process_num*1000 + 2, 1, (int)ncpus);
    rt_clean(rtip_stage_2);
    rt_free_rti(rtip_stage_2);

    bu_log("Test %ld(process %ld): PASSED\n", test_num, process_num);

    return 0;
}

struct subprocess_data {
    char *pname;
    long int test_num;
    long int process_num;
    long int expected;
    char *gfile;
    char *cache_dir;
    char *cname;
    int result;
    struct bu_vls *sd_result;
};

static void
subprocess_launcher(int id, void *data)
{
    int count, aborted;
    int ac = 8;
    char *line = (char *)bu_calloc(MAXPATHLEN+501, sizeof(char), "resultstr");
    const char *av[9];
    int ind = (!id) ? 0 : (id - 1);
    struct subprocess_data **sda = (struct subprocess_data **)data;
    struct subprocess_data *sd = sda[ind];
    struct bu_vls tn = BU_VLS_INIT_ZERO;
    struct bu_vls pn = BU_VLS_INIT_ZERO;
    struct bu_vls en = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tn, "%ld", sd->test_num);
    bu_vls_sprintf(&pn, "%ld", sd->process_num);
    bu_vls_sprintf(&en, "%ld", sd->expected);

    av[0] = sd->pname;
    av[1] = "-1";
    av[2] = bu_vls_cstr(&tn);
    av[3] = bu_vls_cstr(&pn);
    av[4] = bu_vls_cstr(&en);
    av[5] = sd->gfile;
    av[6] = sd->cache_dir;
    av[7] = sd->cname;
    av[8] = NULL;

    sd->result = 0;

    struct bu_process *p = NULL;
    bu_process_exec(&p, sd->pname, ac, (const char **)av, 0, 0);
    bu_vls_printf(sd->sd_result, "Test %ld(process %ld): running...\n", sd->test_num, sd->process_num);

    while (bu_process_read((char *)line, &count, p, BU_PROCESS_STDERR, MAXPATHLEN+501) > 0) {
	bu_vls_printf(sd->sd_result, "%s\n", line);
	memset(line, 0, MAXPATHLEN+501);
    }

    if (bu_process_wait(&aborted, p, 120)) {
	bu_vls_printf(sd->sd_result, "Test %ld(process %ld): rt_cache_subprocess failed\n", sd->test_num, sd->process_num);
	sd->result = 1;
    }

    bu_free(line, "line");
}

/* Core test routine.  Check that the cache object is created, that only the
 * correct number of object(s) is/are created, and that a second rtip can be
 * successfully created after the cache is initialized. */
static int
test_cache(char *rp, long int test_num, long int obj_cnt, int do_parallel, int different_content, int subprocess_cnt, int comb_cnt)
{
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    struct rt_i *rtip_stage_1, *rtip_stage_2;
    struct db_i *dbip;
    const char *oname_root = "sph_";
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    point_t v = VINIT_ZERO;
    double r = 1.0;
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

    bu_vls_sprintf(&cname, "sph_0.s");

    // Unit sphere at the origin
    add_brep_sph(dbip, bu_vls_cstr(&cname), &v, r, test_num);

    // If we're doing multiple objects, put them in a comb
    if (obj_cnt > 1) {
	int oc = obj_cnt;
	char **ov = (char **)bu_calloc(oc+1, sizeof(char  *), "object array");

	ov[0] = bu_strdup("sph_0.s");

	for (long int i = 1; i < obj_cnt; i++) {
	    bu_vls_sprintf(&cname, "%s%ld.s", oname_root, i);
	    if (different_content) {
		// For different content, make a series of larger spheres along
		// the Y axis
		r = 2 * r;
		v[1] = v[1] + (2*r);
		bu_vls_sprintf(&cname, "%s%ld.s", oname_root, i);
		add_brep_sph(dbip, bu_vls_cstr(&cname), &v, r, test_num);
	    } else {
		// For same content, exactly duplicate the original sphere
		// (don't want new UUID from openNURBS in data structure, for
		// example, so we can't just make more unit spheres - need to
		// specifically copy original) */
		cp_brep_sph(dbip, ov[0], bu_vls_cstr(&cname), test_num);
	    }
	    ov[i] = bu_strdup(bu_vls_cstr(&cname));
	}
	bu_vls_sprintf(&cname, "%s%ld.c", oname_root, test_num);

	add_comb(dbip, bu_vls_cstr(&cname), oc, (const char **)ov, test_num);

	if (comb_cnt > 0) {
	    struct bu_vls obj_comb = BU_VLS_INIT_ZERO;
	    char **ovc = (char **)bu_calloc(comb_cnt+1, sizeof(char  *), "object array");
	    const char *ocname = NULL;
	    bu_vls_sprintf(&obj_comb, "%s", bu_vls_cstr(&cname));
	    ocname = bu_vls_cstr(&obj_comb);

	    for (long int i = 0; i < comb_cnt; i++) {
		bu_vls_sprintf(&cname, "%s%ld_child%ld.c", oname_root, test_num, i);
		ovc[i] = bu_strdup(bu_vls_cstr(&cname));
		add_comb(dbip, bu_vls_cstr(&cname), 1, (const char **)&ocname, test_num);
	    }
	    bu_vls_sprintf(&cname, "%s%ld-top.c", oname_root, test_num);
	    add_comb(dbip, bu_vls_cstr(&cname), comb_cnt, (const char **)ovc, test_num);

	    for (long int i = 0; i < comb_cnt; i++) {
		bu_free(ovc[i], "free string");
	    }
	    bu_free(ovc, "free string array");
	    ovc = NULL;
	}

	for (long int i = 0; i < obj_cnt; i++) {
	    bu_free(ov[i], "free string");
	}
	bu_free(ov, "free string array");
	ov = NULL;
    }

    db_close(dbip);

    if (!subprocess_cnt) {
	rtip_stage_1 = build_rtip(test_num, bu_vls_cstr(&gfile), bu_vls_cstr(&cname), 1, do_parallel, (int)ncpus);

	// Confirm the presence of the expected number of file(s) in the cache
	size_t cc = cache_count(bu_vls_cstr(&cache_dir), 0);
	long int expected = (different_content) ? obj_cnt : 1;
	if (cc != (size_t)expected) {
	    bu_exit(1, "Test %ld: expected %ld cache object(s), found %zu\n", test_num, expected, cc);
	}

	rt_clean(rtip_stage_1);
	rt_free_rti(rtip_stage_1);

	/*** Now, do it again with the cache in place */
	rtip_stage_2 = build_rtip(test_num, bu_vls_cstr(&gfile), bu_vls_cstr(&cname), 2, do_parallel, (int)ncpus);
	rt_clean(rtip_stage_2);
	rt_free_rti(rtip_stage_2);
    } else {
	long int expected = (different_content) ? obj_cnt : 1;
	struct subprocess_data **sdata = (struct subprocess_data **)bu_calloc(subprocess_cnt, sizeof(struct subprocess_data *), "launch data array");
	for (long int i = 0; i < subprocess_cnt; i++) {
	    sdata[i] = (struct subprocess_data *)bu_calloc(1, sizeof(struct subprocess_data), "launch data");
	    sdata[i]->pname = bu_strdup(rp);
	    sdata[i]->test_num = test_num;
	    sdata[i]->process_num = i;
	    sdata[i]->expected = expected;
	    sdata[i]->gfile = bu_strdup(bu_vls_cstr(&gfile));
	    sdata[i]->cache_dir = bu_strdup(bu_vls_cstr(&cache_dir));
	    sdata[i]->cname = bu_strdup(bu_vls_cstr(&cname));
	    BU_GET(sdata[i]->sd_result, struct bu_vls);
	    bu_vls_init(sdata[i]->sd_result);
	}
	bu_parallel(subprocess_launcher, subprocess_cnt, (void *)sdata);
	for (long int i = 0; i < subprocess_cnt; i++) {
	    if (sdata[i]->result) {
		bu_exit(1, "Test %ld: subprocess failure:\n%s", test_num, bu_vls_cstr(sdata[i]->sd_result));
	    } else {
		bu_log("%s", bu_vls_cstr(sdata[i]->sd_result));
	    }
	    bu_free(sdata[i]->pname, "gfile");
	    bu_free(sdata[i]->gfile, "gfile");
	    bu_free(sdata[i]->cache_dir, "cache_dir");
	    bu_free(sdata[i]->cname, "cname");
	    bu_vls_free(sdata[i]->sd_result);
	    BU_PUT(sdata[i]->sd_result, struct bu_vls);
	}
    }

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
"       rt_cache 7 [obj_count] (Multiple distinct objects, multiple instances in tree parallel test)\n"
"       rt_cache 20 [obj_count] [subprocess_count] (Multiple process identical objects test)\n"
"       rt_cache 21 [obj_count] [subprocess_count] (Multiple process distinct objects test)\n";

int
main(int ac, char *av[])
{
    long int subprocess_cnt = 2;
    long int obj_cnt = 1;
    long int test_num = 0;
    char *rp = bu_file_realpath(av[0], NULL);
    size_t maxcpus = (bu_avail_cpus() > MAX_PSW) ? MAX_PSW : bu_avail_cpus();

    bu_setprogname(av[0]);

    ac--; av++;

    if (ac < 1) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    sscanf(av[0], "%ld", &test_num);

    if (test_num == -1) {
	ac--; av++;
	return test_subprocess(ac, av);
    }

    if (ac > 3) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    if (test_num < 3 && ac > 1) {
	bu_exit(1, "%s", rt_cache_test_usage);
    }

    if (test_num > 2 && ac >= 2) {
	sscanf(av[1], "%ld", &obj_cnt);
    }

    if (test_num > 19 && ac >= 3) {
	sscanf(av[2], "%ld", &subprocess_cnt);

	// TODO - see if we can use the bu_debug |= BU_DEBUG_PARALLEL trick from
	// the libbu semaphore.c unit test to push this harder... - we may not
	// need to be capped at the CPU count

	if (subprocess_cnt > (long int)maxcpus) {
	    bu_log("Requested %ld subprocesses, but only %zd CPUs available - throttling to %zd\n", subprocess_cnt, maxcpus, maxcpus);
	    subprocess_cnt = (long int)maxcpus;
	}
    }

    /* Just get this done up front - all tests need it */
    rt_init_resource(&rt_uniresource, 0, NULL);

    switch (test_num) {
	case 1:
	    /* Serial prep API, 1 object */
	    return test_cache(rp, test_num, 1, 0, 0, 0, 0);
	    break;
	case 2:
	    /* Parallel prep API, 1 object */
	    return test_cache(rp, test_num, 1, 1, 0, 0, 0);
	    break;
	case 3:
	    /* Serial prep API, multiple objects, identical content */
	    return test_cache(rp, test_num, obj_cnt, 0, 0, 0, 0);
	    break;
	case 4:
	    /* Parallel prep API, multiple objects, identical content */
	    return test_cache(rp, test_num, obj_cnt, 1, 0, 0, 0);
	    break;
	case 5:
	    /* Serial prep API, multiple objects, different content */
	    return test_cache(rp, test_num, obj_cnt, 0, 1, 0, 0);
	    break;
	case 6:
	    /* Parallel prep API, multiple objects, different content */
	    return test_cache(rp, test_num, obj_cnt, 1, 1, 0, 0);
	case 7:
	    /* Parallel prep API, multiple objects, non-unique content, multiple instances in tree */
	    return test_cache(rp, test_num, obj_cnt, 1, 1, 0, 5);
	case 20:
	    /* Multiple objects, same content, multi-process */
	    return test_cache(rp, test_num, obj_cnt, 1, 0, subprocess_cnt, 0);
	case 21:
	    /* Multiple objects, different content, multi-process */
	    return test_cache(rp, test_num, obj_cnt, 1, 1, subprocess_cnt, 0);
	default:
	    break;
    }

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

