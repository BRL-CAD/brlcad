/*
 *			E D S O L . C
 *
 * Functions -
 *	redraw		redraw a single solid, given matrix and record.
 *	init_sedit	set up for a Solid Edit
 *	sedit		Apply Solid Edit transformation(s)
 *	findang		Given a normal vector, find rotation & fallback angles
 *	pr_solid	Print a description of a solid
 *	plane
 *
 */

#include	<math.h>
#include	<string.h>
#include "ged_types.h"
#include "db.h"
#include "sedit.h"
#include "vmath.h"
#include "ged.h"
#include "solid.h"
#include "dir.h"
#include "dm.h"
#include "menu.h"

extern int	printf();

static void	arb_ed(), ars_ed(), ell_ed(), tgc_ed(), tor_ed();

struct menu_item  arb_menu[] = {
	{ "GENERAL ARB MENU", (void (*)())NULL, 0 },
	{ "move edge 12", arb_ed, 12 },
	{ "move edge 23", arb_ed, 23 },
	{ "move edge 34", arb_ed, 43 },
	{ "move edge 14", arb_ed, 14 },
	{ "move edge 15", arb_ed, 15 },
	{ "move edge 26", arb_ed, 26 },
	{ "move edge 56", arb_ed, 56 },
	{ "move edge 67", arb_ed, 67 },
	{ "move edge 78", arb_ed, 87 },
	{ "move edge 58", arb_ed, 58 },
	{ "move edge 37", arb_ed, 37 },
	{ "move edge 48", arb_ed, 48 },
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
	{ "move end of H", tgc_ed, MENUMH },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  tor_menu[] = {
	{ "TORUS MENU", (void (*)())NULL, 0 },
	{ "scale radius 1", tor_ed, MENUR1 },
	{ "scale radius 2", tor_ed, MENUR2 },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item  ell_menu[] = {
	{ "GENERAL ELLIPSOID MENU", (void (*)())NULL, 0 },
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


static void
arb_ed( arg )
int arg;
{
	es_menu = arg;
	es_edflag = EARB;
	newedge = 1;
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
		illump->s_soldash,
		illump->s_last,
		es_mat,
		recp ) != 1
	)  {
		(void)printf("redraw():  error in drawHsolid()\n");
		return(sp);
	}

	/* Release previous chunk of displaylist, and rewrite control list */
	memfree( &(dmp->dmr_map), bytes, addr );
	dmaflag = 1;
	return( sp );
}

/*
 *			I N I T _ S E D I T
 *
 *  First time in for this solid, set things up.
 *  If all goes well, change state to ST_S_EDIT.
 */
init_sedit()
{
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
	if(es_rec.s.s_cgtype < 0)
		es_rec.s.s_cgtype *= -1;

	/* Save aggregate path matrix */
	pathHmat( illump, es_mat );

	/* get the inverse matrix */
	mat_inv( es_invmat, es_mat );

	/* put up menu header */
	MENU_ON( FALSE );
	switch( es_gentype ) {

	case GENARB8:
		MENU_INSTALL( arb_menu );
		break;
	case GENTGC:
		MENU_INSTALL( tgc_menu );
		break;
	case TOR:
		MENU_INSTALL( tor_menu );
		break;
	case GENELL:
		MENU_INSTALL( ell_menu );
		break;
	case ARS:
		MENU_INSTALL( ars_menu );
		break;
	}

	/* Finally, enter solid edit state */
	dmp->dmr_light( LIGHT_ON, BE_ACCEPT );
	dmp->dmr_light( LIGHT_ON, BE_REJECT );
	dmp->dmr_light( LIGHT_OFF, BE_S_ILLUMINATE );

	(void)chg_state( ST_S_PICK, ST_S_EDIT, "Keyboard illuminate");
	es_edflag = IDLE;
	sedraw = 1;

	button( BE_S_EDIT );	/* Drop into edit menu right away */
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

	case PSCALE:
		pscale();
		break;

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
		printf("sedit():  unknown edflag = %d.\n", es_edflag );
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

#define PR_PT(ln,title,base)	sprintf( &es_display[ln*ES_LINELEN],\
		" %c (%.4f, %.4f, %.4f)%c", \
		title, (base)[X], (base)[Y], (base)[Z], '\0' )

#define PR_VECM(ln,title,base,mag)	sprintf( &es_display[ln*ES_LINELEN],\
		" %c (%.4f, %.4f, %.4f) Mag=%f%c", \
		title, (base)[X], (base)[Y], (base)[Z], mag, '\0' )

#define PR_ANG(ln,str,base)	sprintf( &es_display[ln*ES_LINELEN],\
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

	/* convert to local units */
	for(i=0; i<24; i+=3) {
		VSCALE(work, &sp->s_values[i], base2local);
		VMOVE(&sp->s_values[i], work);
	}

	switch( sp->s_type ) {

	case TOR:
		r1 = MAGNITUDE(&sp->s_tor_A);
		r2 = MAGNITUDE(&sp->s_tor_H);
		PR_PT( 0, 'V', &sp->s_tor_V );

		sprintf( &es_display[1*ES_LINELEN],
			" r1=%f, r2=%f%c", r1, r2, '\0' );

		VSCALE( unitv, &sp->s_tor_H, 1.0/r2 );	/* N == H^ */
		PR_PT( 2, 'N', unitv );

		findang( ang, unitv );
		PR_ANG( 3, "N", ang );

		PR_PT( 4, 'I', &sp->s_tor_C );
		PR_PT( 5, 'O', &sp->s_tor_E );
		PR_PT( 6, 'H', &sp->s_tor_H );
		es_nlines = 7;
		break;

	case GENARB8:
		PR_PT( 0, '1', &sp->s_values[0] );
		for(i=3; i<=21; i+=3) {
			VADD2( work, &sp->s_values[i], &sp->s_values[0] );
			PR_PT( i/3, '1'+(i/3), work );
		}
		es_nlines = 8;
		break;

	case GENELL:
		PR_PT( 0, 'V', &sp->s_ell_V );

		ma = MAGNITUDE( &sp->s_ell_A );
		PR_VECM( 1, 'A', &sp->s_ell_A, ma );

		VSCALE( unitv, &sp->s_ell_A, 1.0/ma );
		findang( ang, unitv );
		PR_ANG( 2, "A", ang );

		ma = MAGNITUDE( &sp->s_ell_B );
		PR_VECM( 3, 'B', &sp->s_ell_B, ma );

		VSCALE( unitv, &sp->s_ell_B, 1.0/ma );
		findang( ang, unitv );
		PR_ANG( 4, "B", ang );

		ma = MAGNITUDE( &sp->s_ell_C );
		PR_VECM( 5, 'C', &sp->s_ell_C, ma );

		VSCALE( unitv, &sp->s_ell_C, 1.0/ma );
		findang( ang, unitv );
		PR_ANG( 6, "C", ang );

		es_nlines = 7;
		break;

	case GENTGC:
		PR_PT( 0, 'V', &sp->s_tgc_V );

		ma = MAGNITUDE( &sp->s_tgc_H );
		PR_VECM( 1, 'H', &sp->s_tgc_H, ma );

		VSCALE( unitv, &sp->s_tgc_H, 1.0/ma );
		findang( ang, unitv );
		PR_ANG( 2, "H", ang );

		ma = MAGNITUDE( &sp->s_tgc_A );
		PR_VECM( 3, 'A', &sp->s_tgc_A, ma );

		ma = MAGNITUDE( &sp->s_tgc_B );
		PR_VECM( 4, 'B', &sp->s_tgc_B, ma );

		(void)sprintf( &es_display[5*ES_LINELEN],
			" c = %f, d = %f%c",
			MAGNITUDE( &sp->s_tgc_C ),
			MAGNITUDE( &sp->s_tgc_D ), '\0' );

		/* AxB */
		VCROSS( unitv, &sp->s_tgc_C, &sp->s_tgc_D );
		VUNITIZE( unitv );
		findang( ang, unitv );
		PR_ANG( 6, "AxB", ang );

		es_nlines = 7;
		break;
	}

	/* convert back to base units */
	for(i=0; i<24; i+=3) {
		VSCALE(work, &sp->s_values[i], local2base);
		VMOVE(&sp->s_values[i], work);
	}
}

/*
 *  			P S C A L E
 *  
 *  Partial scaling of a solid.
 */
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
init_objedit()
{
	int i;

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
	pr_solid( &es_rec );

}

