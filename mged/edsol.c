/*
 *			E D S O L . C
 *
 * Functions -
 *	redraw		redraw a single solid, given matrix and record.
 *	init_sedit	set up for a Solid Edit
 *	sedit		Apply Solid Edit transformation(s)
 *	findang		Given a normal vector, find rotation & fallback angles
 *	pr_solid	Print a description of a solid
 *	pscale		Partial scaling of a solid
 *	init_objedit	set up for object edit?
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

#include	<math.h>
#include	<string.h>
#include "./machine.h"	/* special copy */
#include "../h/vmath.h"
#include "../h/db.h"
#include "sedit.h"
#include "ged.h"
#include "solid.h"
#include "objdir.h"
#include "dm.h"
#include "menu.h"

extern int	printf();

static void	arb8_ed(), ars_ed(), ell_ed(), tgc_ed(), tor_ed(), spline_ed();
static void	arb7_ed(), arb6_ed(), arb5_ed(), arb4_ed();

void pscale();

struct menu_item  arb8_menu[] = {
	{ "ARB8 MENU", (void (*)())NULL, 0 },
	{ "move edge 12", arb8_ed, 0 },
	{ "move edge 23", arb8_ed, 1 },
	{ "move edge 34", arb8_ed, 2 },
	{ "move edge 14", arb8_ed, 3 },
	{ "move edge 15", arb8_ed, 4 },
	{ "move edge 26", arb8_ed, 5 },
	{ "move edge 56", arb8_ed, 6 },
	{ "move edge 67", arb8_ed, 7 },
	{ "move edge 78", arb8_ed, 8 },
	{ "move edge 58", arb8_ed, 9 },
	{ "move edge 37", arb8_ed, 10 },
	{ "move edge 48", arb8_ed, 11 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  arb7_menu[] = {
	{ "ARB7 MENU", (void (*)())NULL, 0 },
	{ "move edge 12", arb7_ed, 0 },
	{ "move edge 23", arb7_ed, 1 },
	{ "move edge 34", arb7_ed, 2 },
	{ "move edge 14", arb7_ed, 3 },
	{ "move edge 15", arb7_ed, 4 },
	{ "move edge 26", arb7_ed, 5 },
	{ "move edge 56", arb7_ed, 6 },
	{ "move edge 67", arb7_ed, 7 },
	{ "move edge 37", arb7_ed, 8 },
	{ "move edge 57", arb7_ed, 9 },
	{ "move edge 45", arb7_ed, 10 },
	{ "move point 5", arb7_ed, 11 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  arb6_menu[] = {
	{ "ARB6 MENU", (void (*)())NULL, 0 },
	{ "move edge 12", arb6_ed, 0 },
	{ "move edge 23", arb6_ed, 1 },
	{ "move edge 34", arb6_ed, 2 },
	{ "move edge 14", arb6_ed, 3 },
	{ "move edge 15", arb6_ed, 4 },
	{ "move edge 25", arb6_ed, 5 },
	{ "move edge 36", arb6_ed, 6 },
	{ "move edge 46", arb6_ed, 7 },
	{ "move point 5", arb6_ed, 8 },
	{ "move point 6", arb6_ed, 9 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  arb5_menu[] = {
	{ "ARB5 MENU", (void (*)())NULL, 0 },
	{ "move edge 12", arb5_ed, 0 },
	{ "move edge 23", arb5_ed, 1 },
	{ "move edge 34", arb5_ed, 2 },
	{ "move edge 14", arb5_ed, 3 },
	{ "move edge 15", arb5_ed, 4 },
	{ "move edge 25", arb5_ed, 5 },
	{ "move edge 35", arb5_ed, 6 },
	{ "move edge 45", arb5_ed, 7 },
	{ "move point 5", arb5_ed, 8 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  arb4_menu[] = {
	{ "ARB4 MENU", (void (*)())NULL, 0 },
	{ "move point 1", arb4_ed, 0 },
	{ "move point 2", arb4_ed, 1 },
	{ "move point 3", arb4_ed, 2 },
	{ "move point 4", arb4_ed, 4 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  tgc_menu[] = {
	{ "TGC MENU", (void (*)())NULL, 0 },
	{ "scale H",	tgc_ed, MENUH },
	{ "scale A",	tgc_ed, MENUA },
	{ "scale B",	tgc_ed, MENUB },
	{ "scale c",	tgc_ed, MENUP1 },
	{ "scale d",	tgc_ed, MENUP2 },
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


static void
arb8_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = EARB;
}


static void
arb7_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = EARB;
	if(arg == 11) {
		/* move point 5 */
		es_edflag = PTARB;
		es_menu = 4;	/* location of point */
	}
}

static void
arb6_ed( arg )
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
}

static void
arb5_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = EARB;
	if(arg == 8) {
		/* move point 5 at loaction 4 */
		es_edflag = PTARB;
		es_menu = 4;
	}
}

static void
arb4_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = PTARB;
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
 *  			R E D R A W
 *  
 *  Probably misnamed.
 */
struct solid *
redraw( sp, recp )
struct solid *sp;
union record *recp;
{
	int addr, bytes;

	if( sp == SOLID_NULL )
		return( sp );

	/* Remember displaylist location of previous solid */
	addr = sp->s_addr;
	bytes = sp->s_bytes;

	if( drawHsolid(
		sp,
		sp->s_soldash,
		sp->s_last,
		es_mat,
		recp,
		sp->s_regionid
	) != 1 )  {
		(void)printf("redraw():  error in drawHsolid()\n");
		return(sp);
	}

	/* Release previous chunk of displaylist, and rewrite control list */
	memfree( &(dmp->dmr_map), (unsigned)bytes, (unsigned long)addr );
	dmaflag = 1;
	return( sp );
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
	union record temprec;	/* copy of solid record to determine type */
	register int i, type, p1, p2, p3;
	float *op;

	/*
	 * Check for a processed region or other illegal solid.
	 */
	if( illump->s_Eflag )  {
		(void)printf(
"Unable to Solid_Edit a processed region;  select a primitive instead\n");
		return;
	}

	/* Read solid description.  Save copy of original data */
	db_getrec( illump->s_path[illump->s_last], &es_orig, 0 );
	es_rec = es_orig;		/* struct copy */

	if( es_rec.u_id != ID_SOLID )  {
		(void)printf(
"Unable to Solid_Edit %c record;  select a primitive instead\n");
		return;
	}

	es_menu = 0;

	temprec = es_rec;

	if( (type = es_rec.s.s_cgtype) < 0 )
		type *= -1;
	if(type == BOX || type == RPP)
		type = ARB8;
	if(type == RAW) {
		/* rearrange vectors to correspond to the
		 *  	"standard" ARB6
		 */
		VMOVE(&temprec.s.s_values[3], &es_rec.s.s_values[9]);
		VMOVE(&temprec.s.s_values[6], &es_rec.s.s_values[21]);
		VMOVE(&temprec.s.s_values[9], &es_rec.s.s_values[12]);
		VMOVE(&temprec.s.s_values[12], &es_rec.s.s_values[3]);
		VMOVE(&temprec.s.s_values[15], &es_rec.s.s_values[6]);
		VMOVE(&temprec.s.s_values[18], &es_rec.s.s_values[18]);
		VMOVE(&temprec.s.s_values[21], &es_rec.s.s_values[15]);
		es_rec = temprec;
		type = ARB6;
	}
	es_rec.s.s_cgtype = type;

	if( es_rec.s.s_type == GENARB8 ) {
		/* find the comgeom arb type */
		if( (type = type_arb( &es_rec )) == 0 ) {
			(void)printf("%s: BAD ARB\n",es_rec.s.s_name);
			return;
		}

		temprec = es_rec;
		es_rec.s.s_cgtype = type;

		/* find the plane equations */
		/* point notation - use temprec record */
		for(i=3; i<=21; i+=3) {
			op = &temprec.s.s_values[i];
			VADD2(op, op, &es_rec.s.s_values[0]);
		}
		type -= 4;	/* ARB4 at location 0, ARB5 at 1, etc */
		for(i=0; i<6; i++) {
			if(faces[type][i*4] == -1)
				break;	/* faces are done */
			p1 = faces[type][i*4];
			p2 = faces[type][i*4+1];
			p3 = faces[type][i*4+2];
			if(planeqn(i, p1, p2, p3, &temprec.s)) {
				(void)printf("No eqn for face %d%d%d%d\n",
					p1+1,p2+1,p3+1,faces[type][i*4+3]+1);
				return;
			}
/*
printf("peqn[%d][]: %.4f %.4f %.4f %.4f\n",i,es_peqn[i][0],es_peqn[i][1],es_peqn[i][2],es_peqn[i][3]);
*/
		}
	}


	/* Save aggregate path matrix */
	pathHmat( illump, es_mat );

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

/* put up menu header */
void
sedit_menu()  {

	menuflag = 0;		/* No menu item selected yet */

	menu_array[MENU_L1] = MENU_NULL;
	chg_l2menu(ST_S_EDIT);

	switch( es_gentype ) {

	case GENARB8:
		switch( es_type ) {

			case ARB8:
				menu_array[MENU_L1] = arb8_menu;
			break;

		 	case ARB7:
				menu_array[MENU_L1] = arb7_menu;
			break;
		 	case ARB6:
				menu_array[MENU_L1] = arb6_menu;
			break;
		 	case ARB5:
				menu_array[MENU_L1] = arb5_menu;
			break;
		 	case ARB4:
				menu_array[MENU_L1] = arb4_menu;
			break;
		}
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
	register float *op;
	static vect_t work;
	register int i;
	static int j;
	static float la, lb, lc, ld;	/* TGC: length of vectors */

	sedraw = 0;

	switch( es_edflag ) {

	case IDLE:
		/* do nothing */
		break;

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

	illump = redraw( illump, &es_rec );

	inpara = 0;
	pr_solid( &es_rec.s );
	return;
}


/*
 *			F I N D A N G
 *
 * finds direction cosines and rotation, fallback angles of a unit vector
 * angles = pointer to 5 floats to store angles
 * unitv = pointer to the unit vector (previously computed)
 */
void
findang( angles, unitv )
register float *angles, *unitv;
{
	register int i;

	/* direction cos */
	for( i=0; i<3; i++ ) 
		angles[i] = acos( unitv[i] ) * radtodeg;

	/* fallback angle */
	angles[4] = asin(unitv[Z]);

	/* rotation angle */
	if(cos(angles[4]) != 0.0)
		angles[3] = radtodeg * acos( unitv[X]/cos(angles[4]) );
	else
		angles[3] = 0.0;
	angles[4] *= radtodeg;
}

#define EPSILON 1.0e-7

#define PR_STR(ln,str)	(void)strcpy(&es_display[ln*ES_LINELEN], str);

#define PR_PT(ln,title,base)	(void)sprintf( &es_display[ln*ES_LINELEN],\
		" %c (%.4f, %.4f, %.4f)%c", \
		title, (base)[X], (base)[Y], (base)[Z], '\0' )

#define PR_VECM(ln,title,base,mag)	(void)sprintf( &es_display[ln*ES_LINELEN],\
		" %c (%.4f, %.4f, %.4f) Mag=%f%c", \
		title, (base)[X], (base)[Y], (base)[Z], mag, '\0' )

#define PR_ANG(ln,str,base)	(void)sprintf( &es_display[ln*ES_LINELEN],\
		" %s dir cos=(%.1f, %.1f, %.1f), rot=%.1f, fb=%.1f%c", \
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
	static float ma;
	static float r1, r2;
	static vect_t unitv;
	static float ang[5];
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

	case TOR:
		r1 = MAGNITUDE(&local.s_tor_A);
		r2 = MAGNITUDE(&local.s_tor_H);
		PR_PT( 0, 'V', &local.s_tor_V );

		(void)sprintf( &es_display[1*ES_LINELEN],
			" r1=%f, r2=%f%c", r1, r2, '\0' );

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
				for(i=3; i<=3*length; i+=3) {
					VADD2( work, &local.s_values[i], &local.s_values[0] );
					PR_PT( i/3, '1'+(i/3), work );
				}
			break;

			case ARB7:
				es_nlines = length = 7;
				goto arbcommon;
			break;

			case ARB6:
				es_nlines = length = 6;
				VMOVE(&local.s_values[15], &local.s_values[18]);
				goto arbcommon;
			break;

			case ARB5:
				es_nlines = length = 5;
				goto arbcommon;
			break;

			case ARB4:
				es_nlines = length = 4;
				VMOVE(&local.s_values[9], &local.s_values[12]);
				goto arbcommon;
			break;

			default:
				/* use ARB8 */
				es_nlines = length = 8;
				goto arbcommon;
			break;

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
			" c = %f, d = %f%c",
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
	register float *op;
	static float ma,mb;
	static float mr1,mr2;

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
	}
}

/*
 *			I N I T _ O B J E D I T
 *
 */
void
init_objedit()
{
	register int i;

	/*
	 * Check for a processed region 
	 */
	if( illump->s_Eflag )  {

		/* Have a processed (E'd) region - NO key solid.
		 * 	Use the 'center' as the key
		 */
		VMOVE(es_rec.s.s_values, illump->s_center);

		/* Zero the other values */
		for(i=3; i<24; i++)
			es_rec.s.s_values[i] = 0.0;

		/* The s_center takes the es_mat into account already */
		mat_idn(es_mat);

		/* for safety sake */
		es_menu = 0;
		es_edflag = -1;

		return;
	}

	/* Not an evaluated region - just a regular path ending in a solid */
	db_getrec( illump->s_path[illump->s_last], &es_rec, 0 );

	if( es_rec.u_id == ID_ARS_A || es_rec.u_id == ID_B_SPL_HEAD )  {
		(void)printf("ARS or SPLINE may not work well\n");
		mat_idn(es_mat);
		es_menu = 0;
		es_edflag = -1;
		return;
	}

	if( es_rec.u_id != ID_SOLID )  {
		(void)printf(
"ERROR - Should have a SOLID at bottom of path\n");
		return;
	}

	es_menu = 0;
	es_edflag = -1;

	/* Save aggregate path matrix */
	pathHmat( illump, es_mat );

	/* get the inverse matrix */
	mat_inv( es_invmat, es_mat );

	/* fill the display array */
	pr_solid( &es_rec.s );

}
