/*                         E D S O L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file mged/edsol.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "rt/db4.h"
#include "rt/primitives/arb8.h"
#include "rt/primitives/bot.h"
#include "rt/primitives/nmg.h"
#include "rt/primitives/pipe.h"
#include "ged/view.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"
#include "./menu.h"

static void init_sedit_vars(struct mged_state *), init_oedit_vars(struct mged_state *), init_oedit_guts(struct mged_state *);

// FIXME:  Globals
/* data for solid editing */
int sedraw;	/* apply solid editing changes */

fastf_t es_m[3];		/* edge(line) slope */

/* if (!both) then set only MEDIT(s)->curr_e_axes_pos, otherwise
   set e_axes_pos and MEDIT(s)->curr_e_axes_pos */
int
set_e_axes_pos_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *id)
{
    struct mged_state *s = (struct mged_state *)d;
    int *flag = (int *)id;
    int both = *flag;

    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    struct rt_db_internal *ip = &MEDIT(s)->es_int;

    if (EDOBJ[ip->idb_type].ft_e_axes_pos) {
	bu_vls_trunc(MEDIT(s)->log_str, 0);
	(*EDOBJ[ip->idb_type].ft_e_axes_pos)(MEDIT(s), ip, &s->tol.tol);
	if (bu_vls_strlen(MEDIT(s)->log_str)) {
	    Tcl_AppendResult(s->interp, bu_vls_cstr(MEDIT(s)->log_str), (char *)NULL);
	    bu_vls_trunc(MEDIT(s)->log_str, 0);
	}
	return BRLCAD_OK;
    } else {
	VMOVE(MEDIT(s)->curr_e_axes_pos, MEDIT(s)->e_keypoint);
    }

    if (both) {
	VMOVE(MEDIT(s)->e_axes_pos, MEDIT(s)->curr_e_axes_pos);

	if (EDIT_ROTATE) {
	    s->s_edit->es_edclass = EDIT_CLASS_ROTATE;
	    VSETALL(MEDIT(s)->k.rot_m_abs, 0.0);
	    VSETALL(MEDIT(s)->k.rot_o_abs, 0.0);
	    VSETALL(MEDIT(s)->k.rot_v_abs, 0.0);
	    VSETALL(MEDIT(s)->k.rot_m_abs_last, 0.0);
	    VSETALL(MEDIT(s)->k.rot_o_abs_last, 0.0);
	    VSETALL(MEDIT(s)->k.rot_v_abs_last, 0.0);
	} else if (EDIT_TRAN) {
	    s->s_edit->es_edclass = EDIT_CLASS_TRAN;
	    VSETALL(MEDIT(s)->k.tra_m_abs, 0.0);
	    VSETALL(MEDIT(s)->k.tra_v_abs, 0.0);
	    VSETALL(MEDIT(s)->k.tra_m_abs_last, 0.0);
	    VSETALL(MEDIT(s)->k.tra_v_abs_last, 0.0);
	} else if (EDIT_SCALE) {
	    s->s_edit->es_edclass = EDIT_CLASS_SCALE;

	    if (SEDIT_SCALE) {
		MEDIT(s)->k.sca_abs = 0.0;
		MEDIT(s)->acc_sc_sol = 1.0;
	    }
	} else {
	    s->s_edit->es_edclass = EDIT_CLASS_NULL;
	}

	MAT_IDN(MEDIT(s)->acc_rot_sol);

	for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	    struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	    m_dmp->dm_mged_variables->mv_transform = 'e';
	}
    }

    return BRLCAD_OK;
}

void
set_e_axes_pos(struct mged_state *s, int both)
{
    int flag = both;
    set_e_axes_pos_clbk(0, NULL, (void *)s, (void *)&flag);
}

int
arb_setup_rotface_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *ms = (struct mged_state *)d;
    struct rt_edit *s = MEDIT(ms);
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    int vertex = -1;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    int arb_type = rt_arb_std_type(&s->es_int, s->tol);
    int type = arb_type - 4;
    int loc = aint->edit_menu*4;
    int valid = 0;

    /* check if point 5 is in the face */
    static int pnt5 = 0;
    for (int i=0; i<4; i++) {
	if (rt_arb_vertices[arb_type-4][aint->edit_menu*4+i]==5)
	    pnt5=1;
    }

    /* special case for arb7 */
    if (arb_type == ARB7  && pnt5) {
	bu_vls_printf(s->log_str, "\nFixed vertex is point 5.\n");
	pr_prompt(ms);
	return 5;
    }

    bu_vls_printf(&str, "Enter fixed vertex number(");
    for (int i=0; i<4; i++) {
	if (rt_arb_vertices[type][loc+i])
	    bu_vls_printf(&str, "%d ", rt_arb_vertices[type][loc+i]);
    }
    bu_vls_printf(&str, ") [%d]: ", rt_arb_vertices[type][loc]);

    const struct bu_vls *dnvp = dm_get_dname(ms->mged_curr_dm->dm_dmp);

    bu_vls_printf(&cmd, "cad_input_dialog .get_vertex %s {Need vertex for solid rotate}\
	    {%s} vertex_num %d 0 {{ summary \"Enter a vertex number to rotate about.\"}} OK",
	    (dnvp) ? bu_vls_cstr(dnvp) : "id", bu_vls_cstr(&str), rt_arb_vertices[type][loc]);

    while (!valid) {
	if (Tcl_Eval(ms->interp, bu_vls_addr(&cmd)) != TCL_OK) {
	    bu_vls_printf(s->log_str, "get_rotation_vertex: Error reading vertex\n");
	    /* Using default */
	    pr_prompt(ms);
	    return rt_arb_vertices[type][loc];
	}

	vertex = atoi(Tcl_GetVar(ms->interp, "vertex_num", TCL_GLOBAL_ONLY));
	for (int j=0; j<4; j++) {
	    if (vertex==rt_arb_vertices[type][loc+j])
		valid = 1;
	}
    }

    bu_vls_free(&cmd);
    bu_vls_free(&str);

    pr_prompt(ms);
    return vertex;
}

int
ecmd_bot_mode_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *ms = (struct mged_state *)d;
    struct rt_edit *s = MEDIT(ms);
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->es_int.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    const char *radio_result;
    char mode[10];
    int ret_tcl = TCL_ERROR;

    sprintf(mode, " %d", bot->mode - 1);
    if (dm_get_pathname(ms->mged_curr_dm->dm_dmp)) {
	ret_tcl = Tcl_VarEval(ms->interp, "cad_radio", " .bot_mode_radio ",
		bu_vls_cstr(dm_get_pathname(ms->mged_curr_dm->dm_dmp)), " _bot_mode_result",
		" \"BOT Mode\"", "  \"Select the desired mode\"", mode,
		" { surface volume plate plate/nocosine }",
		" { \"In surface mode, each triangle represents part of a zero thickness surface and no volume is enclosed\" \"In volume mode, the triangles are expected to enclose a volume and that volume becomes the solid\" \"In plate mode, each triangle represents a plate with a specified thickness\" \"In plate/nocosine mode, each triangle represents a plate with a specified thickness, but the LOS thickness reported by the raytracer is independent of obliquity angle\" } ", (char *)NULL);
    }
    if (ret_tcl != TCL_OK) {
	bu_vls_printf(s->log_str, "Mode selection failed!\n");
	return BRLCAD_ERROR;
    }
    radio_result = Tcl_GetVar(ms->interp, "_bot_mode_result", TCL_GLOBAL_ONLY);
    bot->mode = atoi(radio_result) + 1;

    return BRLCAD_OK;
}

int
ecmd_bot_orient_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *s = (struct mged_state *)d;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)MEDIT(s)->es_int.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    const char *radio_result;
    char orient[10];
    int ret_tcl = TCL_ERROR;

    sprintf(orient, " %d", bot->orientation - 1);
    if (dm_get_pathname(DMP)) {
	ret_tcl = Tcl_VarEval(s->interp, "cad_radio", " .bot_orient_radio ",
		bu_vls_addr(dm_get_pathname(DMP)), " _bot_orient_result",
		" \"BOT Face Orientation\"", "  \"Select the desired orientation\"", orient,
		" { none right-hand-rule left-hand-rule }",
		" { \"No orientation means that there is no particular order for the vertices of the triangles\" \"right-hand-rule means that the vertices of each triangle are ordered such that the right-hand-rule produces an outward pointing normal\"  \"left-hand-rule means that the vertices of each triangle are ordered such that the left-hand-rule produces an outward pointing normal\" } ", (char *)NULL);
    }
    if (ret_tcl != TCL_OK) {
	bu_vls_printf(MEDIT(s)->log_str, "Face orientation selection failed!\n");
	return BRLCAD_ERROR;
    }
    radio_result = Tcl_GetVar(s->interp, "_bot_orient_result", TCL_GLOBAL_ONLY);
    bot->orientation = atoi(radio_result) + 1;

    return BRLCAD_OK;
}

int
ecmd_bot_thick_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *s = (struct mged_state *)d;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)MEDIT(s)->es_int.idb_ptr;
    struct rt_bot_edit *b = (struct rt_bot_edit *)MEDIT(s)->ipe_ptr;
    RT_BOT_CK_MAGIC(bot);

    size_t face_no = 0;
    int face_state = 0;

    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	if (Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ", "$mged_gui(mged,screen) ", "{Not Plate Mode} ",
		    "{Cannot edit face thickness in a non-plate BOT} ", "\"\" ", "0 ", "OK ",
		    (char *)NULL) != TCL_OK)
	{
	    bu_log("cad_dialog failed: %s\n", Tcl_GetStringResult(s->interp));
	}
	return BRLCAD_ERROR;
    }

    if (b->bot_verts[0] < 0 || b->bot_verts[1] < 0 || b->bot_verts[2] < 0) {
	/* setting thickness for all faces */
	(void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ",
		"$mged_gui(mged,screen) ", "{Setting Thickness for All Faces} ",
		"{No face is selected, so this operation will modify all the faces in this BOT} ",
		"\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL);
	if (atoi(Tcl_GetStringResult(s->interp)))
	    return BRLCAD_ERROR;

	for (size_t i=0; i<bot->num_faces; i++)
	    bot->thickness[i] = MEDIT(s)->e_para[0];
    } else {
	/* setting thickness for just one face */

	face_state = -1;
	for (size_t i=0; i < bot->num_faces; i++) {
	    if (b->bot_verts[0] == bot->faces[i*3] &&
		    b->bot_verts[1] == bot->faces[i*3+1] &&
		    b->bot_verts[2] == bot->faces[i*3+2])
	    {
		face_no = i;
		face_state = 0;
		break;
	    }
	}
	if (face_state > -1) {
	    bu_log("Cannot find face with vertices %d %d %d!\n", V3ARGS(b->bot_verts));
	    return BRLCAD_ERROR;
	}

	bot->thickness[face_no] = MEDIT(s)->e_para[0];
    }

    return BRLCAD_OK;
}

int
ecmd_bot_flags_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *s = (struct mged_state *)d;
    int ret_tcl = TCL_ERROR;
    const char *dialog_result;
    char cur_settings[11];
    struct rt_bot_internal *bot = (struct rt_bot_internal *)MEDIT(s)->es_int.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    bu_strlcpy(cur_settings, " { 0 0 }", sizeof(cur_settings));

    if (bot->bot_flags & RT_BOT_USE_NORMALS)
	cur_settings[3] = '1';

    if (bot->bot_flags & RT_BOT_USE_FLOATS)
	cur_settings[5] = '1';

    if (dm_get_pathname(DMP)) {
	// TODO - figure out what this is doing...
	ret_tcl = Tcl_VarEval(s->interp,
		"cad_list_buts",
		" .bot_list_flags ",
		bu_vls_addr(dm_get_pathname(DMP)),
		" _bot_flags_result ",
		cur_settings,
		" \"BOT Flags\"",
		" \"Select the desired flags\"",
		" { {Use vertex normals} {Use single precision ray-tracing} }",
		" { {This selection indicates that surface normals at hit points should be interpolated from vertex normals} {This selection indicates that the prepped form of the BOT triangles should use single precision to save memory} } ",
		(char *)NULL);
    }
    if (ret_tcl != TCL_OK) {
	bu_log("ERROR: cad_list_buts: %s\n", Tcl_GetStringResult(s->interp));
	return BRLCAD_ERROR;
    }
    dialog_result = Tcl_GetVar(s->interp, "_bot_flags_result", TCL_GLOBAL_ONLY);

    if (dialog_result[0] == '1') {
	bot->bot_flags |= RT_BOT_USE_NORMALS;
    } else {
	bot->bot_flags &= ~RT_BOT_USE_NORMALS;
    }
    if (dialog_result[2] == '1') {
	bot->bot_flags |= RT_BOT_USE_FLOATS;
    } else {
	bot->bot_flags &= ~RT_BOT_USE_FLOATS;
    }

    return BRLCAD_OK;
}

int
ecmd_bot_fmode_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *s = (struct mged_state *)d;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)MEDIT(s)->es_int.idb_ptr;
    struct rt_bot_edit *b = (struct rt_bot_edit *)MEDIT(s)->ipe_ptr;
    char fmode[10];
    const char *radio_result;
    size_t face_no = 0;
    int face_state = 0;
    int ret_tcl = TCL_ERROR;

    RT_BOT_CK_MAGIC(bot);

    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	(void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ", "$mged_gui(mged,screen) ", "{Not Plate Mode} ",
		"{Cannot edit face mode in a non-plate BOT} ", "\"\" ", "0 ", "OK ",
		(char *)NULL);
	return BRLCAD_ERROR;
    }

    if (b->bot_verts[0] < 0 || b->bot_verts[1] < 0 || b->bot_verts[2] < 0) {
	/* setting mode for all faces */
	(void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ",
		"$mged_gui(mged,screen) ", "{Setting Mode for All Faces} ",
		"{No face is selected, so this operation will modify all the faces in this BOT} ",
		"\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL);
	if (atoi(Tcl_GetStringResult(s->interp)))
	    return BRLCAD_ERROR;

	face_state = -2;
    } else {
	/* setting thickness for just one face */
	face_state = -1;
	for (size_t i=0; i < bot->num_faces; i++) {
	    if (b->bot_verts[0] == bot->faces[i*3] &&
		    b->bot_verts[1] == bot->faces[i*3+1] &&
		    b->bot_verts[2] == bot->faces[i*3+2])
	    {
		face_no = i;
		face_state = 0;
		break;
	    }
	}
	if (face_state < 0) {
	    bu_log("Cannot find face with vertices %d %d %d!\n", V3ARGS(b->bot_verts));
	    return BRLCAD_ERROR;
	}
    }

    if (face_state > -1)
	sprintf(fmode, " %d", BU_BITTEST(bot->face_mode, face_no)?1:0);
    else
	sprintf(fmode, " %d", BU_BITTEST(bot->face_mode, 0)?1:0);

    if (dm_get_pathname(DMP)) {
	ret_tcl = Tcl_VarEval(s->interp, "cad_radio", " .bot_fmode_radio ", bu_vls_addr(dm_get_pathname(DMP)),
		" _bot_fmode_result ", "\"BOT Face Mode\"",
		" \"Select the desired face mode\"", fmode,
		" { {Thickness centered about hit point} {Thickness appended to hit point} }",
		" { {This selection will place the plate thickness centered about the hit point} {This selection will place the plate thickness rayward of the hit point} } ",
		(char *)NULL);
    }
    if (ret_tcl != TCL_OK) {
	bu_log("ERROR: cad_radio: %s\n", Tcl_GetStringResult(s->interp));
	return BRLCAD_ERROR;
    }
    radio_result = Tcl_GetVar(s->interp, "_bot_fmode_result", TCL_GLOBAL_ONLY);

    if (face_state > -1) {
	if (atoi(radio_result))
	    BU_BITSET(bot->face_mode, face_no);
	else
	    BU_BITCLR(bot->face_mode, face_no);
    } else {
	if (atoi(radio_result)) {
	    for (size_t i=0; i<bot->num_faces; i++)
		BU_BITSET(bot->face_mode, i);
	} else
	    bu_bitv_clear(bot->face_mode);
    }

    return BRLCAD_OK;
}

int
ecmd_bot_pickt_multihit_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *d2)
{
    struct mged_state *s = (struct mged_state *)d;
    struct rt_edit *se = (struct rt_edit *)d2;
    struct rt_bot_edit *b = (struct rt_bot_edit *)se->ipe_ptr;
    struct bu_vls *vls = (struct bu_vls *)se->u_ptr;

    // Evil Tcl variable linkage.  Will need to figure out how to do this
    // "on the fly" with temporary s_edit structure internal variables...
    Tcl_LinkVar(s->interp, "bot_v1", (char *)&b->bot_verts[0], TCL_LINK_INT);
    Tcl_LinkVar(s->interp, "bot_v2", (char *)&b->bot_verts[1], TCL_LINK_INT);
    Tcl_LinkVar(s->interp, "bot_v3", (char *)&b->bot_verts[2], TCL_LINK_INT);

    int ret_tcl = Tcl_VarEval(s->interp, "bot_face_select ", bu_vls_cstr(vls), (char *)NULL);
    int ret = BRLCAD_OK;
    if (ret_tcl != TCL_OK) {
	bu_log("bot_face_select failed: %s\n", Tcl_GetStringResult(s->interp));
	b->bot_verts[0] = -1;
	b->bot_verts[1] = -1;
	b->bot_verts[2] = -1;
	ret = BRLCAD_ERROR;
    }
    Tcl_UnlinkVar(s->interp, "bot_v1");
    Tcl_UnlinkVar(s->interp, "bot_v2");
    Tcl_UnlinkVar(s->interp, "bot_v3");
    return ret;
}

int
ecmd_nmg_edebug_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *ms = (struct mged_state *)d;
    struct rt_edit *s = MEDIT(ms);
    struct rt_nmg_edit *en = (struct rt_nmg_edit *)s->ipe_ptr;
    nmg_plot_eu(ms->gedp, en->es_eu, s->tol, s->vlfree);
    return BRLCAD_OK;
}


int
ecmd_extrude_skt_name_clbk(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(d2))
{
    struct mged_state *s = (struct mged_state *)d;
    struct rt_edit *se = MEDIT(s);
    struct rt_extrude_internal *extr = (struct rt_extrude_internal *)se->es_int.idb_ptr;

    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
    bu_vls_printf(&tcl_cmd, "cad_input_dialog .get_sketch_name $mged_gui(mged,screen) {Select Sketch} {Enter the name     of the sketch to be extruded} final_sketch_name %s 0 {{summary \"Enter sketch name\"}} APPLY DISMISS", extr->sketch_name);
    int ret_tcl = Tcl_Eval(s->interp, bu_vls_addr(&tcl_cmd));
    if (ret_tcl != TCL_OK) {
	bu_log("ERROR: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_free(&tcl_cmd);
	return BRLCAD_ERROR;
    }

    if (atoi(Tcl_GetStringResult(s->interp)) == 1)
	return BRLCAD_ERROR;

    bu_vls_free(&tcl_cmd);

    if (extr->sketch_name)
	bu_free((char *)extr->sketch_name, "extr->sketch_name");

    extr->sketch_name = bu_strdup(Tcl_GetVar(s->interp, "final_sketch_name", TCL_GLOBAL_ONLY));

    struct directory *dp = RT_DIR_NULL;
    if ((dp = db_lookup(s->dbip, extr->sketch_name, 0)) == RT_DIR_NULL) {
	bu_log("Warning: %s does not exist!\n",	extr->sketch_name);
	extr->skt = (struct rt_sketch_internal *)NULL;
    } else {
	/* import the new sketch */
	struct rt_db_internal tmp_ip;
	if (rt_db_get_internal(&tmp_ip, dp, s->dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
	    bu_log("rt_extrude_import: ERROR: Cannot import sketch (%.16s) for extrusion\n", extr->sketch_name);
	    extr->skt = (struct rt_sketch_internal *)NULL;
	} else {
	    extr->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
	}
    }

    return BRLCAD_OK;
}

int
f_get_solid_keypoint(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->global_editing_state == ST_VIEW || s->global_editing_state == ST_S_PICK || s->global_editing_state == ST_O_PICK)
	return TCL_OK;

    rt_get_solid_keypoint(MEDIT(s), &MEDIT(s)->e_keypoint, &MEDIT(s)->e_keytag, MEDIT(s)->e_mat);
    return TCL_OK;
}


/*
 * First time in for this solid, set things up.
 * If all goes well, change state to ST_S_EDIT.
 * Solid editing is completed only via sedit_accept() / sedit_reject().
 */
void
init_sedit(struct mged_state *s)
{
    if (s->dbip == DBI_NULL || !illump)
	return;

    /*
     * Check for a processed region or other illegal solid.
     */
    if (illump->s_old.s_Eflag) {
	Tcl_AppendResult(s->interp,
			 "Unable to Solid_Edit a processed region;  select a primitive instead\n", (char *)NULL);
	return;
    }

    if (!illump->s_u_data)
	return;

    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    // TODO - fix
    // MEDIT(s) = rt_edit_create(&bdata->s_fullpath, s->dbip, &s->tol.tol, view_state->vs_gvp);

    /* Finally, enter solid edit state */
    (void)chg_state(s, ST_S_PICK, ST_S_EDIT, "Keyboard illuminate");
    chg_l2menu(s, ST_S_EDIT);
    rt_edit_set_edflag(MEDIT(s), RT_EDIT_IDLE);

    button(s, BE_S_EDIT);	/* Drop into edit menu right away */
    init_sedit_vars(s);

    {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&vls, "begin_edit_callback ");
	db_path_to_vls(&vls, &bdata->s_fullpath);
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
}


static void
init_sedit_vars(struct mged_state *s)
{
    MAT_IDN(MEDIT(s)->acc_rot_sol);
    MAT_IDN(MEDIT(s)->incr_change);

    VSETALL(MEDIT(s)->k.rot_m_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_o_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_v_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_m_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.rot_o_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.rot_v_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.tra_m_abs, 0.0);
    VSETALL(MEDIT(s)->k.tra_v_abs, 0.0);
    VSETALL(MEDIT(s)->k.tra_m_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.tra_v_abs_last, 0.0);
    MEDIT(s)->k.sca_abs = 0.0;
    MEDIT(s)->acc_sc_sol = 1.0;

    VSETALL(MEDIT(s)->k.rot_m, 0.0);
    VSETALL(MEDIT(s)->k.rot_o, 0.0);
    VSETALL(MEDIT(s)->k.rot_v, 0.0);
    VSETALL(MEDIT(s)->k.tra_m, 0.0);
    VSETALL(MEDIT(s)->k.tra_v, 0.0);

    set_e_axes_pos(s, 1);
}


/*
 * All solid edit routines call this subroutine after
 * making a change to es_int or MEDIT(s)->e_mat.
 */
int
replot_editing_solid(int UNUSED(ac), const char **UNUSED(av), void *d, void *UNUSED(id))
{
    struct mged_state *s = (struct mged_state *)d;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    mat_t mat;
    struct bv_scene_obj *sp;
    struct directory *illdp;

    if (!illump) {
	return BRLCAD_OK;
    }
    if (!illump->s_u_data)
	return BRLCAD_OK;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    illdp = LAST_SOLID(bdata);

    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (sp->s_u_data) {
		bdata = (struct ged_bv_data *)sp->s_u_data;
		if (LAST_SOLID(bdata) == illdp) {
		    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);
		    (void)replot_modified_solid(s, sp, &MEDIT(s)->es_int, mat);
		}
	    }
	}

	gdlp = next_gdlp;
    }

    return BRLCAD_OK;
}


/* TODO - this needs dbip because the ft_xform routines are calling ft_export and ft_import
 * under the hood.  Could we get away with a null dbip?  We shouldn't be writing intermediate
 * solids to the disk, unless this routine is also responsible for writing out geometry.
 */ 
void
transform_editing_solid(
    struct mged_state *s,
    struct rt_db_internal *os,		/* output solid */
    const mat_t mat,
    struct rt_db_internal *is,		/* input solid */
    int freeflag)
{
    if (rt_matrix_transform(os, mat, is, freeflag, s->dbip, &rt_uniresource) < 0)
	bu_exit(EXIT_FAILURE, "transform_editing_solid failed to apply a matrix transform, aborting");
}


/*
 * Put up menu header
 */
void
sedit_menu(struct mged_state *s) {

    menu_state->ms_flag = 0;		/* No menu item selected yet */

    mmenu_set_all(s, MENU_L1, NULL);
    chg_l2menu(s, ST_S_EDIT);

    const struct rt_db_internal *ip = &MEDIT(s)->es_int;
    if (EDOBJ[ip->idb_type].ft_menu_item) {
	bu_vls_trunc(MEDIT(s)->log_str, 0);
	struct rt_edit_menu_item *mi = (*EDOBJ[ip->idb_type].ft_menu_item)(&s->tol.tol);
	if (bu_vls_strlen(MEDIT(s)->log_str)) {
	    Tcl_AppendResult(s->interp, bu_vls_cstr(MEDIT(s)->log_str), (char *)NULL);
	    bu_vls_trunc(MEDIT(s)->log_str, 0);
	}
	mmenu_set_all(s, MENU_L1, mi);
    }

    rt_edit_set_edflag(MEDIT(s), RT_EDIT_IDLE);	/* Drop out of previous edit mode */
}

char *
get_sketch_name(struct mged_state *s, const char *sk_n)
{
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
    bu_vls_printf(&tcl_cmd, "cad_input_dialog .get_sketch_name $mged_gui(mged,screen) {Select Sketch} {Enter the name   of the sketch to be extruded} final_sketch_name %s 0 {{summary \"Enter sketch name\"}} APPLY DISMISS", sk_n);
    int ret_tcl = Tcl_Eval(s->interp, bu_vls_addr(&tcl_cmd));
    if (ret_tcl != TCL_OK) {
	bu_log("ERROR: %s\n", Tcl_GetStringResult(s->interp));
	bu_vls_free(&tcl_cmd);
	return NULL;
    }

    if (atoi(Tcl_GetStringResult(s->interp)) == 1)
	return NULL;

    bu_vls_free(&tcl_cmd);

    const char *sketch_name = Tcl_GetVar(s->interp, "final_sketch_name", TCL_GLOBAL_ONLY);
    return bu_strdup(sketch_name);
}

/*
 * Mouse (pen) press in graphics area while doing Solid Edit.
 * mousevec [X] and [Y] are in the range -1.0...+1.0, corresponding
 * to viewspace.
 *
 * In order to allow the "p" command to do the same things that
 * a mouse event can, the preferred strategy is to store the value
 * corresponding to what the "p" command would give in MEDIT(s)->e_mparam,
 * set MEDIT(s)->e_mvalid = 1, set sedraw = 1, and return, allowing sedit(s)
 * to actually do the work.
 */
void
sedit_mouse(struct mged_state *s, const vect_t mousevec)
{
    if (MEDIT(s)->edit_flag <= 0)
	return;

    int ret = 0;
    const struct rt_db_internal *ip = &MEDIT(s)->es_int;
    if (EDOBJ[ip->idb_type].ft_edit_xy) {
	bu_vls_trunc(MEDIT(s)->log_str, 0);
	ret = (*EDOBJ[ip->idb_type].ft_edit_xy)(MEDIT(s), mousevec);
	if (bu_vls_strlen(MEDIT(s)->log_str)) {
	    Tcl_AppendResult(s->interp, bu_vls_cstr(MEDIT(s)->log_str), (char *)NULL);
	    bu_vls_trunc(MEDIT(s)->log_str, 0);
	}
    }

    if (ret == BRLCAD_ERROR)
	return;

    rt_edit_process(MEDIT(s));
}

/*
 * Object Edit
 *
 * TODO - this needs to move to the ft_edit_xy callbacks as MATRIX_EDIT
 * cases.  If handled properly, both sedit_mouse and objedit_mouse should
 * pretty much collapse into one function.
 */
void
objedit_mouse(struct mged_state *s, const vect_t mousevec)
{
    /* Local incremental transform for this mouse action */
    mat_t incr_mat;
    MAT_IDN(incr_mat);

    /* Maintain legacy invariant: incr_change starts (and ends) identity */
    MAT_IDN(MEDIT(s)->incr_change);

    // TODO - not using this anymore, revert/fix
//    struct saved_edflags sf = SAVED_EDFLAGS_INIT;
//    save_edflags(&sf, s);

    if (movedir & SARROW) {
	switch (edobj) {
	    case BE_O_SCALE:
		MEDIT(s)->edit_flag = RT_MATRIX_EDIT_SCALE;
		break;
	    case BE_O_XSCALE:
		MEDIT(s)->edit_flag = RT_MATRIX_EDIT_SCALE_X;
		break;
	    case BE_O_YSCALE:
		MEDIT(s)->edit_flag = RT_MATRIX_EDIT_SCALE_Y;
		break;
	    case BE_O_ZSCALE:
		MEDIT(s)->edit_flag = RT_MATRIX_EDIT_SCALE_Z;
		break;
	}
    } else if (movedir & (RARROW|UARROW)) {
	int use_x = (movedir & RARROW) ? 1 : 0;
	int use_y = (movedir & UARROW) ? 1 : 0;
	if (use_x && !use_y)
	    MEDIT(s)->edit_flag = RT_MATRIX_EDIT_TRANS_VIEW_X;
	if (!use_x && use_y)
	    MEDIT(s)->edit_flag = RT_MATRIX_EDIT_TRANS_VIEW_Y;
	if (use_x && use_y)
	    MEDIT(s)->edit_flag = RT_MATRIX_EDIT_TRANS_VIEW_XY;
	MAT_COPY(MEDIT(s)->model2objview, view_state->vs_model2objview);
    } else {
	Tcl_AppendResult(s->interp, "No object edit mode selected;  mouse press ignored\n", (char *)NULL);
	return;
    }

    const struct rt_db_internal *ip = &MEDIT(s)->es_int;
    if (EDOBJ[ip->idb_type].ft_edit_xy) {
	bu_vls_trunc(MEDIT(s)->log_str, 0);
	(*EDOBJ[ip->idb_type].ft_edit_xy)(MEDIT(s), mousevec);
	if (bu_vls_strlen(MEDIT(s)->log_str)) {
	    Tcl_AppendResult(s->interp, bu_vls_cstr(MEDIT(s)->log_str), (char *)NULL);
	    bu_vls_trunc(MEDIT(s)->log_str, 0);
	}
    }

    // TODO - not using this anymore, revert/fix
    //restore_edflags(s, &sf);
    new_edit_mats(s);
}


void
vls_solid(struct mged_state *ms, struct bu_vls *vp, struct rt_edit *s, const mat_t mat)
{
    struct rt_db_internal *ip = &MEDIT(ms)->es_int;
    struct rt_db_internal intern;
    int id;

    RT_DB_INTERNAL_INIT(&intern);

    if (ms->dbip == DBI_NULL)
	return;

    BU_CK_VLS(vp);
    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_type;
    transform_editing_solid(ms, &intern, mat, (struct rt_db_internal *)ip, 0);

    if (id != ID_ARS && id != ID_POLY && id != ID_BOT) {
	if (OBJ[id].ft_describe(vp, &intern, 1 /*verbose*/, ms->dbip->dbi_base2local) < 0)
	    Tcl_AppendResult(ms->interp, "vls_solid: describe error\n", (char *)NULL);
    } else {
	if (OBJ[id].ft_describe(vp, &intern, 0 /* not verbose */, ms->dbip->dbi_base2local) < 0)
	    Tcl_AppendResult(ms->interp, "vls_solid: describe error\n", (char *)NULL);
    }

    if (id == ID_PIPE) {
	struct rt_pipe_edit *p = (struct rt_pipe_edit *)s->ipe_ptr;
	if (p->es_pipe_pnt) {
	    struct rt_pipe_internal *pipeip;
	    struct wdb_pipe_pnt *ps=(struct wdb_pipe_pnt *)NULL;
	    int seg_no = 0;

	    pipeip = (struct rt_pipe_internal *)ip->idb_ptr;
	    RT_PIPE_CK_MAGIC(pipeip);

	    for (BU_LIST_FOR(ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
		seg_no++;
		if (ps == p->es_pipe_pnt)
		    break;
	    }

	    if (ps == p->es_pipe_pnt)
		rt_vls_pipe_pnt(vp, seg_no, &intern, ms->dbip->dbi_base2local);
	}
    }

    rt_db_free_internal(&intern);
}


static void
init_oedit_guts(struct mged_state *s)
{
    const char *strp="";

    /* for safety sake */
    rt_edit_set_edflag(MEDIT(s), RT_EDIT_DEFAULT);
    MAT_IDN(MEDIT(s)->e_mat);

    if (s->dbip == DBI_NULL || !illump) {
	return;
    }

    /*
     * Check for a processed region
     */
    if (illump->s_old.s_Eflag) {
	/* Have a processed (E'd) region - NO key solid.
	 * Use the 'center' as the key
	 */
	VMOVE(MEDIT(s)->e_keypoint, illump->s_center);

	/* The s_center takes the MEDIT(s)->e_mat into account already */
    }

    /* Not an evaluated region - just a regular path ending in a solid */
    if (!illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&MEDIT(s)->es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(s->interp, "init_oedit(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(s->interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);
	}
	rt_db_free_internal(&MEDIT(s)->es_int);
	button(s, BE_REJECT);
	return;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&MEDIT(s)->es_int);

    /* Save aggregate path matrix */
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, MEDIT(s)->e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(MEDIT(s)->e_invmat, MEDIT(s)->e_mat);

    rt_get_solid_keypoint(MEDIT(s), &MEDIT(s)->e_keypoint, &strp, MEDIT(s)->e_mat);
    init_oedit_vars(s);
}


static void
init_oedit_vars(struct mged_state *s)
{
    set_e_axes_pos(s, 1);

    VSETALL(MEDIT(s)->k.rot_m_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_o_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_v_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_m_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.rot_o_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.rot_v_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.tra_m_abs, 0.0);
    VSETALL(MEDIT(s)->k.tra_v_abs, 0.0);
    VSETALL(MEDIT(s)->k.tra_m_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.tra_v_abs_last, 0.0);
    MEDIT(s)->k.sca_abs = 0.0;
    MEDIT(s)->acc_sc_sol = 1.0;
    MEDIT(s)->acc_sc_obj = 1.0;
    VSETALL(MEDIT(s)->acc_sc, 1.0);

    VSETALL(MEDIT(s)->k.rot_m, 0.0);
    VSETALL(MEDIT(s)->k.rot_o, 0.0);
    VSETALL(MEDIT(s)->k.rot_v, 0.0);
    VSETALL(MEDIT(s)->k.tra_m, 0.0);
    VSETALL(MEDIT(s)->k.tra_v, 0.0);

    MAT_IDN(MEDIT(s)->model_changes);
    MAT_IDN(MEDIT(s)->acc_rot_sol);
}


void
init_oedit(struct mged_state *s)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /* do real initialization work */
    init_oedit_guts(s);

    s->s_edit->es_edclass = EDIT_CLASS_NULL;

    /* begin edit callback */
    bu_vls_strcpy(&vls, "begin_edit_callback {}");
    (void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


void oedit_reject(struct mged_state *s);

static void
oedit_apply(struct mged_state *s, int continue_editing)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    /* matrices used to accept editing done from a depth
     * >= 2 from the top of the illuminated path
     */
    mat_t topm;	/* accum matrix from pathpos 0 to i-2 */
    mat_t inv_topm;	/* inverse */
    mat_t deltam;	/* final "changes":  deltam = (inv_topm)(MEDIT(s)->model_changes)(topm) */
    mat_t tempm;

    if (!illump || !illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    switch (ipathpos) {
	case 0:
	    moveHobj(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
		     MEDIT(s)->model_changes);
	    break;
	case 1:
	    moveHinstance(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos-1),
			  DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
			  MEDIT(s)->model_changes);
	    break;
	default:
	    MAT_IDN(topm);
	    MAT_IDN(inv_topm);
	    MAT_IDN(deltam);
	    MAT_IDN(tempm);

	    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, topm, ipathpos-1, &rt_uniresource);

	    bn_mat_inv(inv_topm, topm);

	    bn_mat_mul(tempm, MEDIT(s)->model_changes, topm);
	    bn_mat_mul(deltam, inv_topm, tempm);

	    moveHinstance(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos-1),
			  DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
			  deltam);
	    break;
    }

    /*
     * Redraw all solids affected by this edit.
     * Regenerate a new control list which does not
     * include the solids about to be replaced,
     * so we can safely fiddle the displaylist.
     */
    MEDIT(s)->model_changes[15] = 1000000000;	/* => small ratio */

    /* Now, recompute new chunks of displaylist */
    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (sp->s_iflag == DOWN)
		continue;
	    (void)replot_original_solid(s, sp);

	    if (continue_editing == DOWN) {
		sp->s_iflag = DOWN;
	    }
	}

	gdlp = next_gdlp;
    }
}


void
oedit_accept(struct mged_state *s)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;

    if (s->dbip == DBI_NULL)
	return;

    if (s->dbip->dbi_read_only) {
	oedit_reject(s);

	gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
	while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (sp->s_iflag == DOWN)
		    continue;
		(void)replot_original_solid(s, sp);
		sp->s_iflag = DOWN;
	    }

	    gdlp = next_gdlp;
	}

	bu_log("Sorry, this database is READ-ONLY\n");
	pr_prompt(s);

	return;
    }

    oedit_apply(s, DOWN); /* finished editing */
    oedit_reject(s);
}


void
oedit_reject(struct mged_state *s)
{
    rt_db_free_internal(&MEDIT(s)->es_int);
}


/*
 * Gets the A, B, C of a planar equation from the command line and puts the
 * result into the array es_peqn[] at the position pointed to by the variable
 * 'edit_menu' which is the plane being redefined. This function is only callable
 * when in solid edit and rotating the face of a GENARB8.
 */
int
f_eqn(ClientData clientData, Tcl_Interp *UNUSED(interp), int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (argc < 4 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help eqn");
	Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->global_editing_state != ST_S_EDIT) {
	Tcl_AppendResult(s->interp, "Eqn: must be in solid edit\n", (char *)NULL);
	return TCL_ERROR;
    }

    int ret = rt_arb_f_eqn(MEDIT(s), argc, argv);
    if (ret != BRLCAD_OK)
	return CMD_BAD;

    /* draw the new version of the solid */
    replot_editing_solid(0, NULL, s, NULL);

    /* update display information */
    view_state->vs_flag = 1;

    return TCL_OK;
}


/* Hooks from buttons.c */

/*
 * Copied from sedit_accept - modified to optionally leave
 * solid edit state.
 */
static int
sedit_apply(struct mged_state *s, int accept_flag)
{
    struct directory *dp;

    /* reset internal variables */
    if (EDOBJ[MEDIT(s)->es_int.idb_type].ft_prim_edit_reset)
	(*EDOBJ[MEDIT(s)->es_int.idb_type].ft_prim_edit_reset)(MEDIT(s));

    /* make sure we are in solid edit mode */
    if (!illump) {
	Tcl_UnlinkVar(s->interp, "edit_solid_flag");
	rt_edit_destroy(MEDIT(s));
	MEDIT(s) = NULL;
	mmenu_set(s, MENU_L1, NULL);
	mmenu_set(s, MENU_L2, NULL);
	return TCL_OK;
    }

    /* write editing changes out to disc */
    if (!illump->s_u_data) {
	Tcl_UnlinkVar(s->interp, "edit_solid_flag");
	rt_edit_destroy(MEDIT(s));
	MEDIT(s) = NULL;
	mmenu_set(s, MENU_L1, NULL);
	mmenu_set(s, MENU_L2, NULL);
	return TCL_ERROR;
    }
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    dp = LAST_SOLID(bdata);
    if (!dp) {
	/* sanity check, unexpected error */
	Tcl_UnlinkVar(s->interp, "edit_solid_flag");
	rt_edit_destroy(MEDIT(s));
	MEDIT(s) = NULL;
	mmenu_set(s, MENU_L1, NULL);
	mmenu_set(s, MENU_L2, NULL);
	return TCL_ERROR;
    }

    /* make sure that any BOT solid is minimally legal */
    if (MEDIT(s)->es_int.idb_type == ID_BOT) {
	struct rt_bot_internal *bot = (struct rt_bot_internal *)MEDIT(s)->es_int.idb_ptr;

	RT_BOT_CK_MAGIC(bot);
	if (bot->mode == RT_BOT_SURFACE || bot->mode == RT_BOT_SOLID) {
	    /* make sure facemodes and thicknesses have been freed */
	    if (bot->thickness) {
		bu_free((char *)bot->thickness, "BOT thickness");
		bot->thickness = NULL;
	    }
	    if (bot->face_mode) {
		bu_free((char *)bot->face_mode, "BOT face_mode");
		bot->face_mode = NULL;
	    }
	} else {
	    /* make sure face_modes and thicknesses exist */
	    if (!bot->thickness)
		bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
	    if (!bot->face_mode) {
		bot->face_mode = bu_bitv_new(bot->num_faces);
	    }
	}
    }

    /* Scale change on export is 1.0 -- no change */
    if (rt_db_put_internal(dp, s->dbip, &MEDIT(s)->es_int, &rt_uniresource) < 0) {
	Tcl_AppendResult(s->interp, "sedit_apply(", dp->d_namep,
			 "):  solid export failure\n", (char *)NULL);
	if (accept_flag) {
	    rt_db_free_internal(&MEDIT(s)->es_int);
	}
	Tcl_UnlinkVar(s->interp, "edit_solid_flag");
	rt_edit_destroy(MEDIT(s));
	MEDIT(s) = NULL;
	mmenu_set(s, MENU_L1, NULL);
	mmenu_set(s, MENU_L2, NULL);
	return TCL_ERROR;				/* FAIL */
    }

    if (accept_flag) {
	menu_state->ms_flag = 0;
	movedir = 0;
	MEDIT(s)->edit_flag = -1;
	s->s_edit->es_edclass = EDIT_CLASS_NULL;

	rt_db_free_internal(&MEDIT(s)->es_int);
    } else {
	/* XXX hack to restore MEDIT(s)->es_int after rt_db_put_internal blows it away */
	/* Read solid description into MEDIT(s)->es_int again! Gaak! */
	if (rt_db_get_internal(&MEDIT(s)->es_int, LAST_SOLID(bdata),
			       s->dbip, NULL, &rt_uniresource) < 0) {
	    Tcl_AppendResult(s->interp, "sedit_apply(",
			     LAST_SOLID(bdata)->d_namep,
			     "):  solid reimport failure\n", (char *)NULL);
	    rt_db_free_internal(&MEDIT(s)->es_int);
	    return TCL_ERROR;
	}
    }

    Tcl_UnlinkVar(s->interp, "edit_solid_flag");
    rt_edit_destroy(MEDIT(s));
    MEDIT(s) = NULL;
    mmenu_set(s, MENU_L1, NULL);
    mmenu_set(s, MENU_L2, NULL);
    return TCL_OK;
}


void
sedit_accept(struct mged_state *s)
{
    if (s->dbip == DBI_NULL)
	return;

    if (not_state(s, ST_S_EDIT, "Solid edit accept"))
	return;

    if (s->dbip->dbi_read_only) {
	sedit_reject(s);
	bu_log("Sorry, this database is READ-ONLY\n");
	pr_prompt(s);
	return;
    }

    if (sedraw > 0) {
	MEDIT(s)->update_views = s->update_views;
	sedraw = 0;
	rt_edit_process(MEDIT(s));
	s->update_views = MEDIT(s)->update_views;
    }

    (void)sedit_apply(s, 1);
}


void
sedit_reject(struct mged_state *s)
{
    if (not_state(s, ST_S_EDIT, "Solid edit reject") || !illump) {
	return;
    }

    if (sedraw > 0) {
	MEDIT(s)->update_views = s->update_views;
	sedraw = 0;
	rt_edit_process(MEDIT(s));
	s->update_views = MEDIT(s)->update_views;
    }

    /* Restore the original solid everywhere */
    {
	struct display_list *gdlp;
	struct display_list *next_gdlp;
	struct bv_scene_obj *sp;
	if (!illump->s_u_data) {
	    Tcl_UnlinkVar(s->interp, "edit_solid_flag");
	    rt_edit_destroy(MEDIT(s));
	    MEDIT(s) = NULL;
	    mmenu_set(s, MENU_L1, NULL);
	    mmenu_set(s, MENU_L2, NULL);
	    return;
	}
	struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
	while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (!sp->s_u_data)
		    continue;
		struct ged_bv_data *bdatas = (struct ged_bv_data *)sp->s_u_data;
		if (LAST_SOLID(bdatas) == LAST_SOLID(bdata))
		    (void)replot_original_solid(s, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    menu_state->ms_flag = 0;
    movedir = 0;
    MEDIT(s)->edit_flag = -1;
    s->s_edit->es_edclass = EDIT_CLASS_NULL;

    rt_db_free_internal(&MEDIT(s)->es_int);
}

int
mged_param(struct mged_state *s, Tcl_Interp *interp, int argc, fastf_t *argvect)
{
    int i;

    CHECK_DBI_NULL;

    if (MEDIT(s)->edit_flag <= 0) {
	Tcl_AppendResult(interp,
			 "A solid editor option not selected\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    MEDIT(s)->e_inpara = 0;
    for (i = 0; i < argc; i++) {
	MEDIT(s)->e_para[ MEDIT(s)->e_inpara++ ] = argvect[i];
    }

    MEDIT(s)->update_views = s->update_views;
    sedraw = 0;
    rt_edit_process(MEDIT(s));
    s->update_views = MEDIT(s)->update_views;

    if (SEDIT_TRAN) {
	vect_t diff;
	fastf_t inv_Viewscale = 1/view_state->vs_gvp->gv_scale;

	VSUB2(diff, MEDIT(s)->e_para, MEDIT(s)->e_axes_pos);
	VSCALE(MEDIT(s)->k.tra_m_abs, diff, inv_Viewscale);
	VMOVE(MEDIT(s)->k.tra_m_abs_last, MEDIT(s)->k.tra_m_abs);
    } else if (SEDIT_ROTATE) {
	VMOVE(MEDIT(s)->k.rot_m_abs, MEDIT(s)->e_para);
    } else if (SEDIT_SCALE) {
	MEDIT(s)->k.sca_abs = MEDIT(s)->acc_sc_sol - 1.0;
	if (MEDIT(s)->k.sca_abs > 0)
	    MEDIT(s)->k.sca_abs /= 3.0;
    }
    return TCL_OK;
}


/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
int
f_param(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int i;
    vect_t argvect;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help p");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    for (i = 1; i < argc && i <= 3; i++) {
	argvect[i-1] = atof(argv[i]);
    }

    return mged_param(s, interp, argc-1, argvect);
}


/*
 * Put labels on the vertices of the currently edited solid.
 * XXX This really should use import/export interface! Or be part of it.
 */
void
label_edited_solid(
    struct mged_state *s,
    int *num_lines, // NOTE - currently used only for BOTs
    point_t *lines, // NOTE - currently used only for BOTs
    struct rt_point_labels pl[],
    int max_pl,
    const mat_t xform,
    struct rt_db_internal *ip)
{
    // TODO - is es_int the same as ip here?  If not, why not?
    RT_CK_DB_INTERNAL(ip);

    // First, see if we have an edit-aware labeling method.  If we do, use it.
    if (EDOBJ[ip->idb_type].ft_labels) {
	bu_vls_trunc(MEDIT(s)->log_str, 0);
	(*EDOBJ[ip->idb_type].ft_labels)(num_lines, lines, pl, max_pl, xform, MEDIT(s), &s->tol.tol);
	if (bu_vls_strlen(MEDIT(s)->log_str)) {
	    Tcl_AppendResult(s->interp, bu_vls_cstr(MEDIT(s)->log_str), (char *)NULL);
	    bu_vls_trunc(MEDIT(s)->log_str, 0);
	}
	return;
    }
    // If there is no editing-aware labeling, use standard librt labels
    if (OBJ[ip->idb_type].ft_labels) {
	OBJ[ip->idb_type].ft_labels(pl, max_pl, xform, &MEDIT(s)->es_int, &s->tol.tol);
	return;
    }

    // If we have nothing, NULL the string
    pl[0].str[0] = '\0';
}


/* -------------------------------- */
int
f_keypoint(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (argc < 1 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help keypoint");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((s->global_editing_state != ST_S_EDIT) && (s->global_editing_state != ST_O_EDIT)) {
	state_err(s, "keypoint assignment");
	return TCL_ERROR;
    }

    switch (--argc) {
	case 0:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t key;

		VSCALE(key, MEDIT(s)->e_keypoint, s->dbip->dbi_base2local);
		bu_vls_printf(&tmp_vls, "%s (%g, %g, %g)\n", MEDIT(s)->e_keytag, V3ARGS(key));
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
	case 3:
	    VSET(MEDIT(s)->e_keypoint,
		 atof(argv[1]) * s->dbip->dbi_local2base,
		 atof(argv[2]) * s->dbip->dbi_local2base,
		 atof(argv[3]) * s->dbip->dbi_local2base);
	    MEDIT(s)->e_keytag = "user-specified";
	    MEDIT(s)->e_keyfixed = 1;
	    break;
	case 1:
	    if (BU_STR_EQUAL(argv[1], "reset")) {
		MEDIT(s)->e_keytag = "";
		MEDIT(s)->e_keyfixed = 0;
		rt_get_solid_keypoint(MEDIT(s), &MEDIT(s)->e_keypoint, &MEDIT(s)->e_keytag, MEDIT(s)->e_mat);
		break;
	    }
	    /* fall through */
	default:
	    Tcl_AppendResult(interp, "Usage: 'keypoint [<x y z> | reset]'\n", (char *)NULL);
	    return TCL_ERROR;
    }

    view_state->vs_flag = 1;
    return TCL_OK;
}


int
f_get_sedit_menus(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct rt_db_internal *ip = &MEDIT(s)->es_int;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->global_editing_state != ST_S_EDIT)
	return TCL_ERROR;

    if (EDOBJ[ip->idb_type].ft_menu_str) {
	bu_vls_trunc(MEDIT(s)->log_str, 0);
	int ret = (*EDOBJ[ip->idb_type].ft_menu_str)(&vls, ip, &s->tol.tol);
	if (bu_vls_strlen(MEDIT(s)->log_str)) {
	    Tcl_AppendResult(s->interp, bu_vls_cstr(MEDIT(s)->log_str), (char *)NULL);
	    bu_vls_trunc(MEDIT(s)->log_str, 0);
	}
	if (ret != BRLCAD_OK)
	    return TCL_ERROR;
    }

    Tcl_AppendResult(interp, bu_vls_cstr(&vls), (char *)0);
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_get_sedit(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int status;
    struct rt_db_internal ces_int;
    Tcl_Obj *pto;
    Tcl_Obj *pnto;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel get_sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->global_editing_state != ST_S_EDIT || !illump) {
	Tcl_AppendResult(interp, "get_sed: must be in solid edit state", (char *)0);
	return TCL_ERROR;
    }

    if (illump || !illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    if (argc == 1) {
	struct bu_vls logstr = BU_VLS_INIT_ZERO;

	/* get solid type and parameters */
	RT_CK_DB_INTERNAL(&MEDIT(s)->es_int);
	RT_CK_FUNCTAB(MEDIT(s)->es_int.idb_meth);
	status = MEDIT(s)->es_int.idb_meth->ft_get(&logstr, &MEDIT(s)->es_int, (char *)0);
	Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
	pto = Tcl_GetObjResult(interp);

	bu_vls_free(&logstr);

	pnto = Tcl_NewObj();
	/* insert solid name, type and parameters */
	Tcl_AppendStringsToObj(pnto, LAST_SOLID(bdata)->d_namep, " ",
			       Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

	Tcl_SetObjResult(interp, pnto);
	return status;
    }

    if (argv[1][0] != '-' || argv[1][1] != 'c') {
	Tcl_AppendResult(interp, "Usage: get_sed [-c]", (char *)0);
	return TCL_ERROR;
    }

    /* apply matrices along the path */
    RT_DB_INTERNAL_INIT(&ces_int);
    transform_editing_solid(s, &ces_int, MEDIT(s)->e_mat, &MEDIT(s)->es_int, 0);

    /* get solid type and parameters */
    RT_CK_DB_INTERNAL(&ces_int);
    RT_CK_FUNCTAB(ces_int.idb_meth);
    {
	struct bu_vls logstr = BU_VLS_INIT_ZERO;

	status = ces_int.idb_meth->ft_get(&logstr, &ces_int, (char *)0);
	Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
	bu_vls_free(&logstr);
    }
    pto = Tcl_GetObjResult(interp);

    pnto = Tcl_NewObj();
    /* insert full pathname */
    {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	db_path_to_vls(&str, &bdata->s_fullpath);
	Tcl_AppendStringsToObj(pnto, bu_vls_addr(&str), NULL);
	bu_vls_free(&str);
    }

    /* insert solid type and parameters */
    Tcl_AppendStringsToObj(pnto, " ", Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

    Tcl_SetObjResult(interp, pnto);

    rt_db_free_internal(&ces_int);

    return status;
}


int
f_put_sedit(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    const struct rt_functab *ftp;
    uint32_t save_magic;
    int context;

    /*XXX needs better argument checking */
    if (argc < 6) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel put_sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->global_editing_state != ST_S_EDIT) {
	Tcl_AppendResult(interp, "put_sed: must be in solid edit state", (char *)0);
	return TCL_ERROR;
    }

    /* look for -c */
    if (argv[1][0] == '-' && argv[1][1] == 'c') {
	context = 1;
	--argc;
	++argv;
    } else
	context = 0;

    ftp = rt_get_functab_by_label(argv[1]);
    if (ftp == NULL ||
	ftp->ft_parsetab == (struct bu_structparse *)NULL) {
	Tcl_AppendResult(interp, "put_sed: ", argv[1],
			 " object type is not supported for db get",
			 (char *)0);
	return TCL_ERROR;
    }

    RT_CK_FUNCTAB(MEDIT(s)->es_int.idb_meth);
    if (MEDIT(s)->es_int.idb_meth != ftp) {
	Tcl_AppendResult(interp,
			 "put_sed: idb_meth type mismatch",
			 (char *)0);
    }

    save_magic = *((uint32_t *)MEDIT(s)->es_int.idb_ptr);
    *((uint32_t *)MEDIT(s)->es_int.idb_ptr) = ftp->ft_internal_magic;
    {
	int ret;
	struct bu_vls vlog = BU_VLS_INIT_ZERO;

	ret = bu_structparse_argv(&vlog, argc-2, argv+2, ftp->ft_parsetab, (char *)MEDIT(s)->es_int.idb_ptr, NULL);
	Tcl_AppendResult(interp, bu_vls_addr(&vlog), (char *)NULL);
	bu_vls_free(&vlog);
	if (ret != BRLCAD_OK)
	    return TCL_ERROR;
    }
    *((uint32_t *)MEDIT(s)->es_int.idb_ptr) = save_magic;

    if (context)
	transform_editing_solid(s, &MEDIT(s)->es_int, MEDIT(s)->e_invmat, &MEDIT(s)->es_int, 1);

    if (!MEDIT(s)->e_keyfixed)
	rt_get_solid_keypoint(MEDIT(s), &MEDIT(s)->e_keypoint, &MEDIT(s)->e_keytag, MEDIT(s)->e_mat);

    set_e_axes_pos(s, 0);
    replot_editing_solid(0, NULL, s, NULL);

    return TCL_OK;
}


int
f_sedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->global_editing_state != ST_S_EDIT || !illump)
	return TCL_ERROR;

    if (argc != 1) {
	bu_vls_printf(&vls, "helpdevel sed_reset");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* free old copy */
    rt_db_free_internal(&MEDIT(s)->es_int);

    /* reset internal variables */
    if (EDOBJ[MEDIT(s)->es_int.idb_type].ft_prim_edit_reset)
	(*EDOBJ[MEDIT(s)->es_int.idb_type].ft_prim_edit_reset)(MEDIT(s));

    /* read in a fresh copy */
    if (!illump || !illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&MEDIT(s)->es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(interp, "sedit_reset(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);

	}
	return TCL_ERROR;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&MEDIT(s)->es_int);
    replot_editing_solid(0, NULL, s, NULL);

    /* Establish initial keypoint */
    MEDIT(s)->e_keytag = "";
    rt_get_solid_keypoint(MEDIT(s), &MEDIT(s)->e_keypoint, &MEDIT(s)->e_keytag, MEDIT(s)->e_mat);

    /* Reset relevant variables */
    MAT_IDN(MEDIT(s)->acc_rot_sol);
    VSETALL(MEDIT(s)->k.rot_m_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_o_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_v_abs, 0.0);
    VSETALL(MEDIT(s)->k.rot_m_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.rot_o_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.rot_v_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.tra_m_abs, 0.0);
    VSETALL(MEDIT(s)->k.tra_v_abs, 0.0);
    VSETALL(MEDIT(s)->k.tra_m_abs_last, 0.0);
    VSETALL(MEDIT(s)->k.tra_v_abs_last, 0.0);
    MEDIT(s)->k.sca_abs = 0.0;
    MEDIT(s)->acc_sc_sol = 1.0;
    VSETALL(MEDIT(s)->k.rot_m, 0.0);
    VSETALL(MEDIT(s)->k.rot_o, 0.0);
    VSETALL(MEDIT(s)->k.rot_v, 0.0);
    VSETALL(MEDIT(s)->k.tra_m, 0.0);
    VSETALL(MEDIT(s)->k.tra_v, 0.0);

    set_e_axes_pos(s, 1);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_sedit_apply(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    if (not_state(s, ST_S_EDIT, "Solid edit apply")) {
	return TCL_ERROR;
    }

    if (sedraw > 0) {
	MEDIT(s)->update_views = s->update_views;
	sedraw = 0;
	rt_edit_process(MEDIT(s));
	s->update_views = MEDIT(s)->update_views;
    }

    init_sedit_vars(s);
    (void)sedit_apply(s, 0);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_oedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->global_editing_state != ST_O_EDIT)
	return TCL_ERROR;

    if (argc != 1) {
	bu_vls_printf(&vls, "helpdevel oed_reset");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    oedit_reject(s);
    init_oedit_guts(s);

    new_edit_mats(s);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_oedit_apply(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;
    const char *strp="";

    CHECK_DBI_NULL;
    oedit_apply(s, UP); /* apply changes, but continue editing */

    if (!illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    /* Save aggregate path matrix */
    MAT_IDN(MEDIT(s)->e_mat);
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, MEDIT(s)->e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(MEDIT(s)->e_invmat, MEDIT(s)->e_mat);

    rt_get_solid_keypoint(MEDIT(s), &MEDIT(s)->e_keypoint, &strp, MEDIT(s)->e_mat);
    init_oedit_vars(s);
    new_edit_mats(s);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}

/* Extrude command - project an arb face */
/* Format: extrude face distance */
int
f_extrude(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    static int face;
    static fastf_t dist;

    CHECK_DBI_NULL;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help extrude");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_S_EDIT, "Extrude"))
	return TCL_ERROR;

    if (MEDIT(s)->es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Extrude: solid type must be ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    int arb_type = rt_arb_std_type(&MEDIT(s)->es_int, MEDIT(s)->tol);

    if (arb_type != ARB8 && arb_type != ARB6 && arb_type != ARB4) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "ARB%d: extrusion of faces not allowed\n", arb_type);
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	return TCL_ERROR;
    }

    face = atoi(argv[1]);

    /* get distance to project face */
    dist = atof(argv[2]);
    /* apply MEDIT(s)->e_mat[15] to get to real model space */
    /* convert from the local unit (as input) to the base unit */
    dist = dist * MEDIT(s)->e_mat[15] * s->dbip->dbi_local2base;

    struct rt_arb_internal *arb = (struct rt_arb_internal *)MEDIT(s)->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    fastf_t es_peqn[7][4];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, es_peqn, &s->tol.tol)) {
	// TODO - write to a vls so parent code can do Tcl_AppendResult
	bu_log("\nCannot calculate plane equations for ARB8\n");
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    if (arb_extrude(arb, face, dist, MEDIT(s)->tol, es_peqn)) {
	Tcl_AppendResult(interp, "Error extruding ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* draw the updated solid */
    replot_editing_solid(0, NULL, s, NULL);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    return TCL_OK;
}


/* Mirface command - mirror an arb face */
/* Format: mirror face axis */
int
f_mirface(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int face;

    if (argc < 3 || 3 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help mirface");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_S_EDIT, "Mirface"))
	return TCL_ERROR;

    if (MEDIT(s)->es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Mirface: solid type must be ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    struct rt_arb_internal *arb = (struct rt_arb_internal *)MEDIT(s)->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    face = atoi(argv[1]);

    fastf_t es_peqn[7][4];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    int arb_type = rt_arb_std_type(&MEDIT(s)->es_int, MEDIT(s)->tol);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, es_peqn, &s->tol.tol)) {
	// TODO - write to a vls so parent code can do Tcl_AppendResult
	bu_log("\nCannot calculate plane equations for ARB8\n");
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    if (arb_mirror_face_axis(arb, es_peqn, face, argv[2], MEDIT(s)->tol)) {
	Tcl_AppendResult(interp, "Mirface: mirror operation failed\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* draw the updated solid */
    replot_editing_solid(0, NULL, s, NULL);
    view_state->vs_flag = 1;

    return TCL_OK;
}


/* Edgedir command: define the direction of an arb edge being moved
 * Format: edgedir deltax deltay deltaz OR edgedir rot fb
*/
int
f_edgedir(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (argc < 3 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help edgedir");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_S_EDIT, "Edgedir"))
	return TCL_ERROR;

    return rt_arb_edgedir(MEDIT(s), argc, argv);
}

/* Permute command - permute the vertex labels of an ARB
 * Format: permute tuple */

/*
 *	     Minimum and maximum tuple lengths
 *     ------------------------------------------------
 *	Solid	# vertices needed	# vertices
 *	type	 to disambiguate	in THE face
 *     ------------------------------------------------
 *	ARB4		3		    3
 *	ARB5		2		    4
 *	ARB6		2		    4
 *	ARB7		1		    4
 *	ARB8		3		    4
 *     ------------------------------------------------
 */
int
f_permute(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    /*
     * 1) Why were all vars declared static?
     * 2) Recompute plane equations?
     */
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 2 < argc) {
	bu_vls_printf(&vls, "help permute");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (not_state(s, ST_S_EDIT, "Permute"))
	return TCL_ERROR;

    if (MEDIT(s)->es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Permute: solid type must be an ARB\n", (char *)NULL);
	return TCL_ERROR;
    }

    struct rt_arb_internal *arb = (struct rt_arb_internal *)MEDIT(s)->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (arb_permute(arb, argv[1], MEDIT(s)->tol)) {
	Tcl_AppendResult(interp, "Permute failed.\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* draw the updated solid */
    replot_editing_solid(0, NULL, s, NULL);
    view_state->vs_flag = 1;

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
