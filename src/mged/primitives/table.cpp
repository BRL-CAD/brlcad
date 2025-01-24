/*                       T A B L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 1989-2025 United States Government as represented by
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
/** @file primitives/table.cpp
 *
 * Per-primitive methods for MGED editing.
 *
 */
/** @} */

#include "common.h"

#include "vmath.h"
#include "./mged_functab.h"

extern "C" {

#define MGED_DECLARE_INTERFACE(name) \
    extern void mged_##name##_labels(int *num_lines, point_t *lines, struct rt_point_labels *pl, int max_pl, const mat_t xform, struct rt_db_internal *ip, struct bn_tol *); \
    extern const char *mged_##name##_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern void mged_##name##_e_axes_pos(struct mged_state *s, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern void mged_##name##_write_params(struct bu_vls *p, const struct rt_db_internal *ip, const struct bn_tol *tol, fastf_t base2local); \
    extern void mged_##name##_read_params(struct rt_db_internal *ip, const char *fc, const struct bn_tol *tol, fastf_t local2base); \
    extern int mged_##name##_edit(struct mged_state *s, int edflag); \
    extern int mged_##name##_edit_xy(struct mged_state *s, int edflag, vect_t mousevec); \
    extern void *mged_##name##_prim_edit_create(void); \
    extern void mged_##name##_prim_edit_destroy(void *); \
    extern int mged_##name##_menu_str(struct bu_vls *m, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern struct menu_item *mged_##name##_menu_item(const struct bn_tol *tol); \

MGED_DECLARE_INTERFACE(tor);
MGED_DECLARE_INTERFACE(tgc);
MGED_DECLARE_INTERFACE(ell);
MGED_DECLARE_INTERFACE(arb);
MGED_DECLARE_INTERFACE(ars);
MGED_DECLARE_INTERFACE(hlf);
MGED_DECLARE_INTERFACE(rec);
MGED_DECLARE_INTERFACE(pg);
MGED_DECLARE_INTERFACE(bspline);
MGED_DECLARE_INTERFACE(sph);
MGED_DECLARE_INTERFACE(ebm);
MGED_DECLARE_INTERFACE(vol);
MGED_DECLARE_INTERFACE(arbn);
MGED_DECLARE_INTERFACE(pipe);
MGED_DECLARE_INTERFACE(part);
MGED_DECLARE_INTERFACE(nmg);
MGED_DECLARE_INTERFACE(rpc);
MGED_DECLARE_INTERFACE(rhc);
MGED_DECLARE_INTERFACE(epa);
MGED_DECLARE_INTERFACE(ehy);
MGED_DECLARE_INTERFACE(eto);
MGED_DECLARE_INTERFACE(grp);
MGED_DECLARE_INTERFACE(hf);
MGED_DECLARE_INTERFACE(dsp);
MGED_DECLARE_INTERFACE(sketch);
MGED_DECLARE_INTERFACE(annot);
MGED_DECLARE_INTERFACE(extrude);
MGED_DECLARE_INTERFACE(submodel);
MGED_DECLARE_INTERFACE(cline);
MGED_DECLARE_INTERFACE(bot);
MGED_DECLARE_INTERFACE(superell);
MGED_DECLARE_INTERFACE(metaball);
MGED_DECLARE_INTERFACE(hyp);
MGED_DECLARE_INTERFACE(revolve);
MGED_DECLARE_INTERFACE(constraint);
MGED_DECLARE_INTERFACE(material);
/* MGED_DECLARE_INTERFACE(binunif); */
MGED_DECLARE_INTERFACE(pnts);
MGED_DECLARE_INTERFACE(hrt);
MGED_DECLARE_INTERFACE(datum);
MGED_DECLARE_INTERFACE(brep);
MGED_DECLARE_INTERFACE(joint);
MGED_DECLARE_INTERFACE(script);

const struct mged_functab MGED_OBJ[] = {
    {
	/* 0: unused, for sanity checking. */
	RT_FUNCTAB_MAGIC, "ID_NULL", "NULL",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    },

    {
	/* 1 */
	RT_FUNCTAB_MAGIC, "ID_TOR", "tor",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_tor_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_tor_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_tor_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_tor_menu_item)    /* menu_item */
    },

    {
	/* 2 */
	RT_FUNCTAB_MAGIC, "ID_TGC", "tgc",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	MGEDFUNCTAB_FUNC_E_AXES_POS_CAST(mged_tgc_e_axes_pos), /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_tgc_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_tgc_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_tgc_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_tgc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_tgc_menu_item)    /* menu_item */
    },

    {
	/* 3 */
	RT_FUNCTAB_MAGIC, "ID_ELL", "ell",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_ell_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_ell_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_ell_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_ell_menu_item)    /* menu_item */
    },

    {
	/* 4 */
	RT_FUNCTAB_MAGIC, "ID_ARB8", "arb8",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_arb_keypoint),     /* keypoint */
	MGEDFUNCTAB_FUNC_E_AXES_POS_CAST(mged_arb_e_axes_pos), /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_arb_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_arb_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_arb_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_arb_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_arb_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_arb_menu_item)    /* menu_item */
    },

    {
	/* 5 */
	RT_FUNCTAB_MAGIC, "ID_ARS", "ars",
	MGEDFUNCTAB_FUNC_LABELS_CAST(mged_ars_labels),    /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_ars_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_ars_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_ars_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_ars_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_ars_menu_item)    /* menu_item */
    },

    {
	/* 6 */
	RT_FUNCTAB_MAGIC, "ID_HALF", "half",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_hlf_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_hlf_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 7 */
	RT_FUNCTAB_MAGIC, "ID_REC", "rec",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	MGEDFUNCTAB_FUNC_E_AXES_POS_CAST(mged_tgc_e_axes_pos), /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_tgc_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_tgc_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_tgc_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_tgc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_tgc_menu_item)    /* menu_item */
    },

    {
	/* 8 */
	RT_FUNCTAB_MAGIC, "ID_POLY", "poly",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
 	NULL   /* menu_item */
    },

    {
	/* 9 */
	RT_FUNCTAB_MAGIC, "ID_BSPLINE", "bspline",
	MGEDFUNCTAB_FUNC_LABELS_CAST(mged_bspline_labels), /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_bspline_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_bspline_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_bspline_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_bspline_menu_item)    /* menu_item */
    },

    {
	/* 10 */
	RT_FUNCTAB_MAGIC, "ID_SPH", "sph",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_ell_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_ell_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_ell_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 11 */
	RT_FUNCTAB_MAGIC, "ID_NMG", "nmg",
	MGEDFUNCTAB_FUNC_LABELS_CAST(mged_nmg_labels),    /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_nmg_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_nmg_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_nmg_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_nmg_menu_item)    /* menu_item */
    },

    {
	/* 12 */
	RT_FUNCTAB_MAGIC, "ID_EBM", "ebm",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_ebm_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_ebm_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_ebm_menu_item)    /* menu_item */
    },

    {
	/* 13 */
	RT_FUNCTAB_MAGIC, "ID_VOL", "vol",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_vol_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_vol_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_vol_menu_item)    /* menu_item */
    },

    {
	/* 14 */
	RT_FUNCTAB_MAGIC, "ID_ARBN", "arbn",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 15 */
	RT_FUNCTAB_MAGIC, "ID_PIPE", "pipe",
	MGEDFUNCTAB_FUNC_LABELS_CAST(mged_pipe_labels), /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_pipe_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_pipe_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_pipe_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_pipe_menu_item)    /* menu_item */
    },

    {
	/* 16 */
	RT_FUNCTAB_MAGIC, "ID_PARTICLE", "part",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_part_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_part_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_part_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_part_menu_item)    /* menu_item */
    },

    {
	/* 17 */
	RT_FUNCTAB_MAGIC, "ID_RPC", "rpc",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_rpc_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_rpc_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_rpc_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_rpc_menu_item)    /* menu_item */
    },

    {
	/* 18 */
	RT_FUNCTAB_MAGIC, "ID_RHC", "rhc",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_rhc_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_rhc_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_rhc_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_rhc_menu_item)    /* menu_item */
    },

    {
	/* 19 */
	RT_FUNCTAB_MAGIC, "ID_EPA", "epa",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_epa_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_epa_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_epa_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_epa_menu_item)    /* menu_item */
    },

    {
	/* 20 */
	RT_FUNCTAB_MAGIC, "ID_EHY", "ehy",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_ehy_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_ehy_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_ehy_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_ehy_menu_item)    /* menu_item */
    },

    {
	/* 21 */
	RT_FUNCTAB_MAGIC, "ID_ETO", "eto",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_eto_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_eto_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_eto_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_eto_menu_item)    /* menu_item */
    },

    {
	/* 22 */
	RT_FUNCTAB_MAGIC, "ID_GRIP", "grip",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_grp_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_grp_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_grp_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 23 -- XXX unimplemented */
	RT_FUNCTAB_MAGIC, "ID_JOINT", "joint",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 24 */
	RT_FUNCTAB_MAGIC, "ID_HF", "hf",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 25 Displacement Map (alt. height field) */
	RT_FUNCTAB_MAGIC, "ID_DSP", "dsp",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_dsp_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_dsp_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_dsp_menu_item)    /* menu_item */
    },

    {
	/* 26 2D sketch */
	RT_FUNCTAB_MAGIC, "ID_SKETCH", "sketch",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 27 Solid of extrusion */
	RT_FUNCTAB_MAGIC, "ID_EXTRUDE", "extrude",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_extrude_keypoint),     /* keypoint */
	MGEDFUNCTAB_FUNC_E_AXES_POS_CAST(mged_extrude_e_axes_pos), /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_extrude_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_extrude_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_extrude_menu_item)    /* menu_item */
    },

    {
	/* 28 Instanced submodel */
	RT_FUNCTAB_MAGIC, "ID_SUBMODEL", "submodel",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 29 Fastgen cline solid */
	RT_FUNCTAB_MAGIC, "ID_CLINE", "cline",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint),   /* keypoint */
	MGEDFUNCTAB_FUNC_E_AXES_POS_CAST(mged_cline_e_axes_pos), /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_cline_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_cline_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_cline_menu_item)    /* menu_item */
    },

    {
	/* 30 Bag o' Triangles */
	RT_FUNCTAB_MAGIC, "ID_BOT", "bot",
	MGEDFUNCTAB_FUNC_LABELS_CAST(mged_bot_labels),    /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_bot_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_bot_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_bot_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_bot_menu_item)    /* menu_item */
    },

    {
	/* 31 combination objects (should not be in this table) */
	RT_FUNCTAB_MAGIC, "ID_COMBINATION", "comb",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 32 available placeholder to not offset latter table indices
	 * (was ID_BINEXPM)
	 */
	RT_FUNCTAB_MAGIC, "ID_UNUSED1", "UNUSED1",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 33 */
	RT_FUNCTAB_MAGIC, "ID_BINUNIF", "binunif",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 34 available placeholder to not offset latter table indices
	 * (was ID_BINMIME)
	 */
	RT_FUNCTAB_MAGIC, "ID_UNUSED2", "UNUSED2",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 35 (but "should" be 31) Superquadratic Ellipsoid */
	RT_FUNCTAB_MAGIC, "ID_SUPERELL", "superell",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_superell_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_superell_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_superell_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_superell_menu_item)    /* menu_item */
    },

    {
	/* 36 (but "should" be 32) Metaball */
	RT_FUNCTAB_MAGIC, "ID_METABALL", "metaball",
	MGEDFUNCTAB_FUNC_LABELS_CAST(mged_metaball_labels),    /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_metaball_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_metaball_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_metaball_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_metaball_menu_item)    /* menu_item */
    },

    {
	/* 37 */
	RT_FUNCTAB_MAGIC, "ID_BREP", "brep",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 38 (but "should" be 34) Hyperboloid */
	RT_FUNCTAB_MAGIC, "ID_HYP", "hyp",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_hyp_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_hyp_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_hyp_edit),    /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	MGEDFUNCTAB_FUNC_MENU_STR_CAST(mged_generic_menu_str),   /* menu_str */
	MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(mged_hyp_menu_item)    /* menu_item */
    },

    {
	/* 39 */
	RT_FUNCTAB_MAGIC, "ID_CONSTRAINT", "constrnt",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 40 */
	RT_FUNCTAB_MAGIC, "ID_REVOLVE", "revolve",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 41 */
	RT_FUNCTAB_MAGIC, "ID_PNTS", "pnts",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 42 */
	RT_FUNCTAB_MAGIC, "ID_ANNOT", "annot",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 43 */
	RT_FUNCTAB_MAGIC, "ID_HRT", "hrt",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },


    {
	/* 44 */
	RT_FUNCTAB_MAGIC, "ID_DATUM", "datum",
	NULL,  /* label */
	MGEDFUNCTAB_FUNC_KEYPOINT_CAST(mged_generic_keypoint), /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(mged_datum_write_params), /* write_params */
	MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(mged_datum_read_params), /* read_params */
	MGEDFUNCTAB_FUNC_EDIT_CAST(mged_generic_edit), /* edit */
	MGEDFUNCTAB_FUNC_EDITXY_CAST(mged_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },


    {
    /* 45 */
    RT_FUNCTAB_MAGIC, "ID_SCRIPT", "script",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 46 */
	RT_FUNCTAB_MAGIC, "ID_MATERIAL", "material",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* this entry for sanity only */
	0L, ">ID_MAXIMUM", ">id_max",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->s_edit->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    }
};

} /* end extern "C" */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

