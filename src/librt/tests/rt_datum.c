/*                        R T _ D A T U M . C
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

#include "common.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/avs.h"
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

/* TODO - are these already expressed as API somewhere? Got most of
 * the logic below from studying MGED code... */
int
apply_mat_obj(struct db_i *dbip, const char *obj_name, matp_t mat)
{
    struct rt_db_internal intern;
    struct directory *dp = db_lookup(dbip, obj_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_log("Error - could not find object %s\n", obj_name);
	return 1;
    }
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, mat, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal error\n");
	rt_db_free_internal(&intern);
	return 1;
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	bu_log("rt_db_put_internal error\n");
	rt_db_free_internal(&intern);
	return 1;
    }
    return 0;
}

int
apply_mat_instance(struct db_i *dbip, const char *comb_name, const char *obj_name, matp_t mat)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    struct directory *cdp = db_lookup(dbip, comb_name, LOOKUP_QUIET);
    struct directory *dp = db_lookup(dbip, obj_name, LOOKUP_QUIET);
    if (cdp == RT_DIR_NULL) {
	bu_log("Error - could not find comb %s\n", comb_name);
	return 1;
    }
    if (dp == RT_DIR_NULL) {
	bu_log("Error - could not find object %s\n", obj_name);
	return 1;
    }
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, cdp, dbip, NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal error\n");
	rt_db_free_internal(&intern);
	return 1;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    if (!comb->tree) {
	bu_log("comb has no tree??");
	return 1;
    }
    tp = (union tree *)db_find_named_leaf(comb->tree, dp->d_namep);
    if (tp != TREE_NULL) {
	/* Got a matching instance, do matrix */
	if (tp->tr_l.tl_mat) {
	     bn_mat_mul2(mat, tp->tr_l.tl_mat);
	} else {
	    tp->tr_l.tl_mat = (matp_t)bu_malloc(16 * sizeof(fastf_t), "tl_mat");
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}
	/* Write result */
	if (rt_db_put_internal(cdp, dbip, &intern, &rt_uniresource) < 0) {
	    bu_log("rt_db_put_internal error\n");
	    rt_db_free_internal(&intern);
	}
    } else {
	bu_log("Error: comb has no instance of %s", dp->d_namep);
	rt_db_free_internal(&intern);
	return 1;
    }

    return 0;
}

void
report_matrix(struct db_i *dbip, const char *p)
{
    mat_t m;
    struct db_full_path path;
    (void)db_string_to_path(&path, dbip, p);
    (void)db_path_to_mat(dbip, &path, (matp_t)(&m), path.fp_len, &rt_uniresource);
    bn_mat_print(p, m);
}

void
report_object_params(struct db_i *dbip, const char *o)
{
    int id;
    struct bu_vls rtlog = BU_VLS_INIT_ZERO;
    struct rt_db_internal intern;

    struct directory *dp = db_lookup(dbip, o, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return;

    RT_DB_INTERNAL_INIT(&intern);
    if ((id = rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource)) < 0)
	return;

    if (!OBJ[id].ft_describe)
	return;
    if (OBJ[id].ft_describe(&rtlog, &intern, 1, dbip->dbi_base2local) < 0)
	return;

    bu_log("OBJECT %s:\n%s\n", o, bu_vls_addr(&rtlog));
    bu_vls_free(&rtlog);
}


void
report_instance_params(struct db_i *dbip, const char *c, const char *o)
{
    int id;
    mat_t cmat = MAT_INIT_IDN;
    struct bu_vls rtlog = BU_VLS_INIT_ZERO;
    struct rt_db_internal intern, cintern;
    struct rt_comb_internal *comb;
    union tree *tp;
    struct directory *cdp = db_lookup(dbip, c, LOOKUP_QUIET);
    struct directory *dp = db_lookup(dbip, o, LOOKUP_QUIET);

    if (cdp == RT_DIR_NULL || dp == RT_DIR_NULL)
	return;

    RT_DB_INTERNAL_INIT(&cintern);
    if (rt_db_get_internal(&cintern, cdp, dbip, NULL, &rt_uniresource) < 0) {
	rt_db_free_internal(&cintern);
	return;
    }

    comb = (struct rt_comb_internal *)cintern.idb_ptr;
    if (!comb->tree) return;
    tp = (union tree *)db_find_named_leaf(comb->tree, dp->d_namep);
    if (tp == TREE_NULL) {
	rt_db_free_internal(&cintern);
	return;
    }

    /* Got a matching instance, grab matrix */
    if (tp->tr_l.tl_mat) {
	MAT_COPY(cmat, tp->tr_l.tl_mat);
    }

    /* Load the actual object instance, applying the comb matrix to transform the
     * object parameter values into the instance values */
    RT_DB_INTERNAL_INIT(&intern);
    if ((id = rt_db_get_internal(&intern, dp, dbip, cmat, &rt_uniresource)) < 0)
	return;

    if (!OBJ[id].ft_describe)
	return;
    if (OBJ[id].ft_describe(&rtlog, &intern, 1, dbip->dbi_base2local) < 0)
	return;

    bu_log("INSTANCE %s/%s:\n%s\n", c, o, bu_vls_addr(&rtlog));
    bu_vls_free(&rtlog);
}


int
main(int UNUSED(argc), char *argv[])
{
    point_t p;
    vect_t delta;
    mat_t mat;
    char rt_tmpfile[MAXPATHLEN];
    struct db_i *dbip = DBI_NULL;

    bu_setprogname(argv[0]);

    /* Get a guaranteed-valid temp file */
    FILE *fp = bu_temp_file(rt_tmpfile, MAXPATHLEN);
    if (fp == NULL) {
	bu_log("%s error: unable to create temporary .g file\n", argv[0]);
	return 1;
    } else {
	bu_log("%s\n", rt_tmpfile);
    }

    /* Manually turn the temp file into a .g */
    if (db5_fwrite_ident(fp, "librt datum test file", 1.0) < 0) {
	bu_log("%s error: failed to prepare .g file %s\n", argv[0], rt_tmpfile);
	return 1;
    }
    (void)fclose(fp);
    if ((dbip = db_open(rt_tmpfile, DB_OPEN_READWRITE)) == DBI_NULL) {
	bu_log("%s error: db_open of %s failed\n", argv[0], rt_tmpfile);
	return 1;
    }
    if (db_dirbuild(dbip) < 0) {
	bu_log("%s error: db_dirbuild of %s failed\n", argv[0], rt_tmpfile);
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

    /* Make combs with all the objects */
    {
	int node_count = 2;
	const char *names[] = {"rcc_1.s", "datum_plane.s"};
	if (add_comb(dbip, "comb_1.c", node_count, (const char **)names)) return 1;
	if (add_comb(dbip, "comb_2.c", node_count, (const char **)names)) return 1;
    }

    db_update_nref(dbip, &rt_uniresource);

    /* Manipulate the objects */


    bu_log("\n\nOperation 01: apply a 90, 90 rotation to a parent comb\n\n");
    bn_mat_ae(mat, 90, 90);
    apply_mat_obj(dbip, "comb_1.c", mat);

    /* Report */
    report_object_params(dbip, "rcc_1.s");
    report_object_params(dbip, "datum_plane.s");
    report_matrix(dbip, "comb_1.c/rcc_1.s");
    report_matrix(dbip, "comb_1.c/datum_plane.s");
    report_instance_params(dbip, "comb_1.c", "rcc_1.s");
    report_instance_params(dbip, "comb_1.c", "datum_plane.s");

    bu_log("\n\nOperation 02: apply a -100,20,45 translation to a parent comb\n\n");
    MAT_IDN(mat);
    VSET(delta, -100, 20, 45);
    MAT_DELTAS_VEC(mat, delta);
    apply_mat_obj(dbip, "comb_2.c", mat);

    /* Report */
    report_object_params(dbip, "rcc_1.s");
    report_object_params(dbip, "datum_plane.s");
    report_matrix(dbip, "comb_2.c/rcc_1.s");
    report_matrix(dbip, "comb_2.c/datum_plane.s");
    report_instance_params(dbip, "comb_2.c", "rcc_1.s");
    report_instance_params(dbip, "comb_2.c", "datum_plane.s");


    bu_log("\n\nOperation 03: apply a -90, -90 rotation to the solids\n\n");
    bn_mat_ae(mat, -90, -90);
    apply_mat_obj(dbip, "rcc_1.s", mat);
    apply_mat_obj(dbip, "datum_plane.s", mat);

    /* Report */
    report_object_params(dbip, "rcc_1.s");
    report_object_params(dbip, "datum_plane.s");
    report_matrix(dbip, "comb_1.c/rcc_1.s");
    report_matrix(dbip, "comb_1.c/datum_plane.s");
    report_matrix(dbip, "comb_2.c/rcc_1.s");
    report_matrix(dbip, "comb_2.c/datum_plane.s");
    report_instance_params(dbip, "comb_1.c", "rcc_1.s");
    report_instance_params(dbip, "comb_1.c", "datum_plane.s");
    report_instance_params(dbip, "comb_2.c", "rcc_1.s");
    report_instance_params(dbip, "comb_2.c", "datum_plane.s");

    bu_log("\n\nOperation 04: apply a 10,30,-40 translation to the solids\n\n");
    MAT_IDN(mat);
    VSET(delta, 10, 30, -40);
    MAT_DELTAS_VEC(mat, delta);
    apply_mat_obj(dbip, "rcc_1.s", mat);
    apply_mat_obj(dbip, "datum_plane.s", mat);

    /* Report */
    report_object_params(dbip, "rcc_1.s");
    report_object_params(dbip, "datum_plane.s");
    report_matrix(dbip, "comb_1.c/rcc_1.s");
    report_matrix(dbip, "comb_1.c/datum_plane.s");
    report_matrix(dbip, "comb_2.c/rcc_1.s");
    report_matrix(dbip, "comb_2.c/datum_plane.s");
    report_instance_params(dbip, "comb_1.c", "rcc_1.s");
    report_instance_params(dbip, "comb_1.c", "datum_plane.s");
    report_instance_params(dbip, "comb_2.c", "rcc_1.s");
    report_instance_params(dbip, "comb_2.c", "datum_plane.s");

    bu_log("\n\nOperation 05: scale a comb about -30, 40, 10 by 5x\n\n");
    VSET(p, -30, 40, 10);
    bn_mat_scale_about_pnt(mat, p, 5);
    apply_mat_obj(dbip, "comb_1.c", mat);

    /* Report */
    report_object_params(dbip, "rcc_1.s");
    report_object_params(dbip, "datum_plane.s");
    report_matrix(dbip, "comb_1.c/rcc_1.s");
    report_matrix(dbip, "comb_1.c/datum_plane.s");
    report_matrix(dbip, "comb_2.c/rcc_1.s");
    report_matrix(dbip, "comb_2.c/datum_plane.s");
    report_instance_params(dbip, "comb_1.c", "rcc_1.s");
    report_instance_params(dbip, "comb_1.c", "datum_plane.s");
    report_instance_params(dbip, "comb_2.c", "rcc_1.s");
    report_instance_params(dbip, "comb_2.c", "datum_plane.s");

    bu_log("\n\nOperation 06: scale the solids about 10, 30, -40 by 5x\n\n");
    VSET(p, 10, 30, -40);
    bn_mat_scale_about_pnt(mat, p, 5);
    apply_mat_obj(dbip, "rcc_1.s", mat);
    apply_mat_obj(dbip, "datum_plane.s", mat);

    /* Report */
    report_object_params(dbip, "rcc_1.s");
    report_object_params(dbip, "datum_plane.s");
    report_matrix(dbip, "comb_1.c/rcc_1.s");
    report_matrix(dbip, "comb_1.c/datum_plane.s");
    report_matrix(dbip, "comb_2.c/rcc_1.s");
    report_matrix(dbip, "comb_2.c/datum_plane.s");
    report_instance_params(dbip, "comb_1.c", "rcc_1.s");
    report_instance_params(dbip, "comb_1.c", "datum_plane.s");
    report_instance_params(dbip, "comb_2.c", "rcc_1.s");
    report_instance_params(dbip, "comb_2.c", "datum_plane.s");




    /*bu_snooze(BU_SEC2USEC(1000));*/

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
