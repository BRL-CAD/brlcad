/*                       E D T A B L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 1989-2026 United States Government as represented by
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
/** @file primitives/edtable.cpp
 *
 * Per-primitive methods for solid editing.
 *
 */
/** @} */

#include "common.h"

#include "vmath.h"
#include "./edit_private.h"

extern "C" {

#define EDIT_DECLARE_INTERFACE(name) \
    extern void rt_edit_##name##_labels(int *num_lines, point_t *lines, struct rt_point_labels *pl, int max_pl, const mat_t xform, struct rt_edit *s, struct bn_tol *); \
    extern const char *rt_edit_##name##_keypoint(point_t *pt, const char *keystr, const mat_t mat, struct rt_edit *s, const struct bn_tol *tol); \
    extern void rt_edit_##name##_e_axes_pos(struct rt_edit *s, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern void rt_edit_##name##_write_params(struct bu_vls *p, const struct rt_db_internal *ip, const struct bn_tol *tol, fastf_t base2local); \
    extern void rt_edit_##name##_read_params(struct rt_db_internal *ip, const char *fc, const struct bn_tol *tol, fastf_t local2base); \
    extern int rt_edit_##name##_edit(struct rt_edit *s); \
    extern int rt_edit_##name##_edit_xy(struct rt_edit *s, vect_t mousevec); \
    extern void *rt_edit_##name##_prim_edit_create(struct rt_edit *s); \
    extern void rt_edit_##name##_prim_edit_destroy(void *); \
    extern void rt_edit_##name##_prim_edit_reset(struct rt_edit *s); \
    extern int rt_edit_##name##_menu_str(struct bu_vls *m, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern void rt_edit_##name##_set_edit_mode(struct rt_edit *s, int mode); \
    extern struct rt_edit_menu_item *rt_edit_##name##_menu_item(const struct bn_tol *tol); \

/* Forward declaration for ft_edit_desc implementations */
extern const struct rt_edit_prim_desc *rt_edit_tor_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_tgc_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_ell_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_epa_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_ehy_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_eto_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_hyp_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_rpc_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_rhc_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_superell_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_part_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_cline_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_dsp_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_ebm_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_vol_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_pipe_edit_desc(void);
extern const struct rt_edit_prim_desc *rt_edit_comb_edit_desc(void);

/* Forward declarations for ft_edit_get_params implementations */
extern int rt_edit_tor_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals);
extern int rt_edit_ell_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals);

EDIT_DECLARE_INTERFACE(tor);
EDIT_DECLARE_INTERFACE(tgc);
EDIT_DECLARE_INTERFACE(ell);
EDIT_DECLARE_INTERFACE(arb);
EDIT_DECLARE_INTERFACE(ars);
EDIT_DECLARE_INTERFACE(hlf);
EDIT_DECLARE_INTERFACE(rec);
EDIT_DECLARE_INTERFACE(pg);
EDIT_DECLARE_INTERFACE(bspline);
EDIT_DECLARE_INTERFACE(sph);
EDIT_DECLARE_INTERFACE(ebm);
EDIT_DECLARE_INTERFACE(vol);
EDIT_DECLARE_INTERFACE(arbn);
EDIT_DECLARE_INTERFACE(pipe);
EDIT_DECLARE_INTERFACE(part);
EDIT_DECLARE_INTERFACE(nmg);
EDIT_DECLARE_INTERFACE(rpc);
EDIT_DECLARE_INTERFACE(rhc);
EDIT_DECLARE_INTERFACE(epa);
EDIT_DECLARE_INTERFACE(ehy);
EDIT_DECLARE_INTERFACE(eto);
EDIT_DECLARE_INTERFACE(grp);
EDIT_DECLARE_INTERFACE(hf);
EDIT_DECLARE_INTERFACE(dsp);
EDIT_DECLARE_INTERFACE(sketch);
EDIT_DECLARE_INTERFACE(annot);
EDIT_DECLARE_INTERFACE(extrude);
EDIT_DECLARE_INTERFACE(submodel);
EDIT_DECLARE_INTERFACE(cline);
EDIT_DECLARE_INTERFACE(bot);
EDIT_DECLARE_INTERFACE(comb);
EDIT_DECLARE_INTERFACE(superell);
EDIT_DECLARE_INTERFACE(metaball);
EDIT_DECLARE_INTERFACE(hyp);
EDIT_DECLARE_INTERFACE(revolve);
EDIT_DECLARE_INTERFACE(constraint);
EDIT_DECLARE_INTERFACE(material);
/* EDIT_DECLARE_INTERFACE(binunif); */
EDIT_DECLARE_INTERFACE(pnts);
EDIT_DECLARE_INTERFACE(hrt);
EDIT_DECLARE_INTERFACE(datum);
EDIT_DECLARE_INTERFACE(brep);
EDIT_DECLARE_INTERFACE(joint);
EDIT_DECLARE_INTERFACE(script);

const struct rt_edit_functab EDOBJ[] = {
    {
	/* 0: unused, for sanity checking. */
	RT_FUNCTAB_MAGIC, "ID_NULL", "NULL",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 1 */
	RT_FUNCTAB_MAGIC, "ID_TOR", "tor",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_tor_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_tor_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_tor_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_tor_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_tor_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_tor_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_tor_edit_desc)   /* edit_desc */,
	EDFUNCTAB_FUNC_GET_PARAMS_CAST(rt_edit_tor_get_params)  /* edit_get_params */
    },

    {
	/* 2 */
	RT_FUNCTAB_MAGIC, "ID_TGC", "tgc",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_edit_tgc_e_axes_pos), /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_tgc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_tgc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_tgc_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_tgc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_tgc_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_tgc_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_tgc_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 3 */
	RT_FUNCTAB_MAGIC, "ID_ELL", "ell",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_ell_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_ell_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_ell_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_ell_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_ell_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_ell_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_ell_edit_desc)   /* edit_desc */,
	EDFUNCTAB_FUNC_GET_PARAMS_CAST(rt_edit_ell_get_params)  /* edit_get_params */
    },

    {
	/* 4 */
	RT_FUNCTAB_MAGIC, "ID_ARB8", "arb8",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_arb_keypoint),     /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_edit_arb_e_axes_pos), /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_arb_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_arb_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_arb_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_arb_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_arb_prim_edit_create),    /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_arb_prim_edit_destroy),  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_edit_arb_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_arb_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_arb_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 5 */
	RT_FUNCTAB_MAGIC, "ID_ARS", "ars",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_edit_ars_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_ars_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_ars_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_ars_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_ars_prim_edit_create),    /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_ars_prim_edit_destroy),  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_edit_ars_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_ars_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_ars_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 6 */
	RT_FUNCTAB_MAGIC, "ID_HALF", "half",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_hlf_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_hlf_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 7 */
	RT_FUNCTAB_MAGIC, "ID_REC", "rec",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_edit_tgc_e_axes_pos), /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_tgc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_tgc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_tgc_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_tgc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_tgc_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_tgc_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 8 */
	RT_FUNCTAB_MAGIC, "ID_POLY", "poly",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
 	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 9 */
	RT_FUNCTAB_MAGIC, "ID_BSPLINE", "bspline",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_edit_bspline_labels), /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_bspline_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_bspline_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_bspline_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_bspline_prim_edit_create),    /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_bspline_prim_edit_destroy),  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_bspline_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_bspline_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 10 */
	RT_FUNCTAB_MAGIC, "ID_SPH", "sph",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_ell_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_ell_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_ell_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_ell_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 11 */
	RT_FUNCTAB_MAGIC, "ID_NMG", "nmg",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_edit_nmg_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_nmg_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_nmg_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_nmg_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_nmg_prim_edit_create),    /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_nmg_prim_edit_destroy),  /* prim edit destroy */
	EDFUNCTAB_FUNC_PRIMEDIT_RESET_CAST(rt_edit_nmg_prim_edit_reset),  /* prim edit reset */
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_nmg_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_nmg_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 12 */
	RT_FUNCTAB_MAGIC, "ID_EBM", "ebm",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_ebm_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_ebm_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_ebm_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_ebm_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_ebm_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 13 */
	RT_FUNCTAB_MAGIC, "ID_VOL", "vol",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_vol_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_vol_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_vol_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_vol_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_vol_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 14 */
	RT_FUNCTAB_MAGIC, "ID_ARBN", "arbn",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 15 */
	RT_FUNCTAB_MAGIC, "ID_PIPE", "pipe",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_edit_pipe_labels), /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_pipe_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_pipe_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_pipe_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_pipe_prim_edit_create),    /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_pipe_prim_edit_destroy),  /* prim edit destroy */
	EDFUNCTAB_FUNC_PRIMEDIT_RESET_CAST(rt_edit_pipe_prim_edit_reset),  /* prim edit reset */
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_pipe_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_pipe_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_pipe_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 16 */
	RT_FUNCTAB_MAGIC, "ID_PARTICLE", "part",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_part_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_part_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_part_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_part_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_part_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_part_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_part_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 17 */
	RT_FUNCTAB_MAGIC, "ID_RPC", "rpc",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_rpc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_rpc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_rpc_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_rpc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_rpc_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_rpc_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_rpc_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 18 */
	RT_FUNCTAB_MAGIC, "ID_RHC", "rhc",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_rhc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_rhc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_rhc_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_rhc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_rhc_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_rhc_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_rhc_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 19 */
	RT_FUNCTAB_MAGIC, "ID_EPA", "epa",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_epa_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_epa_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_epa_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_epa_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_epa_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_epa_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_epa_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 20 */
	RT_FUNCTAB_MAGIC, "ID_EHY", "ehy",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_ehy_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_ehy_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_ehy_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_ehy_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_ehy_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_ehy_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_ehy_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 21 */
	RT_FUNCTAB_MAGIC, "ID_ETO", "eto",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_eto_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_eto_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_eto_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_eto_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_eto_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_eto_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_eto_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 22 */
	RT_FUNCTAB_MAGIC, "ID_GRIP", "grip",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_grp_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_grp_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_grp_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy - grip uses generic XY: center/normal/magnitude are set via tedit or direct parameter input, not interactive mouse drag */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 23 -- ID_JOINT: pseudo-solid used for animation constraints.
	 * Joint primitives represent kinematic joints and are not directly
	 * interactive-edited via solid edit mode; they are manipulated via
	 * the animation "simulate" framework.  Basic matrix-level editing
	 * (translate/rotate/scale) is supported via edit_generic. */
	RT_FUNCTAB_MAGIC, "ID_JOINT", "joint",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 24 */
	RT_FUNCTAB_MAGIC, "ID_HF", "hf",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 25 Displacement Map (alt. height field) */
	RT_FUNCTAB_MAGIC, "ID_DSP", "dsp",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_dsp_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_dsp_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_dsp_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_dsp_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_dsp_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 26 2D sketch */
	RT_FUNCTAB_MAGIC, "ID_SKETCH", "sketch",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_sketch_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_sketch_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_sketch_prim_edit_create), /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_sketch_prim_edit_destroy), /* prim edit destroy */
	EDFUNCTAB_FUNC_PRIMEDIT_RESET_CAST(rt_edit_sketch_prim_edit_reset), /* prim edit reset */
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_sketch_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_sketch_menu_item)  /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 27 Solid of extrusion */
	RT_FUNCTAB_MAGIC, "ID_EXTRUDE", "extrude",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_extrude_keypoint),     /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_edit_extrude_e_axes_pos), /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_extrude_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_extrude_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_extrude_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_extrude_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 28 Instanced submodel */
	RT_FUNCTAB_MAGIC, "ID_SUBMODEL", "submodel",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
	NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 29 Fastgen cline solid */
	RT_FUNCTAB_MAGIC, "ID_CLINE", "cline",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint),   /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_edit_cline_e_axes_pos), /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_cline_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_cline_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_cline_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_cline_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_cline_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 30 Bag o' Triangles */
	RT_FUNCTAB_MAGIC, "ID_BOT", "bot",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_edit_bot_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_bot_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_bot_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_bot_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_bot_prim_edit_create),    /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_bot_prim_edit_destroy),  /* prim edit destroy */
	EDFUNCTAB_FUNC_PRIMEDIT_RESET_CAST(rt_edit_bot_prim_edit_reset),  /* prim edit reset */
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_bot_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_bot_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 31 combination objects */
	RT_FUNCTAB_MAGIC, "ID_COMBINATION", "comb",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_comb_edit),              /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_comb_edit_xy),         /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_comb_prim_edit_create),   /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_comb_prim_edit_destroy), /* prim edit destroy */
	NULL,  /* prim edit reset */
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),              /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_comb_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_comb_menu_item)     /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_comb_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 32 available placeholder to not offset latter table indices
	 * (was ID_BINEXPM)
	 */
	RT_FUNCTAB_MAGIC, "ID_UNUSED1", "UNUSED1",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 33 */
	RT_FUNCTAB_MAGIC, "ID_BINUNIF", "binunif",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 34 available placeholder to not offset latter table indices
	 * (was ID_BINMIME)
	 */
	RT_FUNCTAB_MAGIC, "ID_UNUSED2", "UNUSED2",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 35 (but "should" be 31) Superquadratic Ellipsoid */
	RT_FUNCTAB_MAGIC, "ID_SUPERELL", "superell",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_superell_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_superell_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_superell_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_superell_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_superell_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_superell_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_superell_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 36 (but "should" be 32) Metaball */
	RT_FUNCTAB_MAGIC, "ID_METABALL", "metaball",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_edit_metaball_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_edit_metaball_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_metaball_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_metaball_edit_xy), /* edit xy */
	EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(rt_edit_metaball_prim_edit_create),    /* prim edit create */
	EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(rt_edit_metaball_prim_edit_destroy),  /* prim edit destroy */
	EDFUNCTAB_FUNC_PRIMEDIT_RESET_CAST(rt_edit_metaball_prim_edit_reset),      /* prim edit reset */
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_metaball_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_metaball_menu_item)    /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 37 */
	RT_FUNCTAB_MAGIC, "ID_BREP", "brep",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy - BREP has a dedicated editor in Archer (BRep editor plugin); basic matrix-level XY editing via edit_generic_xy is the fallback here */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 38 (but "should" be 34) Hyperboloid */
	RT_FUNCTAB_MAGIC, "ID_HYP", "hyp",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_hyp_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_hyp_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_edit_hyp_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_edit_hyp_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	EDFUNCTAB_FUNC_MENU_STR_CAST(edit_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_SET_EDIT_MODE_CAST(rt_edit_hyp_set_edit_mode), /* set edit mode */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_edit_hyp_menu_item)    /* menu_item */,
	EDFUNCTAB_FUNC_EDIT_DESC_CAST(rt_edit_hyp_edit_desc)   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 39 */
	RT_FUNCTAB_MAGIC, "ID_CONSTRAINT", "constrnt",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 40 */
	RT_FUNCTAB_MAGIC, "ID_REVOLVE", "revolve",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 41 */
	RT_FUNCTAB_MAGIC, "ID_PNTS", "pnts",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 42 */
	RT_FUNCTAB_MAGIC, "ID_ANNOT", "annot",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 43 */
	RT_FUNCTAB_MAGIC, "ID_HRT", "hrt",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },


    {
	/* 44 */
	RT_FUNCTAB_MAGIC, "ID_DATUM", "datum",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(edit_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_edit_datum_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_edit_datum_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(edit_generic), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(edit_generic_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },


    {
    /* 45 */
    RT_FUNCTAB_MAGIC, "ID_SCRIPT", "script",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* 46 */
	RT_FUNCTAB_MAGIC, "ID_MATERIAL", "material",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
    },

    {
	/* this entry for sanity only */
	0L, ">ID_MAXIMUM", ">id_max",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	NULL,  /* edit */
	NULL,  /* exit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
       	NULL,  /* prim edit reset*/
	NULL,  /* menu_str */
	NULL,  /* set edit mode */
        NULL   /* menu_item */,
	NULL   /* edit_desc */,
	NULL   /* edit_get_params */
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

