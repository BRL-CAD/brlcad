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
 * Per-primitive methods for solid editing.
 *
 */
/** @} */

#include "common.h"

#include "vmath.h"
#include "./edfunctab.h"

extern "C" {

#define EDIT_DECLARE_INTERFACE(name) \
    extern void rt_solid_edit_##name##_labels(int *num_lines, point_t *lines, struct rt_point_labels *pl, int max_pl, const mat_t xform, struct rt_db_internal *ip, struct bn_tol *); \
    extern const char *rt_solid_edit_##name##_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern void rt_solid_edit_##name##_e_axes_pos(struct rt_solid_edit *s, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern void rt_solid_edit_##name##_write_params(struct bu_vls *p, const struct rt_db_internal *ip, const struct bn_tol *tol, fastf_t base2local); \
    extern void rt_solid_edit_##name##_read_params(struct rt_db_internal *ip, const char *fc, const struct bn_tol *tol, fastf_t local2base); \
    extern int rt_solid_edit_##name##_edit(struct rt_solid_edit *s, int edflag); \
    extern int rt_solid_edit_##name##_edit_xy(struct rt_solid_edit *s, int edflag, vect_t mousevec); \
    extern void *rt_solid_edit_##name##_prim_edit_create(void); \
    extern void rt_solid_edit_##name##_prim_edit_destroy(void *); \
    extern int rt_solid_edit_##name##_menu_str(struct bu_vls *m, const struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern struct rt_solid_edit_menu_item *rt_solid_edit_##name##_menu_item(const struct bn_tol *tol); \

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

const struct rt_solid_edit_functab EDOBJ[] = {
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
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_tor_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_tor_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_tor_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_tor_menu_item)    /* menu_item */
    },

    {
	/* 2 */
	RT_FUNCTAB_MAGIC, "ID_TGC", "tgc",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_solid_edit_tgc_e_axes_pos), /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_tgc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_tgc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_tgc_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_tgc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_tgc_menu_item)    /* menu_item */
    },

    {
	/* 3 */
	RT_FUNCTAB_MAGIC, "ID_ELL", "ell",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_ell_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_ell_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_ell_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_ell_menu_item)    /* menu_item */
    },

    {
	/* 4 */
	RT_FUNCTAB_MAGIC, "ID_ARB8", "arb8",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_arb_keypoint),     /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_solid_edit_arb_e_axes_pos), /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_arb_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_arb_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_arb_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_arb_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_arb_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_arb_menu_item)    /* menu_item */
    },

    {
	/* 5 */
	RT_FUNCTAB_MAGIC, "ID_ARS", "ars",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_solid_edit_ars_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_ars_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_ars_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_ars_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_ars_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_ars_menu_item)    /* menu_item */
    },

    {
	/* 6 */
	RT_FUNCTAB_MAGIC, "ID_HALF", "half",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_hlf_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_hlf_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 7 */
	RT_FUNCTAB_MAGIC, "ID_REC", "rec",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_solid_edit_tgc_e_axes_pos), /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_tgc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_tgc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_tgc_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_tgc_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_tgc_menu_item)    /* menu_item */
    },

    {
	/* 8 */
	RT_FUNCTAB_MAGIC, "ID_POLY", "poly",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
 	NULL   /* menu_item */
    },

    {
	/* 9 */
	RT_FUNCTAB_MAGIC, "ID_BSPLINE", "bspline",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_solid_edit_bspline_labels), /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_bspline_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_bspline_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_bspline_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_bspline_menu_item)    /* menu_item */
    },

    {
	/* 10 */
	RT_FUNCTAB_MAGIC, "ID_SPH", "sph",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_ell_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_ell_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_ell_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 11 */
	RT_FUNCTAB_MAGIC, "ID_NMG", "nmg",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_solid_edit_nmg_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_nmg_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_nmg_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_nmg_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_nmg_menu_item)    /* menu_item */
    },

    {
	/* 12 */
	RT_FUNCTAB_MAGIC, "ID_EBM", "ebm",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_ebm_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_ebm_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_ebm_menu_item)    /* menu_item */
    },

    {
	/* 13 */
	RT_FUNCTAB_MAGIC, "ID_VOL", "vol",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_vol_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_vol_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_vol_menu_item)    /* menu_item */
    },

    {
	/* 14 */
	RT_FUNCTAB_MAGIC, "ID_ARBN", "arbn",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 15 */
	RT_FUNCTAB_MAGIC, "ID_PIPE", "pipe",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_solid_edit_pipe_labels), /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_pipe_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_pipe_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_pipe_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_pipe_menu_item)    /* menu_item */
    },

    {
	/* 16 */
	RT_FUNCTAB_MAGIC, "ID_PARTICLE", "part",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_part_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_part_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_part_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_part_menu_item)    /* menu_item */
    },

    {
	/* 17 */
	RT_FUNCTAB_MAGIC, "ID_RPC", "rpc",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_rpc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_rpc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_rpc_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_rpc_menu_item)    /* menu_item */
    },

    {
	/* 18 */
	RT_FUNCTAB_MAGIC, "ID_RHC", "rhc",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_rhc_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_rhc_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_rhc_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_rhc_menu_item)    /* menu_item */
    },

    {
	/* 19 */
	RT_FUNCTAB_MAGIC, "ID_EPA", "epa",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_epa_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_epa_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_epa_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_epa_menu_item)    /* menu_item */
    },

    {
	/* 20 */
	RT_FUNCTAB_MAGIC, "ID_EHY", "ehy",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_ehy_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_ehy_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_ehy_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_ehy_menu_item)    /* menu_item */
    },

    {
	/* 21 */
	RT_FUNCTAB_MAGIC, "ID_ETO", "eto",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_eto_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_eto_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_eto_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_eto_menu_item)    /* menu_item */
    },

    {
	/* 22 */
	RT_FUNCTAB_MAGIC, "ID_GRIP", "grip",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_grp_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_grp_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_grp_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
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
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 24 */
	RT_FUNCTAB_MAGIC, "ID_HF", "hf",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 25 Displacement Map (alt. height field) */
	RT_FUNCTAB_MAGIC, "ID_DSP", "dsp",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_dsp_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_dsp_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_dsp_menu_item)    /* menu_item */
    },

    {
	/* 26 2D sketch */
	RT_FUNCTAB_MAGIC, "ID_SKETCH", "sketch",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 27 Solid of extrusion */
	RT_FUNCTAB_MAGIC, "ID_EXTRUDE", "extrude",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_extrude_keypoint),     /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_solid_edit_extrude_e_axes_pos), /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_extrude_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_extrude_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_extrude_menu_item)    /* menu_item */
    },

    {
	/* 28 Instanced submodel */
	RT_FUNCTAB_MAGIC, "ID_SUBMODEL", "submodel",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
	NULL   /* menu_item */
    },

    {
	/* 29 Fastgen cline solid */
	RT_FUNCTAB_MAGIC, "ID_CLINE", "cline",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint),   /* keypoint */
	EDFUNCTAB_FUNC_E_AXES_POS_CAST(rt_solid_edit_cline_e_axes_pos), /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_cline_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_cline_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_cline_menu_item)    /* menu_item */
    },

    {
	/* 30 Bag o' Triangles */
	RT_FUNCTAB_MAGIC, "ID_BOT", "bot",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_solid_edit_bot_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_bot_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_bot_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_bot_edit_xy), /* edit xy */
	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_bot_menu_item)    /* menu_item */
    },

    {
	/* 31 combination objects (should not be in this table) */
	RT_FUNCTAB_MAGIC, "ID_COMBINATION", "comb",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
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
	NULL,  /* s->e_axes_pos */
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
	NULL,  /* s->e_axes_pos */
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
	NULL,  /* s->e_axes_pos */
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
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_superell_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_superell_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_superell_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_superell_menu_item)    /* menu_item */
    },

    {
	/* 36 (but "should" be 32) Metaball */
	RT_FUNCTAB_MAGIC, "ID_METABALL", "metaball",
	EDFUNCTAB_FUNC_LABELS_CAST(rt_solid_edit_metaball_labels),    /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_metaball_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_metaball_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_metaball_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_metaball_menu_item)    /* menu_item */
    },

    {
	/* 37 */
	RT_FUNCTAB_MAGIC, "ID_BREP", "brep",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 38 (but "should" be 34) Hyperboloid */
	RT_FUNCTAB_MAGIC, "ID_HYP", "hyp",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_hyp_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_hyp_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_hyp_edit),    /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	EDFUNCTAB_FUNC_MENU_STR_CAST(rt_solid_edit_generic_menu_str),   /* menu_str */
	EDFUNCTAB_FUNC_MENU_ITEM_CAST(rt_solid_edit_hyp_menu_item)    /* menu_item */
    },

    {
	/* 39 */
	RT_FUNCTAB_MAGIC, "ID_CONSTRAINT", "constrnt",
	NULL,  /* label */
	NULL,  /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
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
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
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
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },

    {
	/* 42 */
	RT_FUNCTAB_MAGIC, "ID_ANNOT", "annot",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
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
	NULL,  /* s->e_axes_pos */
	NULL,  /* write_params */
	NULL,  /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
       	NULL,  /* prim edit create */
	NULL,  /* prim edit destroy */
	NULL,  /* menu_str */
        NULL   /* menu_item */
    },


    {
	/* 44 */
	RT_FUNCTAB_MAGIC, "ID_DATUM", "datum",
	NULL,  /* label */
	EDFUNCTAB_FUNC_KEYPOINT_CAST(rt_solid_edit_generic_keypoint), /* keypoint */
	NULL,  /* s->e_axes_pos */
	EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(rt_solid_edit_datum_write_params), /* write_params */
	EDFUNCTAB_FUNC_READ_PARAMS_CAST(rt_solid_edit_datum_read_params), /* read_params */
	EDFUNCTAB_FUNC_EDIT_CAST(rt_solid_edit_generic_edit), /* edit */
	EDFUNCTAB_FUNC_EDITXY_CAST(rt_solid_edit_generic_edit_xy), /* edit xy */
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
	NULL,  /* s->e_axes_pos */
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
	NULL,  /* s->e_axes_pos */
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
	NULL,  /* s->e_axes_pos */
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

