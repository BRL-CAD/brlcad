/*                         V D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libged */
/** @{ */
/** @file libged/vdraw.c
 *
 * Edit vector lists and display them as pseudosolids.
 *
 * OPEN COMMAND
 * vdraw	open			- with no argument, asks if there is
 * an open vlist (1 yes, 0 no)
 *
 * name		- opens the specified vlist
 * returns 1 if creating new vlist
 * 0 if opening an existing vlist
 *
 * EDITING COMMANDS - no return value
 *
 * vdraw write i  c x y z	- write params into i-th vector
 * next	c x y z	- write params to end of vector list
 *
 * vdraw insert i	c x y z	- insert params in front of i-th vector
 *
 * vdraw delete i		- delete i-th vector
 * last		- delete last vector on list
 * all		- delete all vectors on list
 *
 * PARAMETER SETTING COMMAND - no return value
 * vdraw params color		- set the current color with 6 hex digits
 * representing rrggbb
 * name		- change the name of the current vlist
 *
 * QUERY COMMAND
 * vdraw read i	- returns contents of i-th vector "c x y z"
 * color	- return the current color in hex
 * length	- return number of vectors in list
 * name		- return name of current vlist
 *
 * DISPLAY COMMAND -
 * vdraw send		- send the current vlist to the display
 * returns 0 on success, -1 if the name
 * conflicts with an existing true solid
 *
 * CURVE COMMANDS
 * vdraw vlist list	- return list of all existing vlists
 * delete name		- delete the named vlist
 *
 * All textual arguments can be replaced by their first letter.
 * (e.g. "vdraw d a" instead of "vdraw delete all"
 *
 * In the above listing:
 * "i" refers to an integer
 * "c" is an integer representing one of the following bn_vlist commands:
 *
 * BN_VLIST_LINE_MOVE     0 / begin new line /
 * BN_VLIST_LINE_DRAW     1 / draw line /
 * BN_VLIST_POLY_START    2 / pt[] has surface normal /
 * BN_VLIST_POLY_MOVE     3 / move to first poly vertex /
 * BN_VLIST_POLY_DRAW     4 / subsequent poly vertex /
 * BN_VLIST_POLY_END      5 / last vert (repeats 1st), draw poly /
 * BN_VLIST_POLY_VERTNORM 6 / per-vertex normal, for interpolation /
 *
 * "x y z" refer to floating point values which represent a point or
 * normal vector. For commands 0, 1, 3, 4, and 5, they represent a
 * point, while for commands 2 and 6 they represent normal vectors
 *
 * Example Use -
 * vdraw open rays
 * vdraw delete all
 * foreach partition $ray {
 * ...stuff...
 * vdraw write next 0 $inpt
 * vdraw write next 1 $outpt
 * }
 * vdraw send
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include "bio.h"

#include "tcl.h"

#include "cmd.h"
#include "vmath.h"
#include "mater.h"
#include "nmg.h"
#include "dg.h"
#include "ged.h"


#define REV_BU_LIST_FOR(p, structure, hp)	\
    (p)=BU_LIST_LAST(structure, hp);	\
       BU_LIST_NOT_HEAD(p, hp);		\
       (p)=BU_LIST_PLAST(structure, p)

static int vdraw_write_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vdraw_insert_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vdraw_delete_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vdraw_read_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vdraw_send_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vdraw_params_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vdraw_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
static int vdraw_vlist_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);

/**
 * figure out which vdraw command we're running, used by dg
 */
int
vdraw_cmd_tcl(struct dg_obj *dgop,
	  Tcl_Interp *interp,
	  int argc,
	  char *argv[])
{
    /**
     * view draw command table
     */
    static struct bu_cmdtab vdraw_cmds[] = {
	{"write",		vdraw_write_tcl},
	{"insert",		vdraw_insert_tcl},
	{"delete",		vdraw_delete_tcl},
	{"read",		vdraw_read_tcl},
	{"send",		vdraw_send_tcl},
	{"params",		vdraw_params_tcl},
	{"open",		vdraw_open_tcl},
	{"vlist",		vdraw_vlist_tcl},
	{(char *)0,		(int (*)())0 }
    };

    return bu_cmd((ClientData)dgop, interp, argc-1, (const char **)argv+1, vdraw_cmds, 0);
}


/*
 * Usage:
 * write i|next c x y z
 */
static int
vdraw_write_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    size_t idx;
    unsigned long uind = 0;
    struct bn_vlist *vp, *cp;

    if (!dgop->dgo_currVHead) {
	Tcl_AppendResult(interp, "vdraw write: no vlist is currently open.", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc < 4) {
	Tcl_AppendResult(interp, "vdraw write: not enough args\n", (char *)NULL);
	return TCL_ERROR;
    }
    if (argv[1][0] == 'n') {
	/* next */
	for (REV_BU_LIST_FOR(vp, bn_vlist, &(dgop->dgo_currVHead->vdc_vhd))) {
	    if (vp->nused > 0) {
		break;
	    }
	}
	if (BU_LIST_IS_HEAD(vp, &(dgop->dgo_currVHead->vdc_vhd))) {
	    /* we went all the way through */
	    vp = BU_LIST_PNEXT(bn_vlist, vp);
	    if (BU_LIST_IS_HEAD(vp, &(dgop->dgo_currVHead->vdc_vhd))) {
		RT_GET_VLIST(vp);
		BU_LIST_INSERT(&(dgop->dgo_currVHead->vdc_vhd), &(vp->l));
	    }
	}
	if (vp->nused >= BN_VLIST_CHUNK) {
	    vp = BU_LIST_PNEXT(bn_vlist, vp);
	    if (BU_LIST_IS_HEAD(vp, &(dgop->dgo_currVHead->vdc_vhd))) {
		RT_GET_VLIST(vp);
		BU_LIST_INSERT(&(dgop->dgo_currVHead->vdc_vhd), &(vp->l));
	    }
	}
	cp = vp;
	idx = vp->nused;
    } else if (sscanf(argv[1], "%lu", &uind) < 1) {
	Tcl_AppendResult(interp, "vdraw: write index not an integer\n", (char *)NULL);
	return TCL_ERROR;
    } else {
	/* uind holds user-specified index */
	/* only allow one past the end */

	for (BU_LIST_FOR(vp, bn_vlist, &(dgop->dgo_currVHead->vdc_vhd))) {
	    if ((size_t)uind < BN_VLIST_CHUNK) {
		/* this is the right vlist */
		break;
	    }
	    if (vp->nused == 0) {
		break;
	    }
	    uind -= vp->nused;
	}

	if (BU_LIST_IS_HEAD(vp, &(dgop->dgo_currVHead->vdc_vhd))) {
	    if (uind > 0) {
		Tcl_AppendResult(interp, "vdraw: write out of range\n", (char *)NULL);
		return TCL_ERROR;
	    }
	    RT_GET_VLIST(vp);
	    BU_LIST_INSERT(&(dgop->dgo_currVHead->vdc_vhd), &(vp->l));
	}
	if ((size_t)uind > vp->nused) {
	    Tcl_AppendResult(interp, "vdraw: write out of range\n", (char *)NULL);
	    return TCL_ERROR;
	}
	cp = vp;
	idx = uind;
    }

    if (sscanf(argv[2], "%d", &(cp->cmd[idx])) < 1) {
	Tcl_AppendResult(interp, "vdraw: cmd not an integer\n", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc == 6) {
	cp->pt[idx][0] = atof(argv[3]);
	cp->pt[idx][1] = atof(argv[4]);
	cp->pt[idx][2] = atof(argv[5]);
    } else {
	if (argc != 4 ||
	    bn_decode_vect(cp->pt[idx], argv[3]) != 3) {
	    Tcl_AppendResult(interp,
			     "vdraw write: wrong # args, need either x y z or {x y z}\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    /* increment counter only if writing onto end */
    if (idx == cp->nused)
	cp->nused++;

    return TCL_OK;
}


/*
 * Usage:
 * insert i c x y z
 */
int
vdraw_insert_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    struct bn_vlist *vp, *cp, *wp;
    int i;
    int idx;
    unsigned long uind = 0;

    if (!dgop->dgo_currVHead) {
	Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc < 6) {
	Tcl_AppendResult(interp, "vdraw: not enough args", (char *)NULL);
	return TCL_ERROR;
    }
    if (sscanf(argv[1], "%lu", &uind) < 1) {
	Tcl_AppendResult(interp, "vdraw: insert index not an integer\n", (char *)NULL);
	return TCL_ERROR;
    }

    /* uinds hold user specified index */
    for (BU_LIST_FOR(vp, bn_vlist, &(dgop->dgo_currVHead->vdc_vhd))) {
	if ((size_t)uind < BN_VLIST_CHUNK) {
	    /* this is the right vlist */
	    break;
	}
	if (vp->nused == 0) {
	    break;
	}
	uind -= vp->nused;
    }

    if (BU_LIST_IS_HEAD(vp, &(dgop->dgo_currVHead->vdc_vhd))) {
	if (uind > 0) {
	    Tcl_AppendResult(interp, "vdraw: insert out of range\n", (char *)NULL);
	    return TCL_ERROR;
	}
	RT_GET_VLIST(vp);
	BU_LIST_INSERT(&(dgop->dgo_currVHead->vdc_vhd), &(vp->l));
    }
    if ((size_t)uind > vp->nused) {
	Tcl_AppendResult(interp, "vdraw: insert out of range\n", (char *)NULL);
	return TCL_ERROR;
    }


    cp = vp;
    idx = uind;

    vp = BU_LIST_LAST(bn_vlist, &(dgop->dgo_currVHead->vdc_vhd));
    vp->nused++;

    while (vp != cp) {
	for (i = vp->nused-1; i > 0; i--) {
	    vp->cmd[i] = vp->cmd[i-1];
	    VMOVE(vp->pt[i], vp->pt[i-1]);
	}
	wp = BU_LIST_PLAST(bn_vlist, vp);
	vp->cmd[0] = wp->cmd[BN_VLIST_CHUNK-1];
	VMOVE(vp->pt[0], wp->pt[BN_VLIST_CHUNK-1]);
	vp = wp;
    }

    for (i=vp->nused-1; i>idx; i--) {
	vp->cmd[i] = vp->cmd[i-1];
	VMOVE(vp->pt[i], vp->pt[i-1]);
    }
    if (sscanf(argv[2], "%d", &(vp->cmd[idx])) < 1) {
	Tcl_AppendResult(interp, "vdraw: cmd not an integer\n", (char *)NULL);
	return TCL_ERROR;
    }
    vp->pt[idx][0] = atof(argv[3]);
    vp->pt[idx][1] = atof(argv[4]);
    vp->pt[idx][2] = atof(argv[5]);

    return TCL_OK;
}


/*
 * Usage:
 * delete i|last|all
 */
int
vdraw_delete_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    struct bn_vlist *vp, *wp;
    size_t i;
    unsigned long uind = 0;

    if (!dgop->dgo_currVHead) {
	Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc < 2) {
	Tcl_AppendResult(interp, "vdraw: not enough args\n", (char *)NULL);
	return TCL_ERROR;
    }
    if (argv[1][0] == 'a') {
	/* delete all */
	for (BU_LIST_FOR(vp, bn_vlist, &(dgop->dgo_currVHead->vdc_vhd))) {
	    vp->nused = 0;
	}
	return TCL_OK;
    }
    if (argv[1][0] == 'l') {
	/* delete last */
	for (REV_BU_LIST_FOR(vp, bn_vlist, &(dgop->dgo_currVHead->vdc_vhd))) {
	    if (vp->nused > 0) {
		vp->nused--;
		break;
	    }
	}
	return TCL_OK;
    }
    if (sscanf(argv[1], "%lu", &uind) < 1) {
	Tcl_AppendResult(interp, "vdraw: delete index not an integer\n", (char *)NULL);
	return TCL_ERROR;
    }

    for (BU_LIST_FOR(vp, bn_vlist, &(dgop->dgo_currVHead->vdc_vhd))) {
	if ((size_t)uind < BN_VLIST_CHUNK) {
	    /* this is the right vlist */
	    break;
	}
	if (vp->nused == 0) {
	    /* no point going further */
	    break;
	}
	uind -= vp->nused;
    }

    if ((size_t)uind >= vp->nused) {
	Tcl_AppendResult(interp, "vdraw: delete out of range\n", (char *)NULL);
	return TCL_ERROR;
    }

    for (i = uind; i < vp->nused - 1; i++) {
	vp->cmd[i] = vp->cmd[i+1];
	VMOVE(vp->pt[i], vp->pt[i+1]);
    }

    wp = BU_LIST_PNEXT(bn_vlist, vp);
    while (BU_LIST_NOT_HEAD(wp, &(dgop->dgo_currVHead->vdc_vhd))) {
	if (wp->nused == 0) {
	    break;
	}

	vp->cmd[BN_VLIST_CHUNK-1] = wp->cmd[0];
	VMOVE(vp->pt[BN_VLIST_CHUNK-1], wp->pt[0]);

	for (i=0; i< wp->nused - 1; i++) {
	    wp->cmd[i] = wp->cmd[i+1];
	    VMOVE(wp->pt[i], wp->pt[i+1]);
	}
	vp = wp;
	wp = BU_LIST_PNEXT(bn_vlist, vp);
    }

    if (vp->nused <= 0) {
	/* this shouldn't happen */
	Tcl_AppendResult(interp, "vdraw: vlist corrupt", (char *)NULL);
	return TCL_ERROR;
    }
    vp->nused--;

    return TCL_OK;
}


/*
 * Usage:
 * read i|color|length|name
 */
static int
vdraw_read_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    struct bn_vlist *vp;
    struct bu_vls vls;
    unsigned long uind = 0;
    int length;

    if (!dgop->dgo_currVHead) {
	Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc < 2) {
	Tcl_AppendResult(interp, "vdraw: need index to read\n", (char *)NULL);
	return TCL_ERROR;
    }
    if (argv[1][0] == 'c') {
	/* read color of current solid */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%.6lx", dgop->dgo_currVHead->vdc_rgb);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }
    if (argv[1][0] == 'n') {
	/*read name of currently open solid*/
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%.89s", dgop->dgo_currVHead->vdc_name);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }
    if (argv[1][0] == 'l') {
	/* return lenght of list */
	length = 0;
	vp = BU_LIST_FIRST(bn_vlist, &(dgop->dgo_currVHead->vdc_vhd));
	while (!BU_LIST_IS_HEAD(vp, &(dgop->dgo_currVHead->vdc_vhd))) {
	    length += vp->nused;
	    vp = BU_LIST_PNEXT(bn_vlist, vp);
	}
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%d", length);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }
    if (sscanf(argv[1], "%lu", &uind) < 1) {
	Tcl_AppendResult(interp, "vdraw: read index not an integer\n", (char *)NULL);
	return TCL_ERROR;
    }

    for (BU_LIST_FOR(vp, bn_vlist, &(dgop->dgo_currVHead->vdc_vhd))) {
	if ((size_t)uind < BN_VLIST_CHUNK) {
	    /* this is the right vlist */
	    break;
	}
	if (vp->nused == 0) {
	    /* no point going further */
	    break;
	}
	uind -= vp->nused;
    }

    if ((size_t)uind >= vp->nused) {
	Tcl_AppendResult(interp, "vdraw: read out of range\n", (char *)NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d %.12e %.12e %.12e",
		  vp->cmd[uind], vp->pt[uind][0],
		  vp->pt[uind][1], vp->pt[uind][2]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * Usage:
 * send
 */
static int
vdraw_send_tcl(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), char *UNUSED(argv[]))
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    struct directory *dp;
    struct bu_vls vls;
    char solid_name [RT_VDRW_MAXNAME+RT_VDRW_PREFIX_LEN+1];
    int idx;
    int real_flag;

    if (!dgop->dgo_currVHead) {
	Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
	return TCL_ERROR;
    }

    snprintf(solid_name, RT_VDRW_MAXNAME+RT_VDRW_PREFIX_LEN+1, "%s%s", RT_VDRW_PREFIX, dgop->dgo_currVHead->vdc_name);
    if ((dp = db_lookup(dgop->dgo_wdbp->dbip, solid_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
	real_flag = 0;
    } else {
	real_flag = (dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
    }

    if (real_flag) {
	/* solid exists - don't kill */
	Tcl_AppendResult(interp, "-1", (char *)NULL);
	return TCL_OK;
    }

    /* 0 means OK, -1 means conflict with real solid name */
    idx = dgo_invent_solid(dgop,
			   interp,
			   solid_name,
			   &(dgop->dgo_currVHead->vdc_vhd),
			   dgop->dgo_currVHead->vdc_rgb,
			   1, 0.0, 0);
    dgo_notify(dgop, interp);

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d", idx);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * Usage:
 * params color|name
 */
static int
vdraw_params_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    struct vd_curve *rcp;
    unsigned long rgb;

    if (!dgop->dgo_currVHead) {
	Tcl_AppendResult(interp, "vdraw: no vlist is currently open.", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc < 3) {
	Tcl_AppendResult(interp, "vdraw: need params to set\n", (char *)NULL);
	return TCL_ERROR;
    }
    if (argv[1][0] == 'c') {
	if (sscanf(argv[2], "%lx", &rgb)>0)
	    dgop->dgo_currVHead->vdc_rgb = rgb;
	return TCL_OK;
    }
    if (argv[1][0] == 'n') {
	/* check for conflicts with existing vlists*/
	for (BU_LIST_FOR(rcp, vd_curve, &dgop->dgo_headVDraw)) {
	    if (!strncmp(rcp->vdc_name, argv[1], RT_VDRW_MAXNAME)) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "vdraw: name %.40s is already in use\n", argv[1]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		return TCL_ERROR;
	    }
	}
	/* otherwise name not yet used */
	bu_strlcpy(dgop->dgo_currVHead->vdc_name, argv[2], RT_VDRW_MAXNAME);

	Tcl_AppendResult(interp, "0", (char *)NULL);
	return TCL_OK;
    }

    return TCL_OK;
}


/*
 * Usage:
 * open [name]
 */
static int
vdraw_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    struct vd_curve *rcp;
    struct bn_vlist *vp;
    char temp_name[RT_VDRW_MAXNAME+1];

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib vdraw_open");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
    }

    if (argc == 1) {
	if (dgop->dgo_currVHead) {
	    Tcl_AppendResult(interp, "1", (char *)NULL);
	    return TCL_OK;
	} else {
	    Tcl_AppendResult(interp, "0", (char *)NULL);
	    return TCL_OK;
	}
    }

    bu_strlcpy(temp_name, argv[1], RT_VDRW_MAXNAME);

    dgop->dgo_currVHead = (struct vd_curve *) NULL;
    for (BU_LIST_FOR(rcp, vd_curve, &dgop->dgo_headVDraw)) {
	if (!strncmp(rcp->vdc_name, temp_name, RT_VDRW_MAXNAME)) {
	    dgop->dgo_currVHead = rcp;
	    break;
	}
    }

    if (!dgop->dgo_currVHead) {
	/* create new entry */
	BU_GETSTRUCT(rcp, vd_curve);
	BU_LIST_APPEND(&dgop->dgo_headVDraw, &(rcp->l));

	bu_strlcpy(rcp->vdc_name, temp_name, RT_VDRW_MAXNAME);

	rcp->vdc_rgb = RT_VDRW_DEF_COLOR;
	BU_LIST_INIT(&(rcp->vdc_vhd));
	RT_GET_VLIST(vp);
	BU_LIST_APPEND(&(rcp->vdc_vhd), &(vp->l));
	dgop->dgo_currVHead = rcp;
	/* 1 means new entry */
	Tcl_AppendResult(interp, "1", (char *)NULL);
	return TCL_OK;
    } else {
	/* entry already existed */
	if (BU_LIST_IS_EMPTY(&(dgop->dgo_currVHead->vdc_vhd))) {
	    RT_GET_VLIST(vp);
	    BU_LIST_APPEND(&(dgop->dgo_currVHead->vdc_vhd), &(vp->l));
	}
	dgop->dgo_currVHead->vdc_name[RT_VDRW_MAXNAME] = '\0'; /*safety*/
	/* 0 means entry already existed*/
	Tcl_AppendResult(interp, "0", (char *)NULL);
	return TCL_OK;
    }
}


/*
 * Usage:
 * vlist list
 * vlist delete name
 */
static int
vdraw_vlist_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    struct dg_obj *dgop = (struct dg_obj *)clientData;
    struct vd_curve *rcp, *rcp2;
    struct bu_vls vls;

    if (argc < 2) {
	Tcl_AppendResult(interp, "vdraw: need more args", (char *)NULL);
	return TCL_ERROR;
    }

    switch (argv[1][0]) {
	case 'l':
	    bu_vls_init(&vls);
	    for (BU_LIST_FOR(rcp, vd_curve, &dgop->dgo_headVDraw)) {
		bu_vls_strcat(&vls, rcp->vdc_name);
		bu_vls_strcat(&vls, " ");
	    }

	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);
	    return TCL_OK;
	case 'd':
	    if (argc < 3) {
		Tcl_AppendResult(interp, "vdraw: need name of vlist to delete", (char *)NULL);
		return TCL_ERROR;
	    }
	    rcp2 = (struct vd_curve *)NULL;
	    for (BU_LIST_FOR(rcp, vd_curve, &dgop->dgo_headVDraw)) {
		if (!strncmp(rcp->vdc_name, argv[2], RT_VDRW_MAXNAME)) {
		    rcp2 = rcp;
		    break;
		}
	    }
	    if (!rcp2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "vdraw: vlist %.40s not found", argv[2]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	    }
	    BU_LIST_DEQUEUE(&(rcp2->l));
	    if (dgop->dgo_currVHead == rcp2) {
		if (BU_LIST_IS_EMPTY(&dgop->dgo_headVDraw)) {
		    dgop->dgo_currVHead = (struct vd_curve *)NULL;
		} else {
		    dgop->dgo_currVHead = BU_LIST_LAST(vd_curve, &dgop->dgo_headVDraw);
		}
	    }
	    RT_FREE_VLIST(&(rcp2->vdc_vhd));
	    bu_free((genptr_t) rcp2, "vd_curve");
	    return TCL_OK;
	default:
	    Tcl_AppendResult(interp, "vdraw: unknown option to vdraw vlist", (char *)NULL);
	    return TCL_ERROR;
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
