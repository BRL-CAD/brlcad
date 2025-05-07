/*                       E D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2025 United States Government as represented by
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
/** @file edit.cpp
 *
 * Implementation of .g geometry editing logic not specific to individual
 * primitives.
 *
 * TODO - study get, put, and adjust to see how they relate (or don't)
 * to the other edit codes.
 */

#include "common.h"

#include <map>

extern "C" {
#include "vmath.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "rt/edit.h"
#include "rt/functab.h"
}

class RT_Edit_Map_Internal {
    public:
        // Key is ECMD_ type, populated from MGED_Internal map
        std::map<int, std::pair<bu_clbk_t, void *>> cmd_prerun_clbk;
        std::map<int, std::pair<bu_clbk_t, void *>> cmd_during_clbk;
        std::map<int, std::pair<bu_clbk_t, void *>> cmd_postrun_clbk;
        std::map<int, std::pair<bu_clbk_t, void *>> cmd_linger_clbk;

        std::map<bu_clbk_t, int> clbk_recursion_depth_cnt;
        std::map<int, int> cmd_recursion_depth_cnt;
};

struct rt_solid_edit_map {
    RT_Edit_Map_Internal *i;
};

extern "C" struct rt_solid_edit_map *
rt_solid_edit_map_create(void)
{
    struct rt_solid_edit_map *o = NULL;
    BU_GET(o, struct rt_solid_edit_map);
    o->i = new RT_Edit_Map_Internal;
    return o;
}

extern "C" void
rt_solid_edit_map_destroy(struct rt_solid_edit_map *o)
{
    if (!o)
	return;
    delete o->i;
    BU_PUT(o, struct rt_solid_edit_map);
}

struct rt_solid_edit *
rt_solid_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *tol, struct bview *v)
{
    struct rt_solid_edit *s;
    BU_GET(s, struct rt_solid_edit);
    BU_GET(s->m, struct rt_solid_edit_map);
    s->m->i = new RT_Edit_Map_Internal;

    RT_DB_INTERNAL_INIT(&s->es_int);

    MAT_IDN(s->acc_rot_sol);
    MAT_IDN(s->e_invmat);
    MAT_IDN(s->e_mat);
    MAT_IDN(s->incr_change);
    MAT_IDN(s->model_changes);
    VSETALL(s->curr_e_axes_pos , 0);
    VSETALL(s->e_axes_pos , 0);
    VSETALL(s->e_keypoint, 0);
    VSETALL(s->e_mparam, 0);
    VSETALL(s->e_para, 0);

    bv_knobs_reset(&s->k, 0);
    s->k.origin_m = '\0';
    s->k.origin_o = '\0';
    s->k.origin_v = '\0';
    s->k.rot_m_udata = NULL;
    s->k.rot_o_udata = NULL;
    s->k.rot_v_udata = NULL;
    s->k.sca_udata = NULL;
    s->k.tra_m_udata = NULL;
    s->k.tra_v_udata = NULL;

    s->acc_sc_sol = 1.0;
    s->base2local = 1.0;
    s->e_inpara = 0;
    s->e_keyfixed = 0;
    s->e_keytag = NULL;
    s->e_mvalid = 0;
    s->edit_flag = 0;
    s->es_scale = 0.0;
    s->ipe_ptr = NULL;
    s->local2base = 1.0;
    s->mv_context = 0;
    s->solid_edit_mode = RT_SOLID_EDIT_DEFAULT;
    s->tol = tol;
    s->u_ptr = NULL;
    s->update_views = 0;
    s->vlfree = NULL;
    s->vp = v;

    BU_GET(s->log_str, struct bu_vls);
    bu_vls_init(s->log_str);

    if (!dfp || !dbip) {

	// It's preferable to have an existing rt_db_internal to set up the
	// prim specific , but if we don't have it we still need to stub in an
	// empty struct so the logic has *something* to work with.
	if (dfp && DB_FULL_PATH_CUR_DIR(dfp) && EDOBJ[DB_FULL_PATH_CUR_DIR(dfp)->d_minor_type].ft_prim_edit_create)
	    s->ipe_ptr = (*EDOBJ[DB_FULL_PATH_CUR_DIR(dfp)->d_minor_type].ft_prim_edit_create)(NULL);

	return s;
    }

    s->local2base = dbip->dbi_local2base;
    s->base2local = dbip->dbi_base2local;

    if (rt_db_get_internal(&s->es_int, DB_FULL_PATH_CUR_DIR(dfp), dbip, NULL, &rt_uniresource) < 0) {
	rt_solid_edit_destroy(s);
	return NULL;                         /* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->es_int);

    // OK, we have the internal now, set up prim specific struct, if any
    if (dfp && DB_FULL_PATH_CUR_DIR(dfp) && EDOBJ[DB_FULL_PATH_CUR_DIR(dfp)->d_minor_type].ft_prim_edit_create)
	s->ipe_ptr = (*EDOBJ[DB_FULL_PATH_CUR_DIR(dfp)->d_minor_type].ft_prim_edit_create)(s);

    /* Save aggregate path matrix */
    (void)db_path_to_mat(dbip, dfp, s->e_mat, dfp->fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->e_invmat, s->e_mat);

    /* Establish initial keypoint */
    s->e_keytag = "";
    rt_get_solid_keypoint(s, &s->e_keypoint, &s->e_keytag, s->e_mat);

    return s;
}

void
rt_solid_edit_destroy(struct rt_solid_edit *s)
{
    if (!s)
	return;

    struct rt_db_internal *ip = &s->es_int;

    if (s->ipe_ptr && EDOBJ[ip->idb_type].ft_prim_edit_destroy)
	 (*EDOBJ[ip->idb_type].ft_prim_edit_destroy)(s->ipe_ptr);

    rt_db_free_internal(&s->es_int);

    bu_vls_free(s->log_str);
    BU_PUT(s->log_str, struct bu_vls);

    delete s->m->i;
    BU_PUT(s->m, struct rt_solid_edit_map);
    BU_PUT(s, struct rt_solid_edit);
}

int
rt_solid_edit_map_clbk_set(struct rt_solid_edit_map *em, int ed_cmd, int mode, bu_clbk_t f, void *d)
{
    if (!em)
	return BRLCAD_OK;

    RT_Edit_Map_Internal *i = em->i;

    std::map<int, std::pair<bu_clbk_t, void *>> *mp = NULL;
    switch (mode) {
	case BU_CLBK_DURING:
	    mp = &i->cmd_during_clbk;
	    break;
	case BU_CLBK_POST:
	    mp = &i->cmd_postrun_clbk;
	    break;
	case BU_CLBK_LINGER:
	    mp = &i->cmd_linger_clbk;
	    break;
	default:
	    mp = &i->cmd_prerun_clbk;
	    break;
    }

    if (ed_cmd == ECMD_CLEAR_CLBKS) {
	mp->clear();
	return BRLCAD_OK;
    }

    if (!f) {
	mp->erase(ed_cmd);
	return BRLCAD_OK;
    }

    (*mp)[ed_cmd] = std::make_pair(f, d);

    return BRLCAD_OK;
}

int
rt_solid_edit_map_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit_map *em, int ed_cmd, int mode)
{
    // Check for no-op case
    if (!f || !d || !em)
	return BRLCAD_OK;

    RT_Edit_Map_Internal *i = em->i;
    std::map<int, std::pair<bu_clbk_t, void *>> *mp;
    switch (mode) {
	case BU_CLBK_DURING:
	    mp = &i->cmd_during_clbk;
	    break;
	case BU_CLBK_POST:
	    mp = &i->cmd_postrun_clbk;
	    break;
	case BU_CLBK_LINGER:
	    mp = &i->cmd_linger_clbk;
	    break;
	default:
	    mp = &i->cmd_prerun_clbk;
	    break;
    }

    if (mp->find(ed_cmd) == mp->end())
	return BRLCAD_ERROR;

    std::pair<bu_clbk_t, void *> clbk_info = (*mp)[ed_cmd];

    (*f) = clbk_info.first;
    (*d) = clbk_info.second;

    return BRLCAD_OK;
}

int
rt_solid_edit_map_clear(struct rt_solid_edit_map *m)
{
    // Check for no-op case
    if (!m)
	return BRLCAD_OK;

    m->i->cmd_prerun_clbk.clear();
    m->i->cmd_during_clbk.clear();
    m->i->cmd_postrun_clbk.clear();
    m->i->cmd_linger_clbk.clear();
    return BRLCAD_OK;
}

int
rt_solid_edit_map_copy(struct rt_solid_edit_map *om, struct rt_solid_edit_map *im)
{
    // Check for no-op case
    if (!om || !im)
	return BRLCAD_OK;

    const int modes[4] = {BU_CLBK_PRE, BU_CLBK_DURING, BU_CLBK_POST, BU_CLBK_LINGER};
    std::map<int, std::pair<bu_clbk_t, void *>> *ip;
    std::map<int, std::pair<bu_clbk_t, void *>> *op;
    for (int i = 0; i < 4; i++) {
	switch (modes[i]) {
	    case BU_CLBK_DURING:
		ip = &im->i->cmd_during_clbk;
		op = &om->i->cmd_during_clbk;
		break;
	    case BU_CLBK_POST:
		ip = &im->i->cmd_postrun_clbk;
		op = &om->i->cmd_postrun_clbk;
		break;
	    case BU_CLBK_LINGER:
		ip = &im->i->cmd_linger_clbk;
		op = &om->i->cmd_linger_clbk;
		break;
	    default:
		ip = &im->i->cmd_prerun_clbk;
		op = &om->i->cmd_prerun_clbk;
		break;
	}

	std::map<int, std::pair<bu_clbk_t, void *>>::iterator mp_it;
	for (mp_it = ip->begin(); mp_it != ip->end(); ++mp_it) {
	    (*op)[mp_it->first] = mp_it->second;
	}
    }

    return BRLCAD_OK;
}


/*
 * Keypoint in model space is established in "pt".
 * If "str" is set, then that point is used, else default
 * for this solid is selected and set.
 * "str" may be a constant string, in either upper or lower case,
 * or it may be something complex like "(3, 4)" for an ARS or spline
 * to select a particular vertex or control point.
 *
 * XXX Perhaps this should be done via solid-specific parse tables,
 * so that solids could be pretty-printed & structprint/structparse
 * processed as well?
 */
void
rt_get_solid_keypoint(struct rt_solid_edit *s, point_t *pt, const char **strp, fastf_t *mat)
{
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (!strp)
	return;

    struct rt_db_internal *ip = &s->es_int;
    RT_CK_DB_INTERNAL(ip);

    if (EDOBJ[ip->idb_type].ft_keypoint) {
	bu_vls_trunc(s->log_str, 0);
	*strp = (*EDOBJ[ip->idb_type].ft_keypoint)(pt, *strp, mat, s, s->tol);
	if (bu_vls_strlen(s->log_str)) {
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_STR, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    bu_vls_trunc(s->log_str, 0);
	}
	return;
    }

    struct bu_vls ltmp = BU_VLS_INIT_ZERO;
    bu_vls_printf(&ltmp, "%s", bu_vls_cstr(s->log_str));
    bu_vls_sprintf(s->log_str, "get_solid_keypoint: unrecognized solid type (setting keypoint to origin)\n");
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_STR, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, NULL);
    bu_vls_sprintf(s->log_str, "%s", bu_vls_cstr(&ltmp));
    bu_vls_free(&ltmp);

    VSETALL(*pt, 0.0);
    *strp = "(origin)";
}


void
rt_update_edit_absolute_tran(struct rt_solid_edit *s, vect_t view_pos)
{
    vect_t model_pos;
    vect_t ea_view_pos;
    vect_t diff;
    fastf_t inv_Viewscale = 1/s->vp->gv_scale;

    MAT4X3PNT(model_pos, s->vp->gv_view2model, view_pos);
    VSUB2(diff, model_pos, s->e_axes_pos);
    VSCALE(s->k.tra_m_abs, diff, inv_Viewscale);
    VMOVE(s->k.tra_m_abs_last, s->k.tra_m_abs);

    MAT4X3PNT(ea_view_pos, s->vp->gv_model2view, s->e_axes_pos);
    VSUB2(s->k.tra_v_abs, view_pos, ea_view_pos);
    VMOVE(s->k.tra_v_abs_last, s->k.tra_v_abs);
}


void
rt_solid_edit_set_edflag(struct rt_solid_edit *s, int edflag)
{
    if (!s)
	return;

    s->edit_flag = edflag;

    // In the case of the four generic (i.e. not primitive data specific) flag
    // settings, we can also set the solid_edit_mode state.  For anything else,
    // it is the responsibility of the primitive specific logic to decode
    // edit_flag (and any other relevant info) into the proper solid_edit_mode.
    // Applications like MGED may use solid_edit_mode to adjust interface
    // behaviors, so it is important to have it properly set, but we can only
    // do so much here.
    switch (edflag) {
	case RT_SOLID_EDIT_ROT:
	case RT_SOLID_EDIT_TRANS:
	case RT_SOLID_EDIT_SCALE:
	case RT_SOLID_EDIT_PICK:
	    s->solid_edit_mode = edflag;
	    break;
	default:
	    s->solid_edit_mode = RT_SOLID_EDIT_DEFAULT;
	    break;
    }
}

/* Processing of editing knob twists. */
int
rt_edit_knob_cmd_process(
	struct rt_solid_edit *s,
	vect_t *rvec, int *do_rot, vect_t *tvec, int *do_tran, int *do_sca,
	struct bview *v, const char *cmd, fastf_t f,
	char origin, int incr_flag, void *u_data)
{
    char c = (cmd[1] == '\0') ? cmd[0] : cmd[1];

    int ind = -1;
    switch (c) {
	case 'x':
	case 'X':
	    ind = X;
	    break;
	case 'y':
	case 'Y':
	    ind = Y;
	    break;
	case 'z':
	case 'Z':
	    ind = Z;
	    break;
	case 'S':
	    ind = 0;
	    break;
    }

    if (ind < 0)
	return BRLCAD_ERROR;

    if (cmd[1] == '\0') {

	if (cmd[0] == 'x' || cmd[0] == 'y' || cmd[0] == 'z') {

	    fastf_t *rot;
	    char *orig;
	    void **edm;

	    switch (v->gv_coord) {
		case 'm':
		    rot = &s->k.rot_m[ind];
		    orig = &s->k.origin_m;
		    edm = &s->k.rot_m_udata;
		    break;
		case 'o':
		    rot = &s->k.rot_o[ind];
		    orig = &s->k.origin_o;
		    edm = &s->k.rot_o_udata;
		    break;
		case 'v':
		default:
		    rot = &s->k.rot_v[ind];
		    orig = &s->k.origin_v;
		    edm = &s->k.rot_v_udata;
		    break;
	    }

	    *orig = origin;
	    *edm = u_data;

	    if (incr_flag) {
		*rot += f;
	    } else {
		*rot = f;
	    }

	    return BRLCAD_OK;
	}

	if (cmd[0] == 'X' || cmd[0] == 'Y' || cmd[0] == 'Z') {
	    fastf_t *tra;
	    void **edm;

	    switch (v->gv_coord) {
		case 'm':
		case 'o':
		    tra = &s->k.tra_m[ind];
		    edm = &s->k.tra_m_udata;
		    break;
		case 'v':
		default:
		    tra = &s->k.tra_v[ind];
		    edm = &s->k.tra_v_udata;
		    break;
	    }

	    *edm = u_data;

	    if (incr_flag) {
		*tra += f;
	    } else {
		*tra = f;
	    }

	    return BRLCAD_OK;
	}

	if (cmd[0] == 'S') {

	    if (incr_flag) {
		s->k.sca += f;
	    } else {
		s->k.sca = f;
	    }

	    return BRLCAD_OK;
	}

    } /* switch cmd[0] */

    if (cmd[0] == 'a' && strlen(cmd) == 2) {

	if (cmd[1] == 'x' || cmd[1] == 'y' || cmd[1] == 'z') {

	    fastf_t *rot_c = NULL;
	    fastf_t *rot_lc = NULL;
	    fastf_t *rvec_c;

	    rvec_c = &(*rvec)[ind];

	    switch (v->gv_coord) {
		case 'm':
		    rot_c = &s->k.rot_m_abs[ind];
		    rot_lc = &s->k.rot_m_abs_last[ind];
		    break;
		case 'o':
		    rot_c = &s->k.rot_o_abs[ind];
		    rot_lc = &s->k.rot_o_abs_last[ind];
		    break;
		case 'v':
		    rot_c = &s->k.rot_v_abs[ind];
		    rot_lc = &s->k.rot_v_abs_last[ind];
		    break;
	    }
	    if (!rot_c || !rot_lc)
		return BRLCAD_ERROR;

	    if (incr_flag) {
		*rot_c += f;
		*rvec_c = f;
	    } else {
		*rot_c = f;
		*rvec_c = f - *rot_lc;
	    }

	    /* wrap around */
	    fastf_t *arp;
	    fastf_t *larp;

	    switch (v->gv_coord) {
		case 'm':
		    arp = s->k.rot_m_abs;
		    larp = s->k.rot_m_abs_last;
		    break;
		case 'o':
		    arp = s->k.rot_o_abs;
		    larp = s->k.rot_o_abs_last;
		    break;
		case 'v':
		    arp = s->k.rot_v_abs;
		    larp = s->k.rot_v_abs_last;
		    break;
		default:
		    bu_log("unknown mv_coords\n");
		    arp = larp = NULL;
		    return BRLCAD_ERROR;
	    }

	    if (arp[ind] < -180.0) {
		arp[ind] = arp[ind] + 360.0;
	    } else if (arp[ind] > 180.0) {
		arp[ind] = arp[ind] - 360.0;
	    }

	    larp[ind] = arp[ind];

	    *do_rot = 1;

	    return BRLCAD_OK;
	}

	if (cmd[1] == 'X' || cmd[1] == 'Y' || cmd[1] == 'Z') {
	    fastf_t *eamt = NULL;
	    fastf_t *leamt = NULL;
	    fastf_t *tvec_c;
	    fastf_t sf = f * v->gv_local2base / v->gv_scale;

	    tvec_c = &(*tvec)[ind];

	    switch (v->gv_coord) {
		case 'm':
		case 'o':
		    eamt = &s->k.tra_m_abs[ind];
		    leamt = &s->k.tra_m_abs_last[ind];
		    break;
		case 'v':
		    eamt = &s->k.tra_v_abs[ind];
		    leamt = &s->k.tra_v_abs_last[ind];
		    break;
	    }

	    if (!eamt || !leamt)
		return BRLCAD_ERROR;

	    if (incr_flag) {
		*eamt += sf;
		*tvec_c = f;
	    } else {
		*eamt = sf;
		*tvec_c = f - *leamt * v->gv_scale * v->gv_base2local;
	    }
	    *leamt = *eamt;

	    *do_tran = 1;

	    return BRLCAD_OK;
	}

	if (cmd[1] == 'S') {
	    if (incr_flag) {
		s->k.sca_abs += f;
	    } else {
		s->k.sca_abs = f;
	    }

	    *do_sca = 1;

	    return BRLCAD_OK;
	}
    } /* switch (cmd[1]) */

    return BRLCAD_ERROR;
}

void
rt_knob_edit_rot(struct rt_solid_edit *s,
	char coords,
	char rotate_about,
	int matrix_edit,
	mat_t newrot)
{
    mat_t temp1, temp2;

    s->update_views = 1;

    switch (coords) {
	case 'm':
	    break;
	case 'o':
	    bn_mat_inv(temp1, s->acc_rot_sol);

	    /* transform into object rotations */
	    bn_mat_mul(temp2, s->acc_rot_sol, newrot);
	    bn_mat_mul(newrot, temp2, temp1);
	    break;
	case 'v':
	    bn_mat_inv(temp1, s->vp->gv_rotation);

	    /* transform into model rotations */
	    bn_mat_mul(temp2, temp1, newrot);
	    bn_mat_mul(newrot, temp2, s->vp->gv_rotation);
	    break;
    }

    if (!matrix_edit) {

	MAT_COPY(s->incr_change, newrot);
	bn_mat_mul2(s->incr_change, s->acc_rot_sol);
	s->e_inpara = 0;

	// Stash state to temporarily put things in the
	// solid edit rotate state (it may already have
	// been there but we're not counting on it)
	char save_rotate_about = s->vp->gv_rotate_about;
	s->vp->gv_rotate_about = rotate_about;

	int save_edflag = s->edit_flag;
	int save_mode = s->solid_edit_mode;

	rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_ROT);

	rt_solid_edit_process(s);

	// Restore previous state
	s->vp->gv_rotate_about = save_rotate_about;
	s->edit_flag = save_edflag;
	s->solid_edit_mode = save_mode;

    } else {

	point_t point;
	vect_t work;

	bn_mat_mul2(newrot, s->acc_rot_sol);

	/* find point for rotation to take place wrt */
	switch (rotate_about) {
	    case 'v':       /* View Center */
		VSET(work, 0.0, 0.0, 0.0);
		MAT4X3PNT(point, s->vp->gv_view2model, work);
		break;
	    case 'e':       /* Eye */
		VSET(work, 0.0, 0.0, 1.0);
		MAT4X3PNT(point, s->vp->gv_view2model, work);
		break;
	    case 'm':       /* Model Center */
		VSETALL(point, 0.0);
		break;
	    case 'k':
	    default:
		MAT4X3PNT(point, s->model_changes, s->e_keypoint);
	}

	/* Apply newrot to the s->model_changes matrix wrt "point" */
	mat_t t, out;
	bn_mat_xform_about_pnt(t, newrot, point);
	bn_mat_mul(out, t, s->model_changes);
	MAT_COPY(s->model_changes, out);

	/* Update the model2objview matrix, which is sometimes used by
	 * applications to display an intermediate editing state. */
	bn_mat_mul(s->model2objview, s->vp->gv_model2view, s->model_changes);

    }
}

void
rt_knob_edit_tran(struct rt_solid_edit *s,
        char coords,
        int matrix_edit,
        vect_t tvec)
{
    point_t p2;
    point_t delta;
    point_t vcenter;
    point_t work;

    /* compute delta */
    switch (coords) {
	case 'm':
	    VSCALE(delta, tvec, s->local2base);
	    break;
	case 'o':
	    VSCALE(p2, tvec, s->local2base);
	    MAT4X3PNT(delta, s->acc_rot_sol, p2);
	    break;
	case 'v':
	default:
	    VSCALE(p2, tvec, s->local2base / s->vp->gv_scale);
	    MAT4X3PNT(work, s->vp->gv_view2model, p2);
	    MAT_DELTAS_GET_NEG(vcenter, s->vp->gv_center);
	    VSUB2(delta, work, vcenter);

	    break;
    }

    if (!matrix_edit) {
	s->e_keyfixed = 0;

	// Get primitive keypoint
	struct rt_db_internal *ip = &s->es_int;
	fastf_t *mat = s->e_mat;
	point_t *pt = &s->e_keypoint;
	RT_CK_DB_INTERNAL(ip);
	if (EDOBJ[ip->idb_type].ft_keypoint) {
	    bu_vls_trunc(s->log_str, 0);
	    s->e_keytag = (*EDOBJ[ip->idb_type].ft_keypoint)(pt, s->e_keytag, mat, s, s->tol);
	    if (bu_vls_strlen(s->log_str)) {
		bu_log("%s", bu_vls_cstr(s->log_str));
		bu_vls_trunc(s->log_str, 0);
	    }
	} else {
	    bu_log("rt_solid_edit_knobs_tra: unrecognized solid type (setting keypoint to origin)\n");
	    VSETALL(*pt, 0.0);
	    s->e_keytag = "(origin)";
	}

	int save_edflag = s->edit_flag;
	int save_mode = s->solid_edit_mode;

	rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_TRANS);

	VADD2(s->e_para, delta, s->curr_e_axes_pos);
	s->e_inpara = 3;
	rt_solid_edit_process(s);
	s->edit_flag = save_edflag;
	s->solid_edit_mode = save_mode;
    } else {
	mat_t xlatemat;
	MAT_IDN(xlatemat);
	MAT_DELTAS_VEC(xlatemat, delta);
	bn_mat_mul2(xlatemat, s->model_changes);

	/* Update the model2objview matrix, which is sometimes used by
	 * applications to display an intermediate editing state.
	 *
	 * Probably don't really need to calculate this here - if an app
	 * has multiple views, it actually needs to calculate this separately
	 * anyway for each view.  MGED calculated them for all views as a
	 * finalization step in the wrapper logic.
	 *
	 * In the new drawing mode, this would get calculated and applied
	 * to a scene object's vlist corresponding to the object or instance
	 * being edited.
	 *
	 * Another approach would be to allow a callback... */
	bn_mat_mul(s->model2objview, s->vp->gv_model2view, s->model_changes);
    }
}

#define MGED_SMALL_SCALE 1.0e-10
void
rt_knob_edit_sca(struct rt_solid_edit *s, int matrix_edit)
{
   if (!matrix_edit) {

        fastf_t old_acc_sc_sol;

        old_acc_sc_sol = s->acc_sc_sol;

        if (-SMALL_FASTF < s->k.sca_abs && s->k.sca_abs < SMALL_FASTF)
            s->acc_sc_sol = 1.0;
        else if (s->k.sca_abs > 0.0)
            s->acc_sc_sol = 1.0 + s->k.sca_abs * 3.0;
        else {
            if ((s->k.sca_abs - MGED_SMALL_SCALE) < -1.0)
                s->k.sca_abs = -1.0 + MGED_SMALL_SCALE;

            s->acc_sc_sol = 1.0 + s->k.sca_abs;
        }

        s->es_scale = s->acc_sc_sol / old_acc_sc_sol;

	int save_edflag = s->edit_flag;
	int save_mode = s->solid_edit_mode;

	rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_SCALE);

	rt_solid_edit_process(s);

	s->edit_flag = save_edflag;
	s->solid_edit_mode = save_mode;

   } else {
       fastf_t scale;
       mat_t incr_mat;
       MAT_IDN(incr_mat);

       // TODO - objedit_mouse SARROW case has different logic for handling mousevec
       // inputs - looking like we may need a mousevec entry for the rt_solid_edit
       // struct so we can have both processing methods here....

       if (-SMALL_FASTF < s->k.sca_abs && s->k.sca_abs < SMALL_FASTF)
	   scale = 1;
       else if (s->k.sca_abs > 0.0)
	   scale = 1.0 + s->k.sca_abs * 3.0;
       else {
	   if ((s->k.sca_abs - MGED_SMALL_SCALE) < -1.0)
	       s->k.sca_abs = -1.0 + MGED_SMALL_SCALE;

	   scale = 1.0 + s->k.sca_abs;
       }

       /* switch depending on scaling option selected */
       switch (s->edit_flag) {

	   case RT_SOLID_EDIT_SCALE:
	       /* global scaling */
	       incr_mat[15] = s->acc_sc_obj / scale;
	       s->acc_sc_obj = scale;
	       break;

	   case RT_MATRIX_EDIT_SCALE_X:
	       /* local scaling ... X-axis */
	       incr_mat[0] = scale / s->acc_sc[0];
	       /* accumulate the scale factor */
	       s->acc_sc[0] = scale;
	       break;

	   case RT_MATRIX_EDIT_SCALE_Y:
	       /* local scaling ... Y-axis */
	       incr_mat[5] = scale / s->acc_sc[1];
	       /* accumulate the scale factor */
	       s->acc_sc[1] = scale;
	       break;

	   case RT_MATRIX_EDIT_SCALE_Z:
	       /* local scaling ... Z-axis */
	       incr_mat[10] = scale / s->acc_sc[2];
	       /* accumulate the scale factor */
	       s->acc_sc[2] = scale;
	       break;

	   default:
	       bu_log("Incorrect edit flag for matrix scale:  %d\n", s->edit_flag);
	       return;
       }

       /* Have scaling take place with respect to keypoint, NOT the view
	* center.  model_changes is the matrix that will ultimately be used to
	* alter the geometry on disk.  This should probably go into rt_solid_edit_process
	* if it is generalized to handle both solid and instance editing. */
       mat_t t, out;
       vect_t pos_model;
       VMOVE(t, s->e_keypoint);
       MAT4X3PNT(pos_model, s->model_changes, t);
       bn_mat_xform_about_pnt(t, incr_mat, pos_model);
       bn_mat_mul(out, t, s->model_changes);
       MAT_COPY(s->model_changes, out);

       /* Update the model2objview matrix, which is sometimes used by
	* applications to display an intermediate editing state. */
       bn_mat_mul(s->model2objview, s->vp->gv_model2view, s->model_changes);
   }
}

/*
 * A great deal of magic takes place here, to accomplish solid editing.
 *
 * Called from mged main loop after any event handlers:
 * if (sedraw > 0) rt_solid_edit_process(s);
 * to process any residual events that the event handlers were too
 * lazy to handle themselves.
 *
 * A lot of processing is deferred to here, so that the "p" command
 * can operate on an equal footing to mouse events.
 */
void
rt_solid_edit_process(struct rt_solid_edit *s)
{
    bu_clbk_t f = NULL;
    void *d = NULL;

    ++s->update_views;

    int had_method = 0;
    const struct rt_db_internal *ip = &s->es_int;
    if (EDOBJ[ip->idb_type].ft_edit) {
	bu_vls_trunc(s->log_str, 0);
	if ((*EDOBJ[ip->idb_type].ft_edit)(s)) {
	    if (bu_vls_strlen(s->log_str)) {
		rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_STR, BU_CLBK_DURING);
		if (f)
		    (*f)(0, NULL, d, NULL);
		bu_vls_trunc(s->log_str, 0);
	    }
	    return;
	}
	if (bu_vls_strlen(s->log_str)) {
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_STR, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    bu_vls_trunc(s->log_str, 0);
	}
	had_method = 1;
    }

    switch (s->edit_flag) {

	case RT_SOLID_EDIT_IDLE:
	    /* do nothing more */
	    --s->update_views;
	    break;
	default:
	    {
		if (had_method)
		    break;
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&tmp_vls, "%s", bu_vls_cstr(s->log_str));
		bu_vls_sprintf(s->log_str, "rt_solid_edit_process:  unknown edflag = %d.\n", s->edit_flag);
		rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_STR, BU_CLBK_DURING);
		if (f)
		    (*f)(0, NULL, d, NULL);
		bu_vls_sprintf(s->log_str, "%s", bu_vls_cstr(&tmp_vls));
		bu_vls_free(&tmp_vls);
		rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
		if (f)
		    (*f)(0, NULL, d, NULL);
	    }
    }

    /* If the keypoint changed location, find about it here */
    if (!s->e_keyfixed)
	rt_get_solid_keypoint(s, &s->e_keypoint, &s->e_keytag, s->e_mat);

    int flag = 0;
    f = NULL; d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);

    f = NULL; d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_REPLOT_EDITING_SOLID, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, NULL);

    if (s->update_views) {
	f = NULL; d = NULL;
	rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_VIEW_UPDATE, BU_CLBK_DURING);
	if (f)
	    (*f)(0, NULL, d, NULL);
    }

    s->e_inpara = 0;
    s->e_mvalid = 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
