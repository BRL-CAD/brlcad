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

#include "./ged_private.h"


#define REV_BU_LIST_FOR(p, structure, hp)	\
    (p)=BU_LIST_LAST(structure, hp);	\
       BU_LIST_NOT_HEAD(p, hp);		\
       (p)=BU_LIST_PLAST(structure, p)


/*
 * Usage:
 *        vdraw write i|next c x y z
 */
static int
vdraw_write(struct ged *gedp, int argc, const char *argv[])
{
    size_t idx;
    unsigned long uind = 0;
    struct bn_vlist *vp, *cp;
    static const char *usage = "i|next c x y z";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "vdraw write: no vlist is currently open.");
	return GED_ERROR;
    }
    if (argc < 5) {
	bu_vls_printf(gedp->ged_result_str, "vdraw write: not enough args\n");
	return GED_ERROR;
    }
    if (argv[2][0] == 'n') {
	/* next */
	for (REV_BU_LIST_FOR(vp, bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    if (vp->nused > 0) {
		break;
	    }
	}
	if (BU_LIST_IS_HEAD(vp, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    /* we went all the way through */
	    vp = BU_LIST_PNEXT(bn_vlist, vp);
	    if (BU_LIST_IS_HEAD(vp, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
		RT_GET_VLIST(vp);
		BU_LIST_INSERT(&(gedp->ged_gdp->gd_currVHead->vdc_vhd), &(vp->l));
	    }
	}
	if (vp->nused >= BN_VLIST_CHUNK) {
	    vp = BU_LIST_PNEXT(bn_vlist, vp);
	    if (BU_LIST_IS_HEAD(vp, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
		RT_GET_VLIST(vp);
		BU_LIST_INSERT(&(gedp->ged_gdp->gd_currVHead->vdc_vhd), &(vp->l));
	    }
	}
	cp = vp;
	idx = vp->nused;
    } else if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: write index not an integer\n");
	return GED_ERROR;
    } else {
	/* uind holds user-specified index */
	/* only allow one past the end */

	for (BU_LIST_FOR(vp, bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    if ((size_t)uind < BN_VLIST_CHUNK) {
		/* this is the right vlist */
		break;
	    }
	    if (vp->nused == 0) {
		break;
	    }
	    uind -= vp->nused;
	}

	if (BU_LIST_IS_HEAD(vp, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    if (uind > 0) {
		bu_vls_printf(gedp->ged_result_str, "vdraw: write out of range\n");
		return GED_ERROR;
	    }
	    RT_GET_VLIST(vp);
	    BU_LIST_INSERT(&(gedp->ged_gdp->gd_currVHead->vdc_vhd), &(vp->l));
	}
	if ((size_t)uind > vp->nused) {
	    bu_vls_printf(gedp->ged_result_str, "vdraw: write out of range\n");
	    return GED_ERROR;
	}
	cp = vp;
	idx = uind;
    }

    if (sscanf(argv[3], "%d", &(cp->cmd[idx])) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: cmd not an integer\n");
	return GED_ERROR;
    }
    if (argc == 7) {
	cp->pt[idx][0] = atof(argv[4]);
	cp->pt[idx][1] = atof(argv[5]);
	cp->pt[idx][2] = atof(argv[6]);
    } else {
	if (argc != 5 ||
	    bn_decode_vect(cp->pt[idx], argv[4]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "vdraw write: wrong # args, need either x y z or {x y z}\n");
	    return GED_ERROR;
	}
    }
    /* increment counter only if writing onto end */
    if (idx == cp->nused)
	cp->nused++;

    return GED_OK;
}


/*
 * Usage:
 *        vdraw insert i c x y z
 */
int
vdraw_insert(struct ged *gedp, int argc, const char *argv[])
{
    struct bn_vlist *vp, *cp, *wp;
    size_t i;
    size_t idx;
    unsigned long uind = 0;
    static const char *usage = "i c x y z";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: no vlist is currently open.");
	return GED_ERROR;
    }
    if (argc < 7) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: not enough args");
	return GED_ERROR;
    }
    if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: insert index not an integer\n");
	return GED_ERROR;
    }

    /* uinds hold user specified index */
    for (BU_LIST_FOR(vp, bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	if ((size_t)uind < BN_VLIST_CHUNK) {
	    /* this is the right vlist */
	    break;
	}
	if (vp->nused == 0) {
	    break;
	}
	uind -= vp->nused;
    }

    if (BU_LIST_IS_HEAD(vp, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	if (uind > 0) {
	    bu_vls_printf(gedp->ged_result_str, "vdraw: insert out of range\n");
	    return GED_ERROR;
	}
	RT_GET_VLIST(vp);
	BU_LIST_INSERT(&(gedp->ged_gdp->gd_currVHead->vdc_vhd), &(vp->l));
    }
    if ((size_t)uind > vp->nused) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: insert out of range\n");
	return GED_ERROR;
    }


    cp = vp;
    idx = uind;

    vp = BU_LIST_LAST(bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd));
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
    if (sscanf(argv[3], "%d", &(vp->cmd[idx])) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: cmd not an integer\n");
	return GED_ERROR;
    }
    vp->pt[idx][0] = atof(argv[4]);
    vp->pt[idx][1] = atof(argv[5]);
    vp->pt[idx][2] = atof(argv[6]);

    return GED_OK;
}


/*
 * Usage:
 *        vdraw delete i|last|all
 */
int
vdraw_delete(struct ged *gedp, int argc, const char *argv[])
{
    struct bn_vlist *vp, *wp;
    size_t i;
    unsigned long uind = 0;
    static const char *usage = "i|last|all";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return GED_ERROR;
    }
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: not enough args\n", argv[0], argv[1]);
	return GED_ERROR;
    }
    if (argv[2][0] == 'a') {
	/* delete all */
	for (BU_LIST_FOR(vp, bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    vp->nused = 0;
	}
	return GED_OK;
    }
    if (argv[2][0] == 'l') {
	/* delete last */
	for (REV_BU_LIST_FOR(vp, bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    if (vp->nused > 0) {
		vp->nused--;
		break;
	    }
	}
	return GED_OK;
    }
    if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: delete index not an integer\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(vp, bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
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
	bu_vls_printf(gedp->ged_result_str, "%s %s: delete out of range\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    for (i = (size_t)uind; i < vp->nused - 1; i++) {
	vp->cmd[i] = vp->cmd[i+1];
	VMOVE(vp->pt[i], vp->pt[i+1]);
    }

    wp = BU_LIST_PNEXT(bn_vlist, vp);
    while (BU_LIST_NOT_HEAD(wp, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
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
	bu_vls_printf(gedp->ged_result_str, "%s %s: vlist corrupt", argv[0], argv[1]);
	return GED_ERROR;
    }
    vp->nused--;

    return GED_OK;
}


/*
 * Usage:
 *        vdraw read i|color|length|name
 */
static int
vdraw_read(struct ged *gedp, int argc, const char *argv[])
{
    struct bn_vlist *vp;
    unsigned long uind = 0;
    int length;
    static const char *usage = "read i|color|length|name";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return GED_ERROR;
    }
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: need index to read\n", argv[0], argv[1]);
	return GED_ERROR;
    }
    if (argv[2][0] == 'c') {
	/* read color of current solid */
	bu_vls_printf(gedp->ged_result_str, "%.6lx", gedp->ged_gdp->gd_currVHead->vdc_rgb);
	return GED_OK;
    }
    if (argv[2][0] == 'n') {
	/*read name of currently open solid*/
	bu_vls_printf(gedp->ged_result_str, "%.89s", gedp->ged_gdp->gd_currVHead->vdc_name);
	return GED_OK;
    }
    if (argv[2][0] == 'l') {
	/* return length of list */
	length = 0;
	vp = BU_LIST_FIRST(bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd));
	while (!BU_LIST_IS_HEAD(vp, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    length += vp->nused;
	    vp = BU_LIST_PNEXT(bn_vlist, vp);
	}
	bu_vls_printf(gedp->ged_result_str, "%d", length);
	return GED_OK;
    }
    if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: read index not an integer\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(vp, bn_vlist, &(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
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
	bu_vls_printf(gedp->ged_result_str, "%s %s: read out of range\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d %.12e %.12e %.12e",
		  vp->cmd[uind], vp->pt[uind][0],
		  vp->pt[uind][1], vp->pt[uind][2]);

    return GED_OK;
}


/*
 * Usage:
 *        vdraw send
 */
static int
vdraw_send(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    char solid_name [RT_VDRW_MAXNAME+RT_VDRW_PREFIX_LEN+1];
    int idx;
    int real_flag;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: missing parameter after [%s]", argv[0]);
	return GED_ERROR;
    }

    if (!gedp->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return GED_ERROR;
    }

    snprintf(solid_name, RT_VDRW_MAXNAME+RT_VDRW_PREFIX_LEN+1, "%s%s", RT_VDRW_PREFIX, gedp->ged_gdp->gd_currVHead->vdc_name);
    if ((dp = db_lookup(gedp->ged_wdbp->dbip, solid_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
	real_flag = 0;
    } else {
	real_flag = (dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
    }

    if (real_flag) {
	/* solid exists - don't kill */
	bu_vls_printf(gedp->ged_result_str, "-1");
	return GED_OK;
    }

    /* 0 means OK, -1 means conflict with real solid name */
    idx = _ged_invent_solid(gedp,
			    solid_name,
			    &(gedp->ged_gdp->gd_currVHead->vdc_vhd),
			    gedp->ged_gdp->gd_currVHead->vdc_rgb,
			    1, 0.0, 0);

    bu_vls_printf(gedp->ged_result_str, "%d", idx);

    return GED_OK;
}


/*
 * Usage:
 *        vdraw params color|name
 */
static int
vdraw_params(struct ged *gedp, int argc, const char *argv[])
{
    struct vd_curve *rcp;
    unsigned long rgb;
    static const char *usage = "color|name args";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return GED_ERROR;
    }
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: need params to set\n", argv[0], argv[1]);
	return GED_ERROR;
    }
    if (argv[2][0] == 'c') {
	if (sscanf(argv[3], "%lx", &rgb)>0)
	    gedp->ged_gdp->gd_currVHead->vdc_rgb = rgb;
	return GED_OK;
    }
    if (argv[2][0] == 'n') {
	/* check for conflicts with existing vlists*/
	for (BU_LIST_FOR(rcp, vd_curve, &gedp->ged_gdp->gd_headVDraw)) {
	    if (!strncmp(rcp->vdc_name, argv[2], RT_VDRW_MAXNAME)) {
		bu_vls_printf(gedp->ged_result_str, "%s %s: name %.40s is already in use\n", argv[0], argv[1], argv[2]);
		return GED_ERROR;
	    }
	}
	/* otherwise name not yet used */
	bu_strlcpy(gedp->ged_gdp->gd_currVHead->vdc_name, argv[2], RT_VDRW_MAXNAME);

	bu_vls_printf(gedp->ged_result_str, "0");
	return GED_OK;
    }

    return GED_OK;
}


/*
 * Usage:
 *        vdraw open [name]
 */
static int
vdraw_open(struct ged *gedp, int argc, const char *argv[])
{
    struct vd_curve *rcp;
    struct bn_vlist *vp;
    char temp_name[RT_VDRW_MAXNAME+1];
    static const char *usage = "[name]";

    if (argc == 2) {
	if (gedp->ged_gdp->gd_currVHead) {
	    bu_vls_printf(gedp->ged_result_str, "1");
	    return GED_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "0");
	    return GED_OK;
	}
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    bu_strlcpy(temp_name, argv[2], RT_VDRW_MAXNAME);

    gedp->ged_gdp->gd_currVHead = (struct vd_curve *) NULL;
    for (BU_LIST_FOR(rcp, vd_curve, &gedp->ged_gdp->gd_headVDraw)) {
	if (!strncmp(rcp->vdc_name, temp_name, RT_VDRW_MAXNAME)) {
	    gedp->ged_gdp->gd_currVHead = rcp;
	    break;
	}
    }

    if (!gedp->ged_gdp->gd_currVHead) {
	/* create new entry */
	BU_GETSTRUCT(rcp, vd_curve);
	BU_LIST_APPEND(&gedp->ged_gdp->gd_headVDraw, &(rcp->l));

	bu_strlcpy(rcp->vdc_name, temp_name, RT_VDRW_MAXNAME);

	rcp->vdc_rgb = RT_VDRW_DEF_COLOR;
	BU_LIST_INIT(&(rcp->vdc_vhd));
	RT_GET_VLIST(vp);
	BU_LIST_APPEND(&(rcp->vdc_vhd), &(vp->l));
	gedp->ged_gdp->gd_currVHead = rcp;
	/* 1 means new entry */
	bu_vls_printf(gedp->ged_result_str, "1");
	return GED_OK;
    } else {
	/* entry already existed */
	if (BU_LIST_IS_EMPTY(&(gedp->ged_gdp->gd_currVHead->vdc_vhd))) {
	    RT_GET_VLIST(vp);
	    BU_LIST_APPEND(&(gedp->ged_gdp->gd_currVHead->vdc_vhd), &(vp->l));
	}
	gedp->ged_gdp->gd_currVHead->vdc_name[RT_VDRW_MAXNAME] = '\0'; /*safety*/
	/* 0 means entry already existed*/
	bu_vls_printf(gedp->ged_result_str, "0");
	return GED_OK;
    }
}


/*
 * Usage:
 *        vdraw vlist list
 *        vdraw vlist delete name
 */
static int
vdraw_vlist(struct ged *gedp, int argc, const char *argv[])
{
    struct vd_curve *rcp, *rcp2;
    static const char *usage = "list\n\tdelete name";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    switch  (argv[2][0]) {
	case 'l':
	    for (BU_LIST_FOR(rcp, vd_curve, &gedp->ged_gdp->gd_headVDraw)) {
		bu_vls_strcat(gedp->ged_result_str, rcp->vdc_name);
		bu_vls_strcat(gedp->ged_result_str, " ");
	    }

	    return GED_OK;
	case 'd':
	    if (argc < 3) {
		bu_vls_printf(gedp->ged_result_str, "%s %s: need name of vlist to delete", argv[0], argv[1]);
		return GED_ERROR;
	    }
	    rcp2 = (struct vd_curve *)NULL;
	    for (BU_LIST_FOR(rcp, vd_curve, &gedp->ged_gdp->gd_headVDraw)) {
		if (!strncmp(rcp->vdc_name, argv[3], RT_VDRW_MAXNAME)) {
		    rcp2 = rcp;
		    break;
		}
	    }
	    if (!rcp2) {
		bu_vls_printf(gedp->ged_result_str, "%s %s: vlist %.40s not found", argv[0], argv[1], argv[3]);
		return GED_ERROR;
	    }
	    BU_LIST_DEQUEUE(&(rcp2->l));
	    if (gedp->ged_gdp->gd_currVHead == rcp2) {
		if (BU_LIST_IS_EMPTY(&gedp->ged_gdp->gd_headVDraw)) {
		    gedp->ged_gdp->gd_currVHead = (struct vd_curve *)NULL;
		} else {
		    gedp->ged_gdp->gd_currVHead = BU_LIST_LAST(vd_curve, &gedp->ged_gdp->gd_headVDraw);
		}
	    }
	    RT_FREE_VLIST(&(rcp2->vdc_vhd));
	    bu_free((genptr_t) rcp2, "vd_curve");
	    return GED_OK;
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s %s: unknown option to vdraw vlist", argv[0], argv[1]);
	    return GED_ERROR;
    }
}


/**
 * view draw command table
 */
static struct bu_cmdtab vdraw_cmds[] = {
    {"write",		vdraw_write},
    {"insert",		vdraw_insert},
    {"delete",		vdraw_delete},
    {"read",		vdraw_read},
    {"send",		vdraw_send},
    {"params",		vdraw_params},
    {"open",		vdraw_open},
    {"vlist",		vdraw_vlist},
    {(char *)0,		(int (*)())0 }
};


static int
vdraw_cmd(struct ged *gedp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp;
    static const char *usage = "write|insert|delete|read|send|params|open|vlist [args]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    for (ctp = vdraw_cmds; ctp->ct_name != (char *)0; ctp++) {
	if (ctp->ct_name[0] == argv[1][0] &&
	    BU_STR_EQUAL(ctp->ct_name, argv[1])) {
	    return (*ctp->ct_func)(gedp, argc, argv);
	}
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return GED_ERROR;
}


int
ged_vdraw(struct ged *gedp, int argc, const char *argv[])
{
    return vdraw_cmd(gedp, argc, argv);
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
