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
#include "rt_ecmds.h"

MGED_Internal::MGED_Internal()
{
}

MGED_Internal::~MGED_Internal()
{
    std::map<int, rt_solid_edit_map *>::iterator c_it;
    for (c_it = cmd_map.begin(); c_it != cmd_map.end(); c_it++) {
	rt_solid_edit_map *m = c_it->second;
	rt_solid_edit_map_destroy(m);
    }
}

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
    mged_state_clbk_set(s, 0, ECMD_PRINT_STR, 0, BU_CLBK_DURING, mged_print_str, s);
    mged_state_clbk_set(s, 0, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING, mged_print_result, s);
    mged_state_clbk_set(s, 0, ECMD_EAXES_POS , 0, BU_CLBK_DURING, set_e_axes_pos, s);
    mged_state_clbk_set(s, 0, ECMD_REPLOT_EDITING_SOLID, 0, BU_CLBK_DURING, replot_editing_solid, s);
    mged_state_clbk_set(s, 0, ECMD_VIEW_UPDATE, 0, BU_CLBK_DURING, mged_view_update, s);
    mged_state_clbk_set(s, 0, ECMD_VIEW_SET_FLAG, 0, BU_CLBK_DURING, mged_view_set_flag, s);
    mged_state_clbk_set(s, 0, ECMD_MENU_SET, 0, BU_CLBK_DURING, mged_mmenu_set, s);
    mged_state_clbk_set(s, 0, ECMD_GET_FILENAME, 0, BU_CLBK_DURING, mged_get_filename, s);

    // Register primitive/ecmd specific callbacks
    mged_state_clbk_set(s, ID_ARB8, ECMD_ARB_SETUP_ROTFACE, 0, BU_CLBK_DURING, arb_setup_rotface_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_MODE, 0, BU_CLBK_DURING, ecmd_bot_mode_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_ORIENT, 0, BU_CLBK_DURING, ecmd_bot_orient_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_THICK, 0, BU_CLBK_DURING, ecmd_bot_thick_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_FLAGS, 0, BU_CLBK_DURING, ecmd_bot_flags_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_FMODE, 0, BU_CLBK_DURING, ecmd_bot_fmode_clbk, s);
    mged_state_clbk_set(s, ID_BOT, ECMD_BOT_PICKT, 0, BU_CLBK_DURING, ecmd_bot_pickt_multihit_clbk, s);
    mged_state_clbk_set(s, ID_NMG, ECMD_NMG_EDEBUG, 0, BU_CLBK_DURING, ecmd_nmg_edebug_clbk, s);
    mged_state_clbk_set(s, ID_EXTRUDE, ECMD_EXTR_SKT_NAME, 0, BU_CLBK_DURING, ecmd_extrude_skt_name_clbk, s);

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

struct rt_solid_edit_map *
mged_internal_clbk_map(MGED_Internal *i, int obj_type)
{
    struct rt_solid_edit_map *omap = NULL;
    std::map<int, rt_solid_edit_map *>::iterator m_it = i->cmd_map.find(obj_type);
    if (m_it != i->cmd_map.end()) {
	omap = m_it->second;
    } else {
	omap = rt_solid_edit_map_create();
	i->cmd_map[obj_type] = omap;
    }
    return omap;
}

int mged_state_clbk_set(struct mged_state *s, int obj_type, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d)
{
    // Check for no-op case
    if (!s)
	return BRLCAD_OK;

    MGED_Internal *i = s->i->i;
    struct rt_solid_edit_map *mp = mged_internal_clbk_map(i, obj_type);
    if (!mp)
	return BRLCAD_ERROR;

    return rt_solid_edit_map_clbk_set(mp, ed_cmd, menu_cmd, mode, f, d);
}

int mged_state_clbk_get(bu_clbk_t *f, void **d, struct mged_state *s, int obj_type, int ed_cmd, int menu_cmd, int mode)
{
    // Check for no-op case
    if (!f || !d || !s)
	return BRLCAD_OK;

    MGED_Internal *i = s->i->i;
    struct rt_solid_edit_map *mp = mged_internal_clbk_map(i, obj_type);
    if (!mp)
	return BRLCAD_ERROR;

    return rt_solid_edit_map_clbk_get(f, d, mp, ed_cmd, menu_cmd, mode);
}


int mged_edit_clbk_sync(struct rt_solid_edit *se, struct mged_state *s)
{
    if (!se)
	return BRLCAD_ERROR;

    MGED_Internal *i = s->i->i;
    struct rt_solid_edit_map *mp = mged_internal_clbk_map(i, s->s_edit->es_int.idb_type);

    return rt_solid_edit_map_sync(se->m, mp);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
