
/*
 *			T R A C K . C
 *
 *	f_amtrack():	Adds "tracks" to the data file given the required info
 *
 *  Author -
 *	Keith A. Applin
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <signal.h>
#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

extern void aexists();

static int	Trackpos = 0;
static fastf_t	plano[4], plant[4];

static union record record;

void		crname(), slope(), crdummy(), trcurve();
void		bottom(), top(), crregion(), itoa();

/*
 *
 *	F _ A M T R A C K ( ) :	adds track given "wheel" info
 *
 */
int
f_amtrack(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{

	register struct directory *dp;
	fastf_t fw[3], lw[3], iw[3], dw[3], tr[3];
	char solname[12], regname[12], grpname[9], oper[3];
	int i, j, memb[4];
	char temp[4];
	vect_t	temp1, temp2;
	int item, mat, los;
	int arg;
	int edit_result;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* interupts */
	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
	else
	  return TCL_OK;

	oper[0] = oper[2] = INTERSECT;
	oper[1] = SUBTRACT;

	arg = 1;

	/* get the roadwheel info */
	if ( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the FIRST roadwheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	fw[0] = atof( argv[arg] ) * local2base;
	++arg;

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the LAST roadwheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	lw[0] = atof( argv[arg] ) * local2base;
	++arg;

	if( fw[0] <= lw[0] ) {
	  Tcl_AppendResult(interp, "First wheel after last wheel - STOP\n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Z of the roadwheels: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	fw[1] = lw[1] = atof( argv[arg] ) * local2base;
	++arg;

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter radius of the roadwheels: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	fw[2] = lw[2] = atof( argv[arg] ) * local2base;
	++arg;
	if( fw[2] <= 0 ) {
	  Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}

	if ( argc < arg+1 ) {
	  /* get the drive wheel info */
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the drive (REAR) wheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	dw[0] = atof( argv[arg] ) * local2base;
	++arg;
	if( dw[0] >= lw[0] ) {
	  Tcl_AppendResult(interp, "DRIVE wheel not in the rear - STOP \n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Z of the drive (REAR) wheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	dw[1] = atof( argv[arg] ) * local2base;
	++arg;

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter radius of the drive (REAR) wheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	dw[2] = atof( argv[arg] ) * local2base;
	++arg;
	if( dw[2] <= 0 ) {
	  Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	
	/* get the idler wheel info */
	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the idler (FRONT) wheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	iw[0] = atof( argv[arg] ) * local2base;
	++arg;
	if( iw[0] <= fw[0] ) {
	  Tcl_AppendResult(interp, "IDLER wheel not in the front - STOP \n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Z of the idler (FRONT) wheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	iw[1] = atof( argv[arg] ) * local2base;
	++arg;

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter radius of the idler (FRONT) wheel: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	iw[2] = atof( argv[arg] ) * local2base;
	++arg;
	if( iw[2] <= 0 ) {
	  Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}

	/* get track info */
	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Y-MIN of the track: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	tr[2] = tr[0] = atof( argv[arg] ) * local2base;
	++arg;

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Y-MAX of the track: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	tr[1] = atof( argv[arg] ) * local2base;
	++arg;
	if( tr[0] == tr[1] ) {
	  Tcl_AppendResult(interp, "MIN == MAX ... STOP\n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	if( tr[0] > tr[1] ) {
	  Tcl_AppendResult(interp, "MIN > MAX .... will switch\n", (char *)NULL);
	  tr[1] = tr[0];
	  tr[0] = tr[2];
	}

	if( argc < arg+1 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter track thickness: ",
			   (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}
	tr[2] = atof( argv[arg] ) * local2base;
	++arg;
	if( tr[2] <= 0 ) {
	  Tcl_AppendResult(interp, "Track thickness <= 0 - STOP\n", (char *)NULL);
	  edit_result = TCL_ERROR;
	  goto end;
	}

	solname[0] = regname[0] = grpname[0] = 't';
	solname[1] = regname[1] = grpname[1] = 'r';
	solname[2] = regname[2] = grpname[2] = 'a';
	solname[3] = regname[3] = grpname[3] = 'c';
	solname[4] = regname[4] = grpname[4] = 'k';
	solname[5] = regname[5] = '.';
	solname[6] = 's';
	regname[6] = 'r';
	solname[7] = regname[7] = '.';
	grpname[5] = solname[8] = regname[8] = '\0';
	grpname[8] = solname[11] = regname[11] = '\0';
/*
	bu_log("\nX of first road wheel  %10.4f\n",fw[0]);
	bu_log("X of last road wheel   %10.4f\n",lw[0]);
	bu_log("Z of road wheels       %10.4f\n",fw[1]);
	bu_log("radius of road wheels  %10.4f\n",fw[2]);
	bu_log("\nX of drive wheel       %10.4f\n",dw[0]);
	bu_log("Z of drive wheel       %10.4f\n",dw[1]);
	bu_log("radius of drive wheel  %10.4f\n",dw[2]);
	bu_log("\nX of idler wheel       %10.4f\n",iw[0]);
	bu_log("Z of idler wheel       %10.4f\n",iw[1]);
	bu_log("radius of idler wheel  %10.4f\n",iw[2]);
	bu_log("\nY MIN of track         %10.4f\n",tr[0]);
	bu_log("Y MAX of track         %10.4f\n",tr[1]);
	bu_log("thickness of track     %10.4f\n",tr[2]);
*/

/* Check for names to use:
 *	1.  start with track.s.1->10 and track.r.1->10
 *	2.  if bad, increment count by 10 and try again
 */

tryagain:	/* sent here to try next set of names */

	for(i=0; i<11; i++) {
		crname(solname, i);
		crname(regname, i);
		if(	(db_lookup( dbip, solname, LOOKUP_QUIET) != DIR_NULL)	||
			(db_lookup( dbip, regname, LOOKUP_QUIET) != DIR_NULL)	) {
			/* name already exists */
			solname[8] = regname[8] = '\0';
			if( (Trackpos += 10) > 500 ) {
			  Tcl_AppendResult(interp, "Track: naming error -- STOP\n",
					   (char *)NULL);
			  return TCL_ERROR;
			}
			goto tryagain;
		}
		solname[8] = regname[8] = '\0';
	}

	/* no interupts */
	(void)signal( SIGINT, SIG_IGN );

	record.s.s_id = ID_SOLID;

	/* find the front track slope to the idler */
	for(i=0; i<24; i++)
		record.s.s_values[i] = 0.0;

	slope(fw, iw, tr);
	VMOVE(temp2, &record.s.s_values[0]);
	crname(solname, 1);
	(void)strcpy(record.s.s_name, solname);
	record.s.s_type = GENARB8;
	record.s.s_cgtype = BOX;		/* BOX */
	if( wrobj(solname, DIR_SOLID) ) 
	  return TCL_ERROR;

	solname[8] = '\0';

	/* find track around idler */
	for(i=0; i<24; i++)
		record.s.s_values[i] = 0.0;
	record.s.s_type = GENTGC;
	record.s.s_cgtype = RCC;
	trcurve(iw, tr);
	crname(solname, 2);
	(void)strcpy(record.s.s_name, solname);
	if( wrobj( solname , DIR_SOLID ) )
	  return TCL_ERROR;
	solname[8] = '\0';
	/* idler dummy rcc */
	record.s.s_values[6] = iw[2];
	record.s.s_values[11] = iw[2];
	VMOVE(&record.s.s_values[12], &record.s.s_values[6]);
	VMOVE(&record.s.s_values[15], &record.s.s_values[9]);
	crname(solname, 3);
	(void)strcpy(record.s.s_name, solname);
	if( wrobj( solname , DIR_SOLID ) )
		return TCL_ERROR;
	solname[8] = '\0';

	/* find idler track dummy arb8 */
	for(i=0; i<24; i++)
		record.s.s_values[i] = 0.0;
	crname(solname, 4);
	(void)strcpy(record.s.s_name, solname);
	record.s.s_type = GENARB8;
	record.s.s_cgtype = ARB8;		/* arb8 */
	crdummy(iw, tr, 1);
	if( wrobj(solname,DIR_SOLID) )
	  return TCL_ERROR;
	solname[8] = '\0';

	/* track slope to drive */
	for(i=0; i<24; i++)
		record.s.s_values[i] = 0.0;
	slope(lw, dw, tr);
	VMOVE(temp1, &record.s.s_values[0]);
	crname(solname, 5);
	(void)strcpy(record.s.s_name, solname);
	record.s.s_cgtype = BOX;		/* box */
	if(wrobj(solname,DIR_SOLID))
		return TCL_ERROR;
	solname[8] = '\0';

	/* track around drive */
	for(i=0; i<24; i++)
		record.s.s_values[i] = 0.0;
	record.s.s_type = GENTGC;
	record.s.s_cgtype = RCC;
	trcurve(dw, tr);
	crname(solname, 6);
	(void)strcpy(record.s.s_name, solname);
	if( wrobj(solname,DIR_SOLID) )
		return TCL_ERROR;
	solname[8] = '\0';

	/* drive dummy rcc */
	record.s.s_values[6] = dw[2];
	record.s.s_values[11] = dw[2];
	VMOVE(&record.s.s_values[12], &record.s.s_values[6]);
	VMOVE(&record.s.s_values[15], &record.s.s_values[9]);
	crname(solname, 7);
	(void)strcpy(record.s.s_name, solname);
	if( wrobj(solname,DIR_SOLID) )
		return TCL_ERROR;
	solname[8] = '\0';

	/* drive dummy arb8 */
	for(i=0; i<24; i++)
		record.s.s_name[i] = 0.0;
	crname(solname, 8);
	(void)strcpy(record.s.s_name, solname);
	record.s.s_type = GENARB8;
	record.s.s_cgtype = ARB8;		/* arb8 */
	crdummy(dw, tr, 2);
	if( wrobj(solname,DIR_SOLID) )
		return TCL_ERROR;
	solname[8] = '\0';
	
	/* track bottom */
	record.s.s_cgtype = ARB8;		/* arb8 */
	temp1[1] = temp2[1] = tr[0];
	bottom(temp1, temp2, tr);
	crname(solname, 9);
	(void)strcpy(record.s.s_name, solname);
	if( wrobj(solname,DIR_SOLID) )
		return TCL_ERROR;
	solname[8] = '\0';

	/* track top */
	temp1[0] = dw[0];
	temp1[1] = temp2[1] = tr[0];
	temp1[2] = dw[1] + dw[2];
	temp2[0] = iw[0];
	temp2[2] = iw[1] + iw[2];
	top(temp1, temp2, tr);
	crname(solname, 10);
	(void)strcpy(record.s.s_name, solname);
	if( wrobj(solname,DIR_SOLID) )
		return TCL_ERROR;
	solname[8] = '\0';

	/* add the regions */
	bzero( (char *)&record , sizeof( union record ) );
	record.c.c_id = ID_COMB;
	record.c.c_flags = 'R';
	record.c.c_aircode = 0;
	record.c.c_regionid = 111;
	record.c.c_material = 0;
	record.c.c_los = 0;

	/* regions 3, 4, 7, 8 - dummy regions */
	for(i=3; i<5; i++) {
		regname[8] = '\0';
		crname(regname, i);
		(void)strcpy(record.c.c_name, regname);
		if( wrobj(regname,DIR_REGION|DIR_COMB) )
			return TCL_ERROR;
		regname[8] = '\0';
		crname(regname, i+4);
		(void)strcpy(record.c.c_name, regname);
		if( wrobj(regname,DIR_REGION|DIR_COMB) )
			return TCL_ERROR;
	}
	regname[8] = '\0';

	item = item_default;
	mat = mat_default;
	los = los_default;
	item_default = 500;
	mat_default = 1;
	los_default = 50;
	/* region 1 */
	memb[0] = 1;
	memb[1] = 4;
	crname(regname, 1);
	crregion(regname, oper, memb, 2, solname);
	solname[8] = regname[8] = '\0';

	/* region 2 */
	crname(regname, 2);
	memb[0] = 2;
	memb[1] = 3;
	memb[2] = 4;
	crregion(regname, oper, memb, 3, solname);
	solname[8] = regname[8] = '\0';

	/* region 5 */
	crname(regname, 5);
	memb[0] = 5;
	memb[1] = 8;
	crregion(regname, oper, memb, 2, solname);
	solname[8] = regname[8] = '\0';

	/* region 6 */
	crname(regname, 6);
	memb[0] = 6;
	memb[1] = 7;
	memb[2] = 8;
	crregion(regname, oper, memb, 3, solname);
	solname[8] = regname[8] = '\0';

	/* region 9 */
	crname(regname, 9);
	memb[0] = 9;
	memb[1] = 1;
	memb[2] = 5;
	oper[2] = SUBTRACT;
	crregion(regname, oper, memb, 3, solname);
	solname[8] = regname[8] = '\0';

	/* region 10 */
	crname(regname, 10);
	memb[0] = 10;
	memb[1] = 4;
	memb[2] = 8;
	crregion(regname, oper, memb, 3, solname);
	solname[8] = regname[8] = '\0';

	/* group all the track regions */
	j = 1;
	if( (i = Trackpos / 10 + 1) > 9 )
		j = 2;
	itoa(i, temp, j);
	(void)strcat(grpname, temp);
	grpname[8] = '\0';
	for(i=1; i<11; i++) {
		regname[8] = '\0';
		crname(regname, i);
		if( (dp = db_lookup( dbip, regname, LOOKUP_QUIET)) == DIR_NULL ) {
		  Tcl_AppendResult(interp, "group: ", grpname, " will skip member: ",
				   regname, "\n", (char *)NULL);
		  continue;
		}
		(void)combadd(dp, grpname, 0, UNION, 0, 0);
	}

	/* draw this track */
	Tcl_AppendResult(interp, "The track regions are in group ", grpname,
			 "\n", (char *)NULL);
	{
		char	*arglist[3];
		arglist[0] = "e";
		arglist[1] = grpname;
		arglist[2] = NULL;
		edit_result = f_edit( clientData, interp, 2, arglist );
	}

	Trackpos += 10;
	item_default = item;
	mat_default = mat;
	los_default = los;
	grpname[5] = solname[8] = regname[8] = '\0';

	return edit_result;
end:
	(void)signal( SIGINT, SIG_IGN );
	return edit_result;
}

void
crname(name, pos)
char name[];
int pos;
{
	int i, j;
	char temp[4];

	j=1;
	if( (i = Trackpos + pos) > 9 )
		j = 2;
	if( i > 99 )
		j = 3;
	itoa(i, temp, j);
	(void)strcat(name,temp);
	return;
}


wrobj( name, flags )
char name[];
int flags;
{
	struct directory *tdp;

	if( db_lookup( dbip, name, LOOKUP_QUIET) != DIR_NULL ) {
	  Tcl_AppendResult(interp, "amtrack naming error: ", name,
			   " already exists\n", (char *)NULL);
	  return(-1);
	}
	if( (tdp = db_diradd( dbip, name, -1, 1, flags)) == DIR_NULL ||
	    db_alloc( dbip, tdp, 1) < 0 ||
	    db_put( dbip, tdp, &record, 0, 1 ) < 0 )  {
	  Tcl_AppendResult(interp, "wrobj(", name, "):  write error\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return( -1 );
	}
	return(0);
}

void
tancir( cir1, cir2 )
register fastf_t cir1[], cir2[];
{
	static fastf_t mag;
	vect_t	work;
	FAST fastf_t f;
	static fastf_t	temp, tempp, ang, angc;

	work[0] = cir2[0] - cir1[0];
	work[2] = cir2[1] - cir1[1];
	work[1] = 0.0;
	mag = MAGNITUDE( work );
	if( mag > 1.0e-20 || mag < -1.0e-20 )  {
		f = 1.0/mag;
	}  else {
	  Tcl_AppendResult(interp, "tancir():  0-length vector!\n", (char *)NULL);
	  return;
	}
	VSCALE(work, work, f);
	temp = acos( work[0] );
	if( work[2] < 0.0 )
		temp = 6.28318512717958646 - temp;
	tempp = acos( (cir1[2] - cir2[2]) * f );
	ang = temp + tempp;
	angc = temp - tempp;
	if( (cir1[1] + cir1[2] * sin(ang)) >
	    (cir1[1] + cir1[2] * sin(angc)) )
		ang = angc;
	plano[0] = cir1[0] + cir1[2] * cos(ang);
	plano[1] = cir1[1] + cir1[2] * sin(ang);
	plant[0] = cir2[0] + cir2[2] * cos(ang);
	plant[1] = cir2[1] + cir2[2] * sin(ang);

	return;
}

void
slope( wh1, wh2 , t )
fastf_t wh1[], wh2[], t[];
{
	int i, j, switchs;
	fastf_t	temp;
	fastf_t	mag;
	fastf_t	z, r, b;
	vect_t	del, work;

	switchs = 0;
	if( wh1[2] < wh2[2] ) {
		switchs++;
		for(i=0; i<3; i++) {
			temp = wh1[i];
			wh1[i] = wh2[i];
			wh2[i] = temp;
		}
	}
	tancir(wh1, wh2);
	if( switchs ) {
		for(i=0; i<3; i++) {
			temp = wh1[i];
			wh1[i] = wh2[i];
			wh2[i] = temp;
		}
	}
	if(plano[1] <= plant[1]) {
		for(i=0; i<2; i++) {
			temp = plano[i];
			plano[i] = plant[i];
			plant[i] = temp;
		}
	}
	del[1] = 0.0;
	del[0] = plano[0] - plant[0];
	del[2] = plano[1] - plant[1];
	mag = MAGNITUDE( del );
	work[0] = -1.0 * t[2] * del[2] / mag;
	if( del[0] < 0.0 )
		work[0] *= -1.0;
	work[1] = 0.0;
	work[2] = t[2] * fabs(del[0]) / mag;
	b = (plano[1] - work[2]) - (del[2]/del[0]*(plano[0] - work[0]));
	z = wh1[1];
	r = wh1[2];
	if( wh1[1] >= wh2[1] ) {
		z = wh2[1];
		r = wh2[2];
	}
	record.s.s_values[2] = z - r - t[2];
	record.s.s_values[1] = t[0];
	record.s.s_values[0] = (record.s.s_values[2] - b) / (del[2] / del[0]);
	record.s.s_values[3] = plano[0] + (del[0]/mag) - work[0] - record.s.s_values[0];
	record.s.s_values[4] = 0.0;
	record.s.s_values[5] = plano[1] + (del[2]/mag) - work[2] - record.s.s_values[2];
	VADD2(&record.s.s_values[6], &record.s.s_values[3], work);
	VMOVE(&record.s.s_values[9], work);
	work[0] = work[2] = 0.0;
	work[1] = t[1] - t[0];
	VMOVE(&record.s.s_values[12], work);
	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&record.s.s_values[j], &record.s.s_values[i], work);
	}

	return;
}

void
crdummy( w, t, flag )
fastf_t	w[3], t[3];
int	flag;
{
	fastf_t	temp;
	vect_t	vec;
	int i, j;

	vec[1] = 0.0;
	if(plano[1] <= plant[1]) {
		for(i=0; i<2; i++) {
			temp = plano[i];
			plano[i] = plant[i];
			plant[i] = temp;
		}
	}

	vec[0] = w[2] + t[2] + 1.0;
	vec[2] = ( (plano[1] - w[1]) * vec[0] ) / (plano[0] - w[0]);
	if( flag > 1 )
		vec[0] *= -1.0;
	if(vec[2] >= 0.0)
		vec[2] *= -1.0;
	record.s.s_values[0] = w[0];
	record.s.s_values[1] = t[0] -1.0;
	record.s.s_values[2] = w[1];
	VMOVE(&record.s.s_values[3] , vec);
	vec[2] = w[2] + t[2] + 1.0;
	VMOVE(&record.s.s_values[6], vec);
	vec[0] = 0.0;
	VMOVE(&record.s.s_values[9], vec);
	vec[2] = 0.0;
	vec[1] = t[1] - t[0] + 2.0;
	VMOVE(&record.s.s_values[12], vec);
	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&record.s.s_values[j], &record.s.s_values[i], vec);
	}

	return;

}

void
trcurve( wh, t )
fastf_t wh[], t[];
{
	record.s.s_values[0] = wh[0];
	record.s.s_values[1] = t[0];
	record.s.s_values[2] = wh[1];
	record.s.s_values[4] = t[1] - t[0];
	record.s.s_values[6] = wh[2] + t[2];
	record.s.s_values[11] = wh[2] + t[2];
	VMOVE(&record.s.s_values[12], &record.s.s_values[6]);
	VMOVE(&record.s.s_values[15], &record.s.s_values[9]);
}

void
bottom( vec1, vec2, t )
vect_t	vec1, vec2;
fastf_t	t[];
{
	vect_t	tvec;
	int i, j;

	VMOVE(&record.s.s_values[0], vec1);
	tvec[0] = vec2[0] - vec1[0];
	tvec[1] = tvec[2] = 0.0;
	VMOVE(&record.s.s_values[3], tvec);
	tvec[0] = tvec[1] = 0.0;
	tvec[2] = t[2];
	VADD2(&record.s.s_values[6], &record.s.s_values[3], tvec);
	VMOVE(&record.s.s_values[9], tvec);
	tvec[0] = tvec[2] = 0.0;
	tvec[1] = t[1] - t[0];
	VMOVE(&record.s.s_values[12], tvec);

	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&record.s.s_values[j], &record.s.s_values[i], tvec);
	}
}

void
top( vec1, vec2, t )
vect_t	vec1, vec2;
fastf_t	t[];
{
	fastf_t	tooch, mag;
	vect_t	del, tvec;
	int i, j;

	tooch = t[2] * .25;
	del[0] = vec2[0] - vec1[0];
	del[1] = 0.0;
	del[2] = vec2[2] - vec1[2];
	mag = MAGNITUDE( del );
	VSCALE(tvec, del, tooch/mag);
	VSUB2(&record.s.s_values[0], vec1, tvec);
	VADD2(del, del, tvec);
	VADD2(&record.s.s_values[3], del, tvec);
	tvec[0] = tvec[2] = 0.0;
	tvec[1] = t[1] - t[0];
	VCROSS(del, tvec, &record.s.s_values[3]);
	mag = MAGNITUDE( del );
	if(del[2] < 0)
		mag *= -1.0;
	VSCALE(&record.s.s_values[9], del, t[2]/mag);
	VADD2(&record.s.s_values[6], &record.s.s_values[3], &record.s.s_values[9]);
	VMOVE(&record.s.s_values[12], tvec);

	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&record.s.s_values[j], &record.s.s_values[i], tvec);
	}
}

void
crregion( region, op, members, number, solidname )
char region[], op[], solidname[];
int members[], number;
{
  struct directory *dp;
  int i;

  for(i=0; i<number; i++) {
    solidname[8] = '\0';
    crname(solidname, members[i]);
    if( (dp = db_lookup( dbip, solidname, LOOKUP_QUIET)) == DIR_NULL ) {
      Tcl_AppendResult(interp, "region: ", region, " will skip member: ",
		       solidname, "\n", (char *)NULL);
      continue;
    }
    (void)combadd(dp, region, 1, op[i], 500+Trackpos+i, 0);
  }
}




/*	==== I T O A ( )
 *	convert integer to ascii  wd format
 */
void
itoa( n, s, w )
char	 s[];
int   n,    w;
{
	int	 c, i, j, sign;

	if( (sign = n) < 0 )	n = -n;
	i = 0;
	do	s[i++] = n % 10 + '0';	while( (n /= 10) > 0 );
	if( sign < 0 )	s[i++] = '-';

	/* blank fill array
	 */
	for( j = i; j < w; j++ )	s[j] = ' ';
	if( i > w )
	  Tcl_AppendResult(interp, "itoa: field length too small\n", (char *)NULL);
	s[w] = '\0';
	/* reverse the array
	 */
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

