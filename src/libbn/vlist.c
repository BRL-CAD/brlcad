/*                        V L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/list.h"
#include "bu/log.h"
#include "vmath.h"
#include "bn/vlist.h"

int
bn_vlist_cmd_cnt(struct bn_vlist *vlist)
{
    int num_commands;
    struct bn_vlist *vp;

    if (UNLIKELY(vlist == NULL)) {
	return 0;
    }

    num_commands = 0;
    for (BU_LIST_FOR(vp, bn_vlist, &(vlist->l))) {
	num_commands += vp->nused;
    }

    return num_commands;
}

int
bn_vlist_bbox(struct bn_vlist *vp, point_t *bmin, point_t *bmax)
{
    int i;
    int nused = vp->nused;
    int *cmd = vp->cmd;
    point_t *pt = vp->pt;

    for (i = 0; i < nused; i++, cmd++, pt++) {
	switch (*cmd) {
	    case BN_VLIST_POLY_START:
	    case BN_VLIST_POLY_VERTNORM:
	    case BN_VLIST_TRI_START:
	    case BN_VLIST_TRI_VERTNORM:
	    case BN_VLIST_POINT_SIZE:
	    case BN_VLIST_LINE_WIDTH:
		/* attribute, not location */
		break;
	    case BN_VLIST_LINE_MOVE:
	    case BN_VLIST_LINE_DRAW:
	    case BN_VLIST_POLY_MOVE:
	    case BN_VLIST_POLY_DRAW:
	    case BN_VLIST_POLY_END:
	    case BN_VLIST_TRI_MOVE:
	    case BN_VLIST_TRI_DRAW:
	    case BN_VLIST_TRI_END:
		V_MIN((*bmin)[X], (*pt)[X]);
		V_MAX((*bmax)[X], (*pt)[X]);
		V_MIN((*bmin)[Y], (*pt)[Y]);
		V_MAX((*bmax)[Y], (*pt)[Y]);
		V_MIN((*bmin)[Z], (*pt)[Z]);
		V_MAX((*bmax)[Z], (*pt)[Z]);
		break;
	    case BN_VLIST_POINT_DRAW:
		V_MIN((*bmin)[X], (*pt)[X]-1.0);
		V_MAX((*bmax)[X], (*pt)[X]+1.0);
		V_MIN((*bmin)[Y], (*pt)[Y]-1.0);
		V_MAX((*bmax)[Y], (*pt)[Y]+1.0);
		V_MIN((*bmin)[Z], (*pt)[Z]-1.0);
		V_MAX((*bmax)[Z], (*pt)[Z]+1.0);
		break;
	    default:
		return *cmd;
	}
    }

    return 0;
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
