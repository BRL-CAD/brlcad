/*                         E D S O L . C
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file edsol.c
 *
 * Functions -
 *	init_sedit	set up for a Solid Edit
 *	sedit		Apply Solid Edit transformation(s)
 *	pscale		Partial scaling of a solid
 *	init_oedit	set up for object edit?
 *	f_dextrude()	extrude a drawing (nmg wire loop) to create a solid
 *	f_eqn		change face of GENARB8 to new equation
 *
 *  Authors -
 *	Keith A. Applin
 *	Bob Suckling
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nurb.h"
#include "wdb.h"
#include "db.h"

#include "./ged.h"
#include "./mged_solid.h"
#include "./sedit.h"
#include "./mged_dm.h"


#if 1
#define TRY_EDIT_NEW_WAY
#endif

extern struct bn_tol		mged_tol;	/* from ged.c */

/* from librt */
extern const int rt_arb_faces[5][24];
extern short earb4[5][18];
extern short earb5[9][18];
extern short earb6[10][18];
extern short earb7[12][18];
extern short earb8[12][18];

static void	arb8_edge(int arg), ars_ed(int arg), ell_ed(int arg), tgc_ed(int arg), tor_ed(int arg), spline_ed(int arg);
static void	nmg_ed(int arg), pipe_ed(int arg), vol_ed(int arg), ebm_ed(int arg), dsp_ed(int arg), cline_ed(int arg), bot_ed(int arg), extr_ed(int arg);
static void	rpc_ed(int arg), rhc_ed(int arg), part_ed(int arg), epa_ed(int arg), ehy_ed(int arg), eto_ed(int arg);
static void	superell_ed(int arg);

static void	arb7_edge(int arg), arb6_edge(int arg), arb5_edge(int arg), arb4_point(int arg);
static void	arb8_mv_face(int arg), arb7_mv_face(int arg), arb6_mv_face(int arg);
static void	arb5_mv_face(int arg), arb4_mv_face(int arg), arb8_rot_face(int arg), arb7_rot_face(int arg);
static void 	arb6_rot_face(int arg), arb5_rot_face(int arg), arb4_rot_face(int arg), arb_control(int arg);
static void	init_sedit_vars(void), init_oedit_vars(void), init_oedit_guts(void);

void pscale(void);
void update_edit_absolute_tran(fastf_t *view_pos);
void set_e_axes_pos(int both);
point_t e_axes_pos;
point_t curr_e_axes_pos;
short int fixv;		/* used in ECMD_ARB_ROTATE_FACE,f_eqn(): fixed vertex */

MGED_EXTERN( struct wdb_pipept *find_pipept_nearest_pt, (const struct bu_list *pipe_hd, const point_t pt ) );
MGED_EXTERN( void split_pipept, (struct bu_list *pipe_hd, struct wdb_pipept *ps, point_t pt ) );
MGED_EXTERN( struct wdb_pipept *del_pipept, (struct wdb_pipept *ps ) );
MGED_EXTERN( struct wdb_pipept *add_pipept, (struct rt_pipe_internal *pipe, struct wdb_pipept *pp, const point_t new_pt ) );

/* data for solid editing */
int			sedraw;	/* apply solid editing changes */

struct rt_db_internal	es_int;
struct rt_db_internal	es_int_orig;

int	es_type;		/* COMGEOM solid type */
int     es_edflag;		/* type of editing for this solid */
int     es_edclass;		/* type of editing class for this solid */
fastf_t	es_scale;		/* scale factor */
fastf_t	es_peqn[7][4];		/* ARBs defining plane equations */
fastf_t	es_m[3];		/* edge(line) slope */
mat_t	es_mat;			/* accumulated matrix of path */
mat_t 	es_invmat;		/* inverse of es_mat   KAA */

int bot_verts[3];		/* vertices for the BOT solid */

point_t	es_keypoint;		/* center of editing xforms */
char	*es_keytag;		/* string identifying the keypoint */
int	es_keyfixed;		/* keypoint specified by user? */

vect_t		es_para;	/* keyboard input param. Only when inpara set.  */
int		inpara;		/* es_para valid.  es_mvalid must = 0 */
static vect_t	es_mparam;	/* mouse input param.  Only when es_mvalid set */
static int	es_mvalid;	/* es_mparam valid.  inpara must = 0 */

static int	spl_surfno;	/* What surf & ctl pt to edit on spline */
static int	spl_ui;
static int	spl_vi;

static int	es_ars_crv;	/* curve and column identifying selected ARS point */
static int	es_ars_col;
static point_t	es_pt;		/* coordinates of selected ARS point */

static struct edgeuse	*es_eu=(struct edgeuse *)NULL;	/* Currently selected NMG edgeuse */
static struct loopuse	*lu_copy=(struct loopuse*)NULL;	/* copy of loop to be extruded */
static plane_t		lu_pl;	/* plane equation for loop to be extruded */
static struct shell	*es_s=(struct shell *)NULL;	/* Shell where extrusion is to end up */
static point_t		lu_keypoint;	/* keypoint of lu_copy for extrusion */

static struct wdb_pipept *es_pipept=(struct wdb_pipept *)NULL; /* Currently selected PIPE segment */

/*  These values end up in es_menu, as do ARB vertex numbers */
int	es_menu;		/* item selected from menu */
#define MENU_TOR_R1		21
#define MENU_TOR_R2		22
#define MENU_TGC_ROT_H		23
#define MENU_TGC_ROT_AB 	24
#define	MENU_TGC_MV_H		25
#define MENU_TGC_MV_HH		26
#define MENU_TGC_SCALE_H	27
#define	MENU_TGC_SCALE_H_V	28
#define MENU_TGC_SCALE_A	29
#define MENU_TGC_SCALE_B	30
#define MENU_TGC_SCALE_C	31
#define MENU_TGC_SCALE_D	32
#define MENU_TGC_SCALE_AB	33
#define MENU_TGC_SCALE_CD	34
#define MENU_TGC_SCALE_ABCD	35
#define MENU_ARB_MV_EDGE	36
#define MENU_ARB_MV_FACE	37
#define MENU_ARB_ROT_FACE	38
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
#define	MENU_PIPE_SELECT	61
#define	MENU_PIPE_NEXT_PT	62
#define MENU_PIPE_PREV_PT	63
#define MENU_PIPE_SPLIT		64
#define MENU_PIPE_PT_OD		65
#define MENU_PIPE_PT_ID		66
#define	MENU_PIPE_SCALE_OD	67
#define	MENU_PIPE_SCALE_ID	68
#define	MENU_PIPE_ADD_PT	69
#define MENU_PIPE_INS_PT	70
#define MENU_PIPE_DEL_PT	71
#define	MENU_PIPE_MOV_PT	72
#define	MENU_PIPE_PT_RADIUS	73
#define	MENU_PIPE_SCALE_RADIUS	74
#define	MENU_VOL_FNAME		75
#define	MENU_VOL_FSIZE		76
#define	MENU_VOL_CSIZE		77
#define	MENU_VOL_THRESH_LO	78
#define	MENU_VOL_THRESH_HI	79
#define	MENU_EBM_FNAME		80
#define	MENU_EBM_FSIZE		81
#define	MENU_EBM_HEIGHT		82
#define	MENU_DSP_FNAME		83
#define	MENU_DSP_FSIZE		84	/* Not implemented yet */
#define	MENU_DSP_SCALE_X	85
#define	MENU_DSP_SCALE_Y	86
#define	MENU_DSP_SCALE_ALT	87
#define MENU_PART_H		88
#define MENU_PART_v		89
#define MENU_PART_h		90
#define MENU_BOT_PICKV		91
#define	MENU_BOT_PICKE		92
#define	MENU_BOT_PICKT		93
#define	MENU_BOT_MOVEV		94
#define	MENU_BOT_MOVEE		95
#define	MENU_BOT_MOVET		96
#define	MENU_BOT_MODE		97
#define	MENU_BOT_ORIENT		98
#define	MENU_BOT_THICK		99
#define	MENU_BOT_FMODE		100
#define MENU_BOT_DELETE_TRI	101
#define MENU_BOT_FLAGS		102
#define MENU_EXTR_SCALE_H	103
#define MENU_EXTR_MOV_H		104
#define MENU_EXTR_ROT_H		105
#define MENU_EXTR_SKT_NAME	106
#define	MENU_CLINE_SCALE_H	107
#define	MENU_CLINE_MOVE_H	108
#define MENU_CLINE_SCALE_R	109
#define	MENU_CLINE_SCALE_T	110
#define MENU_TGC_SCALE_H_CD	111
#define	MENU_TGC_SCALE_H_V_AB	112
#define MENU_SUPERELL_SCALE_A	113
#define MENU_SUPERELL_SCALE_B	114
#define MENU_SUPERELL_SCALE_C	115
#define MENU_SUPERELL_SCALE_ABC	116


struct menu_item cline_menu[] = {
	{ "CLINE MENU",		(void (*)())NULL, 0 },
	{ "Set H",		cline_ed, ECMD_CLINE_SCALE_H },
	{ "Move End H",		cline_ed, ECMD_CLINE_MOVE_H },
	{ "Set R",		cline_ed, ECMD_CLINE_SCALE_R },
	{ "Set plate thickness", cline_ed, ECMD_CLINE_SCALE_T },
	{ "", (void (*)())NULL, 0 }
};

struct  menu_item extr_menu[] = {
	{ "EXTRUSION MENU",	(void (*)())NULL, 0 },
	{ "Set H",		extr_ed, ECMD_EXTR_SCALE_H },
	{ "Move End H",		extr_ed, ECMD_EXTR_MOV_H },
	{ "Rotate H",		extr_ed, ECMD_EXTR_ROT_H },
	{ "Referenced Sketch",	extr_ed, ECMD_EXTR_SKT_NAME },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item ars_pick_menu[] = {
	{ "ARS PICK MENU", (void (*)())NULL, 0 },
	{ "Pick Vertex", ars_ed, ECMD_ARS_PICK },
	{ "Next Vertex", ars_ed, ECMD_ARS_NEXT_PT },
	{ "Prev Vertex", ars_ed, ECMD_ARS_PREV_PT },
	{ "Next Curve", ars_ed, ECMD_ARS_NEXT_CRV },
	{ "Prev Curve", ars_ed, ECMD_ARS_PREV_CRV },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item ars_menu[] = {
	{ "ARS MENU", (void (*)())NULL, 0 },
	{ "Pick Vertex", ars_ed, ECMD_ARS_PICK_MENU },
	{ "Move Point", ars_ed, ECMD_ARS_MOVE_PT },
	{ "Delete Curve", ars_ed, ECMD_ARS_DEL_CRV },
	{ "Delete Column", ars_ed, ECMD_ARS_DEL_COL },
	{ "Dup Curve", ars_ed, ECMD_ARS_DUP_CRV },
	{ "Dup Column", ars_ed, ECMD_ARS_DUP_COL },
	{ "Move Curve", ars_ed, ECMD_ARS_MOVE_CRV },
	{ "Move Column", ars_ed, ECMD_ARS_MOVE_COL },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  edge8_menu[] = {
	{ "ARB8 EDGES", (void (*)())NULL, 0 },
	{ "Move Edge 12", arb8_edge, 0 },
	{ "Move Edge 23", arb8_edge, 1 },
	{ "Move Edge 34", arb8_edge, 2 },
	{ "Move Edge 14", arb8_edge, 3 },
	{ "Move Edge 15", arb8_edge, 4 },
	{ "Move Edge 26", arb8_edge, 5 },
	{ "Move Edge 56", arb8_edge, 6 },
	{ "Move Edge 67", arb8_edge, 7 },
	{ "Move Edge 78", arb8_edge, 8 },
	{ "Move Edge 58", arb8_edge, 9 },
	{ "Move Edge 37", arb8_edge, 10 },
	{ "Move Edge 48", arb8_edge, 11 },
	{ "RETURN",       arb8_edge, 12 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  edge7_menu[] = {
	{ "ARB7 EDGES", (void (*)())NULL, 0 },
	{ "Move Edge 12", arb7_edge, 0 },
	{ "Move Edge 23", arb7_edge, 1 },
	{ "Move Edge 34", arb7_edge, 2 },
	{ "Move Edge 14", arb7_edge, 3 },
	{ "Move Edge 15", arb7_edge, 4 },
	{ "Move Edge 26", arb7_edge, 5 },
	{ "Move Edge 56", arb7_edge, 6 },
	{ "Move Edge 67", arb7_edge, 7 },
	{ "Move Edge 37", arb7_edge, 8 },
	{ "Move Edge 57", arb7_edge, 9 },
	{ "Move Edge 45", arb7_edge, 10 },
	{ "Move Point 5", arb7_edge, 11 },
	{ "RETURN",       arb7_edge, 12 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  edge6_menu[] = {
	{ "ARB6 EDGES", (void (*)())NULL, 0 },
	{ "Move Edge 12", arb6_edge, 0 },
	{ "Move Edge 23", arb6_edge, 1 },
	{ "Move Edge 34", arb6_edge, 2 },
	{ "Move Edge 14", arb6_edge, 3 },
	{ "Move Edge 15", arb6_edge, 4 },
	{ "Move Edge 25", arb6_edge, 5 },
	{ "Move Edge 36", arb6_edge, 6 },
	{ "Move Edge 46", arb6_edge, 7 },
	{ "Move Point 5", arb6_edge, 8 },
	{ "Move Point 6", arb6_edge, 9 },
	{ "RETURN",       arb6_edge, 10 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  edge5_menu[] = {
	{ "ARB5 EDGES", (void (*)())NULL, 0 },
	{ "Move Edge 12", arb5_edge, 0 },
	{ "Move Edge 23", arb5_edge, 1 },
	{ "Move Edge 34", arb5_edge, 2 },
	{ "Move Edge 14", arb5_edge, 3 },
	{ "Move Edge 15", arb5_edge, 4 },
	{ "Move Edge 25", arb5_edge, 5 },
	{ "Move Edge 35", arb5_edge, 6 },
	{ "Move Edge 45", arb5_edge, 7 },
	{ "Move Point 5", arb5_edge, 8 },
	{ "RETURN",       arb5_edge, 9 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  point4_menu[] = {
	{ "ARB4 POINTS", (void (*)())NULL, 0 },
	{ "Move Point 1", arb4_point, 0 },
	{ "Move Point 2", arb4_point, 1 },
	{ "Move Point 3", arb4_point, 2 },
	{ "Move Point 4", arb4_point, 4 },
	{ "RETURN",       arb4_point, 5 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  tgc_menu[] = {
	{ "TGC MENU", (void (*)())NULL, 0 },
	{ "Set H",	tgc_ed, MENU_TGC_SCALE_H },
	{ "Set H (move V)", tgc_ed, MENU_TGC_SCALE_H_V },
	{ "Set H (adj C,D)",	tgc_ed, MENU_TGC_SCALE_H_CD },
	{ "Set H (move V, adj A,B)", tgc_ed, MENU_TGC_SCALE_H_V_AB },
	{ "Set A",	tgc_ed, MENU_TGC_SCALE_A },
	{ "Set B",	tgc_ed, MENU_TGC_SCALE_B },
	{ "Set C",	tgc_ed, MENU_TGC_SCALE_C },
	{ "Set D",	tgc_ed, MENU_TGC_SCALE_D },
	{ "Set A,B",	tgc_ed, MENU_TGC_SCALE_AB },
	{ "Set C,D",	tgc_ed, MENU_TGC_SCALE_CD },
	{ "Set A,B,C,D", tgc_ed, MENU_TGC_SCALE_ABCD },
	{ "Rotate H",	tgc_ed, MENU_TGC_ROT_H },
	{ "Rotate AxB",	tgc_ed, MENU_TGC_ROT_AB },
	{ "Move End H(rt)", tgc_ed, MENU_TGC_MV_H },
	{ "Move End H", tgc_ed, MENU_TGC_MV_HH },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  tor_menu[] = {
	{ "TORUS MENU", (void (*)())NULL, 0 },
	{ "Set Radius 1", tor_ed, MENU_TOR_R1 },
	{ "Set Radius 2", tor_ed, MENU_TOR_R2 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  eto_menu[] = {
	{ "ELL-TORUS MENU", (void (*)())NULL, 0 },
	{ "Set r", eto_ed, MENU_ETO_R },
	{ "Set D", eto_ed, MENU_ETO_RD },
	{ "Set C", eto_ed, MENU_ETO_SCALE_C },
	{ "Rotate C", eto_ed, MENU_ETO_ROT_C },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  ell_menu[] = {
	{ "ELLIPSOID MENU", (void (*)())NULL, 0 },
	{ "Set A", ell_ed, MENU_ELL_SCALE_A },
	{ "Set B", ell_ed, MENU_ELL_SCALE_B },
	{ "Set C", ell_ed, MENU_ELL_SCALE_C },
	{ "Set A,B,C", ell_ed, MENU_ELL_SCALE_ABC },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  spline_menu[] = {
	{ "SPLINE MENU", (void (*)())NULL, 0 },
	{ "Pick Vertex", spline_ed, -1 },
	{ "Move Vertex", spline_ed, ECMD_VTRANS },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  nmg_menu[] = {
	{ "NMG MENU", (void (*)())NULL, 0 },
	{ "Pick Edge", nmg_ed, ECMD_NMG_EPICK },
	{ "Move Edge", nmg_ed, ECMD_NMG_EMOVE },
	{ "Split Edge", nmg_ed, ECMD_NMG_ESPLIT },
	{ "Delete Edge", nmg_ed, ECMD_NMG_EKILL },
	{ "Next EU", nmg_ed, ECMD_NMG_FORW },
	{ "Prev EU", nmg_ed, ECMD_NMG_BACK },
	{ "Radial EU", nmg_ed, ECMD_NMG_RADIAL },
	{ "Extrude Loop", nmg_ed , ECMD_NMG_LEXTRU },
	{ "Eebug Edge", nmg_ed, ECMD_NMG_EDEBUG },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv8_menu[] = {
	{ "ARB8 FACES", (void (*)())NULL, 0 },
	{ "Move Face 1234", arb8_mv_face, 1 },
	{ "Move Face 5678", arb8_mv_face, 2 },
	{ "Move Face 1584", arb8_mv_face, 3 },
	{ "Move Face 2376", arb8_mv_face, 4 },
	{ "Move Face 1265", arb8_mv_face, 5 },
	{ "Move Face 4378", arb8_mv_face, 6 },
	{ "RETURN",         arb8_mv_face, 7 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv7_menu[] = {
	{ "ARB7 FACES", (void (*)())NULL, 0 },
	{ "Move Face 1234", arb7_mv_face, 1 },
	{ "Move Face 2376", arb7_mv_face, 4 },
	{ "RETURN",         arb7_mv_face, 7 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv6_menu[] = {
	{ "ARB6 FACES", (void (*)())NULL, 0 },
	{ "Move Face 1234", arb6_mv_face, 1 },
	{ "Move Face 2365", arb6_mv_face, 2 },
	{ "Move Face 1564", arb6_mv_face, 3 },
	{ "Move Face 125" , arb6_mv_face, 4 },
	{ "Move Face 346" , arb6_mv_face, 5 },
	{ "RETURN",         arb6_mv_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv5_menu[] = {
	{ "ARB5 FACES", (void (*)())NULL, 0 },
	{ "Move Face 1234", arb5_mv_face, 1 },
	{ "Move Face 125" , arb5_mv_face, 2 },
	{ "Move Face 235" , arb5_mv_face, 3 },
	{ "Move Face 345" , arb5_mv_face, 4 },
	{ "Move Face 145" , arb5_mv_face, 5 },
	{ "RETURN",         arb5_mv_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv4_menu[] = {
	{ "ARB4 FACES", (void (*)())NULL, 0 },
	{ "Move Face 123" , arb4_mv_face, 1 },
	{ "Move Face 124" , arb4_mv_face, 2 },
	{ "Move Face 234" , arb4_mv_face, 3 },
	{ "Move Face 134" , arb4_mv_face, 4 },
	{ "RETURN",         arb4_mv_face, 5 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot8_menu[] = {
	{ "ARB8 FACES", (void (*)())NULL, 0 },
	{ "Rotate Face 1234", arb8_rot_face, 1 },
	{ "Rotate Face 5678", arb8_rot_face, 2 },
	{ "Rotate Face 1584", arb8_rot_face, 3 },
	{ "Rotate Face 2376", arb8_rot_face, 4 },
	{ "Rotate Face 1265", arb8_rot_face, 5 },
	{ "Rotate Face 4378", arb8_rot_face, 6 },
	{ "RETURN",         arb8_rot_face, 7 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot7_menu[] = {
	{ "ARB7 FACES", (void (*)())NULL, 0 },
	{ "Rotate Face 1234", arb7_rot_face, 1 },
	{ "Rotate Face 567" , arb7_rot_face, 2 },
	{ "Rotate Face 145" , arb7_rot_face, 3 },
	{ "Rotate Face 2376", arb7_rot_face, 4 },
	{ "Rotate Face 1265", arb7_rot_face, 5 },
	{ "Rotate Face 4375", arb7_rot_face, 6 },
	{ "RETURN",         arb7_rot_face, 7 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot6_menu[] = {
	{ "ARB6 FACES", (void (*)())NULL, 0 },
	{ "Rotate Face 1234", arb6_rot_face, 1 },
	{ "Rotate Face 2365", arb6_rot_face, 2 },
	{ "Rotate Face 1564", arb6_rot_face, 3 },
	{ "Rotate Face 125" , arb6_rot_face, 4 },
	{ "Rotate Face 346" , arb6_rot_face, 5 },
	{ "RETURN",         arb6_rot_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot5_menu[] = {
	{ "ARB5 FACES", (void (*)())NULL, 0 },
	{ "Rotate Face 1234", arb5_rot_face, 1 },
	{ "Rotate Face 125" , arb5_rot_face, 2 },
	{ "Rotate Face 235" , arb5_rot_face, 3 },
	{ "Rotate Face 345" , arb5_rot_face, 4 },
	{ "Rotate Face 145" , arb5_rot_face, 5 },
	{ "RETURN",         arb5_rot_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot4_menu[] = {
	{ "ARB4 FACES", (void (*)())NULL, 0 },
	{ "Rotate Face 123" , arb4_rot_face, 1 },
	{ "Rotate Face 124" , arb4_rot_face, 2 },
	{ "Rotate Face 234" , arb4_rot_face, 3 },
	{ "Rotate Face 134" , arb4_rot_face, 4 },
	{ "RETURN",         arb4_rot_face, 5 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item cntrl_menu[] = {
	{ "ARB MENU", (void (*)())NULL, 0 },
	{ "Move Edges", arb_control, MENU_ARB_MV_EDGE },
	{ "Move Faces", arb_control, MENU_ARB_MV_FACE },
	{ "Rotate Faces", arb_control, MENU_ARB_ROT_FACE },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item part_menu[] = {
	{ "Particle MENU", (void (*)())NULL, 0 },
	{ "Set H", part_ed, MENU_PART_H },
	{ "Set v", part_ed, MENU_PART_v },
	{ "Set h", part_ed, MENU_PART_h },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  rpc_menu[] = {
	{ "RPC MENU", (void (*)())NULL, 0 },
	{ "Set B", rpc_ed, MENU_RPC_B },
	{ "Set H", rpc_ed, MENU_RPC_H },
	{ "Set r", rpc_ed, MENU_RPC_R },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  rhc_menu[] = {
	{ "RHC MENU", (void (*)())NULL, 0 },
	{ "Set B", rhc_ed, MENU_RHC_B },
	{ "Set H", rhc_ed, MENU_RHC_H },
	{ "Set r", rhc_ed, MENU_RHC_R },
	{ "Set c", rhc_ed, MENU_RHC_C },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  epa_menu[] = {
	{ "EPA MENU", (void (*)())NULL, 0 },
	{ "Set H", epa_ed, MENU_EPA_H },
	{ "Set A", epa_ed, MENU_EPA_R1 },
	{ "Set B", epa_ed, MENU_EPA_R2 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  ehy_menu[] = {
	{ "EHY MENU", (void (*)())NULL, 0 },
	{ "Set H", ehy_ed, MENU_EHY_H },
	{ "Set A", ehy_ed, MENU_EHY_R1 },
	{ "Set B", ehy_ed, MENU_EHY_R2 },
	{ "Set c", ehy_ed, MENU_EHY_C },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item pipe_menu[] = {
	{ "PIPE MENU", (void (*)())NULL, 0 },
	{ "Select Point", pipe_ed, MENU_PIPE_SELECT },
	{ "Next Point", pipe_ed, MENU_PIPE_NEXT_PT },
	{ "Previous Point", pipe_ed, MENU_PIPE_PREV_PT },
	{ "Move Point", pipe_ed, MENU_PIPE_MOV_PT },
	{ "Delete Point", pipe_ed, MENU_PIPE_DEL_PT },
	{ "Append Point", pipe_ed, MENU_PIPE_ADD_PT },
	{ "Prepend Point", pipe_ed, MENU_PIPE_INS_PT },
	{ "Set Point OD", pipe_ed, MENU_PIPE_PT_OD },
	{ "Set Point ID", pipe_ed, MENU_PIPE_PT_ID },
	{ "Set Point Bend", pipe_ed, MENU_PIPE_PT_RADIUS },
	{ "Set Pipe OD", pipe_ed, MENU_PIPE_SCALE_OD },
	{ "Set Pipe ID", pipe_ed, MENU_PIPE_SCALE_ID },
	{ "Set Pipe Bend", pipe_ed, MENU_PIPE_SCALE_RADIUS },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item vol_menu[] = {
	{"VOL MENU", (void (*)())NULL, 0 },
	{"File Name", vol_ed, MENU_VOL_FNAME },
	{"File Size (X Y Z)", vol_ed, MENU_VOL_FSIZE },
	{"Voxel Size (X Y Z)", vol_ed, MENU_VOL_CSIZE },
	{"Threshold (low)", vol_ed, MENU_VOL_THRESH_LO },
	{"Threshold (hi)", vol_ed, MENU_VOL_THRESH_HI },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item ebm_menu[] = {
	{"EBM MENU", (void (*)())NULL, 0 },
	{"File Name", ebm_ed, MENU_EBM_FNAME },
	{"File Size (W N)", ebm_ed, MENU_EBM_FSIZE },
	{"Extrude Depth", ebm_ed, MENU_EBM_HEIGHT },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item dsp_menu[] = {
	{"DSP MENU", (void (*)())NULL, 0 },
	{"Name", dsp_ed, MENU_DSP_FNAME },
	{"Set X", dsp_ed, MENU_DSP_SCALE_X },
	{"Set Y", dsp_ed, MENU_DSP_SCALE_Y },
	{"Set ALT", dsp_ed, MENU_DSP_SCALE_ALT },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item bot_menu[] = {
	{ "BOT MENU", (void (*)())NULL, 0 },
	{ "Pick Vertex", bot_ed, ECMD_BOT_PICKV },
	{ "Pick Edge", bot_ed, ECMD_BOT_PICKE },
	{ "Pick Triangle", bot_ed, ECMD_BOT_PICKT },
	{ "Move Vertex", bot_ed, ECMD_BOT_MOVEV },
	{ "Move Edge", bot_ed, ECMD_BOT_MOVEE },
	{ "Move Triangle", bot_ed, ECMD_BOT_MOVET },
	{ "Delete Triangle", bot_ed, ECMD_BOT_FDEL },
	{ "Select Mode", bot_ed, ECMD_BOT_MODE },
	{ "Select Orientation", bot_ed, ECMD_BOT_ORIENT },
	{ "Set flags", bot_ed, ECMD_BOT_FLAGS },
	{ "Set Face Thickness", bot_ed, ECMD_BOT_THICK },
	{ "Set Face Mode", bot_ed, ECMD_BOT_FMODE },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  superell_menu[] = {
	{ "SUPERELLIPSOID MENU", (void (*)())NULL, 0 },
	{ "Set A", superell_ed, MENU_SUPERELL_SCALE_A },
	{ "Set B", superell_ed, MENU_SUPERELL_SCALE_B },
	{ "Set C", superell_ed, MENU_SUPERELL_SCALE_C },
	{ "Set A,B,C", superell_ed, MENU_SUPERELL_SCALE_ABC },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item *which_menu[] = {
	point4_menu,
	edge5_menu,
	edge6_menu,
	edge7_menu,
	edge8_menu,
	mv4_menu,
	mv5_menu,
	mv6_menu,
	mv7_menu,
	mv8_menu,
	rot4_menu,
	rot5_menu,
	rot6_menu,
	rot7_menu,
	rot8_menu
};

short int arb_vertices[5][24] = {
	{ 1,2,3,0, 1,2,4,0, 2,3,4,0, 1,3,4,0, 0,0,0,0, 0,0,0,0 },	/* arb4 */
	{ 1,2,3,4, 1,2,5,0, 2,3,5,0, 3,4,5,0, 1,4,5,0, 0,0,0,0 },	/* arb5 */
	{ 1,2,3,4, 2,3,6,5, 1,5,6,4, 1,2,5,0, 3,4,6,0, 0,0,0,0 },	/* arb6 */
	{ 1,2,3,4, 5,6,7,0, 1,4,5,0, 2,3,7,6, 1,2,6,5, 4,3,7,5 },	/* arb7 */
	{ 1,2,3,4, 5,6,7,8, 1,5,8,4, 2,3,7,6, 1,2,6,5, 4,3,7,8 }	/* arb8 */
};

static void
arb8_edge(int arg)
{
  es_menu = arg;
  es_edflag = EARB;
  if(arg == 12)  {
    es_edflag = ECMD_ARB_MAIN_MENU;
    sedit();
  }

  set_e_axes_pos(1);
}

static void
arb7_edge(int arg)
{
  es_menu = arg;
  es_edflag = EARB;
  if(arg == 11) {
    /* move point 5 */
    es_edflag = PTARB;
    es_menu = 4;	/* location of point */
  }
  if(arg == 12)  {
    es_edflag = ECMD_ARB_MAIN_MENU;
    sedit();
  }

  set_e_axes_pos(1);
}

static void
arb6_edge(int arg)
{
  es_menu = arg;
  es_edflag = EARB;
  if(arg == 8) {
    /* move point 5   location = 4 */
    es_edflag = PTARB;
    es_menu = 4;
  }
  if(arg == 9) {
    /* move point 6   location = 6 */
    es_edflag = PTARB;
    es_menu = 6;
  }
  if(arg == 10)  {
    es_edflag = ECMD_ARB_MAIN_MENU;
    sedit();
  }

  set_e_axes_pos(1);
}

static void
arb5_edge(int arg)
{
  es_menu = arg;
  es_edflag = EARB;
  if(arg == 8) {
    /* move point 5 at loaction 4 */
    es_edflag = PTARB;
    es_menu = 4;
  }
  if(arg == 9)  {
    es_edflag = ECMD_ARB_MAIN_MENU;
    sedit();
  }

  set_e_axes_pos(1);
}

static void
arb4_point(int arg)
{
  es_menu = arg;
  es_edflag = PTARB;
  if(arg == 5)  {
    es_edflag = ECMD_ARB_MAIN_MENU;
    sedit();
  }

  set_e_axes_pos(1);
}

static void
bot_ed(int arg)
{
	es_menu = arg;
	es_edflag = arg;

	sedit();
	set_e_axes_pos(1);
}

static void
ebm_ed(int arg)
{
  es_menu = arg;

  switch( arg ){
  case MENU_EBM_FNAME:
    es_edflag = ECMD_EBM_FNAME;
    break;
  case MENU_EBM_FSIZE:
    es_edflag = ECMD_EBM_FSIZE;
    break;
  case MENU_EBM_HEIGHT:
    es_edflag = ECMD_EBM_HEIGHT;
    break;
  }

  sedit();
  set_e_axes_pos(1);
}

static void
dsp_ed(int arg)
{
	es_menu = arg;

	switch( arg ) {
	case MENU_DSP_FNAME:
		es_edflag = ECMD_DSP_FNAME;
		break;
	case MENU_DSP_FSIZE:
		es_edflag = ECMD_DSP_FSIZE;
		break;
	case MENU_DSP_SCALE_X:
		es_edflag = ECMD_DSP_SCALE_X;
		break;
	case MENU_DSP_SCALE_Y:
		es_edflag = ECMD_DSP_SCALE_Y;
		break;
	case MENU_DSP_SCALE_ALT:
		es_edflag = ECMD_DSP_SCALE_ALT;
		break;
	}
	sedit();
	set_e_axes_pos(1);
}

static void
cline_ed(int arg)
{
	es_edflag = arg;
	sedit();
}

static void
vol_ed(int arg)
{
  es_menu = arg;

  switch( arg ){
  case MENU_VOL_FNAME:
    es_edflag = ECMD_VOL_FNAME;
    break;
  case MENU_VOL_FSIZE:
    es_edflag = ECMD_VOL_FSIZE;
    break;
  case MENU_VOL_CSIZE:
    es_edflag = ECMD_VOL_CSIZE;
    break;
  case MENU_VOL_THRESH_LO:
    es_edflag = ECMD_VOL_THRESH_LO;
    break;
  case MENU_VOL_THRESH_HI:
    es_edflag = ECMD_VOL_THRESH_HI;
    break;
  }

  sedit();
  set_e_axes_pos(1);
}

static void
pipe_ed(int arg)
{
	struct wdb_pipept *next;
	struct wdb_pipept *prev;

	if(dbip == DBI_NULL)
	  return;

	switch( arg )
	{
		case MENU_PIPE_SELECT:
			es_menu = arg;
			es_edflag = ECMD_PIPE_PICK;
		break;
		case MENU_PIPE_NEXT_PT:
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No Pipe Segment selected\n", (char *)NULL);
			  return;
			}
			next = BU_LIST_NEXT( wdb_pipept, &es_pipept->l );
			if( next->l.magic == BU_LIST_HEAD_MAGIC )
			{
			  Tcl_AppendResult(interp, "Current segment is the last\n", (char *)NULL);
			  return;
			}
			es_pipept = next;
			rt_pipept_print( es_pipept, base2local );
			es_menu = arg;
			es_edflag = IDLE;
			sedit();
		break;
		case MENU_PIPE_PREV_PT:
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No Pipe Segment selected\n", (char *)NULL);
			  return;
			}
			prev = BU_LIST_PREV( wdb_pipept, &es_pipept->l );
			if( prev->l.magic == BU_LIST_HEAD_MAGIC )
			{
			  Tcl_AppendResult(interp, "Current segment is the first\n", (char *)NULL);
			  return;
			}
			es_pipept = prev;
			rt_pipept_print( es_pipept, base2local );
			es_menu = arg;
			es_edflag = IDLE;
			sedit();
		break;
		case MENU_PIPE_SPLIT:
			/* not used */
		break;
		case MENU_PIPE_MOV_PT:
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No Pipe Segment selected\n", (char *)NULL);
			  es_edflag = IDLE;
			  return;
			}
			es_menu = arg;
			es_edflag = ECMD_PIPE_PT_MOVE;
		break;
		case MENU_PIPE_PT_OD:
		case MENU_PIPE_PT_ID:
		case MENU_PIPE_PT_RADIUS:
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No Pipe Segment selected\n", (char *)NULL);
			  es_edflag = IDLE;
			  return;
			}
			es_menu = arg;
			es_edflag = PSCALE;
		break;
		case MENU_PIPE_SCALE_OD:
		case MENU_PIPE_SCALE_ID:
		case MENU_PIPE_SCALE_RADIUS:
			es_menu = arg;
			es_edflag = PSCALE;
		break;
		case MENU_PIPE_ADD_PT:
			es_menu = arg;
			es_edflag = ECMD_PIPE_PT_ADD;
		break;
		case MENU_PIPE_INS_PT:
			es_menu = arg;
			es_edflag = ECMD_PIPE_PT_INS;
		break;
		case MENU_PIPE_DEL_PT:
			es_menu = arg;
			es_edflag = ECMD_PIPE_PT_DEL;
			sedit();
		break;
	}
	set_e_axes_pos(1);
}

static void
tgc_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;
	if(arg == MENU_TGC_ROT_H )
		es_edflag = ECMD_TGC_ROT_H;
	if(arg == MENU_TGC_ROT_AB)
		es_edflag = ECMD_TGC_ROT_AB;
	if(arg == MENU_TGC_MV_H)
		es_edflag = ECMD_TGC_MV_H;
	if(arg == MENU_TGC_MV_HH)
		es_edflag = ECMD_TGC_MV_HH;

	set_e_axes_pos(1);
}


static void
tor_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
eto_ed(int arg)
{
	es_menu = arg;
	if(arg == MENU_ETO_ROT_C )
		es_edflag = ECMD_ETO_ROT_C;
	else
		es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
rpc_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
part_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
rhc_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
epa_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
ehy_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
ell_ed(int arg)
{
	es_menu = arg;
	es_edflag = PSCALE;

	set_e_axes_pos(1);
}

static void
arb8_mv_face(int arg)
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 7)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedit();
	}

	set_e_axes_pos(1);
}

static void
arb7_mv_face(int arg)
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 7)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedit();
	}

	set_e_axes_pos(1);
}

static void
arb6_mv_face(int arg)
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 6)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedit();
	}

	set_e_axes_pos(1);
}

static void
arb5_mv_face(int arg)
{
  es_menu = arg - 1;
  es_edflag = ECMD_ARB_MOVE_FACE;
  if(arg == 6)  {
    es_edflag = ECMD_ARB_MAIN_MENU;
    sedit();
  }

  set_e_axes_pos(1);
}

static void
arb4_mv_face(int arg)
{
  es_menu = arg - 1;
  es_edflag = ECMD_ARB_MOVE_FACE;
  if(arg == 5)  {
    es_edflag = ECMD_ARB_MAIN_MENU;
    sedit();
  }

  set_e_axes_pos(1);
}

static void
arb8_rot_face(int arg)
{
  es_menu = arg - 1;
  es_edflag = ECMD_ARB_SETUP_ROTFACE;
  if(arg == 7)
    es_edflag = ECMD_ARB_MAIN_MENU;

  sedit();
}

static void
arb7_rot_face(int arg)
{
  es_menu = arg - 1;
  es_edflag = ECMD_ARB_SETUP_ROTFACE;
  if(arg == 7)
    es_edflag = ECMD_ARB_MAIN_MENU;

  sedit();
}

static void
arb6_rot_face(int arg)
{
  es_menu = arg - 1;
  es_edflag = ECMD_ARB_SETUP_ROTFACE;
  if(arg == 6)
    es_edflag = ECMD_ARB_MAIN_MENU;

  sedit();
}

static void
arb5_rot_face(int arg)
{
  es_menu = arg - 1;
  es_edflag = ECMD_ARB_SETUP_ROTFACE;
  if(arg == 6)
    es_edflag = ECMD_ARB_MAIN_MENU;

  sedit();
}

static void
arb4_rot_face(int arg)
{
  es_menu = arg - 1;
  es_edflag = ECMD_ARB_SETUP_ROTFACE;
  if(arg == 5)
    es_edflag = ECMD_ARB_MAIN_MENU;

  sedit();
}

static void
arb_control(int arg)
{
  es_menu = arg;
  es_edflag = ECMD_ARB_SPECIFIC_MENU;
  sedit();
}

/*ARGSUSED*/
static void
ars_ed(int arg)
{
  es_edflag = arg;
  sedit();
}

/*ARGSUSED*/
static void
extr_ed(int arg)
{
  es_edflag = arg;
  sedit();
}

static void superell_ed(int arg) {
  es_menu = arg;
  es_edflag = PSCALE;
  set_e_axes_pos(1);
  return;
}

/*ARGSUSED*/
static void
spline_ed(int arg)
{
  /* XXX Why wasn't this done by setting es_edflag = ECMD_SPLINE_VPICK? */
  if( arg < 0 )  {
    /* Enter picking state */
    chg_state( ST_S_EDIT, ST_S_VPICK, "Vertex Pick" );
    return;
  }
  /* For example, this will set es_edflag = ECMD_VTRANS */
  es_edflag = arg;
  sedit();

	set_e_axes_pos(1);
}
/*
 *			N M G _ E D
 *
 *  Handler for events in the NMG menu.
 *  Mostly just set appropriate state flags to prepare us for user's
 *  next event.
 */
/*ARGSUSED*/
static void
nmg_ed(int arg)
{
	switch(arg)  {
	default:
	  Tcl_AppendResult(interp, "nmg_ed: undefined menu event?\n", (char *)NULL);
	  return;
	case ECMD_NMG_EPICK:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_ESPLIT:
	case ECMD_NMG_EKILL:
		break;
	case ECMD_NMG_EDEBUG:
		if( !es_eu )  {
		  Tcl_AppendResult(interp, "nmg_ed: no edge selected yet\n", (char *)NULL);
		  return;
		}

		nmg_pr_fu_around_eu( es_eu, &mged_tol );
		{
			struct model		*m;
			struct rt_vlblock	*vbp;
			long			*tab;

			m = nmg_find_model( &es_eu->l.magic );
			NMG_CK_MODEL(m);

			if( *es_eu->g.magic_p == NMG_EDGE_G_LSEG_MAGIC )
			{
				/* get space for list of items processed */
				tab = (long *)bu_calloc( m->maxindex+1, sizeof(long),
					"nmg_ed tab[]");
				vbp = rt_vlblock_init();

				nmg_vlblock_around_eu(vbp, es_eu, tab, 1, &mged_tol);
				cvt_vlblock_to_solids( vbp, "_EU_", 0 );	/* swipe vlist */

				rt_vlblock_free(vbp);
				bu_free( (genptr_t)tab, "nmg_ed tab[]" );
			}
			view_state->vs_flag = 1;
		}
		if( *es_eu->up.magic_p == NMG_LOOPUSE_MAGIC )  {
			nmg_veu( &es_eu->up.lu_p->down_hd, es_eu->up.magic_p );
		}
		/* no change of state or es_edflag */
		return;
	case ECMD_NMG_FORW:
		if( !es_eu )  {
		  Tcl_AppendResult(interp, "nmg_ed: no edge selected yet\n", (char *)NULL);
		  return;
		}
		NMG_CK_EDGEUSE(es_eu);
		es_eu = BU_LIST_PNEXT_CIRC(edgeuse, es_eu);

		{
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "edgeuse selected=x%x (%g %g %g) <-> (%g %g %g)\n",
				es_eu, V3ARGS( es_eu->vu_p->v_p->vg_p->coord ),
				V3ARGS( es_eu->eumate_p->vu_p->v_p->vg_p->coord ) );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}

		sedit();
		return;
	case ECMD_NMG_BACK:
		if( !es_eu )  {
			bu_log("nmg_ed: no edge selected yet\n");
			return;
		}
		NMG_CK_EDGEUSE(es_eu);
		es_eu = BU_LIST_PPREV_CIRC(edgeuse, es_eu);

		{
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "edgeuse selected=x%x (%g %g %g) <-> (%g %g %g)\n",
				es_eu, V3ARGS( es_eu->vu_p->v_p->vg_p->coord ),
				V3ARGS( es_eu->eumate_p->vu_p->v_p->vg_p->coord ) );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}

		sedit();
		return;
	case ECMD_NMG_RADIAL:
		if( !es_eu )  {
			bu_log("nmg_ed: no edge selected yet\n");
			return;
		}
		NMG_CK_EDGEUSE(es_eu);
		es_eu = es_eu->eumate_p->radial_p;

		{
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "edgeuse selected=x%x (%g %g %g) <-> (%g %g %g)\n",
				es_eu, V3ARGS( es_eu->vu_p->v_p->vg_p->coord ),
				V3ARGS( es_eu->eumate_p->vu_p->v_p->vg_p->coord ) );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}

		sedit();
		return;
	case ECMD_NMG_LEXTRU:
		{
			struct model *m,*m_tmp;
			struct nmgregion *r,*r_tmp;
			struct shell *s,*s_tmp;
			struct loopuse *lu=(struct loopuse *)NULL;
			struct loopuse *lu_tmp;
			struct edgeuse *eu;
			fastf_t area;
			int wire_loop_count=0;

			m = (struct model *)es_int.idb_ptr;
			NMG_CK_MODEL( m );

			/* look for wire loops */
			for( BU_LIST_FOR( r , nmgregion , &m->r_hd ) )
			{
				NMG_CK_REGION( r );
				for( BU_LIST_FOR( s , shell , &r->s_hd ) )
				{
					if( BU_LIST_IS_EMPTY( &s->lu_hd ) )
						continue;

					for( BU_LIST_FOR( lu_tmp , loopuse , &s->lu_hd ) )
					{
						if( !lu )
							lu = lu_tmp;
						else if( lu_tmp == lu->lumate_p )
							continue;

						wire_loop_count++;
					}
				}
			}

			if( !wire_loop_count )
			{
			  Tcl_AppendResult(interp, "No sketch (wire loop) to extrude\n",
					   (char *)NULL);
			  return;
			}

			if( wire_loop_count > 1 )
			{
			  Tcl_AppendResult(interp, "Too many wire loops!!! Don't know which to extrude!!\n", (char *)NULL);
				return;
			}

			if( !lu || *lu->up.magic_p != NMG_SHELL_MAGIC )
			{
				/* This should never happen */
				bu_bomb( "Cannot find wire loop!!\n" );
			}

			/* Make sure loop is not a crack */
			area = nmg_loop_plane_area( lu , lu_pl );

			if( area < 0.0 )
			{
			  Tcl_AppendResult(interp, "Cannot extrude loop with no area\n",
					   (char *)NULL);
			  return;
			}

			/* Check if loop crosses itself */
			for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct edgeuse *eu2;
				struct vertex *v1;
				vect_t edge1;

				NMG_CK_EDGEUSE( eu );

				v1 = eu->vu_p->v_p;
				NMG_CK_VERTEX( v1 );
				VSUB2( edge1, eu->eumate_p->vu_p->v_p->vg_p->coord, v1->vg_p->coord );

				for( eu2 = BU_LIST_PNEXT( edgeuse , &eu->l ) ; BU_LIST_NOT_HEAD( eu2 , &lu->down_hd ) ; eu2=BU_LIST_PNEXT( edgeuse, &eu2->l) )
				{
					struct vertex *v2;
					vect_t edge2;
					fastf_t dist[2];
					int ret_val;

					NMG_CK_EDGEUSE( eu2 );

					if( eu2 == eu )
						continue;
					if( eu2 == BU_LIST_PNEXT_CIRC( edgeuse,  &eu->l ) )
						continue;
					if( eu2 == BU_LIST_PPREV_CIRC( edgeuse, &eu->l ) )
						continue;

					v2 = eu2->vu_p->v_p;
					NMG_CK_VERTEX( v2 );
					VSUB2( edge2, eu2->eumate_p->vu_p->v_p->vg_p->coord, v2->vg_p->coord );

					if( (ret_val=bn_isect_lseg3_lseg3( dist, v1->vg_p->coord, edge1,
						v2->vg_p->coord, edge2, &mged_tol )) > (-1) )
					{
					  struct bu_vls tmp_vls;

					  bu_vls_init(&tmp_vls);
					  bu_vls_printf(&tmp_vls,
							"Loop crosses itself, cannot extrude\n" );
					  bu_vls_printf(&tmp_vls,
							"edge1: pt=( %g %g %g ), dir=( %g %g %g)\n",
						  V3ARGS( v1->vg_p->coord ), V3ARGS( edge1 ) );
					  bu_vls_printf(&tmp_vls,
							"edge2: pt=( %g %g %g ), dir=( %g %g %g)\n",
						  V3ARGS( v2->vg_p->coord ), V3ARGS( edge2 ) );
					  if( ret_val == 0 )
					    bu_vls_printf(&tmp_vls,
							  "edges are collinear and overlap\n" );
					  else
					    {
					      point_t isect_pt;

					      VJOIN1( isect_pt, v1->vg_p->coord, dist[0], edge1 );
					      bu_vls_printf(&tmp_vls,
							    "edges intersect at ( %g %g %g )\n",
						      V3ARGS( isect_pt ) );
					    }

					  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls),
							   (char *)NULL);
					  bu_vls_free(&tmp_vls);
					  return;
					}
				}
			}

			/* Create a temporary model to store the basis loop */
			m_tmp = nmg_mm();
			r_tmp = nmg_mrsv( m_tmp );
			s_tmp = BU_LIST_FIRST( shell , &r_tmp->s_hd );
			lu_copy = nmg_dup_loop( lu , &s_tmp->l.magic , (long **)0 );
			if( !lu_copy )
			{
			  Tcl_AppendResult(interp, "Failed to make copy of loop\n", (char *)NULL);
			  nmg_km( m_tmp );
			  return;
			}

			/* Get the first vertex in the loop as the basis for extrusion */
			eu = BU_LIST_FIRST( edgeuse, &lu->down_hd );
			VMOVE( lu_keypoint , eu->vu_p->v_p->vg_p->coord );

			s = lu->up.s_p;

			if( BU_LIST_NON_EMPTY( &s->fu_hd ) )
			{
				/* make a new shell to hold the extruded solid */

				r = BU_LIST_FIRST( nmgregion , &m->r_hd );
				NMG_CK_REGION( r );
				es_s = nmg_msv( r );
			}
			else
				es_s = s;

		}
		break;
	}
	/* For example, this will set es_edflag = ECMD_NMG_EPICK */
	es_edflag = arg;
	sedit();
}

/*
 *  Keypoint in model space is established in "pt".
 *  If "str" is set, then that point is used, else default
 *  for this solid is selected and set.
 *  "str" may be a constant string, in either upper or lower case,
 *  or it may be something complex like "(3,4)" for an ARS or spline
 *  to select a particular vertex or control point.
 *
 *  XXX Perhaps this should be done via solid-specific parse tables,
 *  so that solids could be pretty-printed & structprint/structparse
 *  processed as well?
 */
void
get_solid_keypoint(fastf_t *pt, char **strp, struct rt_db_internal *ip, fastf_t *mat)
{
	char	*cp = *strp;
	point_t	mpt;
	static char		buf[128];

	RT_CK_DB_INTERNAL( ip );

	switch( ip->idb_type )  {
	case ID_CLINE:
		{
			struct rt_cline_internal *cli =
				(struct rt_cline_internal *)ip->idb_ptr;

			RT_CLINE_CK_MAGIC( cli );

			if( !strcmp( cp, "V" ) )
			{
				VMOVE( mpt, cli->v );
				*strp = "V";
			}
			else if( !strcmp( cp, "H" ) )
			{
				VADD2( mpt, cli->v, cli->h );
				*strp = "H";
			}
			else
			{
				VMOVE( mpt, cli->v );
				*strp = "V";
			}
			break;
		}
	case ID_PARTICLE:
		{
			struct rt_part_internal *part =
				(struct rt_part_internal *)ip->idb_ptr;

			RT_PART_CK_MAGIC( part );

			if( !strcmp( cp , "V" ) )
			{
				VMOVE( mpt , part->part_V );
				*strp = "V";
			}
			else if( !strcmp( cp , "H" ) )
			{
				VADD2( mpt , part->part_V , part->part_H );
				*strp = "H";
			}
			else	/* default */
			{
				VMOVE( mpt , part->part_V );
				*strp = "V";
			}
			break;
		}
	case ID_PIPE:
		{
			struct rt_pipe_internal *pipe =
				(struct rt_pipe_internal *)ip->idb_ptr;
			struct wdb_pipept *pipe_seg;

			RT_PIPE_CK_MAGIC( pipe );

			if (es_pipept == (struct wdb_pipept *)NULL) {
			  pipe_seg = BU_LIST_FIRST( wdb_pipept , &pipe->pipe_segs_head );
			  VMOVE( mpt , pipe_seg->pp_coord );
			} else {
			  VMOVE( mpt , es_pipept->pp_coord );
			}
			*strp = "V";
			break;
		}
	case ID_ARBN:
		{
			struct rt_arbn_internal *arbn =
				(struct rt_arbn_internal *)ip->idb_ptr;
			int i,j,k;
			int good_vert=0;

			RT_ARBN_CK_MAGIC( arbn );
			for( i=0 ; i<arbn->neqn ; i++ )
			{
				for( j=i+1 ; j<arbn->neqn ; j++ )
				{
					for( k=j+1 ; k<arbn->neqn ; k++ )
					{
						if( !bn_mkpoint_3planes( mpt , arbn->eqn[i] , arbn->eqn[j] , arbn->eqn[k] ) )
						{
							int l;

							good_vert = 1;
							for( l=0 ; l<arbn->neqn ; l++ )
							{
								if( l == i || l == j || l == k )
									continue;

								if( DIST_PT_PLANE( mpt , arbn->eqn[l] ) > mged_tol.dist )
								{
									good_vert = 0;
									break;
								}
							}

							if( good_vert )
								break;
						}
						if( good_vert )
							break;
					}
					if( good_vert )
						break;
				}
				if( good_vert )
					break;
			}

			*strp = "V";
			break;
		}
	case ID_EBM:
		{
			struct rt_ebm_internal *ebm =
				(struct rt_ebm_internal *)ip->idb_ptr;
			point_t pt;

			RT_EBM_CK_MAGIC( ebm );

			VSETALL( pt , 0.0 );
			MAT4X3PNT( mpt , ebm->mat , pt );
			*strp = "V";
			break;
		}
	case ID_BOT:
		{
			struct rt_bot_internal *bot =
				(struct rt_bot_internal *)ip->idb_ptr;

			if( bot_verts[0] > -1 )
				VMOVE( mpt, &bot->vertices[bot_verts[0]*3] )
			else
				VMOVE( mpt, bot->vertices )

			break;
		}
	case ID_DSP:
		{
			struct rt_dsp_internal *dsp =
				(struct rt_dsp_internal *)ip->idb_ptr;
			point_t pt;

			RT_DSP_CK_MAGIC( dsp );

			VSETALL( pt , 0.0 );
			MAT4X3PNT( mpt , dsp->dsp_stom , pt );
			*strp = "V";
			break;
		}
	case ID_HF:
		{
			struct rt_hf_internal *hf =
				(struct rt_hf_internal *)ip->idb_ptr;

			RT_HF_CK_MAGIC( hf );

			VMOVE( mpt, hf->v );
			*strp = "V";
			break;
		}
	case ID_VOL:
		{
			struct rt_vol_internal *vol =
				(struct rt_vol_internal *)ip->idb_ptr;
			point_t pt;

			RT_VOL_CK_MAGIC( vol );

			VSETALL( pt , 0.0 );
			MAT4X3PNT( mpt , vol->mat , pt );
			*strp = "V";
			break;
		}
	case ID_HALF:
		{
			struct rt_half_internal *haf =
				(struct rt_half_internal *)ip->idb_ptr;
			RT_HALF_CK_MAGIC( haf );

			VSCALE( mpt , haf->eqn , haf->eqn[H] );
			*strp = "V";
			break;
		}
	case ID_ARB8:
		{
			struct rt_arb_internal *arb =
				(struct rt_arb_internal *)ip->idb_ptr;
			RT_ARB_CK_MAGIC( arb );

			if( *cp == 'V' ) {
				int vertex_number;
				char *ptr;

				ptr = cp + 1;
				vertex_number = (*ptr) - '0';
				if( vertex_number < 1 || vertex_number > 8 )
					vertex_number = 1;
				VMOVE( mpt , arb->pt[vertex_number-1] );
				sprintf( buf, "V%d", vertex_number );
				*strp = buf;
				break;
			}

			/* Default */
			VMOVE( mpt , arb->pt[0] );
			*strp = "V1";

			break;
		}
	case ID_ELL:
	case ID_SPH:
		{
			struct rt_ell_internal	*ell =
				(struct rt_ell_internal *)ip->idb_ptr;
			RT_ELL_CK_MAGIC(ell);

			if( strcmp( cp, "V" ) == 0 )  {
				VMOVE( mpt, ell->v );
				*strp = "V";
				break;
			}
			if( strcmp( cp, "A" ) == 0 )  {
				VADD2( mpt , ell->v , ell->a );
				*strp = "A";
				break;
			}
			if( strcmp( cp, "B" ) == 0 )  {
				VADD2( mpt , ell->v , ell->b );
				*strp = "B";
				break;
			}
			if( strcmp( cp, "C" ) == 0 )  {
				VADD2( mpt , ell->v , ell->c );
				*strp = "C";
				break;
			}
			/* Default */
			VMOVE( mpt, ell->v );
			*strp = "V";
			break;
		}
	case ID_SUPERELL:
		{
			struct rt_superell_internal	*superell =
				(struct rt_superell_internal *)ip->idb_ptr;
			RT_SUPERELL_CK_MAGIC(superell);

			if( strcmp( cp, "V" ) == 0 )  {
				VMOVE( mpt, superell->v );
				*strp = "V";
				break;
			}
			if( strcmp( cp, "A" ) == 0 )  {
				VADD2( mpt , superell->v , superell->a );
				*strp = "A";
				break;
			}
			if( strcmp( cp, "B" ) == 0 )  {
				VADD2( mpt , superell->v , superell->b );
				*strp = "B";
				break;
			}
			if( strcmp( cp, "C" ) == 0 )  {
				VADD2( mpt , superell->v , superell->c );
				*strp = "C";
				break;
			}
			/* Default */
			VMOVE( mpt, superell->v );
			*strp = "V";
			break;
		}
	case ID_TOR:
		{
			struct rt_tor_internal	*tor =
				(struct rt_tor_internal *)ip->idb_ptr;
			RT_TOR_CK_MAGIC(tor);

			if( strcmp( cp, "V" ) == 0 )  {
				VMOVE( mpt, tor->v );
				*strp = "V";
				break;
			}
			/* Default */
			VMOVE( mpt, tor->v );
			*strp = "V";
			break;
		}
	case ID_TGC:
	case ID_REC:
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)ip->idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( strcmp( cp, "V" ) == 0 )  {
				VMOVE( mpt, tgc->v );
				*strp = "V";
				break;
			}
			if( strcmp( cp, "H" ) == 0 )  {
				VMOVE( mpt, tgc->h );
				*strp = "H";
				break;
			}
			if( strcmp( cp, "A" ) == 0 )  {
				VMOVE( mpt, tgc->a );
				*strp = "A";
				break;
			}
			if( strcmp( cp, "B" ) == 0 )  {
				VMOVE( mpt, tgc->b );
				*strp = "B";
				break;
			}
			if( strcmp( cp, "C" ) == 0 )  {
				VMOVE( mpt, tgc->c );
				*strp = "C";
				break;
			}
			if( strcmp( cp, "D" ) == 0 )  {
				VMOVE( mpt, tgc->d );
				*strp = "D";
				break;
			}
			/* Default */
			VMOVE( mpt, tgc->v );
			*strp = "V";
			break;
		}
	case ID_BSPLINE:
		{
			register struct rt_nurb_internal *sip =
				(struct rt_nurb_internal *) es_int.idb_ptr;
			register struct face_g_snurb	*surf;
			register fastf_t		*fp;

			RT_NURB_CK_MAGIC(sip);
			surf = sip->srfs[spl_surfno];
			NMG_CK_SNURB(surf);
			fp = &RT_NURB_GET_CONTROL_POINT( surf, spl_ui, spl_vi );
			VMOVE( mpt, fp );
			sprintf(buf, "Surf %d, index %d,%d",
				spl_surfno, spl_ui, spl_vi );
			*strp = buf;
			break;
		}
	case ID_GRIP:
		{
			struct rt_grip_internal *gip =
				(struct rt_grip_internal *)ip->idb_ptr;
			RT_GRIP_CK_MAGIC(gip);
			VMOVE( mpt, gip->center);
			*strp = "C";
			break;
		}
	case ID_ARS:
		{
			register struct rt_ars_internal *ars =
				(struct rt_ars_internal *)es_int.idb_ptr;
			RT_ARS_CK_MAGIC( ars );

			if (es_ars_crv < 0 || es_ars_col < 0) {
			  VMOVE( mpt, es_pt );
			} else {
			  VMOVE( mpt, &ars->curves[es_ars_crv][es_ars_col*3] );
			}

			*strp = "V";
			break;
		}
	case ID_RPC:
		{
			struct rt_rpc_internal *rpc =
				(struct rt_rpc_internal *)ip->idb_ptr;
			RT_RPC_CK_MAGIC( rpc );

			VMOVE( mpt , rpc->rpc_V );
			*strp = "V";
			break;
		}
	case ID_RHC:
		{
			struct rt_rhc_internal *rhc =
				(struct rt_rhc_internal *)ip->idb_ptr;
			RT_RHC_CK_MAGIC( rhc );

			VMOVE( mpt , rhc->rhc_V );
			*strp = "V";
			break;
		}
	case ID_EPA:
		{
			struct rt_epa_internal *epa =
				(struct rt_epa_internal *)ip->idb_ptr;
			RT_EPA_CK_MAGIC( epa );

			VMOVE( mpt , epa->epa_V );
			*strp = "V";
			break;
		}
	case ID_EHY:
		{
			struct rt_ehy_internal *ehy =
				(struct rt_ehy_internal *)ip->idb_ptr;
			RT_EHY_CK_MAGIC( ehy );

			VMOVE( mpt , ehy->ehy_V );
			*strp = "V";
			break;
		}
	case ID_ETO:
		{
			struct rt_eto_internal *eto =
				(struct rt_eto_internal *)ip->idb_ptr;
			RT_ETO_CK_MAGIC( eto );

			VMOVE( mpt , eto->eto_V );
			*strp = "V";
			break;
		}
	case ID_POLY:
		{
			struct rt_pg_face_internal *_poly;
			struct rt_pg_internal *pg =
				(struct rt_pg_internal *)ip->idb_ptr;
			RT_PG_CK_MAGIC( pg );

			_poly = pg->poly;
			VMOVE( mpt , _poly->verts );
			*strp = "V";
			break;
		}
	case ID_SKETCH:
		{
			struct rt_sketch_internal *skt =
				(struct rt_sketch_internal *)ip->idb_ptr;
			RT_SKETCH_CK_MAGIC( skt );

			VMOVE( mpt, skt->V );
			*strp = "V";
			break;
		}
	case ID_EXTRUDE:
		{
			struct rt_extrude_internal *extr =
				(struct rt_extrude_internal *)ip->idb_ptr;
			RT_EXTRUDE_CK_MAGIC( extr );

			if( extr->skt )
			{
				VJOIN2( mpt, extr->V, extr->skt->verts[0][0], extr->u_vec, extr->skt->verts[0][2], extr->v_vec );
				*strp = "V1";
			}
			else
			{
				VMOVE( mpt, extr->V );
				*strp = "V";
			}
			break;
		}
	case ID_NMG:
		{
			struct vertex *v;
			struct vertexuse *vu;
			struct edgeuse *eu;
			struct loopuse *lu;
			struct faceuse *fu;
			struct shell *s;
			struct nmgregion *r;
			register struct model *m =
				(struct model *) es_int.idb_ptr;
			NMG_CK_MODEL(m);
			/* XXX Fall through, for now (How about first vertex?? - JRA) */

			/* set default first */
			VSETALL( mpt, 0.0 );
			*strp = "(origin)";

#if 1
			/* XXX Try to use the first point of the selected edge */
			if (es_eu != (struct edgeuse *)NULL &&
			    es_eu->vu_p != (struct vertexuse *)NULL &&
			    es_eu->vu_p->v_p != (struct vertex *)NULL &&
			    es_eu->vu_p->v_p->vg_p != (struct vertex_g *)NULL) {
			  VMOVE(mpt, es_eu->vu_p->v_p->vg_p->coord);
			  break;
			}
#endif

			if( BU_LIST_IS_EMPTY( &m->r_hd ) )
				break;

			r = BU_LIST_FIRST( nmgregion , &m->r_hd );
			if( !r )
				break;
			NMG_CK_REGION( r );

			if( BU_LIST_IS_EMPTY( &r->s_hd ) )
				break;

			s = BU_LIST_FIRST( shell , &r->s_hd );
			if( !s )
				break;
			NMG_CK_SHELL( s );

			if( BU_LIST_IS_EMPTY( &s->fu_hd ) )
				fu = (struct faceuse *)NULL;
			else
				fu = BU_LIST_FIRST( faceuse , &s->fu_hd );
			if( fu )
			{
				NMG_CK_FACEUSE( fu );
				lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
				NMG_CK_LOOPUSE( lu );
				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC )
				{
					eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
					NMG_CK_EDGEUSE( eu );
					NMG_CK_VERTEXUSE( eu->vu_p );
					v = eu->vu_p->v_p;
				}
				else
				{
					vu = BU_LIST_FIRST( vertexuse , &lu->down_hd );
					NMG_CK_VERTEXUSE( vu );
					v = vu->v_p;
				}
				NMG_CK_VERTEX( v );
				if( !v->vg_p )
					break;
				VMOVE( mpt , v->vg_p->coord );
				*strp = "V";
				break;
			}
			if( BU_LIST_IS_EMPTY( &s->lu_hd ) )
				lu = (struct loopuse *)NULL;
			else
				lu = BU_LIST_FIRST( loopuse , &s->lu_hd );
			if( lu )
			{
				NMG_CK_LOOPUSE( lu );
				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC )
				{
					eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
					NMG_CK_EDGEUSE( eu );
					NMG_CK_VERTEXUSE( eu->vu_p );
					v = eu->vu_p->v_p;
				}
				else
				{
					vu = BU_LIST_FIRST( vertexuse , &lu->down_hd );
					NMG_CK_VERTEXUSE( vu );
					v = vu->v_p;
				}
				NMG_CK_VERTEX( v );
				if( !v->vg_p )
					break;
				VMOVE( mpt , v->vg_p->coord );
				*strp = "V";
				break;
			}
			if( BU_LIST_IS_EMPTY( &s->eu_hd ) )
				eu = (struct edgeuse *)NULL;
			else
				eu = BU_LIST_FIRST( edgeuse , &s->eu_hd );
			if( eu )
			{
				NMG_CK_EDGEUSE( eu );
				NMG_CK_VERTEXUSE( eu->vu_p );
				v = eu->vu_p->v_p;
				NMG_CK_VERTEX( v );
				if( !v->vg_p )
					break;
				VMOVE( mpt , v->vg_p->coord );
				*strp = "V";
				break;
			}
			vu = s->vu_p;
			if( vu )
			{
				NMG_CK_VERTEXUSE( vu );
				v = vu->v_p;
				NMG_CK_VERTEX( v );
				if( !v->vg_p )
					break;
				VMOVE( mpt , v->vg_p->coord );
				*strp = "V";
				break;
			}
		}
	default:
	  Tcl_AppendResult(interp, "get_solid_keypoint: unrecognized solid type (setting keypoint to origin)\n", (char *)NULL);
	  VSETALL( mpt, 0.0 );
	  *strp = "(origin)";
	  break;
	}
	MAT4X3PNT( pt, mat, mpt );
}

int
f_get_solid_keypoint(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	if( state == ST_VIEW || state == ST_S_PICK || state == ST_O_PICK )
		return TCL_OK;

	get_solid_keypoint( es_keypoint, &es_keytag, &es_int, es_mat );
	return TCL_OK;
}

void
set_e_axes_pos(int both)
             /* if(!both) then set only curr_e_axes_pos, otherwise
	      set e_axes_pos and curr_e_axes_pos */
{
	int	i;
	register struct dm_list *dmlp;

	update_views = 1;
#if 0
	VMOVE(curr_e_axes_pos, es_keypoint);
#else
	switch (es_int.idb_type) {
	case	ID_ARB8:
		if (state == ST_O_EDIT)
			i = 0;
		else
			switch (es_edflag) {
			case STRANS:
				i = 0;
				break;
			case EARB:
				switch (es_type) {
				case ARB5:
					i = earb5[es_menu][0];
					break;
				case ARB6:
					i = earb6[es_menu][0];
					break;
				case ARB7:
					i = earb7[es_menu][0];
					break;
				case ARB8:
					i = earb8[es_menu][0];
					break;
				default:
					i = 0;
					break;
				}
				break;
			case PTARB:
				switch (es_type) {
				case ARB4:
					i = es_menu;	/* index for point 1,2,3 or 4 */
					break;
				case ARB5:
				case ARB7:
					i = 4;	/* index for point 5 */
					break;
				case ARB6:
					i = es_menu;	/* index for point 5 or 6 */
					break;
				default:
					i = 0;
					break;
				}
				break;
			case ECMD_ARB_MOVE_FACE:
				switch (es_type) {
				case ARB4:
					i = rt_arb_faces[0][es_menu * 4];
					break;
				case ARB5:
					i = rt_arb_faces[1][es_menu * 4];
					break;
				case ARB6:
					i = rt_arb_faces[2][es_menu * 4];
					break;
				case ARB7:
					i = rt_arb_faces[3][es_menu * 4];
					break;
				case ARB8:
					i = rt_arb_faces[4][es_menu * 4];
					break;
				default:
					i = 0;
					break;
				}
				break;
			case ECMD_ARB_ROTATE_FACE:
				i = fixv;
				break;
			default:
				i = 0;
				break;
			}

		MAT4X3PNT(curr_e_axes_pos, es_mat,
			  ((struct rt_arb_internal *)es_int.idb_ptr)->pt[i]);
		break;
	case ID_TGC:
	case ID_REC:
		if (es_edflag == ECMD_TGC_MV_H ||
		    es_edflag == ECMD_TGC_MV_HH) {
			struct rt_tgc_internal  *tgc = (struct rt_tgc_internal *)es_int.idb_ptr;
			point_t tgc_v;
			vect_t tgc_h;

			MAT4X3PNT(tgc_v, es_mat, tgc->v);
			MAT4X3VEC(tgc_h, es_mat, tgc->h);
			VADD2(curr_e_axes_pos, tgc_h, tgc_v);
		} else
			VMOVE(curr_e_axes_pos, es_keypoint)

				break;
	case ID_EXTRUDE:
		if (es_edflag == ECMD_EXTR_MOV_H) {
			struct rt_extrude_internal *extr = (struct rt_extrude_internal *)es_int.idb_ptr;
			point_t extr_v;
			vect_t extr_h;

			RT_EXTRUDE_CK_MAGIC(extr);

			MAT4X3PNT(extr_v, es_mat, extr->V);
			MAT4X3VEC(extr_h, es_mat, extr->h);
			VADD2(curr_e_axes_pos, extr_h, extr_v);
		} else
			VMOVE(curr_e_axes_pos, es_keypoint)

				break;
	case ID_CLINE:
		if (es_edflag == ECMD_CLINE_MOVE_H) {
			struct rt_cline_internal *cli =
				(struct rt_cline_internal *)es_int.idb_ptr;
			point_t cli_v;
			vect_t cli_h;

			RT_CLINE_CK_MAGIC(cli);

			MAT4X3PNT(cli_v, es_mat, cli->v);
			MAT4X3VEC(cli_h, es_mat, cli->h);
			VADD2(curr_e_axes_pos, cli_h, cli_v);
		} else
			VMOVE(curr_e_axes_pos, es_keypoint)
				break;
	default:
		VMOVE(curr_e_axes_pos, es_keypoint);
		break;
	}
#endif

	if (both) {
		VMOVE(e_axes_pos, curr_e_axes_pos);

		if (EDIT_ROTATE) {
			es_edclass = EDIT_CLASS_ROTATE;
			VSETALL( edit_absolute_model_rotate, 0.0 );
			VSETALL( edit_absolute_object_rotate, 0.0 );
			VSETALL( edit_absolute_view_rotate, 0.0 );
			VSETALL( last_edit_absolute_model_rotate, 0.0 );
			VSETALL( last_edit_absolute_object_rotate, 0.0 );
			VSETALL( last_edit_absolute_view_rotate, 0.0 );
		} else if (EDIT_TRAN) {
			es_edclass = EDIT_CLASS_TRAN;
			VSETALL( edit_absolute_model_tran, 0.0 );
			VSETALL( edit_absolute_view_tran, 0.0 );
			VSETALL( last_edit_absolute_model_tran, 0.0 );
			VSETALL( last_edit_absolute_view_tran, 0.0 );
		} else if (EDIT_SCALE) {
			es_edclass = EDIT_CLASS_SCALE;

			if (SEDIT_SCALE) {
				edit_absolute_scale = 0.0;
				acc_sc_sol = 1.0;
			}
		} else
			es_edclass = EDIT_CLASS_NULL;

#if 1
		MAT_IDN(acc_rot_sol);
#endif

		FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
			dmlp->dml_mged_variables->mv_transform = 'e';
	}
}

/*
 *			I N I T _ S E D I T
 *
 *  First time in for this solid, set things up.
 *  If all goes well, change state to ST_S_EDIT.
 *  Solid editing is completed only via sedit_accept() / sedit_reject().
 */
void
init_sedit(void)
{
	register int		type;
	int			id;

	if(dbip == DBI_NULL)
	  return;

	/*
	 * Check for a processed region or other illegal solid.
	 */
	if( illump->s_Eflag )  {
	  Tcl_AppendResult(interp,
"Unable to Solid_Edit a processed region;  select a primitive instead\n", (char *)NULL);
	  return;
	}

	/* Read solid description into es_int */
	if( rt_db_get_internal( &es_int, LAST_SOLID(illump),
	  dbip, NULL, &rt_uniresource ) < 0 )  {
	  Tcl_AppendResult(interp, "init_sedit(",
	  		LAST_SOLID(illump)->d_namep,
			   "):  solid import failure\n", (char *)NULL);
	  rt_db_free_internal( &es_int, &rt_uniresource );
	  return;				/* FAIL */
	}
	RT_CK_DB_INTERNAL( &es_int );
	id = es_int.idb_type;

	es_menu = 0;
	if( id == ID_ARB8 )
	{
		struct rt_arb_internal *arb;

		arb = (struct rt_arb_internal *)es_int.idb_ptr;
		RT_ARB_CK_MAGIC( arb );

		type = rt_arb_std_type( &es_int , &mged_tol );
		es_type = type;

		if( rt_arb_calc_planes( es_peqn , arb , es_type , &mged_tol ) )
		{
		  Tcl_AppendResult(interp,"Cannot calculate plane equations for ARB8\n",
				   (char *)NULL);
		  rt_db_free_internal( &es_int, &rt_uniresource );
		  return;
		}
	}
	else if( id == ID_BSPLINE )
	{
		register struct rt_nurb_internal *sip =
			(struct rt_nurb_internal *) es_int.idb_ptr;
		register struct face_g_snurb	*surf;
		RT_NURB_CK_MAGIC(sip);
		spl_surfno = sip->nsrf/2;
		surf = sip->srfs[spl_surfno];
		NMG_CK_SNURB(surf);
		spl_ui = surf->s_size[1]/2;
		spl_vi = surf->s_size[0]/2;
	}

	/* Save aggregate path matrix */
	pathHmat( illump, es_mat, illump->s_fullpath.fp_len-2 );

	/* get the inverse matrix */
	bn_mat_inv( es_invmat, es_mat );

	/* Establish initial keypoint */
	es_keytag = "";
	get_solid_keypoint( es_keypoint, &es_keytag, &es_int, es_mat );

	es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
	es_pipept = (struct wdb_pipept *)NULL; /* Reset es_pipept */
	lu_copy = (struct loopuse *)NULL;
	es_ars_crv = (-1);
	es_ars_col = (-1);

	bot_verts[0] = -1;
	bot_verts[1] = -1;
	bot_verts[2] = -1;

	/* Finally, enter solid edit state */
	(void)chg_state( ST_S_PICK, ST_S_EDIT, "Keyboard illuminate");
	chg_l2menu(ST_S_EDIT);
	es_edflag = IDLE;

	button( BE_S_EDIT );	/* Drop into edit menu right away */
	init_sedit_vars();

	{
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_strcpy(&vls, "begin_edit_callback ");
		db_path_to_vls(&vls, &illump->s_fullpath);
		(void)Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
	}
}

static void
init_sedit_vars(void)
{
	MAT_IDN(acc_rot_sol);
	MAT_IDN(incr_change);

	VSETALL(edit_absolute_model_rotate, 0.0);
	VSETALL(edit_absolute_object_rotate, 0.0);
	VSETALL(edit_absolute_view_rotate, 0.0);
	VSETALL(last_edit_absolute_model_rotate, 0.0);
	VSETALL(last_edit_absolute_object_rotate, 0.0);
	VSETALL(last_edit_absolute_view_rotate, 0.0);
	VSETALL(edit_absolute_model_tran, 0.0);
	VSETALL(edit_absolute_view_tran, 0.0);
	VSETALL(last_edit_absolute_model_tran, 0.0);
	VSETALL(last_edit_absolute_view_tran, 0.0);
	edit_absolute_scale = 0.0;
	acc_sc_sol = 1.0;

	VSETALL(edit_rate_model_rotate, 0.0);
	VSETALL(edit_rate_object_rotate, 0.0);
	VSETALL(edit_rate_view_rotate, 0.0);
	VSETALL(edit_rate_model_tran, 0.0);
	VSETALL(edit_rate_view_tran, 0.0);

	set_e_axes_pos(1);
}

/*
 *			R E P L O T _ E D I T I N G _ S O L I D
 *
 *  All solid edit routines call this subroutine after
 *  making a change to es_int or es_mat.
 */
void
replot_editing_solid(void)
{
  mat_t mat;
  register struct solid *sp;
  struct directory *illdp = LAST_SOLID(illump);

  FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
    if(LAST_SOLID(sp) == illdp )  {
      pathHmat( sp, mat, sp->s_fullpath.fp_len-2 );
      (void)replot_modified_solid( sp, &es_int, mat );
    }
  }
}

/*
 *			T R A N S F O R M _ E D I T I N G _ S O L I D
 *
 */
void
transform_editing_solid(
	struct rt_db_internal	*os,		/* output solid */
	const mat_t		mat,
	struct rt_db_internal	*is,		/* input solid */
	int			free)
{
	RT_CK_DB_INTERNAL( is );
	if( rt_functab[is->idb_type].ft_xform( os, mat, is, free, dbip, &rt_uniresource ) < 0 )
		bu_bomb("transform_editing_solid");
}

/*
 *			S E D I T _ M E N U
 *
 *
 *  Put up menu header
 */
void
sedit_menu(void) {

	menu_state->ms_flag = 0;		/* No menu item selected yet */

	mmenu_set_all( MENU_L1, MENU_NULL );
	chg_l2menu(ST_S_EDIT);

	switch( es_int.idb_type ) {

	case ID_ARB8:
		mmenu_set_all( MENU_L1, cntrl_menu );
		break;
	case ID_TGC:
		mmenu_set_all( MENU_L1, tgc_menu );
		break;
	case ID_TOR:
		mmenu_set_all( MENU_L1, tor_menu );
		break;
	case ID_ELL:
		mmenu_set_all( MENU_L1, ell_menu );
		break;
	case ID_SUPERELL:
		mmenu_set_all( MENU_L1, superell_menu );
		break;
	case ID_ARS:
		mmenu_set_all( MENU_L1, ars_menu );
		break;
	case ID_BSPLINE:
		mmenu_set_all( MENU_L1, spline_menu );
		break;
	case ID_RPC:
		mmenu_set_all( MENU_L1, rpc_menu );
		break;
	case ID_RHC:
		mmenu_set_all( MENU_L1, rhc_menu );
		break;
	case ID_EPA:
		mmenu_set_all( MENU_L1, epa_menu );
		break;
	case ID_EHY:
		mmenu_set_all( MENU_L1, ehy_menu );
		break;
	case ID_ETO:
		mmenu_set_all( MENU_L1, eto_menu );
		break;
	case ID_NMG:
		mmenu_set_all( MENU_L1, nmg_menu );
		break;
	case ID_PIPE:
		mmenu_set_all( MENU_L1, pipe_menu );
		break;
	case ID_VOL:
		mmenu_set_all( MENU_L1, vol_menu );
		break;
	case ID_EBM:
		mmenu_set_all( MENU_L1, ebm_menu );
		break;
	case ID_DSP:
		mmenu_set_all( MENU_L1, dsp_menu );
		break;
	case ID_PARTICLE:
		mmenu_set_all( MENU_L1, part_menu );
		break;
	case ID_BOT:
		mmenu_set_all( MENU_L1, bot_menu );
		break;
	case ID_EXTRUDE:
		mmenu_set_all( MENU_L1, extr_menu );
		break;
	case ID_CLINE:
		mmenu_set_all( MENU_L1, cline_menu );
		break;
	}
	es_edflag = IDLE;	/* Drop out of previous edit mode */
	es_menu = 0;
}

int
get_rotation_vertex(void)
{
  int i, j;
  int type, loc, valid;
  int fixv = -1;
  struct bu_vls str;
  struct bu_vls cmd;

  type = es_type - 4;

  loc = es_menu*4;
  valid = 0;
  bu_vls_init(&str);
  bu_vls_init(&cmd);

  bu_vls_printf(&str, "Enter fixed vertex number( ");
  for(i=0; i<4; i++){
    if( arb_vertices[type][loc+i] )
      bu_vls_printf(&str, "%d ", arb_vertices[type][loc+i]);
  }
  bu_vls_printf(&str, ") [%d]: ",arb_vertices[type][loc]);

  bu_vls_printf(&cmd, "cad_input_dialog .get_vertex %S {Need vertex for solid rotate}\
 {%s} vertex_num %d 0 {{ summary \"Enter a vertex number to rotate about.\"}} OK",
		&dName, bu_vls_addr(&str), arb_vertices[type][loc]);

  while(!valid){
    if(Tcl_Eval(interp, bu_vls_addr(&cmd)) != TCL_OK){
      Tcl_AppendResult(interp, "get_rotation_vertex: Error reading vertex\n", (char *)NULL);
      /* Using default */
      return arb_vertices[type][loc];
    }

    fixv = atoi(Tcl_GetVar(interp, "vertex_num", TCL_GLOBAL_ONLY));
    for(j=0; j<4; j++)  {
      if( fixv==arb_vertices[type][loc+j] )
	valid=1;
    }
  }

  bu_vls_free(&str);
  return fixv;
}

char *
get_file_name(char *str)
{
	struct bu_vls cmd;
	struct bu_vls varname_vls;
	char *dir;
	char *fptr;
	char *ptr1;
	char *ptr2;

	bu_vls_init( &cmd );
	bu_vls_init( &varname_vls );
	bu_vls_strcpy( &varname_vls, "mged_gui(getFileDir)" );

	if( (fptr=strrchr( str, '/')))
	{
		dir = (char *)bu_malloc( (strlen(str)+1)*sizeof( char ), "get_file_name: dir" );
		ptr1 = str;
		ptr2 = dir;
		while( ptr1 != fptr )
			*ptr2++ = *ptr1++;
		*ptr2 = '\0';
		Tcl_SetVar(interp, bu_vls_addr(&varname_vls), dir, TCL_GLOBAL_ONLY);
		bu_free((genptr_t)dir, "get_file_name: directory string");
	}

	bu_vls_printf(&cmd,
		      "getFile %s %s {{{All Files} {*}}} {Get File}",
		      bu_vls_addr(&pathName),
		      bu_vls_addr(&varname_vls));
	bu_vls_free(&varname_vls);

	if( Tcl_Eval( interp, bu_vls_addr( &cmd ) ) )
	{
		bu_vls_free( &cmd );
		return( (char *)NULL );
	}
	else if( interp->result[0] != '\0' )
	{
		bu_vls_free( &cmd );
		return( interp->result );
	}
	else
	{
		bu_vls_free( &cmd );
		return( (char *)NULL );
	}
}

static void
dsp_scale(struct rt_dsp_internal *dsp, int idx)
{
	mat_t m, scalemat;

	RT_DSP_CK_MAGIC(dsp);

	MAT_IDN(m);

	if (es_mvalid) {
		bu_log("es_mvalid %g %g %g\n", V3ARGS(es_mparam));
	}

	if (inpara > 0) {
		m[idx] = es_para[0];
		bu_log("Keyboard %g\n", es_para[0]);
	} else if (es_scale != 0.0) {
		m[idx] *= es_scale;
		bu_log("es_scale %g\n", es_scale);
		es_scale = 0.0;
	}

	bn_mat_xform_about_pt(scalemat, m, es_keypoint);

	bn_mat_mul(m, dsp->dsp_stom, scalemat);
	MAT_COPY(dsp->dsp_stom, m);

	bn_mat_mul(m, scalemat, dsp->dsp_mtos);
	MAT_COPY(dsp->dsp_mtos, m);

}
/*
 * 			S E D I T
 *
 * A great deal of magic takes place here, to accomplish solid editing.
 *
 *  Called from mged main loop after any event handlers:
 *		if( sedraw > 0 )  sedit();
 *  to process any residual events that the event handlers were too
 *  lazy to handle themselves.
 *
 *  A lot of processing is deferred to here, so that the "p" command
 *  can operate on an equal footing to mouse events.
 */
void
sedit(void)
{
	struct rt_arb_internal *arb;
	fastf_t	*eqp;
	static vect_t work;
	register int i;
	static int pnt5;		/* ECMD_ARB_SETUP_ROTFACE, special arb7 case */
	static float la, lb, lc, ld;	/* TGC: length of vectors */
	mat_t	mat;
	mat_t	mat1;
	mat_t	edit;
	point_t rot_point;

	if(dbip == DBI_NULL)
	  return;

	sedraw = 0;
	++update_views;

	switch( es_edflag ) {

	case IDLE:
	  /* do nothing more */
	  --update_views;
	  break;

	case ECMD_DSP_SCALE_X:
		dsp_scale( (struct rt_dsp_internal *)es_int.idb_ptr, MSX);
		break;
	case ECMD_DSP_SCALE_Y:
		dsp_scale( (struct rt_dsp_internal *)es_int.idb_ptr, MSY);
		break;
	case ECMD_DSP_SCALE_ALT:
		dsp_scale( (struct rt_dsp_internal *)es_int.idb_ptr, MSZ);
		break;
	case ECMD_DSP_FNAME:
		{
			struct rt_dsp_internal *dsp =
				(struct rt_dsp_internal *)es_int.idb_ptr;
			char *fname;
			struct stat stat_buf;
			off_t need_size;
			struct bu_vls message;

			RT_DSP_CK_MAGIC( dsp );

			/* Pop-up the Tk file browser */
			fname = get_file_name( bu_vls_addr(&dsp->dsp_name) );
			if ( ! fname) break;

			if( stat( fname, &stat_buf ) ) {
				bu_vls_init( &message );
				bu_vls_printf( &message, "Cannot get status of file %s\n", fname );
				Tcl_SetResult(interp, bu_vls_addr( &message ), TCL_VOLATILE );
				bu_vls_free( &message );
				mged_print_result( TCL_ERROR );
				return;
			}

			need_size = dsp->dsp_xcnt * dsp->dsp_ycnt * 2;
			if (stat_buf.st_size < need_size) {
				bu_vls_init( &message );
				bu_vls_printf( &message, "File (%s) is too small, adjust the file size parameters first", fname);
				Tcl_SetResult(interp, bu_vls_addr( &message ), TCL_VOLATILE);
				bu_vls_free( &message );
				mged_print_result( TCL_ERROR );
				return;
			}
			bu_vls_strcpy( &dsp->dsp_name, fname );

			break;
		}

	case ECMD_EBM_FSIZE:	/* set file size */
		{
			struct rt_ebm_internal *ebm =
				(struct rt_ebm_internal *)es_int.idb_ptr;
			struct stat stat_buf;
			off_t need_size;

			RT_EBM_CK_MAGIC( ebm );

			if( inpara == 2 )
			{
				if( stat( ebm->file, &stat_buf ) )
				{
					Tcl_AppendResult(interp, "Cannot get status of file ", ebm->file, (char *)NULL );
					mged_print_result( TCL_ERROR );
					return;
				}
				need_size = es_para[0] * es_para[1] * sizeof( unsigned char );
				if( stat_buf.st_size < need_size )
				{
					Tcl_AppendResult(interp, "File (", ebm->file,
						") is too small, set file name first", (char *)NULL );
					mged_print_result( TCL_ERROR );
					return;
				}
				ebm->xdim = es_para[0];
				ebm->ydim = es_para[1];
			}
			else if( inpara > 0 )
			{
				Tcl_AppendResult(interp, "width and length of file are required\n", (char *)NULL );
				mged_print_result( TCL_ERROR );
				return;
			}
		}
		break;

	case ECMD_EBM_FNAME:
		{
			struct rt_ebm_internal *ebm =
				(struct rt_ebm_internal *)es_int.idb_ptr;
			char *fname;
			struct stat stat_buf;
			off_t need_size;

			RT_EBM_CK_MAGIC( ebm );

			fname = get_file_name( ebm->file );
			if( fname )
			{
				struct bu_vls message;

				if( stat( fname, &stat_buf ) )
				{
					bu_vls_init( &message );
					bu_vls_printf( &message, "Cannot get status of file %s\n", fname );
					Tcl_SetResult(interp, bu_vls_addr( &message ), TCL_VOLATILE );
					bu_vls_free( &message );
					mged_print_result( TCL_ERROR );
					return;
				}
				need_size = ebm->xdim * ebm->ydim * sizeof( unsigned char );
				if( stat_buf.st_size < need_size )
				{
					bu_vls_init( &message );
					bu_vls_printf( &message, "File (%s) is too small, adjust the file size parameters first", fname);
					Tcl_SetResult(interp, bu_vls_addr( &message ), TCL_VOLATILE);
					bu_vls_free( &message );
					mged_print_result( TCL_ERROR );
					return;
				}
				strcpy( ebm->file, fname );
			}

			break;
		}

	case ECMD_EBM_HEIGHT:	/* set extrusion depth */
		{
			struct rt_ebm_internal *ebm =
				(struct rt_ebm_internal *)es_int.idb_ptr;

			RT_EBM_CK_MAGIC( ebm );

			if( inpara == 1 )
				ebm->tallness = es_para[0];
			else if( inpara > 0 )
			{
				Tcl_AppendResult(interp,
					"extrusion depth required\n",
					(char *)NULL );
				mged_print_result( TCL_ERROR );
				return;
			}
			else if( es_scale > 0.0 )
			{
				ebm->tallness *= es_scale;
				es_scale = 0.0;
			}
		}
		break;

	case ECMD_VOL_CSIZE:	/* set voxel size */
		{
			struct rt_vol_internal *vol =
				(struct rt_vol_internal *)es_int.idb_ptr;

			RT_VOL_CK_MAGIC( vol );

			if( inpara == 3 )
				VMOVE( vol->cellsize, es_para )
			else if( inpara > 0 && inpara != 3 )
			{
				Tcl_AppendResult(interp, "x, y, and z cell sizes are required\n", (char *)NULL );
				mged_print_result( TCL_ERROR );
				return;
			}
			else if( es_scale > 0.0 )
			{
				VSCALE( vol->cellsize, vol->cellsize, es_scale )
				es_scale = 0.0;
			}
		}
		break;

	case ECMD_VOL_FSIZE:	/* set file size */
		{
			struct rt_vol_internal *vol =
				(struct rt_vol_internal *)es_int.idb_ptr;
			struct stat stat_buf;
			off_t need_size;

			RT_VOL_CK_MAGIC( vol );

			if( inpara == 3 )
			{
				if( stat( vol->file, &stat_buf ) )
				{
					Tcl_AppendResult(interp, "Cannot get status of file ", vol->file, (char *)NULL );
					mged_print_result( TCL_ERROR );
					return;
				}
				need_size = es_para[0] * es_para[1] * es_para[2] * sizeof( unsigned char );
				if( stat_buf.st_size < need_size )
				{
					Tcl_AppendResult(interp, "File (", vol->file,
						") is too small, set file name first", (char *)NULL );
					mged_print_result( TCL_ERROR );
					return;
				}
				vol->xdim = es_para[0];
				vol->ydim = es_para[1];
				vol->zdim = es_para[2];
			}
			else if( inpara > 0 )
			{
				Tcl_AppendResult(interp, "x, y, and z file sizes are required\n", (char *)NULL );
				mged_print_result( TCL_ERROR );
				return;
			}
		}
		break;

	case ECMD_VOL_THRESH_LO:
		{
			struct rt_vol_internal *vol =
				(struct rt_vol_internal *)es_int.idb_ptr;

			RT_VOL_CK_MAGIC( vol );

			i = vol->lo;
			if( inpara )
				i = es_para[0];
			else if( es_scale > 0.0 )
			{
				i = vol->lo * es_scale;
				if( i == vol->lo && es_scale > 1.0 )
					i++;
				else if( i == vol->lo && es_scale < 1.0 )
					i--;
			}

			if( i < 0 )
				i = 0;

			if( i > 255 )
				i = 255;

			vol->lo = i;
			break;
		}

	case ECMD_VOL_THRESH_HI:
		{
			struct rt_vol_internal *vol =
				(struct rt_vol_internal *)es_int.idb_ptr;

			RT_VOL_CK_MAGIC( vol );

			i = vol->hi;
			if( inpara )
				i = es_para[0];
			else if( es_scale > 0.0 )
			{
				i = vol->hi * es_scale;
				if( i == vol->hi && es_scale > 1.0 )
					i++;
				else if( i == vol->hi && es_scale < 1.0 )
					i--;
			}

			if( i < 0 )
				i = 0;

			if( i > 255 )
				i = 255;

			vol->hi = i;
			break;
		}

	case ECMD_VOL_FNAME:
		{
			struct rt_vol_internal *vol =
				(struct rt_vol_internal *)es_int.idb_ptr;
			char *fname;
			struct stat stat_buf;
			off_t need_size;

			RT_VOL_CK_MAGIC( vol );

			fname = get_file_name( vol->file );
			if( fname )
			{
				struct bu_vls message;

				if( stat( fname, &stat_buf ) )
				{
					bu_vls_init( &message );
					bu_vls_printf( &message, "Cannot get status of file %s\n", fname );
					Tcl_SetResult(interp, bu_vls_addr( &message ), TCL_VOLATILE );
					bu_vls_free( &message );
					mged_print_result( TCL_ERROR );
					return;
				}
				need_size = vol->xdim * vol->ydim * vol->zdim * sizeof( unsigned char );
				if( stat_buf.st_size < need_size )
				{
					bu_vls_init( &message );
					bu_vls_printf( &message, "File (%s) is too small, adjust the file size parameters first", fname);
					Tcl_SetResult(interp, bu_vls_addr( &message ), TCL_VOLATILE);
					bu_vls_free( &message );
					mged_print_result( TCL_ERROR );
					return;
				}
				strcpy( vol->file, fname );
			}

			break;
		}

	case ECMD_BOT_MODE:
		{
			struct rt_bot_internal *bot =
				(struct rt_bot_internal *)es_int.idb_ptr;
			const char *radio_result;
			char mode[10];
			int ret_tcl;
			int old_mode;

			RT_BOT_CK_MAGIC( bot );
			old_mode = bot->mode;
			sprintf( mode, " %d", old_mode - 1);
			ret_tcl = Tcl_VarEval( interp, "cad_radio", " .bot_mode_radio ",
				   bu_vls_addr( &pathName ), " _bot_mode_result",
				   " \"BOT Mode\"", "  \"Select the desired mode\"", mode,
				   " { surface volume plate plate/nocosine }",
				   " { \"In surface mode, each triangle represents part of a zero thickness surface and no volume is enclosed\" \"In volume mode, the triangles are expected to enclose a volume and that volume becomes the solid\" \"In plate mode, each triangle represents a plate with a specified thickness\" \"In plate/nocosine mode, each triangle represents a plate with a specified thickness, but the LOS thickness reported by the raytracer is independent of obliquity angle\" } ", (char *)NULL );
			if( ret_tcl != TCL_OK )
			{
				Tcl_AppendResult(interp, "Mode selection failed!!!\n", (char *)NULL );
				break;
			}
			radio_result = Tcl_GetVar( interp, "_bot_mode_result", TCL_GLOBAL_ONLY );
			bot->mode = atoi( radio_result ) + 1;
			if( bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS )
			{
				if( old_mode != RT_BOT_PLATE && old_mode != RT_BOT_PLATE_NOCOS )
				{
					/* need to create some thicknesses */
					bot->thickness = (fastf_t *)bu_calloc( bot->num_faces, sizeof( fastf_t ), "BOT thickness" );
					bot->face_mode = bu_bitv_new( bot->num_faces );
					bu_bitv_clear( bot->face_mode );
				}
			}
			else
			{
				if( old_mode == RT_BOT_PLATE || old_mode == RT_BOT_PLATE_NOCOS )
				{
					/* free the per face memory */
					bu_free( (char *)bot->thickness, "BOT thickness" );
					bot->thickness = (fastf_t *)NULL;
					bu_free( (char *)bot->face_mode, "BOT face_mode" );
					bot->face_mode = (struct bu_bitv *)NULL;
				}
			}
		}
		break;
	case ECMD_BOT_ORIENT:
		{
			struct rt_bot_internal *bot =
				(struct rt_bot_internal *)es_int.idb_ptr;
			const char *radio_result;
			char orient[10];
			int ret_tcl;

			RT_BOT_CK_MAGIC( bot );
			sprintf( orient, " %d", bot->orientation - 1);
			ret_tcl = Tcl_VarEval( interp, "cad_radio", " .bot_orient_radio ",
				   bu_vls_addr( &pathName ), " _bot_orient_result",
				   " \"BOT Face Orientation\"", "  \"Select the desired orientation\"", orient,
				   " { none right-hand-rule left-hand-rule }",
				   " { \"No orientation means that there is no particular order for the vertices of the triangles\" \"right-hand-rule means that the vertices of each triangle are ordered such that the right-hand-rule produces an outward pointing normal\"  \"left-hand-rule means that the vertices of each triangle are ordered such that the left-hand-rule produces an outward pointing normal\" } ", (char *)NULL );
			if( ret_tcl != TCL_OK )
			{
				Tcl_AppendResult(interp, "Face orientation selection failed!!!\n", (char *)NULL );
				break;
			}
			radio_result = Tcl_GetVar( interp, "_bot_orient_result", TCL_GLOBAL_ONLY );
			bot->orientation = atoi( radio_result ) + 1;
		}
		break;
	case ECMD_BOT_THICK:
		{
			struct rt_bot_internal *bot =
				(struct rt_bot_internal *)es_int.idb_ptr;
			int i, face_no;

			RT_BOT_CK_MAGIC( bot );

			if( bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS )
			{
				if( Tcl_VarEval( interp, "cad_dialog ", ".bot_err ", "$mged_gui(mged,screen) ", "{Not Plate Mode} ",
					"{Cannot edit face thickness in a non-plate BOT} ", "\"\" ", "0 ", "OK ",
					(char *)NULL ) != TCL_OK )
				{
					bu_log( "cad_dialog failed!!!!: %s\n", interp->result );
				}
				break;
			}

			if( bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0 )
			{
				/* setting thickness for all faces */
				if( !inpara )
					break;

				(void)Tcl_VarEval( interp, "cad_dialog ", ".bot_err ",
					"$mged_gui(mged,screen) ", "{Setting Thickness for All Faces} ",
					"{No face is selected, so this operation will modify all the faces in this BOT} ",
					"\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL );
				if( atoi( interp->result) )
					break;

				for( i=0 ; i<bot->num_faces ; i++ )
					bot->thickness[i] = es_para[0];
			}
			else
			{
				/* setting thickness for just one face */
				if( !inpara )
					break;

				face_no = -1;
				for( i=0 ; i < bot->num_faces ; i++ )
				{
					if( bot_verts[0] == bot->faces[i*3] &&
					    bot_verts[1] == bot->faces[i*3+1] &&
					    bot_verts[2] == bot->faces[i*3+2] )
					{
						face_no = i;
						break;
					}
				}
				if( face_no < 0 )
				{
					bu_log( "Cannot find face with vertices %d %d %d!!\n",
						V3ARGS( bot_verts ) );
					break;
				}

				bot->thickness[face_no] = es_para[0];
			}
		}
		break;
	case ECMD_BOT_FLAGS:
		{
			int ret_tcl;
			const char *dialog_result;
			char cur_settings[11];
			struct rt_bot_internal *bot =
				(struct rt_bot_internal *)es_int.idb_ptr;

			RT_BOT_CK_MAGIC( bot );

			strcpy( cur_settings, " { 0 0 }" );

			if( bot->bot_flags & RT_BOT_USE_NORMALS ) {
				cur_settings[3] = '1';
			}
			if( bot->bot_flags & RT_BOT_USE_FLOATS ) {
				cur_settings[5] = '1';
			}

			ret_tcl = Tcl_VarEval( interp,
					       "cad_list_buts",
					       " .bot_list_flags ",
					       bu_vls_addr( &pathName ),
					       " _bot_flags_result ",
					       cur_settings,
					       " \"BOT Flags\"",
					       " \"Select the desired flags\"",
					       " { {Use vertex normals} {Use single precision ray-tracing} }",
					       " { {This selection indicates that surface normals at hit points should be interpolated from vertex normals} {This selection indicates that the prepped form of the BOT triangles should use sigle precision to save memory} } ",
					       (char *)NULL );
			if( ret_tcl != TCL_OK )
			{
				bu_log( "ERROR: cad_list_buts: %s\n", interp->result );
				break;
			}
			dialog_result = Tcl_GetVar( interp, "_bot_flags_result", TCL_GLOBAL_ONLY );

			if( dialog_result[0] == '1' ) {
				bot->bot_flags |= RT_BOT_USE_NORMALS;
			} else {
				bot->bot_flags &= ~RT_BOT_USE_NORMALS;
			}
			if( dialog_result[2] == '1' ) {
				bot->bot_flags |= RT_BOT_USE_FLOATS;
			} else {
				bot->bot_flags &= ~RT_BOT_USE_FLOATS;
			}
		}
		break;
	case ECMD_BOT_FMODE:
		{
			struct rt_bot_internal *bot =
				(struct rt_bot_internal *)es_int.idb_ptr;
			char fmode[10];
			const char *radio_result;
			int face_no;
			int ret_tcl;

			RT_BOT_CK_MAGIC( bot );

			if( bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS )
			{
				(void)Tcl_VarEval( interp, "cad_dialog ", ".bot_err ", "$mged_gui(mged,screen) ", "{Not Plate Mode} ",
					"{Cannot edit face mode in a non-plate BOT} ", "\"\" ", "0 ", "OK ",
					(char *)NULL );
				break;
			}

			if( bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0 )
			{
				/* setting mode for all faces */
				(void)Tcl_VarEval( interp, "cad_dialog ", ".bot_err ",
					"$mged_gui(mged,screen) ", "{Setting Mode for All Faces} ",
					"{No face is selected, so this operation will modify all the faces in this BOT} ",
					"\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL );
				if( atoi( interp->result) )
					break;

				face_no = -2;
			}
			else
			{
				/* setting thickness for just one face */
				face_no = -1;
				for( i=0 ; i < bot->num_faces ; i++ )
				{
					if( bot_verts[0] == bot->faces[i*3] &&
					    bot_verts[1] == bot->faces[i*3+1] &&
					    bot_verts[2] == bot->faces[i*3+2] )
					{
						face_no = i;
						break;
					}
				}
				if( face_no < 0 )
				{
					bu_log( "Cannot find face with vertices %d %d %d!!\n",
						V3ARGS( bot_verts ) );
					break;
				}
			}

			if( face_no > -1 )
				sprintf( fmode, " %d", BU_BITTEST( bot->face_mode, face_no )?1:0 );
			else
				sprintf( fmode, " %d", BU_BITTEST( bot->face_mode, 0 )?1:0 );

			ret_tcl = Tcl_VarEval( interp, "cad_radio", " .bot_fmode_radio ", bu_vls_addr( &pathName ),
					       " _bot_fmode_result ", "\"BOT Face Mode\"",
					       " \"Select the desired face mode\"", fmode,
					       " { {Thickness centered about hit point} {Thickness appended to hit point} }",
					       " { {This selection will place the plate thickness centered about the hit point} {This selection will place the plate thickness rayward of the hit point} } ",
					       (char *)NULL );
			if( ret_tcl != TCL_OK )
			{
				bu_log( "ERROR: cad_radio: %s\n", interp->result );
				break;
			}
			radio_result = Tcl_GetVar( interp, "_bot_fmode_result", TCL_GLOBAL_ONLY );

			if( face_no > -1 )
			{
				if( atoi( radio_result ) )
					BU_BITSET( bot->face_mode, face_no );
				else
					BU_BITCLR( bot->face_mode, face_no );
			}
			else
			{
				if( atoi( radio_result ) )
				{
					for( i=0 ; i<bot->num_faces ; i++ )
						BU_BITSET( bot->face_mode, i );
				}
				else
					bu_bitv_clear( bot->face_mode );
			}
		}
		break;
	case ECMD_BOT_FDEL:
		{
			struct rt_bot_internal *bot =
				(struct rt_bot_internal *)es_int.idb_ptr;

			int i, j, face_no;

			RT_BOT_CK_MAGIC( bot );

			if( bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0 )
			{
				bu_log( "No Face selected!!!\n" );
				return;
			}

			face_no = -1;
			for( i=0 ; i < bot->num_faces ; i++ )
			{
				if( bot_verts[0] == bot->faces[i*3] &&
				    bot_verts[1] == bot->faces[i*3+1] &&
				    bot_verts[2] == bot->faces[i*3+2] )
				{
					face_no = i;
					break;
				}
			}
			if( face_no < 0 )
			{
				bu_log( "Cannot find selected face!!!\n");
				return;
			}
			bot->num_faces--;
			for( i=face_no ; i<bot->num_faces ; i++ )
			{
				j = i + 1;
				bot->faces[3*i] = bot->faces[3*j];
				bot->faces[3*i + 1] = bot->faces[3*j + 1];
				bot->faces[3*i + 2] = bot->faces[3*j + 2];
				if( bot->thickness )
					bot->thickness[i] = bot->thickness[j];
			}

			if( bot->face_mode )
			{
				struct bu_bitv *new_bitv;

				new_bitv = bu_bitv_new( bot->num_faces );
				BU_BITV_ZEROALL( new_bitv )
				for( i=0 ; i<face_no ; i++ )
				{
					if( BU_BITTEST( bot->face_mode, i ) )
						BU_BITSET( new_bitv, i );
				}
				for( i=face_no ; i<bot->num_faces ; i++ )
				{
					j = i+1;
					if( BU_BITTEST( bot->face_mode, j ) )
						BU_BITSET( new_bitv, i );
				}
				bu_bitv_free( bot->face_mode );
				bot->face_mode = new_bitv;
			}
			bot_verts[0] = -1;
			bot_verts[1] = -1;
			bot_verts[2] = -1;
		}
		break;
	case ECMD_EXTR_SKT_NAME:
		{
			struct rt_extrude_internal *extr =
				(struct rt_extrude_internal *)es_int.idb_ptr;
			const char *sketch_name;
			int ret_tcl;
			struct directory *dp;
			struct rt_db_internal tmp_ip;
			struct bu_vls tcl_cmd;

			RT_EXTRUDE_CK_MAGIC( extr );

			bu_vls_init( &tcl_cmd );
			bu_vls_printf( &tcl_cmd, "cad_input_dialog .get_sketch_name $mged_gui(mged,screen) {Select Sketch} {Enter the name of the sketch to be extruded} final_sketch_name %s 0 {{summary \"Enter sketch name\"}} APPLY DISMISS",
				extr->sketch_name );
			ret_tcl = Tcl_Eval( interp, bu_vls_addr( &tcl_cmd ) );
			if( ret_tcl != TCL_OK )
			{
				bu_log( "ERROR: %s\n", Tcl_GetStringResult( interp ) );
				bu_vls_free( &tcl_cmd );
				break;
			}

			if( atoi( Tcl_GetStringResult( interp ) ) == 1 )
				break;

			bu_vls_free( &tcl_cmd );

			sketch_name = Tcl_GetVar( interp, "final_sketch_name", TCL_GLOBAL_ONLY );
			if( extr->sketch_name )
				bu_free( (char *)extr->sketch_name, "extr->sketch_name" );
			extr->sketch_name = bu_strdup( sketch_name );

			if( extr->skt )
			{
				/* free the old sketch */
				RT_INIT_DB_INTERNAL( &tmp_ip );
				tmp_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
				tmp_ip.idb_type = ID_SKETCH;
				tmp_ip.idb_ptr = (genptr_t)extr->skt;
				tmp_ip.idb_meth = &rt_functab[ID_SKETCH];
				rt_db_free_internal( &tmp_ip, &rt_uniresource );
			}

			if( (dp=db_lookup( dbip, sketch_name, 0 )) == DIR_NULL )
			{
				bu_log( "Warning: %s does not exist!!!\n",
					sketch_name );
				extr->skt = (struct rt_sketch_internal *)NULL;
			}
			else
			{
				/* import the new sketch */

			        if( rt_db_get_internal( &tmp_ip, dp, dbip, bn_mat_identity, &rt_uniresource ) != ID_SKETCH )
			        {
			                bu_log( "rt_extrude_import: ERROR: Cannot import sketch (%.16s) for extrusion\n",
			                        sketch_name );
			        	extr->skt = (struct rt_sketch_internal *)NULL;
			        }
				else
					extr->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
			}
		}
		break;
	case ECMD_EXTR_MOV_H:
		{
			struct rt_extrude_internal *extr =
				(struct rt_extrude_internal *)es_int.idb_ptr;

			RT_EXTRUDE_CK_MAGIC( extr );
			if( inpara ) {
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model coordinates */
			    MAT4X3PNT( work, es_invmat, es_para );
			    VSUB2(extr->h, work, extr->V);
			  }else{
			    VSUB2(extr->h, es_para, extr->V);
			  }
#else
				/* apply es_invmat to convert to real model coordinates */
				MAT4X3PNT( work, es_invmat, es_para );
				VSUB2(extr->h, work, extr->V);
#endif
			}

			/* check for zero H vector */
			if( MAGNITUDE( extr->h ) <= SQRT_SMALL_FASTF ) {
			  Tcl_AppendResult(interp, "Zero H vector not allowed, resetting to +Z\n",
					   (char *)NULL);
				mged_print_result( TCL_ERROR );
			  VSET(extr->h, 0.0, 0.0, 1.0);
			  break;
			}
		}
		break;
	case ECMD_EXTR_SCALE_H:
		{
			struct rt_extrude_internal *extr =
				(struct rt_extrude_internal *)es_int.idb_ptr;

			RT_EXTRUDE_CK_MAGIC( extr );

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(extr->h);
				VSCALE(extr->h, extr->h, es_scale);
			}
			else if( es_scale > 0.0 )
			{
				VSCALE(extr->h, extr->h, es_scale);
				es_scale = 0.0;
			}
		}
		break;
	case ECMD_ARB_MAIN_MENU:
		/* put up control (main) menu for GENARB8s */
		menu_state->ms_flag = 0;
		es_edflag = IDLE;
		mmenu_set( MENU_L1, cntrl_menu );
		break;

	case ECMD_ARB_SPECIFIC_MENU:
		/* put up specific arb edit menus */
		menu_state->ms_flag = 0;
		es_edflag = IDLE;
		switch( es_menu ){
			case MENU_ARB_MV_EDGE:
				mmenu_set( MENU_L1, which_menu[es_type-4] );
				break;
			case MENU_ARB_MV_FACE:
				mmenu_set( MENU_L1, which_menu[es_type+1] );
				break;
			case MENU_ARB_ROT_FACE:
				mmenu_set( MENU_L1, which_menu[es_type+6] );
				break;
			default:
			  Tcl_AppendResult(interp, "Bad menu item.\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  return;
		}
		break;

	case ECMD_ARB_MOVE_FACE:
		/* move face through definite point */
		if(inpara) {
			arb = (struct rt_arb_internal *)es_int.idb_ptr;
			RT_ARB_CK_MAGIC( arb );

#ifdef TRY_EDIT_NEW_WAY
			if(mged_variables->mv_context){
			  /* apply es_invmat to convert to real model space */
			  MAT4X3PNT(work,es_invmat,es_para);
			}else{
			  VMOVE(work, es_para);
			}
#else
			/* apply es_invmat to convert to real model space */
			MAT4X3PNT(work,es_invmat,es_para);
#endif
			/* change D of planar equation */
			es_peqn[es_menu][3]=VDOT(&es_peqn[es_menu][0], work);
			/* find new vertices, put in record in vector notation */
			(void)rt_arb_calc_points( arb , es_type , es_peqn , &mged_tol );
		}
		break;

	case ECMD_ARB_SETUP_ROTFACE:
		arb = (struct rt_arb_internal *)es_int.idb_ptr;
		RT_ARB_CK_MAGIC( arb );

		/* check if point 5 is in the face */
		pnt5 = 0;
		for(i=0; i<4; i++)  {
			if( arb_vertices[es_type-4][es_menu*4+i]==5 )
				pnt5=1;
		}

		/* special case for arb7 */
		if( es_type == ARB7  && pnt5 ){
		  Tcl_AppendResult(interp, "\nFixed vertex is point 5.\n", (char *)NULL);
		  fixv = 5;
		}
		else{
#if 1
		  fixv = get_rotation_vertex();
#else
			/* find fixed vertex for ECMD_ARB_ROTATE_FACE */
			fixv=0;
			do  {
				int	type,loc,valid;
				char	line[128];

				type = es_type - 4;
				bu_log("\nEnter fixed vertex number( ");
				loc = es_menu*4;
				for(i=0; i<4; i++){
					if( arb_vertices[type][loc+i] )
						bu_log("%d ",
						    arb_vertices[type][loc+i]);
				}
				bu_log(") [%d]: ",arb_vertices[type][loc]);

				(void)fgets( line, sizeof(line), stdin );
				line[strlen(line)-1] = '\0';		/* remove newline */

				if( feof(stdin) )  quit();
				if( line[0] == '\0' )
					fixv = arb_vertices[type][loc]; 	/* default */
				else
					fixv = atoi( line );

				/* check whether nimble fingers entered valid vertex */
				valid = 0;
				for(j=0; j<4; j++)  {
					if( fixv==arb_vertices[type][loc+j] )
						valid=1;
				}
				if( !valid )
					fixv=0;
			} while( fixv <= 0 || fixv > es_type );
#endif
		}

		pr_prompt();
		fixv--;
		es_edflag = ECMD_ARB_ROTATE_FACE;
		view_state->vs_flag = 1;	/* draw arrow, etc */
		set_e_axes_pos(1);
		break;

	case ECMD_ARB_ROTATE_FACE:
		/* rotate a GENARB8 defining plane through a fixed vertex */

		arb = (struct rt_arb_internal *)es_int.idb_ptr;
		RT_ARB_CK_MAGIC( arb );

		if(inpara) {
			static mat_t invsolr;
			static vect_t tempvec;
			static float rota, fb;

			/*
			 * Keyboard parameters in degrees.
			 * First, cancel any existing rotations,
			 * then perform new rotation
			 */
			bn_mat_inv( invsolr, acc_rot_sol );
			eqp = &es_peqn[es_menu][0];	/* es_menu==plane of interest */
			VMOVE( work, eqp );
			MAT4X3VEC( eqp, invsolr, work );

			if( inpara == 3 ){
				/* 3 params:  absolute X,Y,Z rotations */
				/* Build completely new rotation change */
				MAT_IDN( modelchanges );
				bn_mat_angles(modelchanges,
					      es_para[0],
					      es_para[1],
					      es_para[2]);
				MAT_COPY(acc_rot_sol, modelchanges);

#ifdef TRY_EDIT_NEW_WAY
				/* Borrow incr_change matrix here */
				bn_mat_mul( incr_change, modelchanges, invsolr );
				if(mged_variables->mv_context){
				  /* calculate rotations about keypoint */
				  bn_mat_xform_about_pt( edit, incr_change, es_keypoint );

				  /* We want our final matrix (mat) to xform the original solid
				   * to the position of this instance of the solid, perform the
				   * current edit operations, then xform back.
				   *	mat = es_invmat * edit * es_mat
				   */
				  bn_mat_mul( mat1, edit, es_mat );
				  bn_mat_mul( mat, es_invmat, mat1 );
				  MAT_IDN( incr_change );
				  /* work contains original es_peqn[es_menu][0] */
				  MAT4X3VEC( eqp, mat, work );
				}else{
				  VMOVE( work, eqp );
				  MAT4X3VEC( eqp, modelchanges, work );
				}
#else
				/* Apply new rotation to face */
				eqp = &es_peqn[es_menu][0];

				VMOVE( work, eqp );
				MAT4X3VEC( eqp, modelchanges, work );
#endif
			}
			else if( inpara == 2 ){
				/* 2 parameters:  rot,fb were given */
				rota= es_para[0] * degtorad;
				fb  = es_para[1] * degtorad;

				/* calculate normal vector (length=1) from rot,fb */
				es_peqn[es_menu][0] = cos(fb) * cos(rota);
				es_peqn[es_menu][1] = cos(fb) * sin(rota);
				es_peqn[es_menu][2] = sin(fb);
			}
			else{
			  Tcl_AppendResult(interp, "Must be < rot fb | xdeg ydeg zdeg >\n",
					   (char *)NULL);
				mged_print_result( TCL_ERROR );
			  return;
			}

			/* point notation of fixed vertex */
			VMOVE( tempvec, arb->pt[fixv] );

			/* set D of planar equation to anchor at fixed vertex */
			/* es_menu == plane of interest */
			es_peqn[es_menu][3]=VDOT(eqp,tempvec);

			/*  Clear out solid rotation */
			MAT_IDN( modelchanges );

		}  else  {
			/* Apply incremental changes */
			static vect_t tempvec;

			eqp = &es_peqn[es_menu][0];
			VMOVE( work, eqp );
			MAT4X3VEC( eqp, incr_change, work );

			/* point notation of fixed vertex */
			VMOVE( tempvec, arb->pt[fixv] );

			/* set D of planar equation to anchor at fixed vertex */
			/* es_menu == plane of interest */
			es_peqn[es_menu][3]=VDOT(eqp,tempvec);
		}

		(void)rt_arb_calc_points( arb , es_type , es_peqn , &mged_tol );
		MAT_IDN( incr_change );

		/* no need to calc_planes again */
		replot_editing_solid();

		inpara = 0;
		return;

	case SSCALE:
		/* scale the solid uniformly about it's vertex point */
		{
			mat_t	scalemat;
			mat_t   mat, mat2;

			es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
			es_pipept = (struct wdb_pipept *)NULL; /* Reset es_pipept */
			bot_verts[0] = -1;
			bot_verts[1] = -1;
			bot_verts[2] = -1;
			if(inpara) {
				/* accumulate the scale factor */
				es_scale = es_para[0] / acc_sc_sol;
				acc_sc_sol = es_para[0];
			}

			bn_mat_scale_about_pt( scalemat, es_keypoint, es_scale );
			bn_mat_mul(mat2, scalemat, es_mat);
			bn_mat_mul(mat, es_invmat, mat2);
			transform_editing_solid(&es_int, mat, &es_int, 1);

			/* reset solid scale factor */
			es_scale = 1.0;
		}
		break;

	case STRANS:
		/* translate solid  */
		{
			vect_t	delta;
			mat_t	xlatemat;

			es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
			es_pipept = (struct wdb_pipept *)NULL; /* Reset es_pipept */
			bot_verts[0] = -1;
			bot_verts[1] = -1;
			bot_verts[2] = -1;
			if(inpara) {
				/* Need vector from current vertex/keypoint
				 * to desired new location.
				 */
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){ /* move solid so that es_keypoint is at position es_para */
			    vect_t raw_para;

			    MAT4X3PNT(raw_para, es_invmat, es_para);
			    MAT4X3PNT(work, es_invmat, es_keypoint);
			    VSUB2( delta, work, raw_para );
			    MAT_IDN( xlatemat );
			    MAT_DELTAS_VEC_NEG( xlatemat, delta );
			  }else{ /* move solid to position es_para */
			    /* move solid to position es_para */
			    MAT4X3PNT(work, es_invmat, es_keypoint);
			    VSUB2( delta, work, es_para );
			    MAT_IDN( xlatemat );
			    MAT_DELTAS_VEC_NEG( xlatemat, delta );
			  }
#else
				VSUB2( delta, es_para, es_keypoint );
				MAT_IDN( xlatemat );
				MAT_DELTAS_VEC( xlatemat, delta );
#endif
				transform_editing_solid(&es_int, xlatemat, &es_int, 1);
			}
		}
		break;
	case ECMD_VTRANS:
		/* translate a vertex */
		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		es_pipept = (struct wdb_pipept *)NULL; /* Reset es_pipept */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		if( es_mvalid )  {
			/* Mouse parameter:  new position in model space */
			VMOVE( es_para, es_mparam );
			inpara = 1;
		}
		if(inpara) {


			/* Keyboard parameter:  new position in model space.
			 * XXX for now, splines only here */
			register struct rt_nurb_internal *sip =
				(struct rt_nurb_internal *) es_int.idb_ptr;
			register struct face_g_snurb	*surf;
			register fastf_t	*fp;

			RT_NURB_CK_MAGIC(sip);
			surf = sip->srfs[spl_surfno];
			NMG_CK_SNURB(surf);
			fp = &RT_NURB_GET_CONTROL_POINT( surf, spl_ui, spl_vi );
#ifdef TRY_EDIT_NEW_WAY
			if(mged_variables->mv_context){
			  /* apply es_invmat to convert to real model space */
			  MAT4X3PNT( fp, es_invmat, es_para );
			}else{
			  VMOVE( fp, es_para );
			}
#else
			VMOVE( fp, es_para );
#endif
		}
		break;

	case ECMD_CLINE_SCALE_H:
		/*
		 * Scale height vector
		 */
		{
			struct rt_cline_internal *cli =
				(struct rt_cline_internal *)es_int.idb_ptr;

			RT_CLINE_CK_MAGIC( cli );

			if( inpara )
			{
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(cli->h);
				VSCALE(cli->h, cli->h, es_scale);
			}
			else if( es_scale > 0.0 )
			{
				VSCALE(cli->h, cli->h, es_scale);
				es_scale = 0.0;
			}
		}
		break;

	case ECMD_CLINE_SCALE_R:
		/*
		 * Scale radius
		 */
		{
			struct rt_cline_internal *cli =
				(struct rt_cline_internal *)es_int.idb_ptr;

			RT_CLINE_CK_MAGIC( cli );

			if( inpara )
				cli->radius = es_para[0];
			else if( es_scale > 0.0 )
			{
				cli->radius *= es_scale;
				es_scale = 0.0;
			}
		}
		break;

	case ECMD_CLINE_SCALE_T:
		/*
		 * Scale plate thickness
		 */
		{
			struct rt_cline_internal *cli =
				(struct rt_cline_internal *)es_int.idb_ptr;

			RT_CLINE_CK_MAGIC( cli );

			if( inpara )
				cli->thickness = es_para[0];
			else if( es_scale > 0.0 )
			{
				cli->thickness *= es_scale;
				es_scale = 0.0;
			}
		}
		break;

	case ECMD_CLINE_MOVE_H:
		/*
		 * Move end of height vector
		 */
		{
			struct rt_cline_internal *cli =
				(struct rt_cline_internal *)es_int.idb_ptr;

			RT_CLINE_CK_MAGIC( cli );

			if( inpara )
			{
#ifdef TRY_EDIT_NEW_WAY
				if( mged_variables->mv_context )
				{
					MAT4X3PNT( work, es_invmat, es_para );
					VSUB2( cli->h, work, cli->v );
				}
				else
					VSUB2( cli->h, es_para, cli->v )
#else
				MAT4X3PNT( work, es_invmat, es_para );
				VSUB2( cli->h, work, cli->v );
#endif
			}
			/* check for zero H vector */
			if( MAGNITUDE( cli->h ) <= SQRT_SMALL_FASTF ) {
			  Tcl_AppendResult(interp, "Zero H vector not allowed, resetting to +Z\n",
					   (char *)NULL);
				mged_print_result( TCL_ERROR );
			  VSET(cli->h, 0.0, 0.0, 1.0 );
			  break;
			}
		}
		break;

	case ECMD_TGC_MV_H:
		/*
		 * Move end of H of tgc, keeping plates perpendicular
		 * to H vector.
		 */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;

			RT_TGC_CK_MAGIC(tgc);
			if( inpara ) {
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model coordinates */
			    MAT4X3PNT( work, es_invmat, es_para );
			    VSUB2(tgc->h, work, tgc->v);
			  }else{
			    VSUB2(tgc->h, es_para, tgc->v);
			  }
#else
				/* apply es_invmat to convert to real model coordinates */
				MAT4X3PNT( work, es_invmat, es_para );
				VSUB2(tgc->h, work, tgc->v);
#endif
			}

			/* check for zero H vector */
			if( MAGNITUDE( tgc->h ) <= SQRT_SMALL_FASTF ) {
			  Tcl_AppendResult(interp, "Zero H vector not allowed, resetting to +Z\n",
					   (char *)NULL);
				mged_print_result( TCL_ERROR );
			  VSET(tgc->h, 0.0, 0.0, 1.0 );
			  break;
			}

			/* have new height vector --  redefine rest of tgc */
			la = MAGNITUDE( tgc->a );
			lb = MAGNITUDE( tgc->b );
			lc = MAGNITUDE( tgc->c );
			ld = MAGNITUDE( tgc->d );

			/* find 2 perpendicular vectors normal to H for new A,B */
			bn_vec_perp( tgc->b, tgc->h );
			VCROSS(tgc->a, tgc->b, tgc->h);
			VUNITIZE(tgc->a);
			VUNITIZE(tgc->b);

			/* Create new C,D from unit length A,B, with previous len */
			VSCALE(tgc->c, tgc->a, lc);
			VSCALE(tgc->d, tgc->b, ld);

			/* Restore original vector lengths to A,B */
			VSCALE(tgc->a, tgc->a, la);
			VSCALE(tgc->b, tgc->b, lb);
		}
		break;

	case ECMD_TGC_MV_HH:
		/* Move end of H of tgc - leave ends alone */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;

			RT_TGC_CK_MAGIC(tgc);
			if( inpara ) {
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model coordinates */
			    MAT4X3PNT( work, es_invmat, es_para );
			    VSUB2(tgc->h, work, tgc->v);
			  }else{
			    VSUB2(tgc->h, es_para, tgc->v);
			  }
#else
				/* apply es_invmat to convert to real model coordinates */
				MAT4X3PNT( work, es_invmat, es_para );
				VSUB2(tgc->h, work, tgc->v);
#endif
			}

			/* check for zero H vector */
			if( MAGNITUDE( tgc->h ) <= SQRT_SMALL_FASTF ) {
			  Tcl_AppendResult(interp, "Zero H vector not allowed, resetting to +Z\n",
					   (char *)NULL);
				mged_print_result( TCL_ERROR );
			  VSET(tgc->h, 0.0, 0.0, 1.0);
			  break;
			}
		}
		break;

	case PSCALE:
		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		pscale();
		break;

	case PTARB:	/* move an ARB point */
	case EARB:   /* edit an ARB edge */
		if( inpara ) {
#ifdef TRY_EDIT_NEW_WAY
		  if(mged_variables->mv_context){
		    /* apply es_invmat to convert to real model space */
		    MAT4X3PNT( work, es_invmat, es_para );
		  }else{
		    VMOVE( work, es_para );
		  }
#else
			/* apply es_invmat to convert to real model space */
			MAT4X3PNT( work, es_invmat, es_para );
#endif
			editarb( work );
		}
		break;

	case SROT:
		/* rot solid about vertex */
		{
			es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
			es_pipept = (struct wdb_pipept *)NULL; /* Reset es_pipept */
			bot_verts[0] = -1;
			bot_verts[1] = -1;
			bot_verts[2] = -1;
			if(inpara) {
				static mat_t invsolr;
				/*
				 * Keyboard parameters:  absolute x,y,z rotations,
				 * in degrees.  First, cancel any existing rotations,
				 * then perform new rotation
				 */
				bn_mat_inv( invsolr, acc_rot_sol );

				/* Build completely new rotation change */
				MAT_IDN( modelchanges );
				bn_mat_angles(modelchanges,
					      es_para[0],
					      es_para[1],
					      es_para[2]);
				/* Borrow incr_change matrix here */
				bn_mat_mul( incr_change, modelchanges, invsolr );
				MAT_COPY(acc_rot_sol, modelchanges);

				/* Apply new rotation to solid */
				/*  Clear out solid rotation */
				MAT_IDN( modelchanges );
			}  else  {
				/* Apply incremental changes already in incr_change */
			}
			/* Apply changes to solid */
			/* xlate keypoint to origin, rotate, then put back. */
#ifdef TRY_EDIT_NEW_WAY
			switch(mged_variables->mv_rotate_about){
			case 'v':       /* View Center */
			  VSET(work, 0.0, 0.0, 0.0);
			  MAT4X3PNT(rot_point, view_state->vs_vop->vo_view2model, work);
			  break;
			case 'e':       /* Eye */
			  VSET(work, 0.0, 0.0, 1.0);
			  MAT4X3PNT(rot_point, view_state->vs_vop->vo_view2model, work);
			  break;
			case 'm':       /* Model Center */
			  VSETALL(rot_point, 0.0);
			  break;
			case 'k':       /* Key Point */
			default:
			  VMOVE(rot_point, es_keypoint);
			  break;
			}

			if(mged_variables->mv_context){
			  /* calculate rotations about keypoint */
			  bn_mat_xform_about_pt( edit, incr_change, rot_point );

			  /* We want our final matrix (mat) to xform the original solid
			   * to the position of this instance of the solid, perform the
			   * current edit operations, then xform back.
			   *	mat = es_invmat * edit * es_mat
			   */
			  bn_mat_mul( mat1, edit, es_mat );
			  bn_mat_mul( mat, es_invmat, mat1 );
			}else{
			  MAT4X3PNT(work, es_invmat, rot_point);
			  bn_mat_xform_about_pt( mat, incr_change, work );
			}
#else
			bn_mat_xform_about_pt( mat, incr_change, es_keypoint);
#endif
			transform_editing_solid(&es_int, mat, &es_int, 1);

			MAT_IDN( incr_change );
		}
		break;

	case ECMD_EXTR_ROT_H:
		/* rotate height vector */
		{
			struct rt_extrude_internal	*extr =
				(struct rt_extrude_internal *)es_int.idb_ptr;

			RT_EXTRUDE_CK_MAGIC(extr);
#ifdef TRY_EDIT_NEW_WAY
			if(inpara) {
				static mat_t invsolr;
				/*
				 * Keyboard parameters:  absolute x,y,z rotations,
				 * in degrees.  First, cancel any existing rotations,
				 * then perform new rotation
				 */
				bn_mat_inv( invsolr, acc_rot_sol );

				/* Build completely new rotation change */
				MAT_IDN( modelchanges );
				bn_mat_angles(modelchanges,
					      es_para[0],
					      es_para[1],
					      es_para[2]);
				/* Borrow incr_change matrix here */
				bn_mat_mul( incr_change, modelchanges, invsolr );
				MAT_COPY(acc_rot_sol, modelchanges);

				/* Apply new rotation to solid */
				/*  Clear out solid rotation */
				MAT_IDN( modelchanges );
			}  else  {
				/* Apply incremental changes already in incr_change */
			}

			if(mged_variables->mv_context){
			  /* calculate rotations about keypoint */
			  bn_mat_xform_about_pt( edit, incr_change, es_keypoint );

			  /* We want our final matrix (mat) to xform the original solid
			   * to the position of this instance of the solid, perform the
			   * current edit operations, then xform back.
			   *	mat = es_invmat * edit * es_mat
			   */
			  bn_mat_mul( mat1, edit, es_mat );
			  bn_mat_mul( mat, es_invmat, mat1 );
			  MAT4X3VEC(extr->h, mat, extr->h);
			}else{
			  MAT4X3VEC(extr->h, incr_change, extr->h);
			}
#else
			MAT4X3VEC(extr->h, incr_change, extr->h);
#endif

			MAT_IDN( incr_change );
		}
		break;

	case ECMD_TGC_ROT_H:
		/* rotate height vector */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;

			RT_TGC_CK_MAGIC(tgc);
#ifdef TRY_EDIT_NEW_WAY
			if(inpara) {
				static mat_t invsolr;
				/*
				 * Keyboard parameters:  absolute x,y,z rotations,
				 * in degrees.  First, cancel any existing rotations,
				 * then perform new rotation
				 */
				bn_mat_inv( invsolr, acc_rot_sol );

				/* Build completely new rotation change */
				MAT_IDN( modelchanges );
				bn_mat_angles(modelchanges,
					      es_para[0],
					      es_para[1],
					      es_para[2]);
				/* Borrow incr_change matrix here */
				bn_mat_mul( incr_change, modelchanges, invsolr );
				MAT_COPY(acc_rot_sol, modelchanges);

				/* Apply new rotation to solid */
				/*  Clear out solid rotation */
				MAT_IDN( modelchanges );
			}  else  {
				/* Apply incremental changes already in incr_change */
			}

			if(mged_variables->mv_context){
			  /* calculate rotations about keypoint */
			  bn_mat_xform_about_pt( edit, incr_change, es_keypoint );

			  /* We want our final matrix (mat) to xform the original solid
			   * to the position of this instance of the solid, perform the
			   * current edit operations, then xform back.
			   *	mat = es_invmat * edit * es_mat
			   */
			  bn_mat_mul( mat1, edit, es_mat );
			  bn_mat_mul( mat, es_invmat, mat1 );
			  MAT4X3VEC(tgc->h, mat, tgc->h);
			}else{
			  MAT4X3VEC(tgc->h, incr_change, tgc->h);
			}
#else
			MAT4X3VEC(tgc->h, incr_change, tgc->h);
#endif

			MAT_IDN( incr_change );
		}
		break;

	case ECMD_TGC_ROT_AB:
		/* rotate surfaces AxB and CxD (tgc) */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;

			RT_TGC_CK_MAGIC(tgc);
#ifdef TRY_EDIT_NEW_WAY
			if(inpara) {
				static mat_t invsolr;
				/*
				 * Keyboard parameters:  absolute x,y,z rotations,
				 * in degrees.  First, cancel any existing rotations,
				 * then perform new rotation
				 */
				bn_mat_inv( invsolr, acc_rot_sol );

				/* Build completely new rotation change */
				MAT_IDN( modelchanges );
				bn_mat_angles(modelchanges,
					      es_para[0],
					      es_para[1],
					      es_para[2]);
				/* Borrow incr_change matrix here */
				bn_mat_mul( incr_change, modelchanges, invsolr );
				MAT_COPY(acc_rot_sol, modelchanges);

				/* Apply new rotation to solid */
				/*  Clear out solid rotation */
				MAT_IDN( modelchanges );
			}  else  {
				/* Apply incremental changes already in incr_change */
			}

			if(mged_variables->mv_context){
			  /* calculate rotations about keypoint */
			  bn_mat_xform_about_pt( edit, incr_change, es_keypoint );

			  /* We want our final matrix (mat) to xform the original solid
			   * to the position of this instance of the solid, perform the
			   * current edit operations, then xform back.
			   *	mat = es_invmat * edit * es_mat
			   */
			  bn_mat_mul( mat1, edit, es_mat );
			  bn_mat_mul( mat, es_invmat, mat1 );
			  MAT4X3VEC(tgc->a, mat, tgc->a);
			  MAT4X3VEC(tgc->b, mat, tgc->b);
			  MAT4X3VEC(tgc->c, mat, tgc->c);
			  MAT4X3VEC(tgc->d, mat, tgc->d);
			}else{
			  MAT4X3VEC(tgc->a, incr_change, tgc->a);
			  MAT4X3VEC(tgc->b, incr_change, tgc->b);
			  MAT4X3VEC(tgc->c, incr_change, tgc->c);
			  MAT4X3VEC(tgc->d, incr_change, tgc->d);
			}
#else
			MAT4X3VEC(work, incr_change, tgc->a);
			VMOVE(tgc->a, work);
			MAT4X3VEC(work, incr_change, tgc->b);
			VMOVE(tgc->b, work);
			MAT4X3VEC(work, incr_change, tgc->c);
			VMOVE(tgc->c, work);
			MAT4X3VEC(work, incr_change, tgc->d);
			VMOVE(tgc->d, work);
#endif
			MAT_IDN( incr_change );
		}
		break;

	case ECMD_ETO_ROT_C:
		/* rotate ellipse semi-major axis vector */
		{
			struct rt_eto_internal	*eto =
				(struct rt_eto_internal *)es_int.idb_ptr;

			RT_ETO_CK_MAGIC(eto);
#ifdef TRY_EDIT_NEW_WAY
			if(inpara) {
				static mat_t invsolr;
				/*
				 * Keyboard parameters:  absolute x,y,z rotations,
				 * in degrees.  First, cancel any existing rotations,
				 * then perform new rotation
				 */
				bn_mat_inv( invsolr, acc_rot_sol );

				/* Build completely new rotation change */
				MAT_IDN( modelchanges );
				bn_mat_angles(modelchanges,
					      es_para[0],
					      es_para[1],
					      es_para[2]);
				/* Borrow incr_change matrix here */
				bn_mat_mul( incr_change, modelchanges, invsolr );
				MAT_COPY(acc_rot_sol, modelchanges);

				/* Apply new rotation to solid */
				/*  Clear out solid rotation */
				MAT_IDN( modelchanges );
			}  else  {
				/* Apply incremental changes already in incr_change */
			}

			if(mged_variables->mv_context){
			  /* calculate rotations about keypoint */
			  bn_mat_xform_about_pt( edit, incr_change, es_keypoint );

			  /* We want our final matrix (mat) to xform the original solid
			   * to the position of this instance of the solid, perform the
			   * current edit operations, then xform back.
			   *	mat = es_invmat * edit * es_mat
			   */
			  bn_mat_mul( mat1, edit, es_mat );
			  bn_mat_mul( mat, es_invmat, mat1 );

			  MAT4X3VEC(eto->eto_C, mat, eto->eto_C);
			}else{
			  MAT4X3VEC(eto->eto_C, incr_change, eto->eto_C);
			}
#else
			MAT4X3VEC(work, incr_change, eto->eto_C);
			VMOVE(eto->eto_C, work);
#endif
		}
		MAT_IDN( incr_change );
		break;

	case ECMD_NMG_EPICK:
		/* XXX Nothing to do here (yet), all done in mouse routine. */
		break;
	case ECMD_NMG_EMOVE:
		{
			point_t new_pt;

			if( !es_eu )
			{
			  Tcl_AppendResult(interp, "No edge selected!\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			NMG_CK_EDGEUSE( es_eu );

			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt, es_para );
			  }
#else
				VMOVE( new_pt , es_para );
#endif
			}else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for edge move\n",
					   (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			if( !nmg_find_fu_of_eu( es_eu ) && *es_eu->up.magic_p == NMG_LOOPUSE_MAGIC )
			{
				struct loopuse *lu;
				fastf_t area;
				plane_t pl;

				/* this edge is in a wire loop
				 * keep the loop planar
				 */
				lu = es_eu->up.lu_p;
				NMG_CK_LOOPUSE( lu );

				/* get plane equation for loop */
				area = nmg_loop_plane_area( lu , pl );
				if( area > 0.0 )
				{
					vect_t view_z_dir;
					vect_t view_dir;
					fastf_t dist;

					/* Get view direction vector */
					VSET( view_z_dir, 0.0, 0.0, 1.0 );
					MAT4X3VEC(view_dir , view_state->vs_vop->vo_view2model , view_z_dir);

					/* intersect line through new_pt with plane of loop */
					if( bn_isect_line3_plane( &dist , new_pt , view_dir , pl , &mged_tol ) < 1)
					{
					  /* line does not intersect plane, don't do an esplit */
					  Tcl_AppendResult(interp, "Edge Move: Cannot place new point in plane of loop\n", (char *)NULL);
					  mged_print_result( TCL_ERROR );
						break;
					}
					VJOIN1( new_pt , new_pt , dist , view_dir );
				}
			}

			if( nmg_move_edge_thru_pt( es_eu, new_pt, &mged_tol ) < 0 ) {
				VPRINT("Unable to hit", new_pt);
			}
		}
		break;

	case ECMD_NMG_EKILL:
		{
			struct model *m;
			struct edge_g_lseg *eg;

			if( !es_eu )
			{
			  Tcl_AppendResult(interp, "No edge selected!\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			NMG_CK_EDGEUSE( es_eu );

			m = nmg_find_model( &es_eu->l.magic );

			if( *es_eu->up.magic_p == NMG_LOOPUSE_MAGIC )
			{
				struct loopuse *lu;
				struct edgeuse *prev_eu,*next_eu;

				lu = es_eu->up.lu_p;
				NMG_CK_LOOPUSE( lu );

				if( *lu->up.magic_p != NMG_SHELL_MAGIC )
				{
				  /* Currently can only kill wire edges or edges in wire loops */
				  Tcl_AppendResult(interp, "Currently, we can only kill wire edges or edges in wire loops\n", (char *)NULL);
				  mged_print_result( TCL_ERROR );
				  es_edflag = IDLE;
				  break;
				}

				prev_eu = BU_LIST_PPREV_CIRC( edgeuse , &es_eu->l );
				NMG_CK_EDGEUSE( prev_eu );

				if( prev_eu == es_eu )
				{
					/* only one edge left in the loop
					 * make it an edge to/from same vertex
					 */
					if( es_eu->vu_p->v_p == es_eu->eumate_p->vu_p->v_p )
					{
					  /* refuse to delete last edge that runs
					   * to/from same vertex
					   */
					  Tcl_AppendResult(interp, "Cannot delete last edge running to/from same vertex\n", (char *)NULL);
						mged_print_result( TCL_ERROR );
						break;
					}
					NMG_CK_EDGEUSE( es_eu->eumate_p );
					nmg_movevu( es_eu->eumate_p->vu_p , es_eu->vu_p->v_p );
					break;
				}

				next_eu = BU_LIST_PNEXT_CIRC( edgeuse , &es_eu->l );
				NMG_CK_EDGEUSE( next_eu );

				nmg_movevu( next_eu->vu_p , es_eu->vu_p->v_p );
				if( nmg_keu( es_eu ) )
				{
					/* Should never happen!!! */
					bu_bomb( "sedit(): killed edge and emptied loop!!\n" );
				}
				es_eu = prev_eu;
				nmg_rebound( m , &mged_tol );

				/* fix edge geometry for modified edge (next_eu ) */
				eg = next_eu->g.lseg_p;
				NMG_CK_EDGE_G_LSEG( eg );
				VMOVE( eg->e_pt , next_eu->vu_p->v_p->vg_p->coord );
				VSUB2( eg->e_dir, next_eu->eumate_p->vu_p->v_p->vg_p->coord, next_eu->vu_p->v_p->vg_p->coord );

				break;
			}
			else if( *es_eu->up.magic_p == NMG_SHELL_MAGIC )
			{
				/* wire edge, just kill it */
				(void)nmg_keu( es_eu );
				es_eu = (struct edgeuse *)NULL;
				nmg_rebound( m , &mged_tol );
			}
		}

	case ECMD_NMG_ESPLIT:
		{
			struct vertex *v=(struct vertex *)NULL;
			struct edge_g_lseg *eg;
			struct model *m;
			point_t new_pt;
			fastf_t area;
			plane_t pl;

			if( !es_eu )
			{
			  Tcl_AppendResult(interp, "No edge selected!\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			NMG_CK_EDGEUSE( es_eu );
			m = nmg_find_model( &es_eu->l.magic );
			NMG_CK_MODEL( m );
			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
			  VMOVE( new_pt , es_para );
#endif
			}else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for edge split\n",
					   (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			if( *es_eu->up.magic_p == NMG_LOOPUSE_MAGIC )
			{
				struct loopuse *lu;

				lu = es_eu->up.lu_p;
				NMG_CK_LOOPUSE( lu );

				/* Currently, can only split wire edges or edges in wire loops */
				if( *lu->up.magic_p != NMG_SHELL_MAGIC )
				{
				  Tcl_AppendResult(interp, "Currently, we can only split wire edges or edges in wire loops\n", (char *)NULL);
					es_edflag = IDLE;
					mged_print_result( TCL_ERROR );
					break;
				}

				/* get plane equation for loop */
				area = nmg_loop_plane_area( lu , pl );
				if( area > 0.0 )
				{
					vect_t view_z_dir;
					vect_t view_dir;
					fastf_t dist;

					/* Get view direction vector */
					VSET( view_z_dir, 0.0, 0.0, 1.0 );
					MAT4X3VEC(view_dir, view_state->vs_vop->vo_view2model, view_z_dir);

					/* intersect line through new_pt with plane of loop */
					if( bn_isect_line3_plane( &dist , new_pt , view_dir , pl , &mged_tol ) < 1)
					{
					  /* line does not intersect plane, don't do an esplit */
					  Tcl_AppendResult(interp, "Edge Split: Cannot place new point in plane of loop\n", (char *)NULL);
						mged_print_result( TCL_ERROR );
						break;
					}
					VJOIN1( new_pt , new_pt , dist , view_dir );
				}
			}
			es_eu = nmg_esplit( v , es_eu , 0 );
			nmg_vertex_gv( es_eu->vu_p->v_p , new_pt );
			nmg_rebound( m , &mged_tol );
			eg = es_eu->g.lseg_p;
			NMG_CK_EDGE_G_LSEG( eg );
			VMOVE( eg->e_pt , new_pt );
			VSUB2( eg->e_dir , es_eu->eumate_p->vu_p->v_p->vg_p->coord , new_pt );
		}
		break;
	case ECMD_NMG_LEXTRU:
		{
			fastf_t dist;
			point_t to_pt;
			vect_t extrude_vec;
			struct loopuse *new_lu;
			struct faceuse *fu;
			struct model *m;
			plane_t new_lu_pl;
			fastf_t area;

			if( es_mvalid )
				VMOVE( to_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( to_pt, es_invmat, es_para);
			  }else{
			    VMOVE( to_pt , es_para );
			  }
#else
				VMOVE( to_pt , es_para )
#endif
			}
			else if( inpara == 1 )
				VJOIN1( to_pt, lu_keypoint, es_para[0], lu_pl )
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for loop extrusion\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			VSUB2( extrude_vec , to_pt , lu_keypoint );

			if( bn_isect_line3_plane( &dist , to_pt , extrude_vec , lu_pl , &mged_tol ) < 1 )
			{
			  Tcl_AppendResult(interp, "Cannot extrude parallel to plane of loop\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  return;
			}

			if( BU_LIST_NON_EMPTY( &es_s->fu_hd ) )
			{
				struct nmgregion *r;

				r = es_s->r_p;
				(void) nmg_ks( es_s );
				es_s = nmg_msv( r );
			}

			new_lu = nmg_dup_loop( lu_copy , &es_s->l.magic , (long **)0 );
			area = nmg_loop_plane_area( new_lu , new_lu_pl );
			if( area < 0.0 )
			{
			  Tcl_AppendResult(interp, "loop to be extruded as no area!!!\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  return;
			}

			if( VDOT( extrude_vec , new_lu_pl ) > 0.0 )
			{
				plane_t tmp_pl;

				fu = nmg_mf( new_lu->lumate_p );
				NMG_CK_FACEUSE( fu );
				HREVERSE( tmp_pl , new_lu_pl );
				nmg_face_g( fu , tmp_pl );
			}
			else
			{
				fu = nmg_mf( new_lu );
				NMG_CK_FACEUSE( fu );
				nmg_face_g( fu , new_lu_pl );
			}

			(void)nmg_extrude_face( fu , extrude_vec , &mged_tol );

			nmg_fix_normals( fu->s_p , &mged_tol );

			m = nmg_find_model( &fu->l.magic );
			nmg_rebound( m , &mged_tol );
			(void)nmg_ck_geometry( m , &mged_tol );

			es_eu = (struct edgeuse *)NULL;

			replot_editing_solid();
			view_state->vs_flag = 1;
		}
		break;
	case ECMD_PIPE_PICK:
		{
			struct rt_pipe_internal *pipe=
				(struct rt_pipe_internal *)es_int.idb_ptr;
			point_t new_pt;

			RT_PIPE_CK_MAGIC( pipe );

			if( es_mvalid )
			  VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
			  VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for segment selection\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			es_pipept = find_pipept_nearest_pt( &pipe->pipe_segs_head, new_pt );
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No PIPE segment selected\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			}
			else
				rt_pipept_print( es_pipept, base2local );
		}
		break;
	case ECMD_PIPE_SPLIT:
		{
			struct rt_pipe_internal *pipe=
				(struct rt_pipe_internal *)es_int.idb_ptr;
			point_t new_pt;

			RT_PIPE_CK_MAGIC( pipe );

			if( es_mvalid )
			  VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for segment split\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No pipe segment selected\n", (char *)NULL);
				mged_print_result( TCL_ERROR );
				break;
			}

			split_pipept( &pipe->pipe_segs_head, es_pipept, new_pt );
		}
		break;
	case ECMD_PIPE_PT_MOVE:
		{
			struct rt_pipe_internal *pipe=
				(struct rt_pipe_internal *)es_int.idb_ptr;
			point_t new_pt;

			RT_PIPE_CK_MAGIC( pipe );

			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for segment movement\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No pipe segment selected\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}

			move_pipept( pipe, es_pipept, new_pt );
		}
		break;
	case ECMD_PIPE_PT_ADD:
		{
			struct rt_pipe_internal *pipe=
				(struct rt_pipe_internal *)es_int.idb_ptr;
			point_t new_pt;

			RT_PIPE_CK_MAGIC( pipe );

			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for 'append segment'\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			es_pipept = add_pipept( pipe, es_pipept, new_pt );
		}
		break;
	case ECMD_PIPE_PT_INS:
		{
			struct rt_pipe_internal *pipe=
				(struct rt_pipe_internal *)es_int.idb_ptr;
			point_t new_pt;

			RT_PIPE_CK_MAGIC( pipe );

			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for 'prepend segment'\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			ins_pipept( pipe, es_pipept, new_pt );
		}
		break;
	case ECMD_PIPE_PT_DEL:
		{
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "No pipe segment selected\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			es_pipept = del_pipept( es_pipept );
		}
		break;
	case ECMD_ARS_PICK_MENU:
		/* put up point pick menu for ARS solid */
		menu_state->ms_flag = 0;
		es_edflag = ECMD_ARS_PICK;
		mmenu_set( MENU_L1, ars_pick_menu );
		break;
	case ECMD_ARS_EDIT_MENU:
		/* put up main ARS edit menu */
		menu_state->ms_flag = 0;
		es_edflag = IDLE;
		mmenu_set( MENU_L1, ars_menu );
		break;
	case ECMD_ARS_PICK:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			point_t pick_pt;
			vect_t view_dir;
			vect_t z_dir;
			struct bu_vls tmp_vls;
			point_t selected_pt;

			RT_ARS_CK_MAGIC( ars );

			if( es_mvalid )
				VMOVE( pick_pt, es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( pick_pt, es_invmat, es_para);
			  }else{
			    VMOVE( pick_pt, es_para );
			  }
#else
				VMOVE( pick_pt, es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
				Tcl_AppendResult(interp, "x y z coordinates required for 'pick point'\n", (char *)NULL);
				mged_print_result( TCL_ERROR );
				break;
			}
			else if( !es_mvalid && !inpara )
				break;

			/* Get view direction vector */
			VSET( z_dir, 0.0, 0.0, 1.0 );
			MAT4X3VEC(view_dir , view_state->vs_vop->vo_view2model , z_dir);
			find_nearest_ars_pt( &es_ars_crv, &es_ars_col, ars, pick_pt, view_dir );
			VMOVE( es_pt, &ars->curves[es_ars_crv][es_ars_col*3] );
			VSCALE( selected_pt, es_pt, base2local );
			bu_log( "Selected point #%d from curve #%d (%f %f %f)\n",
				 es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
			bu_vls_init( &tmp_vls );
			bu_vls_printf( &tmp_vls, "Selected point #%d from curve #%d ( %f %f %f )\n", es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL );
			mged_print_result( TCL_ERROR );
			bu_vls_free( &tmp_vls );
		}
		break;
	case ECMD_ARS_NEXT_PT:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			struct bu_vls tmp_vls;
			point_t selected_pt;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv >= 0 && es_ars_col >= 0 )
			{
				es_ars_col++;
				if( es_ars_col >= ars->pts_per_curve )
					es_ars_col = 0;
				VMOVE( es_pt, &ars->curves[es_ars_crv][es_ars_col*3] );
				VSCALE( selected_pt, es_pt, base2local );
				bu_log( "Selected point #%d from curve #%d (%f %f %f)\n",
					 es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				bu_vls_init( &tmp_vls );
				bu_vls_printf( &tmp_vls, "Selected point #%d from curve #%d ( %f %f %f )\n", es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL );
				mged_print_result( TCL_ERROR );
				bu_vls_free( &tmp_vls );
			}
		}
		break;
	case ECMD_ARS_PREV_PT:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			struct bu_vls tmp_vls;
			point_t selected_pt;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv >= 0 && es_ars_col >= 0 )
			{
				es_ars_col--;
				if( es_ars_col < 0 )
					es_ars_col = ars->pts_per_curve - 1;
				VMOVE( es_pt, &ars->curves[es_ars_crv][es_ars_col*3] );
				VSCALE( selected_pt, es_pt, base2local );
				bu_log( "Selected point #%d from curve #%d (%f %f %f)\n",
					 es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				bu_vls_init( &tmp_vls );
				bu_vls_printf( &tmp_vls, "Selected point #%d from curve #%d ( %f %f %f )\n", es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL );
				mged_print_result( TCL_ERROR );
				bu_vls_free( &tmp_vls );
			}
		}
		break;
	case ECMD_ARS_NEXT_CRV:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			struct bu_vls tmp_vls;
			point_t selected_pt;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv >= 0 && es_ars_col >= 0 )
			{
				es_ars_crv++;
				if(es_ars_crv >= ars->ncurves )
					es_ars_crv = 0;
				VMOVE( es_pt, &ars->curves[es_ars_crv][es_ars_col*3] );
				VSCALE( selected_pt, es_pt, base2local );
				bu_log( "Selected point #%d from curve #%d (%f %f %f)\n",
					 es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				bu_vls_init( &tmp_vls );
				bu_vls_printf( &tmp_vls, "Selected point #%d from curve #%d ( %f %f %f )\n", es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL );
				mged_print_result( TCL_ERROR );
				bu_vls_free( &tmp_vls );
			}
		}
		break;
	case ECMD_ARS_PREV_CRV:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			struct bu_vls tmp_vls;
			point_t selected_pt;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv >= 0 && es_ars_col >= 0 )
			{
				es_ars_crv--;
				if( es_ars_crv < 0 )
					es_ars_crv = ars->ncurves - 1;
				VMOVE( es_pt, &ars->curves[es_ars_crv][es_ars_col*3] );
				VSCALE( selected_pt, es_pt, base2local );
				bu_log( "Selected point #%d from curve #%d (%f %f %f)\n",
					 es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				bu_vls_init( &tmp_vls );
				bu_vls_printf( &tmp_vls, "Selected point #%d from curve #%d ( %f %f %f )\n", es_ars_col, es_ars_crv, V3ARGS( selected_pt ) );
				Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL );
				mged_print_result( TCL_ERROR );
				bu_vls_free( &tmp_vls );
			}
		}
		break;
	case ECMD_ARS_DUP_CRV:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			fastf_t **curves;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv < 0 || es_ars_col < 0 )
			{
				bu_log( "No ARS point selected\n" );
				break;
			}

			curves = (fastf_t **)bu_malloc( (ars->ncurves+1) * sizeof( fastf_t * ),
					"new curves" );

			for( i=0 ; i<ars->ncurves+1 ; i++ )
			{
				int j,k;

				curves[i] = (fastf_t *)bu_malloc( ars->pts_per_curve * 3 * sizeof( fastf_t ),
						"new curves[i]" );

				if( i <= es_ars_crv )
					k = i;
				else
					k = i - 1;

				for( j=0 ; j<ars->pts_per_curve*3 ; j++ )
					curves[i][j] = ars->curves[k][j];
			}

			for( i=0 ; i<ars->ncurves ; i++ )
				bu_free( (genptr_t)ars->curves[i], "ars->curves[i]" );
			bu_free( (genptr_t)ars->curves, "ars->curves" );

			ars->curves = curves;
			ars->ncurves++;
		}
		break;
	case ECMD_ARS_DUP_COL:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			fastf_t **curves;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv < 0 || es_ars_col < 0 )
			{
				bu_log( "No ARS point selected\n" );
				break;
			}

			curves = (fastf_t **)bu_malloc( ars->ncurves * sizeof( fastf_t * ),
					"new curves" );

			for( i=0 ; i<ars->ncurves ; i++ )
			{
				int j,k;

				curves[i] = (fastf_t *)bu_malloc( (ars->pts_per_curve + 1) * 3 * sizeof( fastf_t ),
						"new curves[i]" );

				for( j=0 ; j<ars->pts_per_curve+1 ; j++ )
				{
					if( j <= es_ars_col )
						k = j;
					else
						k = j - 1;

					curves[i][j*3] = ars->curves[i][k*3];
					curves[i][j*3+1] = ars->curves[i][k*3+1];
					curves[i][j*3+2] = ars->curves[i][k*3+2];
				}
			}

			for( i=0 ; i<ars->ncurves ; i++ )
				bu_free( (genptr_t)ars->curves[i], "ars->curves[i]" );
			bu_free( (genptr_t)ars->curves, "ars->curves" );

			ars->curves = curves;
			ars->pts_per_curve++;
		}
		break;
	case ECMD_ARS_DEL_CRV:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			fastf_t **curves;
			int k;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv < 0 || es_ars_col < 0 )
			{
				bu_log( "No ARS point selected\n" );
				break;
			}

			if( es_ars_crv == 0 || es_ars_crv == ars->ncurves-1 )
			{
				bu_log( "Cannot delete first or last curve\n" );
				break;
			}

			curves = (fastf_t **)bu_malloc( (ars->ncurves - 1) * sizeof( fastf_t * ),
					"new curves" );

			k = 0;
			for( i=0 ; i<ars->ncurves ; i++ )
			{
				int j;

				if( i == es_ars_crv )
					continue;

				curves[k] = (fastf_t *)bu_malloc( ars->pts_per_curve * 3 * sizeof( fastf_t ),
						"new curves[k]" );

				for( j=0 ; j<ars->pts_per_curve*3 ; j++ )
					curves[k][j] = ars->curves[i][j];

				k++;
			}

			for( i=0 ; i<ars->ncurves ; i++ )
				bu_free( (genptr_t)ars->curves[i], "ars->curves[i]" );
			bu_free( (genptr_t)ars->curves, "ars->curves" );

			ars->curves = curves;
			ars->ncurves--;

			if( es_ars_crv >= ars->ncurves )
				es_ars_crv = ars->ncurves - 1;
		}
		break;
	case ECMD_ARS_DEL_COL:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			fastf_t **curves;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv < 0 || es_ars_col < 0 )
			{
				bu_log( "No ARS point selected\n" );
				break;
			}

			if( es_ars_col == 0 || es_ars_col == ars->ncurves - 1 )
			{
				bu_log( "Cannot delete first or last column\n" );
				break;
			}

			if( ars->pts_per_curve < 3 )
			{
				bu_log( "Cannot create an ARS with less than two points per curve\n" );
				break;
			}

			curves = (fastf_t **)bu_malloc( ars->ncurves * sizeof( fastf_t * ),
					"new curves" );

			for( i=0 ; i<ars->ncurves ; i++ )
			{
				int j,k;


				curves[i] = (fastf_t *)bu_malloc( (ars->pts_per_curve - 1) * 3 * sizeof( fastf_t ),
						"new curves[i]" );

				k = 0;
				for( j=0 ; j<ars->pts_per_curve ; j++ )
				{
					if( j == es_ars_col )
						continue;

					curves[i][k*3] = ars->curves[i][j*3];
					curves[i][k*3+1] = ars->curves[i][j*3+1];
					curves[i][k*3+2] = ars->curves[i][j*3+2];
					k++;
				}
			}

			for( i=0 ; i<ars->ncurves ; i++ )
				bu_free( (genptr_t)ars->curves[i], "ars->curves[i]" );
			bu_free( (genptr_t)ars->curves, "ars->curves" );

			ars->curves = curves;
			ars->pts_per_curve--;

			if( es_ars_col >= ars->pts_per_curve )
				es_ars_col = ars->pts_per_curve - 1;
		}
		break;
	case ECMD_ARS_MOVE_COL:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			point_t new_pt;
			vect_t diff;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv < 0 || es_ars_col < 0 )
			{
				bu_log( "No ARS point selected\n" );
				break;
			}

			if( es_mvalid )
			{
				vect_t view_dir;
				plane_t view_pl;
				fastf_t dist;

				/* construct a plane perpendiculr to view direction
				 * that passes through ARS point being moved
				 */
				VSET( view_dir, 0.0, 0.0, 1.0 );
				MAT4X3VEC(view_pl, view_state->vs_vop->vo_view2model, view_dir);
				VUNITIZE( view_pl );
				view_pl[3] = VDOT( view_pl, &ars->curves[es_ars_crv][es_ars_col*3] );

				/* project es_mparam onto the plane */
				dist = DIST_PT_PLANE( es_mparam, view_pl );
				VJOIN1( new_pt, es_mparam, -dist, view_pl );
			}
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for point movement\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			VSUB2( diff, new_pt, &ars->curves[es_ars_crv][es_ars_col*3] );

			for( i=0 ; i<ars->ncurves ; i++ )
				VADD2( &ars->curves[i][es_ars_col*3],
					&ars->curves[i][es_ars_col*3], diff );

		}
		break;
	case ECMD_ARS_MOVE_CRV:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			point_t new_pt;
			vect_t diff;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv < 0 || es_ars_col < 0 )
			{
				bu_log( "No ARS point selected\n" );
				break;
			}

			if( es_mvalid )
			{
				vect_t view_dir;
				plane_t view_pl;
				fastf_t dist;

				/* construct a plane perpendiculr to view direction
				 * that passes through ARS point being moved
				 */
				VSET( view_dir, 0.0, 0.0, 1.0 );
				MAT4X3VEC(view_pl, view_state->vs_vop->vo_view2model, view_dir);
				VUNITIZE( view_pl );
				view_pl[3] = VDOT( view_pl, &ars->curves[es_ars_crv][es_ars_col*3] );

				/* project es_mparam onto the plane */
				dist = DIST_PT_PLANE( es_mparam, view_pl );
				VJOIN1( new_pt, es_mparam, -dist, view_pl );
			}
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for point movement\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			VSUB2( diff, new_pt, &ars->curves[es_ars_crv][es_ars_col*3] );

			for( i=0 ; i<ars->pts_per_curve ; i++ )
				VADD2( &ars->curves[es_ars_crv][i*3],
					&ars->curves[es_ars_crv][i*3], diff );

		}
		break;
	case ECMD_ARS_MOVE_PT:
		{
			struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;
			point_t new_pt;

			RT_ARS_CK_MAGIC( ars );

			if( es_ars_crv < 0 || es_ars_col < 0 )
			{
				bu_log( "No ARS point selected\n" );
				break;
			}

			if( es_mvalid )
			{
				vect_t view_dir;
				plane_t view_pl;
				fastf_t dist;

				/* construct a plane perpendiculr to view direction
				 * that passes through ARS point being moved
				 */
				VSET( view_dir, 0.0, 0.0, 1.0 );
				MAT4X3VEC(view_pl, view_state->vs_vop->vo_view2model, view_dir);
				VUNITIZE( view_pl );
				view_pl[3] = VDOT( view_pl, &ars->curves[es_ars_crv][es_ars_col*3] );

				/* project es_mparam onto the plane */
				dist = DIST_PT_PLANE( es_mparam, view_pl );
				VJOIN1( new_pt, es_mparam, -dist, view_pl );
			}
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for point movement\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			VMOVE( &ars->curves[es_ars_crv][es_ars_col*3] , new_pt );
		}
		break;
	case ECMD_BOT_MOVEV:
		{
			struct rt_bot_internal *bot = (struct rt_bot_internal *)es_int.idb_ptr;
			int vert;
			point_t new_pt;

			RT_BOT_CK_MAGIC( bot );

			if( bot_verts[0] < 0 )
			{
				bu_log( "No BOT point selected\n" );
				break;
			}

			if( bot_verts[1] >= 0 && bot_verts[2] >= 0 )
			{
				bu_log( "A triangle is selected, not a BOT point!!!\n" );
				break;
			}

			if( bot_verts[1] >= 0 )
			{
				bu_log( "An edge is selected, not a BOT point!!!\n" );
				break;
			}

			vert = bot_verts[0];
			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for point movement\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			VMOVE( &bot->vertices[vert*3] , new_pt );
		}
		break;
	case ECMD_BOT_MOVEE:
		{
			struct rt_bot_internal *bot = (struct rt_bot_internal *)es_int.idb_ptr;
			int v1, v2;
			vect_t diff;
			point_t new_pt;

			RT_BOT_CK_MAGIC( bot );

			if( bot_verts[0] < 0 || bot_verts[1] < 0 )
			{
				Tcl_AppendResult( interp, "No BOT edge selected\n", (char *)NULL );
				mged_print_result( TCL_ERROR );
				break;
			}

			if( bot_verts[2] >= 0 )
			{
				bu_log( "A triangle is selected, not a BOT edge!!!\n" );
				break;
			}
			v1 = bot_verts[0];
			v2 = bot_verts[1];
			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for point movement\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;



			VSUB2( diff, new_pt, &bot->vertices[v1*3] );
			VMOVE( &bot->vertices[v1*3] , new_pt );
			VADD2( &bot->vertices[v2*3], &bot->vertices[v2*3], diff );
		}
		break;
	case ECMD_BOT_MOVET:
		{
			struct rt_bot_internal *bot = (struct rt_bot_internal *)es_int.idb_ptr;
			int v1, v2, v3;
			point_t new_pt;
			vect_t diff;

			RT_BOT_CK_MAGIC( bot );

			if( bot_verts[0] < 0 ||
				bot_verts[1] < 0 ||
				bot_verts[2] < 0 )
			{
				Tcl_AppendResult( interp, "No BOT triangle selected\n", (char *)NULL );
				mged_print_result( TCL_ERROR );
				break;
			}
			v1 = bot_verts[0];
			v2 = bot_verts[1];
			v3 = bot_verts[2];

			if( es_mvalid )
				VMOVE( new_pt , es_mparam )
			else if( inpara == 3 ){
#ifdef TRY_EDIT_NEW_WAY
			  if(mged_variables->mv_context){
			    /* apply es_invmat to convert to real model space */
			    MAT4X3PNT( new_pt, es_invmat, es_para);
			  }else{
			    VMOVE( new_pt , es_para );
			  }
#else
				VMOVE( new_pt , es_para )
#endif
			}
			else if( inpara && inpara != 3 )
			{
			  Tcl_AppendResult(interp, "x y z coordinates required for point movement\n", (char *)NULL);
			  mged_print_result( TCL_ERROR );
			  break;
			}
			else if( !es_mvalid && !inpara )
				break;

			VSUB2( diff, new_pt, &bot->vertices[v1*3] );
			VMOVE( &bot->vertices[v1*3] , new_pt );
			VADD2( &bot->vertices[v2*3], &bot->vertices[v2*3], diff );
			VADD2( &bot->vertices[v3*3], &bot->vertices[v3*3], diff );
		}
		break;
	case ECMD_BOT_PICKV:
	case ECMD_BOT_PICKE:
	case ECMD_BOT_PICKT:
		break;
	default:
	  {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "sedit():  unknown edflag = %d.\n", es_edflag );
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    mged_print_result( TCL_ERROR );
	    bu_vls_free(&tmp_vls);
	  }
	}

	/* must re-calculate the face plane equations for arbs */
	if( es_int.idb_type == ID_ARB8 )
	{
		arb = (struct rt_arb_internal *)es_int.idb_ptr;
		RT_ARB_CK_MAGIC( arb );

		(void)rt_arb_calc_planes( es_peqn , arb , es_type , &mged_tol );
	}

	/* If the keypoint changed location, find about it here */
	if (!es_keyfixed)
		get_solid_keypoint( es_keypoint, &es_keytag, &es_int, es_mat );

	set_e_axes_pos(0);
	replot_editing_solid();

	if(update_views){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "active_edit_callback");
	  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	}

	inpara = 0;
	es_mvalid = 0;
}

/*
 *			S E D I T _ M O U S E
 *
 *  Mouse (pen) press in graphics area while doing Solid Edit.
 *  mousevec [X] and [Y] are in the range -1.0...+1.0, corresponding
 *  to viewspace.
 *
 *  In order to allow the "p" command to do the same things that
 *  a mouse event can, the preferred strategy is to store the value
 *  corresponding to what the "p" command would give in es_mparam,
 *  set es_mvalid=1, set sedraw=1, and return, allowing sedit()
 *  to actually do the work.
 */
void
sedit_mouse( const vect_t mousevec )
{
  vect_t pos_view;	 	/* Unrotated view space pos */
  vect_t pos_model;		/* Rotated screen space pos */
  vect_t tr_temp;		/* temp translation vector */
  vect_t temp;
  vect_t raw_kp;                /* es_keypoint with es_invmat applied */
  vect_t raw_mp;                /* raw model position */


  if( es_edflag <= 0 )
    return;

  switch( es_edflag )  {
  case SSCALE:
  case PSCALE:
  case ECMD_DSP_SCALE_X:
  case ECMD_DSP_SCALE_Y:
  case ECMD_DSP_SCALE_ALT:
  case ECMD_VOL_CSIZE:
  case ECMD_VOL_THRESH_LO:
  case ECMD_VOL_THRESH_HI:
  case ECMD_EBM_HEIGHT:
  case ECMD_EXTR_SCALE_H:
  case ECMD_CLINE_SCALE_H:
  case ECMD_CLINE_SCALE_T:
  case ECMD_CLINE_SCALE_R:
    /* use mouse to get a scale factor */
    es_scale = 1.0 + 0.25 * ((fastf_t)
			     (mousevec[Y] > 0 ? mousevec[Y] : -mousevec[Y]));
    if ( mousevec[Y] <= 0 )
      es_scale = 1.0 / es_scale;

    /* accumulate scale factor */
    acc_sc_sol *= es_scale;

    edit_absolute_scale = acc_sc_sol - 1.0;
    if(edit_absolute_scale > 0)
      edit_absolute_scale /= 3.0;

    sedit();

    return;
  case STRANS:
    /*
     * Use mouse to change solid's location.
     * Project solid's keypoint into view space,
     * replace X,Y (but NOT Z) components, and
     * project result back to model space.
     * Then move keypoint there.
     */
    {
      point_t	pt;
      vect_t	delta;
      mat_t	xlatemat;

      MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
      pos_view[X] = mousevec[X];
      pos_view[Y] = mousevec[Y];
      MAT4X3PNT(pt, view_state->vs_vop->vo_view2model, pos_view);

      /* Need vector from current vertex/keypoint
       * to desired new location.
       */
      MAT4X3PNT( raw_mp, es_invmat, pt );
      MAT4X3PNT( raw_kp, es_invmat, curr_e_axes_pos );
      VSUB2( delta, raw_kp, raw_mp );
      MAT_IDN( xlatemat );
      MAT_DELTAS_VEC_NEG( xlatemat, delta );
      transform_editing_solid(&es_int, xlatemat, &es_int, 1);
    }

    break;
  case ECMD_VTRANS:
    /*
     * Use mouse to change a vertex location.
     * Project vertex (in solid keypoint) into view space,
     * replace X,Y (but NOT Z) components, and
     * project result back to model space.
     * Leave desired location in es_mparam.
     */

    MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
    MAT4X3PNT( es_mparam, es_invmat, temp );
    es_mvalid = 1;	/* es_mparam is valid */
    /* Leave the rest to code in sedit() */

    break;
  case ECMD_TGC_MV_H:
  case ECMD_TGC_MV_HH:
    /* Use mouse to change location of point V+H */
    {
      struct rt_tgc_internal	*tgc =
	(struct rt_tgc_internal *)es_int.idb_ptr;
      RT_TGC_CK_MAGIC(tgc);

      MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
      pos_view[X] = mousevec[X];
      pos_view[Y] = mousevec[Y];
      /* Do NOT change pos_view[Z] ! */
      MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
      MAT4X3PNT( tr_temp, es_invmat, temp );
      VSUB2( tgc->h, tr_temp, tgc->v );
    }

    break;
  case ECMD_EXTR_MOV_H:
    /* Use mouse to change location of point V+H */
    {
      struct rt_extrude_internal	*extr =
	(struct rt_extrude_internal *)es_int.idb_ptr;
      RT_EXTRUDE_CK_MAGIC(extr);

      MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
      pos_view[X] = mousevec[X];
      pos_view[Y] = mousevec[Y];
      /* Do NOT change pos_view[Z] ! */
      MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
      MAT4X3PNT( tr_temp, es_invmat, temp );
      VSUB2( extr->h, tr_temp, extr->V );
    }

    break;
  case ECMD_CLINE_MOVE_H:
    {
    	struct rt_cline_internal *cli =
    		(struct rt_cline_internal *)es_int.idb_ptr;

    	RT_CLINE_CK_MAGIC( cli );

      MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
      pos_view[X] = mousevec[X];
      pos_view[Y] = mousevec[Y];
      /* Do NOT change pos_view[Z] ! */
      MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
      MAT4X3PNT( tr_temp, es_invmat, temp );
      VSUB2( cli->h, tr_temp, cli->v );
    }

    break;
  case PTARB:
    /* move an arb point to indicated point */
    /* point is located at es_values[es_menu*3] */
    MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
    MAT4X3PNT(pos_model, es_invmat, temp);
    editarb( pos_model );

    break;
  case EARB:
    MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
    MAT4X3PNT(pos_model, es_invmat, temp);
    editarb( pos_model );

    break;
  case ECMD_ARB_MOVE_FACE:
    MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
    MAT4X3PNT(pos_model, es_invmat, temp);
    /* change D of planar equation */
    es_peqn[es_menu][3]=VDOT(&es_peqn[es_menu][0], pos_model);
    /* calculate new vertices, put in record as vectors */
    {
      struct rt_arb_internal *arb=
	(struct rt_arb_internal *)es_int.idb_ptr;

      RT_ARB_CK_MAGIC( arb );
      (void)rt_arb_calc_points( arb , es_type , es_peqn , &mged_tol );
    }

    break;
  case ECMD_BOT_PICKV:
    {
  	struct rt_bot_internal *bot = (struct rt_bot_internal *)es_int.idb_ptr;
  	int tmp_vert;
    	char tmp_msg[256];

  	RT_BOT_CK_MAGIC( bot );

	MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
	pos_view[X] = mousevec[X];
	pos_view[Y] = mousevec[Y];

  	tmp_vert = rt_bot_find_v_nearest_pt2(bot, pos_view, view_state->vs_vop->vo_model2view);
  	if( tmp_vert < 0 )
  	{
  		Tcl_AppendResult(interp, "ECMD_BOT_PICKV: unable to find a vertex!!!\n", (char *)NULL );
  		mged_print_result( TCL_ERROR );
  		return;
  	}

  	bot_verts[0] = tmp_vert;
  	bot_verts[1] = -1;
  	bot_verts[2] = -1;
	sprintf( tmp_msg, "picked point at (%g %g %g), vertex #%d\n", V3ARGS( &bot->vertices[tmp_vert*3] ), tmp_vert );
    	Tcl_AppendResult(interp, tmp_msg, (char *)NULL );
	mged_print_result( TCL_OK );
    }
    break;
  case ECMD_BOT_PICKE:
    {
  	struct rt_bot_internal *bot = (struct rt_bot_internal *)es_int.idb_ptr;
    	int vert1, vert2;
    	char tmp_msg[256];

    	RT_BOT_CK_MAGIC( bot );

	MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
	pos_view[X] = mousevec[X];
	pos_view[Y] = mousevec[Y];

    	if (rt_bot_find_e_nearest_pt2(&vert1, &vert2, bot, pos_view, view_state->vs_vop->vo_model2view))
  	{
  		Tcl_AppendResult(interp, "ECMD_BOT_PICKE: unable to find an edge!!!\n", (char *)NULL );
  		mged_print_result( TCL_ERROR );
  		return;
  	}

  	bot_verts[0] = vert1;
  	bot_verts[1] = vert2;
  	bot_verts[2] = -1;
	sprintf( tmp_msg, "picked edge from (%g %g %g) to (%g %g %g)\n", V3ARGS( &bot->vertices[vert1*3] ), V3ARGS( &bot->vertices[vert2*3] ) );
    	Tcl_AppendResult(interp, tmp_msg, (char *)NULL );
	mged_print_result( TCL_OK );
    }
    break;
  case ECMD_BOT_PICKT:
  	{
		struct rt_bot_internal *bot = (struct rt_bot_internal *)es_int.idb_ptr;
  		point_t start_pt, tmp;
  		vect_t dir;
  		int i, hits, ret_tcl;
  		int v1, v2, v3;
  		point_t pt1, pt2, pt3;
		struct bu_vls vls;

  		RT_BOT_CK_MAGIC( bot );

		bu_vls_init( &vls );

  		VSET( tmp, mousevec[X], mousevec[Y], 0.0 );
  		MAT4X3PNT(start_pt, view_state->vs_vop->vo_view2model, tmp);
  		VSET(tmp, 0, 0, 1 );
  		MAT4X3VEC(dir, view_state->vs_vop->vo_view2model, tmp);

		bu_vls_strcat( &vls, " {" );
		hits = 0;
  		for( i=0 ; i<bot->num_faces ; i++ )
  		{
  			v1 = bot->faces[i*3];
  			v2 = bot->faces[i*3+1];
  			v3 = bot->faces[i*3+2];
  			VMOVE( pt1, &bot->vertices[v1*3] );
  			VMOVE( pt2, &bot->vertices[v2*3] );
  			VMOVE( pt3, &bot->vertices[v3*3] );

  			if( bn_does_ray_isect_tri(start_pt, dir, pt1, pt2, pt3, tmp ) )
			  {
			    hits++;
			    bu_vls_printf( &vls, " { %d %d %d }", v1, v2, v3 );
			  }
  		}
		bu_vls_strcat( &vls, " } " );

		if( hits == 0 )
  		{
	  		bot_verts[0] = -1;
	  		bot_verts[1] = -1;
	  		bot_verts[2] = -1;
			bu_vls_free( &vls );
  		}
  		if( hits == 1 )
  		{
  			(void)sscanf( bu_vls_addr( &vls ), " { { %d %d %d", &bot_verts[0], &bot_verts[1], &bot_verts[2] );
			bu_vls_free( &vls );
  		}
		else
		{
			Tcl_LinkVar( interp, "bot_v1", (char *)&bot_verts[0], TCL_LINK_INT );
			Tcl_LinkVar( interp, "bot_v2", (char *)&bot_verts[1], TCL_LINK_INT );
			Tcl_LinkVar( interp, "bot_v3", (char *)&bot_verts[2], TCL_LINK_INT );

			ret_tcl = Tcl_VarEval( interp, "bot_face_select ", bu_vls_addr( &vls ), (char *)NULL );
			bu_vls_free( &vls );
			if( ret_tcl != TCL_OK )
			{
			  bu_log( "bot_face_select failed: %s\n", interp->result );
			  bot_verts[0] = -1;
			  bot_verts[1] = -1;
			  bot_verts[2] = -1;
			  break;
			}
		}
  	}
  	break;
  case ECMD_NMG_EPICK:
    /* XXX Should just leave desired location in es_mparam for sedit() */
    {
      struct model	*m =
	(struct model *)es_int.idb_ptr;
      struct edge	*e;
      struct bn_tol	tmp_tol;
      NMG_CK_MODEL(m);

      /* Picking an edge should not depend on tolerances! */
      tmp_tol.magic = BN_TOL_MAGIC;
      tmp_tol.dist = 0.0;
      tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
      tmp_tol.perp = 0.0;
      tmp_tol.para = 1 - tmp_tol.perp;

      MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
      pos_view[X] = mousevec[X];
      pos_view[Y] = mousevec[Y];
      if ((e = nmg_find_e_nearest_pt2(&m->magic, pos_view,
				      view_state->vs_vop->vo_model2view, &tmp_tol)) == (struct edge *)NULL) {
	Tcl_AppendResult(interp, "ECMD_NMG_EPICK: unable to find an edge\n",
			 (char *)NULL);
	mged_print_result( TCL_ERROR );
	return;
      }
      es_eu = e->eu_p;
      NMG_CK_EDGEUSE(es_eu);

      {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls,
		      "edgeuse selected=x%x (%g %g %g) <-> (%g %g %g)\n",
		      es_eu, V3ARGS( es_eu->vu_p->v_p->vg_p->coord ),
		      V3ARGS( es_eu->eumate_p->vu_p->v_p->vg_p->coord ) );
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	mged_print_result( TCL_ERROR );
	bu_vls_free(&tmp_vls);
      }
  }

    break;
  case ECMD_NMG_LEXTRU:
  case ECMD_NMG_EMOVE:
  case ECMD_NMG_ESPLIT:
  case ECMD_PIPE_PICK:
  case ECMD_PIPE_SPLIT:
  case ECMD_PIPE_PT_MOVE:
  case ECMD_PIPE_PT_ADD:
  case ECMD_PIPE_PT_INS:
  case ECMD_ARS_PICK:
  case ECMD_ARS_MOVE_PT:
  case ECMD_ARS_MOVE_CRV:
  case ECMD_ARS_MOVE_COL:
  case ECMD_BOT_MOVEV:
  case ECMD_BOT_MOVEE:
  case ECMD_BOT_MOVET:

    MAT4X3PNT(pos_view, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, pos_view);
    MAT4X3PNT(es_mparam, es_invmat, temp);
    es_mvalid = 1;

    break;
  default:
    Tcl_AppendResult(interp, "mouse press undefined in this solid edit mode\n", (char *)NULL);
    mged_print_result( TCL_ERROR );
    return;
    }

  update_edit_absolute_tran(pos_view);
  sedit();
}

void
update_edit_absolute_tran(vect_t view_pos)
{
	vect_t model_pos;
	vect_t ea_view_pos;
	vect_t diff;
	fastf_t inv_Viewscale = 1/view_state->vs_vop->vo_scale;

	MAT4X3PNT(model_pos, view_state->vs_vop->vo_view2model, view_pos);
	VSUB2(diff, model_pos, e_axes_pos);
	VSCALE(edit_absolute_model_tran, diff, inv_Viewscale);
	VMOVE(last_edit_absolute_model_tran, edit_absolute_model_tran);

	MAT4X3PNT(ea_view_pos, view_state->vs_vop->vo_model2view, e_axes_pos);
	VSUB2(edit_absolute_view_tran, view_pos, ea_view_pos);
	VMOVE(last_edit_absolute_view_tran, edit_absolute_view_tran);
}

void
sedit_trans(fastf_t *tvec)
{
  vect_t temp;
  vect_t raw_kp;
  vect_t pos_model;

  if( es_edflag <= 0 )
    return;

  switch( es_edflag ) {
  case STRANS:
    /*
     * Use mouse to change solid's location.
     * Project solid's keypoint into view space,
     * replace X,Y and Z components, and
     * project result back to model space.
     * Then move keypoint there.
     */
    {
      point_t	pt;
      vect_t	delta;
      mat_t	xlatemat;

      MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
      MAT4X3PNT( pt, es_invmat, temp );
      MAT4X3PNT( raw_kp, es_invmat, es_keypoint );

      /* Need vector from current vertex/keypoint
       * to desired new location.
       */
      VSUB2( delta, raw_kp, pt );
      MAT_IDN( xlatemat );
      MAT_DELTAS_VEC_NEG( xlatemat, delta );
      transform_editing_solid(&es_int, xlatemat, &es_int, 1);
    }

    break;
  case ECMD_VTRANS:
    /*
     * Use mouse to change a vertex location.
     * Project vertex (in solid keypoint) into view space,
     * replace X,Y and Z components, and
     * project result back to model space.
     * Leave desired location in es_mparam.
     */
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
    MAT4X3PNT( es_mparam, es_invmat, temp );
    es_mvalid = 1;	/* es_mparam is valid */
    /* Leave the rest to code in sedit() */

    break;
  case ECMD_TGC_MV_H:
  case ECMD_TGC_MV_HH:
    /* Use mouse to change location of point V+H */
    {
      vect_t tr_temp;
      struct rt_tgc_internal	*tgc =
	(struct rt_tgc_internal *)es_int.idb_ptr;
      RT_TGC_CK_MAGIC(tgc);

      MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
      MAT4X3PNT( tr_temp, es_invmat, temp );
      VSUB2( tgc->h, tr_temp, tgc->v );
    }

    break;
  case ECMD_EXTR_MOV_H:
    /* Use mouse to change location of point V+H */
    {
      vect_t tr_temp;
      struct rt_extrude_internal	*extr =
	(struct rt_extrude_internal *)es_int.idb_ptr;
      RT_EXTRUDE_CK_MAGIC(extr);

      MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
      MAT4X3PNT( tr_temp, es_invmat, temp );
      VSUB2( extr->h, tr_temp, extr->V );
    }

    break;
  case ECMD_CLINE_MOVE_H:
    /* Use mouse to change location of point V+H */
    {
      vect_t tr_temp;
      struct rt_cline_internal	*cli =
	(struct rt_cline_internal *)es_int.idb_ptr;
      RT_CLINE_CK_MAGIC(cli);

      MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
      MAT4X3PNT( tr_temp, es_invmat, temp );
      VSUB2( cli->h, tr_temp, cli->v );
    }

    break;
  case PTARB:
    /* move an arb point to indicated point */
    /* point is located at es_values[es_menu*3] */
    {
      struct rt_arb_internal *arb=
	(struct rt_arb_internal *)es_int.idb_ptr;
      RT_ARB_CK_MAGIC( arb );

      VMOVE( temp , arb->pt[es_menu] );
    }

    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
    MAT4X3PNT(pos_model, es_invmat, temp);
    editarb( pos_model );

    break;
  case EARB:
    /* move arb edge, through indicated point */
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
    /* apply inverse of es_mat */
    MAT4X3PNT( pos_model, es_invmat, temp );
    editarb( pos_model );

    break;
  case ECMD_ARB_MOVE_FACE:
    /* move arb face, through  indicated  point */
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);

    /* apply inverse of es_mat */
    MAT4X3PNT( pos_model, es_invmat, temp );

    /* change D of planar equation */
    es_peqn[es_menu][3]=VDOT(&es_peqn[es_menu][0], pos_model);

    /* calculate new vertices, put in record as vectors */
    {
      struct rt_arb_internal *arb=
	(struct rt_arb_internal *)es_int.idb_ptr;

      RT_ARB_CK_MAGIC( arb );
      (void)rt_arb_calc_points( arb , es_type , es_peqn , &mged_tol );
    }

    break;
  case ECMD_NMG_EPICK:
    /* XXX Should just leave desired location in es_mparam for sedit() */
    {
      struct model	*m =
	(struct model *)es_int.idb_ptr;
      struct edge	*e;
      struct bn_tol	tmp_tol;
      NMG_CK_MODEL(m);

      /* Picking an edge should not depend on tolerances! */
      tmp_tol.magic = BN_TOL_MAGIC;
      tmp_tol.dist = 0.0;
      tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
      tmp_tol.perp = 0.0;
      tmp_tol.para = 1 - tmp_tol.perp;

      if ((e = nmg_find_e_nearest_pt2(&m->magic,
				      tvec,
				      view_state->vs_vop->vo_model2view,
				      &tmp_tol)) == (struct edge *)NULL) {
	Tcl_AppendResult(interp,
			 "ECMD_NMG_EPICK: unable to find an edge\n",
			 (char *)NULL);
	sedraw = 0;
	return;
      }
      es_eu = e->eu_p;
      NMG_CK_EDGEUSE(es_eu);

      {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls,
		      "edgeuse selected=x%x (%g %g %g) <-> (%g %g %g)\n",
		      es_eu, V3ARGS( es_eu->vu_p->v_p->vg_p->coord ),
		      V3ARGS( es_eu->eumate_p->vu_p->v_p->vg_p->coord ) );
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
      }
    }

    break;
  case ECMD_NMG_LEXTRU:
  case ECMD_NMG_EMOVE:
  case ECMD_NMG_ESPLIT:
  case ECMD_PIPE_PICK:
  case ECMD_PIPE_SPLIT:
  case ECMD_PIPE_PT_MOVE:
  case ECMD_PIPE_PT_ADD:
  case ECMD_PIPE_PT_INS:
  case ECMD_ARS_PICK:
  case ECMD_ARS_MOVE_PT:
  case ECMD_ARS_MOVE_CRV:
  case ECMD_ARS_MOVE_COL:
    MAT4X3PNT(temp, view_state->vs_vop->vo_view2model, tvec);
    /* apply inverse of es_mat */
    MAT4X3PNT( es_mparam, es_invmat, temp );
    es_mvalid = 1;

    break;
  default:
    Tcl_AppendResult(interp,
		     "mouse press undefined in this solid edit mode\n",
		     (char *)NULL);
    sedraw = 0;
    return;
  }

  sedit();
}

void
sedit_abs_scale(void)
{
  fastf_t old_acc_sc_sol;

  if( es_edflag != SSCALE && es_edflag != PSCALE )
    return;

  old_acc_sc_sol = acc_sc_sol;

  if(-SMALL_FASTF < edit_absolute_scale && edit_absolute_scale < SMALL_FASTF)
    acc_sc_sol = 1.0;
  else if(edit_absolute_scale > 0.0)
    acc_sc_sol = 1.0 + edit_absolute_scale * 3.0;
  else{
    if((edit_absolute_scale - MGED_SMALL_SCALE) < -1.0)
      edit_absolute_scale = -1.0 + MGED_SMALL_SCALE;

    acc_sc_sol = 1.0 + edit_absolute_scale;
  }

  es_scale = acc_sc_sol / old_acc_sc_sol;
  sedit();
}


/*
 *  Object Edit
 */
void
objedit_mouse( const vect_t mousevec )
{
  fastf_t			scale;
  vect_t	pos_view;	 	/* Unrotated view space pos */
  vect_t	pos_model;	/* Rotated screen space pos */
  vect_t	tr_temp;		/* temp translation vector */
  vect_t	temp;

  MAT_IDN( incr_change );
  scale = 1;
  if( movedir & SARROW )  {
    /* scaling option is in effect */
    scale = 1.0 + (fastf_t)(mousevec[Y]>0 ?
			    mousevec[Y] : -mousevec[Y]);
    if ( mousevec[Y] <= 0 )
      scale = 1.0 / scale;

    /* switch depending on scaling option selected */
    switch( edobj ) {

    case BE_O_SCALE:
      /* global scaling */
      incr_change[15] = 1.0 / scale;

      acc_sc_obj /= incr_change[15];
      edit_absolute_scale = acc_sc_obj - 1.0;
      if(edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
      break;

    case BE_O_XSCALE:
      /* local scaling ... X-axis */
      incr_change[0] = scale;
      /* accumulate the scale factor */
      acc_sc[0] *= scale;
      edit_absolute_scale = acc_sc[0] - 1.0;
      if(edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
      break;

    case BE_O_YSCALE:
      /* local scaling ... Y-axis */
      incr_change[5] = scale;
      /* accumulate the scale factor */
      acc_sc[1] *= scale;
      edit_absolute_scale = acc_sc[1] - 1.0;
      if(edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
      break;

    case BE_O_ZSCALE:
      /* local scaling ... Z-axis */
      incr_change[10] = scale;
      /* accumulate the scale factor */
      acc_sc[2] *= scale;
      edit_absolute_scale = acc_sc[2] - 1.0;
      if(edit_absolute_scale > 0.0)
	edit_absolute_scale /= 3.0;
      break;
    }

    /* Have scaling take place with respect to keypoint,
     * NOT the view center.
     */
#if 0
    MAT4X3PNT(temp, es_mat, es_keypoint);
#else
    VMOVE(temp, es_keypoint);
#endif
    MAT4X3PNT(pos_model, modelchanges, temp);
    wrt_point(modelchanges, incr_change, modelchanges, pos_model);

    MAT_IDN( incr_change );
    new_edit_mats();
  }  else if( movedir & (RARROW|UARROW) )  {
    mat_t oldchanges;	/* temporary matrix */

    /* Vector from object keypoint to cursor */
#if 0
    MAT4X3PNT( temp, es_mat, es_keypoint );
#else
    VMOVE(temp, es_keypoint);
#endif
    MAT4X3PNT( pos_view, view_state->vs_model2objview, temp );

    if( movedir & RARROW )
      pos_view[X] = mousevec[X];
    if( movedir & UARROW )
      pos_view[Y] = mousevec[Y];

    MAT4X3PNT(pos_model, view_state->vs_vop->vo_view2model, pos_view);/* NOT objview */
    MAT4X3PNT( tr_temp, modelchanges, temp );
    VSUB2( tr_temp, pos_model, tr_temp );
    MAT_DELTAS(incr_change,
	       tr_temp[X], tr_temp[Y], tr_temp[Z]);
    MAT_COPY( oldchanges, modelchanges );
    bn_mat_mul( modelchanges, incr_change, oldchanges );

    MAT_IDN( incr_change );
    new_edit_mats();

    update_edit_absolute_tran(pos_view);
  }  else  {
    Tcl_AppendResult(interp, "No object edit mode selected;  mouse press ignored\n", (char *)NULL);
    return;
  }
}


void
oedit_trans(fastf_t *tvec)
{
  point_t  pos_model;
  point_t  tr_temp;
  point_t  temp;
  mat_t incr_mat;
  mat_t oldchanges;	/* temporary matrix */

  MAT_IDN( incr_mat );
#if 0
  MAT4X3PNT( temp, es_mat, es_keypoint );
#else
  VMOVE(temp, es_keypoint);
#endif
  MAT4X3PNT( pos_model, view_state->vs_vop->vo_view2model, tvec );/* NOT objview */
  MAT4X3PNT( tr_temp, modelchanges, temp );
  VSUB2( tr_temp, pos_model, tr_temp );
  MAT_DELTAS(incr_mat,
	     tr_temp[X], tr_temp[Y], tr_temp[Z]);
  MAT_COPY( oldchanges, modelchanges );
  bn_mat_mul( modelchanges, incr_mat, oldchanges );

  new_edit_mats();
}


void
oedit_abs_scale(void)
{
  fastf_t scale;
  vect_t temp;
  vect_t pos_model;
  mat_t incr_mat;

  MAT_IDN( incr_mat );

  if(-SMALL_FASTF < edit_absolute_scale && edit_absolute_scale < SMALL_FASTF)
    scale = 1;
  else if(edit_absolute_scale > 0.0)
    scale = 1.0 + edit_absolute_scale * 3.0;
  else{
    if((edit_absolute_scale - MGED_SMALL_SCALE) < -1.0)
      edit_absolute_scale = -1.0 + MGED_SMALL_SCALE;

    scale = 1.0 + edit_absolute_scale;
  }

  /* switch depending on scaling option selected */
  switch( edobj ) {

  case BE_O_SCALE:
    /* global scaling */
    incr_mat[15] = acc_sc_obj / scale;
    acc_sc_obj = scale;
    break;

  case BE_O_XSCALE:
    /* local scaling ... X-axis */
    incr_mat[0] = scale / acc_sc[0];
    /* accumulate the scale factor */
    acc_sc[0] = scale;
    break;

  case BE_O_YSCALE:
    /* local scaling ... Y-axis */
    incr_mat[5] = scale / acc_sc[1];
    /* accumulate the scale factor */
    acc_sc[1] = scale;
    break;

  case BE_O_ZSCALE:
    /* local scaling ... Z-axis */
    incr_mat[10] = scale / acc_sc[2];
    /* accumulate the scale factor */
    acc_sc[2] = scale;
    break;
  }

  /* Have scaling take place with respect to keypoint,
   * NOT the view center.
   */
#if 0
  MAT4X3PNT(temp, es_mat, es_keypoint);
#else
  VMOVE(temp, es_keypoint);
#endif
  MAT4X3PNT(pos_model, modelchanges, temp);
  wrt_point(modelchanges, incr_mat, modelchanges, pos_model);

  new_edit_mats();
}


/*
 *			V L S _ S O L I D
 */
void
vls_solid( struct bu_vls *vp, const struct rt_db_internal *ip, const mat_t mat )
{
	struct rt_db_internal	intern;
	int			id;

	if(dbip == DBI_NULL)
	  return;

	BU_CK_VLS(vp);
	RT_CK_DB_INTERNAL(ip);

	id = ip->idb_type;
	transform_editing_solid( &intern, mat, (struct rt_db_internal *)ip, 0 );

	if( id != ID_ARS && id != ID_POLY && id != ID_BOT )
	{
		if( rt_functab[id].ft_describe( vp, &intern, 1 /*verbose*/,
		    base2local, &rt_uniresource, dbip ) < 0 )
		  Tcl_AppendResult(interp, "vls_solid: describe error\n", (char *)NULL);
	}
	else
	{
		if( rt_functab[id].ft_describe( vp, &intern, 0 /* not verbose */,
		    base2local, &rt_uniresource, dbip ) < 0 )
		  Tcl_AppendResult(interp, "vls_solid: describe error\n", (char *)NULL);
	}

	if( id == ID_PIPE && es_pipept )
	{
		struct rt_pipe_internal *pipe;
		struct wdb_pipept *ps=(struct wdb_pipept *)NULL;
		int seg_no=0;

		pipe = (struct rt_pipe_internal *)ip->idb_ptr;
		RT_PIPE_CK_MAGIC( pipe );

		for( BU_LIST_FOR( ps, wdb_pipept, &pipe->pipe_segs_head ) )
		{
			seg_no++;
			if( ps == es_pipept )
				break;
		}

		if( ps == es_pipept )
			rt_vls_pipept( vp, seg_no, &intern, base2local );
	}

	rt_db_free_internal( &intern, &rt_uniresource );
}

/*
 *  			P S C A L E
 *
 *  Partial scaling of a solid.
 */
void
pscale(void)
{
	static fastf_t ma,mb;

	switch( es_menu ) {

	case MENU_VOL_CSIZE:	/* scale voxel size */
		{
			bu_log( "es_scale = %g\n", es_scale );
		}
		break;

	case MENU_TGC_SCALE_H:	/* scale height vector */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->h);
			}
			VSCALE(tgc->h, tgc->h, es_scale);
		}
		break;

	case MENU_TGC_SCALE_H_V:	/* scale height vector (but move V) */
		{
			point_t old_top;

			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->h);
			}
			VADD2( old_top, tgc->v, tgc->h );
			VSCALE(tgc->h, tgc->h, es_scale);
			VSUB2( tgc->v, old_top, tgc->h );
		}
		break;

	case MENU_TGC_SCALE_H_CD:
		{
			vect_t vec1, vec2;
			vect_t c, d;
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;

			RT_TGC_CK_MAGIC(tgc);

			if (inpara) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->h);
			}

			/* calculate new c */
			VSUB2(vec1, tgc->a, tgc->c);
			VSCALE(vec2, vec1, 1-es_scale);
			VADD2(c, tgc->c, vec2);

			/* calculate new d */
			VSUB2(vec1, tgc->b, tgc->d);
			VSCALE(vec2, vec1, 1-es_scale);
			VADD2(d, tgc->d, vec2);

			if (0 <= VDOT(tgc->c, c) &&
			    0 <= VDOT(tgc->d, d) &&
			    !NEAR_ZERO(MAGNITUDE(c), SMALL_FASTF) &&
			    !NEAR_ZERO(MAGNITUDE(d), SMALL_FASTF)) {
				/* adjust c, d and h */
				VMOVE(tgc->c, c);
				VMOVE(tgc->d, d);
				VSCALE(tgc->h, tgc->h, es_scale);
			}
		}
		break;

	case MENU_TGC_SCALE_H_V_AB:
		{
			vect_t vec1, vec2;
			vect_t a, b;
			point_t old_top;
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;

			RT_TGC_CK_MAGIC(tgc);

			if (inpara) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->h);
			}

			/* calculate new a */
			VSUB2(vec1, tgc->c, tgc->a);
			VSCALE(vec2, vec1, 1-es_scale);
			VADD2(a, tgc->a, vec2);

			/* calculate new b */
			VSUB2(vec1, tgc->d, tgc->b);
			VSCALE(vec2, vec1, 1-es_scale);
			VADD2(b, tgc->b, vec2);

			if (0 <= VDOT(tgc->a, a) &&
			    0 <= VDOT(tgc->b, b) &&
			    !NEAR_ZERO(MAGNITUDE(a), SMALL_FASTF) &&
			    !NEAR_ZERO(MAGNITUDE(b), SMALL_FASTF)) {
				/* adjust a, b, v and h */
				VMOVE(tgc->a, a);
				VMOVE(tgc->b, b);
				VADD2( old_top, tgc->v, tgc->h );
				VSCALE(tgc->h, tgc->h, es_scale);
				VSUB2( tgc->v, old_top, tgc->h );
			}
		}
		break;

	case MENU_TOR_R1:
		/* scale radius 1 of TOR */
		{
			struct rt_tor_internal	*tor =
				(struct rt_tor_internal *)es_int.idb_ptr;
			fastf_t	newrad;
			RT_TOR_CK_MAGIC(tor);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				newrad = es_para[0];
			} else {
				newrad = tor->r_a * es_scale;
			}
			if( newrad < SMALL )  newrad = 4*SMALL;
			if( tor->r_h <= newrad )
				tor->r_a = newrad;
		}
		break;

	case MENU_TOR_R2:
		/* scale radius 2 of TOR */
		{
			struct rt_tor_internal	*tor =
				(struct rt_tor_internal *)es_int.idb_ptr;
			fastf_t	newrad;
			RT_TOR_CK_MAGIC(tor);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				newrad = es_para[0];
			} else {
				newrad = tor->r_h * es_scale;
			}
			if( newrad < SMALL )  newrad = 4*SMALL;
			if( newrad <= tor->r_a )
				tor->r_h = newrad;
		}
		break;

	case MENU_ETO_R:
		/* scale radius 1 (r) of ETO */
		{
			struct rt_eto_internal	*eto =
				(struct rt_eto_internal *)es_int.idb_ptr;
			fastf_t	ch, cv, dh, newrad;
			vect_t	Nu;

			RT_ETO_CK_MAGIC(eto);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				newrad = es_para[0];
			} else {
				newrad = eto->eto_r * es_scale;
			}
			if( newrad < SMALL )  newrad = 4*SMALL;
			VMOVE(Nu, eto->eto_N);
			VUNITIZE(Nu);
			/* get horiz and vert components of C and Rd */
			cv = VDOT( eto->eto_C, Nu );
			ch = sqrt( VDOT( eto->eto_C, eto->eto_C ) - cv * cv );
			/* angle between C and Nu */
			dh = eto->eto_rd * cv / MAGNITUDE(eto->eto_C);
			/* make sure revolved ellipse doesn't overlap itself */
			if (ch <= newrad && dh <= newrad)
				eto->eto_r = newrad;
		}
		break;

	case MENU_ETO_RD:
		/* scale Rd, ellipse semi-minor axis length, of ETO */
		{
			struct rt_eto_internal	*eto =
				(struct rt_eto_internal *)es_int.idb_ptr;
			fastf_t	dh, newrad, work;
			vect_t	Nu;

			RT_ETO_CK_MAGIC(eto);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				newrad = es_para[0];
			} else {
				newrad = eto->eto_rd * es_scale;
			}
			if( newrad < SMALL )  newrad = 4*SMALL;
			work = MAGNITUDE(eto->eto_C);
				if (newrad <= work) {
				VMOVE(Nu, eto->eto_N);
				VUNITIZE(Nu);
				dh = newrad * VDOT( eto->eto_C, Nu ) / work;
				/* make sure revolved ellipse doesn't overlap itself */
				if (dh <= eto->eto_r)
					eto->eto_rd = newrad;
			}
		}
		break;

	case MENU_ETO_SCALE_C:
		/* scale vector C */
		{
			struct rt_eto_internal	*eto =
				(struct rt_eto_internal *)es_int.idb_ptr;
			fastf_t	ch, cv;
			vect_t	Nu, Work;

			RT_ETO_CK_MAGIC(eto);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(eto->eto_C);
			}
			if (es_scale * MAGNITUDE(eto->eto_C) >= eto->eto_rd) {
				VMOVE(Nu, eto->eto_N);
				VUNITIZE(Nu);
				VSCALE(Work, eto->eto_C, es_scale);
				/* get horiz and vert comps of C and Rd */
				cv = VDOT( Work, Nu );
				ch = sqrt( VDOT( Work, Work ) - cv * cv );
				if (ch <= eto->eto_r)
					VMOVE(eto->eto_C, Work);
			}
		}
		break;

	case MENU_RPC_B:
		/* scale vector B */
		{
			struct rt_rpc_internal	*rpc =
				(struct rt_rpc_internal *)es_int.idb_ptr;
			RT_RPC_CK_MAGIC(rpc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(rpc->rpc_B);
			}
			VSCALE(rpc->rpc_B, rpc->rpc_B, es_scale);
		}
		break;

	case MENU_RPC_H:
		/* scale vector H */
		{
			struct rt_rpc_internal	*rpc =
				(struct rt_rpc_internal *)es_int.idb_ptr;

			RT_RPC_CK_MAGIC(rpc);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(rpc->rpc_H);
			}
			VSCALE(rpc->rpc_H, rpc->rpc_H, es_scale);
		}
		break;

	case MENU_RPC_R:
		/* scale rectangular half-width of RPC */
		{
			struct rt_rpc_internal	*rpc =
				(struct rt_rpc_internal *)es_int.idb_ptr;

			RT_RPC_CK_MAGIC(rpc);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / rpc->rpc_r;
			}
			rpc->rpc_r *= es_scale;
		}
		break;

	case MENU_RHC_B:
		/* scale vector B */
		{
			struct rt_rhc_internal	*rhc =
				(struct rt_rhc_internal *)es_int.idb_ptr;
			RT_RHC_CK_MAGIC(rhc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(rhc->rhc_B);
			}
			VSCALE(rhc->rhc_B, rhc->rhc_B, es_scale);
		}
		break;

	case MENU_RHC_H:
		/* scale vector H */
		{
			struct rt_rhc_internal	*rhc =
				(struct rt_rhc_internal *)es_int.idb_ptr;
			RT_RHC_CK_MAGIC(rhc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(rhc->rhc_H);
			}
			VSCALE(rhc->rhc_H, rhc->rhc_H, es_scale);
		}
		break;

	case MENU_RHC_R:
		/* scale rectangular half-width of RHC */
		{
			struct rt_rhc_internal	*rhc =
				(struct rt_rhc_internal *)es_int.idb_ptr;

			RT_RHC_CK_MAGIC(rhc);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / rhc->rhc_r;
			}
			rhc->rhc_r *= es_scale;
		}
		break;

	case MENU_RHC_C:
		/* scale rectangular half-width of RHC */
		{
			struct rt_rhc_internal	*rhc =
				(struct rt_rhc_internal *)es_int.idb_ptr;

			RT_RHC_CK_MAGIC(rhc);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / rhc->rhc_c;
			}
			rhc->rhc_c *= es_scale;
		}
		break;

	case MENU_EPA_H:
		/* scale height vector H */
		{
			struct rt_epa_internal	*epa =
				(struct rt_epa_internal *)es_int.idb_ptr;

			RT_EPA_CK_MAGIC(epa);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(epa->epa_H);
			}
			VSCALE(epa->epa_H, epa->epa_H, es_scale);
		}
		break;

	case MENU_EPA_R1:
		/* scale semimajor axis of EPA */
		{
			struct rt_epa_internal	*epa =
				(struct rt_epa_internal *)es_int.idb_ptr;

			RT_EPA_CK_MAGIC(epa);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / epa->epa_r1;
			}
			if (epa->epa_r1 * es_scale >= epa->epa_r2)
				epa->epa_r1 *= es_scale;
			else
				bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
		}
		break;

	case MENU_EPA_R2:
		/* scale semiminor axis of EPA */
		{
			struct rt_epa_internal	*epa =
				(struct rt_epa_internal *)es_int.idb_ptr;

			RT_EPA_CK_MAGIC(epa);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / epa->epa_r2;
			}
			if (epa->epa_r2 * es_scale <= epa->epa_r1)
				epa->epa_r2 *= es_scale;
			else
				bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
		}
		break;

	case MENU_EHY_H:
		/* scale height vector H */
		{
			struct rt_ehy_internal	*ehy =
				(struct rt_ehy_internal *)es_int.idb_ptr;

			RT_EHY_CK_MAGIC(ehy);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(ehy->ehy_H);
			}
			VSCALE(ehy->ehy_H, ehy->ehy_H, es_scale);
		}
		break;

	case MENU_EHY_R1:
		/* scale semimajor axis of EHY */
		{
			struct rt_ehy_internal	*ehy =
				(struct rt_ehy_internal *)es_int.idb_ptr;

			RT_EHY_CK_MAGIC(ehy);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / ehy->ehy_r1;
			}
			if (ehy->ehy_r1 * es_scale >= ehy->ehy_r2)
				ehy->ehy_r1 *= es_scale;
			else
				bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
		}
		break;

	case MENU_EHY_R2:
		/* scale semiminor axis of EHY */
		{
			struct rt_ehy_internal	*ehy =
				(struct rt_ehy_internal *)es_int.idb_ptr;

			RT_EHY_CK_MAGIC(ehy);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / ehy->ehy_r2;
			}
			if (ehy->ehy_r2 * es_scale <= ehy->ehy_r1)
				ehy->ehy_r2 *= es_scale;
			else
				bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
		}
		break;

	case MENU_EHY_C:
		/* scale distance between apex of EHY & asymptotic cone */
		{
			struct rt_ehy_internal	*ehy =
				(struct rt_ehy_internal *)es_int.idb_ptr;

			RT_EHY_CK_MAGIC(ehy);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / ehy->ehy_c;
			}
			ehy->ehy_c *= es_scale;
		}
		break;

	case MENU_TGC_SCALE_A:
		/* scale vector A */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->a);
			}
			VSCALE(tgc->a, tgc->a, es_scale);
		}
		break;

	case MENU_TGC_SCALE_B:
		/* scale vector B */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->b);
			}
			VSCALE(tgc->b, tgc->b, es_scale);
		}
		break;

	case MENU_ELL_SCALE_A:
		/* scale vector A */
		{
			struct rt_ell_internal	*ell =
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(ell->a);
			}
			VSCALE( ell->a, ell->a, es_scale );
		}
		break;

	case MENU_ELL_SCALE_B:
		/* scale vector B */
		{
			struct rt_ell_internal	*ell =
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(ell->b);
			}
			VSCALE( ell->b, ell->b, es_scale );
		}
		break;

	case MENU_ELL_SCALE_C:
		/* scale vector C */
		{
			struct rt_ell_internal	*ell =
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(ell->c);
			}
			VSCALE( ell->c, ell->c, es_scale );
		}
		break;

	case MENU_TGC_SCALE_C:
		/* TGC: scale ratio "c" */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->c);
			}
			VSCALE(tgc->c, tgc->c, es_scale);
		}
		break;

	case MENU_TGC_SCALE_D:   /* scale  d for tgc */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->d);
			}
			VSCALE(tgc->d, tgc->d, es_scale);
		}
		break;

	case MENU_TGC_SCALE_AB:
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->a);
			}
			VSCALE(tgc->a, tgc->a, es_scale);
			ma = MAGNITUDE( tgc->a );
			mb = MAGNITUDE( tgc->b );
			VSCALE(tgc->b, tgc->b, ma/mb);
		}
		break;

	case MENU_TGC_SCALE_CD:	/* scale C and D of tgc */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->c);
			}
			VSCALE(tgc->c, tgc->c, es_scale);
			ma = MAGNITUDE( tgc->c );
			mb = MAGNITUDE( tgc->d );
			VSCALE(tgc->d, tgc->d, ma/mb);
		}
		break;

	case MENU_TGC_SCALE_ABCD: 		/* scale A,B,C, and D of tgc */
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->a);
			}
			VSCALE(tgc->a, tgc->a, es_scale);
			ma = MAGNITUDE( tgc->a );
			mb = MAGNITUDE( tgc->b );
			VSCALE(tgc->b, tgc->b, ma/mb);
			mb = MAGNITUDE( tgc->c );
			VSCALE(tgc->c, tgc->c, ma/mb);
			mb = MAGNITUDE( tgc->d );
			VSCALE(tgc->d, tgc->d, ma/mb);
		}
		break;

	case MENU_ELL_SCALE_ABC:	/* set A,B, and C length the same */
		{
			struct rt_ell_internal	*ell =
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(ell->a);
			}
			VSCALE( ell->a, ell->a, es_scale );
			ma = MAGNITUDE( ell->a );
			mb = MAGNITUDE( ell->b );
			VSCALE(ell->b, ell->b, ma/mb);
			mb = MAGNITUDE( ell->c );
			VSCALE(ell->c, ell->c, ma/mb);
		}
		break;

		/* begin super ellipse menu options */
	case MENU_SUPERELL_SCALE_A:
		/* scale vector A */
		{
			struct rt_superell_internal	*superell =
				(struct rt_superell_internal *)es_int.idb_ptr;
			RT_SUPERELL_CK_MAGIC(superell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(superell->a);
			}
			VSCALE( superell->a, superell->a, es_scale );
		}
		break;

	case MENU_SUPERELL_SCALE_B:
		/* scale vector B */
		{
			struct rt_superell_internal	*superell =
				(struct rt_superell_internal *)es_int.idb_ptr;
			RT_SUPERELL_CK_MAGIC(superell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(superell->b);
			}
			VSCALE( superell->b, superell->b, es_scale );
		}
		break;

	case MENU_SUPERELL_SCALE_C:
		/* scale vector C */
		{
			struct rt_superell_internal	*superell =
				(struct rt_superell_internal *)es_int.idb_ptr;
			RT_SUPERELL_CK_MAGIC(superell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(superell->c);
			}
			VSCALE( superell->c, superell->c, es_scale );
		}
		break;

	case MENU_SUPERELL_SCALE_ABC:	/* set A,B, and C length the same */
		{
			struct rt_superell_internal	*superell =
				(struct rt_superell_internal *)es_int.idb_ptr;
			RT_SUPERELL_CK_MAGIC(superell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(superell->a);
			}
			VSCALE( superell->a, superell->a, es_scale );
			ma = MAGNITUDE( superell->a );
			mb = MAGNITUDE( superell->b );
			VSCALE(superell->b, superell->b, ma/mb);
			mb = MAGNITUDE( superell->c );
			VSCALE(superell->c, superell->c, ma/mb);
		}
		break;


	case MENU_PIPE_PT_OD:	/* scale OD of one pipe segment */
	  {
	    if( !es_pipept )
	      {
		Tcl_AppendResult(interp, "pscale: no pipe segment selected for scaling\n", (char *)NULL);
		return;
	      }

	    if( inpara ) {
	      /* take es_mat[15] (path scaling) into account */
	      if( es_pipept->pp_od > 0.0 )
		es_scale = es_para[0] * es_mat[15]/es_pipept->pp_od;
	      else
		es_scale = (-es_para[0] * es_mat[15]);
	    }
	    pipe_seg_scale_od( es_pipept, es_scale );
	  }
	  break;
	case MENU_PIPE_PT_ID:	/* scale ID of one pipe segment */
		{
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "pscale: no pipe segment selected for scaling\n", (char *)NULL);
			  return;
			}

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				if( es_pipept->pp_id > 0.0 )
					es_scale = es_para[0] * es_mat[15]/es_pipept->pp_id;
				else
					es_scale = (-es_para[0] * es_mat[15]);
			}
			pipe_seg_scale_id( es_pipept, es_scale );
		}
		break;
	case MENU_PIPE_PT_RADIUS:	/* scale bend radius at selected point */
		{
			if( !es_pipept )
			{
			  Tcl_AppendResult(interp, "pscale: no pipe segment selected for scaling\n", (char *)NULL);
			  return;
			}

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				if( es_pipept->pp_id > 0.0 )
					es_scale = es_para[0] * es_mat[15]/es_pipept->pp_bendradius;
				else
					es_scale = (-es_para[0] * es_mat[15]);
			}
			pipe_seg_scale_radius( es_pipept, es_scale );
		}
		break;
	case MENU_PIPE_SCALE_OD:	/* scale entire pipe OD */
		if( inpara )
		{
			struct rt_pipe_internal *pipe =
				(struct rt_pipe_internal *)es_int.idb_ptr;
			struct wdb_pipept *ps;

			RT_PIPE_CK_MAGIC( pipe );

			ps = BU_LIST_FIRST( wdb_pipept, &pipe->pipe_segs_head );
			BU_CKMAG( ps, WDB_PIPESEG_MAGIC, "wdb_pipept" );

			if( ps->pp_od > 0.0 )
				es_scale = es_para[0] * es_mat[15]/ps->pp_od;
			else
			{
				while( ps->l.magic != BU_LIST_HEAD_MAGIC && ps->pp_od <= 0.0 )
					ps = BU_LIST_NEXT( wdb_pipept, &ps->l );

				if( ps->l.magic == BU_LIST_HEAD_MAGIC )
				{
				  Tcl_AppendResult(interp, "Entire pipe solid has zero OD!!!!\n", (char *)NULL);
				  return;
				}

				es_scale = es_para[0] * es_mat[15]/ps->pp_od;
			}
		}
		pipe_scale_od( &es_int, es_scale );
		break;
	case MENU_PIPE_SCALE_ID:	/* scale entire pipe ID */
		if( inpara )
		{
			struct rt_pipe_internal *pipe =
				(struct rt_pipe_internal *)es_int.idb_ptr;
			struct wdb_pipept *ps;

			RT_PIPE_CK_MAGIC( pipe );

			ps = BU_LIST_FIRST( wdb_pipept, &pipe->pipe_segs_head );
			BU_CKMAG( ps, WDB_PIPESEG_MAGIC, "wdb_pipept" );

			if( ps->pp_id > 0.0 )
				es_scale = es_para[0] * es_mat[15]/ps->pp_id;
			else
			{
				while( ps->l.magic != BU_LIST_HEAD_MAGIC && ps->pp_id <= 0.0 )
					ps = BU_LIST_NEXT( wdb_pipept, &ps->l );

				/* Check if entire pipe has zero ID */
				if( ps->l.magic == BU_LIST_HEAD_MAGIC )
					es_scale = (-es_para[0] * es_mat[15]);
				else
					es_scale = es_para[0] * es_mat[15]/ps->pp_id;
			}
		}
		pipe_scale_id( &es_int, es_scale );
		break;
	case MENU_PIPE_SCALE_RADIUS:	/* scale entire pipr bend radius */
		if( inpara )
		{
			struct rt_pipe_internal *pipe =
				(struct rt_pipe_internal *)es_int.idb_ptr;
			struct wdb_pipept *ps;

			RT_PIPE_CK_MAGIC( pipe );

			ps = BU_LIST_FIRST( wdb_pipept, &pipe->pipe_segs_head );
			BU_CKMAG( ps, WDB_PIPESEG_MAGIC, "wdb_pipept" );

			if( ps->pp_bendradius > 0.0 )
				es_scale = es_para[0] * es_mat[15]/ps->pp_bendradius;
			else
			{
				while( ps->l.magic != BU_LIST_HEAD_MAGIC && ps->pp_bendradius <= 0.0 )
					ps = BU_LIST_NEXT( wdb_pipept, &ps->l );

				/* Check if entire pipe has zero ID */
				if( ps->l.magic == BU_LIST_HEAD_MAGIC )
					es_scale = (-es_para[0] * es_mat[15]);
				else
					es_scale = es_para[0] * es_mat[15]/ps->pp_bendradius;
			}
		}
		pipe_scale_radius( &es_int, es_scale );
		break;
	case MENU_PART_H:
		/* scale vector H */
		{
			struct rt_part_internal	*part =
				(struct rt_part_internal *)es_int.idb_ptr;

			RT_PART_CK_MAGIC(part);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(part->part_H);
			}
			VSCALE(part->part_H, part->part_H, es_scale);
		}
		break;

	case MENU_PART_v:
		/* scale v end radius */
		{
			struct rt_part_internal	*part =
				(struct rt_part_internal *)es_int.idb_ptr;

			RT_PART_CK_MAGIC(part);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / part->part_vrad;
			}
			part->part_vrad *= es_scale;
		}
		break;

	case MENU_PART_h:
		/* scale h end radius */
		{
			struct rt_part_internal	*part =
				(struct rt_part_internal *)es_int.idb_ptr;

			RT_PART_CK_MAGIC(part);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / part->part_hrad;
			}
			part->part_hrad *= es_scale;
		}
		break;

	}
}

/*
 *			I N I T _ O B J E D I T _ G U T S
 *
 */
static void
init_oedit_guts(void)
{
	int			id;
	char			*strp="";

	/* for safety sake */
	es_menu = 0;
	es_edflag = -1;
	MAT_IDN(es_mat);

	if (dbip == DBI_NULL)
		return;

	/*
	 * Check for a processed region
	 */
	if (illump->s_Eflag) {
		/* Have a processed (E'd) region - NO key solid.
		 * 	Use the 'center' as the key
		 */
		VMOVE(es_keypoint, illump->s_center);

		/* The s_center takes the es_mat into account already */
	}

	/* Not an evaluated region - just a regular path ending in a solid */
	if( rt_db_get_internal( &es_int, LAST_SOLID(illump),
	    dbip, NULL, &rt_uniresource ) < 0 )  {
		Tcl_AppendResult(interp, "init_oedit(",
	    			LAST_SOLID(illump)->d_namep,
				 "):  solid import failure\n", (char *)NULL);
		rt_db_free_internal( &es_int, &rt_uniresource );
		button(BE_REJECT);
		return;				/* FAIL */
	}
	RT_CK_DB_INTERNAL(&es_int);
	id = es_int.idb_type;

	if (id == ID_ARB8) {
		struct rt_arb_internal *arb;

		arb = (struct rt_arb_internal *)es_int.idb_ptr;
		RT_ARB_CK_MAGIC(arb);

		es_type = rt_arb_std_type(&es_int , &mged_tol);
	}

	/* Save aggregate path matrix */
	pathHmat(illump, es_mat, illump->s_fullpath.fp_len-2);

	/* get the inverse matrix */
	bn_mat_inv(es_invmat, es_mat);

	get_solid_keypoint(es_keypoint, &strp, &es_int, es_mat);
	init_oedit_vars();
}

static void
init_oedit_vars(void)
{
	set_e_axes_pos(1);

	VSETALL(edit_absolute_model_rotate, 0.0);
	VSETALL(edit_absolute_object_rotate, 0.0);
	VSETALL(edit_absolute_view_rotate, 0.0);
	VSETALL(last_edit_absolute_model_rotate, 0.0);
	VSETALL(last_edit_absolute_object_rotate, 0.0);
	VSETALL(last_edit_absolute_view_rotate, 0.0);
	VSETALL(edit_absolute_model_tran, 0.0);
	VSETALL(edit_absolute_view_tran, 0.0);
	VSETALL(last_edit_absolute_model_tran, 0.0);
	VSETALL(last_edit_absolute_view_tran, 0.0);
	edit_absolute_scale = 0.0;
	acc_sc_sol = 1.0;
	acc_sc_obj = 1.0;
	VSETALL(acc_sc, 1.0);

	VSETALL(edit_rate_model_rotate, 0.0);
	VSETALL(edit_rate_object_rotate, 0.0);
	VSETALL(edit_rate_view_rotate, 0.0);
	VSETALL(edit_rate_model_tran, 0.0);
	VSETALL(edit_rate_view_tran, 0.0);

	MAT_IDN(modelchanges);
	MAT_IDN(acc_rot_sol);
}

/*
 *			I N I T _ O B J E D I T
 *
 */
void
init_oedit(void)
{
	struct bu_vls		vls;

	/* do real initialization work */
	init_oedit_guts();

	es_edclass = EDIT_CLASS_NULL;

	/* begin edit callback */
	bu_vls_init(&vls);
	bu_vls_strcpy(&vls, "begin_edit_callback {}");
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
}

void oedit_reject(void);

static void
oedit_apply( int continue_editing )
{
	register struct solid *sp;
	/* matrices used to accept editing done from a depth
	 *	>= 2 from the top of the illuminated path
	 */
	mat_t topm;	/* accum matrix from pathpos 0 to i-2 */
	mat_t inv_topm;	/* inverse */
	mat_t deltam;	/* final "changes":  deltam = (inv_topm)(modelchanges)(topm) */
	mat_t tempm;

	switch (ipathpos) {
	case 0:
		moveHobj(DB_FULL_PATH_GET(&illump->s_fullpath,ipathpos),
			modelchanges);
		break;
	case 1:
		moveHinstance(DB_FULL_PATH_GET(&illump->s_fullpath,ipathpos-1),
			      DB_FULL_PATH_GET(&illump->s_fullpath,ipathpos),
			      modelchanges);
		break;
	default:
		MAT_IDN(topm);
		MAT_IDN(inv_topm);
		MAT_IDN(deltam);
		MAT_IDN(tempm);

		pathHmat(illump, topm, ipathpos-2);

		bn_mat_inv(inv_topm, topm);

		bn_mat_mul(tempm, modelchanges, topm);
		bn_mat_mul(deltam, inv_topm, tempm);

		moveHinstance(DB_FULL_PATH_GET(&illump->s_fullpath,ipathpos-1),
			      DB_FULL_PATH_GET(&illump->s_fullpath,ipathpos),
			      deltam);
		break;
	}

	/*
	 *  Redraw all solids affected by this edit.
	 *  Regenerate a new control list which does not
	 *  include the solids about to be replaced,
	 *  so we can safely fiddle the displaylist.
	 */
	modelchanges[15] = 1000000000;	/* => small ratio */

	/* Now, recompute new chunks of displaylist */
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		if (sp->s_iflag == DOWN)
			continue;
		(void)replot_original_solid(sp);

		if( continue_editing == DOWN ) {
			sp->s_iflag = DOWN;
		}
	}
}

void
oedit_accept(void)
{
	register struct solid *sp;

	if (dbip == DBI_NULL)
		return;

	if (dbip->dbi_read_only) {
		oedit_reject();
		FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
			if (sp->s_iflag == DOWN)
				continue;
			(void)replot_original_solid(sp);
			sp->s_iflag = DOWN;
		}
		bu_log("Sorry, this database is READ-ONLY\n");
		pr_prompt();

		return;
	}

	oedit_apply( DOWN ); /* finished editing */
	oedit_reject();
}

void
oedit_reject(void)
{
	rt_db_free_internal(&es_int, &rt_uniresource);
}

/* 			F _ E Q N ( )
 * Gets the A,B,C of a  planar equation from the command line and puts the
 * result into the array es_peqn[] at the position pointed to by the variable
 * 'es_menu' which is the plane being redefined. This function is only callable
 * when in solid edit and rotating the face of a GENARB8.
 */
int
f_eqn(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	short int i;
	vect_t tempvec;
	struct rt_arb_internal *arb;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 4 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help eqn");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( state != ST_S_EDIT ){
	  Tcl_AppendResult(interp, "Eqn: must be in solid edit\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( es_int.idb_type != ID_ARB8 )
	{
	  Tcl_AppendResult(interp, "Eqn: type must be GENARB8\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( es_edflag != ECMD_ARB_ROTATE_FACE ){
	  Tcl_AppendResult(interp, "Eqn: must be rotating a face\n", (char *)NULL);
	  return TCL_ERROR;
	}

	arb = (struct rt_arb_internal *)es_int.idb_ptr;
	RT_ARB_CK_MAGIC( arb );

	/* get the A,B,C from the command line */
	for(i=0; i<3; i++)
		es_peqn[es_menu][i]= atof(argv[i+1]);
	VUNITIZE( &es_peqn[es_menu][0] );

	VMOVE( tempvec , arb->pt[fixv] );
	es_peqn[es_menu][3]=VDOT( es_peqn[es_menu], tempvec );
	if( rt_arb_calc_points( arb , es_type , es_peqn , &mged_tol ) )
		return CMD_BAD;

	/* draw the new version of the solid */
	replot_editing_solid();

	/* update display information */
	view_state->vs_flag = 1;

	return TCL_OK;
}

/* Hooks from buttons.c */

/*
 * Copied from sedit_accept - modified to optionally leave
 *                            solid edit state.
 */
static int
sedit_apply(int accept_flag)
{
	struct directory	*dp;

	es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
	es_pipept = (struct wdb_pipept *)NULL; /* Reset es_pipept */
	bot_verts[0] = -1;
	bot_verts[1] = -1;
	bot_verts[2] = -1;

	if (lu_copy) {
		struct model *m;

		m = nmg_find_model(&lu_copy->l.magic);
		nmg_km(m);
		lu_copy = (struct loopuse *)NULL;
	}

	/* write editing changes out to disc */
	dp = LAST_SOLID(illump);

	/* make sure that any BOT solid is minimally legal */
	if (es_int.idb_type == ID_BOT) {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)es_int.idb_ptr;

		RT_BOT_CK_MAGIC(bot);
		if (bot->mode == RT_BOT_SURFACE || bot->mode == RT_BOT_SOLID) {
			/* make sure facemodes and thicknesses have been freed */
			if (bot->thickness) {
				bu_free( (char *)bot->thickness, "BOT thickness" );
				bot->thickness = NULL;
			}
			if (bot->face_mode) {
				bu_free( (char *)bot->face_mode, "BOT face_mode" );
				bot->face_mode = NULL;
			}
		} else {
			/* make sure face_modes and thicknesses exist */
			if (!bot->thickness)
				bot->thickness = (fastf_t *)bu_calloc( bot->num_faces, sizeof( fastf_t ), "BOT thickness" );
			if (!bot->face_mode) {
				bot->face_mode = bu_bitv_new( bot->num_faces );
				bu_bitv_clear( bot->face_mode );
			}
		}
	}

	/* Scale change on export is 1.0 -- no change */
	if( rt_db_put_internal( dp, dbip, &es_int, &rt_uniresource ) < 0 )  {
		Tcl_AppendResult(interp, "sedit_apply(", dp->d_namep,
				 "):  solid export failure\n", (char *)NULL);
		if (accept_flag) {
			rt_db_free_internal(&es_int, &rt_uniresource);
		}
		return TCL_ERROR;				/* FAIL */
	}

	if (accept_flag) {
		menu_state->ms_flag = 0;
		movedir = 0;
		es_edflag = -1;
		es_edclass = EDIT_CLASS_NULL;

		rt_db_free_internal(&es_int, &rt_uniresource);
	} else {
		/* XXX hack to restore es_int after rt_db_put_internal blows it away */
		/* Read solid description into es_int again!!! Gaak! */
		if (rt_db_get_internal(&es_int, LAST_SOLID(illump),
				       dbip, NULL, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "sedit_apply(",
					 LAST_SOLID(illump)->d_namep,
					 "):  solid reimport failure\n", (char *)NULL);
			rt_db_free_internal(&es_int, &rt_uniresource);
			return TCL_ERROR;
		}
	}


	return TCL_OK;
}

void
sedit_accept(void)
{
	if (dbip == DBI_NULL)
		return;

	if (not_state(ST_S_EDIT, "Primitive edit accept"))
		return;

	if (dbip->dbi_read_only) {
		sedit_reject();
		bu_log( "Sorry, this database is READ-ONLY\n" );
		pr_prompt();
		return;
	}

	if (sedraw > 0)
		sedit();

	(void)sedit_apply(1);
}

void
sedit_reject(void)
{
	if( not_state( ST_S_EDIT, "Solid edit reject" ) )  return;

	if( sedraw > 0)
	  sedit();

	es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
	es_pipept = (struct wdb_pipept *)NULL; /* Reset es_pipept */
	bot_verts[0] = -1;
	bot_verts[1] = -1;
	bot_verts[2] = -1;
	es_ars_crv = (-1);
	es_ars_col = (-1);

	if( lu_copy )
	{
		struct model *m;

		m = nmg_find_model( &lu_copy->l.magic );
		nmg_km( m );
		lu_copy = (struct loopuse *)NULL;
	}

	/* Restore the original solid everywhere */
	{
	  register struct solid *sp;

	  FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
	    if(LAST_SOLID(sp) == LAST_SOLID(illump))
	      (void)replot_original_solid( sp );
	  }
	}

	menu_state->ms_flag = 0;
	movedir = 0;
	es_edflag = -1;
	es_edclass = EDIT_CLASS_NULL;

	rt_db_free_internal( &es_int, &rt_uniresource );
}

int
mged_param(Tcl_Interp *interp, int argc, fastf_t *argvect)
{
  register int i;

  CHECK_DBI_NULL;

  if( es_edflag <= 0 )  {
    Tcl_AppendResult(interp,
		     "A solid editor option not selected\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

#if 0
  if( es_edflag == ECMD_TGC_ROT_H
      || es_edflag == ECMD_TGC_ROT_AB
      || es_edflag == ECMD_ETO_ROT_C ) {
    Tcl_AppendResult(interp,
		     "\"p\" command not defined for this option\n",
		     (char *)NULL);
    return TCL_ERROR;
  }
#endif

  inpara = 0;
  for( i = 0; i < argc; i++ )  {
    es_para[ inpara++ ] = argvect[i];
  }

  if( es_edflag == PSCALE || es_edflag == SSCALE || es_edflag == ECMD_BOT_THICK )  {
    if (inpara != 1) {
	    Tcl_AppendResult(interp, "ERROR: only one argument needed\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
    }

    if( es_menu == MENU_PIPE_PT_OD || es_menu == MENU_PIPE_PT_ID || es_menu == MENU_PIPE_SCALE_ID )
      {
	if( es_para[0] < 0.0 )
	  {
	    Tcl_AppendResult(interp, "ERROR: SCALE FACTOR < 0\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
	  }
      }
    else
      {
	if(es_para[0] <= 0.0) {
	  Tcl_AppendResult(interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
	  inpara = 0;
	  return TCL_ERROR;
	}
      }
  } else {
    if (inpara != 3) {
	    Tcl_AppendResult(interp, "ERROR: three arguments needed\n", (char *)NULL);
	    inpara = 0;
	    return TCL_ERROR;
    }
  }

  /* check if need to convert input values to the base unit */
  switch( es_edflag ) {

  case STRANS:
  case ECMD_VTRANS:
  case PSCALE:
  case EARB:
  case ECMD_ARB_MOVE_FACE:
  case ECMD_TGC_MV_H:
  case ECMD_TGC_MV_HH:
  case PTARB:
  case ECMD_NMG_ESPLIT:
  case ECMD_NMG_EMOVE:
  case ECMD_NMG_LEXTRU:
  case ECMD_PIPE_PICK:
  case ECMD_PIPE_SPLIT:
  case ECMD_PIPE_PT_MOVE:
  case ECMD_PIPE_PT_ADD:
  case ECMD_PIPE_PT_INS:
  case ECMD_ARS_PICK:
  case ECMD_ARS_MOVE_PT:
  case ECMD_ARS_MOVE_CRV:
  case ECMD_ARS_MOVE_COL:
  case ECMD_VOL_CSIZE:
  case ECMD_DSP_SCALE_X:
  case ECMD_DSP_SCALE_Y:
  case ECMD_DSP_SCALE_ALT:
  case ECMD_EBM_HEIGHT:
  case ECMD_EXTR_SCALE_H:
  case ECMD_CLINE_SCALE_H:
  case ECMD_CLINE_SCALE_T:
  case ECMD_CLINE_SCALE_R:
  case ECMD_CLINE_MOVE_H:
  case ECMD_EXTR_MOV_H:
  case ECMD_BOT_THICK:
  case ECMD_BOT_MOVET:
  case ECMD_BOT_MOVEE:
  case ECMD_BOT_MOVEV:
    /* must convert to base units */
    es_para[0] *= local2base;
    es_para[1] *= local2base;
    es_para[2] *= local2base;
    /* fall through */
  default:
    break;
  }

#if 1
  sedit();

  if(SEDIT_TRAN){
    vect_t diff;
    fastf_t inv_Viewscale = 1/view_state->vs_vop->vo_scale;

    VSUB2(diff, es_para, e_axes_pos);
    VSCALE(edit_absolute_model_tran, diff, inv_Viewscale);
    VMOVE(last_edit_absolute_model_tran, edit_absolute_model_tran);
  }else if(SEDIT_ROTATE){
    VMOVE(edit_absolute_model_rotate, es_para);
  }else if(SEDIT_SCALE){
    edit_absolute_scale = acc_sc_sol - 1.0;
    if(edit_absolute_scale > 0)
      edit_absolute_scale /= 3.0;
  }
#endif
  return TCL_OK;
}

/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
int
f_param(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  register int i;
  vect_t argvect;

  CHECK_DBI_NULL;
  CHECK_READ_ONLY;

  if(argc < 2 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help p");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for( i = 1; i < argc && i <= 3 ; i++ ){
    argvect[i-1] = atof( argv[i] );
  }

  return mged_param(interp, argc-1, argvect);
}

#if 0
/*
 *  Returns -
 *	1	solid edit claimes the rotate event
 *	0	rotate event can be used some other way.
 */
int
sedit_rotate( xangle, yangle, zangle )
double	xangle, yangle, zangle;
{
	mat_t	tempp;

	if(!SEDIT_ROTATE)
	  return 0;

	MAT_IDN( incr_change );
	bn_mat_angles_rad(incr_change, xangle, yangle, zangle);

	/* accumulate the translations */
	bn_mat_mul(tempp, incr_change, acc_rot_sol);
	MAT_COPY(acc_rot_sol, tempp);

	/* sedit() will use incr_change or acc_rot_sol ?? */
	sedit();	/* change es_int only, NOW */

	return 1;
}

/*
 *  Returns -
 *	1	object edit claimes the rotate event
 *	0	rotate event can be used some other way.
 */
int
objedit_rotate( xangle, yangle, zangle )
double	xangle, yangle, zangle;
{
	mat_t	tempp;
	vect_t	point;

	if( movedir != ROTARROW )  return 0;

	MAT_IDN( incr_change );
	bn_mat_angles_rad(incr_change, xangle, yangle, zangle);

	/* accumulate change matrix - do it wrt a point NOT view center */
#if 0
	MAT4X3PNT(point, modelchanges, es_keypoint);
#else
	bn_mat_mul(tempp, modelchanges, es_mat);
	MAT4X3PNT(point, tempp, es_keypoint);
#endif
	wrt_point(modelchanges, incr_change, modelchanges, point);

	new_edit_mats();

	return 1;
}
#endif

/*
 *			L A B E L _ E D I T E D _ S O L I D
 *
 *  Put labels on the vertices of the currently edited solid.
 *  XXX This really should use import/export interface!!!  Or be part of it.
 */
void
label_edited_solid(
	int *num_lines,
	point_t *lines,
	struct rt_point_labels	pl[],
	int			max_pl,
	const mat_t		xform,
	struct rt_db_internal	*ip)
{
	register int	i;
	point_t		work;
	point_t		pos_view;
	int		npl = 0;

	RT_CK_DB_INTERNAL( ip );

	switch( ip->idb_type )  {

#define	POINT_LABEL( _pt, _char )	{ \
	VMOVE( pl[npl].pt, _pt ); \
	pl[npl].str[0] = _char; \
	pl[npl++].str[1] = '\0'; }

#define	POINT_LABEL_STR( _pt, _str )	{ \
	VMOVE( pl[npl].pt, _pt ); \
	strncpy( pl[npl++].str, _str, sizeof(pl[0].str)-1 ); }

	case ID_ARB8:
		{
			struct rt_arb_internal *arb=
				(struct rt_arb_internal *)es_int.idb_ptr;
			RT_ARB_CK_MAGIC( arb );
			switch( es_type )
			{
				case ARB8:
					for( i=0 ; i<8 ; i++ )
					{
						MAT4X3PNT( pos_view, xform, arb->pt[i] );
						POINT_LABEL( pos_view, i+'1' );
					}
					break;
				case ARB7:
					for( i=0 ; i<7 ; i++ )
					{
						MAT4X3PNT( pos_view, xform, arb->pt[i] );
						POINT_LABEL( pos_view, i+'1' );
					}
					break;
				case ARB6:
					for( i=0 ; i<5 ; i++ )
					{
						MAT4X3PNT( pos_view, xform, arb->pt[i] );
						POINT_LABEL( pos_view, i+'1' );
					}
					MAT4X3PNT( pos_view, xform, arb->pt[6] );
					POINT_LABEL( pos_view, '6' );
					break;
				case ARB5:
					for( i=0 ; i<5 ; i++ )
					{
						MAT4X3PNT( pos_view, xform, arb->pt[i] );
						POINT_LABEL( pos_view, i+'1' );
					}
					break;
				case ARB4:
					for( i=0 ; i<3 ; i++ )
					{
						MAT4X3PNT( pos_view, xform, arb->pt[i] );
						POINT_LABEL( pos_view, i+'1' );
					}
					MAT4X3PNT( pos_view, xform, arb->pt[4] );
					POINT_LABEL( pos_view, '4' );
					break;
			}
		}
		break;
	case ID_TGC:
		{
			struct rt_tgc_internal	*tgc =
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);
			MAT4X3PNT( pos_view, xform, tgc->v );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, tgc->v, tgc->a );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'A' );

			VADD2( work, tgc->v, tgc->b );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'B' );

			VADD3( work, tgc->v, tgc->h, tgc->c );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'C' );

			VADD3( work, tgc->v, tgc->h, tgc->d );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'D' );
		}
		break;

	case ID_ELL:
		{
			point_t	work;
			point_t	pos_view;
			struct rt_ell_internal	*ell =
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);

			MAT4X3PNT( pos_view, xform, ell->v );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, ell->v, ell->a );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'A' );

			VADD2( work, ell->v, ell->b );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'B' );

			VADD2( work, ell->v, ell->c );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'C' );
		}
		break;

	case ID_SUPERELL:
		{
			point_t	work;
			point_t	pos_view;
			struct rt_superell_internal	*superell =
				(struct rt_superell_internal *)es_int.idb_ptr;
			RT_SUPERELL_CK_MAGIC(superell);

			MAT4X3PNT( pos_view, xform, superell->v );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, superell->v, superell->a );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'A' );

			VADD2( work, superell->v, superell->b );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'B' );

			VADD2( work, superell->v, superell->c );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'C' );
		}
		break;

	case ID_TOR:
		{
			struct rt_tor_internal	*tor =
				(struct rt_tor_internal *)es_int.idb_ptr;
			fastf_t	r3, r4;
			vect_t	adir;
			RT_TOR_CK_MAGIC(tor);

			bn_vec_ortho( adir, tor->h );

			MAT4X3PNT( pos_view, xform, tor->v );
			POINT_LABEL( pos_view, 'V' );

			r3 = tor->r_a - tor->r_h;
			VJOIN1( work, tor->v, r3, adir );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'I' );

			r4 = tor->r_a + tor->r_h;
			VJOIN1( work, tor->v, r4, adir );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'O' );

			VJOIN1( work, tor->v, tor->r_a, adir );
			VADD2( work, work, tor->h );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'H' );
		}
		break;

	case ID_RPC:
		{
			struct rt_rpc_internal	*rpc =
				(struct rt_rpc_internal *)es_int.idb_ptr;
			vect_t	Ru;

			RT_RPC_CK_MAGIC(rpc);
			MAT4X3PNT( pos_view, xform, rpc->rpc_V );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, rpc->rpc_V, rpc->rpc_B );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'B' );

			VADD2( work, rpc->rpc_V, rpc->rpc_H );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'H' );

			VCROSS( Ru, rpc->rpc_B, rpc->rpc_H );
			VUNITIZE( Ru );
			VSCALE( Ru, Ru, rpc->rpc_r );
			VADD2( work, rpc->rpc_V, Ru );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'r' );
		}
		break;

	case ID_PARTICLE:
		{
			struct rt_part_internal	*part =
				(struct rt_part_internal *)es_int.idb_ptr;
			vect_t	Ru, ortho;

			RT_PART_CK_MAGIC(part);
			MAT4X3PNT( pos_view, xform, part->part_V );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, part->part_V, part->part_H );
                        MAT4X3PNT(pos_view, xform, work);
                        POINT_LABEL( pos_view, 'H' );

			VMOVE( Ru, part->part_H );
			VUNITIZE( Ru );
			bn_vec_ortho( ortho, Ru );
			VSCALE( work, ortho, part->part_vrad );
			VADD2( work, part->part_V, work );
                        MAT4X3PNT( pos_view, xform, work );
                        POINT_LABEL( pos_view, 'v' );

			VSCALE( work, ortho, part->part_hrad );
			VADD3( work, part->part_V, part->part_H, work );
                        MAT4X3PNT( pos_view, xform, work );
                        POINT_LABEL( pos_view, 'h' );
		}
		break;

	case ID_RHC:
		{
			struct rt_rhc_internal	*rhc =
				(struct rt_rhc_internal *)es_int.idb_ptr;
			vect_t	Ru;

			RT_RHC_CK_MAGIC(rhc);
			MAT4X3PNT( pos_view, xform, rhc->rhc_V );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, rhc->rhc_V, rhc->rhc_B );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'B' );

			VADD2( work, rhc->rhc_V, rhc->rhc_H );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'H' );

			VCROSS( Ru, rhc->rhc_B, rhc->rhc_H );
			VUNITIZE( Ru );
			VSCALE( Ru, Ru, rhc->rhc_r );
			VADD2( work, rhc->rhc_V, Ru );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'r' );

			VMOVE( work, rhc->rhc_B );
			VUNITIZE( work );
			VSCALE( work, work,
				MAGNITUDE(rhc->rhc_B) + rhc->rhc_c );
			VADD2( work, work, rhc->rhc_V );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'c' );
		}
		break;

	case ID_EPA:
		{
			struct rt_epa_internal	*epa =
				(struct rt_epa_internal *)es_int.idb_ptr;
			vect_t	A, B;

			RT_EPA_CK_MAGIC(epa);
			MAT4X3PNT( pos_view, xform, epa->epa_V );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, epa->epa_V, epa->epa_H );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'H' );

			VSCALE( A, epa->epa_Au, epa->epa_r1 );
			VADD2( work, epa->epa_V, A );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'A' );

			VCROSS( B, epa->epa_Au, epa->epa_H );
			VUNITIZE( B );
			VSCALE( B, B, epa->epa_r2 );
			VADD2( work, epa->epa_V, B );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'B' );
		}
		break;

	case ID_EHY:
		{
			struct rt_ehy_internal	*ehy =
				(struct rt_ehy_internal *)es_int.idb_ptr;
			vect_t	A, B;

			RT_EHY_CK_MAGIC(ehy);
			MAT4X3PNT( pos_view, xform, ehy->ehy_V );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work, ehy->ehy_V, ehy->ehy_H );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'H' );

			VSCALE( A, ehy->ehy_Au, ehy->ehy_r1 );
			VADD2( work, ehy->ehy_V, A );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'A' );

			VCROSS( B, ehy->ehy_Au, ehy->ehy_H );
			VUNITIZE( B );
			VSCALE( B, B, ehy->ehy_r2 );
			VADD2( work, ehy->ehy_V, B );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'B' );

			VMOVE( work, ehy->ehy_H );
			VUNITIZE( work );
			VSCALE( work, work,
				MAGNITUDE(ehy->ehy_H) + ehy->ehy_c );
			VADD2( work, ehy->ehy_V, work );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'c' );
		}
		break;

	case ID_ETO:
		{
			struct rt_eto_internal	*eto =
				(struct rt_eto_internal *)es_int.idb_ptr;
			fastf_t	ch, cv, dh, dv, cmag, phi;
			vect_t	Au, Nu;

			RT_ETO_CK_MAGIC(eto);

			MAT4X3PNT( pos_view, xform, eto->eto_V );
			POINT_LABEL( pos_view, 'V' );

			VMOVE(Nu, eto->eto_N);
			VUNITIZE(Nu);
			bn_vec_ortho( Au, Nu );
			VUNITIZE(Au);

			cmag = MAGNITUDE(eto->eto_C);
			/* get horizontal and vertical components of C and Rd */
			cv = VDOT( eto->eto_C, Nu );
			ch = sqrt( cmag*cmag - cv*cv );
			/* angle between C and Nu */
			phi = acos( cv / cmag );
			dv = -eto->eto_rd * sin(phi);
			dh = eto->eto_rd * cos(phi);

			VJOIN2(work, eto->eto_V, eto->eto_r+ch, Au, cv, Nu);
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'C' );

			VJOIN2(work, eto->eto_V, eto->eto_r+dh, Au, dv, Nu);
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'D' );

			VJOIN1(work, eto->eto_V, eto->eto_r, Au);
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'r' );
		}
		break;

	case ID_ARS:
		{
			register struct rt_ars_internal *ars=
				(struct rt_ars_internal *)es_int.idb_ptr;

			RT_ARS_CK_MAGIC( ars );

			MAT4X3PNT(pos_view, xform, ars->curves[0] )

			if( es_ars_crv >= 0 && es_ars_col >= 0 )
			{
				point_t work;
				point_t ars_pt;

				VMOVE( work, &ars->curves[es_ars_crv][es_ars_col*3] );
				MAT4X3PNT(ars_pt, xform, work);
				POINT_LABEL_STR( ars_pt, "pt" );
			}
		}
		POINT_LABEL( pos_view, 'V' );
		break;

	case ID_BSPLINE:
		{
			register struct rt_nurb_internal *sip =
				(struct rt_nurb_internal *) es_int.idb_ptr;
			register struct face_g_snurb	*surf;
			register fastf_t	*fp;

			RT_NURB_CK_MAGIC(sip);
			surf = sip->srfs[spl_surfno];
			NMG_CK_SNURB(surf);
			fp = &RT_NURB_GET_CONTROL_POINT( surf, spl_ui, spl_vi );
			MAT4X3PNT(pos_view, xform, fp);
			POINT_LABEL( pos_view, 'V' );

			fp = &RT_NURB_GET_CONTROL_POINT( surf, 0, 0 );
			MAT4X3PNT(pos_view, xform, fp);
			POINT_LABEL_STR( pos_view, " 0,0" );
			fp = &RT_NURB_GET_CONTROL_POINT( surf, 0, surf->s_size[1]-1 );
			MAT4X3PNT(pos_view, xform, fp);
			POINT_LABEL_STR( pos_view, " 0,u" );
			fp = &RT_NURB_GET_CONTROL_POINT( surf, surf->s_size[0]-1, 0 );
			MAT4X3PNT(pos_view, xform, fp);
			POINT_LABEL_STR( pos_view, " v,0" );
			fp = &RT_NURB_GET_CONTROL_POINT( surf, surf->s_size[0]-1, surf->s_size[1]-1 );
			MAT4X3PNT(pos_view, xform, fp);
			POINT_LABEL_STR( pos_view, " u,v" );
		}
		break;
	case ID_NMG:
		/* New way only */
		{
#ifndef NO_MAGIC_CHECKING
			register struct model *m =
				(struct model *) es_int.idb_ptr;
			NMG_CK_MODEL(m);
#endif

			if( es_eu )  {
				point_t	cent;
				NMG_CK_EDGEUSE(es_eu);
				VADD2SCALE( cent,
					es_eu->vu_p->v_p->vg_p->coord,
					es_eu->eumate_p->vu_p->v_p->vg_p->coord,
					0.5 );
				MAT4X3PNT(pos_view, xform, cent);
				POINT_LABEL_STR( pos_view, " eu" );
			}
		}
		break;
	case ID_PIPE:
		{
#ifndef NO_MAGIC_CHECKING
			register struct rt_pipe_internal *pipe =
				(struct rt_pipe_internal *)es_int.idb_ptr;

			RT_PIPE_CK_MAGIC( pipe );
#endif

			if( es_pipept ) {
				BU_CKMAG( es_pipept, WDB_PIPESEG_MAGIC, "wdb_pipept" );

				MAT4X3PNT(pos_view, xform, es_pipept->pp_coord);
				POINT_LABEL_STR( pos_view, "pt" );
			}
		}
		break;
	case ID_CLINE:
		{
			register struct rt_cline_internal *cli =
				(struct rt_cline_internal *)es_int.idb_ptr;
			point_t work1;

			RT_CLINE_CK_MAGIC( cli );

			MAT4X3PNT( pos_view, xform, cli->v );
			POINT_LABEL( pos_view, 'V' );

			VADD2( work1, cli->v, cli->h );
			MAT4X3PNT( pos_view, xform, work1 );
			POINT_LABEL( pos_view, 'H' );
		}
		break;
	case ID_BOT:
		{
			register struct rt_bot_internal *bot =
				(struct rt_bot_internal *)es_int.idb_ptr;

			RT_BOT_CK_MAGIC( bot );

			if( bot_verts[2] > -1 &&
				bot_verts[1] > -1 &&
				bot_verts[0] > -1 )
			{
				/* editing a face */
				point_t mid_pt;
				point_t p1, p2, p3;
				fastf_t one_third=1.0/3.0;

				MAT4X3PNT( p1, xform, &bot->vertices[bot_verts[0]*3] );
				MAT4X3PNT( p2, xform, &bot->vertices[bot_verts[1]*3] );
				MAT4X3PNT( p3, xform, &bot->vertices[bot_verts[2]*3] );
				VADD3( mid_pt, p1, p2, p3 );

				VSCALE( mid_pt, mid_pt, one_third );

				*num_lines = 3;
				VMOVE( lines[0], mid_pt );
				VMOVE( lines[1], p1 );
				VMOVE( lines[2], mid_pt );
				VMOVE( lines[3], p2 );
				VMOVE( lines[4], mid_pt );
				VMOVE( lines[5], p3 );
			}
			else if( bot_verts[1] > -1 && bot_verts[0] > -1 )
			{
				/* editing an edge */
				point_t mid_pt;

				VBLEND2( mid_pt, 0.5, &bot->vertices[bot_verts[0]*3],
						 0.5, &bot->vertices[bot_verts[1]*3] );

				MAT4X3PNT( pos_view, xform, mid_pt );
				POINT_LABEL_STR( pos_view, "edge" );
			}
			if( bot_verts[0] > -1 )
			{
				/* editing something, always label the vertex (this is the keypoint) */
				MAT4X3PNT( pos_view, xform, &bot->vertices[bot_verts[0]*3] );
				POINT_LABEL_STR( pos_view, "pt" );
			}
		}
		break;
	}

	pl[npl].str[0] = '\0';	/* Mark ending */
}

#ifndef HIDE_MGEDS_ARB_ROUTINES
/* -------------------------------- */
/*
 *			R T _ A R B _ C A L C _ P L A N E S
 *
 *	Calculate the plane (face) equations for an arb
 *	output previously went to es_peqn[i].
 *
 *  Returns -
 *	-1	Failure
 *	 0	OK
 */
int
rt_arb_calc_planes(
	plane_t			planes[6],
	struct rt_arb_internal	*arb,
	int			type,
	const struct bn_tol	*tol)
{
	register int i, p1, p2, p3;

	RT_ARB_CK_MAGIC( arb);
	BN_CK_TOL( tol );

	type -= 4;	/* ARB4 at location 0, ARB5 at 1, etc */

	for(i=0; i<6; i++) {
		if(rt_arb_faces[type][i*4] == -1)
			break;	/* faces are done */
		p1 = rt_arb_faces[type][i*4];
		p2 = rt_arb_faces[type][i*4+1];
		p3 = rt_arb_faces[type][i*4+2];

		if( bn_mk_plane_3pts( planes[i],
		    arb->pt[p1], arb->pt[p2], arb->pt[p3], tol ) < 0 )  {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "rt_arb_calc_planes: No eqn for face %d%d%d%d\n",
				p1+1, p2+1, p3+1, rt_arb_faces[type][i*4+3]+1);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);
		  return -1;
		}
	}
	return 0;
}
#endif

/* -------------------------------- */
void
sedit_vpick( point_t v_pos )
{
	point_t	m_pos;
	int	surfno, u, v;

	MAT4X3PNT( m_pos, view_state->vs_objview2model, v_pos );

	if( nurb_closest2d( &surfno, &u, &v,
	    (struct rt_nurb_internal *)es_int.idb_ptr,
	    m_pos, view_state->vs_model2objview ) >= 0 )  {
		spl_surfno = surfno;
		spl_ui = u;
		spl_vi = v;
		get_solid_keypoint( es_keypoint, &es_keytag, &es_int, es_mat );
	}
	chg_state( ST_S_VPICK, ST_S_EDIT, "Vertex Pick Complete");
	view_state->vs_flag = 1;
}

#define DIST2D(P0, P1)	sqrt(	((P1)[X] - (P0)[X])*((P1)[X] - (P0)[X]) + \
				((P1)[Y] - (P0)[Y])*((P1)[Y] - (P0)[Y]) )

#define DIST3D(P0, P1)	sqrt(	((P1)[X] - (P0)[X])*((P1)[X] - (P0)[X]) + \
				((P1)[Y] - (P0)[Y])*((P1)[Y] - (P0)[Y]) + \
				((P1)[Z] - (P0)[Z])*((P1)[Z] - (P0)[Z]) )

/*
 *	C L O S E S T 3 D
 *
 *	Given a vlist pointer (vhead) to point coordinates and a reference
 *	point (ref_pt), pass back in "closest_pt" the coordinates of the
 *	point nearest the reference point in 3 space.
 *
 */
int
rt_vl_closest3d(struct bu_list *vhead, fastf_t *ref_pt, fastf_t *closest_pt)
{
	fastf_t		dist, cur_dist;
	pointp_t	c_pt;
	struct rt_vlist	*cur_vp;

	if (vhead == BU_LIST_NULL || BU_LIST_IS_EMPTY(vhead))
		return(1);	/* fail */

	/* initialize smallest distance using 1st point in list */
	cur_vp = BU_LIST_FIRST(rt_vlist, vhead);
	dist = DIST3D(ref_pt, cur_vp->pt[0]);
	c_pt = cur_vp->pt[0];

	for (BU_LIST_FOR(cur_vp, rt_vlist, vhead)) {
		register int	i;
		register int	nused = cur_vp->nused;
		register point_t *cur_pt = cur_vp->pt;

		for (i = 0; i < nused; i++) {
			cur_dist = DIST3D(ref_pt, cur_pt[i]);
			if (cur_dist < dist) {
				dist = cur_dist;
				c_pt = cur_pt[i];
			}
		}
	}
	VMOVE(closest_pt, c_pt);
	return(0);	/* success */
}

/*
 *	C L O S E S T 2 D
 *
 *	Given a pointer (vhead) to vlist point coordinates, a reference
 *	point (ref_pt), and a transformation matrix (mat), pass back in
 *	"closest_pt" the original, untransformed 3 space coordinates of
 *	the point nearest the reference point after all points have been
 *	transformed into 2 space projection plane coordinates.
 */
int
rt_vl_closest2d(struct bu_list *vhead, fastf_t *ref_pt, fastf_t *mat, fastf_t *closest_pt)
{
	fastf_t		dist, cur_dist;
	point_t		cur_pt2d, ref_pt2d;
	pointp_t	c_pt;
	struct rt_vlist	*cur_vp;

	if (vhead == BU_LIST_NULL || BU_LIST_IS_EMPTY(vhead))
		return(1);	/* fail */

	/* transform reference point to 2d */
	MAT4X3PNT(ref_pt2d, mat, ref_pt);

	/* initialize smallest distance using 1st point in list */
	cur_vp = BU_LIST_FIRST(rt_vlist, vhead);
	MAT4X3PNT(cur_pt2d, mat, cur_vp->pt[0]);
	dist = DIST2D(ref_pt2d, cur_pt2d);
	c_pt = cur_vp->pt[0];

	for (BU_LIST_FOR(cur_vp, rt_vlist, vhead)) {
		register int	i;
		register int	nused = cur_vp->nused;
		register point_t *cur_pt = cur_vp->pt;

		for (i = 0; i < nused; i++) {
			MAT4X3PNT(cur_pt2d, mat, cur_pt[i]);
			cur_dist = DIST2D(ref_pt2d, cur_pt2d);
			if (cur_dist < dist) {
				dist = cur_dist;
				c_pt = cur_pt[i];
			}
		}
	}
	VMOVE(closest_pt, c_pt);
	return(0);	/* success */
}

/*
 *				N U R B _ C L O S E S T 3 D
 *
 *	Given a vlist pointer (vhead) to point coordinates and a reference
 *	point (ref_pt), pass back in "closest_pt" the coordinates of the
 *	point nearest the reference point in 3 space.
 *
 */
int
nurb_closest3d(int *surface, int *uval, int *vval, const struct rt_nurb_internal *spl, const fastf_t *ref_pt)
{
	struct face_g_snurb	*srf;
	fastf_t		*mesh;
	fastf_t		d;
	fastf_t		c_dist;		/* closest dist so far */
	int		c_surfno;
	int		c_u, c_v;
	int		u, v;
	int		i;

	RT_NURB_CK_MAGIC(spl);

	c_dist = INFINITY;
	c_surfno = c_u = c_v = -1;

	for( i = 0; i < spl->nsrf; i++ )  {
		int	advance;

		srf = spl->srfs[i];
		NMG_CK_SNURB(srf);
		mesh = srf->ctl_points;
		advance = RT_NURB_EXTRACT_COORDS(srf->pt_type);

		for( v = 0; v < srf->s_size[0]; v++ )  {
			for( u = 0; u < srf->s_size[1]; u++ )  {
				/* XXX 4-tuples? */
				d = DIST3D(ref_pt, mesh);
				if (d < c_dist)  {
					c_dist = d;
					c_surfno = i;
					c_u = u;
					c_v = v;
				}
				mesh += advance;
			}
		}
	}
	if( c_surfno < 0 )  return  -1;		/* FAIL */
	*surface = c_surfno;
	*uval = c_u;
	*vval = c_v;

	return(0);				/* success */
}

/*
 *				N U R B _ C L O S E S T 2 D
 *
 *	Given a pointer (vhead) to vlist point coordinates, a reference
 *	point (ref_pt), and a transformation matrix (mat), pass back in
 *	"closest_pt" the original, untransformed 3 space coordinates of
 *	the point nearest the reference point after all points have been
 *	transformed into 2 space projection plane coordinates.
 */
int
nurb_closest2d(
	int				*surface,
	int				*uval,
	int				*vval,
	const struct rt_nurb_internal	*spl,
	const point_t			ref_pt,
	const mat_t			mat)
{
	struct face_g_snurb	*srf;
	point_t		ref_2d;
	fastf_t		*mesh;
	fastf_t		d;
	fastf_t		c_dist;		/* closest dist so far */
	int		c_surfno;
	int		c_u, c_v;
	int		u, v;
	int		i;

	RT_NURB_CK_MAGIC(spl);

	c_dist = INFINITY;
	c_surfno = c_u = c_v = -1;

	/* transform reference point to 2d */
	MAT4X3PNT(ref_2d, mat, ref_pt);

	for( i = 0; i < spl->nsrf; i++ )  {
		int	advance;

		srf = spl->srfs[i];
		NMG_CK_SNURB(srf);
		mesh = srf->ctl_points;
		advance = RT_NURB_EXTRACT_COORDS(srf->pt_type);

		for( v = 0; v < srf->s_size[0]; v++ )  {
			for( u = 0; u < srf->s_size[1]; u++ )  {
				point_t	cur;
				/* XXX 4-tuples? */
				MAT4X3PNT( cur, mat, mesh );
				d = DIST2D(ref_2d, cur);
				if (d < c_dist)  {
					c_dist = d;
					c_surfno = i;
					c_u = u;
					c_v = v;
				}
				mesh += advance;
			}
		}
	}
	if( c_surfno < 0 )  return  -1;		/* FAIL */
	*surface = c_surfno;
	*uval = c_u;
	*vval = c_v;

	return(0);				/* success */
}


int
f_keypoint(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  CHECK_DBI_NULL;

  if(argc < 1 || 4 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help keypoint");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if ((state != ST_S_EDIT) && (state != ST_O_EDIT)) {
    state_err("keypoint assignment");
    return TCL_ERROR;
  }

  switch (--argc) {
  case 0:
    {
      struct bu_vls tmp_vls;
      point_t key;


      VSCALE( key, es_keypoint, base2local );
      bu_vls_init(&tmp_vls);
      bu_vls_printf(&tmp_vls, "%s (%g, %g, %g)\n", es_keytag, V3ARGS(key));
      Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
      bu_vls_free(&tmp_vls);
    }

    break;
  case 3:
    VSET(es_keypoint,
	 atof( argv[1] ) * local2base,
	 atof( argv[2] ) * local2base,
	 atof( argv[3] ) * local2base);
    es_keytag = "user-specified";
    es_keyfixed = 1;
    break;
  case 1:
    if (strcmp(argv[1], "reset") == 0) {
      es_keytag = "";
      es_keyfixed = 0;
      get_solid_keypoint(es_keypoint, &es_keytag,
			 &es_int, es_mat);
      break;
    }
  default:
    Tcl_AppendResult(interp, "Usage: 'keypoint [<x y z> | reset]'\n", (char *)NULL);
    return TCL_ERROR;
  }

  view_state->vs_flag = 1;
  return TCL_OK;
}

int
f_get_sedit_menus(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  struct menu_item *mip = (struct menu_item *)NULL;
  struct bu_vls vls;

  if(state != ST_S_EDIT)
    return TCL_ERROR;

  bu_vls_init(&vls);

  switch( es_int.idb_type ) {
  case ID_ARB8:
    {
      struct bu_vls vls2;

      bu_vls_init(&vls2);

      /* title */
      bu_vls_printf(&vls, "{{ARB MENU} {}}");

      /* build "move edge" menu */
      mip = which_menu[es_type-4];
      /* submenu title */
      bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
      for(++mip; mip->menu_func != (void (*)())NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

      bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[1].menu_string, bu_vls_addr(&vls2));
      bu_vls_trunc(&vls2, 0);

      /* build "move face" menu */
      mip = which_menu[es_type+1];
      /* submenu title */
      bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
      for(++mip; mip->menu_func != (void (*)())NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

      bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[2].menu_string, bu_vls_addr(&vls2));
      bu_vls_trunc(&vls2, 0);

      /* build "rotate face" menu */
      mip = which_menu[es_type+6];
      /* submenu title */
      bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
      for(++mip; mip->menu_func != (void (*)())NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

      bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[3].menu_string, bu_vls_addr(&vls2));
      bu_vls_free(&vls2);
    }

    break;
  case ID_ARS:
    {
      struct bu_vls vls2;

      /* build ARS PICK MENU Tcl list */
      bu_vls_init(&vls2);

      mip = ars_pick_menu;
      /* title */
      bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);
      for(++mip; mip->menu_func != (void (*)())NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

      mip = ars_menu;
      /* title */
      bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

      /* pick vertex menu */
      bu_vls_printf(&vls, " {{%s} {%s}}", (++mip)->menu_string,
		    bu_vls_addr(&vls2));

      for(++mip; mip->menu_func != (void (*)())NULL; ++mip)
	bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

      bu_vls_free(&vls2);
    }

    break;
  default:
    switch( es_int.idb_type ) {
    case ID_TGC:
      mip = tgc_menu;
      break;
    case ID_TOR:
      mip = tor_menu;
      break;
    case ID_ELL:
      mip = ell_menu;
      break;
    case ID_SUPERELL:
      mip = superell_menu;
      break;
    case ID_BSPLINE:
      mip = spline_menu;
      break;
    case ID_RPC:
      mip = rpc_menu;
      break;
    case ID_RHC:
      mip = rhc_menu;
      break;
    case ID_EPA:
      mip = epa_menu;
      break;
    case ID_EHY:
      mip = ehy_menu;
      break;
    case ID_ETO:
      mip = eto_menu;
      break;
    case ID_NMG:
      mip = nmg_menu;
      break;
    case ID_PIPE:
      mip = pipe_menu;
      break;
    case ID_VOL:
      mip = vol_menu;
      break;
    case ID_EBM:
      mip = ebm_menu;
      break;
    case ID_DSP:
      mip = dsp_menu;
      break;
    case ID_PARTICLE:
      mip = part_menu;
      break;
   case ID_BOT:
      mip = bot_menu;
      break;
   case ID_EXTRUDE:
      mip = extr_menu;
      break;
   case ID_CLINE:
      mip = cline_menu;
      break;
    }

    if(mip == (struct menu_item *)NULL)
      break;

    /* title */
    bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

    for(++mip; mip->menu_func != (void (*)())NULL; ++mip)
      bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

    break;
  }

  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
  bu_vls_free(&vls);

  return TCL_OK;
}

int
f_get_sedit(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  int status;
  struct rt_db_internal ces_int;
  Tcl_Obj *pto;
  Tcl_Obj *pnto;

  if(argc < 1 || 2 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helpdevel get_sed");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(state != ST_S_EDIT){
    Tcl_AppendResult(interp, "get_sed: must be in solid edit state", (char *)0);
    return TCL_ERROR;
  }

  if(argc == 1){
    /* get solid type and parameters */
    RT_CK_DB_INTERNAL(&es_int);
    RT_CK_FUNCTAB(es_int.idb_meth);
    status = es_int.idb_meth->ft_tclget(interp, &es_int, (char *)0);
    pto = Tcl_GetObjResult(interp);

    pnto = Tcl_NewObj();
    /* insert solid name, type and parameters */
    Tcl_AppendStringsToObj(pnto, LAST_SOLID(illump)->d_namep, " ",
			   Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

    Tcl_SetObjResult(interp, pnto);
    return status;
  }

  if(argv[1][0] != '-' || argv[1][1] != 'c'){
    Tcl_AppendResult(interp, "Usage: get_sed [-c]", (char *)0);
    return TCL_ERROR;
  }

  RT_INIT_DB_INTERNAL(&ces_int);
  /* apply matrices along the path */
  transform_editing_solid(&ces_int, es_mat, &es_int, 0);

  /* get solid type and parameters */
  RT_CK_DB_INTERNAL(&ces_int);
  RT_CK_FUNCTAB(ces_int.idb_meth);
  status = ces_int.idb_meth->ft_tclget(interp, &ces_int, (char *)0);
  pto = Tcl_GetObjResult(interp);

  pnto = Tcl_NewObj();
  /* insert full pathname */
  {
  	struct bu_vls str;
  	bu_vls_init(&str);
  	db_path_to_vls(&str, &illump->s_fullpath);
        Tcl_AppendStringsToObj(pnto, bu_vls_addr(&str), NULL );
  	bu_vls_free(&str);
  }

  /* insert solid type and parameters */
  Tcl_AppendStringsToObj(pnto, " ", Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

  Tcl_SetObjResult(interp, pnto);

  rt_db_free_internal( &ces_int, &rt_uniresource );

  return status;
}

int
f_put_sedit(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  register const struct rt_functab *ftp;
  long save_magic;
  int context;

  /*XXX needs better argument checking */
  if(argc < 6){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helpdevel put_sed");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(state != ST_S_EDIT){
    Tcl_AppendResult(interp, "put_sed: must be in solid edit state", (char *)0);
    return TCL_ERROR;
  }

  /* look for -c */
  if(argv[1][0] == '-' && argv[1][1] == 'c'){
    context = 1;
    --argc;
    ++argv;
  } else
    context = 0;

  ftp = rt_get_functab_by_label( argv[1] );
  if( ftp == NULL ||
      ftp->ft_parsetab == (struct bu_structparse *)NULL) {
    Tcl_AppendResult( interp, "put_sed: ", argv[1],
		      " object type is not supported for db get",
		      (char *)0 );
    return TCL_ERROR;
  }

  RT_CK_FUNCTAB(es_int.idb_meth);
  if( es_int.idb_meth != ftp ) {
    Tcl_AppendResult( interp,
		      "put_sed: idb_meth type mismatch",
		      (char *)0 );
  }

  save_magic = *((long *)es_int.idb_ptr);
  *((long *)es_int.idb_ptr) = ftp->ft_internal_magic;
  if( bu_structparse_argv(interp, argc-2, argv+2, ftp->ft_parsetab,
			  (char *)es_int.idb_ptr )==TCL_ERROR ) {
    return TCL_ERROR;
  }
  *((long *)es_int.idb_ptr) = save_magic;

  if(context)
    transform_editing_solid(&es_int, es_invmat, &es_int, 1);

  /* must re-calculate the face plane equations for arbs */
  if( es_int.idb_type == ID_ARB8 ){
    struct rt_arb_internal *arb;

    arb = (struct rt_arb_internal *)es_int.idb_ptr;
    RT_ARB_CK_MAGIC( arb );

    (void)rt_arb_calc_planes( es_peqn , arb , es_type , &mged_tol );
  }

  if(!es_keyfixed)
    get_solid_keypoint(es_keypoint, &es_keytag, &es_int, es_mat);

  set_e_axes_pos(0);
  replot_editing_solid();

  return TCL_OK;
}

/*
 *			F _ S E D I T _ R E S E T
 */
int
f_sedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  struct bu_vls vls;

  if(state != ST_S_EDIT)
    return TCL_ERROR;

  if(argc != 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helpdevel sed_reset");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* free old copy */
  rt_db_free_internal( &es_int, &rt_uniresource );

  /* reset */
  es_pipept = (struct wdb_pipept *)NULL;
  es_s = (struct shell *)NULL;
  es_eu = (struct edgeuse *)NULL;

  /* read in a fresh copy */
  if( rt_db_get_internal( &es_int, LAST_SOLID(illump),
   dbip, NULL, &rt_uniresource ) < 0 )  {
    Tcl_AppendResult(interp, "sedit_reset(",
   			LAST_SOLID(illump)->d_namep,
		     "):  solid import failure\n", (char *)NULL);
    return TCL_ERROR;				/* FAIL */
  }
  RT_CK_DB_INTERNAL( &es_int );
  replot_editing_solid();

  /* Establish initial keypoint */
  es_keytag = "";
  get_solid_keypoint( es_keypoint, &es_keytag, &es_int, es_mat );

  /* Reset relevant variables */
  MAT_IDN(acc_rot_sol);
  VSETALL( edit_absolute_model_rotate, 0.0 );
  VSETALL( edit_absolute_object_rotate, 0.0 );
  VSETALL( edit_absolute_view_rotate, 0.0 );
  VSETALL( last_edit_absolute_model_rotate, 0.0 );
  VSETALL( last_edit_absolute_object_rotate, 0.0 );
  VSETALL( last_edit_absolute_view_rotate, 0.0 );
  VSETALL( edit_absolute_model_tran, 0.0 );
  VSETALL( edit_absolute_view_tran, 0.0 );
  VSETALL( last_edit_absolute_model_tran, 0.0 );
  VSETALL( last_edit_absolute_view_tran, 0.0 );
  edit_absolute_scale = 0.0;
  acc_sc_sol = 1.0;
  VSETALL( edit_rate_model_rotate, 0.0 );
  VSETALL( edit_rate_object_rotate, 0.0 );
  VSETALL( edit_rate_view_rotate, 0.0 );
  VSETALL( edit_rate_model_tran, 0.0 );
  VSETALL( edit_rate_view_tran, 0.0 );

  set_e_axes_pos(1);
  update_views = 1;

  /* active edit callback */
  bu_vls_init(&vls);
  bu_vls_printf(&vls, "active_edit_callback");
  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  return TCL_OK;
}

int
f_sedit_apply(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_vls vls;

	CHECK_DBI_NULL;

	if (not_state(ST_S_EDIT, "Primitive edit accept")) {
		Tcl_AppendResult(interp, "Must be in solid edit state!\n");
		return TCL_ERROR;
	}

	if (sedraw > 0)
		sedit();

	init_sedit_vars();
	(void)sedit_apply(0);

	/* active edit callback */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "active_edit_callback");
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_OK;
}

int
f_oedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_vls vls;

	if (state != ST_O_EDIT)
		return TCL_ERROR;

	if (argc != 1) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helpdevel oed_reset");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	oedit_reject();
	init_oedit_guts();

	new_edit_mats();
	update_views = 1;

	/* active edit callback */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "active_edit_callback");
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_OK;
}

int
f_oedit_apply(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_vls	vls;
	char		*strp="";

	CHECK_DBI_NULL;
	oedit_apply( UP ); /* apply changes, but continue editing */

	/* Save aggregate path matrix */
	MAT_IDN(es_mat);
	pathHmat(illump, es_mat, illump->s_fullpath.fp_len-2);

	/* get the inverse matrix */
	bn_mat_inv(es_invmat, es_mat);

	get_solid_keypoint(es_keypoint, &strp, &es_int, es_mat);
	init_oedit_vars();
	new_edit_mats();
	update_views = 1;

	/* active edit callback */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "active_edit_callback");
	(void)Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
