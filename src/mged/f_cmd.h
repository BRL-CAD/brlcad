/*                         F _ C M D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @file mged/f_cmd.h
 *
 */

#ifndef MGED_F_CMD_H
#define MGED_F_CMD_H

#include "common.h"

#include <tcl.h>

extern int f_adc(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_aip(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_amtrack(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_area(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_attach(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bo(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bomb(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_closedb(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_comm(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_copy_inv(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_dm(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_edcodes(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_edcolor(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_edgedir(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_edmater(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_eqn(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_extrude(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_facedef(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_fhelp(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_get_dm_list(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_get_sedit(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_get_sedit_menus(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_get_solid_keypoint(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_help(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_hideline(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_history(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_ill(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_in(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_joint(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_journal(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_keypoint(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_knob(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_list(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_make(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_matpick(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_mirface(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_model2view(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_mouse(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_nirt(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_oedit_apply(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_oedit_reset(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_opendb(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_param(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_permute(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_plot(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_prefix(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_press(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_postscript(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_put_sedit(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_qorot(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_qray(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_quit(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_red(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_refresh(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_regdebug(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_release(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_rfarb(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_rmats(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_rot_obj(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_rset(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_sc_obj(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_sed(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_sedit_apply(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_sedit_reset(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_set(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_share(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_slewview(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_status(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_svbase(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_tedit(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_tie(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_tr_obj(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_tracker(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_update(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_view(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_view2grid_lu(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_view2model(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_view2model_lu(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_view2model_vec(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_view_ring(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_vnirt(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_wait(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_winset(ClientData, Tcl_Interp *, int, const char *[]);

/* button callbacks, in buttons.c */
extern int f_be_accept(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_illuminate(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_rotate(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_scale(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_x(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_xscale(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_xy(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_y(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_yscale(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_o_zscale(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_reject(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_s_edit(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_s_illuminate(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_s_rotate(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_s_scale(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_be_s_trans(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_35_25(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_45_45(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_bottom(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_front(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_left(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_rate_toggle(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_rear(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_reset(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_right(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_top(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_vrestore(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_vsave(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_zoomin(ClientData, Tcl_Interp *, int, const char *[]);
extern int f_bv_zoomout(ClientData, Tcl_Interp *, int, const char *[]);

#endif /* MGED_F_CMD_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
