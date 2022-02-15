/*                          M O U S E . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2022 United States Government as represented by
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

#include "bu/path.h"
#include "bv.h"
#include "bg/lseg.h"
#include "tclcad.h"

/* Private headers */
#include "./tclcad_private.h"
#include "./view/view.h"

int
to_get_prev_mouse(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d %d", (int)gdvp->gv_prevMouseX, (int)gdvp->gv_prevMouseY);
    return BRLCAD_OK;
}


int
to_mouse_append_pnt_common(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int UNUSED(maxargs))
{
    int ret;
    char *av[4];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &x, &y, x, y);
    VSET(view, x, y, 0.0);

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);

    gedp->ged_gvp = gdvp;
    int snapped = 0;
    if (gedp->ged_gvp->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(gedp->ged_gvp, &view[X], &view[Y]);
    }
    if (!snapped && gedp->ged_gvp->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(gedp->ged_gvp, &view[X], &view[Y]);
    }

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = (char *)argv[0];
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    ret = (*func)(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_brep_selection_append(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr UNUSED(func),
			       const char *usage,
			       int maxargs)
{
    const char *cmd_argv[11] = {"brep", NULL, "selection", "append", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    char *brep_name;
    char *end;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct bu_vls start[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    struct bu_vls dir[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    point_t screen_pt, view_pt, model_pt;
    vect_t view_dir, model_dir;
    mat_t invRot;

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* parse args */
    brep_name = bu_path_basename(argv[2], NULL);

    screen_pt[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %f\n", screen_pt[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    screen_pt[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %f\n", screen_pt[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* stash point coordinates for future drag handling */
    gdvp->gv_prevMouseX = screen_pt[X];
    gdvp->gv_prevMouseY = screen_pt[Y];

    /* convert screen point to model-space start point and direction */
    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &view_pt[X], &view_pt[Y], screen_pt[X], screen_pt[Y]);
    view_pt[Z] = 1.0;

    MAT4X3PNT(model_pt, gdvp->gv_view2model, view_pt);

    VSET(view_dir, 0.0, 0.0, -1.0);
    bn_mat_inv(invRot, gedp->ged_gvp->gv_rotation);
    MAT4X3PNT(model_dir, invRot, view_dir);

    /* brep brep_name selection append selection_name startx starty startz dirx diry dirz */
    bu_vls_printf(&start[X], "%f", model_pt[X]);
    bu_vls_printf(&start[Y], "%f", model_pt[Y]);
    bu_vls_printf(&start[Z], "%f", model_pt[Z]);

    cmd_argv[1] = brep_name;
    cmd_argv[5] = bu_vls_addr(&start[X]);
    cmd_argv[6] = bu_vls_addr(&start[Y]);
    cmd_argv[7] = bu_vls_addr(&start[Z]);

    bu_vls_printf(&dir[X], "%f", model_dir[X]);
    bu_vls_printf(&dir[Y], "%f", model_dir[Y]);
    bu_vls_printf(&dir[Z], "%f", model_dir[Z]);

    cmd_argv[8] = bu_vls_addr(&dir[X]);
    cmd_argv[9] = bu_vls_addr(&dir[Y]);
    cmd_argv[10] = bu_vls_addr(&dir[Z]);

    gedp->ged_gvp = gdvp;
    ret = ged_exec(gedp, cmd_argc, cmd_argv);

    bu_vls_free(&start[X]);
    bu_vls_free(&start[Y]);
    bu_vls_free(&start[Z]);
    bu_vls_free(&dir[X]);
    bu_vls_free(&dir[Y]);
    bu_vls_free(&dir[Z]);

    if (ret != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    struct bu_vls *dname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (dname && bu_vls_strlen(dname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_brep_selection_translate %s %s %%x %%y; "
		      "%s brep %s plot SCV}",
		      bu_vls_cstr(dname),
		      bu_vls_cstr(&current_top->to_gedp->go_name),
		      bu_vls_cstr(&gdvp->gv_name),
		      brep_name,
		      bu_vls_cstr(&current_top->to_gedp->go_name),
		      brep_name);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    bu_free((void *)brep_name, "brep_name");

    return BRLCAD_OK;
}


int
to_mouse_brep_selection_translate(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr UNUSED(func),
				  const char *usage,
				  int maxargs)
{
    const char *cmd_argv[8] = {"brep", NULL, "selection", "translate", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    char *brep_name;
    char *end;
    point_t screen_end, view_start, view_end, model_start, model_end;
    vect_t model_delta;
    struct bu_vls delta[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    brep_name = bu_path_basename(argv[2], NULL);

    screen_end[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %f\n", screen_end[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    screen_end[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %f\n", screen_end[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* convert screen-space delta to model-space delta */
    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &view_start[X], &view_start[Y], gdvp->gv_prevMouseX, gdvp->gv_prevMouseY);
    view_start[Z] = 1;
    MAT4X3PNT(model_start, gdvp->gv_view2model, view_start);

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &view_end[X], &view_end[Y], screen_end[X], screen_end[Y]);
    view_end[Z] = 1;
    MAT4X3PNT(model_end, gdvp->gv_view2model, view_end);

    VSUB2(model_delta, model_end, model_start);

    bu_vls_printf(&delta[X], "%f", model_delta[X]);
    bu_vls_printf(&delta[Y], "%f", model_delta[Y]);
    bu_vls_printf(&delta[Z], "%f", model_delta[Z]);

    cmd_argv[1] = brep_name;
    cmd_argv[5] = bu_vls_addr(&delta[X]);
    cmd_argv[6] = bu_vls_addr(&delta[Y]);
    cmd_argv[7] = bu_vls_addr(&delta[Z]);

    ret = ged_exec(gedp, cmd_argc, cmd_argv);

    bu_free((void *)brep_name, "brep_name");
    bu_vls_free(&delta[X]);
    bu_vls_free(&delta[Y]);
    bu_vls_free(&delta[Z]);

    if (ret != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    /* need to tell front-end that we've modified the db */
    tclcad_eval_noresult(current_top->to_interp, "$::ArcherCore::application setSave", 0, NULL);

    gdvp->gv_prevMouseX = screen_end[X];
    gdvp->gv_prevMouseY = screen_end[Y];

    cmd_argc = 2;
    cmd_argv[0] = "draw";
    cmd_argv[1] = argv[2];
    cmd_argv[2] = NULL;
    ret = to_edit_redraw(gedp, cmd_argc, cmd_argv);

    return ret;
}


int
to_mouse_constrain_rot(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    int ret;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    fastf_t sf;
    struct bu_vls rot_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }


    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    dx *= gdvp->gv_rscale;
    dy *= gdvp->gv_rscale;

    if (fabs(dx) > fabs(dy))
	sf = dx;
    else
	sf = dy;

    switch (argv[2][0]) {
	case 'x':
	    bu_vls_printf(&rot_vls, "%lf 0 0", -sf);
	    break;
	case 'y':
	    bu_vls_printf(&rot_vls, "0 %lf 0", -sf);
	    break;
	case 'z':
	    bu_vls_printf(&rot_vls, "0 0 %lf", -sf);
    }

    gedp->ged_gvp = gdvp;
    ac = 3;
    av[0] = "rot";
    av[1] = "-m";
    av[2] = bu_vls_addr(&rot_vls);
    av[3] = (char *)0;

    ret = ged_exec(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == BRLCAD_OK) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    tclcad_eval_noresult(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback), 0, NULL);
	}

	to_refresh_view(gdvp);
    }

    return BRLCAD_OK;
}


int
to_mouse_constrain_trans(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr UNUSED(func),
			 const char *usage,
			 int UNUSED(maxargs))
{
    int width;
    int ret;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls tran_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gv_size * gedp->dbip->dbi_local2base;
    dy *= inv_width * gdvp->gv_size * gedp->dbip->dbi_local2base;

    if (fabs(dx) > fabs(dy))
	sf = dx;
    else
	sf = dy;

    switch (argv[2][0]) {
	case 'x':
	    bu_vls_printf(&tran_vls, "%lf 0 0", -sf);
	    break;
	case 'y':
	    bu_vls_printf(&tran_vls, "0 %lf 0", -sf);
	    break;
	case 'z':
	    bu_vls_printf(&tran_vls, "0 0 %lf", -sf);
    }

    gedp->ged_gvp = gdvp;
    ac = 3;
    av[0] = "tra";
    av[1] = "-m";
    av[2] = bu_vls_addr(&tran_vls);
    av[3] = (char *)0;

    ret = ged_exec(gedp, ac, (const char **)av);
    bu_vls_free(&tran_vls);

    if (ret == BRLCAD_OK) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    tclcad_eval_noresult(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback), 0, NULL);
	}

	to_refresh_view(gdvp);
    }

    return BRLCAD_OK;
}


int
to_mouse_find_arb_edge(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    char *av[6];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &x, &y, x, y);
    VSET(view, x, y, 0.0);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "find_arb_edge_nearest_pnt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)argv[5];
    av[4] = (char *)0;

    (void)ged_exec(gedp, 4, (const char **)av);
    bu_vls_free(&pt_vls);

    return BRLCAD_OK;
}


int
to_mouse_find_bot_edge(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    char *av[6];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &x, &y, x, y);
    VSET(view, x, y, 0.0);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "find_bot_edge_nearest_pnt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_exec(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return BRLCAD_OK;
}


int
to_mouse_find_bot_pnt(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr UNUSED(func),
		      const char *usage,
		      int UNUSED(maxargs))
{
    char *av[6];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &x, &y, x, y);
    VSET(view, x, y, 0.0);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "find_bot_pnt_nearest_pnt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_exec(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return BRLCAD_OK;
}


int
to_mouse_find_metaball_pnt(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr UNUSED(func),
			   const char *usage,
			   int UNUSED(maxargs))
{
    char *av[6];
    point_t model;
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &x, &y, x, y);
    VSET(view, x, y, 0.0);
    MAT4X3PNT(model, gdvp->gv_view2model, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "find_metaball_pnt_nearest_pnt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_exec(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return BRLCAD_OK;
}


int
to_mouse_find_pipe_pnt(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    char *av[6];
    point_t model;
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &x, &y, x, y);
    VSET(view, x, y, 0.0);
    MAT4X3PNT(model, gdvp->gv_view2model, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "find_pipe_pnt_nearest_pnt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_exec(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return BRLCAD_OK;
}


int
to_mouse_joint_select(
    struct ged *gedp,
    int argc,
    const char *argv[],
    ged_func_ptr UNUSED(func),
    const char *usage,
    int maxargs)
{
    const char *cmd_argv[11] = {"joint2", NULL, "selection", "replace", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    char *joint_name;
    char *end;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct bu_vls start[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    struct bu_vls dir[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    point_t screen_pt, view_pt, model_pt;
    vect_t view_dir, model_dir;
    mat_t invRot;

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* parse args */
    joint_name = bu_path_basename(argv[2], NULL);

    screen_pt[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %f\n", screen_pt[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    screen_pt[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %f\n", screen_pt[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* stash point coordinates for future drag handling */
    gdvp->gv_prevMouseX = screen_pt[X];
    gdvp->gv_prevMouseY = screen_pt[Y];

    /* convert screen point to model-space start point and direction */
    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &view_pt[X], &view_pt[Y], screen_pt[X], screen_pt[Y]);
    view_pt[Z] = 1.0;

    MAT4X3PNT(model_pt, gdvp->gv_view2model, view_pt);

    VSET(view_dir, 0.0, 0.0, -1.0);
    bn_mat_inv(invRot, gedp->ged_gvp->gv_rotation);
    MAT4X3PNT(model_dir, invRot, view_dir);

    /* joint2 joint_name selection append selection_name startx starty startz dirx diry dirz */
    bu_vls_printf(&start[X], "%f", model_pt[X]);
    bu_vls_printf(&start[Y], "%f", model_pt[Y]);
    bu_vls_printf(&start[Z], "%f", model_pt[Z]);

    cmd_argv[1] = joint_name;
    cmd_argv[5] = bu_vls_addr(&start[X]);
    cmd_argv[6] = bu_vls_addr(&start[Y]);
    cmd_argv[7] = bu_vls_addr(&start[Z]);

    bu_vls_printf(&dir[X], "%f", model_dir[X]);
    bu_vls_printf(&dir[Y], "%f", model_dir[Y]);
    bu_vls_printf(&dir[Z], "%f", model_dir[Z]);

    cmd_argv[8] = bu_vls_addr(&dir[X]);
    cmd_argv[9] = bu_vls_addr(&dir[Y]);
    cmd_argv[10] = bu_vls_addr(&dir[Z]);

    gedp->ged_gvp = gdvp;
    ret = ged_exec(gedp, cmd_argc, cmd_argv);

    bu_vls_free(&start[X]);
    bu_vls_free(&start[Y]);
    bu_vls_free(&start[Z]);
    bu_vls_free(&dir[X]);
    bu_vls_free(&dir[Y]);
    bu_vls_free(&dir[Z]);

    if (ret != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    struct bu_vls *dname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (dname) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_joint_selection_translate %s %s %%x %%y}",
		      bu_vls_cstr(dname),
		      bu_vls_cstr(&current_top->to_gedp->go_name),
		      bu_vls_cstr(&gdvp->gv_name),
		      joint_name);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    bu_free((void *)joint_name, "joint_name");

    return BRLCAD_OK;
}


int
to_mouse_joint_selection_translate(
    struct ged *gedp,
    int argc,
    const char *argv[],
    ged_func_ptr UNUSED(func),
    const char *usage,
    int maxargs)
{
    const char *cmd_argv[8] = {"joint2", NULL, "selection", "translate", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    char *joint_name;
    char *end;
    point_t screen_end, view_start, view_end, model_start, model_end;
    vect_t model_delta;
    struct bu_vls delta[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    joint_name = bu_path_basename(argv[2], NULL);

    screen_end[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %f\n", screen_end[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    screen_end[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %f\n", screen_end[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* convert screen-space delta to model-space delta */
    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &view_start[X], &view_start[Y], gdvp->gv_prevMouseX, gdvp->gv_prevMouseY);
    view_start[Z] = 1;
    MAT4X3PNT(model_start, gdvp->gv_view2model, view_start);

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &view_end[X], &view_end[Y], screen_end[X], screen_end[Y]);
    view_end[Z] = 1;
    MAT4X3PNT(model_end, gdvp->gv_view2model, view_end);

    VSUB2(model_delta, model_end, model_start);

    bu_vls_printf(&delta[X], "%f", model_delta[X]);
    bu_vls_printf(&delta[Y], "%f", model_delta[Y]);
    bu_vls_printf(&delta[Z], "%f", model_delta[Z]);

    cmd_argv[1] = joint_name;
    cmd_argv[5] = bu_vls_addr(&delta[X]);
    cmd_argv[6] = bu_vls_addr(&delta[Y]);
    cmd_argv[7] = bu_vls_addr(&delta[Z]);

    ret = ged_exec(gedp, cmd_argc, cmd_argv);

    if (ret != BRLCAD_OK) {
	bu_free((void *)joint_name, "joint_name");
	bu_vls_free(&delta[X]);
	bu_vls_free(&delta[Y]);
	bu_vls_free(&delta[Z]);
	return BRLCAD_ERROR;
    }

    /* need to tell front-end that we've modified the db */
    Tcl_Eval(current_top->to_interp, "$::ArcherCore::application setSave");

    gdvp->gv_prevMouseX = screen_end[X];
    gdvp->gv_prevMouseY = screen_end[Y];

    cmd_argc = 3;
    cmd_argv[0] = "get";
    cmd_argv[1] = joint_name;
    cmd_argv[2] = "RP1";
    cmd_argv[3] = NULL;
    ret = ged_exec(gedp, cmd_argc, cmd_argv);

    if (ret == BRLCAD_OK) {
	char *path_name = bu_strdup(bu_vls_cstr(gedp->ged_result_str));
	int dmode = 0;
	struct bu_vls path_dmode = BU_VLS_INIT_ZERO;

	/* get current display mode of path */
	cmd_argc = 2;
	cmd_argv[0] = "how";
	cmd_argv[1] = path_name;
	cmd_argv[2] = NULL;
	ret = ged_exec(gedp, cmd_argc, cmd_argv);

	if (ret == BRLCAD_OK) {
	    bu_sscanf(bu_vls_cstr(gedp->ged_result_str), "%d", &dmode);
	}
	if (dmode == 4) {
	    bu_vls_printf(&path_dmode, "-h");
	} else {
	    bu_vls_printf(&path_dmode, "-m%d", dmode);
	}

	/* erase path to split it from visible vlists */
	cmd_argc = 2;
	cmd_argv[0] = "erase";
	cmd_argv[1] = path_name;
	cmd_argv[2] = NULL;
	ret = ged_exec(gedp, cmd_argc, cmd_argv);

	if (ret == BRLCAD_OK) {
	    /* redraw path with its previous display mode */
	    cmd_argc = 4;
	    cmd_argv[0] = "draw";
	    cmd_argv[1] = "-R";
	    cmd_argv[2] = bu_vls_cstr(&path_dmode);
	    cmd_argv[3] = path_name;
	    cmd_argv[4] = NULL;
	    ret = ged_exec(gedp, cmd_argc, cmd_argv);

	    to_refresh_all_views(current_top);
	}
	bu_vls_free(&path_dmode);
	bu_free(path_name, "path_name");
    }

    bu_free((void *)joint_name, "joint_name");
    bu_vls_free(&delta[X]);
    bu_vls_free(&delta[Y]);
    bu_vls_free(&delta[Z]);

    return ret;
}


int
to_mouse_move_arb_edge(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    int width;
    int ret;
    char *av[6];
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_move_arb_edge expects things to be in local units */
    dx *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "move_arb_edge";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = ged_exec(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_move_arb_face(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    int width;
    int ret;
    char *av[6];
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_move_arb_face expects things to be in local units */
    dx *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "move_arb_face";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = ged_exec(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_move_bot_pnt(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr UNUSED(func),
		      const char *usage,
		      int UNUSED(maxargs))
{
    int width;
    int ret;
    int rflag;
    char *av[6];
    const char *cmd;
    fastf_t dx, dy, dz;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t v2m_mat;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_HELP;
    }

    if (argc == 7) {
	if (argv[1][0] != '-' || argv[1][1] != 'r' || argv[1][2] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return BRLCAD_ERROR;
	}

	rflag = 1;
	--argc;
	++argv;
    } else
	rflag = 0;

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;

    if (rflag) {
	dx = x - gdvp->gv_prevMouseX;
	dy = gdvp->gv_prevMouseY - y;
	dz = 0.0;

	gdvp->gv_prevMouseX = x;
	gdvp->gv_prevMouseY = y;

	if (dx < gdvp->gv_minMouseDelta)
	    dx = gdvp->gv_minMouseDelta;
	else if (gdvp->gv_maxMouseDelta < dx)
	    dx = gdvp->gv_maxMouseDelta;

	if (dy < gdvp->gv_minMouseDelta)
	    dy = gdvp->gv_minMouseDelta;
	else if (gdvp->gv_maxMouseDelta < dy)
	    dy = gdvp->gv_maxMouseDelta;

	bn_mat_inv(v2m_mat, gdvp->gv_rotation);

	dx *= inv_width * gdvp->gv_size;
	dy *= inv_width * gdvp->gv_size;
    } else {
	struct rt_db_internal intern;
	struct rt_bot_internal *botip;
	mat_t mat;
	size_t vertex_i;
	char *last;

	if ((last = strrchr(argv[2], '/')) == NULL)
	    last = (char *)argv[2];
	else
	    ++last;

	if (last[0] == '\0') {
	    bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", cmd, argv[2]);
	    return BRLCAD_ERROR;
	}

	if (bu_sscanf(argv[3], "%zu", &vertex_i) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad bot vertex index - %s", cmd, argv[3]);
	    return BRLCAD_ERROR;
	}

	if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[2], gedp->ged_wdbp, mat) & BRLCAD_ERROR) {
	    bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, argv[2]);
	    return BRLCAD_ERROR;
	}

	if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	    bu_vls_printf(gedp->ged_result_str, "Object is not a BOT");
	    rt_db_free_internal(&intern);

	    return BRLCAD_ERROR;
	}

	botip = (struct rt_bot_internal *)intern.idb_ptr;

	if (vertex_i >= botip->num_vertices) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad bot vertex index - %s", cmd, argv[3]);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}

	MAT4X3PNT(view, gdvp->gv_model2view, &botip->vertices[vertex_i*3]);
	MAT_COPY(v2m_mat, gdvp->gv_view2model);

	gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
	gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
	bv_screen_to_view(gdvp, &dx, &dy, x, y);
	dz = view[Z];

	rt_db_free_internal(&intern);
    }

    VSET(view, dx, dy, dz);
    MAT4X3PNT(model, v2m_mat, view);

    /* ged_bot_move_pnt expects things to be in local units */
    VSCALE(model, model, gedp->dbip->dbi_base2local);
    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "bot_move_pnt";

    if (rflag) {
	av[1] = "-r";
	av[2] = (char *)argv[2];
	av[3] = (char *)argv[3];
	av[4] = bu_vls_addr(&pt_vls);
	av[5] = (char *)0;

	ret = ged_exec(gedp, 5, (const char **)av);
    } else {
	av[1] = (char *)argv[2];
	av[2] = (char *)argv[3];
	av[3] = bu_vls_addr(&pt_vls);
	av[4] = (char *)0;

	ret = ged_exec(gedp, 4, (const char **)av);
    }

    bu_vls_free(&pt_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_move_bot_pnts(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    int ret, width;
    const char *cmd;
    fastf_t dx, dy, dz;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t v2m_mat;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_HELP;
    }

    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;
    dz = 0.0;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    bn_mat_inv(v2m_mat, gdvp->gv_rotation);

    dx *= inv_width * gdvp->gv_size;
    dy *= inv_width * gdvp->gv_size;

    VSET(view, dx, dy, dz);
    MAT4X3PNT(model, v2m_mat, view);

    /* ged_bot_move_pnts expects things to be in local units */
    VSCALE(model, model, gedp->dbip->dbi_base2local);
    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;

    {
	register int i, j;
	int ac = argc - 2;
	char **av = (char **)bu_calloc(ac, sizeof(char *), "to_mouse_move_bot_pnts: av[]");
	av[0] = "bot_move_pnts";

	av[1] = (char *)argv[4];
	av[2] = bu_vls_addr(&pt_vls);
	av[ac-1] = (char *)0;

	for (i=3, j=5; i < ac; ++i, ++j)
	    av[i] = (char *)argv[j];

	ret = ged_exec(gedp, ac, (const char **)av);
	bu_vls_free(&pt_vls);

	if (ret == BRLCAD_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[4];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}

	bu_free((void *)av, "to_mouse_move_bot_pnts: av[]");
    }

    return BRLCAD_OK;
}


int
to_mouse_move_pnt_common(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int UNUSED(maxargs))
{
    int ret, width;
    char *av[6];
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_pipe_move_pnt expects things to be in local units */
    dx *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = (char *)argv[0];
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = (*func)(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_orotate(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls rot_x_vls = BU_VLS_INIT_ZERO;
    struct bu_vls rot_y_vls = BU_VLS_INIT_ZERO;
    struct bu_vls rot_z_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = y - gdvp->gv_prevMouseY;
    dy = x - gdvp->gv_prevMouseX;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    dx *= gdvp->gv_rscale;
    dy *= gdvp->gv_rscale;

    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&rot_x_vls, "%lf", model[X]);
    bu_vls_printf(&rot_y_vls, "%lf", model[Y]);
    bu_vls_printf(&rot_z_vls, "%lf", model[Z]);

    gedp->ged_gvp = gdvp;

    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    if (0 < bu_vls_strlen(&tvd->gdv_edit_motion_delta_callback)) {
	const char *command = bu_vls_addr(&tvd->gdv_edit_motion_delta_callback);
	const char *args[4];
	args[0] = "orotate";
	args[1] = bu_vls_addr(&rot_x_vls);
	args[2] = bu_vls_addr(&rot_y_vls);
	args[3] = bu_vls_addr(&rot_z_vls);
	tclcad_eval(current_top->to_interp, command, sizeof(args) / sizeof(args[0]), args);
    } else {
	char *av[6];

	av[0] = "orotate";
	av[1] = (char *)argv[2];
	av[2] = bu_vls_addr(&rot_x_vls);
	av[3] = bu_vls_addr(&rot_y_vls);
	av[4] = bu_vls_addr(&rot_z_vls);
	av[5] = (char *)0;

	if (ged_exec(gedp, 5, (const char **)av) == BRLCAD_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}
    }

    bu_vls_free(&rot_x_vls);
    bu_vls_free(&rot_y_vls);
    bu_vls_free(&rot_z_vls);

    return BRLCAD_OK;
}


int
to_mouse_oscale(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr UNUSED(func),
		const char *usage,
		int UNUSED(maxargs))
{
    int width;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls sf_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gv_sscale;
    dy *= inv_width * gdvp->gv_sscale;

    if (fabs(dx) < fabs(dy))
	sf = 1.0 + dy;
    else
	sf = 1.0 + dx;

    bu_vls_printf(&sf_vls, "%lf", sf);

    gedp->ged_gvp = gdvp;

    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    if (0 < bu_vls_strlen(&tvd->gdv_edit_motion_delta_callback)) {
	struct bu_vls tcl_cmd;

	bu_vls_init(&tcl_cmd);
	bu_vls_printf(&tcl_cmd, "%s oscale %s", bu_vls_addr(&tvd->gdv_edit_motion_delta_callback), bu_vls_addr(&sf_vls));
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&tcl_cmd));
	bu_vls_free(&tcl_cmd);
    } else {
	char *av[6];

	av[0] = "oscale";
	av[1] = (char *)argv[2];
	av[2] = bu_vls_addr(&sf_vls);
	av[3] = (char *)0;

	if (ged_exec(gedp, 3, (const char **)av) == BRLCAD_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}
    }

    bu_vls_free(&sf_vls);

    return BRLCAD_OK;
}


int
to_mouse_otranslate(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    int width;
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model = VINIT_ZERO;
    point_t view = VINIT_ZERO;
    mat_t inv_rot;
    struct bu_vls tran_x_vls = BU_VLS_INIT_ZERO;
    struct bu_vls tran_y_vls = BU_VLS_INIT_ZERO;
    struct bu_vls tran_z_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_otranslate expects things to be in local units */
    dx *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;

    VSET(view, dx, dy, 0.0);
    bu_vls_printf(&tran_x_vls, "%lf", model[X]);
    bu_vls_printf(&tran_y_vls, "%lf", model[Y]);
    bu_vls_printf(&tran_z_vls, "%lf", model[Z]);

    gedp->ged_gvp = gdvp;

    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
    if (0 < bu_vls_strlen(&tvd->gdv_edit_motion_delta_callback)) {
	const char *path_string = argv[2];
	vect_t dvec;
	struct dm_path_edit_params *params = (struct dm_path_edit_params *)bu_hash_get(tgd->go_dmv.edited_paths,
										 (uint8_t *)path_string,
										 sizeof(char) * strlen(path_string) + 1);

	if (!params) {
	    BU_GET(params, struct dm_path_edit_params);
	    params->edit_mode = gdvp->gv_tcl.gv_polygon_mode;
	    params->dx = params->dy = 0.0;
	    (void)bu_hash_set(tgd->go_dmv.edited_paths,
			      (uint8_t *)path_string,
			      sizeof(char) * strlen(path_string) + 1, (void *)params);
	}

	params->dx += dx;
	params->dy += dy;
	VSET(view, params->dx, params->dy, 0.0);
	bn_mat_inv(inv_rot, gdvp->gv_rotation);
	MAT4X3PNT(model, inv_rot, view);

	MAT_IDN(params->edit_mat);
	MAT4X3PNT(model, inv_rot, view);
	VSCALE(dvec, model, gedp->dbip->dbi_local2base);
	MAT_DELTAS_VEC(params->edit_mat, dvec);

	to_refresh_view(gdvp);
    } else {
	char *av[6];

	av[0] = "otranslate";
	av[1] = (char *)argv[2];
	av[2] = bu_vls_addr(&tran_x_vls);
	av[3] = bu_vls_addr(&tran_y_vls);
	av[4] = bu_vls_addr(&tran_z_vls);
	av[5] = (char *)0;

	if (ged_exec(gedp, 5, (const char **)av) == BRLCAD_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}
    }

    bu_vls_free(&tran_x_vls);
    bu_vls_free(&tran_y_vls);
    bu_vls_free(&tran_z_vls);

    return BRLCAD_OK;
}


int
go_mouse_poly_circ(Tcl_Interp *interp,
		   struct ged *gedp,
		   struct bview *gdvp,
		   int argc,
		   const char *argv[],
		   const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_mouse_poly_circ_func(interp, gedp, gdvp, argc, argv, usage);
}


int
to_mouse_poly_circ(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* shift the command name to argv[1] before calling to_mouse_poly_circ_func */
    argv[1] = argv[0];
    ret = to_mouse_poly_circ_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage);
#if 0
    if (ret == BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
#endif

    to_refresh_view(gdvp);

    return ret;
}


int
to_mouse_poly_circ_func(Tcl_Interp *interp,
			struct ged *gedp,
			struct bview *gdvp,
			int UNUSED(argc),
			const char *argv[],
			const char *usage)
{
    int ac;
    char *av[5];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls i_vls = BU_VLS_INIT_ZERO;
    bv_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_tcl.gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_tcl.gv_data_polygons;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &fx, &fy, x, y);

    int snapped = 0;
    if (gedp->ged_gvp->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(gedp->ged_gvp, &fx, &fy);
    }
    if (!snapped && gedp->ged_gvp->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(gedp->ged_gvp, &fx, &fy);
    }

    bu_vls_printf(&plist, "{0 ");

    {
	vect_t vdiff;
	fastf_t r, arc;
	fastf_t curr_fx, curr_fy;
	register int nsegs, n;

	VSET(v_pt, fx, fy, gdvp->gv_data_vZ);
	VSUB2(vdiff, v_pt, gdpsp->gdps_prev_point);
	r = MAGNITUDE(vdiff);

	/* use a variable number of segments based on the size of the
	 * circle being created so small circles have few segments and
	 * large ones are nice and smooth.  select a chord length that
	 * results in segments approximately 4 pixels in length.
	 *
	 * circumference / 4 = PI * diameter / 4
	 *
	 */
	nsegs = M_PI_2 * r * gdvp->gv_scale;

	if (nsegs < 32)
	    nsegs = 32;

	arc = 360.0 / nsegs;
	for (n = 0; n < nsegs; ++n) {
	    fastf_t ang = n * arc;

	    curr_fx = cos(ang*DEG2RAD) * r + gdpsp->gdps_prev_point[X];
	    curr_fy = sin(ang*DEG2RAD) * r + gdpsp->gdps_prev_point[Y];
	    VSET(v_pt, curr_fx, curr_fy, gdvp->gv_data_vZ);
	    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
	    bu_vls_printf(&plist, " {%lf %lf %lf}", V3ARGS(m_pt));
	}
    }

    bu_vls_printf(&plist, " }");
    bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);

    gedp->ged_gvp = gdvp;
    ac = 4;
    av[0] = "data_polygons";
    av[1] = "replace_poly";
    av[2] = bu_vls_addr(&i_vls);
    av[3] = bu_vls_addr(&plist);
    av[4] = (char *)0;

    (void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
    bu_vls_free(&plist);
    bu_vls_free(&i_vls);

    return BRLCAD_OK;
}


int
go_mouse_poly_cont(Tcl_Interp *interp,
		   struct ged *gedp,
		   struct bview *gdvp,
		   int argc,
		   const char *argv[],
		   const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_mouse_poly_cont_func(interp, gedp, gdvp, argc, argv, usage);
}


int
to_mouse_poly_cont(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* shift the command name to argv[1] before calling to_mouse_poly_cont_func */
    argv[1] = argv[0];
    ret = to_mouse_poly_cont_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage);
#if 0
    if (ret == BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
#endif

    to_refresh_view(gdvp);

    return ret;
}


int
to_mouse_poly_cont_func(Tcl_Interp *interp,
			struct ged *gedp,
			struct bview *gdvp,
			int UNUSED(argc),
			const char *argv[],
			const char *usage)
{
    int ac;
    char *av[7];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    bv_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_tcl.gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_tcl.gv_data_polygons;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &fx, &fy, x, y);
    VSET(v_pt, fx, fy, gdvp->gv_data_vZ);

    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
    gedp->ged_gvp = gdvp;

    {
	struct bu_vls i_vls = BU_VLS_INIT_ZERO;
	struct bu_vls k_vls = BU_VLS_INIT_ZERO;
	struct bu_vls plist = BU_VLS_INIT_ZERO;

	bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);
	bu_vls_printf(&k_vls, "%zu", gdpsp->gdps_curr_point_i);
	bu_vls_printf(&plist, "%lf %lf %lf", V3ARGS(m_pt));

	ac = 6;
	av[0] = "data_polygons";
	av[1] = "replace_point";
	av[2] = bu_vls_addr(&i_vls);
	av[3] = "0";
	av[4] = bu_vls_addr(&k_vls);
	av[5] = bu_vls_addr(&plist);
	av[6] = (char *)0;

	(void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
	bu_vls_free(&i_vls);
	bu_vls_free(&k_vls);
	bu_vls_free(&plist);
    }

    return BRLCAD_OK;
}


int
go_mouse_poly_ell(Tcl_Interp *interp,
		  struct ged *gedp,
		  struct bview *gdvp,
		  int argc,
		  const char *argv[],
		  const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_mouse_poly_ell_func(interp, gedp, gdvp, argc, argv, usage);
}


int
to_mouse_poly_ell(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* shift the command name to argv[1] before calling to_mouse_poly_ell_func */
    argv[1] = argv[0];
    ret = to_mouse_poly_ell_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage);
#if 0
    if (ret == BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
#endif

    to_refresh_view(gdvp);

    return ret;
}


int
to_mouse_poly_ell_func(Tcl_Interp *interp,
		       struct ged *gedp,
		       struct bview *gdvp,
		       int UNUSED(argc),
		       const char *argv[],
		       const char *usage)
{
    int ac;
    char *av[5];
    int x, y;
    fastf_t fx, fy;
    point_t m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls i_vls = BU_VLS_INIT_ZERO;
    bv_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_tcl.gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_tcl.gv_data_polygons;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;


    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &fx, &fy, x, y);

    int snapped = 0;
    if (gedp->ged_gvp->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(gedp->ged_gvp, &fx, &fy);
    }
    if (!snapped && gedp->ged_gvp->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(gedp->ged_gvp, &fx, &fy);
    }

    bu_vls_printf(&plist, "{0 ");

    {
	fastf_t a, b, arc;
	point_t ellout;
	point_t A, B;
	register int nsegs, n;

	a = fx - gdpsp->gdps_prev_point[X];
	b = fy - gdpsp->gdps_prev_point[Y];

	/*
	 * For angle alpha, compute surface point as
	 *
	 * V + cos(alpha) * A + sin(alpha) * B
	 *
	 * note that sin(alpha) is cos(90-alpha).
	 */

	VSET(A, a, 0, gdvp->gv_data_vZ);
	VSET(B, 0, b, gdvp->gv_data_vZ);

	/* use a variable number of segments based on the size of the
	 * circle being created so small circles have few segments and
	 * large ones are nice and smooth.  select a chord length that
	 * results in segments approximately 4 pixels in length.
	 *
	 * circumference / 4 = PI * diameter / 4
	 *
	 */
	nsegs = M_PI_2 * FMAX(a, b) * gdvp->gv_scale;

	if (nsegs < 32)
	    nsegs = 32;

	arc = 360.0 / nsegs;
	for (n = 0; n < nsegs; ++n) {
	    fastf_t cosa = cos(n * arc * DEG2RAD);
	    fastf_t sina = sin(n * arc * DEG2RAD);

	    VJOIN2(ellout, gdpsp->gdps_prev_point, cosa, A, sina, B);
	    MAT4X3PNT(m_pt, gdvp->gv_view2model, ellout);
	    bu_vls_printf(&plist, " {%lf %lf %lf}", V3ARGS(m_pt));
	}
    }

    bu_vls_printf(&plist, " }");
    bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);

    gedp->ged_gvp = gdvp;
    ac = 4;
    av[0] = "data_polygons";
    av[1] = "replace_poly";
    av[2] = bu_vls_addr(&i_vls);
    av[3] = bu_vls_addr(&plist);
    av[4] = (char *)0;

    (void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
    bu_vls_free(&plist);
    bu_vls_free(&i_vls);

    return BRLCAD_OK;
}


int
go_mouse_poly_rect(Tcl_Interp *interp,
		   struct ged *gedp,
		   struct bview *gdvp,
		   int argc,
		   const char *argv[],
		   const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_mouse_poly_rect_func(interp, gedp, gdvp, argc, argv, usage);
}


int
to_mouse_poly_rect(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* shift the command name to argv[1] before calling to_mouse_poly_rect_func */
    argv[1] = argv[0];
    ret = to_mouse_poly_rect_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage);
#if 0
    if (ret == BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
#endif

    to_refresh_view(gdvp);

    return ret;
}


int
to_mouse_poly_rect_func(Tcl_Interp *interp,
			struct ged *gedp,
			struct bview *gdvp,
			int UNUSED(argc),
			const char *argv[],
			const char *usage)
{
    int ac;
    char *av[5];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls i_vls = BU_VLS_INIT_ZERO;
    bv_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_tcl.gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_tcl.gv_data_polygons;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);
    bv_screen_to_view(gdvp, &fx, &fy, x, y);

    int snapped = 0;
    if (gedp->ged_gvp->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(gedp->ged_gvp, &fx, &fy);
    }
    if (!snapped && gedp->ged_gvp->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(gedp->ged_gvp, &fx, &fy);
    }


    if (gdvp->gv_tcl.gv_polygon_mode == TCLCAD_POLY_SQUARE_MODE) {
	fastf_t dx, dy;

	dx = fx - gdpsp->gdps_prev_point[X];
	dy = fy - gdpsp->gdps_prev_point[Y];

	if (fabs(dx) > fabs(dy)) {
	    if (dy < 0.0)
		fy = gdpsp->gdps_prev_point[Y] - fabs(dx);
	    else
		fy = gdpsp->gdps_prev_point[Y] + fabs(dx);
	} else {
	    if (dx < 0.0)
		fx = gdpsp->gdps_prev_point[X] - fabs(dy);
	    else
		fx = gdpsp->gdps_prev_point[X] + fabs(dy);
	}
    }

    MAT4X3PNT(m_pt, gdvp->gv_view2model, gdpsp->gdps_prev_point);
    bu_vls_printf(&plist, "{0 {%lf %lf %lf} ",  V3ARGS(m_pt));

    VSET(v_pt, gdpsp->gdps_prev_point[X], fy, gdvp->gv_data_vZ);
    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
    bu_vls_printf(&plist, "{%lf %lf %lf} ",  V3ARGS(m_pt));

    VSET(v_pt, fx, fy, gdvp->gv_data_vZ);
    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
    bu_vls_printf(&plist, "{%lf %lf %lf} ",  V3ARGS(m_pt));
    VSET(v_pt, fx, gdpsp->gdps_prev_point[Y], gdvp->gv_data_vZ);
    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
    bu_vls_printf(&plist, "{%lf %lf %lf} }",  V3ARGS(m_pt));

    bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);

    gedp->ged_gvp = gdvp;
    ac = 4;
    av[0] = "data_polygons";
    av[1] = "replace_poly";
    av[2] = bu_vls_addr(&i_vls);
    av[3] = bu_vls_addr(&plist);
    av[4] = (char *)0;

    (void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
    bu_vls_free(&plist);
    bu_vls_free(&i_vls);

    return BRLCAD_OK;
}


int
to_mouse_ray(struct ged *UNUSED(gedp),
	     int UNUSED(argc),
	     const char *UNUSED(argv[]),
	     ged_func_ptr UNUSED(func),
	     const char *UNUSED(usage),
	     int UNUSED(maxargs))
{
    return BRLCAD_OK;
}


int
to_mouse_rect(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    int ret;
    int ac;
    char *av[5];
    int x, y;
    int dx, dy;
    struct bu_vls dx_vls = BU_VLS_INIT_ZERO;
    struct bu_vls dy_vls = BU_VLS_INIT_ZERO;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = dm_get_height((struct dm *)gdvp->dmp) - y - gdvp->gv_prevMouseY;

    bu_vls_printf(&dx_vls, "%d", dx);
    bu_vls_printf(&dy_vls, "%d", dy);
    gedp->ged_gvp = gdvp;
    ac = 4;
    av[0] = "rect";
    av[1] = "dim";
    av[2] = bu_vls_addr(&dx_vls);
    av[3] = bu_vls_addr(&dy_vls);
    av[4] = (char *)0;

    ret = ged_exec(gedp, ac, (const char **)av);
    bu_vls_free(&dx_vls);
    bu_vls_free(&dy_vls);

    if (ret == BRLCAD_OK)
	to_refresh_view(gdvp);

    return BRLCAD_OK;
}


int
to_mouse_rot(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    int ret;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    struct bu_vls rot_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gv_prevMouseY - y;
    dy = gdvp->gv_prevMouseX - x;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    dx *= gdvp->gv_rscale;
    dy *= gdvp->gv_rscale;

    bu_vls_printf(&rot_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp;
    ac = 3;
    av[0] = "rot";
    av[1] = "-v";
    av[2] = bu_vls_addr(&rot_vls);
    av[3] = (char *)0;

    ret = ged_exec(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == BRLCAD_OK) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback));
	}

	to_refresh_view(gdvp);
    }

    return BRLCAD_OK;
}


int
to_mouse_rotate_arb_face(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr UNUSED(func),
			 const char *usage,
			 int UNUSED(maxargs))
{
    int ret;
    char *av[6];
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 7) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[5], "%lf", &x) != 1 ||
	bu_sscanf(argv[6], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = y - gdvp->gv_prevMouseY;
    dy = x - gdvp->gv_prevMouseX;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    dx *= gdvp->gv_rscale;
    dy *= gdvp->gv_rscale;

    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp;
    av[0] = "rotate_arb_face";
    av[1] = (char *)argv[2];
    av[2] = (char *)argv[3];
    av[3] = (char *)argv[4];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = ged_exec(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


#define TO_COMMON_MOUSE_SCALE(_gdvp, _zoom_vls, _argc, _argv, _usage) { \
	int _width; \
	fastf_t _dx, _dy; \
	fastf_t _inv_width; \
	fastf_t _sf; \
 \
	/* must be double for scanf */ \
	double _x, _y; \
 \
	/* initialize result */ \
	bu_vls_trunc(gedp->ged_result_str, 0); \
 \
	/* must be wanting help */ \
	if ((_argc) == 1) { \
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", (_argv)[0], (_usage)); \
	    return BRLCAD_HELP; \
	} \
 \
	if ((_argc) != 4) { \
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", (_argv)[0], (_usage)); \
	    return BRLCAD_ERROR; \
	} \
 \
        gdvp = ged_find_view(gedp, argv[1]); \
        if (!gdvp) { \
	    bu_vls_printf(gedp->ged_result_str, "View not found - %s", (_argv)[1]); \
	    return BRLCAD_ERROR; \
	} \
 \
	if (bu_sscanf((_argv)[2], "%lf", &_x) != 1 || \
	    bu_sscanf((_argv)[3], "%lf", &_y) != 1) { \
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", (_argv)[0], (_usage)); \
	    return BRLCAD_ERROR; \
	} \
 \
	_dx = _x - (_gdvp)->gv_prevMouseX; \
	_dy = (_gdvp)->gv_prevMouseY - _y; \
 \
	(_gdvp)->gv_prevMouseX = _x; \
	(_gdvp)->gv_prevMouseY = _y; \
 \
	if (_dx < (_gdvp)->gv_minMouseDelta) \
	    _dx = (_gdvp)->gv_minMouseDelta; \
	else if ((_gdvp)->gv_maxMouseDelta < _dx) \
	    _dx = (_gdvp)->gv_maxMouseDelta; \
 \
	if (_dy < (_gdvp)->gv_minMouseDelta) \
	    _dy = (_gdvp)->gv_minMouseDelta; \
	else if ((_gdvp)->gv_maxMouseDelta < _dy) \
	    _dy = (_gdvp)->gv_maxMouseDelta; \
 \
	_width = dm_get_width((struct dm *)(_gdvp)->dmp); \
	_inv_width = 1.0 / (fastf_t)_width; \
	_dx *= _inv_width * (_gdvp)->gv_sscale; \
	_dy *= _inv_width * (_gdvp)->gv_sscale; \
 \
	if (fabs(_dx) > fabs(_dy)) \
	    _sf = 1.0 + _dx; \
	else \
	    _sf = 1.0 + _dy; \
 \
	bu_vls_printf(&(_zoom_vls), "%lf", _sf);	\
    }

/*
 * Usage: data_scale vname dtype sf
 */
static int
to_data_scale(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    register int i;
    fastf_t sf;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    usage = "vname dtype sf";
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &sf) != 1 || sf < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid scale factor - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    /* scale data arrows */
    {
	struct bv_data_arrow_state *gdasp = &gdvp->gv_tcl.gv_data_arrows;
	point_t vcenter = {0, 0, 0};

	/* Scale the length of each arrow */
	for (i = 0; i < gdasp->gdas_num_points; i += 2) {
	    vect_t diff;
	    point_t vpoint;

	    MAT4X3PNT(vpoint, gedp->ged_gvp->gv_model2view, gdasp->gdas_points[i]);
	    vcenter[Z] = vpoint[Z];
	    VSUB2(diff, vpoint, vcenter);
	    VSCALE(diff, diff, sf);
	    VADD2(vpoint, vcenter, diff);
	    MAT4X3PNT(gdasp->gdas_points[i], gedp->ged_gvp->gv_view2model, vpoint);
	}
    }

    /* scale data labels */
    {
	struct bv_data_label_state *gdlsp = &gdvp->gv_tcl.gv_data_labels;
	point_t vcenter = {0, 0, 0};
	point_t vpoint;

	/* Scale the location of each label WRT the view center */
	for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	    vect_t diff;

	    MAT4X3PNT(vpoint, gedp->ged_gvp->gv_model2view, gdlsp->gdls_points[i]);
	    vcenter[Z] = vpoint[Z];
	    VSUB2(diff, vpoint, vcenter);
	    VSCALE(diff, diff, sf);
	    VADD2(vpoint, vcenter, diff);
	    MAT4X3PNT(gdlsp->gdls_points[i], gedp->ged_gvp->gv_view2model, vpoint);
	}
    }


    to_refresh_view(gdvp);
    return BRLCAD_OK;
}

int
to_mouse_data_scale(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    int ret;
    char *av[4];
    struct bu_vls scale_vls = BU_VLS_INIT_ZERO;
    struct bview *gdvp;

    TO_COMMON_MOUSE_SCALE(gdvp, scale_vls, argc, argv, usage);
    gedp->ged_gvp = gdvp;

    av[0] = "to_data_scale";
    av[1] = (char *)argv[1];
    av[2] = bu_vls_addr(&scale_vls);
    av[3] = (char *)0;

    ret = to_data_scale(gedp, 3, (const char **)av, (ged_func_ptr)NULL, NULL, 4);

    bu_vls_free(&scale_vls);

    return ret;
}


int
to_mouse_scale(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    int ret;
    char *av[3];
    struct bu_vls zoom_vls = BU_VLS_INIT_ZERO;
    struct bview *gdvp;

    TO_COMMON_MOUSE_SCALE(gdvp, zoom_vls, argc, argv, usage);
    gedp->ged_gvp = gdvp;

    av[0] = "zoom";
    av[1] = bu_vls_addr(&zoom_vls);
    av[2] = (char *)0;
    ret = ged_exec(gedp, 2, (const char **)av);
    bu_vls_free(&zoom_vls);

    if (ret == BRLCAD_OK) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback));
	}

	to_refresh_view(gdvp);
    }

    return BRLCAD_OK;
}


int
to_mouse_protate(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    int ret;
    char *av[6];
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls mrot_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = y - gdvp->gv_prevMouseY;
    dy = x - gdvp->gv_prevMouseX;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    dx *= gdvp->gv_rscale;
    dy *= gdvp->gv_rscale;

    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&mrot_vls, "%lf %lf %lf", V3ARGS(model));

    gedp->ged_gvp = gdvp;
    av[0] = "protate";
    av[1] = (char *)argv[2];
    av[2] = (char *)argv[3];
    av[3] = bu_vls_addr(&mrot_vls);
    av[4] = (char *)0;

    ret = ged_exec(gedp, 4, (const char **)av);
    bu_vls_free(&mrot_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_pscale(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr UNUSED(func),
		const char *usage,
		int UNUSED(maxargs))
{
    int ret, width;
    char *av[6];
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls sf_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gv_sscale;
    dy *= inv_width * gdvp->gv_sscale;

    if (fabs(dx) < fabs(dy))
	sf = 1.0 + dy;
    else
	sf = 1.0 + dx;

    bu_vls_printf(&sf_vls, "%lf", sf);

    gedp->ged_gvp = gdvp;
    av[0] = "pscale";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&sf_vls);
    av[5] = (char *)0;

    ret = ged_exec(gedp, 5, (const char **)av);
    bu_vls_free(&sf_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_ptranslate(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    int ret, width;
    char *av[6];
    fastf_t dx, dy;
    point_t model;
    point_t view;
    fastf_t inv_width;
    mat_t inv_rot;
    struct bu_vls tvec_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gv_prevMouseX;
    dy = gdvp->gv_prevMouseY - y;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_ptranslate expects things to be in local units */
    dx *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gv_size * gedp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&tvec_vls, "%lf %lf %lf", V3ARGS(model));

    gedp->ged_gvp = gdvp;
    av[0] = "ptranslate";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&tvec_vls);
    av[5] = (char *)0;

    ret = ged_exec(gedp, 5, (const char **)av);
    bu_vls_free(&tvec_vls);

    if (ret == BRLCAD_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


int
to_mouse_trans(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    int ret, width;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    fastf_t inv_width;
    struct bu_vls trans_vls = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gv_prevMouseX - x;
    dy = y - gdvp->gv_prevMouseY;

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (dx < gdvp->gv_minMouseDelta)
	dx = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dx)
	dx = gdvp->gv_maxMouseDelta;

    if (dy < gdvp->gv_minMouseDelta)
	dy = gdvp->gv_minMouseDelta;
    else if (gdvp->gv_maxMouseDelta < dy)
	dy = gdvp->gv_maxMouseDelta;

    width = dm_get_width((struct dm *)gdvp->dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gv_size * gedp->dbip->dbi_local2base;
    dy *= inv_width * gdvp->gv_size * gedp->dbip->dbi_local2base;

    bu_vls_printf(&trans_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp;
    ac = 3;
    av[0] = "tra";
    av[1] = "-v";
    av[2] = bu_vls_addr(&trans_vls);
    av[3] = (char *)0;

    ret = ged_exec(gedp, ac, (const char **)av);
    bu_vls_free(&trans_vls);

    if (ret == BRLCAD_OK) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback));
	}

	to_refresh_view(gdvp);
    }

    return BRLCAD_OK;
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
