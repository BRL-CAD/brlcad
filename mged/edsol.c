/*
 *			E D S O L . C
 *
 * Functions -
 *	init_sedit	set up for a Solid Edit
 *	sedit		Apply Solid Edit transformation(s)
 *	pscale		Partial scaling of a solid
 *	init_objedit	set up for object edit?
 *	f_eqn		change face of GENARB8 to new equation
 *
 *  Authors -
 *	Keith A. Applin
 *	Bob Suckling
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

#include <stdio.h>
#include <math.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "externs.h"

#include "./ged.h"
#include "./solid.h"
#include "./sedit.h"
#include "./dm.h"
#include "./menu.h"

extern char    *cmd_args[];
extern int 	numargs;

static int	new_way = 0;	/* Set 1 for import/export handling */

static void	arb8_edge(), ars_ed(), ell_ed(), tgc_ed(), tor_ed(), spline_ed();
static void	eto_ed();
static void	arb7_edge(), arb6_edge(), arb5_edge(), arb4_point();
static void	arb8_mv_face(), arb7_mv_face(), arb6_mv_face();
static void	arb5_mv_face(), arb4_mv_face(), arb8_rot_face(), arb7_rot_face();
static void 	arb6_rot_face(), arb5_rot_face(), arb4_rot_face(), arb_control();

void pscale();
void	calc_planes();
static short int fixv;		/* used in ECMD_ARB_ROTATE_FACE,f_eqn(): fixed vertex */

/* data for solid editing */
int			sedraw;	/* apply solid editing changes */

struct rt_external	es_ext;
struct rt_db_internal	es_int;
struct rt_db_internal	es_int_orig;

union record es_rec;		/* current solid record */
int     es_edflag;		/* type of editing for this solid */
fastf_t	es_scale;		/* scale factor */
fastf_t	es_para[3];		/* keyboard input parameter changes */
fastf_t	es_peqn[7][4];		/* ARBs defining plane equations */
fastf_t	es_m[3];		/* edge(line) slope */
mat_t	es_mat;			/* accumulated matrix of path */ 
mat_t 	es_invmat;		/* inverse of es_mat   KAA */

point_t	es_keypoint;		/* center of editing xforms */
char	*es_keytag;		/* string identifying the keypoint */

/*  These values end up in es_menu, as do ARB vertex numbers */
int	es_menu;		/* item selected from menu */
#define MENU_TOR_R1		21
#define MENU_TOR_R2		22
#define MENU_TGC_ROT_H		23
#define MENU_TGC_ROT_AB 	24
#define	MENU_TGC_MV_H		25
#define MENU_TGC_MV_HH		26
#define MENU_TGC_SCALE_H	27
#define MENU_TGC_SCALE_A	28
#define MENU_TGC_SCALE_B	29
#define MENU_TGC_SCALE_C	30
#define MENU_TGC_SCALE_D	31
#define MENU_TGC_SCALE_AB	32
#define MENU_TGC_SCALE_CD	33
#define MENU_TGC_SCALE_ABCD	34
#define MENU_ARB_MV_EDGE	35
#define MENU_ARB_MV_FACE	36
#define MENU_ARB_ROT_FACE	37
#define MENU_ELL_SCALE_A	38
#define MENU_ELL_SCALE_B	39
#define MENU_ELL_SCALE_C	40
#define MENU_ELL_SCALE_ABC	41
#define MENU_ETO_R1		42
#define MENU_ETO_R2		43

/*
 *			M A T _ S C A L E _ A B O U T _ P T
 *
 *  Build a matrix to scale uniformly around a given point.
 *
 *  Returns -
 *	-1	if scale is too small.
 *	 0	if OK.
 */
int
mat_scale_about_pt( mat, pt, scale )
mat_t		mat;
CONST point_t	pt;
CONST double	scale;
{
	mat_t	xlate;
	mat_t	s;
	mat_t	tmp;

	mat_idn( xlate );
	MAT_DELTAS_VEC_NEG( xlate, pt );

	mat_idn( s );
	if( NEAR_ZERO( scale, SMALL ) )  {
		mat_zero( mat );
		return -1;			/* ERROR */
	}
	s[15] = 1/scale;

	mat_mul( tmp, s, xlate );

	MAT_DELTAS_VEC( xlate, pt );
	mat_mul( mat, xlate, tmp );
	return 0;				/* OK */
}

/*
 *			M A T _ X F O R M _ A B O U T _ P T
 *
 *  Build a matrix to apply arbitary 4x4 transformation around a given point.
 */
void
mat_xform_about_pt( mat, xform, pt )
mat_t		mat;
CONST mat_t	xform;
CONST point_t	pt;
{
	mat_t	xlate;
	mat_t	tmp;

	mat_idn( xlate );
	MAT_DELTAS_VEC_NEG( xlate, pt );

	mat_mul( tmp, xform, xlate );

	MAT_DELTAS_VEC( xlate, pt );
	mat_mul( mat, xlate, tmp );
}

extern int arb_faces[5][24];	/* from edarb.c */
extern int arb_planes[5][24];	/* from edarb.c */

struct menu_item  edge8_menu[] = {
	{ "ARB8 EDGES", (void (*)())NULL, 0 },
	{ "move edge 12", arb8_edge, 0 },
	{ "move edge 23", arb8_edge, 1 },
	{ "move edge 34", arb8_edge, 2 },
	{ "move edge 14", arb8_edge, 3 },
	{ "move edge 15", arb8_edge, 4 },
	{ "move edge 26", arb8_edge, 5 },
	{ "move edge 56", arb8_edge, 6 },
	{ "move edge 67", arb8_edge, 7 },
	{ "move edge 78", arb8_edge, 8 },
	{ "move edge 58", arb8_edge, 9 },
	{ "move edge 37", arb8_edge, 10 },
	{ "move edge 48", arb8_edge, 11 },
	{ "RETURN",       arb8_edge, 12 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  edge7_menu[] = {
	{ "ARB7 EDGES", (void (*)())NULL, 0 },
	{ "move edge 12", arb7_edge, 0 },
	{ "move edge 23", arb7_edge, 1 },
	{ "move edge 34", arb7_edge, 2 },
	{ "move edge 14", arb7_edge, 3 },
	{ "move edge 15", arb7_edge, 4 },
	{ "move edge 26", arb7_edge, 5 },
	{ "move edge 56", arb7_edge, 6 },
	{ "move edge 67", arb7_edge, 7 },
	{ "move edge 37", arb7_edge, 8 },
	{ "move edge 57", arb7_edge, 9 },
	{ "move edge 45", arb7_edge, 10 },
	{ "move point 5", arb7_edge, 11 },
	{ "RETURN",       arb7_edge, 12 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  edge6_menu[] = {
	{ "ARB6 EDGES", (void (*)())NULL, 0 },
	{ "move edge 12", arb6_edge, 0 },
	{ "move edge 23", arb6_edge, 1 },
	{ "move edge 34", arb6_edge, 2 },
	{ "move edge 14", arb6_edge, 3 },
	{ "move edge 15", arb6_edge, 4 },
	{ "move edge 25", arb6_edge, 5 },
	{ "move edge 36", arb6_edge, 6 },
	{ "move edge 46", arb6_edge, 7 },
	{ "move point 5", arb6_edge, 8 },
	{ "move point 6", arb6_edge, 9 },
	{ "RETURN",       arb6_edge, 10 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  edge5_menu[] = {
	{ "ARB5 EDGES", (void (*)())NULL, 0 },
	{ "move edge 12", arb5_edge, 0 },
	{ "move edge 23", arb5_edge, 1 },
	{ "move edge 34", arb5_edge, 2 },
	{ "move edge 14", arb5_edge, 3 },
	{ "move edge 15", arb5_edge, 4 },
	{ "move edge 25", arb5_edge, 5 },
	{ "move edge 35", arb5_edge, 6 },
	{ "move edge 45", arb5_edge, 7 },
	{ "move point 5", arb5_edge, 8 },
	{ "RETURN",       arb5_edge, 9 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  point4_menu[] = {
	{ "ARB4 POINTS", (void (*)())NULL, 0 },
	{ "move point 1", arb4_point, 0 },
	{ "move point 2", arb4_point, 1 },
	{ "move point 3", arb4_point, 2 },
	{ "move point 4", arb4_point, 4 },
	{ "RETURN",       arb4_point, 5 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  tgc_menu[] = {
	{ "TGC MENU", (void (*)())NULL, 0 },
	{ "scale H",	tgc_ed, MENU_TGC_SCALE_H },
	{ "scale A",	tgc_ed, MENU_TGC_SCALE_A },
	{ "scale B",	tgc_ed, MENU_TGC_SCALE_B },
	{ "scale c",	tgc_ed, MENU_TGC_SCALE_C },
	{ "scale d",	tgc_ed, MENU_TGC_SCALE_D },
	{ "scale A,B",	tgc_ed, MENU_TGC_SCALE_AB },
	{ "scale C,D",	tgc_ed, MENU_TGC_SCALE_CD },
	{ "scale A,B,C,D", tgc_ed, MENU_TGC_SCALE_ABCD },
	{ "rotate H",	tgc_ed, MENU_TGC_ROT_H },
	{ "rotate AxB",	tgc_ed, MENU_TGC_ROT_AB },
	{ "move end H(rt)", tgc_ed, MENU_TGC_MV_H },
	{ "move end H", tgc_ed, MENU_TGC_MV_HH },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  tor_menu[] = {
	{ "TORUS MENU", (void (*)())NULL, 0 },
	{ "scale radius 1", tor_ed, MENU_TOR_R1 },
	{ "scale radius 2", tor_ed, MENU_TOR_R2 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  eto_menu[] = {
	{ "ELL-TORUS MENU", (void (*)())NULL, 0 },
	{ "scale r1 (r)", eto_ed, MENU_ETO_R1 },
	{ "scale r2 (rd)", eto_ed, MENU_ETO_R2 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  ell_menu[] = {
	{ "ELLIPSOID MENU", (void (*)())NULL, 0 },
	{ "scale A", ell_ed, MENU_ELL_SCALE_A },
	{ "scale B", ell_ed, MENU_ELL_SCALE_B },
	{ "scale C", ell_ed, MENU_ELL_SCALE_C },
	{ "scale A,B,C", ell_ed, MENU_ELL_SCALE_ABC },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  ars_menu[] = {
	{ "ARS MENU", (void (*)())NULL, 0 },
	{ "not implemented", ars_ed, 1 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  spline_menu[] = {
	{ "SPLINE MENU", (void (*)())NULL, 0 },
	{ "not implemented", spline_ed, 1 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv8_menu[] = {
	{ "ARB8 FACES", (void (*)())NULL, 0 },
	{ "move face 1234", arb8_mv_face, 1 },
	{ "move face 5678", arb8_mv_face, 2 },
	{ "move face 1584", arb8_mv_face, 3 },
	{ "move face 2376", arb8_mv_face, 4 },
	{ "move face 1265", arb8_mv_face, 5 },
	{ "move face 4378", arb8_mv_face, 6 },
	{ "RETURN",         arb8_mv_face, 7 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv7_menu[] = {
	{ "ARB7 FACES", (void (*)())NULL, 0 },
	{ "move face 1234", arb7_mv_face, 1 },
	{ "move face 2376", arb7_mv_face, 4 },
	{ "RETURN",         arb7_mv_face, 7 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv6_menu[] = {
	{ "ARB6 FACES", (void (*)())NULL, 0 },
	{ "move face 1234", arb6_mv_face, 1 },
	{ "move face 2365", arb6_mv_face, 2 },
	{ "move face 1564", arb6_mv_face, 3 },
	{ "move face 125" , arb6_mv_face, 4 },
	{ "move face 346" , arb6_mv_face, 5 },
	{ "RETURN",         arb6_mv_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv5_menu[] = {
	{ "ARB5 FACES", (void (*)())NULL, 0 },
	{ "move face 1234", arb5_mv_face, 1 },
	{ "move face 125" , arb5_mv_face, 2 },
	{ "move face 235" , arb5_mv_face, 3 },
	{ "move face 345" , arb5_mv_face, 4 },
	{ "move face 145" , arb5_mv_face, 5 },
	{ "RETURN",         arb5_mv_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item mv4_menu[] = {
	{ "ARB4 FACES", (void (*)())NULL, 0 },
	{ "move face 123" , arb4_mv_face, 1 },
	{ "move face 124" , arb4_mv_face, 2 },
	{ "move face 234" , arb4_mv_face, 3 },
	{ "move face 134" , arb4_mv_face, 4 },
	{ "RETURN",         arb4_mv_face, 5 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot8_menu[] = {
	{ "ARB8 FACES", (void (*)())NULL, 0 },
	{ "rotate face 1234", arb8_rot_face, 1 },
	{ "rotate face 5678", arb8_rot_face, 2 },
	{ "rotate face 1584", arb8_rot_face, 3 },
	{ "rotate face 2376", arb8_rot_face, 4 },
	{ "rotate face 1265", arb8_rot_face, 5 },
	{ "rotate face 4378", arb8_rot_face, 6 },
	{ "RETURN",         arb8_rot_face, 7 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot7_menu[] = {
	{ "ARB7 FACES", (void (*)())NULL, 0 },
	{ "rotate face 1234", arb7_rot_face, 1 },
	{ "rotate face 567" , arb7_rot_face, 2 },
	{ "rotate face 145" , arb7_rot_face, 3 },
	{ "rotate face 2376", arb7_rot_face, 4 },
	{ "rotate face 1265", arb7_rot_face, 5 },
	{ "rotate face 4375", arb7_rot_face, 6 },
	{ "RETURN",         arb7_rot_face, 7 },
	{ "", (void (*)())NULL, 0 }
};



struct menu_item rot6_menu[] = {
	{ "ARB6 FACES", (void (*)())NULL, 0 },
	{ "rotate face 1234", arb6_rot_face, 1 },
	{ "rotate face 2365", arb6_rot_face, 2 },
	{ "rotate face 1564", arb6_rot_face, 3 },
	{ "rotate face 125" , arb6_rot_face, 4 },
	{ "rotate face 346" , arb6_rot_face, 5 },
	{ "RETURN",         arb6_rot_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot5_menu[] = {
	{ "ARB5 FACES", (void (*)())NULL, 0 },
	{ "rotate face 1234", arb5_rot_face, 1 },
	{ "rotate face 125" , arb5_rot_face, 2 },
	{ "rotate face 235" , arb5_rot_face, 3 },
	{ "rotate face 345" , arb5_rot_face, 4 },
	{ "rotate face 145" , arb5_rot_face, 5 },
	{ "RETURN",         arb5_rot_face, 6 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item rot4_menu[] = {
	{ "ARB4 FACES", (void (*)())NULL, 0 },
	{ "rotate face 123" , arb4_rot_face, 1 },
	{ "rotate face 124" , arb4_rot_face, 2 },
	{ "rotate face 234" , arb4_rot_face, 3 },
	{ "rotate face 134" , arb4_rot_face, 4 },
	{ "RETURN",         arb4_rot_face, 5 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item cntrl_menu[] = {
	{ "ARB MENU", (void (*)())NULL, 0 },
	{ "move edges", arb_control, MENU_ARB_MV_EDGE },
	{ "move faces", arb_control, MENU_ARB_MV_FACE },
	{ "rotate faces", arb_control, MENU_ARB_ROT_FACE },
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
arb8_edge( arg )
int arg;
{
	es_menu = arg;
	es_edflag = EARB;
	if(arg == 12)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb7_edge( arg )
int arg;
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
		sedraw = 1;
	}
}

static void
arb6_edge( arg )
int arg;
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
		sedraw = 1;
	}
}

static void
arb5_edge( arg )
int arg;
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
		sedraw = 1;
	}
}

static void
arb4_point( arg )
int arg;
{
	es_menu = arg;
	es_edflag = PTARB;
	if(arg == 5)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
tgc_ed( arg )
int arg;
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
}


static void
tor_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = PSCALE;
}

static void
eto_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = PSCALE;
}

static void
ell_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = PSCALE;
}

static void
arb8_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 7)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb7_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 7)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}		

static void
arb6_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 6)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb5_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 6)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb4_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_MOVE_FACE;
	if(arg == 5)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb8_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_SETUP_ROTFACE;
	sedraw = 1;
	if(arg == 7)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb7_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_SETUP_ROTFACE;
	sedraw = 1;
	if(arg == 7)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}		

static void
arb6_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_SETUP_ROTFACE;
	sedraw = 1;
	if(arg == 6)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb5_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_SETUP_ROTFACE;
	sedraw = 1;
	if(arg == 6)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb4_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = ECMD_ARB_SETUP_ROTFACE;
	sedraw = 1;
	if(arg == 5)  {
		es_edflag = ECMD_ARB_MAIN_MENU;
		sedraw = 1;
	}
}

static void
arb_control( arg )
int arg;
{
	es_menu = arg;
	es_edflag = ECMD_ARB_SPECIFIC_MENU;
	sedraw = 1;
}

/*ARGSUSED*/
static void
ars_ed( arg )
int arg;
{
	(void)printf("NOT IMPLEMENTED YET\n");
}

/*ARGSUSED*/
static void
spline_ed( arg )
int arg;
{
	(void)printf("NOT IMPLEMENTED YET\n");
}

/*
 *			I N I T _ S E D I T
 *
 *  First time in for this solid, set things up.
 *  If all goes well, change state to ST_S_EDIT.
 *  Solid editing is completed only via sedit_accept() / sedit_reject().
 */
void
init_sedit()
{
	register int i, type, p1, p2, p3;
	int			id;

	/*
	 * Check for a processed region or other illegal solid.
	 */
	if( illump->s_Eflag )  {
		(void)printf(
"Unable to Solid_Edit a processed region;  select a primitive instead\n");
		return;
	}

	/* Read solid description.  Save copy of original data */
	RT_INIT_EXTERNAL(&es_ext);
	RT_INIT_DB_INTERNAL(&es_int);
	if( db_get_external( &es_ext, illump->s_path[illump->s_last], dbip ) < 0 )
		READ_ERR_return;

	id = rt_id_solid( &es_ext );
	if( rt_functab[id].ft_import( &es_int, &es_ext, rt_identity ) < 0 )  {
		rt_log("init_sedit(%s):  solid import failure\n",
			illump->s_path[illump->s_last]->d_namep );
	    	if( es_int.idb_ptr )  rt_functab[id].ft_ifree( &es_int );
		db_free_external( &es_ext );
		return;				/* FAIL */
	}
	RT_CK_DB_INTERNAL( &es_int );

	es_menu = 0;
	new_way = 0;
	bcopy( (char *)es_ext.ext_buf, (char *)&es_rec, sizeof(es_rec) );

	if( es_rec.u_id == ID_SOLID )  {
		struct solidrec temprec;	/* copy of solid to determine type */

		temprec = es_rec.s;		/* struct copy */
		VMOVE( es_keypoint, es_rec.s.s_values );

		if( (type = es_rec.s.s_cgtype) < 0 )
			type *= -1;
		if(type == BOX || type == RPP)
			type = ARB8;
		if(type == RAW) {
			/* rearrange vectors to correspond to the
			 *  	"standard" ARB6
			 */
			register struct solidrec *trp = &temprec;
			VMOVE(&trp->s_values[3], &es_rec.s.s_values[9]);
			VMOVE(&trp->s_values[6], &es_rec.s.s_values[21]);
			VMOVE(&trp->s_values[9], &es_rec.s.s_values[12]);
			VMOVE(&trp->s_values[12], &es_rec.s.s_values[3]);
			VMOVE(&trp->s_values[15], &es_rec.s.s_values[6]);
			VMOVE(&trp->s_values[18], &es_rec.s.s_values[18]);
			VMOVE(&trp->s_values[21], &es_rec.s.s_values[15]);
			es_rec.s = *trp;	/* struct copy */
			type = ARB6;
		}
		es_rec.s.s_cgtype = type;

		if( es_rec.s.s_type == GENARB8 ) {
			/* find the comgeom arb type */
			if( (type = type_arb( &es_rec )) == 0 ) {
				(void)printf("%s: BAD ARB\n",es_rec.s.s_name);
				return;
			}

			temprec = es_rec.s;
			es_rec.s.s_cgtype = type;

			/* find the plane equations */
			calc_planes( &es_rec.s, type );
		}
	}
#if 1
	/* Experimental, but working. */
	switch( id )  {
	case ID_ELL:
	case ID_TGC:
	case ID_TOR:
	case ID_ETO:
		rt_log("Experimental:  new_way=1\n");
		new_way = 1;

	}
#endif

	/* Save aggregate path matrix */
	pathHmat( illump, es_mat, illump->s_last-1 );

	/* get the inverse matrix */
	mat_inv( es_invmat, es_mat );

	/* Establish initial keypoint */
	es_keytag = "";
	get_solid_keypoint( es_keypoint, &es_keytag, &es_int, es_mat );
printf("es_keypoint (%s) (%g, %g, %g)\n", es_keytag, V3ARGS(es_keypoint) );

	sedit_menu();		/* put up menu header */

	/* Finally, enter solid edit state */
	dmp->dmr_light( LIGHT_ON, BE_ACCEPT );
	dmp->dmr_light( LIGHT_ON, BE_REJECT );
	dmp->dmr_light( LIGHT_OFF, BE_S_ILLUMINATE );

	(void)chg_state( ST_S_PICK, ST_S_EDIT, "Keyboard illuminate");
	chg_l2menu(ST_S_EDIT);
	es_edflag = IDLE;
	sedraw = 1;

	button( BE_S_EDIT );	/* Drop into edit menu right away */
}

/*
 *			R E P L O T _ E D I T I N G _ S O L I D
 *
 *  All solid edit routines call this subroutine after
 *  making a change to es_rec or es_mat.
 */
void
replot_editing_solid()
{
	int			id;
	struct rt_external	ext;
	struct rt_db_internal	intern;
	struct rt_db_internal	*ip;
	struct directory	*dp;

	dp = illump->s_path[illump->s_last];

	if( new_way )  {
		ip = &es_int;
	} else {
		/* Fake up an external representation */
		RT_INIT_EXTERNAL( &ext );
		ext.ext_buf = (genptr_t)&es_rec;
		ext.ext_nbytes = sizeof(union record);

		if( (id = rt_id_solid( &ext )) == ID_NULL )  {
			(void)printf("replot_editing_solid() unable to identify type of solid %s\n",
				dp->d_namep );
			return;
		}

	    	RT_INIT_DB_INTERNAL(&intern);
		if( rt_functab[id].ft_import( &intern, &ext, rt_identity ) < 0 )  {
			rt_log("%s:  solid import failure\n",
				dp->d_namep );
		    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
		    	return;			/* ERROR */
		}
		ip = &intern;
	}
	RT_CK_DB_INTERNAL( ip );

	(void)replot_modified_solid( illump, ip, es_mat );

	if( !new_way )  {
	    	if( intern.idb_ptr )  rt_functab[id].ft_ifree( &intern );
	}
}

/*
 *			T R A N S F O R M _ E D I T I N G _ S O L I D
 *
 * XXX This should be part of the import/export interface.
 * XXX Each solid should know how to transform it's internal representation.
 */
void
transform_editing_solid(os, mat, is, free)
struct rt_db_internal	*os;
mat_t			mat;
struct rt_db_internal	*is;
int			free;
{
	struct rt_external	ext;
	struct directory	*dp;
	int			id;

	RT_CK_DB_INTERNAL( is );
	dp = illump->s_path[illump->s_last];
	id = is->idb_type;
	RT_INIT_EXTERNAL(&ext);
	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[id].ft_export( &ext, is, 1.0 ) < 0 )  {
		rt_log("transform_editing_solid(%s):  solid export failure\n", dp->d_namep);
		rt_bomb("transform_editing_solid");		/* FAIL */
	}
	if( (free || os == is) && is->idb_ptr )  {
		rt_functab[id].ft_ifree( is );
    		is->idb_ptr = (genptr_t)0;
    	}

	if( rt_functab[id].ft_import( os, &ext, mat ) < 0 )  {
		rt_log("transform_editing_solid(%s):  solid import failure\n",
			illump->s_path[illump->s_last]->d_namep );
		rt_bomb("transform_editing_solid");		/* FAIL */
	}
	RT_CK_DB_INTERNAL( os );

}

/* put up menu header */
void
sedit_menu()  {

	menuflag = 0;		/* No menu item selected yet */

	menu_array[MENU_L1] = MENU_NULL;
	chg_l2menu(ST_S_EDIT);
                                                                      
	switch( es_int.idb_type ) {

	case ID_ARB8:
		menu_array[MENU_L1] = cntrl_menu;
		break;
	case ID_TGC:
		menu_array[MENU_L1] = tgc_menu;
		break;
	case ID_TOR:
		menu_array[MENU_L1] = tor_menu;
		break;
	case ID_ELL:
		menu_array[MENU_L1] = ell_menu;
		break;
	case ID_ARS:
		menu_array[MENU_L1] = ars_menu;
		break;
	case ID_BSPLINE:
		menu_array[MENU_L1] = spline_menu;
		break;
	case ID_ETO:
		menu_array[MENU_L1] = eto_menu;
		break;
	}
	es_edflag = IDLE;	/* Drop out of previous edit mode */
	es_menu = 0;
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
 */
void
sedit()
{
	register dbfloat_t *op;
	fastf_t	*eqp;
	static vect_t work;
	register int i;
	static int pnt5;		/* ECMD_ARB_SETUP_ROTFACE, special arb7 case */
	static int j;
	static float la, lb, lc, ld;	/* TGC: length of vectors */

	sedraw = 0;

	switch( es_edflag ) {

	case IDLE:
		/* do nothing */
		break;

	case ECMD_ARB_MAIN_MENU:
		/* put up control (main) menu for GENARB8s */
		menuflag = 0;
		es_edflag = IDLE;
		menu_array[MENU_L1] = cntrl_menu;
		break;

	case ECMD_ARB_SPECIFIC_MENU:
		/* put up specific arb edit menus */
		menuflag = 0;
		es_edflag = IDLE;
		switch( es_menu ){
			case MENU_ARB_MV_EDGE:  
				menu_array[MENU_L1] = which_menu[es_type-4];
				break;
			case MENU_ARB_MV_FACE:
				menu_array[MENU_L1] = which_menu[es_type+1];
				break;
			case MENU_ARB_ROT_FACE:
				menu_array[MENU_L1] = which_menu[es_type+6];
				break;
			default:
				(void)printf("Bad menu item.\n");
				return;
		}
		break;

	case ECMD_ARB_MOVE_FACE:
		/* move face through definite point */
		if(inpara) {
			/* apply es_invmat to convert to real model space */
			MAT4X3PNT(work,es_invmat,es_para);
			/* change D of planar equation */
			es_peqn[es_menu][3]=VDOT(&es_peqn[es_menu][0], work);
			/* find new vertices, put in record in vector notation */
			calc_pnts( &es_rec.s, es_rec.s.s_cgtype );
		}
		break;

	case ECMD_ARB_SETUP_ROTFACE:
		/* check if point 5 is in the face */
		pnt5 = 0;
		for(i=0; i<4; i++)  {
			if( arb_vertices[es_rec.s.s_cgtype-4][es_menu*4+i]==5 )
				pnt5=1;
		}
		
		/* special case for arb7 */
		if( es_rec.s.s_cgtype == ARB7  && pnt5 ){
				(void)printf("\nFixed vertex is point 5.\n");
				fixv = 5;
		}
		else{
			/* find fixed vertex for ECMD_ARB_ROTATE_FACE */
			fixv=0;
			do  {
				int	type,loc,valid;
				char	line[128];
				
				type = es_rec.s.s_cgtype - 4;
				(void)printf("\nEnter fixed vertex number( ");
				loc = es_menu*4;
				for(i=0; i<4; i++){
					if( arb_vertices[type][loc+i] )
						printf("%d ",
						    arb_vertices[type][loc+i]);
				}
				printf(") [%d]: ",arb_vertices[type][loc]);

				(void)gets( line );		/* Null terminated */
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
			} while( fixv <= 0 || fixv > es_rec.s.s_cgtype );
		}
		
		pr_prompt();
		fixv--;
		es_edflag = ECMD_ARB_ROTATE_FACE;
		mat_idn( acc_rot_sol );
		dmaflag = 1;	/* draw arrow, etc */
		break;

	case ECMD_ARB_ROTATE_FACE:
		/* rotate a GENARB8 defining plane through a fixed vertex */
		if(inpara) {
			static mat_t invsolr;
			static vect_t tempvec;
			static float rota, fb;

			/*
			 * Keyboard parameters in degrees.
			 * First, cancel any existing rotations,
			 * then perform new rotation
			 */
			mat_inv( invsolr, acc_rot_sol );
			eqp = &es_peqn[es_menu][0];	/* es_menu==plane of interest */
			VMOVE( work, eqp );
			MAT4X3VEC( eqp, invsolr, work );

			if( numargs == (1+3) ){		/* absolute X,Y,Z rotations */
				/* Build completely new rotation change */
				mat_idn( modelchanges );
				buildHrot( modelchanges,
					es_para[0] * degtorad,
					es_para[1] * degtorad,
					es_para[2] * degtorad );
				mat_copy(acc_rot_sol, modelchanges);

				/* Apply new rotation to face */
				eqp = &es_peqn[es_menu][0];
				VMOVE( work, eqp );
				MAT4X3VEC( eqp, modelchanges, work );
			}
			else if( numargs== (1+2) ){	/* rot,fb given */
				rota= atof(cmd_args[1]) * degtorad;
				fb  = atof(cmd_args[2]) * degtorad;
	
				/* calculate normal vector (length=1) from rot,fb */
				es_peqn[es_menu][0] = cos(fb) * cos(rota);
				es_peqn[es_menu][1] = cos(fb) * sin(rota);
				es_peqn[es_menu][2] = sin(fb);
			}
			else{
				(void)printf("Must be < rot fb | xdeg ydeg zdeg >\n");
				return;
			}

			/* point notation of fixed vertex */
			if( fixv ){		/* special case for solid vertex */
				VADD2(tempvec, &es_rec.s.s_values[fixv*3], &es_rec.s.s_values[0] );
			}
			else{
				VMOVE( tempvec, &es_rec.s.s_values[fixv] );
			}

			/* set D of planar equation to anchor at fixed vertex */
			/* es_menu == plane of interest */
			es_peqn[es_menu][3]=VDOT(eqp,tempvec);	

			/*  Clear out solid rotation */
			mat_idn( modelchanges );

		}  else  {
			/* Apply incremental changes */
			static vect_t tempvec;

			eqp = &es_peqn[es_menu][0];
			VMOVE( work, eqp );
			MAT4X3VEC( eqp, incr_change, work );

			/* point notation of fixed vertex */
			if( fixv ){		/* special case for solid vertex */
				VADD2(tempvec, &es_rec.s.s_values[fixv*3], &es_rec.s.s_values[0] );
			}
			else{
				VMOVE( tempvec, &es_rec.s.s_values[fixv] );
			}

			/* set D of planar equation to anchor at fixed vertex */
			/* es_menu == plane of interest */
			es_peqn[es_menu][3]=VDOT(eqp,tempvec);	
		}

		calc_pnts( &es_rec.s, es_rec.s.s_cgtype );
		mat_idn( incr_change );

		/* no need to calc_planes again */
		replot_editing_solid();

		inpara = 0;
		return;

	case SSCALE:
		/* scale the solid uniformly about it's vertex point */
		if(inpara) {
			/* accumulate the scale factor */
			es_scale = es_para[0] / acc_sc_sol;
			acc_sc_sol = es_para[0];

		}
		if( new_way )  {
			mat_t	scalemat;
			mat_scale_about_pt( scalemat, es_keypoint, es_scale );
			transform_editing_solid(&es_int, scalemat, &es_int, 1);
		} else {
			for(i=3; i<=21; i+=3) { 
				op = &es_rec.s.s_values[i];
				VSCALE(op,op,es_scale);
			}
		}
		/* reset solid scale factor */
		es_scale = 1.0;
		break;

	case STRANS:
		/* translate solid  */
		if(inpara) {
			/* Keyboard parameter.
			 * Apply inverse of es_mat to these
			 * model coordinates first, because sedit_mouse()
			 * as already applied es_mat to them.
			 */
			MAT4X3PNT( work, es_invmat, es_para );
			if( new_way )  {
				vect_t	delta;
				mat_t	xlatemat;

				/* Need vector from current vertex/keypoint
				 * to desired new location.
				 */
				VSUB2( delta, work, es_keypoint );
				mat_idn( xlatemat );
				MAT_DELTAS_VEC_NEG( xlatemat, delta );
				transform_editing_solid(&es_int, xlatemat, &es_int, 1);
			} else {
				VMOVE(es_rec.s.s_values, work);
			}
		}
		break;

	case ECMD_TGC_MV_H:
		/*
		 * Move end of H of tgc, keeping plates perpendicular
		 * to H vector.
		 */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);
			if( inpara ) {
				/* apply es_invmat to convert to real model coordinates */
				MAT4X3PNT( work, es_invmat, es_para );
				VSUB2(tgc->h, work, tgc->v);
			}

			/* check for zero H vector */
			if( MAGNITUDE( tgc->h ) <= SQRT_SMALL_FASTF ) {
				(void)printf("Zero H vector not allowed, resetting to +Z\n");
				VSET(tgc->h, 0, 0, 1 );
				break;
			}

			/* have new height vector --  redefine rest of tgc */
			la = MAGNITUDE( tgc->a );
			lb = MAGNITUDE( tgc->b );
			lc = MAGNITUDE( tgc->c );
			ld = MAGNITUDE( tgc->d );

			/* find 2 perpendicular vectors normal to H for new A,B */
			mat_vec_perp( tgc->b, tgc->h );
			VCROSS(tgc->a, tgc->b, tgc->h);
			VUNITIZE(tgc->a);
			VUNITIZE(tgc->b);

			/* Create new C,D from unit length A,B, with previous len */
			VSCALE(tgc->c, tgc->a, lc);
			VSCALE(tgc->d, tgc->b, ld);

			/* Restore original vector lengths to A,B */
			VSCALE(tgc->a, tgc->a, la);
			VSCALE(tgc->b, tgc->b, lb);
		} else {
		if( inpara ) {
			/* apply es_invmat to convert to real model coordinates */
			MAT4X3PNT( work, es_invmat, es_para );
			VSUB2(&es_rec.s.s_tgc_H, work, &es_rec.s.s_tgc_V);
		}

		/* check for zero H vector */
		if( MAGNITUDE( &es_rec.s.s_tgc_H ) == 0.0 ) {
			(void)printf("Zero H vector not allowed, resetting to +Z\n");
			VSET( &es_rec.s.s_tgc_H, 0, 0, 1 );
			break;
		}

		/* have new height vector --  redefine rest of tgc */
		la = MAGNITUDE( &es_rec.s.s_tgc_A );
		lb = MAGNITUDE( &es_rec.s.s_tgc_B );
		lc = MAGNITUDE( &es_rec.s.s_tgc_C );
		ld = MAGNITUDE( &es_rec.s.s_tgc_D );

		/* find 2 perpendicular vectors normal to H for new A,B */
		j=0;
		for(i=0; i<3; i++) {
			work[i] = 0.0;
			if( fabs(es_rec.s.s_values[i+3]) < 
			    fabs(es_rec.s.s_values[j+3]) )
				j = i;
		}
		work[j] = 1.0;
		VCROSS(&es_rec.s.s_tgc_B, work, &es_rec.s.s_tgc_H);
		VCROSS(&es_rec.s.s_tgc_A, &es_rec.s.s_tgc_B, &es_rec.s.s_tgc_H);
		VUNITIZE(&es_rec.s.s_tgc_A);
		VUNITIZE(&es_rec.s.s_tgc_B);

		/* Create new C,D from unit length A,B, with previous len */
		VSCALE(&es_rec.s.s_tgc_C, &es_rec.s.s_tgc_A, lc);
		VSCALE(&es_rec.s.s_tgc_D, &es_rec.s.s_tgc_B, ld);

		/* Restore original vector lengths to A,B */
		VSCALE(&es_rec.s.s_tgc_A, &es_rec.s.s_tgc_A, la);
		VSCALE(&es_rec.s.s_tgc_B, &es_rec.s.s_tgc_B, lb);
		}
		break;

	case ECMD_TGC_MV_HH:
		/* Move end of H of tgc - leave ends alone */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);
			if( inpara ) {
				/* apply es_invmat to convert to real model coordinates */
				MAT4X3PNT( work, es_invmat, es_para );
				VSUB2(tgc->h, work, tgc->v);
			}

			/* check for zero H vector */
			if( MAGNITUDE( tgc->h ) <= SQRT_SMALL_FASTF ) {
				(void)printf("Zero H vector not allowed, resetting to +Z\n");
				VSET(tgc->h, 0, 0, 1 );
				break;
			}
		} else {
		if( inpara ) {
			/* apply es_invmat to convert to real model coordinates */
			MAT4X3PNT( work, es_invmat, es_para );
			VSUB2(&es_rec.s.s_tgc_H, work, &es_rec.s.s_tgc_V);
		}

		/* check for zero H vector */
		if( MAGNITUDE( &es_rec.s.s_tgc_H ) == 0.0 ) {
			(void)printf("Zero H vector not allowed, resetting to +Z\n");
			VSET( &es_rec.s.s_tgc_H, 0, 0, 1 );
			break;
		}
		}
		break;

	case PSCALE:
		pscale();
		break;

	case PTARB:	/* move an ARB point */
	case EARB:   /* edit an ARB edge */
		if( inpara ) { 
			/* apply es_invmat to convert to real model space */
			MAT4X3PNT( work, es_invmat, es_para );
			editarb( work );
		}
		break;

	case SROT:
		/* rot solid about vertex */
		if(inpara) {
			static mat_t invsolr;
			/*
			 * Keyboard parameters:  absolute x,y,z rotations,
			 * in degrees.  First, cancel any existing rotations,
			 * then perform new rotation
			 */
			mat_inv( invsolr, acc_rot_sol );

			/* Build completely new rotation change */
			mat_idn( modelchanges );
			buildHrot( modelchanges,
				es_para[0] * degtorad,
				es_para[1] * degtorad,
				es_para[2] * degtorad );
			/* Borrow incr_change matrix here */
			mat_mul( incr_change, modelchanges, invsolr );
			mat_copy(acc_rot_sol, modelchanges);

			/* Apply new rotation to solid */
			/*  Clear out solid rotation */
			mat_idn( modelchanges );
		}  else  {
			/* Apply incremental changes already in incr_change */
		}
		/* Apply changes to solid */
		if( new_way )  {
			mat_t	mat;
			/* xlate keypoint to origin, rotate, then put back. */
			mat_xform_about_pt( mat, incr_change, es_keypoint );
			transform_editing_solid(&es_int, mat, &es_int, 1);
		} else {
			for(i=1; i<8; i++) {
				op = &es_rec.s.s_values[i*3];
				VMOVE( work, op );
				MAT4X3VEC( op, incr_change, work );
			}
		}
		mat_idn( incr_change );
		break;

	case ECMD_TGC_ROT_H:
		/* rotate height vector */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);
			MAT4X3VEC(work, incr_change, tgc->h);
			VMOVE(tgc->h, work);
		} else {
			MAT4X3VEC(work, incr_change, &es_rec.s.s_tgc_H);
			VMOVE(&es_rec.s.s_tgc_H, work);
		}
		mat_idn( incr_change );
		break;

	case ECMD_TGC_ROT_AB:
		/* rotate surfaces AxB and CxD (tgc) */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			MAT4X3VEC(work, incr_change, tgc->a);
			VMOVE(tgc->a, work);
			MAT4X3VEC(work, incr_change, tgc->b);
			VMOVE(tgc->b, work);
			MAT4X3VEC(work, incr_change, tgc->c);
			VMOVE(tgc->c, work);
			MAT4X3VEC(work, incr_change, tgc->d);
			VMOVE(tgc->d, work);
		} else {
			for(i=2; i<6; i++) {
				op = &es_rec.s.s_values[i*3];
				MAT4X3VEC( work, incr_change, op );
				VMOVE( op, work );
			}
		}
		mat_idn( incr_change );
		break;

	default:
		(void)printf("sedit():  unknown edflag = %d.\n", es_edflag );
	}

	/* must re-calculate the face plane equations for arbs */
	if( es_rec.s.s_type == GENARB8 )
		calc_planes( &es_rec.s, es_rec.s.s_cgtype );

	/* If the keypoint changed location, find about it here */
	if( new_way )  {
		get_solid_keypoint( es_keypoint, &es_keytag, &es_int, es_mat );
	}

	replot_editing_solid();

	inpara = 0;
	return;
}

/*
 *			S E D I T _ M O U S E
 *
 *  Mouse (pen) press in graphics area while doing Solid Edit.
 */
void
sedit_mouse( mousevec )
CONST vect_t	mousevec;
{
	vect_t	pos_view;	 	/* Unrotated view space pos */
	vect_t	pos_model;		/* Rotated screen space pos */
	vect_t	tr_temp;		/* temp translation vector */
	vect_t	temp;

	if( es_edflag <= 0 )  return;
	switch( es_edflag )  {

	case SSCALE:
	case PSCALE:
		/* use mouse to get a scale factor */
		es_scale = 1.0 + 0.25 * ((fastf_t)
			(mousevec[Y] > 0 ? mousevec[Y] : -mousevec[Y]));
		if ( mousevec[Y] <= 0 )
			es_scale = 1.0 / es_scale;

		/* accumulate scale factor */
		acc_sc_sol *= es_scale;

		sedraw = 1;
		return;
	case STRANS:
		/* 
		 * Use mouse to change solid's location.
		 * Project solid's keypoint into view space,
		 * replace X,Y (but NOT Z) components, and
		 * project result back to model space.
		 * Then move keypoint there.
		 */
		if( new_way )  {
			point_t	pt;
			vect_t	delta;
			mat_t	xlatemat;

			MAT4X3PNT( temp, es_mat, es_keypoint );
			MAT4X3PNT( pos_view, model2view, temp );
			pos_view[X] = mousevec[X];
			pos_view[Y] = mousevec[Y];
			MAT4X3PNT( temp, view2model, pos_view );
			MAT4X3PNT( pt, es_invmat, temp );

			/* Need vector from current vertex/keypoint
			 * to desired new location.
			 */
			VSUB2( delta, es_keypoint, pt );
			mat_idn( xlatemat );
			MAT_DELTAS_VEC_NEG( xlatemat, delta );
			transform_editing_solid(&es_int, xlatemat, &es_int, 1);
		} else {
			/* XXX this makes bad assumptions about format of es_rec !! */
			MAT4X3PNT( temp, es_mat, es_rec.s.s_values );
			MAT4X3PNT( pos_view, model2view, temp );
			pos_view[X] = mousevec[X];
			pos_view[Y] = mousevec[Y];
			MAT4X3PNT( temp, view2model, pos_view );
			MAT4X3PNT( es_rec.s.s_values, es_invmat, temp );
		}
		sedraw = 1;
		return;
	case ECMD_TGC_MV_H:
	case ECMD_TGC_MV_HH:
		/* Use mouse to change location of point V+H */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			VADD2( temp, tgc->v, tgc->h );
			MAT4X3PNT(pos_model, es_mat, temp);
			MAT4X3PNT( pos_view, model2view, pos_model );
			pos_view[X] = mousevec[X];
			pos_view[Y] = mousevec[Y];
			/* Do NOT change pos_view[Z] ! */
			MAT4X3PNT( temp, view2model, pos_view );
			MAT4X3PNT( tr_temp, es_invmat, temp );
			VSUB2( tgc->h, tr_temp, tgc->v );
		} else {
			VADD2( temp, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H );
			MAT4X3PNT(pos_model, es_mat, temp);
			MAT4X3PNT( pos_view, model2view, pos_model );
			pos_view[X] = mousevec[X];
			pos_view[Y] = mousevec[Y];
			/* Do NOT change pos_view[Z] ! */
			MAT4X3PNT( temp, view2model, pos_view );
			MAT4X3PNT( tr_temp, es_invmat, temp );
			VSUB2( &es_rec.s.s_tgc_H, tr_temp, &es_rec.s.s_tgc_V );
		}
		sedraw = 1;
		return;
	case PTARB:
		/* move an arb point to indicated point */
		/* point is located at es_values[es_menu*3] */
		VADD2(temp, es_rec.s.s_values, &es_rec.s.s_values[es_menu*3]);
		MAT4X3PNT(pos_model, es_mat, temp);
		MAT4X3PNT(pos_view, model2view, pos_model);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];
		MAT4X3PNT(temp, view2model, pos_view);
		MAT4X3PNT(pos_model, es_invmat, temp);
		editarb( pos_model );
		sedraw = 1;
		return;
	case EARB:
		/* move arb edge, through indicated point */
		MAT4X3PNT( temp, view2model, mousevec );
		/* apply inverse of es_mat */
		MAT4X3PNT( pos_model, es_invmat, temp );
		editarb( pos_model );
		sedraw = 1;
		return;
	case ECMD_ARB_MOVE_FACE:
		/* move arb face, through  indicated  point */
		MAT4X3PNT( temp, view2model, mousevec );
		/* apply inverse of es_mat */
		MAT4X3PNT( pos_model, es_invmat, temp );
		/* change D of planar equation */
		es_peqn[es_menu][3]=VDOT(&es_peqn[es_menu][0], pos_model);
		/* calculate new vertices, put in record as vectors */
		calc_pnts( &es_rec.s, es_rec.s.s_cgtype );
		sedraw = 1;
		return;
		
	default:
		(void)printf("mouse press undefined in this solid edit mode\n");
		break;
	}
}

/*
 *  Object Edit
 */
void
objedit_mouse( mousevec )
CONST vect_t	mousevec;
{
	fastf_t			scale;
	vect_t	pos_view;	 	/* Unrotated view space pos */
	vect_t	pos_model;	/* Rotated screen space pos */
	vect_t	tr_temp;		/* temp translation vector */
	vect_t	temp;

	mat_idn( incr_change );
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
			break;

			case BE_O_XSCALE:
				/* local scaling ... X-axis */
				incr_change[0] = scale;
				/* accumulate the scale factor */
				acc_sc[0] *= scale;
			break;

			case BE_O_YSCALE:
				/* local scaling ... Y-axis */
				incr_change[5] = scale;
				/* accumulate the scale factor */
				acc_sc[1] *= scale;
			break;

			case BE_O_ZSCALE:
				/* local scaling ... Z-axis */
				incr_change[10] = scale;
				/* accumulate the scale factor */
				acc_sc[2] *= scale;
			break;
		}

		/* Have scaling take place with respect to keypoint,
		 * NOT the view center.
		 */
		MAT4X3PNT(temp, es_mat, es_keypoint);
		MAT4X3PNT(pos_model, modelchanges, temp);
		wrt_point(modelchanges, incr_change, modelchanges, pos_model);
	}  else if( movedir & (RARROW|UARROW) )  {
		mat_t oldchanges;	/* temporary matrix */

		/* Vector from object keypoint to cursor */
		MAT4X3PNT( temp, es_mat, es_keypoint );
		MAT4X3PNT( pos_view, model2objview, temp );
		if( movedir & RARROW )
			pos_view[X] = mousevec[X];
		if( movedir & UARROW )
			pos_view[Y] = mousevec[Y];

		MAT4X3PNT( pos_model, view2model, pos_view );/* NOT objview */
		MAT4X3PNT( tr_temp, modelchanges, temp );
		VSUB2( tr_temp, pos_model, tr_temp );
		MAT_DELTAS(incr_change,
			tr_temp[X], tr_temp[Y], tr_temp[Z]);
		mat_copy( oldchanges, modelchanges );
		mat_mul( modelchanges, incr_change, oldchanges );
	}  else  {
		(void)printf("No object edit mode selected;  mouse press ignored\n");
		return;
	}
	mat_idn( incr_change );
	new_mats();
}

#define EPSILON 1.0e-7

/*
 *			V L S _ S O L I D
 */
void
vls_solid( vp, ip, mat )
register struct rt_vls		*vp;
CONST struct rt_db_internal	*ip;
CONST mat_t			mat;
{
	struct rt_external	ext;
	struct rt_db_internal	intern;
	struct solidrec		sol;
	mat_t			ident;
	int			id;

	RT_VLS_CHECK(vp);
	RT_CK_DB_INTERNAL(ip);

	id = ip->idb_type;
	transform_editing_solid( &intern, mat, ip, 0 );

	if( rt_functab[id].ft_describe( vp, &intern, 1 /*verbose*/,
	    base2local ) < 0 )
		printf("vls_solid: describe error\n");
	rt_functab[id].ft_ifree( &intern );
}

/*
 *  			P S C A L E
 *  
 *  Partial scaling of a solid.
 */
void
pscale()
{
	register dbfloat_t *op;
	static fastf_t ma,mb;
	static fastf_t mr1,mr2;

	switch( es_menu ) {

	case MENU_TGC_SCALE_H:	/* scale height vector */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->h);
			}
			VSCALE(tgc->h, tgc->h, es_scale);
		} else {
			op = &es_rec.s.s_tgc_H;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_TOR_R1:
		/* scale radius 1 of TOR */
		if( new_way )  {
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
		} else {
			mr2 = MAGNITUDE(&es_rec.s.s_tor_H);
			op = &es_rec.s.s_tor_B;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);

			op = &es_rec.s.s_tor_A;
			VSCALE(op, op, es_scale);
			mr1 = MAGNITUDE(op);
			if( mr1 < mr2 ) {
				VSCALE(op, op, (mr2+0.01)/mr1);
				op = &es_rec.s.s_tor_B;
				VSCALE(op, op, (mr2+0.01)/mr1);
				mr1 = MAGNITUDE(op);
			}
torcom:
			ma = mr1 - mr2;
			op = &es_rec.s.s_tor_C;
			mb = MAGNITUDE(op);
			VSCALE(op, op, ma/mb);

			op = &es_rec.s.s_tor_D;
			mb = MAGNITUDE(op);
			VSCALE(op, op, ma/mb);

			ma = mr1 + mr2;
			op = &es_rec.s.s_tor_E;
			mb = MAGNITUDE(op);
			VSCALE(op, op, ma/mb);

			op = &es_rec.s.s_tor_F;
			mb = MAGNITUDE(op);
			VSCALE(op, op, ma/mb);
		}
		break;

	case MENU_TOR_R2:
		/* scale radius 2 of TOR */
		if( new_way )  {
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
		} else {
			op = &es_rec.s.s_values[3];
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			mr2 = MAGNITUDE(op);
			mr1 = MAGNITUDE(&es_rec.s.s_values[6]);
			if(mr1 < mr2) {
				VSCALE(op, op, (mr1-0.01)/mr2);
				mr2 = MAGNITUDE(op);
			}
			goto torcom;
		}
		break;

	case MENU_ETO_R1:
		/* scale radius 1 (r) of ETO */
		/* new_way only */
		{
			struct rt_eto_internal	*eto = 
				(struct rt_eto_internal *)es_int.idb_ptr;
			fastf_t	newrad;
			RT_ETO_CK_MAGIC(eto);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				newrad = es_para[0];
			} else {
				newrad = eto->eto_r * es_scale;
			}
			if( newrad < SMALL )  newrad = 4*SMALL;
			if( eto->eto_rd <= newrad )
				eto->eto_r = newrad;
		}
		break;

	case MENU_ETO_R2:
		/* scale radius 2 (rd) of ETO */
		/* new_way only */
		{
			struct rt_eto_internal	*eto = 
				(struct rt_eto_internal *)es_int.idb_ptr;
			fastf_t	newrad;
			RT_ETO_CK_MAGIC(eto);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				newrad = es_para[0];
			} else {
				newrad = eto->eto_rd * es_scale;
			}
			if( newrad < SMALL )  newrad = 4*SMALL;
			if( newrad <= eto->eto_r )
				eto->eto_rd = newrad;
		}
		break;

	case MENU_TGC_SCALE_A:
		/* scale vector A */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->a);
			}
			VSCALE(tgc->a, tgc->a, es_scale);
		} else {
			op = &es_rec.s.s_tgc_A;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_TGC_SCALE_B:
		/* scale vector B */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->b);
			}
			VSCALE(tgc->b, tgc->b, es_scale);
		} else {
			op = &es_rec.s.s_tgc_B;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_ELL_SCALE_A:
		/* scale vector A */
		if( new_way )  {
			struct rt_ell_internal	*ell = 
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(ell->a);
			}
			VSCALE( ell->a, ell->a, es_scale );
		} else {
			op = &es_rec.s.s_ell_A;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_ELL_SCALE_B:
		/* scale vector B */
		if( new_way )  {
			struct rt_ell_internal	*ell = 
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(ell->b);
			}
			VSCALE( ell->b, ell->b, es_scale );
		} else {
			op = &es_rec.s.s_ell_B;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_ELL_SCALE_C:
		/* scale vector C */
		if( new_way )  {
			struct rt_ell_internal	*ell = 
				(struct rt_ell_internal *)es_int.idb_ptr;
			RT_ELL_CK_MAGIC(ell);
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_scale = es_para[0] * es_mat[15] /
					MAGNITUDE(ell->c);
			}
			VSCALE( ell->c, ell->c, es_scale );
		} else {
			op = &es_rec.s.s_ell_C;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_TGC_SCALE_C:
		/* TGC: scale ratio "c" */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->c);
			}
			VSCALE(tgc->c, tgc->c, es_scale);
		} else {
			op = &es_rec.s.s_tgc_C;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_TGC_SCALE_D:   /* scale  d for tgc */
		if( new_way )  {
			struct rt_tgc_internal	*tgc = 
				(struct rt_tgc_internal *)es_int.idb_ptr;
			RT_TGC_CK_MAGIC(tgc);

			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(tgc->d);
			}
			VSCALE(tgc->d, tgc->d, es_scale);
		} else {
			op = &es_rec.s.s_tgc_D;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
		}
		break;

	case MENU_TGC_SCALE_AB:
		if( new_way )  {
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
		} else {
			op = &es_rec.s.s_tgc_A;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			ma = MAGNITUDE( op );
			op = &es_rec.s.s_tgc_B;
			mb = MAGNITUDE( op );
			VSCALE(op, op, ma/mb);
		}
		break;

	case MENU_TGC_SCALE_CD:	/* scale C and D of tgc */
		if( new_way )  {
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
		} else {
			op = &es_rec.s.s_tgc_C;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			ma = MAGNITUDE( op );
			op = &es_rec.s.s_tgc_D;
			mb = MAGNITUDE( op );
			VSCALE(op, op, ma/mb);
		}
		break;

	case MENU_TGC_SCALE_ABCD: 		/* scale A,B,C, and D of tgc */
		if( new_way )  {
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
		} else {
			op = &es_rec.s.s_tgc_A;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			ma = MAGNITUDE( op );
			op = &es_rec.s.s_tgc_B;
			mb = MAGNITUDE( op );
			VSCALE(op, op, ma/mb);
			op = &es_rec.s.s_tgc_C;
			mb = MAGNITUDE( op );
			VSCALE(op, op, ma/mb);
			op = &es_rec.s.s_tgc_D;
			mb = MAGNITUDE( op );
			VSCALE(op, op, ma/mb);
		}
		break;

	case MENU_ELL_SCALE_ABC:	/* set A,B, and C length the same */
		if( new_way )  {
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
		} else {
			op = &es_rec.s.s_ell_A;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			ma = MAGNITUDE( op );
			op = &es_rec.s.s_ell_B;
			mb = MAGNITUDE( op );
			VSCALE(op, op, ma/mb);
			op = &es_rec.s.s_ell_C;
			mb = MAGNITUDE( op );
			VSCALE(op, op, ma/mb);
		}
		break;

	}
}

/*
 *			I N I T _ O B J E D I T
 *
 */
void
init_objedit()
{
	register int		i;
	register int		type;
	int			id;

	/* for safety sake */
	es_menu = 0;
	es_edflag = -1;
	mat_idn(es_mat);

	/*
	 * Check for a processed region 
	 */
	if( illump->s_Eflag )  {

		/* Have a processed (E'd) region - NO key solid.
		 * 	Use the 'center' as the key
		 */
		VMOVE(es_keypoint, illump->s_center);

		/* The s_center takes the es_mat into account already */
	}

	/* Not an evaluated region - just a regular path ending in a solid */
	if( db_get_external( &es_ext, illump->s_path[illump->s_last], dbip ) < 0 )  {
		(void)printf("init_objedit(%s): db_get_external failure\n",
			illump->s_path[illump->s_last]->d_namep );
		button(BE_REJECT);
		return;
	}

	id = rt_id_solid( &es_ext );
	if( rt_functab[id].ft_import( &es_int, &es_ext, rt_identity ) < 0 )  {
		rt_log("init_sedit(%s):  solid import failure\n",
			illump->s_path[illump->s_last]->d_namep );
	    	if( es_int.idb_ptr )  rt_functab[id].ft_ifree( &es_int );
		db_free_external( &es_ext );
		return;				/* FAIL */
	}
	RT_CK_DB_INTERNAL( &es_int );

	/* XXX hack:  get first granule into es_rec (ugh) */
	bcopy( (char *)es_ext.ext_buf, (char *)&es_rec, sizeof(es_rec) );

	/* Find the keypoint for editing */
	id = rt_id_solid( &es_ext );
	switch( id )  {
	case ID_NULL:
		(void)printf("init_objedit(%s): bad database record\n",
			illump->s_path[illump->s_last]->d_namep );
		button(BE_REJECT);
		db_free_external( &es_ext );
		return;

	case ID_ARS_A:
		{
			register union record *rec =
				(union record *)es_ext.ext_buf;

			/* XXX should import the ARS! */

			/* only interested in vertex */
			VMOVE(es_keypoint, rec[1].b.b_values);
			es_rec.s.s_type = ARS;		/* XXX wrong */
			es_rec.s.s_cgtype = ARS;
		}
		break;

	case ID_TOR:
	case ID_TGC:
	case ID_ELL:
	case ID_ARB8:
	case ID_HALF:
	case ID_ETO:
		/* All folks with u_id == (DB_)ID_SOLID */
		if( es_rec.s.s_cgtype < 0 )
			es_rec.s.s_cgtype *= -1;

		if( es_rec.s.s_type == GENARB8 ) {
			/* find the comgeom arb type */
			if( (type = type_arb( &es_rec )) == 0 ) {
				(void)printf("%s: BAD ARB\n",es_rec.s.s_name);
				return;
			}
			es_rec.s.s_cgtype = type;
		}
		VMOVE( es_keypoint, es_rec.s.s_values );
		break;

	case ID_EBM:
		/* Use model origin as key point */
		VSETALL(es_keypoint, 0 );
		break;

	default:
		VMOVE(es_keypoint, illump->s_center);
		printf("init_objedit() using %g,%g,%g as keypoint\n",
			V3ARGS(es_keypoint) );
	}

	/* Save aggregate path matrix */
	pathHmat( illump, es_mat, illump->s_last-1 );

	/* get the inverse matrix */
	mat_inv( es_invmat, es_mat );

	/* XXX Zap out es_rec, nobody should look there any further */
	bzero( (char *)&es_rec, sizeof(es_rec) );
}

void
oedit_accept()
{
	register struct solid *sp;
	/* matrices used to accept editing done from a depth
	 *	>= 2 from the top of the illuminated path
	 */
	mat_t topm;	/* accum matrix from pathpos 0 to i-2 */
	mat_t inv_topm;	/* inverse */
	mat_t deltam;	/* final "changes":  deltam = (inv_topm)(modelchanges)(topm) */
	mat_t tempm;

	switch( ipathpos )  {
	case 0:
		moveHobj( illump->s_path[ipathpos], modelchanges );
		break;
	case 1:
		moveHinstance(
			illump->s_path[ipathpos-1],
			illump->s_path[ipathpos],
			modelchanges
		);
		break;
	default:
		mat_idn( topm );
		mat_idn( inv_topm );
		mat_idn( deltam );
		mat_idn( tempm );

		pathHmat( illump, topm, ipathpos-2 );

		mat_inv( inv_topm, topm );

		mat_mul( tempm, modelchanges, topm );
		mat_mul( deltam, inv_topm, tempm );

		moveHinstance(
			illump->s_path[ipathpos-1],
			illump->s_path[ipathpos],
			deltam
		);
		break;
	}

	/*
	 *  Redraw all solids affected by this edit.
	 *  Regenerate a new control list which does not
	 *  include the solids about to be replaced,
	 *  so we can safely fiddle the displaylist.
	 */
	modelchanges[15] = 1000000000;	/* => small ratio */
	dmaflag=1;
	refresh();

	/* Now, recompute new chunks of displaylist */
	FOR_ALL_SOLIDS( sp )  {
		if( sp->s_iflag == DOWN )
			continue;
		(void)replot_original_solid( sp );
		sp->s_iflag = DOWN;
	}
	mat_idn( modelchanges );

    	if( es_int.idb_ptr )  rt_functab[es_int.idb_type].ft_ifree( &es_int );
	es_int.idb_ptr = (genptr_t)NULL;
	db_free_external( &es_ext );
}

void
oedit_reject()
{
    	if( es_int.idb_ptr )  rt_functab[es_int.idb_type].ft_ifree( &es_int );
	es_int.idb_ptr = (genptr_t)NULL;
	db_free_external( &es_ext );
}


/* 	CALC_PLANES()
 *		calculate the plane (face) equations for an arb
 *		in solidrec pointed at by sp
 */
void
calc_planes( sp, type )
struct solidrec *sp;
int type;
{
	struct solidrec temprec;
	register int i, p1, p2, p3;

	/* find the plane equations */
	/* point notation - use temprec record */
	VMOVE( &temprec.s_values[0], &sp->s_values[0] );
	for(i=3; i<=21; i+=3) {
		VADD2( &temprec.s_values[i], &sp->s_values[i], &sp->s_values[0] );
	}
	type -= 4;	/* ARB4 at location 0, ARB5 at 1, etc */
	for(i=0; i<6; i++) {
		if(arb_faces[type][i*4] == -1)
			break;	/* faces are done */
		p1 = arb_faces[type][i*4];
		p2 = arb_faces[type][i*4+1];
		p3 = arb_faces[type][i*4+2];
		if(planeqn(i, p1, p2, p3, &temprec)) {
			(void)printf("No eqn for face %d%d%d%d\n",
				p1+1,p2+1,p3+1,arb_faces[type][i*4+3]+1);
			return;
		}
	}
}

/* 			F _ E Q N ( )
 * Gets the A,B,C of a  planar equation from the command line and puts the
 * result into the array es_peqn[] at the position pointed to by the variable
 * 'es_menu' which is the plane being redefined. This function is only callable
 * when in solid edit and rotating the face of a GENARB8.
 */
void
f_eqn()
{
	short int i;
	vect_t tempvec;

	if( state != ST_S_EDIT ){
		(void)printf("Eqn: must be in solid edit\n");
		return;
	}
	else if( es_rec.s.s_type != GENARB8 ){
		(void)printf("Eqn: type must be GENARB8\n");
		return;
	}
	else if( es_edflag != ECMD_ARB_ROTATE_FACE ){
		(void)printf("Eqn: must be rotating a face\n");
		return;
	}

	/* get the A,B,C from the command line */
	for(i=0; i<3; i++)
		es_peqn[es_menu][i]= atof(cmd_args[i+1]);
	VUNITIZE( &es_peqn[es_menu][0] );

	/* set D of planar equation to anchor at fixed vertex */
	if( fixv ){				/* not the solid vertex */
		VADD2( tempvec, &es_rec.s.s_values[fixv*3], &es_rec.s.s_values[0] );
	}
	else{
		VMOVE( tempvec, &es_rec.s.s_values[0] );
	}
	es_peqn[es_menu][3]=VDOT( &es_peqn[es_menu][0], tempvec );
	
	calc_pnts( &es_rec.s, es_rec.s.s_cgtype );

	/* draw the new version of the solid */
	replot_editing_solid();

	/* update display information */
	dmaflag = 1;
}

/* Hooks from buttons.c */

void
sedit_accept()
{
	struct directory	*dp;
	int	id;

	if( not_state( ST_S_EDIT, "Solid edit accept" ) )  return;

	/* write editing changes out to disc */
	dp = illump->s_path[illump->s_last];
	if( !new_way )  {
		db_put( dbip, dp, &es_rec, 0, 1 );
	} else {
		/* Scale change on export is 1.0 -- no change */
		if( rt_functab[es_int.idb_type].ft_export( &es_ext, &es_int, 1.0 ) < 0 )  {
			rt_log("sedit_accept(%s):  solid export failure\n", dp->d_namep);
		    	if( es_int.idb_ptr )  rt_functab[es_int.idb_type].ft_ifree( &es_int );
			db_free_external( &es_ext );
			return;				/* FAIL */
		}
	    	if( es_int.idb_ptr )  rt_functab[es_int.idb_type].ft_ifree( &es_int );

		if( db_put_external( &es_ext, dp, dbip ) < 0 )  {
			db_free_external( &es_ext );
			ERROR_RECOVERY_SUGGESTION;
			WRITE_ERR_return;
		}
	}

	es_edflag = -1;
	menuflag = 0;
	movedir = 0;
	new_way = 0;

    	if( es_int.idb_ptr )  rt_functab[es_int.idb_type].ft_ifree( &es_int );
	es_int.idb_ptr = (genptr_t)NULL;
	db_free_external( &es_ext );
}

void
sedit_reject()
{
	if( not_state( ST_S_EDIT, "Solid edit reject" ) )  return;

	/* Restore the original solid */
	replot_original_solid( illump );

	menuflag = 0;
	movedir = 0;
	es_edflag = -1;
	new_way = 0;

    	if( es_int.idb_ptr )  rt_functab[es_int.idb_type].ft_ifree( &es_int );
	es_int.idb_ptr = (genptr_t)NULL;
	db_free_external( &es_ext );
}

/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
void
f_param( argc, argv )
int	argc;
char	**argv;
{
	register int i;

	if( es_edflag <= 0 )  {
		(void)printf("A solid editor option not selected\n");
		return;
	}
	if( es_edflag == ECMD_TGC_ROT_H || es_edflag == ECMD_TGC_ROT_AB ) {
		(void)printf("\"p\" command not defined for this option\n");
		return;
	}

	inpara = 1;
	sedraw++;
	for( i = 1; i < argc; i++ )  {
		es_para[ i - 1 ] = atof( argv[i] );
		if( es_edflag == PSCALE || es_edflag == SSCALE )  {
			if(es_para[0] <= 0.0) {
				(void)printf("ERROR: SCALE FACTOR <= 0\n");
				inpara = 0;
				sedraw = 0;
				return;
			}
		}
	}
	/* check if need to convert to the base unit */
	switch( es_edflag ) {

		case STRANS:
		case PSCALE:
		case EARB:
		case ECMD_ARB_MOVE_FACE:
		case ECMD_TGC_MV_H:
		case ECMD_TGC_MV_HH:
		case PTARB:
			/* must convert to base units */
			es_para[0] *= local2base;
			es_para[1] *= local2base;
			es_para[2] *= local2base;

		default:
			return;
	}
}

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

	if( es_edflag != ECMD_TGC_ROT_H &&
	    es_edflag != ECMD_TGC_ROT_AB &&
	    es_edflag != SROT &&
	    es_edflag != ECMD_ARB_ROTATE_FACE)
		return 0;

	mat_idn( incr_change );
	buildHrot( incr_change, xangle, yangle, zangle );

	/* accumulate the translations */
	mat_mul(tempp, incr_change, acc_rot_sol);
	mat_copy(acc_rot_sol, tempp);

	/* sedit() will use incr_change or acc_rot_sol ?? */
	sedit();	/* change es_rec only, NOW */

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

	mat_idn( incr_change );
	buildHrot( incr_change, xangle, yangle, zangle );

	/* accumulate change matrix - do it wrt a point NOT view center */
	mat_mul(tempp, modelchanges, es_mat);
	MAT4X3PNT(point, tempp, es_keypoint);
	wrt_point(modelchanges, incr_change, modelchanges, point);

	new_mats();

	return 1;
}

/*
 *			L A B E L _ E D I T E D _ S O L I D
 *
 *  Put labels on the vertices of the currently edited solid.
 *  XXX This really should use import/export interface!!!  Or be part of it.
 */
label_edited_solid( pl, max_pl, xform, ip )
struct rt_point_labels	pl[];
int			max_pl;
CONST mat_t		xform;
struct rt_db_internal	*ip;
{
	register int	i;
	union record	temp_rec;	/* copy of es_rec record */
	point_t		work;
	point_t		pos_view;
	int		npl = 0;

	RT_CK_DB_INTERNAL( ip );

	switch( ip->idb_type )  {

#define	POINT_LABEL( _pt, _char )	{ \
	VMOVE( pl[npl].pt, _pt ); \
	pl[npl].str[0] = _char; \
	pl[npl++].str[1] = '\0'; }

	case ID_ARB8:
		MAT4X3PNT( pos_view, xform, es_rec.s.s_values );
		POINT_LABEL( pos_view, '1' );
		temp_rec.s = es_rec.s;
		if(es_type == ARB4) {
			VMOVE(&temp_rec.s.s_values[9], &temp_rec.s.s_values[12]);
		}
		if(es_type == ARB6) {
			VMOVE(&temp_rec.s.s_values[15], &temp_rec.s.s_values[18]);
		}
		for(i=1; i<es_type; i++) {
			VADD2( work, es_rec.s.s_values, &temp_rec.s.s_values[i*3] );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, i + '1' );
		}
		break;
	case ID_TGC:
		if( new_way )  {
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
		} else {
		MAT4X3PNT( pos_view, xform, &es_rec.s.s_tgc_V );
		POINT_LABEL( pos_view, 'V' );

		VADD2( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_A );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'A' );

		VADD2( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_B );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'B' );

		VADD3( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H, &es_rec.s.s_tgc_C );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'C' );

		VADD3( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H, &es_rec.s.s_tgc_D );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'D' );
		}
		break;

	case ID_ELL:
		if( new_way )  {
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
		} else {
		MAT4X3PNT( pos_view, xform, &es_rec.s.s_ell_V );
		POINT_LABEL( pos_view, 'V' );

		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_A );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'A' );

		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_B );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'B' );

		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_C );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'C' );
		}
		break;

	case ID_TOR:
		if( new_way )  {
			struct rt_tor_internal	*tor = 
				(struct rt_tor_internal *)es_int.idb_ptr;
			fastf_t	r3, r4;
			vect_t	adir;
			RT_TOR_CK_MAGIC(tor);

			mat_vec_ortho( adir, tor->h );

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
		} else {
		MAT4X3PNT( pos_view, xform, &es_rec.s.s_tor_V );
		POINT_LABEL( pos_view, 'V' );

		VADD2( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_C );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'I' );

		VADD2( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_E );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'O' );

		VADD3( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_A, &es_rec.s.s_tor_H);
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'H' );
		}
		break;

	case ID_ETO:
		/* new_way only */
		{
			struct rt_eto_internal	*eto = 
				(struct rt_eto_internal *)es_int.idb_ptr;
			fastf_t	r3, r4;
			vect_t	adir;
			RT_ETO_CK_MAGIC(eto);

			mat_vec_ortho( adir, eto->eto_N );

			MAT4X3PNT( pos_view, xform, eto->eto_V );
			POINT_LABEL( pos_view, 'V' );

#if 0
			r3 = eto->r_a - eto->r_h;
			VJOIN1( work, eto->v, r3, adir );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'I' );

			r4 = eto->r_a + eto->r_h;
			VJOIN1( work, eto->v, r4, adir );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'O' );

			VJOIN1( work, eto->v, eto->r_a, adir );
			VADD2( work, work, eto->h );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, 'H' );
#endif
		}
		break;

	case ID_ARS:
		MAT4X3PNT(pos_view, xform, es_rec.s.s_values);
		POINT_LABEL( pos_view, 'V' );
		break;
	}

	pl[npl].str[0] = '\0';	/* Mark ending */
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
get_solid_keypoint( pt, strp, ip, mat )
point_t		pt;
char		**strp;
struct rt_db_internal	*ip;
mat_t		mat;
{
	char	*cp = *strp;
	point_t	mpt;

	RT_CK_DB_INTERNAL( ip );

	switch( ip->idb_type )  {
	case ID_ELL:
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
				VMOVE( mpt, ell->a );
				*strp = "A";
				break;
			}
			if( strcmp( cp, "B" ) == 0 )  {
				VMOVE( mpt, ell->b );
				*strp = "B";
				break;
			}
			if( strcmp( cp, "C" ) == 0 )  {
				VMOVE( mpt, ell->c );
				*strp = "C";
				break;
			}
			/* Default */
			VMOVE( mpt, ell->v );
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
	default:
		VSETALL( mpt, 0 );
		*strp = "(origin)";
		break;
	}
	MAT4X3PNT( pt, mat, mpt );
}
