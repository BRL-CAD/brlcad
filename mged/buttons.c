/*
 *			B U T T O N S . C
 *
 * Functions -
 *	buttons		Process button-box functions
 *	press		button function request
 *	state_err	state error printer
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./menu.h"
#include "./scroll.h"
#include "./dm.h"
#include "./sedit.h"

#ifdef XMGED
extern point_t orig_pos;
extern int	update_views;
extern mat_t	ModelDelta;
extern struct menu_item edge8_menu[], edge7_menu[], edge6_menu[], edge5_menu[];
extern struct menu_item point4_menu[], tgc_menu[], tor_menu[], eto_menu[];
extern struct menu_item ell_menu[], ars_menu[], spline_menu[], nmg_menu[];
extern struct menu_item mv8_menu[], mv7_menu[], mv6_menu[], mv5_menu[], mv4_menu[];
extern struct menu_item rot8_menu[], rot7_menu[], rot6_menu[], rot5_menu[], rot4_menu[];
extern struct menu_item rpc_menu[], rhc_menu[], epa_menu[], ehy_menu[];

void (*adc_hook)();
#endif
int	adcflag;	/* angle/distance cursor in use */

/* This flag indicates that SOLID editing is in effect.
 * edobj may not be set at the same time.
 * It is set to the 0 if off, or the value of the button function
 * that is currently in effect (eg, BE_S_SCALE).
 */
static	edsol;

/* This flag indicates that OBJECT editing is in effect.
 * edsol may not be set at the same time.
 * Value is 0 if off, or the value of the button function currently
 * in effect (eg, BE_O_XY).
 */
int	edobj;		/* object editing */
int	movedir;	/* RARROW | UARROW | SARROW | ROTARROW */

/*
 * The "accumulation" solid rotation matrix and scale factor
 */
mat_t	acc_rot_sol;
fastf_t	acc_sc_sol;
fastf_t	acc_sc[3];	/* local object scale factors --- accumulations */

#ifdef XMGED
static void p_help();
static void edge8_doit(), edge7_doit(), edge6_doit(), edge5_doit();
static void mp4_doit(), tgc_doit(), tor_doit(), eto_doit(), ell_doit();
static void ars_doit(), spline_doit(), nmg_doit();
static void mf8_doit(), mf7_doit(), mf6_doit(), mf5_doit(), mf4_doit();
static void rf8_doit(), rf7_doit(), rf6_doit(), rf5_doit(), rf4_doit();
static void cntrl_doit(), rpc_doit(), rhc_doit(), epa_doit(), ehy_doit();
#endif
static void bv_zoomin(), bv_zoomout(), bv_rate_toggle();
static void bv_top(), bv_bottom(), bv_right();
static void bv_left(), bv_front(), bv_rear();
static void bv_vrestore(), bv_vsave(), bv_adcursor(), bv_reset();
static void bv_45_45(), bv_35_25();
static void be_o_illuminate(), be_s_illuminate();
static void be_o_scale(), be_o_x(), be_o_y(), be_o_xy(), be_o_rotate();
static void be_accept(), be_reject(), bv_slicemode();
static void be_s_edit(), be_s_rotate(), be_s_trans(), be_s_scale();
static void be_o_xscale(), be_o_yscale(), be_o_zscale();

struct buttons  {
	int	bu_code;	/* dm_values.dv_button */
	char	*bu_name;	/* keyboard string */
	void	(*bu_func)();	/* function to call */
#ifdef XMGED
	int     bu_param;       /* parameter to pass to bu_func */
#endif
}  button_table[] = {
#ifdef XMGED
	BV_35_25,	"35,25",	bv_35_25,		NULL,
	BV_45_45,	"45,45",	bv_45_45,		NULL,
	BE_ACCEPT,	"accept",	be_accept,		NULL,
	BV_ADCURSOR,	"adc",		bv_adcursor,		NULL,
	BV_BOTTOM,	"bottom",	bv_bottom,		NULL,
	BV_FRONT,	"front",	bv_front,		NULL,
	BV_LEFT,	"left",		bv_left,		NULL,
	BE_O_ILLUMINATE,"oill",		be_o_illuminate,	NULL,
	BE_O_ROTATE,	"orot",		be_o_rotate,		NULL,
	BE_O_SCALE,	"oscale",	be_o_scale,		NULL,
	BE_O_X,		"ox",		be_o_x,			NULL,
	BE_O_XY,	"oxy",		be_o_xy,		NULL,
	BE_O_XSCALE,	"oxscale",	be_o_xscale,		NULL,
	BE_O_Y,		"oy",		be_o_y,			NULL,
	BE_O_YSCALE,	"oyscale",	be_o_yscale,		NULL,
	BE_O_ZSCALE,	"ozscale",	be_o_zscale,		NULL,
	BV_REAR,	"rear",		bv_rear,		NULL,
	BE_REJECT,	"reject",	be_reject,		NULL,
	BV_RESET,	"reset",	bv_reset,		NULL,
	BV_VRESTORE,	"restore",	bv_vrestore,		NULL,
	BV_RIGHT,	"right",	bv_right,		NULL,
	BV_VSAVE,	"save",		bv_vsave,		NULL,
	BE_S_EDIT,	"sedit",	be_s_edit,		NULL,
	BE_S_ILLUMINATE,"sill",		be_s_illuminate,	NULL,
	BV_SLICEMODE,	"slice",	bv_slicemode,		NULL,
	BE_S_ROTATE,	"srot",		be_s_rotate,		NULL,
	BE_S_SCALE,	"sscale",	be_s_scale,		NULL,
	BE_S_TRANS,	"sxy",		be_s_trans,		NULL,
	BE_S_TRANS,	"stran",	be_s_trans,		NULL,
	BV_TOP,		"top",		bv_top,			NULL,
	BV_ZOOM_IN,	"zoomin",	bv_zoomin,              NULL,
	BV_ZOOM_OUT,	"zoomout",	bv_zoomout,             NULL,
	BV_RATE_TOGGLE, "rate",		bv_rate_toggle,         NULL,
	P_HELP,		"help",		p_help,			NULL,
	EDGE8_1,	"a8_me12",	edge8_doit,		1,
	EDGE8_2,	"a8_me23",	edge8_doit,		2,
	EDGE8_3,	"a8_me34",	edge8_doit,		3,
	EDGE8_4,	"a8_me14",	edge8_doit,		4,
	EDGE8_5,	"a8_me15",	edge8_doit,		5,
	EDGE8_6,	"a8_me26",	edge8_doit,		6,
	EDGE8_7,	"a8_me56",	edge8_doit,		7,
	EDGE8_8,	"a8_me67",	edge8_doit,		8,
	EDGE8_9,	"a8_me78",	edge8_doit,		9,
	EDGE8_10,	"a8_me58",	edge8_doit,		10,
	EDGE8_11,	"a8_me37",	edge8_doit,		11,
	EDGE8_12,	"a8_me48",	edge8_doit,		12,
	EDGE7_1,	"a7_me12",	edge7_doit,		1,
	EDGE7_2,	"a7_me23",	edge7_doit,		2,
	EDGE7_3,	"a7_me34",	edge7_doit,		3,
	EDGE7_4,	"a7_me14",	edge7_doit,		4,
	EDGE7_5,	"a7_me15",	edge7_doit,		5,
	EDGE7_6,	"a7_me26",	edge7_doit,		6,
	EDGE7_7,	"a7_me56",	edge7_doit,		7,
	EDGE7_8,	"a7_me67",	edge7_doit,		8,
	EDGE7_9,	"a7_me37",	edge7_doit,		9,
	EDGE7_10,	"a7_me57",	edge7_doit,		10,
	EDGE7_11,	"a7_me45",	edge7_doit,		11,
	EDGE7_12,	"a7_mp5",	edge7_doit,		12,
	EDGE6_1,	"a6_me12",	edge6_doit,		1,
	EDGE6_2,	"a6_me23",	edge6_doit,		2,
	EDGE6_3,	"a6_me34",	edge6_doit,		3,
	EDGE6_4,	"a6_me14",	edge6_doit,		4,
	EDGE6_5,	"a6_me15",	edge6_doit,		5,
	EDGE6_6,	"a6_me25",	edge6_doit,		6,
	EDGE6_7,	"a6_me36",	edge6_doit,		7,
	EDGE6_8,	"a6_me46",	edge6_doit,		8,
	EDGE6_9,	"a6_mp5",	edge6_doit,		9,
	EDGE6_10,	"a6_mp6",	edge6_doit,		10,
	EDGE5_1,	"a5_me12",	edge5_doit,		1,
	EDGE5_2,	"a5_me23",	edge5_doit,		2,
	EDGE5_3,	"a5_me34",	edge5_doit,		3,
	EDGE5_4,	"a5_me14",	edge5_doit,		4,
	EDGE5_5,	"a5_me15",	edge5_doit,		5,
	EDGE5_6,	"a5_me25",	edge5_doit,		6,
	EDGE5_7,	"a5_me35",	edge5_doit,		7,
	EDGE5_8,	"a5_me45",	edge5_doit,		8,
	EDGE5_9,	"a5_mp5",	edge5_doit,		9,
	POINT4_1,	"a4_mp1",	mp4_doit,		1,
	POINT4_2,	"a4_mp2",	mp4_doit,		2,
	POINT4_3,	"a4_mp3",	mp4_doit,		3,
	POINT4_4,	"a4_mp4",	mp4_doit,		4,
	TGC_1,		"tgc_sh",	tgc_doit,		1,
	TGC_2,		"tgc_sa",	tgc_doit,		2,
	TGC_3,		"tgc_sb",	tgc_doit,		3,
	TGC_4,		"tgc_sc",	tgc_doit,		4,
	TGC_5,		"tgc_sd",	tgc_doit,		5,
	TGC_6,		"tgc_sab",	tgc_doit,		6,
	TGC_7,		"tgc_scd",	tgc_doit,		7,
	TGC_8,		"tgc_sabcd",	tgc_doit,		8,
	TGC_9,		"tgc_rh",	tgc_doit,		9,
	TGC_10,		"tgc_raxb",	tgc_doit,		10,
	TGC_11,		"tgc_mh",	tgc_doit,		11,
	TGC_12,		"tgc_mhh",	tgc_doit,		12,
	TOR_1,		"tor_sr1",	tor_doit,		1,
	TOR_2,		"tor_sr2",	tor_doit,		2,
	ETO_1,		"eto_sr",	eto_doit,		1,
	ETO_2,		"eto_sd",	eto_doit,		2,
	ETO_3,		"eto_sc",	eto_doit,		3,
	ETO_4,		"eto_rc",	eto_doit,		4,
	ELL_1,		"ell_sa",	ell_doit,		1,
	ELL_2,		"ell_sb",	ell_doit,		2,
	ELL_3,		"ell_sc",	ell_doit,		3,
	ELL_4,		"ell_sabc",	ell_doit,		4,
	ARS_1,		"ars",		ars_doit,		1,
	SPLINE_1,	"spl_pv",	spline_doit,		1,
	SPLINE_2,	"spl_mv",	spline_doit,		2,
	NMG_1,          "nmg_pe",       nmg_doit,               1,
	NMG_2,          "nmg_me",       nmg_doit,               2,
        NMG_3,          "nmg_de",       nmg_doit,               3,
        NMG_4,          "nmg_se",       nmg_doit,               4,
        NMG_5,          "nmg_del",      nmg_doit,               5,
        NMG_6,          "nmg_neu",      nmg_doit,               6,
        NMG_7,          "nmg_peu",      nmg_doit,               7,
	NMG_8,          "nmg_reu",      nmg_doit,               8,
	MV8_1,		"a8_mf1234",	mf8_doit,		1,
	MV8_2,		"a8_mf5678",	mf8_doit,		2,
	MV8_3,		"a8_mf1584",	mf8_doit,		3,
	MV8_4,		"a8_mf2376",	mf8_doit,		4,
	MV8_5,		"a8_mf1265",	mf8_doit,		5,
	MV8_6,		"a8_mf4378",	mf8_doit,		6,
	MV7_1,		"a7_mf1234",	mf7_doit,		1,
	MV7_2,		"a7_mf2376",	mf7_doit,		2,
	MV6_1,		"a6_mf1234",	mf6_doit,		1,
	MV6_2,		"a6_mf2365",	mf6_doit,		2,
	MV6_3,		"a6_mf1564",	mf6_doit,		3,
	MV6_4,		"a6_mf125",	mf6_doit,		4,
	MV6_5,		"a6_mf346",	mf6_doit,		5,
	MV5_1,		"a5_mf1234",	mf5_doit,		1,
	MV5_2,		"a5_mf125",	mf5_doit,		2,
	MV5_3,		"a5_mf235",	mf5_doit,		3,
	MV5_4,		"a5_mf345",	mf5_doit,		4,
	MV5_5,		"a5_mf145",	mf5_doit,		5,
	MV4_1,		"a4_mf123",	mf4_doit,		1,
	MV4_2,		"a4_mf124",	mf4_doit,		2,
	MV4_3,		"a4_mf234",	mf4_doit,		3,
	MV4_4,		"a4_mf134",	mf4_doit,		4,
	ROT8_1,		"a8_rf1234",	rf8_doit,		1,
	ROT8_2,		"a8_rf5678",	rf8_doit,		2,
	ROT8_3,		"a8_rf1584",	rf8_doit,		3,
	ROT8_4,		"a8_rf2376",	rf8_doit,		4,
	ROT8_5,		"a8_rf1265",	rf8_doit,		5,
	ROT8_6,		"a8_rf4378",	rf8_doit,		6,
	ROT7_1,		"a7_rf1234",	rf7_doit,		1,
	ROT7_2,		"a7_rf567",	rf7_doit,		2,
	ROT7_3,		"a7_rf145",	rf7_doit,		3,
	ROT7_4,		"a7_rf2376",	rf7_doit,		4,
	ROT7_5,		"a7_rf1265",	rf7_doit,		5,
	ROT7_6,		"a7_rf4375",	rf7_doit,		6,
	ROT6_1,		"a6_rf1234",	rf6_doit,		1,
	ROT6_2,		"a6_rf2365",	rf6_doit,		2,
	ROT6_3,		"a6_rf1564",	rf6_doit,		3,
	ROT6_4,		"a6_rf125",	rf6_doit,		4,
	ROT6_5,		"a6_rf346",	rf6_doit,		5,
	ROT5_1,		"a5_rf1234",	rf5_doit,		1,
	ROT5_2,		"a5_rf125",	rf5_doit,		2,
	ROT5_3,		"a5_rf235",	rf5_doit,		3,
	ROT5_4,		"a5_rf345",	rf5_doit,		4,
	ROT5_5,		"a5_rf145",	rf5_doit,		5,
	ROT4_1,		"a4_rf123",	rf4_doit,		1,
	ROT4_2,		"a4_rf124",	rf4_doit,		2,
	ROT4_3,		"a4_rf234",	rf4_doit,		3,
	ROT4_4,		"a4_rf134",	rf4_doit,		4,
	RPC_1,		"rpc_sb",	rpc_doit,		1,
	RPC_2,		"rpc_sh",	rpc_doit,		2,
	RPC_3,		"rpc_sr",	rpc_doit,		3,
	RHC_1,		"rhc_sb",	rhc_doit,		1,
	RHC_2,		"rhc_sh",	rhc_doit,		2,
	RHC_3,		"rhc_sr",	rhc_doit,		3,
	RHC_4,		"rhc_sc",	rhc_doit,		4,
	EPA_1,		"epa_sh",	epa_doit,		1,
	EPA_2,		"epa_sa",	epa_doit,		2,
	EPA_3,		"epa_sb",	epa_doit,		3,
	EHY_1,		"ehy_sh",	ehy_doit,		1,
	EHY_2,		"ehy_sa",	ehy_doit,		2,
	EHY_3,		"ehy_sb",	ehy_doit,		3,
	EHY_4,		"ehy_sc",	ehy_doit,		4,
#else
	BV_35_25,	"35,25",	bv_35_25,
	BV_45_45,	"45,45",	bv_45_45,
	BE_ACCEPT,	"accept",	be_accept,
	BV_ADCURSOR,	"adc",		bv_adcursor,
	BV_BOTTOM,	"bottom",	bv_bottom,
	BV_FRONT,	"front",	bv_front,
	BV_LEFT,	"left",		bv_left,
	BE_O_ILLUMINATE,"oill",		be_o_illuminate,
	BE_O_ROTATE,	"orot",		be_o_rotate,
	BE_O_SCALE,	"oscale",	be_o_scale,
	BE_O_X,		"ox",		be_o_x,
	BE_O_XY,	"oxy",		be_o_xy,
	BE_O_XSCALE,	"oxscale",	be_o_xscale,
	BE_O_Y,		"oy",		be_o_y,
	BE_O_YSCALE,	"oyscale",	be_o_yscale,
	BE_O_ZSCALE,	"ozscale",	be_o_zscale,
	BV_REAR,	"rear",		bv_rear,
	BE_REJECT,	"reject",	be_reject,
	BV_RESET,	"reset",	bv_reset,
	BV_VRESTORE,	"restore",	bv_vrestore,
	BV_RIGHT,	"right",	bv_right,
	BV_VSAVE,	"save",		bv_vsave,
	BE_S_EDIT,	"sedit",	be_s_edit,
	BE_S_ILLUMINATE,"sill",		be_s_illuminate,
	BV_SLICEMODE,	"slice",	bv_slicemode,
	BE_S_ROTATE,	"srot",		be_s_rotate,
	BE_S_SCALE,	"sscale",	be_s_scale,
	BE_S_TRANS,	"sxy",		be_s_trans,
	BV_TOP,		"top",		bv_top,
	BV_ZOOM_IN,	"zoomin",	bv_zoomin,
	BV_ZOOM_OUT,	"zoomout",	bv_zoomout,
	BV_RATE_TOGGLE, "rate",		bv_rate_toggle,
#endif
	-1,		"-end-",	be_reject
};

static mat_t sav_viewrot, sav_toviewcenter;
static fastf_t sav_vscale;
static int	vsaved = 0;	/* set iff view saved */

extern void	sl_halt_scroll();	/* in scroll.c */
extern void	sl_toggle_scroll();

void		btn_head_menu();
void		btn_item_hit();

static struct menu_item first_menu[] = {
	{ "(BUTTON MENU)", btn_head_menu, 1 },		/* chg to 2nd menu */
	{ "", (void (*)())NULL, 0 }
};
static struct menu_item second_menu[] = {
	{ "BUTTON MENU", btn_head_menu, 0 },	/* chg to 1st menu */
	{ "REJECT Edit", btn_item_hit, BE_REJECT },
	{ "ACCEPT Edit", btn_item_hit, BE_ACCEPT },
	{ "35,25", btn_item_hit, BV_35_25 },
	{ "Top", btn_item_hit, BV_TOP },
	{ "Right", btn_item_hit, BV_RIGHT },
	{ "Front", btn_item_hit, BV_FRONT },
	{ "45,45", btn_item_hit, BV_45_45 },
	{ "Restore View", btn_item_hit, BV_VRESTORE },
	{ "Save View", btn_item_hit, BV_VSAVE },
	{ "Ang/Dist Curs", btn_item_hit, BV_ADCURSOR },
	{ "Reset Viewsize", btn_item_hit, BV_RESET },
	{ "Zero Sliders", sl_halt_scroll, 0 },
	{ "Sliders", sl_toggle_scroll, 0 },
	{ "Rate/Abs", btn_item_hit, BV_RATE_TOGGLE },
	{ "Zoom In 2X", btn_item_hit, BV_ZOOM_IN },
	{ "Zoom Out 2X", btn_item_hit, BV_ZOOM_OUT },
	{ "Solid Illum", btn_item_hit, BE_S_ILLUMINATE },
	{ "Object Illum", btn_item_hit, BE_O_ILLUMINATE },
	{ "", (void (*)())NULL, 0 }
};
static struct menu_item sed_menu[] = {
	{ "*SOLID EDIT*", btn_head_menu, 2 },
	{ "edit menu", btn_item_hit, BE_S_EDIT },
	{ "Rotate", btn_item_hit, BE_S_ROTATE },
	{ "Translate", btn_item_hit, BE_S_TRANS },
	{ "Scale", btn_item_hit, BE_S_SCALE },
	{ "", (void (*)())NULL, 0 }
};

static struct menu_item oed_menu[] = {
	{ "*OBJ EDIT*", btn_head_menu, 2 },
	{ "Scale", btn_item_hit, BE_O_SCALE },
	{ "X move", btn_item_hit, BE_O_X },
	{ "Y move", btn_item_hit, BE_O_Y },
	{ "XY move", btn_item_hit, BE_O_XY },
	{ "Rotate", btn_item_hit, BE_O_ROTATE },
	{ "Scale X", btn_item_hit, BE_O_XSCALE },
	{ "Scale Y", btn_item_hit, BE_O_YSCALE },
	{ "Scale Z", btn_item_hit, BE_O_ZSCALE },
	{ "", (void (*)())NULL, 0 }
};

/*
 *			B U T T O N
 */
void
button( bnum )
register int bnum;
{
	register struct buttons *bp;

	if( edsol && edobj )
		(void)rt_log("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj );

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( bnum != bp->bu_code )
			continue;
#ifdef XMGED
		bp->bu_func(bp->bu_param);
#else
		bp->bu_func();
#endif
		return;
	}
	(void)rt_log("button(%d):  Not a defined operation\n", bnum);
}

/*
 *  			P R E S S
 */
void
press( str )
char *str;{
	register struct buttons *bp;

	if( edsol && edobj )
		(void)rt_log("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj );

	if(strcmp(str, "help") == 0) {
		for( bp = button_table; bp->bu_code >= 0; bp++ )
			col_item(bp->bu_name);
		col_eol();
		return;
	}

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( strcmp( str, bp->bu_name) )
			continue;
#ifdef XMGED
		bp->bu_func(bp->bu_param);
#else
		bp->bu_func();
#endif
		return;
	}
	(void)rt_log("press(%s):  Unknown operation, type 'press help' for help\n",
		str);
}
/*
 *  			L A B E L _ B U T T O N
 *  
 *  For a given GED button number, return the "press" ID string.
 *  Useful for displays with programable button lables, etc.
 */
char *
label_button(bnum)
int bnum;
{
	register struct buttons *bp;

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( bnum != bp->bu_code )
			continue;
		return( bp->bu_name );
	}
	(void)rt_log("label_button(%d):  Not a defined operation\n", bnum);
	return("");
}

#ifdef XMGED
/*
 * Help for press command
 */
static void
p_help(param)
int	param;
{
	register struct buttons *bp;

	for( bp = button_table; bp->bu_code >= 0; bp++ )
		col_item(bp->bu_name);
	col_eol();
	return;
}

static void
edge8_doit(param)
int	param;
{
	(*edge8_menu[param].menu_func)(edge8_menu[param].menu_arg);
}

static void
edge7_doit(param)
int	param;
{
	(*edge7_menu[param].menu_func)(edge7_menu[param].menu_arg);
}

static void
edge6_doit(param)
int	param;
{
	(*edge6_menu[param].menu_func)(edge6_menu[param].menu_arg);
}

static void
edge5_doit(param)
int	param;
{
	(*edge5_menu[param].menu_func)(edge5_menu[param].menu_arg);
}

static void
mp4_doit(param)
int	param;
{
	(*point4_menu[param].menu_func)(point4_menu[param].menu_arg);
}

static void
tgc_doit(param)
int	param;
{
	(*tgc_menu[param].menu_func)(tgc_menu[param].menu_arg);
}

static void
tor_doit(param)
int	param;
{
	(*tor_menu[param].menu_func)(tor_menu[param].menu_arg);
}

static void
eto_doit(param)
int	param;
{
	(*eto_menu[param].menu_func)(eto_menu[param].menu_arg);
}

static void
ell_doit(param)
int	param;
{
	(*ell_menu[param].menu_func)(ell_menu[param].menu_arg);
}

static void
ars_doit(param)
int	param;
{
	(*ars_menu[param].menu_func)(ars_menu[param].menu_arg);
}

static void
spline_doit(param)
int	param;
{
	(*spline_menu[param].menu_func)(spline_menu[param].menu_arg);
}

static void
nmg_doit(param)
int	param;
{
	(*nmg_menu[param].menu_func)(nmg_menu[param].menu_arg);
}

static void
mf8_doit(param)
int	param;
{
	(*mv8_menu[param].menu_func)(mv8_menu[param].menu_arg);
}

static void
mf7_doit(param)
int	param;
{
	(*mv7_menu[param].menu_func)(mv7_menu[param].menu_arg);
}

static void
mf6_doit(param)
int	param;
{
	(*mv6_menu[param].menu_func)(mv6_menu[param].menu_arg);
}

static void
mf5_doit(param)
int	param;
{
	(*mv5_menu[param].menu_func)(mv5_menu[param].menu_arg);
}

static void
mf4_doit(param)
int	param;
{
	(*mv4_menu[param].menu_func)(mv4_menu[param].menu_arg);
}

static void
rf8_doit(param)
int	param;
{
	(*rot8_menu[param].menu_func)(rot8_menu[param].menu_arg);
}

static void
rf7_doit(param)
int	param;
{
	(*rot7_menu[param].menu_func)(rot7_menu[param].menu_arg);
}

static void
rf6_doit(param)
int	param;
{
	(*rot6_menu[param].menu_func)(rot6_menu[param].menu_arg);
}

static void
rf5_doit(param)
int	param;
{
	(*rot5_menu[param].menu_func)(rot5_menu[param].menu_arg);
}

static void
rf4_doit(param)
int	param;
{
	(*rot4_menu[param].menu_func)(rot4_menu[param].menu_arg);
}

static void
rpc_doit(param)
int	param;
{
	(*rpc_menu[param].menu_func)(rpc_menu[param].menu_arg);
}

static void
rhc_doit(param)
int	param;
{
	(*rhc_menu[param].menu_func)(rhc_menu[param].menu_arg);
}

static void
epa_doit(param)
int	param;
{
	(*epa_menu[param].menu_func)(epa_menu[param].menu_arg);
}

static void
ehy_doit(param)
int	param;
{
	(*ehy_menu[param].menu_func)(ehy_menu[param].menu_arg);
}
#endif

static void bv_zoomin()
{
	Viewscale *= 0.5;
	new_mats();
	dmaflag = 1;
}

static void bv_zoomout()
{
	Viewscale *= 2;
	new_mats();
	dmaflag = 1;
}

static void bv_rate_toggle()
{
	mged_variables.rateknobs = !mged_variables.rateknobs;
	dmaflag = 1;
}

static void bv_top()  {
	/* Top view */
	setview( 0.0, 0.0, 0.0 );
}

static void bv_bottom()  {
	/* Bottom view */
	setview( 180.0, 0.0, 0.0 );
}

static void bv_right()  {
	/* Right view */
	setview( 270.0, 0.0, 0.0 );
}

static void bv_left()  {
	/* Left view */
	setview( 270.0, 0.0, 180.0 );
}

static void bv_front()  {
	/* Front view */
	setview( 270.0, 0.0, 270.0 );
}

static void bv_rear()  {
	/* Rear view */
	setview( 270.0, 0.0, 90.0 );
}

static void bv_vrestore()  {
	/* restore to saved view */
	if ( vsaved )  {
		Viewscale = sav_vscale;
		mat_copy( Viewrot, sav_viewrot );
		mat_copy( toViewcenter, sav_toviewcenter );
		new_mats();
		dmaflag++;
	}
}

static void bv_vsave()  {
	/* save current view */
	sav_vscale = Viewscale;
	mat_copy( sav_viewrot, Viewrot );
	mat_copy( sav_toviewcenter, toViewcenter );
	vsaved = 1;
	dmp->dmr_light( LIGHT_ON, BV_VRESTORE );
}

static void bv_adcursor()  {
	if (adcflag)  {
		/* Was on, turn off */
		adcflag = 0;
		dmp->dmr_light( LIGHT_OFF, BV_ADCURSOR );
	}  else  {
		/* Was off, turn on */
		adcflag = 1;
		dmp->dmr_light( LIGHT_ON, BV_ADCURSOR );
	}
#ifdef XMGED
		 if(adc_hook)
		   (*adc_hook)(0);  /* toggle adc button */
#endif
	dmaflag = 1;
}

static void bv_reset()  {
	/* Reset view such that all solids can be seen */
#ifdef XMGED
/*XXXX*/        mat_idn( ModelDelta );
#endif
	size_reset();
	setview( 0.0, 0.0, 0.0 );
}

static void bv_45_45()  {
	setview( 270.0+45.0, 0.0, 270.0-45.0 );
}

static void bv_35_25()  {
	/* Use Azmuth=35, Elevation=25 in GIFT's backwards space */
	setview( 270.0+25.0, 0.0, 270.0-35.0 );
}

/* returns 0 if error, !0 if success */
static int ill_common()  {
	/* Common part of illumination */
	dmp->dmr_light( LIGHT_ON, BE_REJECT );
	if( HeadSolid.s_forw == &HeadSolid )  {
		(void)rt_log("no solids in view\n");
		return(0);	/* BAD */
	}
	illump = HeadSolid.s_forw;/* any valid solid would do */
	edobj = 0;		/* sanity */
	edsol = 0;		/* sanity */
	movedir = 0;		/* No edit modes set */
	mat_idn( modelchanges );	/* No changes yet */
	dmaflag++;
#ifdef XMGED
	update_views = 1;
#endif
	return(1);		/* OK */
}

static void be_o_illuminate()  {
	if( not_state( ST_VIEW, "Object Illuminate" ) )
		return;

	dmp->dmr_light( LIGHT_ON, BE_O_ILLUMINATE );
	if( ill_common() )  {
		(void)chg_state( ST_VIEW, ST_O_PICK, "Object Illuminate" );
		new_mats();
	}
	/* reset accumulation local scale factors */
	acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;
}

static void be_s_illuminate()  {
	if( not_state( ST_VIEW, "Solid Illuminate" ) )
		return;

	dmp->dmr_light( LIGHT_ON, BE_S_ILLUMINATE );
	if( ill_common() )  {
		(void)chg_state( ST_VIEW, ST_S_PICK, "Solid Illuminate" );
		new_mats();
	}
}

static void be_o_scale()  {
	if( not_state( ST_O_EDIT, "Object Scale" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_SCALE );
	movedir = SARROW;
	dmaflag++;
}

static void be_o_xscale()  {
	if( not_state( ST_O_EDIT, "Object Local X Scale" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_XSCALE );
	movedir = SARROW;
	dmaflag++;
}

static void be_o_yscale()  {
	if( not_state( ST_O_EDIT, "Object Local Y Scale" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_YSCALE );
	movedir = SARROW;
	dmaflag++;
}

static void be_o_zscale()  {
	if( not_state( ST_O_EDIT, "Object Local Z Scale" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_ZSCALE );
	movedir = SARROW;
	dmaflag++;
}

static void be_o_x()  {
	if( not_state( ST_O_EDIT, "Object X Motion" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_X );
	movedir = RARROW;
	dmaflag++;
}

static void be_o_y()  {
	if( not_state( ST_O_EDIT, "Object Y Motion" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_Y );
	movedir = UARROW;
	dmaflag++;
}


static void be_o_xy()  {
	if( not_state( ST_O_EDIT, "Object XY Motion" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_XY );
	movedir = UARROW | RARROW;
	dmaflag++;
}

static void be_o_rotate()  {
	if( not_state( ST_O_EDIT, "Object Rotation" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edobj );
	dmp->dmr_light( LIGHT_ON, edobj = BE_O_ROTATE );
	movedir = ROTARROW;
	dmaflag++;
}

static void be_accept()  {
	register struct solid *sp;

#ifdef XMGED
	update_views = 1;
#endif

	mmenu_set( MENU_L2, MENU_NULL );
	if( state == ST_S_EDIT )  {
		/* Accept a solid edit */
		dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
		dmp->dmr_light( LIGHT_OFF, BE_REJECT );
		dmp->dmr_light( LIGHT_OFF, edsol );
		edsol = 0;

		sedit_accept();		/* zeros "edsol" var */

		mmenu_set( MENU_L1, MENU_NULL );
		mmenu_set( MENU_L2, MENU_NULL );
		dmp->dmr_light( LIGHT_OFF, BE_S_EDIT );

		FOR_ALL_SOLIDS( sp )
			sp->s_iflag = DOWN;

		illump = SOLID_NULL;
		dmp->dmr_colorchange();
		(void)chg_state( ST_S_EDIT, ST_VIEW, "Edit Accept" );
		dmaflag = 1;		/* show completion */
	}  else if( state == ST_O_EDIT )  {
		/* Accept an object edit */
		dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
		dmp->dmr_light( LIGHT_OFF, BE_REJECT );
		dmp->dmr_light( LIGHT_OFF, edobj );
		edobj = 0;
		movedir = 0;	/* No edit modes set */

		oedit_accept();

		illump = SOLID_NULL;
		dmp->dmr_colorchange();
		(void)chg_state( ST_O_EDIT, ST_VIEW, "Edit Accept" );
		dmaflag = 1;		/* show completion */
	} else {
		(void)not_state( ST_S_EDIT, "Edit Accept" );
		return;
	}
}

static void be_reject()  {
	register struct solid *sp;

#ifdef XMGED
	update_views = 1;
#endif

	/* Reject edit */
	dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
	dmp->dmr_light( LIGHT_OFF, BE_REJECT );

	switch( state )  {
	default:
		state_err( "Edit Reject" );
		return;

	case ST_S_EDIT:
		/* Reject a solid edit */
		if( edsol )
			dmp->dmr_light( LIGHT_OFF, edsol );
		mmenu_set( MENU_L1, MENU_NULL );
		mmenu_set( MENU_L2, MENU_NULL );

		sedit_reject();
		break;

	case ST_O_EDIT:
		if( edobj )
			dmp->dmr_light( LIGHT_OFF, edobj );
		mmenu_set( MENU_L1, MENU_NULL );
		mmenu_set( MENU_L2, MENU_NULL );

		oedit_reject();
		break;
	case ST_O_PICK:
		dmp->dmr_light( LIGHT_OFF, BE_O_ILLUMINATE );
		break;
	case ST_S_PICK:
		dmp->dmr_light( LIGHT_OFF, BE_S_ILLUMINATE );
		break;
	case ST_O_PATH:
		break;
	}

	menuflag = 0;
	movedir = 0;
	edsol = 0;
	edobj = 0;
	es_edflag = -1;
	illump = SOLID_NULL;		/* None selected */
	dmaflag = 1;

	/* Clear illumination flags */
	FOR_ALL_SOLIDS( sp )
		sp->s_iflag = DOWN;
	dmp->dmr_colorchange();
	(void)chg_state( state, ST_VIEW, "Edit Reject" );
}

static void bv_slicemode() {
}

static void be_s_edit()  {
	/* solid editing */
	if( not_state( ST_S_EDIT, "Solid Edit (Menu)" ) )
		return;

	if( edsol )
		dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_EDIT );
	sedit_menu();		/* Install appropriate menu */
	dmaflag++;
#ifdef XMGED
	set_e_axis_pos();
#endif
}

static void be_s_rotate()  {
	/* rotate solid */
	if( not_state( ST_S_EDIT, "Solid Rotate" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_ROTATE );
	mmenu_set( MENU_L1, MENU_NULL );
	es_edflag = SROT;
	mat_idn(acc_rot_sol);
	dmaflag++;
#ifdef XMGED
        set_e_axis_pos();
#endif
}

static void be_s_trans()  {
	/* translate solid */
	if( not_state( ST_S_EDIT, "Solid Translate" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_TRANS );
	es_edflag = STRANS;
	movedir = UARROW | RARROW;
	mmenu_set( MENU_L1, MENU_NULL );
	dmaflag++;
#ifdef XMGED
        set_e_axis_pos();
#endif
}

static void be_s_scale()  {
	/* scale solid */
	if( not_state( ST_S_EDIT, "Solid Scale" ) )
		return;

	dmp->dmr_light( LIGHT_OFF, edsol );
	dmp->dmr_light( LIGHT_ON, edsol = BE_S_SCALE );
	es_edflag = SSCALE;
	mmenu_set( MENU_L1, MENU_NULL );
	acc_sc_sol = 1.0;
	dmaflag++;
#ifdef XMGED
        set_e_axis_pos();
#endif
}

/*
 *			N O T _ S T A T E
 *  
 *  Returns 0 if current state is as desired,
 *  Returns !0 and prints error message if state mismatch.
 */
int
not_state( desired, str )
int desired;
char *str;
{
	if( state != desired ) {
		(void)rt_log("Unable to do <%s> from %s state.\n",
			str, state_str[state] );
		return(1);	/* BAD */
	}
	return(0);		/* GOOD */
}

/*
 *  			C H G _ S T A T E
 *
 *  Returns 0 if state change is OK,
 *  Returns !0 and prints error message if error.
 */
int
chg_state( from, to, str )
int from, to;
char *str;
{
	if( state != from ) {
		(void)rt_log("Unable to do <%s> going from %s to %s state.\n",
			str, state_str[from], state_str[to] );
		return(1);	/* BAD */
	}
	state = to;
	/* Advise display manager of state change */
	dmp->dmr_statechange( from, to );

#ifdef XMGED
	if(to == ST_VIEW){
	  mat_t o_toViewcenter;
	  fastf_t o_Viewscale;

	  /* save toViewcenter and Viewscale */
	  mat_copy(o_toViewcenter, toViewcenter);
	  o_Viewscale = Viewscale;

	  /* get new orig_pos */
	  size_reset();
	  MAT_DELTAS_GET(orig_pos, toViewcenter);

	  /* restore old toViewcenter and Viewscale */
	  mat_copy(toViewcenter, o_toViewcenter);
	  Viewscale = o_Viewscale;
	}
#endif

	return(0);		/* GOOD */
}

void
state_err( str )
char *str;
{
	(void)rt_log("Unable to do <%s> from %s state.\n",
		str, state_str[state] );
}


/*
 *			B T N _ I T E M _ H I T
 *
 *  Called when a menu item is hit
 */
void btn_item_hit(arg, menu, item)  {
	button(arg);
	if( menu == MENU_GEN && 
	    ( arg != BE_O_ILLUMINATE && arg != BE_S_ILLUMINATE) )
		menuflag = 0;
}

/*
 *			B T N _ H E A D _ M E N U
 *
 *  Called to handle hits on menu heads.
 *  Also called from main() with arg 0 in init.
 */
void
btn_head_menu(i, menu, item)  {
	switch(i)  {
	case 0:
		mmenu_set( MENU_GEN, first_menu );
		break;
	case 1:
		mmenu_set( MENU_GEN, second_menu );
		break;
	case 2:
		/* nothing happens */
		break;
	default:
		rt_log("btn_head_menu(%d): bad arg\n", i);
		break;
	}
	dmaflag = 1;
}

void
chg_l2menu(i)  {
	switch( i )  {
	case ST_S_EDIT:
		mmenu_set( MENU_L2, sed_menu );
		break;
	case ST_O_EDIT:
		mmenu_set( MENU_L2, oed_menu );
		break;
	default:
		(void)rt_log("chg_l2menu(%d): bad arg\n");
		break;
	}
}
