/*                         T R A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup librt */

/*@{*/
/** @file track.c
 * f_amtrack():	Adds "tracks" to the data file given the required info
 *
 *  Author -
 *	Keith A. Applin
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *
 *
 * Acknowledgements:
 * Modifications by Bob Parker (SURVICE Engineering):
 *       *- adapt for use in LIBRT's database object
 *       *- removed prompting for input
 *       *- removed signal catching
 *       *- added basename parameter
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

/*XXX The following WDB_ defines need to go inside of a header file */
#define	WDB_TCL_CHECK_READ_ONLY \
	if (wdbp->dbip->dbi_read_only) {\
		Tcl_AppendResult(interp, "Sorry, this database is READ-ONLY\n", (char *)NULL); \
		return TCL_ERROR; \
	}
#define	WDB_TCL_ERROR_RECOVERY_SUGGESTION\
        Tcl_AppendResult(interp, "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety,\n\
you should exit now, and resolve the I/O problem, before continuing.\n", (char *)NULL)

extern void aexists();

static int Trackpos = 0;
static int mat_default = 1;
static int los_default = 50;
static int item_default = 500;
static fastf_t	plano[4], plant[4];
static int grpname_len;
static int extraTypeChars = 3;
static int extraChars = 4;

static struct track_solid {
  int s_type;
  char *s_name;
  fastf_t s_values[24];
} sol;

static int wrobj();
static void crname(), slope(), crdummy(), trcurve();
static void bottom(), top(), crregion(), itoa();

static void track_mk_tree_pure();
static int track_mk_tree_gift();
static struct wmember *track_mk_addmember();
static void track_mk_freemembers();
static int track_mk_comb();

/*
 *
 *	Adds track given "wheel" info
 *
 */
int
wdb_track_cmd(struct rt_wdb	*wdbp,
	      Tcl_Interp	*interp,
	      int		argc,
	      char 		**argv) {
  fastf_t fw[3], lw[3], iw[3], dw[3], tr[3];
  char *solname = NULL;
  char *regname = NULL;
  char *grpname = NULL;
  char oper[3];
  int i, memb[4];
  vect_t temp1, temp2;
  int item, mat, los;
  int arg;
  int edit_result = TCL_OK;
  struct bu_list head;

  WDB_TCL_CHECK_READ_ONLY;

  BU_LIST_INIT(&head);

  if (argc != 15) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib_alias wdb_track %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  oper[0] = oper[2] = WMOP_INTERSECT;
  oper[1] = WMOP_SUBTRACT;

  /* base name */
  arg = 1;
  grpname = bu_strdup(argv[arg]);
  grpname_len = strlen(grpname);
  solname = bu_malloc(grpname_len + 1 + extraChars, "solid name");
  regname = bu_malloc(grpname_len + 1 + extraChars, "region name");
  sol.s_name = bu_malloc(grpname_len + 1 + extraChars, "sol.s_name");

  /* first road wheel X */
  ++arg;
  fw[0] = atof(argv[arg]) * wdbp->dbip->dbi_local2base;

  /* last road wheel X */
  ++arg;
  lw[0] = atof(argv[arg]) * wdbp->dbip->dbi_local2base;

  if (fw[0] <= lw[0]) {
    Tcl_AppendResult(interp, "First wheel after last wheel - STOP\n", (char *)NULL);
    edit_result = TCL_ERROR;
    goto end;
  }

  /* road wheel Z */
  ++arg;
  fw[1] = lw[1] = atof(argv[arg]) * wdbp->dbip->dbi_local2base;

  /* roadwheel radius */
  ++arg;
  fw[2] = lw[2] = atof(argv[arg]) * wdbp->dbip->dbi_local2base;

  if( fw[2] <= 0 ) {
    Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
    edit_result = TCL_ERROR;
    goto end;
  }

  /* drive sprocket X */
  ++arg;
  dw[0] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  if( dw[0] >= lw[0] ) {
    Tcl_AppendResult(interp, "DRIVE wheel not in the rear - STOP \n", (char *)NULL);
    edit_result = TCL_ERROR;
    goto end;
  }

  /* drive sprocket Z */
  ++arg;
  dw[1] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  /* drive sprocket radius */
  ++arg;
  dw[2] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  if( dw[2] <= 0 ) {
    Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
    edit_result = TCL_ERROR;
    goto end;
  }

  /* idler wheel X */
  ++arg;
  iw[0] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  if( iw[0] <= fw[0] ) {
    Tcl_AppendResult(interp, "IDLER wheel not in the front - STOP \n", (char *)NULL);
    edit_result = TCL_ERROR;
    goto end;
  }

  /* idler wheel Z */
  ++arg;
  iw[1] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  /* idler wheel radius */
  ++arg;
  iw[2] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  if( iw[2] <= 0 ) {
    Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
    edit_result = TCL_ERROR;
    goto end;
  }

  /* track MIN Y */
  ++arg;
  tr[2] = tr[0] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  /* track MAX Y */
  ++arg;
  tr[1] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

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

  /* track thickness */
  ++arg;
  tr[2] = atof( argv[arg] ) * wdbp->dbip->dbi_local2base;

  if( tr[2] <= 0 ) {
    Tcl_AppendResult(interp, "Track thickness <= 0 - STOP\n", (char *)NULL);
    edit_result = TCL_ERROR;
    goto end;
  }

  for (i = 0; i < grpname_len; ++i) {
    solname[i] = grpname[i];
    regname[i] = grpname[i];
  }

  solname[i] = regname[i] = '.';
  ++i;
  solname[i] = 's';
  regname[i] = 'r';
  ++i;
  solname[i] = regname[i] = '.';
  ++i;
  solname[i] = regname[i] = '\0';
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
 *	grpname.s.0->9 and grpname.r.0->9
 */

  for (i=0; i<10; i++) {
    crname(interp, solname, i);
    crname(interp, regname, i);
    if ((db_lookup( wdbp->dbip, solname, LOOKUP_QUIET) != DIR_NULL) ||
       (db_lookup( wdbp->dbip, regname, LOOKUP_QUIET) != DIR_NULL)) {
      /* name already exists */
      Tcl_AppendResult(interp, "Track: naming error -- STOP\n",
		       (char *)NULL);
      edit_result = TCL_ERROR;
      goto end;
    }

    solname[grpname_len + extraTypeChars] = '\0';
    regname[grpname_len + extraTypeChars] = '\0';
  }

  /* find the front track slope to the idler */
  for(i=0; i<24; i++)
    sol.s_values[i] = 0.0;

  /* add the solids */
  /* solid 0 */
  slope(interp, fw, iw, tr);
  VMOVE(temp2, &sol.s_values[0]);
  crname(interp, solname, 0);
  (void)strcpy(sol.s_name, solname);
  sol.s_type = ID_ARB8;
  if (wrobj(wdbp, interp, solname, DIR_SOLID))
    return TCL_ERROR;

  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 1 */
  /* find track around idler */
  for(i=0; i<24; i++)
    sol.s_values[i] = 0.0;
  sol.s_type = ID_TGC;
  trcurve(iw, tr);
  crname(interp, solname, 1);
  (void)strcpy(sol.s_name, solname);
  if (wrobj(wdbp, interp, solname , DIR_SOLID ) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';
  /* idler dummy rcc */
  sol.s_values[6] = iw[2];
  sol.s_values[11] = iw[2];
  VMOVE(&sol.s_values[12], &sol.s_values[6]);
  VMOVE(&sol.s_values[15], &sol.s_values[9]);
  /* solid 2 */
  crname(interp, solname, 2);
  (void)strcpy(sol.s_name, solname);
  if (wrobj(wdbp, interp, solname , DIR_SOLID ) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 3 */
  /* find idler track dummy arb8 */
  for(i=0; i<24; i++)
    sol.s_values[i] = 0.0;
  crname(interp, solname, 3);
  (void)strcpy(sol.s_name, solname);
  sol.s_type = ID_ARB8;
  crdummy(iw, tr, 1);
  if (wrobj(wdbp, interp,solname,DIR_SOLID) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 4 */
  /* track slope to drive */
  for(i=0; i<24; i++)
    sol.s_values[i] = 0.0;
  slope(interp, lw, dw, tr);
  VMOVE(temp1, &sol.s_values[0]);
  crname(interp, solname, 4);
  (void)strcpy(sol.s_name, solname);
  if (wrobj(wdbp, interp,solname,DIR_SOLID))
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 5 */
  /* track around drive */
  for(i=0; i<24; i++)
    sol.s_values[i] = 0.0;
  sol.s_type = ID_TGC;
  trcurve(dw, tr);
  crname(interp, solname, 5);
  (void)strcpy(sol.s_name, solname);
  if (wrobj(wdbp, interp,solname,DIR_SOLID) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 6 */
  /* drive dummy rcc */
  sol.s_values[6] = dw[2];
  sol.s_values[11] = dw[2];
  VMOVE(&sol.s_values[12], &sol.s_values[6]);
  VMOVE(&sol.s_values[15], &sol.s_values[9]);
  crname(interp, solname, 6);
  (void)strcpy(sol.s_name, solname);
  if (wrobj(wdbp, interp,solname,DIR_SOLID) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 7 */
  /* drive dummy arb8 */
  for(i=0; i<24; i++)
    sol.s_values[i] = 0.0;
  crname(interp, solname, 7);
  (void)strcpy(sol.s_name, solname);
  sol.s_type = ID_ARB8;
  crdummy(dw, tr, 2);
  if (wrobj(wdbp, interp,solname,DIR_SOLID) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 8 */
  /* track bottom */
  temp1[1] = temp2[1] = tr[0];
  bottom(temp1, temp2, tr);
  crname(interp, solname, 8);
  (void)strcpy(sol.s_name, solname);
  if (wrobj(wdbp, interp,solname,DIR_SOLID) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* solid 9 */
  /* track top */
  temp1[0] = dw[0];
  temp1[1] = temp2[1] = tr[0];
  temp1[2] = dw[1] + dw[2];
  temp2[0] = iw[0];
  temp2[2] = iw[1] + iw[2];
  top(temp1, temp2, tr);
  crname(interp, solname, 9);
  (void)strcpy(sol.s_name, solname);
  if (wrobj(wdbp, interp,solname,DIR_SOLID) )
    return TCL_ERROR;
  solname[grpname_len + extraTypeChars] = '\0';

  /* add the regions */
  /* region 0 */
  item = item_default;
  mat = mat_default;
  los = los_default;
  item_default = 500;
  mat_default = 1;
  los_default = 50;
  /* region 1 */
  memb[0] = 0;
  memb[1] = 3;
  crname(interp, regname, 0);
  crregion(wdbp, interp, regname, oper, memb, 2, solname);
  solname[grpname_len + extraTypeChars] = '\0';
  regname[grpname_len + extraTypeChars] = '\0';

  /* region 1 */
  crname(interp, regname, 1);
  memb[0] = 1;
  memb[1] = 2;
  memb[2] = 3;
  crregion(wdbp, interp, regname, oper, memb, 3, solname);
  solname[grpname_len + extraTypeChars] = '\0';
  regname[grpname_len + extraTypeChars] = '\0';

  /* region 4 */
  crname(interp, regname, 4);
  memb[0] = 4;
  memb[1] = 7;
  crregion(wdbp, interp, regname, oper, memb, 2, solname);
  solname[grpname_len + extraTypeChars] = '\0';
  regname[grpname_len + extraTypeChars] = '\0';

  /* region 5 */
  crname(interp, regname, 5);
  memb[0] = 5;
  memb[1] = 6;
  memb[2] = 7;
  crregion(wdbp, interp, regname, oper, memb, 3, solname);
  solname[grpname_len + extraTypeChars] = '\0';
  regname[grpname_len + extraTypeChars] = '\0';

  /* region 8 */
  crname(interp, regname, 8);
  memb[0] = 8;
  memb[1] = 0;
  memb[2] = 4;
  oper[2] = WMOP_SUBTRACT;
  crregion(wdbp, interp, regname, oper, memb, 3, solname);
  solname[grpname_len + extraTypeChars] = '\0';
  regname[grpname_len + extraTypeChars] = '\0';

  /* region 9 */
  crname(interp, regname, 9);
  memb[0] = 9;
  memb[1] = 3;
  memb[2] = 7;
  crregion(wdbp, interp, regname, oper, memb, 3, solname);
  solname[grpname_len + extraTypeChars] = '\0';
  regname[grpname_len + extraTypeChars] = '\0';

  /* group all the track regions */
  for (i=0; i<10; i++) {
    if (i == 2 || i == 3 || i == 6 || i == 7)
      continue;
    regname[grpname_len + extraTypeChars] = '\0';
    crname(interp, regname, i);
    if (db_lookup( wdbp->dbip, regname, LOOKUP_QUIET) == DIR_NULL) {
      Tcl_AppendResult(interp, "group: ", grpname, " will skip member: ",
		       regname, "\n", (char *)NULL);
      continue;
    }
    track_mk_addmember(regname, &head, NULL, WMOP_UNION);
  }

  /* Add them all at once */
  if (track_mk_comb( wdbp, grpname, &head,
	       0, NULL, NULL, NULL,
	       0, 0, 0, 0,
	       0, 1, 1) < 0) {
    Tcl_AppendResult(interp,
		     "An error has occured while adding '",
		     grpname, "' to the database.\n", (char *)NULL);
  }

  Trackpos += 10;
  item_default = item;
  mat_default = mat;
  los_default = los;

  bu_free((genptr_t)solname, "solid name");
  bu_free((genptr_t)regname, "region name");
  bu_free((genptr_t)grpname, "group name");
  bu_free((genptr_t)sol.s_name, "sol.s_name");

  return edit_result;

end:
  bu_free((genptr_t)solname, "solid name");
  bu_free((genptr_t)regname, "region name");
  bu_free((genptr_t)grpname, "group name");
  bu_free((genptr_t)sol.s_name, "sol.s_name");

  return edit_result;
}

static void
crname(Tcl_Interp	*interp,
       char		name[],
       int		pos) {
  char temp[4];

  itoa(interp, pos, temp, 1);
  (void)strcat(name, temp);

  return;
}

static int
wrobj(struct rt_wdb	*wdbp,
      Tcl_Interp	*interp,
      char		name[],
      int		flags) {
	struct directory *tdp;
	struct rt_db_internal intern;
	int i;

	if(wdbp->dbip == DBI_NULL)
	  return 0;

	if( db_lookup( wdbp->dbip, name, LOOKUP_QUIET) != DIR_NULL ) {
	  Tcl_AppendResult(interp, "track naming error: ", name,
			   " already exists\n", (char *)NULL);
	  return(-1);
	}

	if( flags != DIR_SOLID )
	{
		Tcl_AppendResult(interp, "wrobj can only write solids, aborting\n" );
		return( -1 );
	}

	RT_INIT_DB_INTERNAL( &intern );
	switch( sol.s_type )
	{
		case ID_ARB8:
			{
				struct rt_arb_internal *arb;

				BU_GETSTRUCT( arb, rt_arb_internal );

				arb->magic = RT_ARB_INTERNAL_MAGIC;

				VMOVE( arb->pt[0], &sol.s_values[0] );
				for( i=1 ; i<8 ; i++ )
					VADD2( arb->pt[i], &sol.s_values[i*3], arb->pt[0] )

				intern.idb_ptr = (genptr_t)arb;
				intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
				intern.idb_type = ID_ARB8;
				intern.idb_meth = &rt_functab[ID_ARB8];
			}
			break;
		case ID_TGC:
			{
				struct rt_tgc_internal *tgc;

				BU_GETSTRUCT( tgc, rt_tgc_internal );

				tgc->magic = RT_TGC_INTERNAL_MAGIC;

				VMOVE( tgc->v, &sol.s_values[0] );
				VMOVE( tgc->h, &sol.s_values[3] );
				VMOVE( tgc->a, &sol.s_values[6] );
				VMOVE( tgc->b, &sol.s_values[9] );
				VMOVE( tgc->c, &sol.s_values[12] );
				VMOVE( tgc->d, &sol.s_values[15] );

				intern.idb_ptr = (genptr_t)tgc;
				intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
				intern.idb_type = ID_TGC;
				intern.idb_meth = &rt_functab[ID_TGC];
			}
			break;
		default:
			Tcl_AppendResult(interp, "Unrecognized solid type in 'wrobj', aborting\n", (char *)NULL );
			return( -1 );
	}

	if( (tdp = db_diradd( wdbp->dbip, name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL )
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		Tcl_AppendResult(interp, "Cannot add '", name, "' to directory, aborting\n", (char *)NULL );
		return( -1 );
	}

	if( rt_db_put_internal( tdp, wdbp->dbip, &intern, &rt_uniresource ) < 0 )
	{
		rt_db_free_internal( &intern, &rt_uniresource );
		Tcl_AppendResult(interp, "wrobj(wdbp, interp, ", name, "):  write error\n", (char *)NULL);
		WDB_TCL_ERROR_RECOVERY_SUGGESTION;
		return( -1 );
	}
	return(0);
}

static void
tancir(Tcl_Interp	*interp,
       register fastf_t cir1[],
       register fastf_t cir2[]) {
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

static void
slope(Tcl_Interp *interp,
      fastf_t wh1[],
      fastf_t wh2[],
      fastf_t t[]) {
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
	tancir(interp, wh1, wh2);
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
	sol.s_values[2] = z - r - t[2];
	sol.s_values[1] = t[0];
	sol.s_values[0] = (sol.s_values[2] - b) / (del[2] / del[0]);
	sol.s_values[3] = plano[0] + (del[0]/mag) - work[0] - sol.s_values[0];
	sol.s_values[4] = 0.0;
	sol.s_values[5] = plano[1] + (del[2]/mag) - work[2] - sol.s_values[2];
	VADD2(&sol.s_values[6], &sol.s_values[3], work);
	VMOVE(&sol.s_values[9], work);
	work[0] = work[2] = 0.0;
	work[1] = t[1] - t[0];
	VMOVE(&sol.s_values[12], work);
	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&sol.s_values[j], &sol.s_values[i], work);
	}

	return;
}

static void
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
	sol.s_values[0] = w[0];
	sol.s_values[1] = t[0] -1.0;
	sol.s_values[2] = w[1];
	VMOVE(&sol.s_values[3] , vec);
	vec[2] = w[2] + t[2] + 1.0;
	VMOVE(&sol.s_values[6], vec);
	vec[0] = 0.0;
	VMOVE(&sol.s_values[9], vec);
	vec[2] = 0.0;
	vec[1] = t[1] - t[0] + 2.0;
	VMOVE(&sol.s_values[12], vec);
	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&sol.s_values[j], &sol.s_values[i], vec);
	}

	return;

}

static void
trcurve( wh, t )
fastf_t wh[], t[];
{
	sol.s_values[0] = wh[0];
	sol.s_values[1] = t[0];
	sol.s_values[2] = wh[1];
	sol.s_values[4] = t[1] - t[0];
	sol.s_values[6] = wh[2] + t[2];
	sol.s_values[11] = wh[2] + t[2];
	VMOVE(&sol.s_values[12], &sol.s_values[6]);
	VMOVE(&sol.s_values[15], &sol.s_values[9]);
}

static void
bottom( vec1, vec2, t )
vect_t	vec1, vec2;
fastf_t	t[];
{
	vect_t	tvec;
	int i, j;

	VMOVE(&sol.s_values[0], vec1);
	tvec[0] = vec2[0] - vec1[0];
	tvec[1] = tvec[2] = 0.0;
	VMOVE(&sol.s_values[3], tvec);
	tvec[0] = tvec[1] = 0.0;
	tvec[2] = t[2];
	VADD2(&sol.s_values[6], &sol.s_values[3], tvec);
	VMOVE(&sol.s_values[9], tvec);
	tvec[0] = tvec[2] = 0.0;
	tvec[1] = t[1] - t[0];
	VMOVE(&sol.s_values[12], tvec);

	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&sol.s_values[j], &sol.s_values[i], tvec);
	}
}

static void
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
	VSUB2(&sol.s_values[0], vec1, tvec);
	VADD2(del, del, tvec);
	VADD2(&sol.s_values[3], del, tvec);
	tvec[0] = tvec[2] = 0.0;
	tvec[1] = t[1] - t[0];
	VCROSS(del, tvec, &sol.s_values[3]);
	mag = MAGNITUDE( del );
	if(del[2] < 0)
		mag *= -1.0;
	VSCALE(&sol.s_values[9], del, t[2]/mag);
	VADD2(&sol.s_values[6], &sol.s_values[3], &sol.s_values[9]);
	VMOVE(&sol.s_values[12], tvec);

	for(i=3; i<=9; i+=3) {
		j = i + 12;
		VADD2(&sol.s_values[j], &sol.s_values[i], tvec);
	}
}

static void
crregion(struct rt_wdb	*wdbp,
	 Tcl_Interp	*interp,
	 char region[],
	 char op[],
	 int members[],
	 int number,
	 char solidname[]) {
  int i;
  struct bu_list head;

  if(wdbp->dbip == DBI_NULL)
    return;

  BU_LIST_INIT(&head);

  for(i=0; i<number; i++) {
    solidname[grpname_len + extraTypeChars] = '\0';
    crname(interp, solidname, members[i]);
    if( db_lookup( wdbp->dbip, solidname, LOOKUP_QUIET) == DIR_NULL ) {
      Tcl_AppendResult(interp, "region: ", region, " will skip member: ",
		       solidname, "\n", (char *)NULL);
      continue;
    }
    track_mk_addmember( solidname, &head, NULL, op[i] );
  }
  (void)track_mk_comb( wdbp, region, &head,
	    1, NULL, NULL, NULL,
	    500+Trackpos+i, 0, mat_default, los_default,
	    0, 1, 1 );
}




/*	==== I T O A ( )
 *	convert integer to ascii  wd format
 */
static void
itoa(Tcl_Interp *interp,
     int n,
     char s[],
     int w) {
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

/*
 * The following functions were pulled in from libwdb
 * to prevent creating a new dependency.
 */

/*
 *			M K _ T R E E _ P U R E
 *
 *  Given a list of wmember structures, build a tree that performs
 *  the boolean operations in the given sequence.
 *  No GIFT semantics or precedence is provided.
 *  For that, use mk_tree_gift().
 */
static void
track_mk_tree_pure( struct rt_comb_internal *comb, struct bu_list *member_hd )
{
	register struct wmember *wp;

	for( BU_LIST_FOR( wp, wmember, member_hd ) )  {
		union tree	*leafp, *nodep;

		WDB_CK_WMEMBER(wp);

		BU_GETUNION( leafp, tree );
		leafp->tr_l.magic = RT_TREE_MAGIC;
		leafp->tr_l.tl_op = OP_DB_LEAF;
		leafp->tr_l.tl_name = bu_strdup( wp->wm_name );
		if( !bn_mat_is_identity( wp->wm_mat ) )  {
			leafp->tr_l.tl_mat = bn_mat_dup( wp->wm_mat );
		}

		if( !comb->tree )  {
			comb->tree = leafp;
			continue;
		}
		/* Build a left-heavy tree */
		BU_GETUNION( nodep, tree );
		nodep->tr_b.magic = RT_TREE_MAGIC;
		switch( wp->wm_op )  {
		case WMOP_UNION:
			nodep->tr_b.tb_op = OP_UNION;
			break;
		case WMOP_INTERSECT:
			nodep->tr_b.tb_op = OP_INTERSECT;
			break;
		case WMOP_SUBTRACT:
			nodep->tr_b.tb_op = OP_SUBTRACT;
			break;
		default:
			bu_bomb("track_mk_tree_pure() bad wm_op");
		}
		nodep->tr_b.tb_left = comb->tree;
		nodep->tr_b.tb_right = leafp;
		comb->tree = nodep;
	}
}

/*
 *			M K _ T R E E _ G I F T
 *
 *  Add some nodes to a new or existing combination's tree,
 *  with GIFT precedence and semantics.
 *
 *  NON-PARALLEL due to rt_uniresource
 *
 *  Returns -
 *	-1	ERROR
 *	0	OK
 */
static int
track_mk_tree_gift( struct rt_comb_internal *comb, struct bu_list *member_hd )
{
	struct wmember *wp;
	union tree *tp;
	struct rt_tree_array *tree_list;
	int node_count;
	int actual_count;
	int new_nodes;

	if( (new_nodes = bu_list_len( member_hd )) <= 0 )
		return 0;	/* OK, nothing to do */

	if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 )
	{
		db_non_union_push( comb->tree, &rt_uniresource );
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
		{
			bu_log("track_mk_tree_gift() Cannot flatten tree for editing\n");
			return -1;
		}
	}

	/* make space for an extra leaf */
	node_count = db_tree_nleaves( comb->tree ) ;
	tree_list = (struct rt_tree_array *)bu_calloc( node_count + new_nodes,
		sizeof( struct rt_tree_array ), "tree list" );

	/* flatten tree */
	if( comb->tree )  {
		/* Release storage for non-leaf nodes, steal leaves */
		actual_count = (struct rt_tree_array *)db_flatten_tree(
			tree_list, comb->tree, OP_UNION,
			1, &rt_uniresource ) - tree_list;
		BU_ASSERT_LONG( actual_count, ==, node_count );
		comb->tree = TREE_NULL;
	} else {
		actual_count = 0;
	}

	/* Add new members to the array */
	for( BU_LIST_FOR( wp, wmember, member_hd ) )  {
		WDB_CK_WMEMBER(wp);

		switch( wp->wm_op )  {
			case WMOP_INTERSECT:
				tree_list[node_count].tl_op = OP_INTERSECT;
				break;
			case WMOP_SUBTRACT:
				tree_list[node_count].tl_op = OP_SUBTRACT;
				break;
			default:
				bu_log("track_mk_tree_gift() unrecognized relation %c (assuming UNION)\n", wp->wm_op);
				/* Fall through */
			case WMOP_UNION:
				tree_list[node_count].tl_op = OP_UNION;
				break;
		}

		/* make new leaf node, and insert at end of array */
		BU_GETUNION( tp, tree );
		tree_list[node_count++].tl_tree = tp;
		tp->tr_l.magic = RT_TREE_MAGIC;
		tp->tr_l.tl_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup( wp->wm_name );
		if( !bn_mat_is_identity( wp->wm_mat ) )  {
			tp->tr_l.tl_mat = bn_mat_dup( wp->wm_mat );
		} else {
			tp->tr_l.tl_mat = (matp_t)NULL;
		}
	}
	BU_ASSERT_LONG( node_count, ==, actual_count + new_nodes );

	/* rebuild the tree with GIFT semantics */
	comb->tree = (union tree *)db_mkgift_tree( tree_list, node_count, &rt_uniresource );

	bu_free( (char *)tree_list, "track_mk_tree_gift: tree_list" );

	return 0;	/* OK */
}

/*
 *			M K _ A D D M E M B E R
 *
 *  Obtain dynamic storage for a new wmember structure, fill in the
 *  name, default the operation and matrix, and add to doubly linked
 *  list.  In typical use, a one-line call is sufficient.  To change
 *  the defaults, catch the pointer that is returned, and adjust the
 *  structure to taste.
 *
 *  The caller is responsible for initializing the header structures
 *  forward and backward links.
 */
static struct wmember *
track_mk_addmember(
	const char	*name,
	struct bu_list	*headp,
	mat_t mat,
	int		op)
{
	register struct wmember *wp;

	BU_GETSTRUCT( wp, wmember );
	wp->l.magic = WMEMBER_MAGIC;
	wp->wm_name = bu_strdup( name );
	switch( op )  {
	case WMOP_UNION:
	case WMOP_INTERSECT:
	case WMOP_SUBTRACT:
		wp->wm_op = op;
		break;
	default:
		bu_log("mk_addmember() op=x%x is bad\n", op);
		return(WMEMBER_NULL);
	}

	/* if the user gave a matrix, use it.  otherwise use identity matrix*/
	if (mat) {
		MAT_COPY( wp->wm_mat, mat );
	} else {
		MAT_IDN( wp->wm_mat );
	}

	/* Append to end of doubly linked list */
	BU_LIST_INSERT( headp, &wp->l );
	return(wp);
}

/*
 *			M K _ F R E E M E M B E R S
 */
static void
track_mk_freemembers( struct bu_list *headp )
{
	register struct wmember *wp;

	while( BU_LIST_WHILE( wp, wmember, headp ) )  {
		WDB_CK_WMEMBER(wp);
		BU_LIST_DEQUEUE( &wp->l );
		bu_free( (char *)wp->wm_name, "wm_name" );
		bu_free( (char *)wp, "wmember" );
	}
}

/*
 *			M K _ C O M B
 *
 *  Make a combination, where the
 *  members are described by a linked list of wmember structs.
 *
 *  The linked list is freed when it has been output.
 *
 *  Has many operating modes.
 *
 *  Returns -
 *	-1	ERROR
 *	0	OK
 */
static int
track_mk_comb(
	struct rt_wdb		*wdbp,
	const char		*combname,
	struct bu_list		*headp,		/* Made by mk_addmember() */
	int			region_kind,	/* 1 => region.  'P' and 'V' for FASTGEN */
	const char		*shadername,	/* shader name, or NULL */
	const char		*shaderargs,	/* shader args, or NULL */
	const unsigned char	*rgb,		/* NULL => no color */
	int			id,		/* region_id */
	int			air,		/* aircode */
	int			material,	/* GIFTmater */
	int			los,
	int			inherit,
	int			append_ok,	/* 0 = obj must not exit */
	int			gift_semantics)	/* 0 = pure, 1 = gift */
{
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;
	int fresh_combination;

	RT_CK_WDB(wdbp);

	RT_INIT_DB_INTERNAL(&intern);

	if( append_ok &&
	    wdb_import( wdbp, &intern, combname, (matp_t)NULL ) >= 0 )  {
	    	/* We retrieved an existing object, append to it */
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB( comb );

	    	fresh_combination = 0;
	} else {
		/* Create a fresh new object for export */
		BU_GETSTRUCT( comb, rt_comb_internal );
		comb->magic = RT_COMB_MAGIC;
		bu_vls_init( &comb->shader );
		bu_vls_init( &comb->material );

		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_type = ID_COMBINATION;
		intern.idb_ptr = (genptr_t)comb;
		intern.idb_meth = &rt_functab[ID_COMBINATION];

		fresh_combination = 1;
	}

	if( gift_semantics )
		track_mk_tree_gift( comb, headp );
	else
		track_mk_tree_pure( comb, headp );

	/* Release the wmember list dynamic storage */
	track_mk_freemembers( headp );

	/* Don't change these things when appending to existing combination */
	if( fresh_combination )  {
		if( region_kind )  {
			comb->region_flag = 1;
			switch( region_kind )  {
			case 'P':
				comb->is_fastgen = REGION_FASTGEN_PLATE;
				break;
			case 'V':
				comb->is_fastgen = REGION_FASTGEN_VOLUME;
				break;
			case 'R':
			case 1:
				/* Regular non-FASTGEN Region */
				break;
			default:
				bu_log("mk_comb(%s) unknown region_kind=%d (%c), assuming normal non-FASTGEN\n",
					combname, region_kind, region_kind);
			}
		}
		if( shadername )  bu_vls_strcat( &comb->shader, shadername );
		if( shaderargs )  {
			bu_vls_strcat( &comb->shader, " " );
			bu_vls_strcat( &comb->shader, shaderargs );
			/* Convert to Tcl form if necessary.  Use heuristics */
			if( strchr( shaderargs, '=' ) != NULL &&
			    strchr( shaderargs, '{' ) == NULL )
			{
				struct bu_vls old;
				bu_vls_init(&old);
				bu_vls_vlscatzap(&old, &comb->shader);
				if( bu_shader_to_tcl_list( bu_vls_addr(&old), &comb->shader) )
					bu_log("Unable to convert shader string '%s %s'\n", shadername, shaderargs);
				bu_vls_free(&old);
			}
		}

		if( rgb )  {
			comb->rgb_valid = 1;
			comb->rgb[0] = rgb[0];
			comb->rgb[1] = rgb[1];
			comb->rgb[2] = rgb[2];
		}

		comb->region_id = id;
		comb->aircode = air;
		comb->GIFTmater = material;
		comb->los = los;

		comb->inherit = inherit;
	}

	/* The internal representation will be freed */
	return wdb_put_internal( wdbp, combname, &intern, 1.0);
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
