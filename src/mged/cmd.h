/*                           C M D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
/** @file mged/cmd.h
 *
 */

#ifndef MGED_CMD_H
#define MGED_CMD_H

#include "common.h"

#include "mged.h"

extern int mged_db_search_callback(int, const char **, void *, void*);


/* Commands */

extern int cmd_ged_edit_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_info_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_erase_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_gqa(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_in(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_inside(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_more_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_plain_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_view_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_dm_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_E(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_arot(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_autoview(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_blast(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_center(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_cmd_win(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_draw(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ev(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_get(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_get_more_default(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_has_embedded_fb(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_hist(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_kill(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_list(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_lm(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ls(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_mmenu_get(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_mrot(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_nmg_collapse(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_nop(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_oed(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_output_hook(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_overlay(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ps(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_put(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_rot(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_rrt(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_rt(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_rt_gettrees(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_sca(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_search(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_set_more_default(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_setview(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_shaded_mode(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_size(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_ged_simulate_wrapper(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_stat(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_stub(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_stuff_str(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_tk(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_tol(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_tra(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_units(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_vrot(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_zap(ClientData, Tcl_Interp *, int, const char *[]);
extern int cmd_zoom(ClientData, Tcl_Interp *, int, const char *[]);

#endif /* MGED_CMD_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
