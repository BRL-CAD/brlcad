/*                        V L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
#include "bnetwork.h"

#include "vmath.h"
#include "bu/cv.h"
#include "bu/malloc.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bn/plot3.h"
#include "bn/vlist.h"

size_t
bn_vlist_cmd_cnt(struct bn_vlist *vlist)
{
    size_t num_commands;
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

static int
bn_vlist_bbox_internal(struct bn_vlist *vp, point_t *bmin, point_t *bmax, int *disp_mode)
{
    size_t i;
    size_t nused = vp->nused;
    int *cmd = vp->cmd;
    point_t *pt = vp->pt;

    for (i = 0; i < nused; i++, cmd++, pt++) {
	if(*disp_mode == 1 && *cmd != BN_VLIST_MODEL_MAT)
	    continue;
	*disp_mode = 0;
	switch (*cmd) {
	    case BN_VLIST_POLY_START:
	    case BN_VLIST_POLY_VERTNORM:
	    case BN_VLIST_TRI_START:
	    case BN_VLIST_TRI_VERTNORM:
	    case BN_VLIST_POINT_SIZE:
	    case BN_VLIST_LINE_WIDTH:
	    case BN_VLIST_MODEL_MAT:
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
	    case BN_VLIST_DISPLAY_MAT:
		*disp_mode = 1;
		/* fall through */
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

int
bn_vlist_bbox(struct bu_list *vlistp, point_t *bmin, point_t *bmax, size_t *length)
{
    struct bn_vlist* vp;
    int cmd = 0;
    int disp_mode = 0;
    size_t len = 0;
    for (BU_LIST_FOR(vp, bn_vlist, vlistp)) {
	cmd = bn_vlist_bbox_internal(vp, bmin, bmax, &disp_mode);
	if (cmd) {
	    break;
	}
	len += vp->nused;
    }
    if (length){
	*length = len;
    }
    return cmd;
}

const char *
bn_vlist_get_cmd_description(int cmd)
{
    /* bn_vlist_cmd_descriptions contains descriptions of the first
     * num_described_cmds vlist cmds
     */
    const int num_described_cmds = 13;
    static const char *bn_vlist_cmd_descriptions[] = {
	"line move ",
	"line draw ",
	"poly start",
	"poly move ",
	"poly draw ",
	"poly end  ",
	"poly vnorm",
	"tri start",
	"tri move",
	"tri draw",
	"tri end",
	"tri vnorm",
	"point draw",
	"model mat",
	"display mat",
    };
    if (cmd < num_described_cmds) {
	return bn_vlist_cmd_descriptions[cmd];
    } else {
	return "**unknown*";
    }
}

size_t
bn_ck_vlist(const struct bu_list *vhead)
{
    register struct bn_vlist *vp;
    size_t npts = 0;

    for (BU_LIST_FOR(vp, bn_vlist, vhead)) {
	size_t i;
	size_t nused = vp->nused;
	register int *cmd = vp->cmd;
	register point_t *pt = vp->pt;

	BN_CK_VLIST(vp);
	npts += nused;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    register int j;

	    for (j=0; j < 3; j++) {
		/*
		 * If (*pt)[j] is an IEEE NaN, then all comparisons
		 * between it and any genuine number will return
		 * FALSE.  This test is formulated so that NaN values
		 * will activate the "else" clause.
		 */
		if ((*pt)[j] > -INFINITY && (*pt)[j] < INFINITY) {
		    /* Number is good */
		} else {
		    bu_log("  %s (%g, %g, %g)\n",
			   bn_vlist_get_cmd_description(*cmd),
			   V3ARGS(*pt));
		    bu_bomb("bn_ck_vlist() bad coordinate value\n");
		}
		/* XXX Need a define for largest command number */
		if (*cmd < 0 || *cmd > BN_VLIST_CMD_MAX) {
		    bu_log("cmd = x%x (%d.)\n", *cmd, *cmd);
		    bu_bomb("bn_ck_vlist() bad vlist command\n");
		}
	    }
	}
    }
    return npts;
}

void
bn_vlist_copy(struct bu_list *vlists, struct bu_list *dest, const struct bu_list *src)
{
    struct bn_vlist *vp;

    for (BU_LIST_FOR(vp, bn_vlist, src)) {
	size_t i;
	size_t nused = vp->nused;
	register int *cmd = vp->cmd;
	register point_t *pt = vp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    BN_ADD_VLIST(vlists, dest, *pt, *cmd);
	}
    }
}

void
bn_vlist_cleanup(struct bu_list *hd)
{
    register struct bn_vlist *vp;

    if (!BU_LIST_IS_INITIALIZED(hd)) {
	BU_LIST_INIT(hd);
	return;
    }

    while (BU_LIST_WHILE(vp, bn_vlist, hd)) {
	BN_CK_VLIST(vp);
	BU_LIST_DEQUEUE(&(vp->l));
	bu_free((char *)vp, "bn_vlist");
    }
}

void
bn_vlist_export(struct bu_vls *vls, struct bu_list *hp, const char *name)
{
    register struct bn_vlist *vp;
    size_t nelem;
    size_t namelen;
    size_t nbytes;
    unsigned char *buf;
    unsigned char *bp;

    BU_CK_VLS(vls);

    /* Count number of element in the vlist */
    nelem = 0;
    for (BU_LIST_FOR(vp, bn_vlist, hp)) {
	nelem += vp->nused;
    }

    /* Build output buffer for binary transmission
     * nelem[4], String[n+1], cmds[nelem*1], pts[3*nelem*8]
     */
    namelen = strlen(name)+1;
    nbytes = namelen + 4 + nelem * (1+ELEMENTS_PER_VECT*SIZEOF_NETWORK_DOUBLE) + 2;

    /* FIXME: this is pretty much an abuse of vls.  should be using
     * vlb for variable-length byte buffers.
     */
    bu_vls_setlen(vls, (int)nbytes);
    buf = (unsigned char *)bu_vls_addr(vls);
    *(uint32_t *)buf = htonl((uint32_t)nelem);
    bp = buf+sizeof(uint32_t);
    bu_strlcpy((char *)bp, name, namelen);
    bp += namelen;

    /* Output cmds, as bytes */
    for (BU_LIST_FOR(vp, bn_vlist, hp)) {
	size_t i;
	size_t nused = vp->nused;
	register int *cmd = vp->cmd;
	for (i = 0; i < nused; i++) {
	    *bp++ = *cmd++;
	}
    }

    /* Output points, as three 8-byte doubles */
    for (BU_LIST_FOR(vp, bn_vlist, hp)) {
	size_t i;
	size_t nused = vp->nused;
	register point_t *pt = vp->pt;

	/* must be double for import and export */
	double point[ELEMENTS_PER_POINT];

	for (i = 0; i < nused; i++) {
	    VMOVE(point, pt[i]); /* convert fastf_t to double */
	    bu_cv_htond(bp, (unsigned char *)point, ELEMENTS_PER_VECT);
	    bp += ELEMENTS_PER_VECT*SIZEOF_NETWORK_DOUBLE;
	}
    }
}

void
bn_vlist_import(struct bu_list *vlists, struct bu_list *hp, struct bu_vls *namevls, const unsigned char *buf)
{
    register const unsigned char *bp;
    const unsigned char *pp;            /* point pointer */
    size_t nelem;
    size_t namelen;
    size_t i;

    /* must be double for import and export */
    double point[ELEMENTS_PER_POINT];

    BU_CK_VLS(namevls);

    nelem = ntohl(*(uint32_t *)buf);
    bp = buf+4;

    namelen = strlen((char *)bp)+1;
    bu_vls_strncpy(namevls, (char *)bp, namelen);
    bp += namelen;

    pp = bp + nelem*1;

    for (i=0; i < nelem; i++) {
	int cmd;

	cmd = *bp++;
	bu_cv_ntohd((unsigned char *)point, pp, ELEMENTS_PER_POINT);
	pp += ELEMENTS_PER_POINT*SIZEOF_NETWORK_DOUBLE;
	BN_ADD_VLIST(vlists, hp, point, cmd);
    }
}

struct bn_vlblock *
bn_vlblock_init(struct bu_list *free_vlist_hd, /**< where to get/put free vlists */
		int max_ent /**< maximum number of entities to get/put */)
{
    struct bn_vlblock *vbp;
    size_t i;

    if (!BU_LIST_IS_INITIALIZED(free_vlist_hd))
	BU_LIST_INIT(free_vlist_hd);

    BU_ALLOC(vbp, struct bn_vlblock);
    vbp->magic = BN_VLBLOCK_MAGIC;
    vbp->free_vlist_hd = free_vlist_hd;
    vbp->max = max_ent;
    vbp->head = (struct bu_list *)bu_calloc(vbp->max,
					    sizeof(struct bu_list), "head[]");
    vbp->rgb = (long *)bu_calloc(vbp->max,
				 sizeof(long), "rgb[]");

    for (i=0; i < vbp->max; i++) {
	vbp->rgb[i] = 0;
	BU_LIST_INIT(&(vbp->head[i]));
    }

    vbp->rgb[0] = 0xFFFF00L;    /* Yellow, default */
    vbp->rgb[1] = 0xFFFFFFL;    /* White */
    vbp->nused = 2;

    return vbp;
}

void
bn_vlblock_free(struct bn_vlblock *vbp)
{
    size_t i;

    BN_CK_VLBLOCK(vbp);
    for (i=0; i < vbp->nused; i++) {
	/* Release any remaining vlist storage */
	if (vbp->rgb[i] == 0) continue;
	if (BU_LIST_IS_EMPTY(&(vbp->head[i]))) continue;
	BN_FREE_VLIST(vbp->free_vlist_hd, &(vbp->head[i]));
    }

    bu_free((char *)(vbp->head), "head[]");
    bu_free((char *)(vbp->rgb), "rgb[]");
    bu_free((char *)vbp, "bn_vlblock");

}

struct bu_list *
bn_vlblock_find(struct bn_vlblock *vbp, int r, int g, int b)
{
    long newrgb;
    size_t n;
    size_t omax;                /* old max */

    BN_CK_VLBLOCK(vbp);

    newrgb = ((r&0xFF)<<16)|((g&0xFF)<<8)|(b&0xFF);

    for (n=0; n < vbp->nused; n++) {
	if (vbp->rgb[n] == newrgb)
	    return &(vbp->head[n]);
    }
    if (vbp->nused < vbp->max) {
	/* Allocate empty slot */
	n = vbp->nused++;
	vbp->rgb[n] = newrgb;
	return &(vbp->head[n]);
    }

    /************** enlarge the table ****************/
    omax = vbp->max;
    vbp->max *= 2;

    /* Look for empty lists and mark for use below. */
    for (n=0; n < omax; n++)
	if (BU_LIST_IS_EMPTY(&vbp->head[n]))
	    vbp->head[n].forw = BU_LIST_NULL;

    vbp->head = (struct bu_list *)bu_realloc((void *)vbp->head,
					     vbp->max * sizeof(struct bu_list),
					     "head[]");
    vbp->rgb = (long *)bu_realloc((void *)vbp->rgb,
				  vbp->max * sizeof(long),
				  "rgb[]");

    /* re-initialize pointers in lower half */
    for (n=0; n < omax; n++) {
	/*
	 * Check to see if list is empty
	 * (i.e. yellow and/or white are not used).
	 * Note - we can't use BU_LIST_IS_EMPTY here because
	 * the addresses of the list heads have possibly changed.
	 */
	if (vbp->head[n].forw == BU_LIST_NULL) {
	    vbp->head[n].forw = &vbp->head[n];
	    vbp->head[n].back = &vbp->head[n];
	} else {
	    vbp->head[n].forw->back = &vbp->head[n];
	    vbp->head[n].back->forw = &vbp->head[n];
	}
    }

    /* initialize upper half of memory */
    for (n=omax; n < vbp->max; n++) {
	vbp->rgb[n] = 0;
	BU_LIST_INIT(&vbp->head[n]);
    }

    /* here we go again */
    return bn_vlblock_find(vbp, r, g, b);
}

void
bn_vlist_rpp(struct bu_list *vlists, struct bu_list *hd, const point_t minn, const point_t maxx)
{
    point_t p;

    VSET(p, minn[X], minn[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_MOVE);

    /* first side */
    VSET(p, minn[X], maxx[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);
    VSET(p, minn[X], maxx[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);
    VSET(p, minn[X], minn[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);
    VSET(p, minn[X], minn[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);

    /* across */
    VSET(p, maxx[X], minn[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);

    /* second side */
    VSET(p, maxx[X], maxx[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);
    VSET(p, maxx[X], maxx[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);
    VSET(p, maxx[X], minn[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);
    VSET(p, maxx[X], minn[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);

    /* front edge */
    VSET(p, minn[X], maxx[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_MOVE);
    VSET(p, maxx[X], maxx[Y], minn[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);

    /* bottom back */
    VSET(p, minn[X], minn[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_MOVE);
    VSET(p, maxx[X], minn[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);

    /* top back */
    VSET(p, minn[X], maxx[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_MOVE);
    VSET(p, maxx[X], maxx[Y], maxx[Z]);
    BN_ADD_VLIST(vlists, hd, p, BN_VLIST_LINE_DRAW);
}

void
bn_plot_vlblock(FILE *fp, const struct bn_vlblock *vbp)
{
    size_t i;

    BN_CK_VLBLOCK(vbp);

    for (i=0; i < vbp->nused; i++) {
	if (vbp->rgb[i] == 0) continue;
	if (BU_LIST_IS_EMPTY(&(vbp->head[i]))) continue;
	pl_color(fp,
		 (vbp->rgb[i]>>16) & 0xFF,
		 (vbp->rgb[i]>> 8) & 0xFF,
		 (vbp->rgb[i]) & 0xFF);
	bn_vlist_to_uplot(fp, &(vbp->head[i]));
    }
}


void
bn_vlist_to_uplot(FILE *fp, const struct bu_list *vhead)
{
    register struct bn_vlist *vp;

    for (BU_LIST_FOR(vp, bn_vlist, vhead)) {
	size_t i;
	size_t nused = vp->nused;
	register const int *cmd = vp->cmd;
	register point_t *pt = vp->pt;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    break;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_TRI_MOVE:
		    pdv_3move(fp, *pt);
		    break;
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_POLY_END:
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_TRI_DRAW:
		case BN_VLIST_TRI_END:
		    pdv_3cont(fp, *pt);
		    break;
		default:
		    bu_log("bn_vlist_to_uplot: unknown vlist cmd x%x\n",
			   *cmd);
	    }
	}
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
