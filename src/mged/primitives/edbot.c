/*                         E D B O T . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
/** @file mged/primitives/edbot.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./mged_functab.h"
#include "./edbot.h"

int bot_verts[3];		/* vertices for the BOT solid */

extern int es_mvalid;           /* es_mparam valid.  inpara must = 0 */
extern vect_t es_mparam;        /* mouse input param.  Only when es_mvalid set */

static void
bot_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = arg;

    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item bot_menu[] = {
    { "BOT MENU", NULL, 0 },
    { "Pick Vertex", bot_ed, ECMD_BOT_PICKV },
    { "Pick Edge", bot_ed, ECMD_BOT_PICKE },
    { "Pick Triangle", bot_ed, ECMD_BOT_PICKT },
    { "Move Vertex", bot_ed, ECMD_BOT_MOVEV },
    { "Move Edge", bot_ed, ECMD_BOT_MOVEE },
    { "Move Triangle", bot_ed, ECMD_BOT_MOVET },
    { "Delete Triangle", bot_ed, ECMD_BOT_FDEL },
    { "Select Mode", bot_ed, ECMD_BOT_MODE },
    { "Select Orientation", bot_ed, ECMD_BOT_ORIENT },
    { "Set flags", bot_ed, ECMD_BOT_FLAGS },
    { "Set Face Thickness", bot_ed, ECMD_BOT_THICK },
    { "Set Face Mode", bot_ed, ECMD_BOT_FMODE },
    { "", NULL, 0 }
};

struct menu_item *
mged_bot_menu_item(const struct bn_tol *UNUSED(tol))
{
    return bot_menu;
}

void
mged_bot_labels(
	int *num_lines,
	point_t *lines,
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_db_internal *ip,
	struct bn_tol *UNUSED(tol))
{
    point_t pos_view;
    int npl = 0;

#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;

    // Conditional labeling
    if (bot_verts[2] > -1 &&
	    bot_verts[1] > -1 &&
	    bot_verts[0] > -1)
    {

	RT_BOT_CK_MAGIC(bot);

	/* editing a face */
	point_t mid_pt;
	point_t p1, p2, p3;
	fastf_t one_third = 1.0/3.0;

	MAT4X3PNT(p1, xform, &bot->vertices[bot_verts[0]*3]);
	MAT4X3PNT(p2, xform, &bot->vertices[bot_verts[1]*3]);
	MAT4X3PNT(p3, xform, &bot->vertices[bot_verts[2]*3]);
	VADD3(mid_pt, p1, p2, p3);

	VSCALE(mid_pt, mid_pt, one_third);

	*num_lines = 3;
	VMOVE(lines[0], mid_pt);
	VMOVE(lines[1], p1);
	VMOVE(lines[2], mid_pt);
	VMOVE(lines[3], p2);
	VMOVE(lines[4], mid_pt);
	VMOVE(lines[5], p3);

    } else if (bot_verts[1] > -1 && bot_verts[0] > -1) {

	RT_BOT_CK_MAGIC(bot);

	/* editing an edge */
	point_t mid_pt;

	VBLEND2(mid_pt, 0.5, &bot->vertices[bot_verts[0]*3],
		0.5, &bot->vertices[bot_verts[1]*3]);

	MAT4X3PNT(pos_view, xform, mid_pt);
	POINT_LABEL_STR(pos_view, "edge");

    }
    if (bot_verts[0] > -1) {

	RT_BOT_CK_MAGIC(bot);

	/* editing something, always label the vertex (this is the keypoint) */
	MAT4X3PNT(pos_view, xform, &bot->vertices[bot_verts[0]*3]);
	POINT_LABEL_STR(pos_view, "pt");
    }

    pl[npl].str[0] = '\0';	/* Mark ending */

}

const char *
mged_bot_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol)
{
    const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, keystr, mat, ip, tol);
    // If we're editing, use that position instead
    if (bot_verts[0] > -1) {
	point_t mpt = VINIT_ZERO;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
	RT_BOT_CK_MAGIC(bot);
	VMOVE(mpt, &bot->vertices[bot_verts[0]*3]);
	MAT4X3PNT(*pt, mat, mpt);
    }
    return strp;
}

void
ecmd_bot_mode(struct mged_state *s)
{
    struct rt_bot_internal *bot =
	(struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    const char *radio_result;
    char mode[10];
    int ret_tcl = TCL_ERROR;
    int old_mode;

    RT_BOT_CK_MAGIC(bot);
    old_mode = bot->mode;
    sprintf(mode, " %d", old_mode - 1);
    if (dm_get_pathname(DMP)) {
	ret_tcl = Tcl_VarEval(s->interp, "cad_radio", " .bot_mode_radio ",
		bu_vls_addr(dm_get_pathname(DMP)), " _bot_mode_result",
		" \"BOT Mode\"", "  \"Select the desired mode\"", mode,
		" { surface volume plate plate/nocosine }",
		" { \"In surface mode, each triangle represents part of a zero thickness surface and no volume is enclosed\" \"In volume mode, the triangles are expected to enclose a volume and that volume becomes the solid\" \"In plate mode, each triangle represents a plate with a specified thickness\" \"In plate/nocosine mode, each triangle represents a plate with a specified thickness, but the LOS thickness reported by the raytracer is independent of obliquity angle\" } ", (char *)NULL);
    }
    if (ret_tcl != TCL_OK) {
	Tcl_AppendResult(s->interp, "Mode selection failed!\n", (char *)NULL);
	return;
    }
    radio_result = Tcl_GetVar(s->interp, "_bot_mode_result", TCL_GLOBAL_ONLY);
    bot->mode = atoi(radio_result) + 1;
    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	if (old_mode != RT_BOT_PLATE && old_mode != RT_BOT_PLATE_NOCOS) {
	    /* need to create some thicknesses */
	    bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
	    bot->face_mode = bu_bitv_new(bot->num_faces);
	}
    } else {
	if (old_mode == RT_BOT_PLATE || old_mode == RT_BOT_PLATE_NOCOS) {
	    /* free the per face memory */
	    bu_free((char *)bot->thickness, "BOT thickness");
	    bot->thickness = (fastf_t *)NULL;
	    bu_free((char *)bot->face_mode, "BOT face_mode");
	    bot->face_mode = (struct bu_bitv *)NULL;
	}
    }
}

void
ecmd_bot_orient(struct mged_state *s)
{
    struct rt_bot_internal *bot =
	(struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    const char *radio_result;
    char orient[10];
    int ret_tcl = TCL_ERROR;

    RT_BOT_CK_MAGIC(bot);
    sprintf(orient, " %d", bot->orientation - 1);
    if (dm_get_pathname(DMP)) {
	ret_tcl = Tcl_VarEval(s->interp, "cad_radio", " .bot_orient_radio ",
		bu_vls_addr(dm_get_pathname(DMP)), " _bot_orient_result",
		" \"BOT Face Orientation\"", "  \"Select the desired orientation\"", orient,
		" { none right-hand-rule left-hand-rule }",
		" { \"No orientation means that there is no particular order for the vertices of the triangles\" \"right-hand-rule means that the vertices of each triangle are ordered such that the right-hand-rule produces an outward pointing normal\"  \"left-hand-rule means that the vertices of each triangle are ordered such that the left-hand-rule produces an outward pointing normal\" } ", (char *)NULL);
    }
    if (ret_tcl != TCL_OK) {
	Tcl_AppendResult(s->interp, "Face orientation selection failed!\n", (char *)NULL);
	return;
    }
    radio_result = Tcl_GetVar(s->interp, "_bot_orient_result", TCL_GLOBAL_ONLY);
    bot->orientation = atoi(radio_result) + 1;
}

void
ecmd_bot_thick(struct mged_state *s)
{
    struct rt_bot_internal *bot =
	(struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    size_t face_no = 0;
    int face_state = 0;

    RT_BOT_CK_MAGIC(bot);

    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	if (Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ", "$mged_gui(mged,screen) ", "{Not Plate Mode} ",
		    "{Cannot edit face thickness in a non-plate BOT} ", "\"\" ", "0 ", "OK ",
		    (char *)NULL) != TCL_OK)
	{
	    bu_log("cad_dialog failed: %s\n", Tcl_GetStringResult(s->interp));
	}
	return;
    }

    if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
	/* setting thickness for all faces */
	if (!inpara)
	    return;

	(void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ",
		"$mged_gui(mged,screen) ", "{Setting Thickness for All Faces} ",
		"{No face is selected, so this operation will modify all the faces in this BOT} ",
		"\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL);
	if (atoi(Tcl_GetStringResult(s->interp)))
	    return;

	for (size_t i=0; i<bot->num_faces; i++)
	    bot->thickness[i] = es_para[0];
    } else {
	/* setting thickness for just one face */
	if (!inpara)
	    return;

	face_state = -1;
	for (size_t i=0; i < bot->num_faces; i++) {
	    if (bot_verts[0] == bot->faces[i*3] &&
		    bot_verts[1] == bot->faces[i*3+1] &&
		    bot_verts[2] == bot->faces[i*3+2])
	    {
		face_no = i;
		face_state = 0;
		break;
	    }
	}
	if (face_state > -1) {
	    bu_log("Cannot find face with vertices %d %d %d!\n",
		    V3ARGS(bot_verts));
	    return;
	}

	bot->thickness[face_no] = es_para[0];
    }
}

void
ecmd_bot_flags(struct mged_state *s)
{
    int ret_tcl = TCL_ERROR;
    const char *dialog_result;
    char cur_settings[11];
    struct rt_bot_internal *bot =
	(struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;

    RT_BOT_CK_MAGIC(bot);

    bu_strlcpy(cur_settings, " { 0 0 }", sizeof(cur_settings));

    if (bot->bot_flags & RT_BOT_USE_NORMALS) {
	cur_settings[3] = '1';
    }
    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	cur_settings[5] = '1';
    }

    if (dm_get_pathname(DMP)) {
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
	return;
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
}

void
ecmd_bot_fmode(struct mged_state *s)
{
    struct rt_bot_internal *bot =
	(struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
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
	return;
    }

    if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
	/* setting mode for all faces */
	(void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ",
		"$mged_gui(mged,screen) ", "{Setting Mode for All Faces} ",
		"{No face is selected, so this operation will modify all the faces in this BOT} ",
		"\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL);
	if (atoi(Tcl_GetStringResult(s->interp)))
	    return;

	face_state = -2;
    } else {
	/* setting thickness for just one face */
	face_state = -1;
	for (size_t i=0; i < bot->num_faces; i++) {
	    if (bot_verts[0] == bot->faces[i*3] &&
		    bot_verts[1] == bot->faces[i*3+1] &&
		    bot_verts[2] == bot->faces[i*3+2])
	    {
		face_no = i;
		face_state = 0;
		break;
	    }
	}
	if (face_state < 0) {
	    bu_log("Cannot find face with vertices %d %d %d!\n",
		    V3ARGS(bot_verts));
	    return;
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
	return;
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
}

int
ecmd_bot_fdel(struct mged_state *s)
{
    struct rt_bot_internal *bot =
	(struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;

    int j, face_no;

    RT_BOT_CK_MAGIC(bot);

    if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
	bu_log("No Face selected!\n");
	return BRLCAD_ERROR;
    }

    face_no = -1;
    for (size_t i=0; i < bot->num_faces; i++) {
	if (bot_verts[0] == bot->faces[i*3] &&
		bot_verts[1] == bot->faces[i*3+1] &&
		bot_verts[2] == bot->faces[i*3+2])
	{
	    face_no = i;
	    break;
	}
    }
    if (face_no < 0) {
	bu_log("Cannot find selected face!\n");
	return BRLCAD_ERROR;
    }
    bot->num_faces--;
    for (size_t i=face_no; i<bot->num_faces; i++) {
	j = i + 1;
	bot->faces[3*i] = bot->faces[3*j];
	bot->faces[3*i + 1] = bot->faces[3*j + 1];
	bot->faces[3*i + 2] = bot->faces[3*j + 2];
	if (bot->thickness)
	    bot->thickness[i] = bot->thickness[j];
    }

    if (bot->face_mode) {
	struct bu_bitv *new_bitv;

	new_bitv = bu_bitv_new(bot->num_faces);
	for (size_t i=0; i<(size_t)face_no; i++) {
	    if (BU_BITTEST(bot->face_mode, i))
		BU_BITSET(new_bitv, i);
	}
	for (size_t i=face_no; i<bot->num_faces; i++) {
	    j = i+1;
	    if (BU_BITTEST(bot->face_mode, j))
		BU_BITSET(new_bitv, i);
	}
	bu_bitv_free(bot->face_mode);
	bot->face_mode = new_bitv;
    }
    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;

    return BRLCAD_OK;
}

void
ecmd_bot_movev(struct mged_state *s)
{
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    int vert;
    point_t new_pt = VINIT_ZERO;

    RT_BOT_CK_MAGIC(bot);

    if (bot_verts[0] < 0) {
	bu_log("No BOT point selected\n");
	return;
    }

    if (bot_verts[1] >= 0 && bot_verts[2] >= 0) {
	bu_log("A triangle is selected, not a BOT point!\n");
	return;
    }

    if (bot_verts[1] >= 0) {
	bu_log("An edge is selected, not a BOT point!\n");
	return;
    }

    vert = bot_verts[0];
    if (es_mvalid) {
	VMOVE(new_pt, es_mparam);
    } else if (inpara == 3) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, es_invmat, es_para);
	} else {
	    VMOVE(new_pt, es_para);
	}
    } else if (inpara && inpara != 3) {
	Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!es_mvalid && !inpara) {
	return;
    }

    VMOVE(&bot->vertices[vert*3], new_pt);
}

void
ecmd_bot_movee(struct mged_state *s)
{
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    int v1, v2;
    vect_t diff;
    point_t new_pt = VINIT_ZERO;

    RT_BOT_CK_MAGIC(bot);

    if (bot_verts[0] < 0 || bot_verts[1] < 0) {
	Tcl_AppendResult(s->interp, "No BOT edge selected\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }

    if (bot_verts[2] >= 0) {
	bu_log("A triangle is selected, not a BOT edge!\n");
	return;
    }
    v1 = bot_verts[0];
    v2 = bot_verts[1];
    if (es_mvalid) {
	VMOVE(new_pt, es_mparam);
    } else if (inpara == 3) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, es_invmat, es_para);
	} else {
	    VMOVE(new_pt, es_para);
	}
    } else if (inpara && inpara != 3) {
	Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!es_mvalid && !inpara) {
	return;
    }


    VSUB2(diff, new_pt, &bot->vertices[v1*3]);
    VMOVE(&bot->vertices[v1*3], new_pt);
    VADD2(&bot->vertices[v2*3], &bot->vertices[v2*3], diff);
}

void
ecmd_bot_movet(struct mged_state *s)
{
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    int v1, v2, v3;
    point_t new_pt = VINIT_ZERO;
    vect_t diff;

    RT_BOT_CK_MAGIC(bot);

    if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
	Tcl_AppendResult(s->interp, "No BOT triangle selected\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    }
    v1 = bot_verts[0];
    v2 = bot_verts[1];
    v3 = bot_verts[2];

    if (es_mvalid) {
	VMOVE(new_pt, es_mparam);
    } else if (inpara == 3) {
	if (mged_variables->mv_context) {
	    /* apply es_invmat to convert to real model space */
	    MAT4X3PNT(new_pt, es_invmat, es_para);
	} else {
	    VMOVE(new_pt, es_para);
	}
    } else if (inpara && inpara != 3) {
	Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else if (!es_mvalid && !inpara) {
	return;
    }

    VSUB2(diff, new_pt, &bot->vertices[v1*3]);
    VMOVE(&bot->vertices[v1*3], new_pt);
    VADD2(&bot->vertices[v2*3], &bot->vertices[v2*3], diff);
    VADD2(&bot->vertices[v3*3], &bot->vertices[v3*3], diff);
}

int
ecmd_bot_pickv(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    int tmp_vert;
    char tmp_msg[256];
    point_t selected_pt;

    RT_BOT_CK_MAGIC(bot);

    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];

    tmp_vert = rt_bot_find_v_nearest_pt2(bot, pos_view, view_state->vs_gvp->gv_model2view);
    if (tmp_vert < 0) {
	Tcl_AppendResult(s->interp, "ECMD_BOT_PICKV: unable to find a vertex!\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return BRLCAD_ERROR;
    }

    bot_verts[0] = tmp_vert;
    bot_verts[1] = -1;
    bot_verts[2] = -1;
    VSCALE(selected_pt, &bot->vertices[tmp_vert*3], s->dbip->dbi_base2local);
    sprintf(tmp_msg, "picked point at (%g %g %g), vertex #%d\n", V3ARGS(selected_pt), tmp_vert);
    Tcl_AppendResult(s->interp, tmp_msg, (char *)NULL);
    mged_print_result(s, TCL_OK);

    return BRLCAD_OK;
}

int
ecmd_bot_picke(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    int vert1, vert2;
    char tmp_msg[256];
    point_t from_pt, to_pt;

    RT_BOT_CK_MAGIC(bot);

    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];

    if (rt_bot_find_e_nearest_pt2(&vert1, &vert2, bot, pos_view, view_state->vs_gvp->gv_model2view)) {
	Tcl_AppendResult(s->interp, "ECMD_BOT_PICKE: unable to find an edge!\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return BRLCAD_ERROR;
    }

    bot_verts[0] = vert1;
    bot_verts[1] = vert2;
    bot_verts[2] = -1;
    VSCALE(from_pt, &bot->vertices[vert1*3], s->dbip->dbi_base2local);
    VSCALE(to_pt, &bot->vertices[vert2*3], s->dbip->dbi_base2local);
    sprintf(tmp_msg, "picked edge from (%g %g %g) to (%g %g %g)\n", V3ARGS(from_pt), V3ARGS(to_pt));
    Tcl_AppendResult(s->interp, tmp_msg, (char *)NULL);
    mged_print_result(s, TCL_OK);

    return BRLCAD_OK;
}

void
ecmd_bot_pickt(struct mged_state *s, const vect_t mousevec)
{
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
    point_t start_pt, tmp;
    vect_t dir;
    size_t i;
    int hits, ret_tcl;
    int v1, v2, v3;
    point_t pt1, pt2, pt3;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    RT_BOT_CK_MAGIC(bot);

    VSET(tmp, mousevec[X], mousevec[Y], 0.0);
    MAT4X3PNT(start_pt, view_state->vs_gvp->gv_view2model, tmp);
    VSET(tmp, 0, 0, 1);
    MAT4X3VEC(dir, view_state->vs_gvp->gv_view2model, tmp);

    bu_vls_strcat(&vls, " {");
    hits = 0;
    for (i=0; i<bot->num_faces; i++) {
	v1 = bot->faces[i*3];
	v2 = bot->faces[i*3+1];
	v3 = bot->faces[i*3+2];
	VMOVE(pt1, &bot->vertices[v1*3]);
	VMOVE(pt2, &bot->vertices[v2*3]);
	VMOVE(pt3, &bot->vertices[v3*3]);

	if (bg_does_ray_isect_tri(start_pt, dir, pt1, pt2, pt3, tmp)) {
	    hits++;
	    bu_vls_printf(&vls, " { %d %d %d }", v1, v2, v3);
	}
    }
    bu_vls_strcat(&vls, " } ");

    if (hits == 0) {
	bot_verts[0] = -1;
	bot_verts[1] = -1;
	bot_verts[2] = -1;
	bu_vls_free(&vls);
    }
    if (hits == 1) {
	sscanf(bu_vls_addr(&vls), " { { %d %d %d", &bot_verts[0], &bot_verts[1], &bot_verts[2]);
	bu_vls_free(&vls);
    } else {
	Tcl_LinkVar(s->interp, "bot_v1", (char *)&bot_verts[0], TCL_LINK_INT);
	Tcl_LinkVar(s->interp, "bot_v2", (char *)&bot_verts[1], TCL_LINK_INT);
	Tcl_LinkVar(s->interp, "bot_v3", (char *)&bot_verts[2], TCL_LINK_INT);

	ret_tcl = Tcl_VarEval(s->interp, "bot_face_select ", bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	if (ret_tcl != TCL_OK) {
	    bu_log("bot_face_select failed: %s\n", Tcl_GetStringResult(s->interp));
	    bot_verts[0] = -1;
	    bot_verts[1] = -1;
	    bot_verts[2] = -1;
	    return;
	}
    }
}

int
mged_bot_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    mged_generic_sscale(s, &s->edit_state.es_int);
	    break;
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->edit_state.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->edit_state.es_int);
	    break;
	case ECMD_BOT_MODE:
	    ecmd_bot_mode(s);
	    break;
	case ECMD_BOT_ORIENT:
	    ecmd_bot_orient(s);
	    break;
	case ECMD_BOT_THICK:
	    ecmd_bot_thick(s);
	    break;
	case ECMD_BOT_FLAGS:
	    ecmd_bot_flags(s);
	    break;
	case ECMD_BOT_FMODE:
	    ecmd_bot_fmode(s);
	    break;
	case ECMD_BOT_FDEL:
	    if (ecmd_bot_fdel(s) != BRLCAD_OK)
		return -1;
	    break;
	case ECMD_BOT_MOVEV:
	    ecmd_bot_movev(s);
	    break;
	case ECMD_BOT_MOVEE:
	    ecmd_bot_movee(s);
	    break;
	case ECMD_BOT_MOVET:
	    ecmd_bot_movet(s);
	    break;
	case ECMD_BOT_PICKV:
	case ECMD_BOT_PICKE:
	case ECMD_BOT_PICKT:
	    break;
    }

    return 0;
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
