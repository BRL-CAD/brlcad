/*                         T E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file mged/tedit.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"


#define V3BASE2LOCAL(_pt)	(_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local
#define V4BASE2LOCAL(_pt)	(_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local, (_pt)[W]*base2local

extern int classic_mged;

/* editors to test, in order of discovery preference (EDITOR overrides) */
#define WIN_EDITOR "\"c:/Program Files/Windows NT/Accessories/wordpad\""
#define MAC_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define EMACS_EDITOR "emacs"
#define JOVE_EDITOR "jove"
#define VIM_EDITOR "vim"
#define VI_EDITOR "vi"
#define NANO_EDITOR "nano"
#define ED_EDITOR "ed"

/* used to invoke the above editor if X11 is in use */
#define XTERM_COMMAND "xterm"

/* Can the mac terminal be used to launch applications?  Doesn't seem like it
 * in initial trials, but maybe there's some trick? */
#define MAC_BINARY "/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal"

extern struct rt_db_internal es_int;
extern struct rt_db_internal es_int_orig;

static char tmpfil[MAXPATHLEN] = {0};

/* used in handling different arb types */
static int numUnique = 0;
static int cgtype = 8;
static int uvec[8];
static int svec[11];
static int j;
			

int writesolid(void), readsolid(void);

int
f_tedit(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    FILE *fp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help ted");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Only do this if in solid edit state */
    if (not_state(ST_S_EDIT, "Primitive Text Edit"))
	return TCL_ERROR;

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (fp == NULL)
	return TCL_ERROR;

    if (writesolid()) {
	bu_file_delete(tmpfil);
	return TCL_ERROR;
    }

    (void)fclose(fp);

    if (editit(argv[0], tmpfil) == TCL_OK) {
	if (readsolid()) {
	    bu_file_delete(tmpfil);
	    return TCL_ERROR;
	}

	/* Update the display */
	replot_editing_solid();
	view_state->vs_flag = 1;
	Tcl_AppendResult(interp, "done\n", (char *)NULL);
    }

    bu_file_delete(tmpfil);

    return TCL_OK;
}


/*
 * given the index of a vertex of the arb currently being edited,
 * return 1 if this vertex should appear in the editor
 * return 0 if this vertex is a duplicate of one of the above
 */
static int
useThisVertex(int idx)
{
    int i;

    for (i=0; i<8 && uvec[i] != -1; i++) {
	if (uvec[i] == idx) return 1;
    }

    if (svec[0] != 0 && idx == svec[2]) return 1;

    if (svec[1] != 0 && idx == svec[2+svec[0]]) return 1;

    return 0;
}


/* Write numerical parameters of a solid into a file */
int
writesolid(void)
{
    int i;
    FILE *fp;
    char *eol = "\n";

    CHECK_DBI_NULL;

    fp = fopen(tmpfil, "w");

    /* Print solid parameters, 1 vector or point per line */
    switch (es_int.idb_type) {
	struct rt_tor_internal *tor;
	struct rt_tgc_internal *tgc;
	struct rt_ell_internal *ell;
	struct rt_arb_internal *arb;
	struct rt_half_internal *haf;
	struct rt_grip_internal *grip;
	struct rt_rpc_internal *rpc;
	struct rt_rhc_internal *rhc;
	struct rt_epa_internal *epa;
	struct rt_ehy_internal *ehy;
	struct rt_hyp_internal *hyp;
	struct rt_eto_internal *eto;
	struct rt_part_internal *part;
	struct rt_superell_internal *superell;

	default:
	    Tcl_AppendResult(INTERP, "Cannot text edit this solid type\n", (char *)NULL);
	    (void)fclose(fp);
	    return 1;
	case ID_TOR:
	    tor = (struct rt_tor_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(tor->v), eol);
	    (void)fprintf(fp, "Normal: %.9f %.9f %.9f%s", V3BASE2LOCAL(tor->h), eol);
	    (void)fprintf(fp, "radius_1: %.9f%s", tor->r_a*base2local, eol);
	    (void)fprintf(fp, "radius_2: %.9f%s", tor->r_h*base2local, eol);
	    break;
	case ID_TGC:
	case ID_REC:
	    tgc = (struct rt_tgc_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->v), eol);
	    (void)fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->h), eol);
	    (void)fprintf(fp, "A: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->a), eol);
	    (void)fprintf(fp, "B: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->b), eol);
	    (void)fprintf(fp, "C: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->c), eol);
	    (void)fprintf(fp, "D: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->d), eol);
	    break;
	case ID_ELL:
	case ID_SPH:
	    ell = (struct rt_ell_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->v), eol);
	    (void)fprintf(fp, "A: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->a), eol);
	    (void)fprintf(fp, "B: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->b), eol);
	    (void)fprintf(fp, "C: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->c), eol);
	    break;
	case ID_ARB8:
	    for (j=0; j<8; j++) uvec[j] = -1;
	    arb = (struct rt_arb_internal *)es_int.idb_ptr;
	    numUnique = rt_arb_get_cgtype(&cgtype, arb, &mged_tol, uvec, svec);
	    j = 0;
	    for (i=0; i<8; i++) {
		if (useThisVertex(i)) {
		    j++;
		    (void)fprintf(fp, "pt[%d]: %.9f %.9f %.9f%s",
				  j, V3BASE2LOCAL(arb->pt[i]), eol);
		}
	    }
	    break;
	case ID_HALF:
	    haf = (struct rt_half_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Plane: %.9f %.9f %.9f %.9f%s", V4BASE2LOCAL(haf->eqn), eol);
	    break;
	case ID_GRIP:
	    grip = (struct rt_grip_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Center: %.9f %.9f %.9f%s", V3BASE2LOCAL(grip->center), eol);
	    (void)fprintf(fp, "Normal: %.9f %.9f %.9f%s", V3BASE2LOCAL(grip->normal), eol);
	    (void)fprintf(fp, "Magnitude: %.9f%s", grip->mag*base2local, eol);
	    break;
	case ID_PARTICLE:
	    part = (struct rt_part_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(part->part_V), eol);
	    (void)fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(part->part_H), eol);
	    (void)fprintf(fp, "v radius: %.9f%s", part->part_vrad * base2local, eol);
	    (void)fprintf(fp, "h radius: %.9f%s", part->part_hrad * base2local, eol);
	    break;
	case ID_RPC:
	    rpc = (struct rt_rpc_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(rpc->rpc_V), eol);
	    (void)fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(rpc->rpc_H), eol);
	    (void)fprintf(fp, "Breadth: %.9f %.9f %.9f%s", V3BASE2LOCAL(rpc->rpc_B), eol);
	    (void)fprintf(fp, "Half-width: %.9f%s", rpc->rpc_r * base2local, eol);
	    break;
	case ID_RHC:
	    rhc = (struct rt_rhc_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(rhc->rhc_V), eol);
	    (void)fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(rhc->rhc_H), eol);
	    (void)fprintf(fp, "Breadth: %.9f %.9f %.9f%s", V3BASE2LOCAL(rhc->rhc_B), eol);
	    (void)fprintf(fp, "Half-width: %.9f%s", rhc->rhc_r * base2local, eol);
	    (void)fprintf(fp, "Dist_to_asymptotes: %.9f%s", rhc->rhc_c * base2local, eol);
	    break;
	case ID_EPA:
	    epa = (struct rt_epa_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(epa->epa_V), eol);
	    (void)fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(epa->epa_H), eol);
	    (void)fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3ARGS(epa->epa_Au), eol);
	    (void)fprintf(fp, "Semi-major length: %.9f%s", epa->epa_r1 * base2local, eol);
	    (void)fprintf(fp, "Semi-minor length: %.9f%s", epa->epa_r2 * base2local, eol);
	    break;
	case ID_EHY:
	    ehy = (struct rt_ehy_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(ehy->ehy_V), eol);
	    (void)fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(ehy->ehy_H), eol);
	    (void)fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3ARGS(ehy->ehy_Au), eol);
	    (void)fprintf(fp, "Semi-major length: %.9f%s", ehy->ehy_r1 * base2local, eol);
	    (void)fprintf(fp, "Semi-minor length: %.9f%s", ehy->ehy_r2 * base2local, eol);
	    (void)fprintf(fp, "Dist to asymptotes: %.9f%s", ehy->ehy_c * base2local, eol);
	    break;
	case ID_HYP:
	    hyp = (struct rt_hyp_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(hyp->hyp_Vi), eol);
	    (void)fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(hyp->hyp_Hi), eol);
	    (void)fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3BASE2LOCAL(hyp->hyp_A), eol);
	    (void)fprintf(fp, "Semi-minor length: %.9f%s", hyp->hyp_b * base2local, eol);
	    (void)fprintf(fp, "Ratio of Neck to Base: %.9f%s", hyp->hyp_bnr, eol);
	    break;
	case ID_ETO:
	    eto = (struct rt_eto_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(eto->eto_V), eol);
	    (void)fprintf(fp, "Normal: %.9f %.9f %.9f%s", V3BASE2LOCAL(eto->eto_N), eol);
	    (void)fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3BASE2LOCAL(eto->eto_C), eol);
	    (void)fprintf(fp, "Semi-minor length: %.9f%s", eto->eto_rd * base2local, eol);
	    (void)fprintf(fp, "Radius of roation: %.9f%s", eto->eto_r * base2local, eol);
	    break;
	case ID_SUPERELL:
	    superell = (struct rt_superell_internal *)es_int.idb_ptr;
	    (void)fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->v), eol);
	    (void)fprintf(fp, "A: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->a), eol);
	    (void)fprintf(fp, "B: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->b), eol);
	    (void)fprintf(fp, "C: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->c), eol);
	    (void)fprintf(fp, "<n, e>: <%.9f, %.9f>%s", superell->n, superell->e, eol);
	    break;
    }

    (void)fclose(fp);
    return 0;
}


static char *
Get_next_line(FILE *fp)
{
    static char line[RT_MAXLINE];
    size_t i;
    size_t len;

    if (bu_fgets(line, sizeof(line), fp) == NULL)
	return (char *)NULL;

    len = strlen(line);

    i = 0;
    while (i<len && line[i++] != ':');

    if (i == len || line[i] == '\0')
	return (char *)NULL;

    return &line[i];
}


/* Read numerical parameters of solid from file */
int
readsolid(void)
{
    int i;
    FILE *fp;
    int ret_val=0;

    CHECK_DBI_NULL;

    fp = fopen(tmpfil, "r");
    if (fp == NULL) {
	perror(tmpfil);
	return 1;	/* FAIL */
    }

    switch (es_int.idb_type) {
	struct rt_tor_internal *tor;
	struct rt_tgc_internal *tgc;
	struct rt_ell_internal *ell;
	struct rt_arb_internal *arb;
	struct rt_half_internal *haf;
	struct rt_grip_internal *grip;
	struct rt_rpc_internal *rpc;
	struct rt_rhc_internal *rhc;
	struct rt_epa_internal *epa;
	struct rt_ehy_internal *ehy;
	struct rt_hyp_internal *hyp;
	struct rt_eto_internal *eto;
	struct rt_part_internal *part;
	struct rt_superell_internal *superell;
	char *str;
	double a, b, c, d;

	default:
	    Tcl_AppendResult(INTERP, "Cannot text edit this solid type\n", (char *)NULL);
	    ret_val = 1;
	    break;
	case ID_TOR:
	    tor = (struct rt_tor_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tor->v, a, b, c);
	    VSCALE(tor->v, tor->v, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tor->h, a, b, c);
	    VUNITIZE(tor->h);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    tor->r_a = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    tor->r_h = a * local2base;
	    break;
	case ID_TGC:
	case ID_REC:
	    tgc = (struct rt_tgc_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->v, a, b, c);
	    VSCALE(tgc->v, tgc->v, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->h, a, b, c);
	    VSCALE(tgc->h, tgc->h, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->a, a, b, c);
	    VSCALE(tgc->a, tgc->a, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->b, a, b, c);
	    VSCALE(tgc->b, tgc->b, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->c, a, b, c);
	    VSCALE(tgc->c, tgc->c, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->d, a, b, c);
	    VSCALE(tgc->d, tgc->d, local2base);

	    break;
	case ID_ELL:
	case ID_SPH:
	    ell = (struct rt_ell_internal *)es_int.idb_ptr;

	    fprintf(stderr, "ID_SPH\n");

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->v, a, b, c);
	    VSCALE(ell->v, ell->v, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->a, a, b, c);
	    VSCALE(ell->a, ell->a, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->b, a, b, c);
	    VSCALE(ell->b, ell->b, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->c, a, b, c);
	    VSCALE(ell->c, ell->c, local2base);
	    break;
	case ID_ARB8:
	    arb = (struct rt_arb_internal *)es_int.idb_ptr;
	    for (i=0; i<8; i++) {
		/* only read vertices that we wrote */
		if (useThisVertex(i)) {
		    if ((str=Get_next_line(fp)) == NULL) {
			ret_val = 1;
			break;
		    }
		    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
		    VSET(arb->pt[i], a, b, c);
		    VSCALE(arb->pt[i], arb->pt[i], local2base);
		}
	    }
	    /* fill in the duplicate vertices
	     * (based on rt_arb_get_cgtype called in writesolid)
	     */
	    if (svec[0] != -1) {
		for (i=1; i<svec[0]; i++) {
		    int start = 2;
		    VMOVE(arb->pt[svec[start+i]], arb->pt[svec[start]]);
		}
	    }
	    if (svec[1] != -1) {
		int start = 2 + svec[0];
		for (i=1; i<svec[1]; i++) {
		    VMOVE(arb->pt[svec[start+i]], arb->pt[svec[start]]);
		}
	    }
	    break;
	case ID_HALF:
	    haf = (struct rt_half_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf %lf", &a, &b, &c, &d);
	    VSET(haf->eqn, a, b, c);
	    haf->eqn[W] = d * local2base;
	    break;
	case ID_GRIP:
	    grip = (struct rt_grip_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(grip->center, a, b, c);
	    VSCALE(grip->center, grip->center, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(grip->normal, a, b, c);
	    break;
	case ID_PARTICLE:
	    part = (struct rt_part_internal *)es_int.idb_ptr;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(part->part_V, a, b, c);
	    VSCALE(part->part_V, part->part_V, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(part->part_H, a, b, c);
	    VSCALE(part->part_H, part->part_H, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    part->part_vrad = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    part->part_hrad = a * local2base;

	    break;
	case ID_RPC:
	    rpc = (struct rt_rpc_internal *)es_int.idb_ptr;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rpc->rpc_V, a, b, c);
	    VSCALE(rpc->rpc_V, rpc->rpc_V, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rpc->rpc_H, a, b, c);
	    VSCALE(rpc->rpc_H, rpc->rpc_H, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rpc->rpc_B, a, b, c);
	    VSCALE(rpc->rpc_B, rpc->rpc_B, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    rpc->rpc_r = a * local2base;
	    break;
	case ID_RHC:
	    rhc = (struct rt_rhc_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rhc->rhc_V, a, b, c);
	    VSCALE(rhc->rhc_V, rhc->rhc_V, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rhc->rhc_H, a, b, c);
	    VSCALE(rhc->rhc_H, rhc->rhc_H, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rhc->rhc_B, a, b, c);
	    VSCALE(rhc->rhc_B, rhc->rhc_B, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    rhc->rhc_r = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    rhc->rhc_c = a * local2base;
	    break;
	case ID_EPA:
	    epa = (struct rt_epa_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(epa->epa_V, a, b, c);
	    VSCALE(epa->epa_V, epa->epa_V, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(epa->epa_H, a, b, c);
	    VSCALE(epa->epa_H, epa->epa_H, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(epa->epa_Au, a, b, c);
	    VUNITIZE(epa->epa_Au);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    epa->epa_r1 = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    epa->epa_r2 = a * local2base;
	    break;
	case ID_EHY:
	    ehy = (struct rt_ehy_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ehy->ehy_V, a, b, c);
	    VSCALE(ehy->ehy_V, ehy->ehy_V, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ehy->ehy_H, a, b, c);
	    VSCALE(ehy->ehy_H, ehy->ehy_H, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ehy->ehy_Au, a, b, c);
	    VUNITIZE(ehy->ehy_Au);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    ehy->ehy_r1 = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    ehy->ehy_r2 = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    ehy->ehy_c = a * local2base;
	    break;
	case ID_HYP:
	    hyp = (struct rt_hyp_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(hyp->hyp_Vi, a, b, c);
	    VSCALE(hyp->hyp_Vi, hyp->hyp_Vi, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(hyp->hyp_Hi, a, b, c);
	    VSCALE(hyp->hyp_Hi, hyp->hyp_Hi, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(hyp->hyp_A, a, b, c);
	    VSCALE(hyp->hyp_A, hyp->hyp_A, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    hyp->hyp_b = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    hyp->hyp_bnr = a;

	    break;
	case ID_ETO:
	    eto = (struct rt_eto_internal *)es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(eto->eto_V, a, b, c);
	    VSCALE(eto->eto_V, eto->eto_V, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(eto->eto_N, a, b, c);
	    VUNITIZE(eto->eto_N);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(eto->eto_C, a, b, c);
	    VSCALE(eto->eto_C, eto->eto_C, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    eto->eto_rd = a * local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf", &a);
	    eto->eto_r = a * local2base;
	    break;
	case ID_SUPERELL:
	    superell = (struct rt_superell_internal *)es_int.idb_ptr;

	    fprintf(stderr, "ID_SUPERELL\n");

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->v, a, b, c);
	    VSCALE(superell->v, superell->v, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->a, a, b, c);
	    VSCALE(superell->a, superell->a, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->b, a, b, c);
	    VSCALE(superell->b, superell->b, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void)sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->c, a, b, c);
	    VSCALE(superell->c, superell->c, local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void) sscanf(str, "%lf %lf", &superell->n, &superell->e);
	    break;
    }

    (void)fclose(fp);
    return ret_val;
}


int
get_editor_string(struct bu_vls *editstring)
{
    char buffer[RT_MAXLINE] = {0};
    int count = 0;
    const char *os = (char *)NULL;
    const char *terminal = (char *)NULL;
    const char *terminal_opt = (char *)NULL;
    const char *editor = (char *)NULL;
    const char *editor_opt = (char *)NULL;

    os = Tcl_GetVar(INTERP, "::tcl_platform(os)", TCL_GLOBAL_ONLY);
    editor = Tcl_GetVar(INTERP, "editor", TCL_GLOBAL_ONLY);
    if (!editor || editor[0] == '\0')
	editor = Tcl_GetVar(INTERP, "EDITOR", TCL_GLOBAL_ONLY);

    if (!editor || editor[0] == '\0')
	editor = getenv("EDITOR");

    /* still unset? try windows */
    if (!editor || editor[0] == '\0') {
	if (BU_STR_EQUAL(os, "Windows 95") || BU_STR_EQUAL(os, "Windows NT")) {
    	    editor = WIN_EDITOR;
	} else {
    	    editor = (char *)NULL;
	}
    }

    /* still unset? try mac os x */
    if (!editor || editor[0] == '\0') {
	if (bu_file_exists(MAC_EDITOR)) {
	    editor = MAC_EDITOR;
	}
    }

    /* still unset? try emacs */
    if (!editor || editor[0] == '\0') {
	editor = bu_which(EMACS_EDITOR);
    }

    /* still unset? try vim */
    if (!editor) {
	editor = bu_which(VIM_EDITOR);
    }

    /* still unset? try vi */
    if (!editor) {
	editor = bu_which(VI_EDITOR);
    }

    /* still unset? try nano */
    if (!editor) {
	editor = bu_which(NANO_EDITOR);
    }

    /* still unset? try ed */
    if (!editor) {
       	editor = bu_which(ED_EDITOR);
    }

    /* still unset? default to jove */
    if (!editor) {
	const char *jovepath = bu_brlcad_root("bin/jove", 1);
	editor = JOVE_EDITOR;
	if (jovepath) {
	    snprintf(buffer, RT_MAXLINE, "%s", jovepath);
	    editor = buffer;
	}
    }

    /* There are two possible situations for MGED - in classic mode
     * the assumption is made that the command window is a controlling
     * terminal, and an editor should be launched that will utilize
     * that controlling window. Otherwise, some editor settings will
     * need a terminal supplied via an xterm and some will not. */


    /* If we're in classic mode on Windows, go with jove */
    if (classic_mged && (BU_STR_EQUAL(os, "Windows 95") || BU_STR_EQUAL(os, "Windows NT"))) {
	const char *jovepath = bu_brlcad_root("bin/jove", 1);
	editor = JOVE_EDITOR;
	if (jovepath) {
	    snprintf(buffer, RT_MAXLINE, "%s", jovepath);
	    editor = buffer;
	}
    }

    if (classic_mged) {
	const char *which = NULL;

	/* In this situation, make sure we're using an editor that will
	 * work within the mged terminal (i.e. no launching a separate
	 * gui, regardless of EDITOR settings. In this situation, emacs
	 * will be invoked with the -nw option.
	 * 
	 * Standard:  emacs, vim, vi, ed, jove
	 * Windows: jove
	 *
	 * terminal and terminal_opt remain unset
	 */

	/* Test for any of the editor conditions that will require intervention.
	 * Unfortunately, because we can't be certain that a user supplied EDITOR
	 * will work in console mode, if it's not one of the known good cases
	 * we have to attempt to set one of the known working editor configs. 
	 * Hence, check for known working AND known not-working up front - need
	 * to satisfy both that there IS a working config already and that one
	 * of the non-working configs isn't set.*/
	which = bu_which(EMACS_EDITOR);
	if (which)
	    count += (BU_STR_EQUAL(editor, which) && (!editor_opt || editor_opt[0] == '\0'));
	which = bu_which(VIM_EDITOR);
	if (which)
	    count += BU_STR_EQUAL(editor, which);
	which = bu_which(VI_EDITOR);
	if (which)
	    count += BU_STR_EQUAL(editor, which);
	which = bu_which(NANO_EDITOR);
	if (which)
	    count += BU_STR_EQUAL(editor, which);
	which = bu_which(ED_EDITOR);
	if (which)
	    count += BU_STR_EQUAL(editor, which);
	count += BU_STR_EQUAL(editor, JOVE_EDITOR);
	count += BU_STR_EQUAL(editor, MAC_EDITOR);
	if (count > 0) {
	    /* start with emacs... */ 
	    editor = bu_which(EMACS_EDITOR);
	    /* if emacs is found, set editor_opt */
	    if (editor) {
		editor_opt = "-nw";
	    }
	    if (!editor) {
		editor = bu_which(VIM_EDITOR);
	    }
	    if (!editor) {
		editor = bu_which(VI_EDITOR);
	    }
	    if (!editor) {
		editor = bu_which(NANO_EDITOR);
	    }
	    if (!editor) {
		editor = bu_which(ED_EDITOR);
	    }
	    if (!editor) {
		const char *binpath = bu_brlcad_root("bin", 1);
		editor = JOVE_EDITOR;
		if (binpath) {
		    snprintf(buffer, RT_MAXLINE, "%s/%s", binpath, editor);
		    editor = buffer;
		}
	    }
	}
    } else { 
    	/* Spell out in which situations we need a terminal.
	 */
    	if (BU_STR_EQUAL(os, "Darwin")) {
    	    /* on the mac, if it's not mac editor assume a terminal is needed. Until
	     * we figure out how to use Mac terminal, use X11 xterm */
    	    if (!BU_STR_EQUAL(editor, MAC_EDITOR)) {
    		terminal = bu_which(XTERM_COMMAND);

		/* look a little harder if we found nothing */
		if (!terminal) {
		    terminal = bu_which("/usr/X11R6/bin/" XTERM_COMMAND);
		}
		if (!terminal) {
		    terminal = bu_which("/usr/X11/bin/" XTERM_COMMAND);
		}

		if (terminal)
		    terminal_opt = "-e";
    	    }
    	}
	
    	/* For now, assume there aren't any situations where Windows will use a terminal */
	
    	/* If it's not mac, and it's not Windows, we need a controlling terminal */
    	if (!BU_STR_EQUAL(os, "Darwin") && !BU_STR_EQUAL(os, "Windows 95") && !BU_STR_EQUAL(os, "Windows NT")) {
    	    if (BU_STR_EQUAL(editor, EMACS_EDITOR)) {
    		terminal = bu_which(XTERM_COMMAND);
		if (terminal)
		    terminal_opt = "-e";
    	    }
    	    if (BU_STR_EQUAL(editor, VIM_EDITOR)) {
    		terminal = bu_which(XTERM_COMMAND);
		if (terminal)
		    terminal_opt = "-e";
    	    }
    	    if (BU_STR_EQUAL(editor, VI_EDITOR)) {
    		terminal = bu_which(XTERM_COMMAND);
		if (terminal)
		    terminal_opt = "-e";
    	    }
	    if (BU_STR_EQUAL(editor, NANO_EDITOR)) {
		    terminal = bu_which(XTERM_COMMAND);
		if (terminal)
		    terminal_opt = "-e";
    	    }
    	    if (BU_STR_EQUAL(editor, ED_EDITOR)) {
    		terminal = bu_which(XTERM_COMMAND);
		if (terminal)
		    terminal_opt = "-e";
    	    }
    	    if (BU_STR_EQUAL(editor, JOVE_EDITOR)) {
    		terminal = bu_which(XTERM_COMMAND);
		if (terminal)
		    terminal_opt = "-e";
    	    }
    	}
    	/* if it's not something we know about, assume no terminal - user can arrange for one if needed */
    }	

    bu_vls_sprintf(editstring, "%s %s %s %s", terminal?terminal:"(null)", terminal_opt?terminal_opt:"(null)", editor?editor:"(null)", editor_opt?editor_opt:"(null)"); 

    return 1;
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
