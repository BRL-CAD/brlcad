/*                         V L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2021 United States Government as represented by
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
#include <string.h>
#include <math.h>
#include <string.h>
#include "bnetwork.h"

#include "bu/cv.h"
#include "vmath.h"

#include "bn.h"
#include "raytrace.h"
#include "bview/plot3.h"
#include "bview/vlist.h"


struct bv_vlblock *
rt_vlblock_init(void)
{
    return bv_vlblock_init(&RTG.rtg_vlfree, 32);
}

void
rt_vlist_copy(struct bu_list *dest, const struct bu_list *src)
{
    bv_vlist_copy(&RTG.rtg_vlfree, dest, src);
}


void
rt_vlist_cleanup(void)
{
    bv_vlist_cleanup(&RTG.rtg_vlfree);
}

void
rt_vlist_import(struct bu_list *hp, struct bu_vls *namevls, const unsigned char *buf)
{
    bv_vlist_import(&RTG.rtg_vlfree, hp, namevls, buf);
}

#define TBAD	0 /* no such command */
#define TNONE	1 /* no arguments */
#define TSHORT	2 /* Vax 16-bit short */
#define TIEEE	3 /* IEEE 64-bit floating */
#define TCHAR	4 /* unsigned chars */
#define TSTRING	5 /* linefeed terminated string */

struct uplot {
    int targ; /* type of args */
    int narg; /* number or args */
    char desc[14]; /* description */
};
static const struct uplot rt_uplot_error = { 0, 0, "error" };
static const struct uplot rt_uplot_letters[] = {
    /*A*/	{ 0, 0, "" },
    /*B*/	{ 0, 0, "" },
    /*C*/	{ TCHAR, 3, "color" },
    /*D*/	{ 0, 0, "" },
    /*E*/	{ 0, 0, "" },
    /*F*/	{ TNONE, 0, "flush" },
    /*G*/	{ 0, 0, "" },
    /*H*/	{ 0, 0, "" },
    /*I*/	{ 0, 0, "" },
    /*J*/	{ 0, 0, "" },
    /*K*/	{ 0, 0, "" },
    /*L*/	{ TSHORT, 6, "3line" },
    /*M*/	{ TSHORT, 3, "3move" },
    /*N*/	{ TSHORT, 3, "3cont" },
    /*O*/	{ TIEEE, 3, "d_3move" },
    /*P*/	{ TSHORT, 3, "3point" },
    /*Q*/	{ TIEEE, 3, "d_3cont" },
    /*R*/	{ 0, 0, "" },
    /*S*/	{ TSHORT, 6, "3space" },
    /*T*/	{ 0, 0, "" },
    /*U*/	{ 0, 0, "" },
    /*V*/	{ TIEEE, 6, "d_3line" },
    /*W*/	{ TIEEE, 6, "d_3space" },
    /*X*/	{ TIEEE, 3, "d_3point" },
    /*Y*/	{ 0, 0, "" },
    /*Z*/	{ 0, 0, "" },
    /*[*/	{ 0, 0, "" },
    /*\*/	{ 0, 0, "" },
    /*]*/	{ 0, 0, "" },
    /*^*/	{ 0, 0, "" },
    /*_*/	{ 0, 0, "" },
    /*`*/	{ 0, 0, "" },
    /*a*/	{ TSHORT, 6, "arc" },
    /*b*/	{ 0, 0, "" },
    /*c*/	{ TSHORT, 3, "circle" },
    /*d*/	{ 0, 0, "" },
    /*e*/	{ TNONE, 0, "erase" },
    /*f*/	{ TSTRING, 1, "linmod" },
    /*g*/	{ 0, 0, "" },
    /*h*/	{ 0, 0, "" },
    /*i*/	{ TIEEE, 3, "d_circle" },
    /*j*/	{ 0, 0, "" },
    /*k*/	{ 0, 0, "" },
    /*l*/	{ TSHORT, 4, "line" },
    /*m*/	{ TSHORT, 2, "move" },
    /*n*/	{ TSHORT, 2, "cont" },
    /*o*/	{ TIEEE, 2, "d_move" },
    /*p*/	{ TSHORT, 2, "point" },
    /*q*/	{ TIEEE, 2, "d_cont" },
    /*r*/	{ TIEEE, 6, "d_arc" },
    /*s*/	{ TSHORT, 4, "space" },
    /*t*/	{ TSTRING, 1, "label" },
    /*u*/	{ 0, 0, "" },
    /*v*/	{ TIEEE, 4, "d_line" },
    /*w*/	{ TIEEE, 4, "d_space" },
    /*x*/	{ TIEEE, 2, "d_point" },
    /*y*/	{ 0, 0, "" },
    /*z*/	{ 0, 0, "" }
};


/**
 * Read VAX-order 16-bit number
 */
static int
getshort(FILE *fp)
{
    register long v, w;

    v = getc(fp);
    v |= (getc(fp)<<8);	/* order is important! */

    /* worry about sign extension - sigh */
    if (v <= 0x7FFF) return v;
    w = -1;
    w &= ~0x7FFF;
    return w | v;
}


static void
rt_uplot_get_args(FILE *fp, const struct uplot *up, char *carg, fastf_t *arg)
{
    size_t ret;
    int i, j;
    int cc = 0;
    char inbuf[SIZEOF_NETWORK_DOUBLE] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};

    for (i = 0; i < up->narg; i++) {
	switch (up->targ) {
	    case TSHORT:
		arg[i] = getshort(fp);
		break;
	    case TIEEE:
	    {
		double scan;
		ret = fread(inbuf, SIZEOF_NETWORK_DOUBLE, 1, fp);
		if (ret != 1)
		    bu_log("WARNING: uplot read failure\n");
		bu_cv_ntohd((unsigned char *)&scan, (unsigned char *)inbuf, 1);
		arg[i] = scan; /* convert double to fastf_t */
		break;
	    }
	    case TSTRING:
		j = 0;
		while (!feof(fp) &&
		       (cc = getc(fp)) != '\n')
		    carg[j++] = cc;
		carg[j] = '\0';
		break;
	    case TCHAR:
		cc = getc(fp);
		if (cc == EOF)
		    return;

		carg[i] = cc;
		arg[i] = 0;
		break;
	    case TNONE:
	    default:
		arg[i] = 0;	/* ? */
		break;
	}
    }
}


static void
rt_uplot_get_text_args(FILE *fp, const struct uplot *up, char *carg, fastf_t *arg)
{
    int ret;
    int i = 0;
    unsigned int tchar = 0;
    double val;

    for (i = 0; i < up->narg; i++) {
	switch (up->targ) {
	    case TSHORT:
		ret = fscanf(fp, "%lf", &val);
		if (ret != 1)
		    bu_log("WARNING: uplot short input failure\n");
		else
		    arg[i] = val;
		break;
	    case TIEEE:
		ret = fscanf(fp, "%lf", &val);
		if (ret != 1)
		    bu_log("WARNING: uplot floating point input failure\n");
		else
		    arg[i] = val;
		break;
	    case TSTRING:
		ret = fscanf(fp, "%256s\n", &carg[0]);
		if (ret != 1)
		    bu_log("WARNING: uplot string input failure\n");
		break;
	    case TCHAR:
		ret = fscanf(fp, "%u", &tchar);
		if (ret != 1)
		    bu_log("WARNING: uplot character input failure\n");
		if (tchar > 255) tchar = 255;
		carg[i] = tchar;
		arg[i] = 0;
		break;
	    case TNONE:
	    default:
		arg[i] = 0;	/* ? */
		break;
	}
    }
}


int
rt_process_uplot_value(register struct bu_list **vhead,
		       struct bv_vlblock *vbp,
		       FILE *fp,
		       register int c,
		       double char_size,
		       int mode)
{
    mat_t mat;
    const struct uplot *up;
#define CARG_LEN 256
#define ARG_LEN 6
    char carg[CARG_LEN];
    fastf_t arg[ARG_LEN];
    vect_t a, b;
    point_t last_pos;
    static point_t lpnt;		/* last point of a move/draw series */
    static int moved = 0;	/* moved since color change */

    memset(carg, 0, sizeof(char)*CARG_LEN);
    memset(arg, 0, sizeof(fastf_t)*ARG_LEN);
#undef ARG_LEN
#undef CARG_LEN


    /* look it up */
    if (c < 'A' || c > 'z') {
	up = &rt_uplot_error;
    } else {
	up = &rt_uplot_letters[ c - 'A' ];
    }

    if (up->targ == TBAD) {
	fprintf(stderr, "Lee : Bad command '%c' (0x%02x)\n", c, c);
	return -1;
    }

    if (up->narg > 0) {
	if (mode == PL_OUTPUT_MODE_BINARY)
	    rt_uplot_get_args(fp, up, carg, arg);
	else
	    rt_uplot_get_text_args(fp, up, carg, arg);
    }

    switch (c) {
	case 's':
	case 'w':
	case 'S':
	case 'W':
	    /* Space commands, do nothing. */
	    break;
	case 'm':
	case 'o':
	    /* 2-D move */
	    arg[Z] = 0;
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_MOVE);
	    VMOVE(lpnt, arg);
	    moved = 1;
	    break;
	case 'M':
	case 'O':
	    /* 3-D move */
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_MOVE);
	    VMOVE(lpnt, arg);
	    moved = 1;
	    break;
	case 'n':
	case 'q':
	    /*
	     * If no move command was issued since the last color
	     * change, insert one now using the last point from a
	     * move/draw.
	     */
	    if (!moved) {
		BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, lpnt, BV_VLIST_LINE_MOVE);
		moved = 1;
	    }

	    /* 2-D draw */
	    arg[Z] = 0;
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_DRAW);
	    VMOVE(lpnt, arg);
	    break;
	case 'N':
	case 'Q':
	    /*
	     * If no move command was issued since the last color
	     * change, insert one now using the last point from a
	     * move/draw.
	     */
	    if (!moved) {
		BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, lpnt, BV_VLIST_LINE_MOVE);
		moved = 1;
	    }

	    /* 3-D draw */
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_DRAW);
	    VMOVE(lpnt, arg);
	    break;
	case 'l':
	case 'v':
	    /* 2-D line */
	    VSET(a, arg[0], arg[1], 0.0);
	    VSET(b, arg[2], arg[3], 0.0);
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, a, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, b, BV_VLIST_LINE_DRAW);
	    break;
	case 'L':
	case 'V':
	    /* 3-D line */
	    VSET(a, arg[0], arg[1], arg[2]);
	    VSET(b, arg[3], arg[4], arg[5]);
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, a, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, b, BV_VLIST_LINE_DRAW);
	    break;
	case 'p':
	case 'x':
	    /* 2-D point */
	    arg[Z] = 0;
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_DRAW);
	    break;
	case 'P':
	case 'X':
	    /* 3-D point */
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vbp->free_vlist_hd, *vhead, arg, BV_VLIST_LINE_DRAW);
	    break;
	case 'C':
	    /* Color */
	    *vhead = bv_vlblock_find(vbp,
				     carg[0], carg[1], carg[2]);
	    moved = 0;
	    break;
	case 't':
	    /* Text string */
	    MAT_IDN(mat);
	    if (BU_LIST_NON_EMPTY(*vhead)) {
		struct bv_vlist *vlp;
		/* Use coordinates of last op */
		vlp = BU_LIST_LAST(bv_vlist, *vhead);
		VMOVE(last_pos, vlp->pt[vlp->nused-1]);
	    } else {
		VSETALL(last_pos, 0);
	    }
	    bv_vlist_3string(*vhead, vbp->free_vlist_hd, carg, last_pos, mat, char_size);
	    break;
    }

    return 0;
}


int
rt_uplot_to_vlist(struct bv_vlblock *vbp, register FILE *fp, double char_size, int mode)
{
    struct bu_list *vhead;
    register int c;

    vhead = bv_vlblock_find(vbp, 0xFF, 0xFF, 0x00);	/* Yellow */

    while (!feof(fp) && (c=getc(fp)) != EOF) {
	int ret;

	/* pass the address of vhead so it can be updated */
	ret = rt_process_uplot_value(&vhead,
				     vbp,
				     fp,
				     c,
				     char_size,
				     mode);
	if (ret)
	    return ret;
    }

    return 0;
}

void
rt_label_vlist_verts(struct bv_vlblock *vbp, struct bu_list *src, fastf_t *mat, double sz, double mm2local)
{
    struct bv_vlist *vp;
    struct bu_list *vhead;
    char label[256];

    vhead = bv_vlblock_find(vbp, 255, 255, 255);	/* white */

    for (BU_LIST_FOR(vp, bv_vlist, src)) {
	register int i;
	register int nused = vp->nused;
	register int *cmd = vp->cmd;
	register point_t *pt = vp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    /* XXX Skip polygon markers? */
	    sprintf(label, " %g, %g, %g",
		    (*pt)[0]*mm2local, (*pt)[1]*mm2local, (*pt)[2]*mm2local);
	    bv_vlist_3string(vhead, vbp->free_vlist_hd, label, (*pt), mat, sz);
	}
    }
}

void
rt_label_vlist_faces(struct bv_vlblock* vbp, struct bu_list* f_list,
		     fastf_t *mat, double sz, double UNUSED(mm2local) )
{
    struct bu_list* vhead;
    struct face* curr_f;
    char label[256];
    point_t avg_pt;

    vhead = bv_vlblock_find(vbp, 255, 255, 255);    /* white */

    for( BU_LIST_FOR(curr_f, face, f_list) ) {
	avg_pt[0] = (curr_f->min_pt[0] + curr_f->max_pt[0]) / 2;
	avg_pt[1] = (curr_f->min_pt[1] + curr_f->max_pt[1]) / 2;
	avg_pt[2] = (curr_f->min_pt[2] + curr_f->max_pt[2]) / 2;

	sprintf(label, " %d", (int)curr_f->index );
	bv_vlist_3string(vhead, vbp->free_vlist_hd, label, avg_pt, mat, sz);
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
