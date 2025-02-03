/*                     M G E D _ I M P L . C P P
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
/** @file mged_impl.cpp
 *
 * Internal state implementations
 */

#include "common.h"

#include "mged.h"
#include "./mged_ecmds.h"

struct mged_state *
mged_state_create(void)
{
    struct mged_state *s;
    BU_GET(s, struct mged_state);
    s->magic = MGED_STATE_MAGIC;

    BU_GET(s->i, struct mged_state_impl);
    s->i->i = new MGED_Internal;

    s->classic_mged = 1;
    s->interactive = 0; /* >0 means interactive, intentionally starts
			 * 0 to know when interactive, e.g., via -C
			 * option
			 */
    bu_vls_init(&s->input_str);
    bu_vls_init(&s->input_str_prefix);
    bu_vls_init(&s->scratchline);
    bu_vls_init(&s->mged_prompt);
    s->dpy_string = NULL;

    s->s_edit = NULL;

    // Register default callbacks
    mged_state_clbk_set(s, 0, ECMD_PRINT_STR, 0, GED_CLBK_DURING, mged_print_str, s);
    mged_state_clbk_set(s, 0, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING, mged_print_result, s);
    mged_state_clbk_set(s, 0, ECMD_EAXES_POS , 0, GED_CLBK_DURING, set_e_axes_pos, s);
    mged_state_clbk_set(s, 0, ECMD_REPLOT_EDITING_SOLID, 0, GED_CLBK_DURING, replot_editing_solid, s);
    mged_state_clbk_set(s, 0, ECMD_VIEW_UPDATE, 0, GED_CLBK_DURING, mged_view_update, s);
    mged_state_clbk_set(s, 0, ECMD_VIEW_SET_FLAG, 0, GED_CLBK_DURING, mged_view_set_flag, s);
    mged_state_clbk_set(s, 0, ECMD_MENU_SET, 0, GED_CLBK_DURING, mged_mmenu_set, s);

    // Register primitive/ecmd specific callbacks
    mged_state_clbk_set(s, ID_ARB8, ECMD_ARB_SETUP_ROTFACE, 0, GED_CLBK_DURING, arb_setup_rotface_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_MODE, 0, GED_CLBK_DURING, ecmd_bot_mode_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_ORIENT, 0, GED_CLBK_DURING, ecmd_bot_orient_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_THICK, 0, GED_CLBK_DURING, ecmd_bot_thick_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_FLAGS, 0, GED_CLBK_DURING, ecmd_bot_flags_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_FMODE, 0, GED_CLBK_DURING, ecmd_bot_fmode_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_PICKT, 0, GED_CLBK_DURING, ecmd_bot_pickt_multihit_clbk, s);
    mged_state_clbk_set(s, ID_NMG, ECMD_NMG_EDEBUG, 0, GED_CLBK_DURING, ecmd_nmg_edebug_clbk, s);
    mged_state_clbk_set(s, ID_EXTRUDE, ECMD_EXTR_SKT_NAME, 0, GED_CLBK_DURING, ecmd_extrude_skt_name_clbk, s);

    return s;
}

void
mged_state_destroy(struct mged_state *s)
{
    if (!s)
	return;

    s->magic = 0; // make sure anything trying to use this after free gets a magic failure
    bu_vls_free(&s->input_str);
    bu_vls_free(&s->input_str_prefix);
    bu_vls_free(&s->scratchline);
    bu_vls_free(&s->mged_prompt);
    if (s->s_edit)
	rt_solid_edit_destroy(s->s_edit);
    s->s_edit = NULL;

    delete s->i->i;
    BU_PUT(s->i, struct mged_state_impl);
    BU_PUT(s, struct mged_state);
}

struct rt_solid_edit *
rt_solid_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *tol, struct bview *v)
{
    struct rt_solid_edit *s;
    BU_GET(s, struct rt_solid_edit);
    BU_GET(s->i, struct rt_solid_edit_impl);
    s->i->i = new MGED_SEDIT_Internal;
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
    s->edit_menu = 0;

    BU_GET(s->log_str, struct bu_vls);
    bu_vls_init(s->log_str);

    if (!dfp || !dbip)
	return s;

    s->local2base = dbip->dbi_local2base;
    s->base2local = dbip->dbi_base2local;

    if (rt_db_get_internal(&s->es_int, DB_FULL_PATH_CUR_DIR(dfp), dbip, NULL, &rt_uniresource) < 0) {
	rt_solid_edit_destroy(s);
	return NULL;                         /* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->es_int);

    // TODO - prim specific data creation via functab
    if (s->es_int.idb_type == ID_BSPLINE) {
	//bspline_init_sedit(s);
    }

    /* Save aggregate path matrix */
    (void)db_path_to_mat(dbip, dfp, s->e_mat, dfp->fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->e_invmat, s->e_mat);

    /* Establish initial keypoint */
    s->e_keytag = "";
    get_solid_keypoint(s, &s->e_keypoint, &s->e_keytag, &s->es_int, s->e_mat);

    return s;
}

void
rt_solid_edit_destroy(struct rt_solid_edit *s)
{
    if (!s)
	return;

    rt_db_free_internal(&s->es_int);

    bu_vls_free(s->log_str);
    BU_PUT(s->log_str, struct bu_vls);

    delete s->i->i;
    BU_PUT(s->i, struct rt_solid_edit_impl);
    BU_PUT(s, struct rt_solid_edit);
}


std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *
mged_internal_clbk_map(MGED_Internal *i, int obj_type, int mode)
{
    std::map<int, std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>>> *omp = NULL;
    switch (mode) {
	case GED_CLBK_DURING:
	    omp = &i->cmd_during_clbk;
	    break;
	case GED_CLBK_POST:
	    omp = &i->cmd_postrun_clbk;
	    break;
	case GED_CLBK_LINGER:
	    omp = &i->cmd_linger_clbk;
	    break;
	default:
	    omp = &i->cmd_prerun_clbk;
	    break;
    }
    std::map<int, std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>>>::iterator o_it;
    o_it = omp->find(obj_type);
    if (o_it == omp->end()) {
	(*omp)[obj_type] = std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>>();
    }

    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp = &(*omp)[obj_type];
    return mp;
}

std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *
mged_sedit_clbk_map(MGED_SEDIT_Internal *i, int mode)
{
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp = NULL;
    switch (mode) {
	case GED_CLBK_DURING:
	    mp = &i->cmd_during_clbk;
	    break;
	case GED_CLBK_POST:
	    mp = &i->cmd_postrun_clbk;
	    break;
	case GED_CLBK_LINGER:
	    mp = &i->cmd_linger_clbk;
	    break;
	default:
	    mp = &i->cmd_prerun_clbk;
	    break;
    }

    return mp;
}

int mged_edit_clbk_set(std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp, int ed_cmd, int menu_cmd, bu_clbk_t f, void *d)
{
    // Check for no-op case
    if (!mp)
	return BRLCAD_OK;

    if (!f) {
	mp->erase(std::make_pair(ed_cmd, menu_cmd));
	return BRLCAD_OK;
    }

    (*mp)[std::make_pair(ed_cmd, menu_cmd)] = std::make_pair(f, d);

    return BRLCAD_OK;
}
int mged_edit_clbk_get(bu_clbk_t *f, void **d, std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp, int ed_cmd, int menu_cmd)
{
    // Check for no-op case
    if (!f || !d || !mp)
	return BRLCAD_OK;

    if (mp->find(std::make_pair(ed_cmd,menu_cmd)) == mp->end())
	return BRLCAD_ERROR;

    std::pair<bu_clbk_t, void *> clbk_info = (*mp)[std::make_pair(ed_cmd, menu_cmd)];

    (*f) = clbk_info.first;
    (*d) = clbk_info.second;

    return BRLCAD_OK;
}

int mged_state_clbk_set(struct mged_state *s, int obj_type, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d)
{
    // Check for no-op case
    if (!s)
	return BRLCAD_OK;

    MGED_Internal *i = s->i->i;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp;
    mp = mged_internal_clbk_map(i, obj_type, mode);
    if (!mp)
	return BRLCAD_ERROR;

    return mged_edit_clbk_set(mp, ed_cmd, menu_cmd, f, d);
}
int mged_state_clbk_get(bu_clbk_t *f, void **d, struct mged_state *s, int obj_type, int ed_cmd, int menu_cmd, int mode)
{
    // Check for no-op case
    if (!f || !d || !s)
	return BRLCAD_OK;

    MGED_Internal *i = s->i->i;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp;
    mp = mged_internal_clbk_map(i, obj_type, mode);
    if (!mp)
	return BRLCAD_ERROR;

    return mged_edit_clbk_get(f, d, mp, ed_cmd, menu_cmd);
}

int mged_sedit_clbk_set(struct rt_solid_edit *s, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d)
{
    // Check for no-op case
    if (!s)
	return BRLCAD_OK;

    MGED_SEDIT_Internal *i = s->i->i;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp;
    mp = mged_sedit_clbk_map(i, mode);
    if (!mp)
	return BRLCAD_ERROR;

    return mged_edit_clbk_set(mp, ed_cmd, menu_cmd, f, d);
}
int mged_sedit_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit *s, int ed_cmd, int menu_cmd, int mode)
{
    // Check for no-op case
    if (!f || !d || !s)
	return BRLCAD_OK;

    MGED_SEDIT_Internal *i = s->i->i;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp;
    mp = mged_sedit_clbk_map(i, mode);
    if (!mp)
	return BRLCAD_ERROR;

    return mged_edit_clbk_get(f, d, mp, ed_cmd, menu_cmd);
}

int mged_sedit_clbk_sync(struct rt_solid_edit *se, struct mged_state *s)
{
    if (!se)
	return BRLCAD_ERROR;

    MGED_SEDIT_Internal *sei = se->i->i;
    sei->cmd_prerun_clbk.clear();
    sei->cmd_during_clbk.clear();
    sei->cmd_postrun_clbk.clear();
    sei->cmd_linger_clbk.clear();

    if (!s)
	return BRLCAD_OK;

    int modes[4] = {GED_CLBK_PRE, GED_CLBK_DURING, GED_CLBK_POST, GED_CLBK_LINGER};
    MGED_Internal *si = s->i->i;

    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *emp;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>>::iterator mp_it;
    for (int i = 0; i < 4; i++) {
	mp = mged_internal_clbk_map(si, se->es_int.idb_minor_type, modes[i]);
	emp = mged_sedit_clbk_map(sei, modes[i]);
	for (mp_it = mp->begin(); mp_it != mp->end(); ++mp_it) {
	    (*emp)[mp_it->first] = mp_it->second;
	}
    }

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
