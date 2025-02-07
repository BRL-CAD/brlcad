/*                         T E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
#include "bresource.h"

#include "bu/app.h"
#include "bu/env.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"


#define V3BASE2LOCAL(_pt)	(_pt)[X]*s->dbip->dbi_base2local, (_pt)[Y]*s->dbip->dbi_base2local, (_pt)[Z]*s->dbip->dbi_base2local
#define V4BASE2LOCAL(_pt)	(_pt)[X]*s->dbip->dbi_base2local, (_pt)[Y]*s->dbip->dbi_base2local, (_pt)[Z]*s->dbip->dbi_base2local, (_pt)[W]*s->dbip->dbi_base2local

/* editors to test, in order of discovery preference (EDITOR overrides) */
#define WIN_EDITOR "\"c:/Program Files/Windows NT/Accessories/wordpad\""
#define MAC_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define EMACS_EDITOR "emacs"
#define VIM_EDITOR "vim"
#define VI_EDITOR "vi"
#define NANO_EDITOR "nano"
#define ED_EDITOR "ed"

/* used to invoke the above editor if X11 is in use */
#define XTERM_COMMAND "xterm"

/* Can the mac terminal be used to launch applications?  Doesn't seem like it
 * in initial trials, but maybe there's some trick? */
#define MAC_BINARY "/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal"

static char tmpfil[MAXPATHLEN] = {0};

/* used in handling different arb types */
static int numUnique = 0;
static int cgtype = 8;
static int uvec[8];
static int svec[11];
static int j;


int writesolid(struct mged_state *), readsolid(struct mged_state *);

int
f_tedit(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    FILE *fp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 1 || 1 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help ted");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* Only do this if in solid edit state */
    if (not_state(s, ST_S_EDIT, "Primitive Text Edit"))
	return TCL_ERROR;

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (fp == NULL)
	return TCL_ERROR;

    if (writesolid(s)) {
	bu_file_delete(tmpfil);
	fclose(fp);
	return TCL_ERROR;
    }

    (void)fclose(fp);

    if (editit(s, argv[0], tmpfil) == TCL_OK) {
	if (readsolid(s)) {
	    bu_file_delete(tmpfil);
	    return TCL_ERROR;
	}

	/* Update the display */
	replot_editing_solid(s);
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
writesolid(struct mged_state *s)
{
    int i;
    FILE *fp;
    char *eol = "\n";

    CHECK_DBI_NULL;

    fp = fopen(tmpfil, "w");

    /* Print solid parameters, 1 vector or point per line */
    switch (s->edit_state.es_int.idb_type) {
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
	struct rt_datum_internal *datum;

	default:
	    Tcl_AppendResult(s->interp, "Cannot text edit this solid type\n", (char *)NULL);
	    (void)fclose(fp);
	    return 1;
	case ID_TOR:
	    tor = (struct rt_tor_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(tor->v), eol);
	    fprintf(fp, "Normal: %.9f %.9f %.9f%s", V3BASE2LOCAL(tor->h), eol);
	    fprintf(fp, "radius_1: %.9f%s", tor->r_a*s->dbip->dbi_base2local, eol);
	    fprintf(fp, "radius_2: %.9f%s", tor->r_h*s->dbip->dbi_base2local, eol);
	    break;
	case ID_TGC:
	case ID_REC:
	    tgc = (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->v), eol);
	    fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->h), eol);
	    fprintf(fp, "A: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->a), eol);
	    fprintf(fp, "B: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->b), eol);
	    fprintf(fp, "C: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->c), eol);
	    fprintf(fp, "D: %.9f %.9f %.9f%s", V3BASE2LOCAL(tgc->d), eol);
	    break;
	case ID_ELL:
	case ID_SPH:
	    ell = (struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->v), eol);
	    fprintf(fp, "A: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->a), eol);
	    fprintf(fp, "B: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->b), eol);
	    fprintf(fp, "C: %.9f %.9f %.9f%s", V3BASE2LOCAL(ell->c), eol);
	    break;
	case ID_ARB8:
	    for (j=0; j<8; j++) uvec[j] = -1;
	    arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	    numUnique = rt_arb_get_cgtype(&cgtype, arb, &s->tol.tol, uvec, svec);
	    j = 0;
	    for (i=0; i<8; i++) {
		if (useThisVertex(i)) {
		    j++;
		    fprintf(fp, "pt[%d]: %.9f %.9f %.9f%s",
				  j, V3BASE2LOCAL(arb->pt[i]), eol);
		}
	    }
	    break;
	case ID_HALF:
	    haf = (struct rt_half_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Plane: %.9f %.9f %.9f %.9f%s", V4BASE2LOCAL(haf->eqn), eol);
	    break;
	case ID_GRIP:
	    grip = (struct rt_grip_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Center: %.9f %.9f %.9f%s", V3BASE2LOCAL(grip->center), eol);
	    fprintf(fp, "Normal: %.9f %.9f %.9f%s", V3BASE2LOCAL(grip->normal), eol);
	    fprintf(fp, "Magnitude: %.9f%s", grip->mag*s->dbip->dbi_base2local, eol);
	    break;
	case ID_PARTICLE:
	    part = (struct rt_part_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(part->part_V), eol);
	    fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(part->part_H), eol);
	    fprintf(fp, "v radius: %.9f%s", part->part_vrad * s->dbip->dbi_base2local, eol);
	    fprintf(fp, "h radius: %.9f%s", part->part_hrad * s->dbip->dbi_base2local, eol);
	    break;
	case ID_RPC:
	    rpc = (struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(rpc->rpc_V), eol);
	    fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(rpc->rpc_H), eol);
	    fprintf(fp, "Breadth: %.9f %.9f %.9f%s", V3BASE2LOCAL(rpc->rpc_B), eol);
	    fprintf(fp, "Half-width: %.9f%s", rpc->rpc_r * s->dbip->dbi_base2local, eol);
	    break;
	case ID_RHC:
	    rhc = (struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(rhc->rhc_V), eol);
	    fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(rhc->rhc_H), eol);
	    fprintf(fp, "Breadth: %.9f %.9f %.9f%s", V3BASE2LOCAL(rhc->rhc_B), eol);
	    fprintf(fp, "Half-width: %.9f%s", rhc->rhc_r * s->dbip->dbi_base2local, eol);
	    fprintf(fp, "Dist_to_asymptotes: %.9f%s", rhc->rhc_c * s->dbip->dbi_base2local, eol);
	    break;
	case ID_EPA:
	    epa = (struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(epa->epa_V), eol);
	    fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(epa->epa_H), eol);
	    fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3ARGS(epa->epa_Au), eol);
	    fprintf(fp, "Semi-major length: %.9f%s", epa->epa_r1 * s->dbip->dbi_base2local, eol);
	    fprintf(fp, "Semi-minor length: %.9f%s", epa->epa_r2 * s->dbip->dbi_base2local, eol);
	    break;
	case ID_EHY:
	    ehy = (struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(ehy->ehy_V), eol);
	    fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(ehy->ehy_H), eol);
	    fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3ARGS(ehy->ehy_Au), eol);
	    fprintf(fp, "Semi-major length: %.9f%s", ehy->ehy_r1 * s->dbip->dbi_base2local, eol);
	    fprintf(fp, "Semi-minor length: %.9f%s", ehy->ehy_r2 * s->dbip->dbi_base2local, eol);
	    fprintf(fp, "Dist to asymptotes: %.9f%s", ehy->ehy_c * s->dbip->dbi_base2local, eol);
	    break;
	case ID_HYP:
	    hyp = (struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(hyp->hyp_Vi), eol);
	    fprintf(fp, "Height: %.9f %.9f %.9f%s", V3BASE2LOCAL(hyp->hyp_Hi), eol);
	    fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3BASE2LOCAL(hyp->hyp_A), eol);
	    fprintf(fp, "Semi-minor length: %.9f%s", hyp->hyp_b * s->dbip->dbi_base2local, eol);
	    fprintf(fp, "Ratio of Neck to Base: %.9f%s", hyp->hyp_bnr, eol);
	    break;
	case ID_ETO:
	    eto = (struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(eto->eto_V), eol);
	    fprintf(fp, "Normal: %.9f %.9f %.9f%s", V3BASE2LOCAL(eto->eto_N), eol);
	    fprintf(fp, "Semi-major axis: %.9f %.9f %.9f%s", V3BASE2LOCAL(eto->eto_C), eol);
	    fprintf(fp, "Semi-minor length: %.9f%s", eto->eto_rd * s->dbip->dbi_base2local, eol);
	    fprintf(fp, "Radius of rotation: %.9f%s", eto->eto_r * s->dbip->dbi_base2local, eol);
	    break;
	case ID_SUPERELL:
	    superell = (struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
	    fprintf(fp, "Vertex: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->v), eol);
	    fprintf(fp, "A: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->a), eol);
	    fprintf(fp, "B: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->b), eol);
	    fprintf(fp, "C: %.9f %.9f %.9f%s", V3BASE2LOCAL(superell->c), eol);
	    fprintf(fp, "<n, e>: <%.9f, %.9f>%s", superell->n, superell->e, eol);
	    break;
	case ID_DATUM:
	    datum = (struct rt_datum_internal *)s->edit_state.es_int.idb_ptr;
	    do {
		if (!ZERO(datum->w))
		    fprintf(fp, "Plane: %.9f %.9f %.9f (pnt) %.9f %.9f %.9f (dir) %.9f (scale)%s", V3BASE2LOCAL(datum->pnt), V3BASE2LOCAL(datum->dir), datum->w, eol);
		else if (!ZERO(MAGNITUDE(datum->dir)))
		    fprintf(fp, "Line: %.9f %.9f %.9f (pnt) %.9f %.9f %.9f (dir)%s", V3BASE2LOCAL(datum->pnt), V3BASE2LOCAL(datum->dir), eol);
		else
		    fprintf(fp, "Point: %.9f %.9f %.9f%s", V3BASE2LOCAL(datum->pnt), eol);
	    } while ((datum = datum->next));

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
readsolid(struct mged_state *s)
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

    switch (s->edit_state.es_int.idb_type) {
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
	struct rt_datum_internal *datum;

	char *str;
	double a, b, c, d, e, f, g;

	default:
	    Tcl_AppendResult(s->interp, "Cannot text edit this solid type\n", (char *)NULL);
	    ret_val = 1;
	    break;
	case ID_TOR:
	    tor = (struct rt_tor_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tor->v, a, b, c);
	    VSCALE(tor->v, tor->v, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tor->h, a, b, c);
	    VUNITIZE(tor->h);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    tor->r_a = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    tor->r_h = a * s->dbip->dbi_local2base;
	    break;
	case ID_TGC:
	case ID_REC:
	    tgc = (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->v, a, b, c);
	    VSCALE(tgc->v, tgc->v, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->h, a, b, c);
	    VSCALE(tgc->h, tgc->h, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->a, a, b, c);
	    VSCALE(tgc->a, tgc->a, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->b, a, b, c);
	    VSCALE(tgc->b, tgc->b, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->c, a, b, c);
	    VSCALE(tgc->c, tgc->c, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(tgc->d, a, b, c);
	    VSCALE(tgc->d, tgc->d, s->dbip->dbi_local2base);

	    break;
	case ID_ELL:
	case ID_SPH:
	    ell = (struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;

	    fprintf(stderr, "ID_SPH\n");

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->v, a, b, c);
	    VSCALE(ell->v, ell->v, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->a, a, b, c);
	    VSCALE(ell->a, ell->a, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->b, a, b, c);
	    VSCALE(ell->b, ell->b, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ell->c, a, b, c);
	    VSCALE(ell->c, ell->c, s->dbip->dbi_local2base);
	    break;
	case ID_ARB8:
	    arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	    for (i=0; i<8; i++) {
		/* only read vertices that we wrote */
		if (useThisVertex(i)) {
		    if ((str=Get_next_line(fp)) == NULL) {
			ret_val = 1;
			break;
		    }
		    sscanf(str, "%lf %lf %lf", &a, &b, &c);
		    VSET(arb->pt[i], a, b, c);
		    VSCALE(arb->pt[i], arb->pt[i], s->dbip->dbi_local2base);
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
	    haf = (struct rt_half_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf %lf", &a, &b, &c, &d);
	    VSET(haf->eqn, a, b, c);
	    haf->eqn[W] = d * s->dbip->dbi_local2base;
	    break;
	case ID_GRIP:
	    grip = (struct rt_grip_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(grip->center, a, b, c);
	    VSCALE(grip->center, grip->center, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(grip->normal, a, b, c);
	    break;
	case ID_PARTICLE:
	    part = (struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(part->part_V, a, b, c);
	    VSCALE(part->part_V, part->part_V, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(part->part_H, a, b, c);
	    VSCALE(part->part_H, part->part_H, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    part->part_vrad = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    part->part_hrad = a * s->dbip->dbi_local2base;

	    break;
	case ID_RPC:
	    rpc = (struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rpc->rpc_V, a, b, c);
	    VSCALE(rpc->rpc_V, rpc->rpc_V, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rpc->rpc_H, a, b, c);
	    VSCALE(rpc->rpc_H, rpc->rpc_H, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rpc->rpc_B, a, b, c);
	    VSCALE(rpc->rpc_B, rpc->rpc_B, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    rpc->rpc_r = a * s->dbip->dbi_local2base;
	    break;
	case ID_RHC:
	    rhc = (struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rhc->rhc_V, a, b, c);
	    VSCALE(rhc->rhc_V, rhc->rhc_V, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rhc->rhc_H, a, b, c);
	    VSCALE(rhc->rhc_H, rhc->rhc_H, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(rhc->rhc_B, a, b, c);
	    VSCALE(rhc->rhc_B, rhc->rhc_B, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    rhc->rhc_r = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    rhc->rhc_c = a * s->dbip->dbi_local2base;
	    break;
	case ID_EPA:
	    epa = (struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(epa->epa_V, a, b, c);
	    VSCALE(epa->epa_V, epa->epa_V, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(epa->epa_H, a, b, c);
	    VSCALE(epa->epa_H, epa->epa_H, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(epa->epa_Au, a, b, c);
	    VUNITIZE(epa->epa_Au);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    epa->epa_r1 = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    epa->epa_r2 = a * s->dbip->dbi_local2base;
	    break;
	case ID_EHY:
	    ehy = (struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ehy->ehy_V, a, b, c);
	    VSCALE(ehy->ehy_V, ehy->ehy_V, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ehy->ehy_H, a, b, c);
	    VSCALE(ehy->ehy_H, ehy->ehy_H, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(ehy->ehy_Au, a, b, c);
	    VUNITIZE(ehy->ehy_Au);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    ehy->ehy_r1 = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    ehy->ehy_r2 = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    ehy->ehy_c = a * s->dbip->dbi_local2base;
	    break;
	case ID_HYP:
	    hyp = (struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(hyp->hyp_Vi, a, b, c);
	    VSCALE(hyp->hyp_Vi, hyp->hyp_Vi, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(hyp->hyp_Hi, a, b, c);
	    VSCALE(hyp->hyp_Hi, hyp->hyp_Hi, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(hyp->hyp_A, a, b, c);
	    VSCALE(hyp->hyp_A, hyp->hyp_A, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    hyp->hyp_b = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    hyp->hyp_bnr = a;

	    break;
	case ID_ETO:
	    eto = (struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(eto->eto_V, a, b, c);
	    VSCALE(eto->eto_V, eto->eto_V, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(eto->eto_N, a, b, c);
	    VUNITIZE(eto->eto_N);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(eto->eto_C, a, b, c);
	    VSCALE(eto->eto_C, eto->eto_C, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    eto->eto_rd = a * s->dbip->dbi_local2base;

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf", &a);
	    eto->eto_r = a * s->dbip->dbi_local2base;
	    break;
	case ID_SUPERELL:
	    superell = (struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;

	    fprintf(stderr, "ID_SUPERELL\n");

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->v, a, b, c);
	    VSCALE(superell->v, superell->v, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->a, a, b, c);
	    VSCALE(superell->a, superell->a, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->b, a, b, c);
	    VSCALE(superell->b, superell->b, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    sscanf(str, "%lf %lf %lf", &a, &b, &c);
	    VSET(superell->c, a, b, c);
	    VSCALE(superell->c, superell->c, s->dbip->dbi_local2base);

	    if ((str=Get_next_line(fp)) == NULL) {
		ret_val = 1;
		break;
	    }
	    (void) sscanf(str, "%lf %lf", &superell->n, &superell->e);
	    break;
	case ID_DATUM:
	    datum = (struct rt_datum_internal *)s->edit_state.es_int.idb_ptr;
	    do {
		if ((str=Get_next_line(fp)) == NULL) {
		    ret_val = 1;
		    break;
		}
		if (bu_strncasecmp(str, "point", strlen("point")) == 0) {
		    sscanf(str, "%lf %lf %lf", &a, &b, &c);
		    VSET(datum->pnt, a, b, c);
		    VSCALE(datum->pnt, datum->pnt, s->dbip->dbi_local2base);
		} else if (bu_strncasecmp(str, "line", strlen("line")) == 0) {
		    sscanf(str, "%lf %lf %lf %lf %lf %lf", &a, &b, &c, &d, &e, &f);
		    VSET(datum->pnt, a, b, c);
		    VSET(datum->dir, d, e, f);
		    VSCALE(datum->pnt, datum->pnt, s->dbip->dbi_local2base);
		    VSCALE(datum->dir, datum->dir, s->dbip->dbi_local2base);
		} else if (bu_strncasecmp(str, "plane", strlen("plane")) == 0) {
		    sscanf(str, "%lf %lf %lf %lf %lf %lf %lf", &a, &b, &c, &d, &e, &f, &g);
		    VSET(datum->pnt, a, b, c);
		    VSET(datum->dir, d, e, f);
		    VSCALE(datum->pnt, datum->pnt, s->dbip->dbi_local2base);
		    VSCALE(datum->dir, datum->dir, s->dbip->dbi_local2base);
		    datum->w = g;
		}
	    } while ((datum = datum->next));

	    break;
    }

    (void)fclose(fp);
    return ret_val;
}


int
get_editor_string(struct mged_state *s, struct bu_vls *editstring)
{
    /* There are two possible situations for MGED - in classic mode
     * the assumption is made that the command window is a controlling
     * terminal, and an editor should be launched that will utilize
     * that controlling window.  In GUI mode, the editor will be launched
     * either as a separate GUI application or in a separate terminal. */
    int need_terminal = 0;
    const char *editor = (char *)NULL;
    const char *editor_opt = (char *)NULL;

    if (s->classic_mged) {
	// Console editors only
	editor = bu_editor(&editor_opt, 1, 0, NULL);
    } else {
	// First try GUI editors only
	editor = bu_editor(&editor_opt, 2, 0, NULL);
	if (!editor) {
	    // Falling back to console, will need terminal
	    need_terminal = 1;
	    editor = bu_editor(&editor_opt, 1, 0, NULL);
	}
    }

    if (!editor) {
	// No suitable editor found
	return 0;
    }

    if (!need_terminal) {
	bu_vls_sprintf(editstring, "(null) (null) %s %s", editor, editor_opt?editor_opt:"(null)");
	return 1;
    }

    // If we do need a terminal, try to find one
    const char *terminal = bu_which(XTERM_COMMAND);
    /* look a little harder if we found nothing */
    if (!terminal) {
	terminal = bu_which("/usr/X11R6/bin/" XTERM_COMMAND);
    }
    if (!terminal) {
	terminal = bu_which("/usr/X11/bin/" XTERM_COMMAND);
    }

    const char *terminal_opt = (char *)NULL;
    if (terminal)
	terminal_opt = "-e";

    bu_vls_sprintf(editstring, "%s %s %s %s", terminal?terminal:"(null)", terminal_opt?terminal_opt:"(null)", editor, editor_opt?editor_opt:"(null)");

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
