/*                        D O D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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

#include "common.h"

#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"		/* for ID_POLY special support */
#include "raytrace.h"
#include "rt/db4.h"
#include "ged/view.h"

#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"

static int
mged_check_shape_ref(struct mged_state *s, ged_draw_shape_ref ref, const char *caller)
{
    if (!s || !s->gedp || ged_draw_shape_ref_is_null(ref)) {
	if (s && s->interp && caller)
	    Tcl_AppendResult(s->interp, caller, "() ref is NULL\n", (char *)NULL);
	return 0;
    }

    struct ged_draw_shape_record rec;
    if (!ged_draw_shape_record_get(s->gedp, ref, &rec)) {
	if (s->interp && caller)
	    Tcl_AppendResult(s->interp, caller, "() stale draw ref\n", (char *)NULL);
	return 0;
    }
    return 1;
}


/*
 * Given an existing solid structure that may have been subjected to
 * solid editing, recompute the vector list, etc., to make the solid
 * the same as it originally was.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
int
replot_original_solid(struct mged_state *s, ged_draw_shape_ref ref)
{
    struct rt_db_internal intern;
    struct directory *dp;
    mat_t mat;

    if (s->dbip == DBI_NULL)
	return 0;

    struct ged_draw_shape_record rec;
    if (!ged_draw_shape_record_get(s->gedp, ref, &rec))
	return 0;
    if (!rec.fullpath || rec.fullpath->fp_len <= 0)
	return 0;
    dp = DB_FULL_PATH_CUR_DIR(rec.fullpath);
    if (rec.evaluated_region) {
	Tcl_AppendResult(s->interp, "replot_original_solid(", dp->d_namep,
			 "): Unable to plot evaluated regions, skipping\n", (char *)NULL);
	return -1;
    }
    struct db_full_path path;
    db_full_path_init(&path);
    db_dup_full_path(&path, rec.fullpath);
    (void)db_path_to_mat(s->dbip, &path, mat, path.fp_len-1);
    db_free_full_path(&path);

    if (rt_db_get_internal(&intern, dp, s->dbip, mat) < 0) {
	Tcl_AppendResult(s->interp, dp->d_namep, ":  solid import failure\n", (char *)NULL);
	return -1;		/* ERROR */
    }
    RT_CK_DB_INTERNAL(&intern);

    if (replot_modified_solid(s, ref, &intern, bn_mat_identity) < 0) {
	rt_db_free_internal(&intern);
	return -1;
    }
    rt_db_free_internal(&intern);
    return 0;
}


/*
 * Given the solid structure of a solid that has already been drawn,
 * and a new database record and transform matrix,
 * create a new vector list for that solid, and substitute.
 * Used for solid editing mode.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
int
replot_modified_solid(
	struct mged_state *s,
	ged_draw_shape_ref ref,
	struct rt_db_internal *ip,
	const mat_t mat)
{
    struct rt_db_internal intern;

    RT_DB_INTERNAL_INIT(&intern);

    if (!mged_check_shape_ref(s, ref, "replot_modified_solid")) {
	Tcl_AppendResult(s->interp, "replot_modified_solid() sp==NULL?\n", (char *)NULL);
	return -1;
    }

    /* Release existing vlist of this solid */
    ged_draw_shape_ref_geometry_clear(s->gedp, ref);

    /* Draw (plot) a normal solid */
    RT_CK_DB_INTERNAL(ip);

    s->tol.ttol.magic = BG_TESS_TOL_MAGIC;
    s->tol.ttol.abs = s->tol.abs_tol;
    s->tol.ttol.rel = s->tol.rel_tol;
    s->tol.ttol.norm = s->tol.nrm_tol;

    transform_editing_solid(s, &intern, mat, ip, 0);

    if (ged_draw_shape_ref_publish_primitive_wireframe(s->gedp, ref, &intern,
	    &s->tol.ttol, &s->tol.tol, NULL, 0) < 0) {
	struct ged_draw_shape_record rec;
	if (ged_draw_shape_record_get(s->gedp, ref, &rec) && rec.leaf_name)
	    Tcl_AppendResult(s->interp, rec.leaf_name,
		    ": re-plot failure\n", (char *)NULL);
	rt_db_free_internal(&intern);
	return -1;
    }
    rt_db_free_internal(&intern);

    {
	int bad_cmd = 0;
	if (!ged_draw_shape_ref_update_bounds_from_geometry(s->gedp, ref, &bad_cmd) && bad_cmd) {
	    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&tmp_vls, "unknown vlist op %d\n", bad_cmd);
	    Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	}
    }
    ged_draw_shape_set_highlighted(s->gedp, ref, 1);

    mged_refresh_request_view(s, view_state, BSG_VIEW_REFRESH_VIEW);
    return 0;
}

void
add_solid_record_path_to_result(
    Tcl_Interp *interp,
    const struct ged_draw_shape_record *rec)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    if (!rec || !rec->fullpath)
	return;
    db_path_to_vls(&str, rec->fullpath);
    Tcl_AppendResult(interp, bu_vls_addr(&str), " ", NULL);
    bu_vls_free(&str);
}

int
redraw_visible_objects(struct mged_state *s)
{
    const char *av[1] = {"redraw"};
    int ret = ged_exec_redraw(s->gedp, 1, av);

    if (ret & BRLCAD_ERROR)
	return TCL_ERROR;

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
