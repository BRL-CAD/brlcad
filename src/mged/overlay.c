/*                       O V E R L A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/overlay.c
 *
 */

#include "common.h"

#include <math.h>
#include <signal.h>

#include "vmath.h"
#include "raytrace.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

/* Usage:  overlay file.plot3 [name] */
int
cmd_overlay(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    int ret;
    Tcl_DString ds;

    if (GEDP == GED_NULL)
	return TCL_OK;

    Tcl_DStringInit(&ds);

    GEDP->ged_dmp = (void *)mged_curr_dm->dm_dmp;
    ret = ged_overlay(GEDP, argc, argv);
    Tcl_DStringAppend(&ds, bu_vls_addr(GEDP->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret & GED_HELP)
	return TCL_OK;

    if (ret != GED_OK)
	return TCL_ERROR;

    update_views = 1;
    dm_set_dirty(DMP, 1);

    return ret;
}


/* Usage:  labelvert solid(s) */
int
f_labelvert(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    struct bn_vlblock*vbp;
    struct directory *dp;
    mat_t mat;
    fastf_t scale;

    CHECK_DBI_NULL;

    if (argc < 2) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help labelvert");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, view_state->vs_gvp->gv_rotation);
    scale = view_state->vs_gvp->gv_size / 100;		/* divide by # chars/screen */

    for (i=1; i<argc; i++) {
	struct bview_scene_obj *s;
	if ((dp = db_lookup(DBIP, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	/* Find uses of this solid in the solid table */
	gdlp = BU_LIST_NEXT(display_list, GEDP->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, GEDP->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(s, bview_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (db_full_path_search(&s->s_fullpath, dp)) {
		    rt_label_vlist_verts(vbp, &s->s_vlist, mat, scale, base2local);
		}
	    }

	    gdlp = next_gdlp;
	}
    }

    cvt_vlblock_to_solids(vbp, "_LABELVERT_", 0);

    bn_vlblock_free(vbp);
    update_views = 1;
    dm_set_dirty(DMP, 1);
    return TCL_OK;
}

void get_face_list( const struct model* m, struct bu_list* f_list )
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct face* curr_f;
    int found;

    NMG_CK_MODEL(m);

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
        NMG_CK_REGION(r);

        if (r->ra_p) {
            NMG_CK_REGION_A(r->ra_p);
        }

        for (BU_LIST_FOR(s, shell, &r->s_hd)) {
            NMG_CK_SHELL(s);

            if (s->sa_p) {
                NMG_CK_SHELL_A(s->sa_p);
            }

            /* Faces in shell */
            for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
                NMG_CK_FACEUSE(fu);
                f = fu->f_p;
                NMG_CK_FACE(f);

                found = 0;
                /* check for duplicate face struct */
                for (BU_LIST_FOR(curr_f, face, f_list)) {
                    if (curr_f->index == f->index) {
                        found = 1;
                        break;
                    }
                }

                if ( !found )
                    BU_LIST_INSERT( f_list, &(f->l) );

                if (f->g.magic_p) switch (*f->g.magic_p) {
                    case NMG_FACE_G_PLANE_MAGIC:
                        break;
                    case NMG_FACE_G_SNURB_MAGIC:
                        break;
                }

            }
        }
    }
}

/* Usage:  labelface solid(s) */
int
f_labelface(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    struct bn_vlblock *vbp;
    mat_t mat;
    fastf_t scale;
    struct model* m;
    const char* name;
    struct bu_list f_list;

    BU_LIST_INIT( &f_list );

    CHECK_DBI_NULL;

    if (argc < 2) {
        struct bu_vls vls = BU_VLS_INIT_ZERO;

        bu_vls_printf(&vls, "help labelface");
        Tcl_Eval(interp, bu_vls_addr(&vls));
        bu_vls_free(&vls);
        return TCL_ERROR;
    }

    /* attempt to resolve and verify */
    name = argv[1];

    if ( (dp=db_lookup(GEDP->ged_wdbp->dbip, name, LOOKUP_QUIET))
        == RT_DIR_NULL ) {
        bu_vls_printf(GEDP->ged_result_str, "%s does not exist\n", name);
        return GED_ERROR;
    }

    if (rt_db_get_internal(&internal, dp, GEDP->ged_wdbp->dbip,
        bn_mat_identity, &rt_uniresource) < 0) {
        bu_vls_printf(GEDP->ged_result_str, "rt_db_get_internal() error\n");
        return GED_ERROR;
    }

    if (internal.idb_type != ID_NMG) {
        bu_vls_printf(GEDP->ged_result_str, "%s is not an NMG solid\n", name);
        rt_db_free_internal(&internal);
        return GED_ERROR;
    }

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, view_state->vs_gvp->gv_rotation);
    scale = view_state->vs_gvp->gv_size / 100;      /* divide by # chars/screen */

    for (i=1; i<argc; i++) {
        struct bview_scene_obj *s;
        if ((dp = db_lookup(DBIP, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
            continue;

        /* Find uses of this solid in the solid table */
        gdlp = BU_LIST_NEXT(display_list, GEDP->ged_gdp->gd_headDisplay);
        while (BU_LIST_NOT_HEAD(gdlp, GEDP->ged_gdp->gd_headDisplay)) {
            next_gdlp = BU_LIST_PNEXT(display_list, gdlp);
            for (BU_LIST_FOR(s, bview_scene_obj, &gdlp->dl_head_scene_obj)) {
                if (db_full_path_search(&s->s_fullpath, dp)) {
                    get_face_list(m, &f_list);
                    rt_label_vlist_faces(vbp, &f_list, mat, scale, base2local);
                }
            }

            gdlp = next_gdlp;
        }
    }

    cvt_vlblock_to_solids(vbp, "_LABELFACE_", 0);

    bn_vlblock_free(vbp);
    update_views = 1;
    dm_set_dirty(DMP, 1);
    return TCL_OK;
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
