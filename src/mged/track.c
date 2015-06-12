/*                         T R A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file mged/track.c
 *
 * f_amtrack():	Adds "tracks" to the data file given the required info
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"
#include "rt/db4.h"

#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"


static int Trackpos = 0;
static fastf_t plano[4], plant[4];

static struct track_solid
{
    int s_type;
    char s_name[NAMESIZE+1];
    fastf_t s_values[24];
} sol;

void crname(char *name, int pos, int maxlen);
void slope(fastf_t *wh1, fastf_t *wh2, fastf_t *t);
void crdummy(fastf_t *w, fastf_t *t, int flag);
void trcurve(fastf_t *wh, fastf_t *t);
void bottom(fastf_t *vec1, fastf_t *vec2, fastf_t *t);
void top(fastf_t *vec1, fastf_t *vec2, fastf_t *t);
void crregion(char *region, char *op, int *members, int number, char *solidname, int maxlen);
void itoa(int n, char *s, int w);


/*
 * adds track given "wheel" info
 */
int
f_amtrack(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{

    fastf_t fw[3], lw[3], iw[3], dw[3], tr[3];
    char solname[12], regname[12], grpname[9], oper[3];
    int i, j, memb[4];
    char temp[4];
    vect_t temp1, temp2;
    int item, mat, los;
    int arg;
    int edit_result;
    struct bu_list head;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    BU_LIST_INIT(&head);

    if (argc < 1 || 27 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help track");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* interrupts */
    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);  /* allow interrupts */
    else
	return TCL_OK;

    oper[0] = oper[2] = WMOP_INTERSECT;
    oper[1] = WMOP_SUBTRACT;

    arg = 1;

    /* get the roadwheel info */
    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the FIRST roadwheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    fw[0] = atof(argv[arg]) * local2base;
    ++arg;

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the LAST roadwheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    lw[0] = atof(argv[arg]) * local2base;
    ++arg;

    if (fw[0] <= lw[0]) {
	Tcl_AppendResult(interp, "First wheel after last wheel - STOP\n", (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Z of the roadwheels: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    fw[1] = lw[1] = atof(argv[arg]) * local2base;
    ++arg;

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter radius of the roadwheels: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    fw[2] = lw[2] = atof(argv[arg]) * local2base;
    ++arg;
    if (fw[2] <= 0) {
	Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }

    if (argc < arg+1) {
	/* get the drive wheel info */
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the drive (REAR) wheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    dw[0] = atof(argv[arg]) * local2base;
    ++arg;
    if (dw[0] >= lw[0]) {
	Tcl_AppendResult(interp, "DRIVE wheel not in the rear - STOP \n", (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Z of the drive (REAR) wheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    dw[1] = atof(argv[arg]) * local2base;
    ++arg;

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter radius of the drive (REAR) wheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    dw[2] = atof(argv[arg]) * local2base;
    ++arg;
    if (dw[2] <= 0) {
	Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }

    /* get the idler wheel info */
    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter X of the idler (FRONT) wheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    iw[0] = atof(argv[arg]) * local2base;
    ++arg;
    if (iw[0] <= fw[0]) {
	Tcl_AppendResult(interp, "IDLER wheel not in the front - STOP \n", (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Z of the idler (FRONT) wheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    iw[1] = atof(argv[arg]) * local2base;
    ++arg;

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter radius of the idler (FRONT) wheel: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    iw[2] = atof(argv[arg]) * local2base;
    ++arg;
    if (iw[2] <= 0) {
	Tcl_AppendResult(interp, "Radius <= 0 - STOP\n", (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }

    /* get track info */
    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Y-MIN of the track: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    tr[2] = tr[0] = atof(argv[arg]) * local2base;
    ++arg;

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter Y-MAX of the track: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    tr[1] = atof(argv[arg]) * local2base;
    ++arg;
    if (EQUAL(tr[0], tr[1])) {
	Tcl_AppendResult(interp, "MIN == MAX ... STOP\n", (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    if (tr[0] > tr[1] - SMALL_FASTF) {
	Tcl_AppendResult(interp, "MIN > MAX .... will switch\n", (char *)NULL);
	tr[1] = tr[0];
	tr[0] = tr[2];
    }

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter track thickness: ",
			 (char *)NULL);
	edit_result = TCL_ERROR;
	goto end;
    }
    tr[2] = atof(argv[arg]) * local2base;
    ++arg;
    if (tr[2] <= 0) {
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
  bu_log("\nX of first road wheel  %10.4f\n", fw[0]);
  bu_log("X of last road wheel   %10.4f\n", lw[0]);
  bu_log("Z of road wheels       %10.4f\n", fw[1]);
  bu_log("radius of road wheels  %10.4f\n", fw[2]);
  bu_log("\nX of drive wheel       %10.4f\n", dw[0]);
  bu_log("Z of drive wheel       %10.4f\n", dw[1]);
  bu_log("radius of drive wheel  %10.4f\n", dw[2]);
  bu_log("\nX of idler wheel       %10.4f\n", iw[0]);
  bu_log("Z of idler wheel       %10.4f\n", iw[1]);
  bu_log("radius of idler wheel  %10.4f\n", iw[2]);
  bu_log("\nY MIN of track         %10.4f\n", tr[0]);
  bu_log("Y MAX of track         %10.4f\n", tr[1]);
  bu_log("thickness of track     %10.4f\n", tr[2]);
*/

/* Check for names to use:
 * 1.  start with track.s.1->10 and track.r.1->10
 * 2.  if bad, increment count by 10 and try again
 */

 tryagain:	/* sent here to try next set of names */

    for (i=0; i<11; i++) {
	crname(solname, i, sizeof(solname));
	crname(regname, i, sizeof(regname));
	if ((db_lookup(dbip, solname, LOOKUP_QUIET) != RT_DIR_NULL)	||
	    (db_lookup(dbip, regname, LOOKUP_QUIET) != RT_DIR_NULL)) {
	    /* name already exists */
	    solname[8] = regname[8] = '\0';
	    if ((Trackpos += 10) > 500) {
		Tcl_AppendResult(interp, "Track: naming error -- STOP\n",
				 (char *)NULL);
		edit_result = TCL_ERROR;
		goto end;
	    }
	    goto tryagain;
	}
	solname[8] = regname[8] = '\0';
    }

    /* no interrupts */
    (void)signal(SIGINT, SIG_IGN);

    /* find the front track slope to the idler */
    for (i=0; i<24; i++)
	sol.s_values[i] = 0.0;

    slope(fw, iw, tr);
    VMOVE(temp2, &sol.s_values[0]);
    crname(solname, 1, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    sol.s_type = ID_ARB8;
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;

    solname[8] = '\0';

    /* find track around idler */
    for (i=0; i<24; i++)
	sol.s_values[i] = 0.0;
    sol.s_type = ID_TGC;
    trcurve(iw, tr);
    crname(solname, 2, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';
    /* idler dummy rcc */
    sol.s_values[6] = iw[2];
    sol.s_values[11] = iw[2];
    VMOVE(&sol.s_values[12], &sol.s_values[6]);
    VMOVE(&sol.s_values[15], &sol.s_values[9]);
    crname(solname, 3, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* find idler track dummy arb8 */
    for (i=0; i<24; i++)
	sol.s_values[i] = 0.0;
    crname(solname, 4, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    sol.s_type = ID_ARB8;
    crdummy(iw, tr, 1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* track slope to drive */
    for (i=0; i<24; i++)
	sol.s_values[i] = 0.0;
    slope(lw, dw, tr);
    VMOVE(temp1, &sol.s_values[0]);
    crname(solname, 5, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* track around drive */
    for (i=0; i<24; i++)
	sol.s_values[i] = 0.0;
    sol.s_type = ID_TGC;
    trcurve(dw, tr);
    crname(solname, 6, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* drive dummy rcc */
    sol.s_values[6] = dw[2];
    sol.s_values[11] = dw[2];
    VMOVE(&sol.s_values[12], &sol.s_values[6]);
    VMOVE(&sol.s_values[15], &sol.s_values[9]);
    crname(solname, 7, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* drive dummy arb8 */
    for (i=0; i<24; i++)
	sol.s_values[i] = 0.0;
    crname(solname, 8, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    sol.s_type = ID_ARB8;
    crdummy(dw, tr, 2);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* track bottom */
    temp1[1] = temp2[1] = tr[0];
    bottom(temp1, temp2, tr);
    crname(solname, 9, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* track top */
    temp1[0] = dw[0];
    temp1[1] = temp2[1] = tr[0];
    temp1[2] = dw[1] + dw[2];
    temp2[0] = iw[0];
    temp2[2] = iw[1] + iw[2];
    top(temp1, temp2, tr);
    crname(solname, 10, sizeof(solname));
    bu_strlcpy(sol.s_name, solname, NAMESIZE+1);
    if (wrobj(solname, RT_DIR_SOLID))
	return TCL_ERROR;
    solname[8] = '\0';

    /* add the regions */
    item = item_default;
    mat = mat_default;
    los = los_default;
    item_default = 500;
    mat_default = 1;
    los_default = 50;
    /* region 1 */
    memb[0] = 1;
    memb[1] = 4;
    crname(regname, 1, sizeof(regname));
    crregion(regname, oper, memb, 2, solname, sizeof(regname));
    solname[8] = regname[8] = '\0';

    /* region 2 */
    crname(regname, 2, sizeof(regname));
    memb[0] = 2;
    memb[1] = 3;
    memb[2] = 4;
    crregion(regname, oper, memb, 3, solname, sizeof(regname));
    solname[8] = regname[8] = '\0';

    /* region 5 */
    crname(regname, 5, sizeof(regname));
    memb[0] = 5;
    memb[1] = 8;
    crregion(regname, oper, memb, 2, solname, sizeof(regname));
    solname[8] = regname[8] = '\0';

    /* region 6 */
    crname(regname, 6, sizeof(regname));
    memb[0] = 6;
    memb[1] = 7;
    memb[2] = 8;
    crregion(regname, oper, memb, 3, solname, sizeof(regname));
    solname[8] = regname[8] = '\0';

    /* region 9 */
    crname(regname, 9, sizeof(regname));
    memb[0] = 9;
    memb[1] = 1;
    memb[2] = 5;
    oper[2] = WMOP_SUBTRACT;
    crregion(regname, oper, memb, 3, solname, sizeof(regname));
    solname[8] = regname[8] = '\0';

    /* region 10 */
    crname(regname, 10, sizeof(regname));
    memb[0] = 10;
    memb[1] = 4;
    memb[2] = 8;
    crregion(regname, oper, memb, 3, solname, sizeof(regname));
    solname[8] = regname[8] = '\0';

    /* group all the track regions */
    j = 1;
    if ((i = Trackpos / 10 + 1) > 9)
	j = 2;
    itoa(i, temp, j);
    bu_strlcat(grpname, temp, sizeof(grpname));
    for (i=1; i<11; i++) {
	if (i == 3 || i ==4 || i == 7 || i == 8)
	    continue;
	regname[8] = '\0';
	crname(regname, i, sizeof(regname));
	if (db_lookup(dbip, regname, LOOKUP_QUIET) == RT_DIR_NULL) {
	    Tcl_AppendResult(interp, "group: ", grpname, " will skip member: ",
			     regname, "\n", (char *)NULL);
	    continue;
	}
	mk_addmember(regname, &head, NULL, WMOP_UNION);
    }

    /* Add them all at once */
    if (mk_comb(wdbp, grpname, &head,
		0, NULL, NULL, NULL,
		0, 0, 0, 0,
		0, 1, 1) < 0)
    {
	Tcl_AppendResult(interp,
			 "An error has occurred while adding '",
			 grpname, "' to the database.\n", (char *)NULL);
    }

    /* draw this track */
    Tcl_AppendResult(interp, "The track regions are in group ", grpname,
		     "\n", (char *)NULL);
    {
	const char *arglist[3];
	arglist[0] = "e";
	arglist[1] = grpname;
	arglist[2] = NULL;
	edit_result = cmd_draw(clientData, interp, 2, arglist);
    }

    Trackpos += 10;
    item_default = item;
    mat_default = mat;
    los_default = los;
    grpname[5] = solname[8] = regname[8] = '\0';

    return edit_result;
 end:
    (void)signal(SIGINT, SIG_IGN);
    return edit_result;
}


void
crname(char *name, int pos, int maxlen)
{
    int i, j;
    char temp[4];

    j=1;
    if ((i = Trackpos + pos) > 9)
	j = 2;
    if (i > 99)
	j = 3;
    itoa(i, temp, j);
    bu_strlcat(name, temp, maxlen);
    return;
}


int
wrobj(char name[], int flags)
{
    struct directory *tdp;
    struct rt_db_internal intern;
    int i;

    if (dbip == DBI_NULL)
	return 0;

    if (db_lookup(dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(INTERP, "track naming error: ", name,
			 " already exists\n", (char *)NULL);
	return -1;
    }

    if (flags != RT_DIR_SOLID) {
	Tcl_AppendResult(INTERP, "wrobj can only write solids, aborting\n");
	return -1;
    }

    RT_DB_INTERNAL_INIT(&intern);
    switch (sol.s_type) {
	case ID_ARB8:
	    {
		struct rt_arb_internal *arb;

		BU_ALLOC(arb, struct rt_arb_internal);

		arb->magic = RT_ARB_INTERNAL_MAGIC;

		VMOVE(arb->pt[0], &sol.s_values[0]);
		for (i=1; i<8; i++)
		    VADD2(arb->pt[i], &sol.s_values[i*3], arb->pt[0]);

		intern.idb_ptr = (void *)arb;
		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_type = ID_ARB8;
		intern.idb_meth = &OBJ[ID_ARB8];
	    }
	    break;
	case ID_TGC:
	    {
		struct rt_tgc_internal *tgc;

		BU_ALLOC(tgc, struct rt_tgc_internal);

		tgc->magic = RT_TGC_INTERNAL_MAGIC;

		VMOVE(tgc->v, &sol.s_values[0]);
		VMOVE(tgc->h, &sol.s_values[3]);
		VMOVE(tgc->a, &sol.s_values[6]);
		VMOVE(tgc->b, &sol.s_values[9]);
		VMOVE(tgc->c, &sol.s_values[12]);
		VMOVE(tgc->d, &sol.s_values[15]);

		intern.idb_ptr = (void *)tgc;
		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_type = ID_TGC;
		intern.idb_meth = &OBJ[ID_TGC];
	    }
	    break;
	default:
	    Tcl_AppendResult(INTERP, "Unrecognized solid type in 'wrobj', aborting\n", (char *)NULL);
	    return -1;
    }

    if ((tdp = db_diradd(dbip, name, -1L, 0, flags, (void *)&intern.idb_type)) == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	Tcl_AppendResult(INTERP, "Cannot add '", name, "' to directory, aborting\n", (char *)NULL);
	return -1;
    }

    if (rt_db_put_internal(tdp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	Tcl_AppendResult(INTERP, "wrobj(", name, "):  write error\n", (char *)NULL);
	TCL_ERROR_RECOVERY_SUGGESTION;
	return -1;
    }
    return 0;
}


void
tancir(fastf_t *cir1, fastf_t *cir2)
{
    static fastf_t mag;
    vect_t work;
    fastf_t f;
    static fastf_t temp, tempp, ang, angc;

    work[0] = cir2[0] - cir1[0];
    work[2] = cir2[1] - cir1[1];
    work[1] = 0.0;
    mag = MAGNITUDE(work);
    if (mag > 1.0e-20 || mag < -1.0e-20) {
	f = 1.0/mag;
    } else {
	Tcl_AppendResult(INTERP, "tancir():  0-length vector!\n", (char *)NULL);
	return;
    }
    VSCALE(work, work, f);
    temp = acos(work[0]);
    if (work[2] < 0.0)
	temp = 6.28318512717958646 - temp;
    tempp = acos((cir1[2] - cir2[2]) * f);
    ang = temp + tempp;
    angc = temp - tempp;
    if ((cir1[1] + cir1[2] * sin(ang)) >
	(cir1[1] + cir1[2] * sin(angc)))
	ang = angc;
    plano[0] = cir1[0] + cir1[2] * cos(ang);
    plano[1] = cir1[1] + cir1[2] * sin(ang);
    plant[0] = cir2[0] + cir2[2] * cos(ang);
    plant[1] = cir2[1] + cir2[2] * sin(ang);

    return;
}


void
slope(fastf_t *wh1, fastf_t *wh2, fastf_t *t)
{
    int i, j, switchs;
    fastf_t temp;
    fastf_t mag;
    fastf_t z, r, b;
    vect_t del, work;

    switchs = 0;
    if (wh1[2] < wh2[2]) {
	switchs++;
	for (i=0; i<3; i++) {
	    temp = wh1[i];
	    wh1[i] = wh2[i];
	    wh2[i] = temp;
	}
    }
    tancir(wh1, wh2);
    if (switchs) {
	for (i=0; i<3; i++) {
	    temp = wh1[i];
	    wh1[i] = wh2[i];
	    wh2[i] = temp;
	}
    }
    if (plano[1] <= plant[1]) {
	for (i=0; i<2; i++) {
	    temp = plano[i];
	    plano[i] = plant[i];
	    plant[i] = temp;
	}
    }
    del[1] = 0.0;
    del[0] = plano[0] - plant[0];
    del[2] = plano[1] - plant[1];
    mag = MAGNITUDE(del);
    work[0] = -1.0 * t[2] * del[2] / mag;
    if (del[0] < 0.0)
	work[0] *= -1.0;
    work[1] = 0.0;
    work[2] = t[2] * fabs(del[0]) / mag;
    b = (plano[1] - work[2]) - (del[2]/del[0]*(plano[0] - work[0]));
    z = wh1[1];
    r = wh1[2];
    if (wh1[1] >= wh2[1]) {
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
    for (i=3; i<=9; i+=3) {
	j = i + 12;
	VADD2(&sol.s_values[j], &sol.s_values[i], work);
    }

    return;
}


void
crdummy(fastf_t *w, fastf_t *t, int flag)
{
    fastf_t temp;
    vect_t vec;
    int i, j;

    vec[1] = 0.0;
    if (plano[1] <= plant[1]) {
	for (i=0; i<2; i++) {
	    temp = plano[i];
	    plano[i] = plant[i];
	    plant[i] = temp;
	}
    }

    vec[0] = w[2] + t[2] + 1.0;
    vec[2] = ((plano[1] - w[1]) * vec[0]) / (plano[0] - w[0]);
    if (flag > 1)
	vec[0] *= -1.0;
    if (vec[2] >= 0.0)
	vec[2] *= -1.0;
    sol.s_values[0] = w[0];
    sol.s_values[1] = t[0] -1.0;
    sol.s_values[2] = w[1];
    VMOVE(&sol.s_values[3], vec);
    vec[2] = w[2] + t[2] + 1.0;
    VMOVE(&sol.s_values[6], vec);
    vec[0] = 0.0;
    VMOVE(&sol.s_values[9], vec);
    vec[2] = 0.0;
    vec[1] = t[1] - t[0] + 2.0;
    VMOVE(&sol.s_values[12], vec);
    for (i=3; i<=9; i+=3) {
	j = i + 12;
	VADD2(&sol.s_values[j], &sol.s_values[i], vec);
    }

    return;

}


void
trcurve(fastf_t *wh, fastf_t *t)
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


void
bottom(fastf_t *vec1, fastf_t *vec2, fastf_t *t)
{
    vect_t tvec;
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

    for (i=3; i<=9; i+=3) {
	j = i + 12;
	VADD2(&sol.s_values[j], &sol.s_values[i], tvec);
    }
}


void
top(fastf_t *vec1, fastf_t *vec2, fastf_t *t)
{
    fastf_t tooch, mag;
    vect_t del, tvec;
    int i, j;

    tooch = t[2] * .25;
    del[0] = vec2[0] - vec1[0];
    del[1] = 0.0;
    del[2] = vec2[2] - vec1[2];
    mag = MAGNITUDE(del);
    VSCALE(tvec, del, tooch/mag);
    VSUB2(&sol.s_values[0], vec1, tvec);
    VADD2(del, del, tvec);
    VADD2(&sol.s_values[3], del, tvec);
    tvec[0] = tvec[2] = 0.0;
    tvec[1] = t[1] - t[0];
    VCROSS(del, tvec, &sol.s_values[3]);
    mag = MAGNITUDE(del);
    if (del[2] < 0)
	mag *= -1.0;
    VSCALE(&sol.s_values[9], del, t[2]/mag);
    VADD2(&sol.s_values[6], &sol.s_values[3], &sol.s_values[9]);
    VMOVE(&sol.s_values[12], tvec);

    for (i=3; i<=9; i+=3) {
	j = i + 12;
	VADD2(&sol.s_values[j], &sol.s_values[i], tvec);
    }
}


void
crregion(char *region, char *op, int *members, int number, char *solidname, int maxlen)
{
    int i;
    struct bu_list head;

    if (dbip == DBI_NULL)
	return;

    BU_LIST_INIT(&head);

    for (i=0; i<number; i++) {
	solidname[8] = '\0';
	crname(solidname, members[i], maxlen);
	if (db_lookup(dbip, solidname, LOOKUP_QUIET) == RT_DIR_NULL) {
	    Tcl_AppendResult(INTERP, "region: ", region, " will skip member: ",
			     solidname, "\n", (char *)NULL);
	    continue;
	}
	mk_addmember(solidname, &head, NULL, op[i]);
    }
    (void)mk_comb(wdbp, region, &head,
		  1, NULL, NULL, NULL,
		  500+Trackpos+i, 0, mat_default, los_default,
		  0, 1, 1);
}


/*
 * convert integer to ascii wd format
 */
void
itoa(int n, char *s, int w)
{
    int c, i, j, sign;

    if ((sign = n) < 0) n = -n;
    i = 0;
    do s[i++] = n % 10 + '0';	while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';

    /* blank fill array
     */
    for (j = i; j < w; j++) s[j] = ' ';
    if (i > w)
	Tcl_AppendResult(INTERP, "itoa: field length too small\n", (char *)NULL);
    s[w] = '\0';
    /* reverse the array
     */
    for (i = 0, j = w - 1; i < j; i++, j--) {
	c    = s[i];
	s[i] = s[j];
	s[j] =    c;
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
