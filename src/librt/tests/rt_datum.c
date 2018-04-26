/*                        R T _ D A T U M . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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

#include "vmath.h"
#include "bu/avs.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "../librt_private.h"

/* Note - initially, the thought was to use only librt API for this, without
 * pulling in libwdb.  Normally libwdb should be used to create objects of this
 * sort (and there really isn't much point to not doing so here, in retrospect)
 * but since the below code is working leave it for now. Eventually, we'll
 * regularize the libwdb API (probably making it a part of a "libg" library and
 * once we do the below add_* functions can be simplified down to mk_* calls */

int
add_rcc(struct db_i *dbip, const char *name, point_t *v, vect_t *h, double r)
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct rt_tgc_internal *tip;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_TGC;
    intern.idb_meth = &OBJ[ID_TGC];
    BU_ALLOC(intern.idb_ptr, struct rt_tgc_internal);
    tip = (struct rt_tgc_internal *)intern.idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    VMOVE(tip->v, *v);
    VMOVE(tip->h, *h);

    bn_vec_ortho(tip->a, tip->h);
    VUNITIZE(tip->a);
    VCROSS(tip->b, tip->h, tip->a);
    VUNITIZE(tip->b);

    VSCALE(tip->a, tip->a, r);
    VSCALE(tip->b, tip->b, r);
    VMOVE(tip->c, tip->a);
    VMOVE(tip->d, tip->b);

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_log("Error: Cannot add %s to directory\n", name);
	return 1;
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_log("Error: Database write error, aborting\n");
	return 1;
    }

    return 0;
}

int
add_datum(struct db_i *dbip, const char *name, point_t *pnt, vect_t *dir, double w)
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct rt_datum_internal *datum;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_meth = &OBJ[ID_DATUM];
    intern.idb_type = ID_DATUM;
    BU_ALLOC(intern.idb_ptr, struct rt_datum_internal);
    datum = (struct rt_datum_internal *)intern.idb_ptr;
    datum->magic = RT_DATUM_INTERNAL_MAGIC;

    /* By default datum objects are yellow */
    bu_avs_add(&intern.idb_avs, "color", "255/255/0");

    /* Dimensions */
    VMOVE(datum->pnt, *pnt);
    if (dir) {
	VMOVE(datum->dir, *dir);
    }
    /* Direction scalar multiplier */
    datum->w = w;

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_log("Error: Cannot add %s to directory\n", name);
	return 1;
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_log("Error: Database write error, aborting\n");
	return 1;
    }

    return 0;
}

/* Make a comb with all the objects in obj_argv */
int
add_comb(struct db_i *dbip, const char *name, int obj_argc, const char **obj_argv)
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
	RT_GET_TREE(tp, &rt_uniresource);
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
	bu_log("Error: Cannot add %s to directory\n", name);
	return 1;
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_log("%s error: Database write error, aborting\n");
	return 1;
    }

    return 0;
}


int
main(int UNUSED(argc), char *argv[])
{
    char tmpfile[MAXPATHLEN];
    struct db_i *dbip = DBI_NULL;

    /* Get a guaranteed-valid temp file */
    FILE *fp = bu_temp_file(tmpfile, MAXPATHLEN);
    if (fp == NULL) {
	bu_log("%s error: unable to create temporary .g file\n", argv[0]);
	return 1;
    } else {
	bu_log("%s\n", tmpfile);
    }

    /* Manually turn the temp file into a .g */
    if (db5_fwrite_ident(fp, "librt datum test file", 1.0) < 0) {
	bu_log("%s error: failed to prepare .g file %s\n", argv[0], tmpfile);
	return 1;
    }
    (void)fclose(fp);
    if ((dbip = db_open(tmpfile, DB_OPEN_READWRITE)) == DBI_NULL) {
	bu_log("%s error: db_open of %s failed\n", argv[0], tmpfile);
	return 1;
    }
    if (db_dirbuild(dbip) < 0) {
	bu_log("%s error: db_dirbuild of %s failed\n", argv[0], tmpfile);
	return 1;
    }
    RT_CK_DBI(dbip);
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* Control RCC shape */
    {
	point_t v;
	vect_t h;
	double r = 1.0;
	VSET(v, 0, 0, 0);
	VSET(h, 0, 0, 10);
	if (add_rcc(dbip, "rcc_1.s", &v, &h, r)) return 1;
    }

    /* Datum plane */
    {
	point_t pnt;
	vect_t dir;
	double w = 1.0;
	VSET(pnt, 0, 0, 0);
	VSET(dir, 0, 0, 10);
	if (add_datum(dbip, "datum_plane.s", &pnt, &dir, w)) return 1;
    }

    /* Make a comb with all the objects */
    {
	int node_count = 2;
	const char *names[] = {"rcc_1.s", "datum_plane.s"};
	if (add_comb(dbip, "comb_1.c", node_count, (const char **)names)) return 1;
    }

    db_update_nref(dbip, &rt_uniresource);

    /* Manipulate the comb */

    /* TODO */

    /* Read the values with the comb matrix applied */
    {
	mat_t rm, dm;
	struct db_full_path datum_path, rcc_path;
	(void)db_string_to_path(&rcc_path, dbip, "comb_1.c/rcc_1.s");
	(void)db_string_to_path(&datum_path, dbip, "comb_1.c/datum_plane.s");
	(void)db_path_to_mat(dbip, &rcc_path, (matp_t)(&rm), rcc_path.fp_len, &rt_uniresource);
	bn_mat_print("rcc matrix", rm);
	(void)db_path_to_mat(dbip, &datum_path, (matp_t)(&dm), datum_path.fp_len, &rt_uniresource);
	bn_mat_print("datum matrix", dm);
    }

    /*sleep(1000);*/

    db_close(dbip);
    /*bu_file_delete(tmpfile);*/
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
