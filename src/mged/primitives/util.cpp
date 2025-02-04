/*                       U T I L . C P P
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
/** @file util.cpp
 *
 * Implementation of callbacks, rt_solid_state and map create/destroy
 * functions.
 */

#include "common.h"

#include <map>

#include "vmath.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "edfunctab.h"

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
    s->edit_menu = 0;

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
rt_solid_edit_map_clbk_set(struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d)
{
    if (!em)
	return BRLCAD_OK;

    RT_Edit_Map_Internal *i = em->i;

    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp = NULL;
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
	mp->erase(std::make_pair(ed_cmd, menu_cmd));
	return BRLCAD_OK;
    }

    (*mp)[std::make_pair(ed_cmd, menu_cmd)] = std::make_pair(f, d);

    return BRLCAD_OK;
}

int
rt_solid_edit_map_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode)
{
    // Check for no-op case
    if (!f || !d || !em)
	return BRLCAD_OK;

    RT_Edit_Map_Internal *i = em->i;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp;
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

    if (mp->find(std::make_pair(ed_cmd,menu_cmd)) == mp->end())
	return BRLCAD_ERROR;

    std::pair<bu_clbk_t, void *> clbk_info = (*mp)[std::make_pair(ed_cmd, menu_cmd)];

    (*f) = clbk_info.first;
    (*d) = clbk_info.second;

    return BRLCAD_OK;
}

int
rt_solid_edit_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit *s, int ed_cmd, int menu_cmd, int mode)
{
    return rt_solid_edit_map_clbk_get(f, d, s->m, ed_cmd, menu_cmd, mode);
}

int
rt_solid_edit_map_sync(struct rt_solid_edit_map *om, struct rt_solid_edit_map *im)
{
    // Check for no-op case
    if (!om)
	return BRLCAD_OK;

    om->i->cmd_prerun_clbk.clear();
    om->i->cmd_during_clbk.clear();
    om->i->cmd_postrun_clbk.clear();
    om->i->cmd_linger_clbk.clear();

    if (!im)
	return BRLCAD_OK;

    int modes[4] = {BU_CLBK_PRE, BU_CLBK_DURING, BU_CLBK_POST, BU_CLBK_LINGER};
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *ip;
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *op;
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

	std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>>::iterator mp_it;
	for (mp_it = ip->begin(); mp_it != ip->end(); ++mp_it) {
	    (*op)[mp_it->first] = mp_it->second;
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
