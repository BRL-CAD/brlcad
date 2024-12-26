/*                           M E N U . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file mged/menu.h
 */

#ifndef MGED_MENU_H
#define MGED_MENU_H

#include "common.h"
#include "mged.h"

/* These values end up in es_menu, as do ARB vertex numbers */
#define MENU_TOR_R1		21
#define MENU_TOR_R2		22
#define MENU_TGC_ROT_H		23
#define MENU_TGC_ROT_AB 	24
#define MENU_TGC_MV_H		25
#define MENU_TGC_MV_HH		26
#define MENU_TGC_SCALE_H	27
#define MENU_TGC_SCALE_H_V	28
#define MENU_TGC_SCALE_A	29
#define MENU_TGC_SCALE_B	30
#define MENU_TGC_SCALE_C	31
#define MENU_TGC_SCALE_D	32
#define MENU_TGC_SCALE_AB	33
#define MENU_TGC_SCALE_CD	34
#define MENU_TGC_SCALE_ABCD	35
#define MENU_ELL_SCALE_A	39
#define MENU_ELL_SCALE_B	40
#define MENU_ELL_SCALE_C	41
#define MENU_ELL_SCALE_ABC	42
#define MENU_RPC_B		43
#define MENU_RPC_H		44
#define MENU_RPC_R		45
#define MENU_RHC_B		46
#define MENU_RHC_H		47
#define MENU_RHC_R		48
#define MENU_RHC_C		49
#define MENU_EPA_H		50
#define MENU_EPA_R1		51
#define MENU_EPA_R2		52
#define MENU_EHY_H		53
#define MENU_EHY_R1		54
#define MENU_EHY_R2		55
#define MENU_EHY_C		56
#define MENU_ETO_R		57
#define MENU_ETO_RD		58
#define MENU_ETO_SCALE_C	59
#define MENU_ETO_ROT_C		60

#define MENU_VOL_FNAME		75
#define MENU_VOL_FSIZE		76
#define MENU_VOL_CSIZE		77
#define MENU_VOL_THRESH_LO	78
#define MENU_VOL_THRESH_HI	79
#define MENU_EBM_FNAME		80
#define MENU_EBM_FSIZE		81
#define MENU_EBM_HEIGHT		82
#define MENU_DSP_FNAME		83
#define MENU_DSP_FSIZE		84	/* Not implemented yet */
#define MENU_DSP_SCALE_X	85
#define MENU_DSP_SCALE_Y	86
#define MENU_DSP_SCALE_ALT	87
#define MENU_PART_H		88
#define MENU_PART_v		89
#define MENU_PART_h		90
#define MENU_BOT_PICKV		91
#define MENU_BOT_PICKE		92
#define MENU_BOT_PICKT		93
#define MENU_BOT_MOVEV		94
#define MENU_BOT_MOVEE		95
#define MENU_BOT_MOVET		96
#define MENU_BOT_MODE		97
#define MENU_BOT_ORIENT		98
#define MENU_BOT_THICK		99
#define MENU_BOT_FMODE		100
#define MENU_BOT_DELETE_TRI	101
#define MENU_BOT_FLAGS		102
#define MENU_EXTR_SCALE_H	103
#define MENU_EXTR_MOV_H		104
#define MENU_EXTR_ROT_H		105
#define MENU_EXTR_SKT_NAME	106
#define MENU_CLINE_SCALE_H	107
#define MENU_CLINE_MOVE_H	108
#define MENU_CLINE_SCALE_R	109
#define MENU_CLINE_SCALE_T	110
#define MENU_TGC_SCALE_H_CD	111
#define MENU_TGC_SCALE_H_V_AB	112
#define MENU_SUPERELL_SCALE_A	113
#define MENU_SUPERELL_SCALE_B	114
#define MENU_SUPERELL_SCALE_C	115
#define MENU_SUPERELL_SCALE_ABC	116
#define MENU_METABALL_SET_THRESHOLD	117
#define MENU_METABALL_SET_METHOD	118
#define MENU_METABALL_PT_SET_GOO	119
#define MENU_METABALL_SELECT	120
#define MENU_METABALL_NEXT_PT	121
#define MENU_METABALL_PREV_PT	122
#define MENU_METABALL_MOV_PT	123
#define MENU_METABALL_PT_FLDSTR	124
#define MENU_METABALL_DEL_PT	125
#define MENU_METABALL_ADD_PT	126
#define MENU_HYP_H              127
#define MENU_HYP_SCALE_A        128
#define MENU_HYP_SCALE_B	129
#define MENU_HYP_C		130
#define MENU_HYP_ROT_H		131

extern struct menu_item ars_menu[];
extern struct menu_item ars_pick_menu[];
extern struct menu_item bot_menu[];
extern struct menu_item cline_menu[];
extern struct menu_item cntrl_menu[];
extern struct menu_item dsp_menu[];
extern struct menu_item ebm_menu[];
extern struct menu_item ehy_menu[];
extern struct menu_item ell_menu[];
extern struct menu_item epa_menu[];
extern struct menu_item eto_menu[];
extern struct menu_item extr_menu[];
extern struct menu_item hyp_menu[];
extern struct menu_item metaball_menu[];
extern struct menu_item nmg_menu[];
extern struct menu_item part_menu[];
extern struct menu_item rhc_menu[];
extern struct menu_item rpc_menu[];
extern struct menu_item spline_menu[];
extern struct menu_item superell_menu[];
extern struct menu_item tgc_menu[];
extern struct menu_item tor_menu[];
extern struct menu_item vol_menu[];

extern struct menu_item point4_menu[];
extern struct menu_item edge5_menu[];
extern struct menu_item edge6_menu[];
extern struct menu_item edge7_menu[];
extern struct menu_item edge8_menu[];
extern struct menu_item mv4_menu[];
extern struct menu_item mv5_menu[];
extern struct menu_item mv6_menu[];
extern struct menu_item mv7_menu[];
extern struct menu_item mv8_menu[];
extern struct menu_item rot4_menu[];
extern struct menu_item rot5_menu[];
extern struct menu_item rot6_menu[];
extern struct menu_item rot7_menu[];
extern struct menu_item rot8_menu[];
extern struct menu_item *which_menu[];

extern void mmenu_init(struct mged_state *s);
extern void mmenu_display(struct mged_state *s, int y_top);
extern void mmenu_set(struct mged_state *s, int idx, struct menu_item *value);
extern void mmenu_set_all(struct mged_state *s, int idx, struct menu_item *value);
extern void sedit_menu(struct mged_state *s);
extern int mmenu_select(struct mged_state *s, int pen_y, int do_func);

#endif  /* MGED_MGED_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
