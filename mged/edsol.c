/*
 *			E D S O L . C
 *
 * Functions -
 *	init_sedit	set up for a Solid Edit
 *	sedit		Apply Solid Edit transformation(s)
 *	findang		Given a normal vector, find rotation & fallback angles
 *	pr_solid	Print a description of a solid
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
#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"
#include "./menu.h"

extern char    *cmd_args[];
extern int 	numargs;

static void	arb8_edge(), ars_ed(), ell_ed(), tgc_ed(), tor_ed(), spline_ed();
static void	arb7_edge(), arb6_edge(), arb5_edge(), arb4_point();
static void	arb8_mv_face(), arb7_mv_face(), arb6_mv_face();
static void	arb5_mv_face(), arb4_mv_face(), arb8_rot_face(), arb7_rot_face();
static void 	arb6_rot_face(), arb5_rot_face(), arb4_rot_face(), arb_control();

void pscale();
void	calc_planes();
static short int fixv;		/* used in ROTFACE,f_eqn(): fixed vertex */

/* data for solid editing */
union record es_rec;		/* current solid record */
union record es_orig;		/* original solid record */
int     es_edflag;		/* type of editing for this solid */
fastf_t	es_scale;		/* scale factor */
fastf_t	es_para[3];		/* keyboard input parameter changes */
fastf_t	es_peqn[7][4];		/* ARBs defining plane equations */
fastf_t	es_m[3];		/* edge(line) slope */
int	es_menu;		/* item selected from menu */
mat_t	es_mat;			/* accumulated matrix of path */ 
mat_t 	es_invmat;		/* inverse of es_mat   KAA */
int	es_nlines;		/* # lines in printed display */
char	es_display[ES_LINELEN*10];/* buffer for lines of display */

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
	{ "scale H",	tgc_ed, MENUH },
	{ "scale A",	tgc_ed, MENUA },
	{ "scale B",	tgc_ed, MENUB },
	{ "scale c",	tgc_ed, MENUP1 },
	{ "scale d",	tgc_ed, MENUP2 },
	{ "scale A,B",	tgc_ed, MENUAB },
	{ "scale C,D",	tgc_ed, MENUCD },
	{ "scale A,B,C,D", tgc_ed, MENUABCD },
	{ "rotate H",	tgc_ed, MENURH },
	{ "rotate AxB",	tgc_ed, MENURAB },
	{ "move end H(rt)", tgc_ed, MENUMH },
	{ "move end H", tgc_ed, MENUMHH },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  tor_menu[] = {
	{ "TORUS MENU", (void (*)())NULL, 0 },
	{ "scale radius 1", tor_ed, MENUR1 },
	{ "scale radius 2", tor_ed, MENUR2 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  ell_menu[] = {
	{ "ELLIPSOID MENU", (void (*)())NULL, 0 },
	{ "scale A", ell_ed, MENUA },
	{ "scale B", ell_ed, MENUB },
	{ "scale C", ell_ed, MENUC },
	{ "scale A,B,C", ell_ed, MENUABC },
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
	{ "move edges", arb_control, EDGEMENU },
	{ "move faces", arb_control, MOVEMENU },
	{ "rotate faces", arb_control, ROTMENU },
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
	if(arg == 12)
		es_edflag = CONTROL;
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
	if(arg == 12)
		es_edflag = CONTROL;
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
	if(arg == 10)
		es_edflag = CONTROL;
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
	if(arg == 9)
		es_edflag = CONTROL;
}

static void
arb4_point( arg )
int arg;
{
	es_menu = arg;
	es_edflag = PTARB;
	if(arg == 5)
		es_edflag = CONTROL;
}

static void
tgc_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = PSCALE;
	if(arg == MENURH || arg == MENURAB)
		es_edflag = PROT;
	if(arg == MENUMH)
		es_edflag = MOVEH;
	if(arg == MENUMHH)
		es_edflag = MOVEHH;
}


static void
tor_ed( arg )
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
	es_edflag = MVFACE;
	if( arg == 7 )
		es_edflag = CONTROL;
}

static void
arb7_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = MVFACE;
	if( arg == 7 )
		es_edflag = CONTROL;
}		

static void
arb6_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = MVFACE;
	if( arg == 6 )
		es_edflag = CONTROL;
}

static void
arb5_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = MVFACE;
	if( arg == 6 )
		es_edflag = CONTROL;
}

static void
arb4_mv_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = MVFACE;
	if( arg == 5 )
		es_edflag = CONTROL;
}

static void
arb8_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = SETUP_ROTFACE;
	if( arg == 7 )
		es_edflag = CONTROL;
}

static void
arb7_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = SETUP_ROTFACE;
	if( arg == 7 )
		es_edflag = CONTROL;
}		

static void
arb6_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = SETUP_ROTFACE;
	if( arg == 6 )
		es_edflag = CONTROL;
}

static void
arb5_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = SETUP_ROTFACE;
	if( arg == 6 )
		es_edflag = CONTROL;
}

static void
arb4_rot_face( arg )
int arg;
{
	es_menu = arg - 1;
	es_edflag = SETUP_ROTFACE;
	if( arg == 5 )
		es_edflag = CONTROL;
}

static void
arb_control( arg )
int arg;
{
	es_menu = arg;
	es_edflag = CHGMENU;
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
 */
void
init_sedit()
{
	struct solidrec temprec;	/* copy of solid to determine type */
	register int i, type, p1, p2, p3;

	/*
	 * Check for a processed region or other illegal solid.
	 */
	if( illump->s_Eflag )  {
		(void)printf(
"Unable to Solid_Edit a processed region;  select a primitive instead\n");
		return;
	}

	/* Read solid description.  Save copy of original data */
	db_get( dbip,  illump->s_path[illump->s_last], &es_orig, 0 , 1);
	es_rec = es_orig;		/* struct copy */

	if( es_rec.u_id != ID_SOLID )  {
		(void)printf(
"Unable to Solid_Edit %c record;  select a primitive instead\n");
		return;
	}

	es_menu = 0;

	temprec = es_rec.s;		/* struct copy */

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


	/* Save aggregate path matrix */
	pathHmat( illump, es_mat, illump->s_last-1 );

	/* get the inverse matrix */
	mat_inv( es_invmat, es_mat );

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
	replot_modified_solid( illump, &es_rec, es_mat );
}

/* put up menu header */
void
sedit_menu()  {

	menuflag = 0;		/* No menu item selected yet */

	menu_array[MENU_L1] = MENU_NULL;
	chg_l2menu(ST_S_EDIT);
                                                                      
	switch( es_gentype ) {

	case GENARB8:
		menu_array[MENU_L1] = cntrl_menu;
		break;
	case GENTGC:
		menu_array[MENU_L1] = tgc_menu;
		break;
	case TOR:
		menu_array[MENU_L1] = tor_menu;
		break;
	case GENELL:
		menu_array[MENU_L1] = ell_menu;
		break;

	/* These two sections are wrong!!  Not solid records at all! */
	case ARS:
		menu_array[MENU_L1] = ars_menu;
		break;
	case SPLINE:
		menu_array[MENU_L1] = spline_menu;
		break;
	}
}

/*
 * 			S E D I T
 * 
 * A great deal of magic takes place here, to accomplish solid editing.
 */
void
sedit()
{
	register dbfloat_t *op;
	fastf_t	*eqp;
	static vect_t work;
	register int i;
	static int pnt5;		/* SETUP_ROTFACE, special arb7 case */
	static int j;
	static float la, lb, lc, ld;	/* TGC: length of vectors */

	sedraw = 0;

	switch( es_edflag ) {

	case IDLE:
		/* do nothing */
		break;

	case CONTROL:
		/* put up control menu for GENARB8s */
		menuflag = 0;
		es_edflag = IDLE;
		menu_array[MENU_L1] = cntrl_menu;
		break;

	case CHGMENU:
		/* put up specific arb edit menus */
		menuflag = 0;
		es_edflag = IDLE;
		switch( es_menu ){
			case EDGEMENU:  
				menu_array[MENU_L1] = which_menu[es_type-4];
				break;
			case MOVEMENU:
				menu_array[MENU_L1] = which_menu[es_type+1];
				break;
			case ROTMENU:
				menu_array[MENU_L1] = which_menu[es_type+6];
				break;
			default:
				(void)printf("Bad menu item.\n");
				return;
		}
		break;

	case MVFACE:
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

	case SETUP_ROTFACE:
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
			/* find fixed vertex for ROTFACE */
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
		es_edflag = ROTFACE;
		mat_idn( acc_rot_sol );
		dmaflag = 1;	/* draw arrow, etc */
		break;

	case ROTFACE:
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
		pr_solid( &es_rec.s );
		return;

	case SSCALE:
		/* scale the solid */
		if(inpara) {
			/* accumulate the scale factor */
			es_scale = es_para[0] / acc_sc_sol;
			acc_sc_sol = es_para[0];

		}
		for(i=3; i<=21; i+=3) { 
			op = &es_rec.s.s_values[i];
			VSCALE(op,op,es_scale);
		}
		/* reset solid scale factor */
		es_scale = 1.0;
		break;

	case STRANS:
		/* translate solid  */
		if(inpara) {
			/* Keyboard parameter.
			 * Apply inverse of es_mat to these
			 * model coordinates first.
			 */
			MAT4X3PNT( work, es_invmat, es_para );
			VMOVE(es_rec.s.s_values, work);
		}
		break;

	case MENU:
		/* do nothing */
		break;

	case MOVEH:
		/*
		 * Move end of H of tgc, keeping plates perpendicular
		 * to H vector.
		 */
		if( inpara ) {
			/* apply es_invmat to convert to real model coordinates */
			MAT4X3PNT( work, es_invmat, es_para );
			VSUB2(&es_rec.s.s_tgc_H, work, &es_rec.s.s_tgc_V);
		}

		/* check for zero H vector */
		if( MAGNITUDE( &es_rec.s.s_tgc_H ) == 0.0 ) {
			(void)printf("Zero H vector not allowed\n");
			/* Replace with original H */
			VMOVE(&es_rec.s.s_tgc_H, &es_orig.s.s_tgc_H);
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
		break;

	case MOVEHH:
		/* Move end of H of tgc - leave ends alone */
		if( inpara ) {
			/* apply es_invmat to convert to real model coordinates */
			MAT4X3PNT( work, es_invmat, es_para );
			VSUB2(&es_rec.s.s_tgc_H, work, &es_rec.s.s_tgc_V);
		}

		/* check for zero H vector */
		if( MAGNITUDE( &es_rec.s.s_tgc_H ) == 0.0 ) {
			(void)printf("Zero H vector not allowed\n");
			/* Replace with original H */
			VMOVE(&es_rec.s.s_tgc_H, &es_orig.s.s_tgc_H);
			break;
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
			for(i=1; i<8; i++) {
				op = &es_rec.s.s_values[i*3];
				VMOVE( work, op );
				MAT4X3VEC( op, invsolr, work );
			}

			/* Build completely new rotation change */
			mat_idn( modelchanges );
			buildHrot( modelchanges,
				es_para[0] * degtorad,
				es_para[1] * degtorad,
				es_para[2] * degtorad );
			mat_copy(acc_rot_sol, modelchanges);

			/* Apply new rotation to solid */
			for(i=1; i<8; i++) {
				op = &es_rec.s.s_values[i*3];
				VMOVE( work, op );
				MAT4X3VEC( op, modelchanges, work );
			}
			/*  Clear out solid rotation */
			mat_idn( modelchanges );

		}  else  {
			/* Apply incremental changes */
			for(i=1; i<8; i++) {
				op = &es_rec.s.s_values[i*3];
				VMOVE( work, op );
				MAT4X3VEC( op, incr_change, work );
			}
		}
		mat_idn( incr_change );
		break;

	case PROT:   /* partial rotation of a solid */
		switch( es_menu ) {

		case MENURH:  /* rotate height vector */
			MAT4X3VEC(work, incr_change, &es_rec.s.s_tgc_H);
			VMOVE(&es_rec.s.s_tgc_H, work);
			break;

		case MENURAB:  /* rotate surfaces AxB and CxD (tgc) */
			for(i=2; i<6; i++) {
				op = &es_rec.s.s_values[i*3];
				MAT4X3VEC( work, incr_change, op );
				VMOVE( op, work );
			}
			break;
		}
		mat_idn( incr_change );
		break;

	default:
		(void)printf("sedit():  unknown edflag = %d.\n", es_edflag );
	}

	/* must re-calculate the face plane equations for arbs */
	if( es_rec.s.s_type == GENARB8 )
		calc_planes( &es_rec.s, es_rec.s.s_cgtype );

	replot_editing_solid();

	inpara = 0;
	pr_solid( &es_rec.s );
	return;
}


/*
 *			F I N D A N G
 *
 * finds direction cosines and rotation, fallback angles of a unit vector
 * angles = pointer to 5 fastf_t's to store angles
 * unitv = pointer to the unit vector (previously computed)
 */
void
findang( angles, unitv )
register fastf_t	*angles;
register vect_t		unitv;
{
	FAST fastf_t f;

	/* convert direction cosines into axis angles */
	if( unitv[X] <= -1.0 )  angles[X] = -90.0;
	else if( unitv[X] >= 1.0 )  angles[X] = 90.0;
	else angles[X] = acos( unitv[X] ) * radtodeg;

	if( unitv[Y] <= -1.0 )  angles[Y] = -90.0;
	else if( unitv[Y] >= 1.0 )  angles[Y] = 90.0;
	else angles[Y] = acos( unitv[Y] ) * radtodeg;

	if( unitv[Z] <= -1.0 )  angles[Z] = -90.0;
	else if( unitv[Z] >= 1.0 )  angles[Z] = 90.0;
	else angles[Z] = acos( unitv[Z] ) * radtodeg;

	/* fallback angle */
	if( unitv[Z] <= -1.0 )  unitv[Z] = -1.0;
	else if( unitv[Z] >= 1.0 )  unitv[Z] = 1.0;
	angles[4] = asin(unitv[Z]);

	/* rotation angle */
	/* For the tolerance below, on an SGI 4D/70, cos(asin(1.0)) != 0.0
	 * with an epsilon of +/- 1.0e-17, so the tolerance below was
	 * substituted for the original +/- 1.0e-20.
	 */
	if((f = cos(angles[4])) > 1.0e-16 || f < -1.0e-16 )  {
		f = unitv[X]/f;
		if( f <= -1.0 )
			angles[3] = 180;
		else if( f >= 1.0 )
			angles[3] = 0;
		else
			angles[3] = radtodeg * acos( f );
	}  else
		angles[3] = 0.0;
	if( unitv[Y] < 0 )
		angles[3] = 360.0 - angles[3];

	angles[4] *= radtodeg;
}

#define EPSILON 1.0e-7

#define PR_STR(ln,str)	(void)strcpy(&es_display[ln*ES_LINELEN], str);

#define PR_PT(ln,title,base)	(void)sprintf( &es_display[ln*ES_LINELEN],\
		"\t%c (%.4f, %.4f, %.4f)%c", \
		title, (base)[X], (base)[Y], (base)[Z], '\0' )

#define PR_VECM(ln,title,base,mag)	(void)sprintf( &es_display[ln*ES_LINELEN],\
		"\t%c (%.4f, %.4f, %.4f) Mag=%f%c", \
		title, (base)[X], (base)[Y], (base)[Z], mag, '\0' )

#define PR_ANG(ln,str,base)	(void)sprintf( &es_display[ln*ES_LINELEN],\
		"\t%s dir cos=(%.1f, %.1f, %.1f), rot=%.1f, fb=%.1f%c", \
		str, (base)[0], (base)[1], (base)[2], \
		(base)[3], (base)[4], '\0' )

/*
 *			P R _ S O L I D
 *
 * Generate neatly formatted representation of the solid,
 * for display purposes.
 */
void
pr_solid( sp )
register struct solidrec *sp;
{
	static vect_t work;
	register int i;
	static fastf_t ma;
	static fastf_t r1, r2;
	static vect_t unitv;
	static fastf_t ang[5];
	static struct solidrec local;
	register int length;
	int cgtype;

	/* make a private copy in local units */
	for(i=0; i<24; i+=3) {
		VSCALE(work, &sp->s_values[i], base2local);
		VMOVE(&local.s_values[i], work);
	}
	if( (cgtype = sp->s_cgtype) < 0 )
		cgtype *= -1;

	switch( sp->s_type ) {

	case HALFSPACE:
		PR_PT( 0, 'N', &sp->s_values[0]);
		(void)sprintf( &es_display[1*ES_LINELEN],
			"\td=%f", local.s_values[3]);
		es_nlines = 2;
		break;

	case ARS:
	case ARSCONT:
		PR_PT( 0, 'V', &local.s_values[0]);
		es_nlines = 1;
		break;

	case TOR:
		r1 = MAGNITUDE(&local.s_tor_A);
		r2 = MAGNITUDE(&local.s_tor_H);
		PR_PT( 0, 'V', &local.s_tor_V );

		(void)sprintf( &es_display[1*ES_LINELEN],
			"\tr1=%f, r2=%f%c", r1, r2, '\0' );

		if( r2 < EPSILON )  {
			PR_STR( 2, "N too small");
		} else {
			VSCALE( unitv, &local.s_tor_H, 1.0/r2 );/* N == H^ */
			PR_PT( 2, 'N', unitv );
		}

		findang( ang, unitv );
		PR_ANG( 3, "N", ang );

		PR_PT( 4, 'I', &local.s_tor_C );
		PR_PT( 5, 'O', &local.s_tor_E );
		PR_PT( 6, 'H', &local.s_tor_H );
		es_nlines = 7;
		break;

	case GENARB8:
		PR_PT( 0, '1', &local.s_values[0] );
		switch( cgtype ) {
			case ARB8:
				es_nlines = length = 8;
/* common area for arbs */
arbcommon:
				for(i=3; i<3*length; i+=3) {
					VADD2( work, &local.s_values[i], &local.s_values[0] );
					PR_PT( i/3, '1'+(i/3), work );
				}
				break;

			case ARB7:
				es_nlines = length = 7;
				goto arbcommon;

			case ARB6:
				es_nlines = length = 6;
				VMOVE(&local.s_values[15], &local.s_values[18]);
				goto arbcommon;

			case ARB5:
				es_nlines = length = 5;
				goto arbcommon;

			case ARB4:
				es_nlines = length = 4;
				VMOVE(&local.s_values[9], &local.s_values[12]);
				goto arbcommon;

			default:
				/* use ARB8 */
				es_nlines = length = 8;
				goto arbcommon;
		}
		break;

	case GENELL:
		PR_PT( 0, 'V', &local.s_ell_V );

		ma = MAGNITUDE( &local.s_ell_A );
		PR_VECM( 1, 'A', &local.s_ell_A, ma );

		if( ma < EPSILON )  {
			PR_STR( 2, "A too small");
		} else {
			VSCALE( unitv, &local.s_ell_A, 1.0/ma );
			findang( ang, unitv );
			PR_ANG( 2, "A", ang );
		}

		ma = MAGNITUDE( &local.s_ell_B );
		PR_VECM( 3, 'B', &local.s_ell_B, ma );

		if( ma < EPSILON )  {
			PR_STR( 4, "B too small");
		} else {
			VSCALE( unitv, &local.s_ell_B, 1.0/ma );
			findang( ang, unitv );
			PR_ANG( 4, "B", ang );
		}

		ma = MAGNITUDE( &local.s_ell_C );
		PR_VECM( 5, 'C', &local.s_ell_C, ma );

		if( ma < EPSILON )  {
			PR_STR( 6, "C too small");
		} else {
			VSCALE( unitv, &local.s_ell_C, 1.0/ma );
			findang( ang, unitv );
			PR_ANG( 6, "C", ang );
		}

		es_nlines = 7;
		break;

	case GENTGC:
		PR_PT( 0, 'V', &local.s_tgc_V );

		ma = MAGNITUDE( &local.s_tgc_H );
		PR_VECM( 1, 'H', &local.s_tgc_H, ma );

		if( ma < EPSILON )  {
			PR_STR( 2, "H magnitude too small");
		} else {
			VSCALE( unitv, &local.s_tgc_H, 1.0/ma );
			findang( ang, unitv );
			PR_ANG( 2, "H", ang );
		}

		ma = MAGNITUDE( &local.s_tgc_A );
		PR_VECM( 3, 'A', &local.s_tgc_A, ma );

		ma = MAGNITUDE( &local.s_tgc_B );
		PR_VECM( 4, 'B', &local.s_tgc_B, ma );

		(void)sprintf( &es_display[5*ES_LINELEN],
			"\tc = %f, d = %f%c",
			MAGNITUDE( &local.s_tgc_C ),
			MAGNITUDE( &local.s_tgc_D ), '\0' );

		/* AxB */
		VCROSS( unitv, &local.s_tgc_C, &local.s_tgc_D );
		VUNITIZE( unitv );
		findang( ang, unitv );
		PR_ANG( 6, "AxB", ang );

		es_nlines = 7;
		break;
	}
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

	case MENUH:	/* scale height vector */
		op = &es_rec.s.s_tgc_H;
		if( inpara ) {
			/* take es_mat[15] (path scaling) into account */
			es_para[0] *= es_mat[15];
			es_scale = es_para[0] / MAGNITUDE(op);
		}

		VSCALE(op, op, es_scale);
		break;

	case MENUR1:
		/* scale radius 1 of TOR */
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
		break;

	case MENUR2:
		/* scale radius 2 of TOR */
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

	case MENUA:
		/* scale vector A */
		switch( es_gentype ) {

		case GENELL:
			op = &es_rec.s.s_ell_A;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			break;

		case GENTGC:
			op = &es_rec.s.s_tgc_A;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			break;

		}
		break;

	case MENUB:
		/* scale vector B */
		switch(es_gentype) {

		case GENELL:
			op = &es_rec.s.s_ell_B;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			break;

		case GENTGC:
			op = &es_rec.s.s_tgc_B;
			if( inpara ) {
				/* take es_mat[15] (path scaling) into account */
				es_para[0] *= es_mat[15];
				es_scale = es_para[0] / MAGNITUDE(op);
			}
			VSCALE(op, op, es_scale);
			break;
		}
		break;

	case MENUC:
		/* scale vector C (ELL only) */
		op = &es_rec.s.s_ell_C;
		if( inpara ) {
			/* take es_mat[15] (path scaling) into account */
			es_para[0] *= es_mat[15];
			es_scale = es_para[0] / MAGNITUDE(op);
		}

		VSCALE(op, op, es_scale);
		break;

	case MENUP1:
		/* TGC: scale ratio "c" */
		op = &es_rec.s.s_tgc_C;
		if( inpara ) {
			/* take es_mat[15] (path scaling) into account */
			es_para[0] *= es_mat[15];
			es_scale = es_para[0] / MAGNITUDE(op);
		}
		VSCALE(op, op, es_scale);
		break;

	case MENUP2:   /* scale  d for tgc */
		op = &es_rec.s.s_tgc_D;
		if( inpara ) {
			/* take es_mat[15] (path scaling) into account */
			es_para[0] *= es_mat[15];
			es_scale = es_para[0] / MAGNITUDE(op);
		}
		VSCALE(op, op, es_scale);
		break;

	case MENUAB:
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
		break;

	case MENUCD:	/* scale C and D of tgc */
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
		break;

	case MENUABCD: 		/* scale A,B,C, and D of tgc */
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
		break;

	case MENUABC:	/* scale A,B, and C of ellg */
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
	struct rt_external	es_ext;	/* This should be external */
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
		/* XXX should have an es_keypoint for this */
		VMOVE(es_rec.s.s_values, illump->s_center);

		/* Zero the other values */
		for(i=3; i<24; i++)
			es_rec.s.s_values[i] = 0.0;

		/* The s_center takes the es_mat into account already */
		return;
	}

	/* Not an evaluated region - just a regular path ending in a solid */
	/* XXX should be db_getmrec() */
	if( db_get_external( &es_ext, illump->s_path[illump->s_last], dbip ) < 0 )  {
		(void)printf("init_objedit(%s): db_get_external failure\n",
			illump->s_path[illump->s_last]->d_namep );
		button(BE_REJECT);
		return;
	}
	/* XXX hack:  get first granule into es_rec (ugh) */
	bcopy( (char *)es_ext.ext_buf, (char *)&es_rec, sizeof(es_rec) );

	/* XXX should have an es_keypoint here;
	 * instead, be certain that es_rec.s.s_values[0] has key point! */
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
			VMOVE(es_rec.s.s_values, rec[1].b.b_values);
			es_rec.s.s_type = ARS;		/* XXX wrong */
			es_rec.s.s_cgtype = ARS;
		}
		break;

	case ID_TOR:
	case ID_TGC:
	case ID_ELL:
	case ID_ARB8:
	case ID_HALF:
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
		break;

	case ID_EBM:
		/* Use model origin as key point */
		VSETALL(es_rec.s.s_values, 0 );
		break;

	default:
		/* XXX Need SOMETHING in s_values, s/b es_keypoint */
		printf("init_objedit() using %g,%g,%g as keypoint\n",
			V3ARGS(illump->s_center) );
		VMOVE(es_rec.s.s_values, illump->s_center);
	}

	/* Save aggregate path matrix */
	pathHmat( illump, es_mat, illump->s_last-1 );

	/* get the inverse matrix */
	mat_inv( es_invmat, es_mat );

	/* fill the display array */
	pr_solid( &es_rec.s );

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
	else if( es_edflag != ROTFACE ){
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
	pr_solid( &es_rec.s );
	dmaflag = 1;
}

/* Hooks from buttons.c */

void
sedit_accept()
{
	if( not_state( ST_S_EDIT, "Solid edit accept" ) )  return;

	/* write editing changes out to disc */
	db_put( dbip, illump->s_path[illump->s_last], &es_rec, 0, 1 );
	es_edflag = -1;
	menuflag = 0;
	movedir = 0;
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
}
