/*                         T E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
#include "./primitives/mged_functab.h"


#define V3BASE2LOCAL(_pt)	(_pt)[X]*s->dbip->dbi_base2local, (_pt)[Y]*s->dbip->dbi_base2local, (_pt)[Z]*s->dbip->dbi_base2local

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
static int uvec[8];
static int svec[11];
static int cgtype = 8;

int writesolid(struct mged_state *), readsolid(struct mged_state *);

/*
 *
 * No-frills edit - opens an editor on the supplied
 * file name.
 *
 */
int
editit(struct mged_state *s, const char *tempfile) {
    int argc = 5;
    const char *av[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    struct bu_vls editstring = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    if (!get_editor_string(s, &editstring))
	return TCL_ERROR;

    av[0] = "editit";
    av[1] = "-e";
    av[2] = bu_vls_addr(&editstring);
    av[3] = "-f";
    av[4] = tempfile;
    av[5] = NULL;

    ged_exec(s->gedp, argc, (const char **)av);

    bu_vls_free(&editstring);
    return TCL_OK;
}

int
f_tedit(ClientData clientData, Tcl_Interp *interp, int argc, const char **UNUSED(argv))
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

    if (editit(s, tmpfil) == TCL_OK) {
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
    FILE *fp;

    CHECK_DBI_NULL;

    struct rt_db_internal *ip = &s->edit_state.es_int;
    if (!MGED_OBJ[ip->idb_type].ft_write_params) {
	Tcl_AppendResult(s->interp, "Cannot text edit this solid type\n", (char *)NULL);
	return 1;
    }

    struct bu_vls params = BU_VLS_INIT_ZERO;
    (*MGED_OBJ[ip->idb_type].ft_write_params)(&params, ip, &s->tol.tol, s->dbip->dbi_base2local);
    fp = fopen(tmpfil, "w");
    fprintf(fp, "%s", bu_vls_cstr(&params));
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

    struct rt_db_internal *ip = &s->edit_state.es_int;

    if (MGED_OBJ[ip->idb_type].ft_read_params) {
	struct bu_vls solid_in = BU_VLS_INIT_ZERO;
	struct bu_mapped_file *mf = bu_open_mapped_file(tmpfil, (char *)NULL);
	if (!mf) {
	    bu_log("cannot read temporary file \"%s\"\n", tmpfil);
	    return 1;	/* FAIL */
	}
	bu_vls_strncpy(&solid_in, (char *)mf->buf, mf->buflen);
	bu_close_mapped_file(mf);

	if ((*MGED_OBJ[ip->idb_type].ft_read_params)(ip, bu_vls_cstr(&solid_in), s->dbip->dbi_local2base) == BRLCAD_ERROR) {
	    bu_vls_free(&solid_in);
	    return 1;   /* FAIL */
	}

	bu_vls_free(&solid_in);
	return 0;
    }

    fp = fopen(tmpfil, "r");
    if (fp == NULL) {
	perror(tmpfil);
	return 1;	/* FAIL */
    }

    switch (s->edit_state.es_int.idb_type) {
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
	case ID_ARB8:
	    arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	    rt_arb_get_cgtype(&cgtype, arb, &s->tol.tol, uvec, svec);
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
