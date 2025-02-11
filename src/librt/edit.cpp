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
    s->ipe_ptr = NULL;

    s->tol = tol;
    s->vp = v;

    RT_DB_INTERNAL_INIT(&s->es_int);

    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 0;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    s->e_inpara = 0;

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
    VSCALE(s->edit_absolute_model_tran, diff, inv_Viewscale);
    VMOVE(s->last_edit_absolute_model_tran, s->edit_absolute_model_tran);

    MAT4X3PNT(ea_view_pos, s->vp->gv_model2view, s->e_axes_pos);
    VSUB2(s->edit_absolute_view_tran, view_pos, ea_view_pos);
    VMOVE(s->last_edit_absolute_view_tran, s->edit_absolute_view_tran);
}


void
rt_solid_edit_set_edflag(struct rt_solid_edit *s, int edflag)
{
    if (!s)
	return;

    s->edit_flag = edflag;
    s->solid_edit_pick = 0;

    switch (edflag) {
	case RT_SOLID_EDIT_ROT:
	    s->solid_edit_rotate = 1;
	    s->solid_edit_translate = 0;
	    s->solid_edit_scale = 0;
	    break;
	case RT_SOLID_EDIT_TRANS:
	    s->solid_edit_rotate = 0;
	    s->solid_edit_translate = 1;
	    s->solid_edit_scale = 0;
	    break;
	case RT_SOLID_EDIT_SCALE:
	    s->solid_edit_rotate = 0;
	    s->solid_edit_translate = 0;
	    s->solid_edit_scale = 1;
	    break;
	default:
	    s->solid_edit_rotate = 0;
	    s->solid_edit_translate = 0;
	    s->solid_edit_scale = 0;
	    break;
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
