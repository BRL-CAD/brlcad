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
	mged_solid_edit_destroy(s->s_edit);
    s->s_edit = NULL;

    delete s->i->i;
    BU_PUT(s->i, struct mged_state_impl);
    BU_PUT(s, struct mged_state);
}

struct mged_solid_edit *
mged_solid_edit_create(struct rt_db_internal *UNUSED(ip), struct bn_tol *tol, struct bview *v)
{
    struct mged_solid_edit *s;
    BU_GET(s, struct mged_solid_edit);
    BU_GET(s->i, struct mged_solid_edit_impl);
    s->i->i = new MGED_SEDIT_Internal;
    s->tol = tol;
    s->vp = v;

    MAT_IDN(s->model_changes);
    MAT_IDN(s->acc_rot_sol);
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 0;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    s->e_inpara = 0;

    BU_GET(s->log_str, struct bu_vls);
    bu_vls_init(s->log_str);

    return s;
}

void
mged_solid_edit_destroy(struct mged_solid_edit *s)
{
    if (!s)
	return;

    bu_vls_free(s->log_str);
    BU_PUT(s->log_str, struct bu_vls);

    delete s->i->i;
    BU_PUT(s->i, struct mged_solid_edit_impl);
    BU_PUT(s, struct mged_solid_edit);
}


std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *
mged_internal_clbk_map(MGED_Internal *i, int obj_type, int mode)
{
    std::map<int, std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>>> *omp;
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
    if (o_it == omp->end())
	return NULL;

    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp = &(*omp)[obj_type];
    return mp;
}

std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *
mged_sedit_clbk_map(MGED_SEDIT_Internal *i, int mode)
{
    std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> *mp;
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

    bu_log("ed_cmd: %d, menu_cmd: %d\n", ed_cmd, menu_cmd);
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

    bu_log("ed_cmd: %d, menu_cmd: %d\n", ed_cmd, menu_cmd);
    (*f) = clbk_info.first;
    (*d) = clbk_info.second;

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
