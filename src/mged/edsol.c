/*                         E D S O L . C
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
/** @file mged/edsol.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/primitives/arb8.h"
#include "wdb.h"
#include "rt/db4.h"
#include "ged/view.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"
#include "./menu.h"

extern void pipe_split_pnt(struct bu_list *, struct wdb_pipe_pnt *, point_t);
extern struct wdb_pipe_pnt *pipe_add_pnt(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);

static void init_sedit_vars(struct mged_state *), init_oedit_vars(struct mged_state *), init_oedit_guts(struct mged_state *);

int nurb_closest2d(int *surface, int *uval, int *vval, const struct rt_nurb_internal *spl, const point_t ref_pt  , const mat_t mat);

/* Ew. Globals. */
short int fixv;		/* used in ECMD_ARB_ROTATE_FACE, f_eqn(): fixed vertex */

/* data for solid editing */
int sedraw;	/* apply solid editing changes */

int es_edflag;		/* type of editing for this solid */
fastf_t es_peqn[7][4];		/* ARBs defining plane equations */
fastf_t es_m[3];		/* edge(line) slope */

int bot_verts[3];		/* vertices for the BOT solid */

static int spl_surfno;	/* What surf & ctl pt to edit on spline */
static int spl_ui;
static int spl_vi;

static int es_ars_crv;	/* curve and column identifying selected ARS point */
static int es_ars_col;
static point_t es_pt;		/* coordinates of selected ARS point */

struct edgeuse *es_eu=(struct edgeuse *)NULL;	/* Currently selected NMG edgeuse */
struct loopuse *lu_copy=(struct loopuse*)NULL;	/* copy of loop to be extruded */
plane_t lu_pl;	/* plane equation for loop to be extruded */
struct shell *es_s=(struct shell *)NULL;	/* Shell where extrusion is to end up */
point_t lu_keypoint;	/* keypoint of lu_copy for extrusion */

struct wdb_pipe_pnt *es_pipe_pnt=(struct wdb_pipe_pnt *)NULL; /* Currently selected PIPE segment */
struct wdb_metaball_pnt *es_metaball_pnt=(struct wdb_metaball_pnt *)NULL; /* Currently selected METABALL Point */

/* These values end up in es_menu, as do ARB vertex numbers */
int es_menu;		/* item selected from menu */

#define PARAM_1ARG (es_edflag == SSCALE || \
		    es_edflag == PSCALE || \
		    es_edflag == ECMD_BOT_THICK || \
		    es_edflag == ECMD_VOL_THRESH_LO || \
		    es_edflag == ECMD_VOL_THRESH_HI || \
		    es_edflag == ECMD_DSP_SCALE_X || \
		    es_edflag == ECMD_DSP_SCALE_Y || \
		    es_edflag == ECMD_DSP_SCALE_ALT || \
		    es_edflag == ECMD_EBM_HEIGHT || \
		    es_edflag == ECMD_CLINE_SCALE_H || \
		    es_edflag == ECMD_CLINE_SCALE_R || \
		    es_edflag == ECMD_CLINE_SCALE_T || \
		    es_edflag == ECMD_EXTR_SCALE_H)
#define PARAM_2ARG (es_edflag == ECMD_DSP_FSIZE || \
		    es_edflag == ECMD_EBM_FSIZE)

void
set_e_axes_pos(struct mged_state *s, int both)
    /* if (!both) then set only s->edit_state.curr_e_axes_pos, otherwise
       set e_axes_pos and s->edit_state.curr_e_axes_pos */
{
    int i;
    const short earb8[12][18] = earb8_edit_array;
    const short earb7[12][18] = earb7_edit_array;
    const short earb6[10][18] = earb6_edit_array;
    const short earb5[9][18] = earb5_edit_array;
    const int local_arb_faces[5][24] = rt_arb_faces;

    s->update_views = 1;
    dm_set_dirty(DMP, 1);
    switch (s->edit_state.es_int.idb_type) {
	case ID_ARB8:
	    if (s->edit_state.global_editing_state == ST_O_EDIT) {
		i = 0;
	    } else {
		switch (es_edflag) {
		    case STRANS:
			i = 0;
			break;
		    case EARB:
			switch (s->edit_state.e_type) {
			    case ARB5:
				i = earb5[es_menu][0];
				break;
			    case ARB6:
				i = earb6[es_menu][0];
				break;
			    case ARB7:
				i = earb7[es_menu][0];
				break;
			    case ARB8:
				i = earb8[es_menu][0];
				break;
			    default:
				i = 0;
				break;
			}
			break;
		    case PTARB:
			switch (s->edit_state.e_type) {
			    case ARB4:
				i = es_menu;	/* index for point 1, 2, 3 or 4 */
				break;
			    case ARB5:
			    case ARB7:
				i = 4;	/* index for point 5 */
				break;
			    case ARB6:
				i = es_menu;	/* index for point 5 or 6 */
				break;
			    default:
				i = 0;
				break;
			}
			break;
		    case ECMD_ARB_MOVE_FACE:
			switch (s->edit_state.e_type) {
			    case ARB4:
				i = local_arb_faces[0][es_menu * 4];
				break;
			    case ARB5:
				i = local_arb_faces[1][es_menu * 4];
				break;
			    case ARB6:
				i = local_arb_faces[2][es_menu * 4];
				break;
			    case ARB7:
				i = local_arb_faces[3][es_menu * 4];
				break;
			    case ARB8:
				i = local_arb_faces[4][es_menu * 4];
				break;
			    default:
				i = 0;
				break;
			}
			break;
		    case ECMD_ARB_ROTATE_FACE:
			i = fixv;
			break;
		    default:
			i = 0;
			break;
		}
	    }
	    MAT4X3PNT(s->edit_state.curr_e_axes_pos, s->edit_state.e_mat,
		      ((struct rt_arb_internal *)s->edit_state.es_int.idb_ptr)->pt[i]);
	    break;
	case ID_TGC:
	case ID_REC:
	    if (es_edflag == ECMD_TGC_MV_H ||
		es_edflag == ECMD_TGC_MV_HH) {
		struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		point_t tgc_v;
		vect_t tgc_h;

		MAT4X3PNT(tgc_v, s->edit_state.e_mat, tgc->v);
		MAT4X3VEC(tgc_h, s->edit_state.e_mat, tgc->h);
		VADD2(s->edit_state.curr_e_axes_pos, tgc_h, tgc_v);
	    } else {
		VMOVE(s->edit_state.curr_e_axes_pos, s->edit_state.e_keypoint);
	    }

	    break;
	case ID_EXTRUDE:
	    if (es_edflag == ECMD_EXTR_MOV_H) {
		struct rt_extrude_internal *extr = (struct rt_extrude_internal *)s->edit_state.es_int.idb_ptr;
		point_t extr_v;
		vect_t extr_h;

		RT_EXTRUDE_CK_MAGIC(extr);

		MAT4X3PNT(extr_v, s->edit_state.e_mat, extr->V);
		MAT4X3VEC(extr_h, s->edit_state.e_mat, extr->h);
		VADD2(s->edit_state.curr_e_axes_pos, extr_h, extr_v);
	    } else {
		VMOVE(s->edit_state.curr_e_axes_pos, s->edit_state.e_keypoint);
	    }

	    break;
	case ID_CLINE:
	    if (es_edflag == ECMD_CLINE_MOVE_H) {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;
		point_t cli_v;
		vect_t cli_h;

		RT_CLINE_CK_MAGIC(cli);

		MAT4X3PNT(cli_v, s->edit_state.e_mat, cli->v);
		MAT4X3VEC(cli_h, s->edit_state.e_mat, cli->h);
		VADD2(s->edit_state.curr_e_axes_pos, cli_h, cli_v);
	    } else {
		VMOVE(s->edit_state.curr_e_axes_pos, s->edit_state.e_keypoint);
	    }

	    break;
	default:
	    VMOVE(s->edit_state.curr_e_axes_pos, s->edit_state.e_keypoint);
	    break;
    }

    if (both) {
	VMOVE(s->edit_state.e_axes_pos, s->edit_state.curr_e_axes_pos);

	if (EDIT_ROTATE) {
	    s->edit_state.e_edclass = EDIT_CLASS_ROTATE;
	    VSETALL(s->edit_state.k.rot_m_abs, 0.0);
	    VSETALL(s->edit_state.k.rot_o_abs, 0.0);
	    VSETALL(s->edit_state.k.rot_v_abs, 0.0);
	    VSETALL(s->edit_state.k.rot_m_abs_last, 0.0);
	    VSETALL(s->edit_state.k.rot_o_abs_last, 0.0);
	    VSETALL(s->edit_state.k.rot_v_abs_last, 0.0);
	} else if (EDIT_TRAN) {
	    s->edit_state.e_edclass = EDIT_CLASS_TRAN;
	    VSETALL(s->edit_state.k.tra_m_abs, 0.0);
	    VSETALL(s->edit_state.k.tra_v_abs, 0.0);
	    VSETALL(s->edit_state.k.tra_m_abs_last, 0.0);
	    VSETALL(s->edit_state.k.tra_v_abs_last, 0.0);
	} else if (EDIT_SCALE) {
	    s->edit_state.e_edclass = EDIT_CLASS_SCALE;

	    if (SEDIT_SCALE) {
		s->edit_state.k.sca_abs = 0.0;
		s->edit_state.acc_sc_sol = 1.0;
	    }
	} else {
	    s->edit_state.e_edclass = EDIT_CLASS_NULL;
	}

	MAT_IDN(s->edit_state.acc_rot_sol);

	for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	    struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	    m_dmp->dm_mged_variables->mv_transform = 'e';
	}
    }
}

/*
 * Keypoint in model space is established in "pt".
 * If "str" is set, then that point is used, else default
 * for this solid is selected and set.
 * "str" may be a constant string, in either upper or lower case,
 * or it may be something complex like "(3, 4)" for an ARS or spline
 * to select a particular vertex or control point.
 *
 * XXX Perhaps this should be done via solid-specific parse tables,
 * so that solids could be pretty-printed & structprint/structparse
 * processed as well?
 */
void
get_solid_keypoint(struct mged_state *s, point_t *pt, const char **strp, struct rt_db_internal *ip, fastf_t *mat)
{
    static const char *vert_str = "V";
    const char *cp = *strp;
    point_t mpt = VINIT_ZERO;
    static char buf[BUFSIZ];

    RT_CK_DB_INTERNAL(ip);
    memset(buf, 0, BUFSIZ);

    switch (ip->idb_type) {
	
	case ID_PIPE:
	    {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)ip->idb_ptr;
		struct wdb_pipe_pnt *pipe_seg;

		RT_PIPE_CK_MAGIC(pipeip);

		if (es_pipe_pnt == (struct wdb_pipe_pnt *)NULL) {
		    pipe_seg = BU_LIST_FIRST(wdb_pipe_pnt, &pipeip->pipe_segs_head);
		    VMOVE(mpt, pipe_seg->pp_coord);
		} else {
		    VMOVE(mpt, es_pipe_pnt->pp_coord);
		}
		*strp = "V";
		MAT4X3PNT(*pt, mat, mpt);
		return;
	    }
	case ID_METABALL:
	    {
		struct rt_metaball_internal *metaball = (struct rt_metaball_internal *)ip->idb_ptr;

		RT_METABALL_CK_MAGIC(metaball);

		VSETALL(mpt, 0.0);
		if (es_metaball_pnt==NULL) {
		    snprintf(buf, BUFSIZ, "no point selected");
		} else {
		    VMOVE(mpt, es_metaball_pnt->coord);
		    snprintf(buf, BUFSIZ, "V %f", es_metaball_pnt->fldstr);
		}
		*strp = buf;
		MAT4X3PNT(*pt, mat, mpt);
		return;
	    }
	case ID_BOT:
	    {
		*strp = OBJ[ip->idb_type].ft_keypoint(pt, cp, mat, ip, &s->tol.tol);
		// If we're editing, use that position instead
		if (bot_verts[0] > -1) {
		    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
		    RT_BOT_CK_MAGIC(bot);
		    VMOVE(mpt, &bot->vertices[bot_verts[0]*3]);
		    MAT4X3PNT(*pt, mat, mpt);
		}
		return;
	    }
	case ID_ARB8:
	    if (*cp == 'V') {
		*strp = OBJ[ip->idb_type].ft_keypoint(pt, cp, mat, ip, &s->tol.tol);
	    } else {
		static const char *vstr = "V1";
		*strp = OBJ[ip->idb_type].ft_keypoint(pt, vstr, mat, ip, &s->tol.tol);
	    }
	    return;
	case ID_BSPLINE:
	    {
		struct rt_nurb_internal *sip =
		    (struct rt_nurb_internal *) s->edit_state.es_int.idb_ptr;
		struct face_g_snurb *surf;
		fastf_t *fp;

		RT_NURB_CK_MAGIC(sip);
		surf = sip->srfs[spl_surfno];
		NMG_CK_SNURB(surf);
		fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
		VMOVE(mpt, fp);
		sprintf(buf, "Surf %d, index %d,%d",
			spl_surfno, spl_ui, spl_vi);
		*strp = buf;
		MAT4X3PNT(*pt, mat, mpt);
		return;
	    }
	case ID_GRIP:
	    {
		*strp = OBJ[ip->idb_type].ft_keypoint(pt, cp, mat, ip, &s->tol.tol);
		if (!*strp) {
		    static const char *c_str = "C";
		    *strp = OBJ[ip->idb_type].ft_keypoint(pt, c_str, mat, ip, &s->tol.tol);
		}
		return;
	    }
	case ID_ARS:
	    {
		struct rt_ars_internal *ars =
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    VMOVE(mpt, es_pt);
		} else {
		    VMOVE(mpt, &ars->curves[es_ars_crv][es_ars_col*3]);
		}

		MAT4X3PNT(*pt, mat, mpt);
		*strp = "V";
		return;
	    }
	case ID_EXTRUDE:
	    {
		struct rt_extrude_internal *extr = (struct rt_extrude_internal *)ip->idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);
		if (extr->skt && extr->skt->verts) {
		    static const char *vstr = "V1";
		    *strp = OBJ[ip->idb_type].ft_keypoint(pt, vstr, mat, ip, &s->tol.tol);
		} else {
		    *strp = OBJ[ip->idb_type].ft_keypoint(pt, NULL, mat, ip, &s->tol.tol);
		}
		return;
	    }
	case ID_NMG:
	    {
		struct vertex *v;
		struct vertexuse *vu;
		struct edgeuse *eu;
		struct loopuse *lu;
		struct faceuse *fu;
		struct shell *nmg_s;
		struct nmgregion *r;
		struct model *m =
		    (struct model *) s->edit_state.es_int.idb_ptr;
		NMG_CK_MODEL(m);
		/* XXX Fall through, for now (How about first vertex?? - JRA) */

		/* set default first */
		VSETALL(mpt, 0.0);
		*strp = "(origin)";

		/* XXX Try to use the first point of the selected edge */
		if (es_eu != (struct edgeuse *)NULL &&
		    es_eu->vu_p != (struct vertexuse *)NULL &&
		    es_eu->vu_p->v_p != (struct vertex *)NULL &&
		    es_eu->vu_p->v_p->vg_p != (struct vertex_g *)NULL) {
		    VMOVE(mpt, es_eu->vu_p->v_p->vg_p->coord);
		    break;
		}

		if (BU_LIST_IS_EMPTY(&m->r_hd))
		    break;

		r = BU_LIST_FIRST(nmgregion, &m->r_hd);
		if (!r)
		    break;
		NMG_CK_REGION(r);

		if (BU_LIST_IS_EMPTY(&r->s_hd))
		    break;

		nmg_s = BU_LIST_FIRST(shell, &r->s_hd);
		if (!s)
		    break;
		NMG_CK_SHELL(nmg_s);

		if (BU_LIST_IS_EMPTY(&nmg_s->fu_hd))
		    fu = (struct faceuse *)NULL;
		else
		    fu = BU_LIST_FIRST(faceuse, &nmg_s->fu_hd);
		if (fu) {
		    NMG_CK_FACEUSE(fu);
		    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		    NMG_CK_LOOPUSE(lu);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
			NMG_CK_EDGEUSE(eu);
			NMG_CK_VERTEXUSE(eu->vu_p);
			v = eu->vu_p->v_p;
		    } else {
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			NMG_CK_VERTEXUSE(vu);
			v = vu->v_p;
		    }
		    NMG_CK_VERTEX(v);
		    if (!v->vg_p)
			break;
		    VMOVE(mpt, v->vg_p->coord);
		    *strp = "V";
		    break;
		}
		if (BU_LIST_IS_EMPTY(&nmg_s->lu_hd))
		    lu = (struct loopuse *)NULL;
		else
		    lu = BU_LIST_FIRST(loopuse, &nmg_s->lu_hd);
		if (lu) {
		    NMG_CK_LOOPUSE(lu);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
			eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
			NMG_CK_EDGEUSE(eu);
			NMG_CK_VERTEXUSE(eu->vu_p);
			v = eu->vu_p->v_p;
		    } else {
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			NMG_CK_VERTEXUSE(vu);
			v = vu->v_p;
		    }
		    NMG_CK_VERTEX(v);
		    if (!v->vg_p)
			break;
		    VMOVE(mpt, v->vg_p->coord);
		    *strp = "V";
		    break;
		}
		if (BU_LIST_IS_EMPTY(&nmg_s->eu_hd))
		    eu = (struct edgeuse *)NULL;
		else
		    eu = BU_LIST_FIRST(edgeuse, &nmg_s->eu_hd);
		if (eu) {
		    NMG_CK_EDGEUSE(eu);
		    NMG_CK_VERTEXUSE(eu->vu_p);
		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    if (!v->vg_p)
			break;
		    VMOVE(mpt, v->vg_p->coord);
		    *strp = "V";
		    break;
		}
		vu = nmg_s->vu_p;
		if (vu) {
		    NMG_CK_VERTEXUSE(vu);
		    v = vu->v_p;
		    NMG_CK_VERTEX(v);
		    if (!v->vg_p)
			break;
		    VMOVE(mpt, v->vg_p->coord);
		    *strp = "V";
		    break;
		}
		break;
	    }
	case ID_CLINE:
	case ID_PARTICLE:
	case ID_ARBN:
	case ID_EBM:
	case ID_DSP:
	case ID_HF:
	case ID_VOL:
	case ID_HALF:
	case ID_ELL:
	case ID_SPH:
	case ID_SUPERELL:
	case ID_TOR:
	case ID_TGC:
	case ID_REC:
	case ID_RPC:
	case ID_RHC:
	case ID_EPA:
	case ID_EHY:
	case ID_HYP:
	case ID_ETO:
	case ID_POLY:
	case ID_SKETCH:
	case ID_ANNOT:
	case ID_DATUM:
	    *strp = OBJ[ip->idb_type].ft_keypoint(pt, cp, mat, ip, &s->tol.tol);
	    if (!*strp)
		*strp = OBJ[ip->idb_type].ft_keypoint(pt, vert_str, mat, ip, &s->tol.tol);
	    return;
	    /* fall through */
	default:
	    Tcl_AppendResult(s->interp, "get_solid_keypoint: unrecognized solid type (setting keypoint to origin)\n", (char *)NULL);
	    VSETALL(mpt, 0.0);
	    *strp = "(origin)";
	    break;
    }

    // Most of the time this is handled, but if it hasn't been yet do the mat calculation
    MAT4X3PNT(*pt, mat, mpt);
}


int
f_get_solid_keypoint(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    if (s->edit_state.global_editing_state == ST_VIEW || s->edit_state.global_editing_state == ST_S_PICK || s->edit_state.global_editing_state == ST_O_PICK)
	return TCL_OK;

    get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);
    return TCL_OK;
}


/*
 * First time in for this solid, set things up.
 * If all goes well, change state to ST_S_EDIT.
 * Solid editing is completed only via sedit_accept() / sedit_reject().
 */
void
init_sedit(struct mged_state *s)
{
    int type;
    int id;

    if (s->dbip == DBI_NULL || !illump)
	return;

    /*
     * Check for a processed region or other illegal solid.
     */
    if (illump->s_old.s_Eflag) {
	Tcl_AppendResult(s->interp,
			 "Unable to Solid_Edit a processed region;  select a primitive instead\n", (char *)NULL);
	return;
    }

    /* Read solid description into s->edit_state.es_int */
    if (!illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(s->interp, "init_sedit(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(s->interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);
	}
	rt_db_free_internal(&s->edit_state.es_int);
	return;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->edit_state.es_int);
    id = s->edit_state.es_int.idb_type;

    es_menu = 0;
    if (id == ID_ARB8) {
	struct rt_arb_internal *arb;
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	type = rt_arb_std_type(&s->edit_state.es_int, &s->tol.tol);
	s->edit_state.e_type = type;

	if (rt_arb_calc_planes(&error_msg, arb, s->edit_state.e_type, es_peqn, &s->tol.tol)) {
	    Tcl_AppendResult(s->interp, bu_vls_addr(&error_msg),
			     "\nCannot calculate plane equations for ARB8\n",
			     (char *)NULL);
	    rt_db_free_internal(&s->edit_state.es_int);
	    bu_vls_free(&error_msg);
	    return;
	}
	bu_vls_free(&error_msg);
    } else if (id == ID_BSPLINE) {
	struct rt_nurb_internal *sip =
	    (struct rt_nurb_internal *) s->edit_state.es_int.idb_ptr;
	struct face_g_snurb *surf;
	RT_NURB_CK_MAGIC(sip);
	spl_surfno = sip->nsrf/2;
	surf = sip->srfs[spl_surfno];
	NMG_CK_SNURB(surf);
	spl_ui = surf->s_size[1]/2;
	spl_vi = surf->s_size[0]/2;
    }

    /* Save aggregate path matrix */
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, s->edit_state.e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->edit_state.e_invmat, s->edit_state.e_mat);

    /* Establish initial keypoint */
    s->edit_state.e_keytag = "";
    get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
    lu_copy = (struct loopuse *)NULL;
    es_ars_crv = (-1);
    es_ars_col = (-1);

    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;

    /* Finally, enter solid edit state */
    (void)chg_state(s, ST_S_PICK, ST_S_EDIT, "Keyboard illuminate");
    chg_l2menu(s, ST_S_EDIT);
    es_edflag = IDLE;

    button(s, BE_S_EDIT);	/* Drop into edit menu right away */
    init_sedit_vars(s);

    {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&vls, "begin_edit_callback ");
	db_path_to_vls(&vls, &bdata->s_fullpath);
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
}


static void
init_sedit_vars(struct mged_state *s)
{
    MAT_IDN(s->edit_state.acc_rot_sol);
    MAT_IDN(s->edit_state.incr_change);

    bv_knobs_reset(&s->edit_state.k, BV_KNOBS_ALL);

    s->edit_state.acc_sc_sol = 1.0;

    set_e_axes_pos(s, 1);
}


/*
 * All solid edit routines call this subroutine after
 * making a change to es_int or s->edit_state.e_mat.
 */
void
replot_editing_solid(struct mged_state *s)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    mat_t mat;
    struct bv_scene_obj *sp;
    struct directory *illdp;

    if (!illump) {
	return;
    }
    if (!illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    illdp = LAST_SOLID(bdata);

    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (sp->s_u_data) {
		bdata = (struct ged_bv_data *)sp->s_u_data;
		if (LAST_SOLID(bdata) == illdp) {
		    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);
		    (void)replot_modified_solid(s, sp, &s->edit_state.es_int, mat);
		}
	    }
	}

	gdlp = next_gdlp;
    }
}


void
transform_editing_solid(
    struct mged_state *s,
    struct rt_db_internal *os,		/* output solid */
    const mat_t mat,
    struct rt_db_internal *is,		/* input solid */
    int freeflag)
{
    if (rt_matrix_transform(os, mat, is, freeflag, s->dbip, &rt_uniresource) < 0)
	bu_exit(EXIT_FAILURE, "transform_editing_solid failed to apply a matrix transform, aborting");
}


/*
 * Put up menu header
 */
void
sedit_menu(struct mged_state *s) {

    menu_state->ms_flag = 0;		/* No menu item selected yet */

    mmenu_set_all(s, MENU_L1, NULL);
    chg_l2menu(s, ST_S_EDIT);

    switch (s->edit_state.es_int.idb_type) {

	case ID_ARB8:
	    mmenu_set_all(s, MENU_L1, cntrl_menu);
	    break;
	case ID_TGC:
	    mmenu_set_all(s, MENU_L1, tgc_menu);
	    break;
	case ID_TOR:
	    mmenu_set_all(s, MENU_L1, tor_menu);
	    break;
	case ID_ELL:
	    mmenu_set_all(s, MENU_L1, ell_menu);
	    break;
	case ID_SUPERELL:
	    mmenu_set_all(s, MENU_L1, superell_menu);
	    break;
	case ID_ARS:
	    mmenu_set_all(s, MENU_L1, ars_menu);
	    break;
	case ID_BSPLINE:
	    mmenu_set_all(s, MENU_L1, spline_menu);
	    break;
	case ID_RPC:
	    mmenu_set_all(s, MENU_L1, rpc_menu);
	    break;
	case ID_RHC:
	    mmenu_set_all(s, MENU_L1, rhc_menu);
	    break;
	case ID_EPA:
	    mmenu_set_all(s, MENU_L1, epa_menu);
	    break;
	case ID_EHY:
	    mmenu_set_all(s, MENU_L1, ehy_menu);
	    break;
	case ID_HYP:
	    mmenu_set_all(s, MENU_L1, hyp_menu);
	    break;
	case ID_ETO:
	    mmenu_set_all(s, MENU_L1, eto_menu);
	    break;
	case ID_NMG:
	    mmenu_set_all(s, MENU_L1, nmg_menu);
	    break;
	case ID_PIPE:
	    mmenu_set_all(s, MENU_L1, pipe_menu);
	    break;
	case ID_METABALL:
	    mmenu_set_all(s, MENU_L1, metaball_menu);
	    break;
	case ID_VOL:
	    mmenu_set_all(s, MENU_L1, vol_menu);
	    break;
	case ID_EBM:
	    mmenu_set_all(s, MENU_L1, ebm_menu);
	    break;
	case ID_DSP:
	    mmenu_set_all(s, MENU_L1, dsp_menu);
	    break;
	case ID_PARTICLE:
	    mmenu_set_all(s, MENU_L1, part_menu);
	    break;
	case ID_BOT:
	    mmenu_set_all(s, MENU_L1, bot_menu);
	    break;
	case ID_EXTRUDE:
	    mmenu_set_all(s, MENU_L1, extr_menu);
	    break;
	case ID_CLINE:
	    mmenu_set_all(s, MENU_L1, cline_menu);
	    break;
    }
    es_edflag = IDLE;	/* Drop out of previous edit mode */
    es_menu = 0;
}


int
get_rotation_vertex(struct mged_state *s)
{
    int i, j;
    int type, loc, valid;
    int vertex = -1;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls cmd = BU_VLS_INIT_ZERO;

    type = s->edit_state.e_type - 4;

    loc = es_menu*4;
    valid = 0;

    bu_vls_printf(&str, "Enter fixed vertex number(");
    for (i=0; i<4; i++) {
	if (rt_arb_vertices[type][loc+i])
	    bu_vls_printf(&str, "%d ", rt_arb_vertices[type][loc+i]);
    }
    bu_vls_printf(&str, ") [%d]: ", rt_arb_vertices[type][loc]);

    const struct bu_vls *dnvp = dm_get_dname(s->mged_curr_dm->dm_dmp);

    bu_vls_printf(&cmd, "cad_input_dialog .get_vertex %s {Need vertex for solid rotate}\
 {%s} vertex_num %d 0 {{ summary \"Enter a vertex number to rotate about.\"}} OK",
		  (dnvp) ? bu_vls_addr(dnvp) : "id", bu_vls_addr(&str), rt_arb_vertices[type][loc]);

    while (!valid) {
	if (Tcl_Eval(s->interp, bu_vls_addr(&cmd)) != TCL_OK) {
	    Tcl_AppendResult(s->interp, "get_rotation_vertex: Error reading vertex\n", (char *)NULL);
	    /* Using default */
	    return rt_arb_vertices[type][loc];
	}

	vertex = atoi(Tcl_GetVar(s->interp, "vertex_num", TCL_GLOBAL_ONLY));
	for (j=0; j<4; j++) {
	    if (vertex==rt_arb_vertices[type][loc+j])
		valid = 1;
	}
    }

    bu_vls_free(&str);
    return vertex;
}


const char *
get_file_name(struct mged_state *s, char *str)
{
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    struct bu_vls varname_vls = BU_VLS_INIT_ZERO;
    char *dir;
    char *fptr;
    char *ptr1;
    char *ptr2;

    bu_vls_strcpy(&varname_vls, "mged_gui(getFileDir)");

    if ((fptr=strrchr(str, '/'))) {
	dir = (char *)bu_malloc((strlen(str)+1)*sizeof(char), "get_file_name: dir");
	ptr1 = str;
	ptr2 = dir;
	while (ptr1 != fptr)
	    *ptr2++ = *ptr1++;
	*ptr2 = '\0';
	Tcl_SetVar(s->interp, bu_vls_addr(&varname_vls), dir, TCL_GLOBAL_ONLY);
	bu_free((void *)dir, "get_file_name: directory string");
    }

    if (dm_get_pathname(DMP)) {
	bu_vls_printf(&cmd,
		"getFile %s %s {{{All Files} {*}}} {Get File}",
		bu_vls_addr(dm_get_pathname(DMP)),
		bu_vls_addr(&varname_vls));
    }
    bu_vls_free(&varname_vls);

    if (Tcl_Eval(s->interp, bu_vls_addr(&cmd))) {
	bu_vls_free(&cmd);
	return (char *)NULL;
    } else if (Tcl_GetStringResult(s->interp)[0] != '\0') {
	bu_vls_free(&cmd);
	return Tcl_GetStringResult(s->interp);
    } else {
	bu_vls_free(&cmd);
    }
    return (char *)NULL;
}


static void
dsp_scale(struct mged_state *s, struct rt_dsp_internal *dsp, int idx)
{
    mat_t m, scalemat;

    RT_DSP_CK_MAGIC(dsp);

    MAT_IDN(m);

    if (s->edit_state.e_mvalid) {
	bu_log("s->edit_state.e_mvalid %g %g %g\n", V3ARGS(s->edit_state.e_mparam));
    }

    if (s->edit_state.e_inpara > 0) {
	m[idx] = s->edit_state.e_para[0];
	bu_log("Keyboard %g\n", s->edit_state.e_para[0]);
    } else if (!ZERO(s->edit_state.es_scale)) {
	m[idx] *= s->edit_state.es_scale;
	bu_log("s->edit_state.es_scale %g\n", s->edit_state.es_scale);
	s->edit_state.es_scale = 0.0;
    }

    bn_mat_xform_about_pnt(scalemat, m, s->edit_state.e_keypoint);

    bn_mat_mul(m, dsp->dsp_stom, scalemat);
    MAT_COPY(dsp->dsp_stom, m);

    bn_mat_mul(m, scalemat, dsp->dsp_mtos);
    MAT_COPY(dsp->dsp_mtos, m);

}


/*
 * Partial scaling of a solid.
 */
void
pscale(struct mged_state *s)
{
    static fastf_t ma, mb;

    switch (es_menu) {

	case MENU_VOL_CSIZE:	/* scale voxel size */
	    {
		bu_log("s->edit_state.es_scale = %g\n", s->edit_state.es_scale);
	    }
	    break;

	case MENU_TGC_SCALE_H:	/* scale height vector */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->h);
		}
		VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
	    }
	    break;

	case MENU_TGC_SCALE_H_V:	/* scale height vector (but move V) */
	    {
		point_t old_top;

		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->h);
		}
		VADD2(old_top, tgc->v, tgc->h);
		VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
		VSUB2(tgc->v, old_top, tgc->h);
	    }
	    break;

	case MENU_TGC_SCALE_H_CD:
	    {
		vect_t vec1, vec2;
		vect_t c, d;
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->h);
		}

		/* calculate new c */
		VSUB2(vec1, tgc->a, tgc->c);
		VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
		VADD2(c, tgc->c, vec2);

		/* calculate new d */
		VSUB2(vec1, tgc->b, tgc->d);
		VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
		VADD2(d, tgc->d, vec2);

		if (0 <= VDOT(tgc->c, c) &&
		    0 <= VDOT(tgc->d, d) &&
		    !ZERO(MAGNITUDE(c)) &&
		    !ZERO(MAGNITUDE(d))) {
		    /* adjust c, d and h */
		    VMOVE(tgc->c, c);
		    VMOVE(tgc->d, d);
		    VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
		}
	    }
	    break;

	case MENU_TGC_SCALE_H_V_AB:
	    {
		vect_t vec1, vec2;
		vect_t a, b;
		point_t old_top;
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->h);
		}

		/* calculate new a */
		VSUB2(vec1, tgc->c, tgc->a);
		VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
		VADD2(a, tgc->a, vec2);

		/* calculate new b */
		VSUB2(vec1, tgc->d, tgc->b);
		VSCALE(vec2, vec1, 1-s->edit_state.es_scale);
		VADD2(b, tgc->b, vec2);

		if (0 <= VDOT(tgc->a, a) &&
		    0 <= VDOT(tgc->b, b) &&
		    !ZERO(MAGNITUDE(a)) &&
		    !ZERO(MAGNITUDE(b))) {
		    /* adjust a, b, v and h */
		    VMOVE(tgc->a, a);
		    VMOVE(tgc->b, b);
		    VADD2(old_top, tgc->v, tgc->h);
		    VSCALE(tgc->h, tgc->h, s->edit_state.es_scale);
		    VSUB2(tgc->v, old_top, tgc->h);
		}
	    }
	    break;

	case MENU_TOR_R1:
	    /* scale radius 1 of TOR */
	    {
		struct rt_tor_internal *tor =
		    (struct rt_tor_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t newrad;
		RT_TOR_CK_MAGIC(tor);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    newrad = s->edit_state.e_para[0];
		} else {
		    newrad = tor->r_a * s->edit_state.es_scale;
		}
		if (newrad < SMALL) newrad = 4*SMALL;
		if (tor->r_h <= newrad)
		    tor->r_a = newrad;
	    }
	    break;

	case MENU_TOR_R2:
	    /* scale radius 2 of TOR */
	    {
		struct rt_tor_internal *tor =
		    (struct rt_tor_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t newrad;
		RT_TOR_CK_MAGIC(tor);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    newrad = s->edit_state.e_para[0];
		} else {
		    newrad = tor->r_h * s->edit_state.es_scale;
		}
		if (newrad < SMALL) newrad = 4*SMALL;
		if (newrad <= tor->r_a)
		    tor->r_h = newrad;
	    }
	    break;

	case MENU_ETO_R:
	    /* scale radius 1 (r) of ETO */
	    {
		struct rt_eto_internal *eto =
		    (struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t ch, cv, dh, newrad;
		vect_t Nu;

		RT_ETO_CK_MAGIC(eto);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    newrad = s->edit_state.e_para[0];
		} else {
		    newrad = eto->eto_r * s->edit_state.es_scale;
		}
		if (newrad < SMALL) newrad = 4*SMALL;
		VMOVE(Nu, eto->eto_N);
		VUNITIZE(Nu);
		/* get horiz and vert components of C and Rd */
		cv = VDOT(eto->eto_C, Nu);
		ch = sqrt(VDOT(eto->eto_C, eto->eto_C) - cv * cv);
		/* angle between C and Nu */
		dh = eto->eto_rd * cv / MAGNITUDE(eto->eto_C);
		/* make sure revolved ellipse doesn't overlap itself */
		if (ch <= newrad && dh <= newrad)
		    eto->eto_r = newrad;
	    }
	    break;

	case MENU_ETO_RD:
	    /* scale Rd, ellipse semi-minor axis length, of ETO */
	    {
		struct rt_eto_internal *eto =
		    (struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t dh, newrad, work;
		vect_t Nu;

		RT_ETO_CK_MAGIC(eto);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    newrad = s->edit_state.e_para[0];
		} else {
		    newrad = eto->eto_rd * s->edit_state.es_scale;
		}
		if (newrad < SMALL) newrad = 4*SMALL;
		work = MAGNITUDE(eto->eto_C);
		if (newrad <= work) {
		    VMOVE(Nu, eto->eto_N);
		    VUNITIZE(Nu);
		    dh = newrad * VDOT(eto->eto_C, Nu) / work;
		    /* make sure revolved ellipse doesn't overlap itself */
		    if (dh <= eto->eto_r)
			eto->eto_rd = newrad;
		}
	    }
	    break;

	case MENU_ETO_SCALE_C:
	    /* scale vector C */
	    {
		struct rt_eto_internal *eto =
		    (struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t ch, cv;
		vect_t Nu, Work;

		RT_ETO_CK_MAGIC(eto);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(eto->eto_C);
		}
		if (s->edit_state.es_scale * MAGNITUDE(eto->eto_C) >= eto->eto_rd) {
		    VMOVE(Nu, eto->eto_N);
		    VUNITIZE(Nu);
		    VSCALE(Work, eto->eto_C, s->edit_state.es_scale);
		    /* get horiz and vert comps of C and Rd */
		    cv = VDOT(Work, Nu);
		    ch = sqrt(VDOT(Work, Work) - cv * cv);
		    if (ch <= eto->eto_r)
			VMOVE(eto->eto_C, Work);
		}
	    }
	    break;

	case MENU_RPC_B:
	    /* scale vector B */
	    {
		struct rt_rpc_internal *rpc =
		    (struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;
		RT_RPC_CK_MAGIC(rpc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(rpc->rpc_B);
		}
		VSCALE(rpc->rpc_B, rpc->rpc_B, s->edit_state.es_scale);
	    }
	    break;

	case MENU_RPC_H:
	    /* scale vector H */
	    {
		struct rt_rpc_internal *rpc =
		    (struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;

		RT_RPC_CK_MAGIC(rpc);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(rpc->rpc_H);
		}
		VSCALE(rpc->rpc_H, rpc->rpc_H, s->edit_state.es_scale);
	    }
	    break;

	case MENU_RPC_R:
	    /* scale rectangular half-width of RPC */
	    {
		struct rt_rpc_internal *rpc =
		    (struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;

		RT_RPC_CK_MAGIC(rpc);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / rpc->rpc_r;
		}
		rpc->rpc_r *= s->edit_state.es_scale;
	    }
	    break;

	case MENU_RHC_B:
	    /* scale vector B */
	    {
		struct rt_rhc_internal *rhc =
		    (struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;
		RT_RHC_CK_MAGIC(rhc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(rhc->rhc_B);
		}
		VSCALE(rhc->rhc_B, rhc->rhc_B, s->edit_state.es_scale);
	    }
	    break;

	case MENU_RHC_H:
	    /* scale vector H */
	    {
		struct rt_rhc_internal *rhc =
		    (struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;
		RT_RHC_CK_MAGIC(rhc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(rhc->rhc_H);
		}
		VSCALE(rhc->rhc_H, rhc->rhc_H, s->edit_state.es_scale);
	    }
	    break;

	case MENU_RHC_R:
	    /* scale rectangular half-width of RHC */
	    {
		struct rt_rhc_internal *rhc =
		    (struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;

		RT_RHC_CK_MAGIC(rhc);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / rhc->rhc_r;
		}
		rhc->rhc_r *= s->edit_state.es_scale;
	    }
	    break;

	case MENU_RHC_C:
	    /* scale rectangular half-width of RHC */
	    {
		struct rt_rhc_internal *rhc =
		    (struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;

		RT_RHC_CK_MAGIC(rhc);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / rhc->rhc_c;
		}
		rhc->rhc_c *= s->edit_state.es_scale;
	    }
	    break;

	case MENU_EPA_H:
	    /* scale height vector H */
	    {
		struct rt_epa_internal *epa =
		    (struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;

		RT_EPA_CK_MAGIC(epa);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(epa->epa_H);
		}
		VSCALE(epa->epa_H, epa->epa_H, s->edit_state.es_scale);
	    }
	    break;

	case MENU_EPA_R1:
	    /* scale semimajor axis of EPA */
	    {
		struct rt_epa_internal *epa =
		    (struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;

		RT_EPA_CK_MAGIC(epa);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / epa->epa_r1;
		}
		if (epa->epa_r1 * s->edit_state.es_scale >= epa->epa_r2)
		    epa->epa_r1 *= s->edit_state.es_scale;
		else
		    bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
	    }
	    break;

	case MENU_EPA_R2:
	    /* scale semiminor axis of EPA */
	    {
		struct rt_epa_internal *epa =
		    (struct rt_epa_internal *)s->edit_state.es_int.idb_ptr;

		RT_EPA_CK_MAGIC(epa);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / epa->epa_r2;
		}
		if (epa->epa_r2 * s->edit_state.es_scale <= epa->epa_r1)
		    epa->epa_r2 *= s->edit_state.es_scale;
		else
		    bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
	    }
	    break;

	case MENU_EHY_H:
	    /* scale height vector H */
	    {
		struct rt_ehy_internal *ehy =
		    (struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

		RT_EHY_CK_MAGIC(ehy);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(ehy->ehy_H);
		}
		VSCALE(ehy->ehy_H, ehy->ehy_H, s->edit_state.es_scale);
	    }
	    break;

	case MENU_EHY_R1:
	    /* scale semimajor axis of EHY */
	    {
		struct rt_ehy_internal *ehy =
		    (struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

		RT_EHY_CK_MAGIC(ehy);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / ehy->ehy_r1;
		}
		if (ehy->ehy_r1 * s->edit_state.es_scale >= ehy->ehy_r2)
		    ehy->ehy_r1 *= s->edit_state.es_scale;
		else
		    bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
	    }
	    break;

	case MENU_EHY_R2:
	    /* scale semiminor axis of EHY */
	    {
		struct rt_ehy_internal *ehy =
		    (struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

		RT_EHY_CK_MAGIC(ehy);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / ehy->ehy_r2;
		}
		if (ehy->ehy_r2 * s->edit_state.es_scale <= ehy->ehy_r1)
		    ehy->ehy_r2 *= s->edit_state.es_scale;
		else
		    bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
	    }
	    break;

	case MENU_EHY_C:
	    /* scale distance between apex of EHY & asymptotic cone */
	    {
		struct rt_ehy_internal *ehy =
		    (struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

		RT_EHY_CK_MAGIC(ehy);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / ehy->ehy_c;
		}
		ehy->ehy_c *= s->edit_state.es_scale;
	    }
	    break;

	case MENU_HYP_H:
	    /* scale height of HYP */
	    {
		struct rt_hyp_internal *hyp =
		    (struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

		RT_HYP_CK_MAGIC(hyp);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0];
		}
		VSCALE(hyp->hyp_Hi, hyp->hyp_Hi, s->edit_state.es_scale);
	    }
	    break;

	case MENU_HYP_SCALE_A:
	    /* scale A vector of HYP */
	    {
		struct rt_hyp_internal *hyp =
		    (struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

		RT_HYP_CK_MAGIC(hyp);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0];
		}
		VSCALE(hyp->hyp_A, hyp->hyp_A, s->edit_state.es_scale);
	    }
	    break;

	case MENU_HYP_SCALE_B:
	    /* scale B vector of HYP */
	    {
		struct rt_hyp_internal *hyp =
		    (struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

		RT_HYP_CK_MAGIC(hyp);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0];
		}
		hyp->hyp_b = hyp->hyp_b * s->edit_state.es_scale;
	    }
	    break;


	case MENU_HYP_C:
	    /* scale Neck to Base ratio of HYP */
	    {
		struct rt_hyp_internal *hyp =
		    (struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

		RT_HYP_CK_MAGIC(hyp);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0];
		}
		if (hyp->hyp_bnr * s->edit_state.es_scale <= 1.0) {
		    hyp->hyp_bnr = hyp->hyp_bnr * s->edit_state.es_scale;
		}
	    }
	    break;


	case MENU_TGC_SCALE_A:
	    /* scale vector A */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->a);
		}
		VSCALE(tgc->a, tgc->a, s->edit_state.es_scale);
	    }
	    break;

	case MENU_TGC_SCALE_B:
	    /* scale vector B */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->b);
		}
		VSCALE(tgc->b, tgc->b, s->edit_state.es_scale);
	    }
	    break;

	case MENU_ELL_SCALE_A:
	    /* scale vector A */
	    {
		struct rt_ell_internal *ell =
		    (struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
		RT_ELL_CK_MAGIC(ell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(ell->a);
		}
		VSCALE(ell->a, ell->a, s->edit_state.es_scale);
	    }
	    break;

	case MENU_ELL_SCALE_B:
	    /* scale vector B */
	    {
		struct rt_ell_internal *ell =
		    (struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
		RT_ELL_CK_MAGIC(ell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(ell->b);
		}
		VSCALE(ell->b, ell->b, s->edit_state.es_scale);
	    }
	    break;

	case MENU_ELL_SCALE_C:
	    /* scale vector C */
	    {
		struct rt_ell_internal *ell =
		    (struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
		RT_ELL_CK_MAGIC(ell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(ell->c);
		}
		VSCALE(ell->c, ell->c, s->edit_state.es_scale);
	    }
	    break;

	case MENU_TGC_SCALE_C:
	    /* TGC: scale ratio "c" */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->c);
		}
		VSCALE(tgc->c, tgc->c, s->edit_state.es_scale);
	    }
	    break;

	case MENU_TGC_SCALE_D:   /* scale d for tgc */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->d);
		}
		VSCALE(tgc->d, tgc->d, s->edit_state.es_scale);
	    }
	    break;

	case MENU_TGC_SCALE_AB:
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->a);
		}
		VSCALE(tgc->a, tgc->a, s->edit_state.es_scale);
		ma = MAGNITUDE(tgc->a);
		mb = MAGNITUDE(tgc->b);
		VSCALE(tgc->b, tgc->b, ma/mb);
	    }
	    break;

	case MENU_TGC_SCALE_CD:	/* scale C and D of tgc */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->c);
		}
		VSCALE(tgc->c, tgc->c, s->edit_state.es_scale);
		ma = MAGNITUDE(tgc->c);
		mb = MAGNITUDE(tgc->d);
		VSCALE(tgc->d, tgc->d, ma/mb);
	    }
	    break;

	case MENU_TGC_SCALE_ABCD: 		/* scale A, B, C, and D of tgc */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(tgc->a);
		}
		VSCALE(tgc->a, tgc->a, s->edit_state.es_scale);
		ma = MAGNITUDE(tgc->a);
		mb = MAGNITUDE(tgc->b);
		VSCALE(tgc->b, tgc->b, ma/mb);
		mb = MAGNITUDE(tgc->c);
		VSCALE(tgc->c, tgc->c, ma/mb);
		mb = MAGNITUDE(tgc->d);
		VSCALE(tgc->d, tgc->d, ma/mb);
	    }
	    break;

	case MENU_ELL_SCALE_ABC:	/* set A, B, and C length the same */
	    {
		struct rt_ell_internal *ell =
		    (struct rt_ell_internal *)s->edit_state.es_int.idb_ptr;
		RT_ELL_CK_MAGIC(ell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(ell->a);
		}
		VSCALE(ell->a, ell->a, s->edit_state.es_scale);
		ma = MAGNITUDE(ell->a);
		mb = MAGNITUDE(ell->b);
		VSCALE(ell->b, ell->b, ma/mb);
		mb = MAGNITUDE(ell->c);
		VSCALE(ell->c, ell->c, ma/mb);
	    }
	    break;

	    /* begin super ellipse menu options */
	case MENU_SUPERELL_SCALE_A:
	    /* scale vector A */
	    {
		struct rt_superell_internal *superell =
		    (struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
		RT_SUPERELL_CK_MAGIC(superell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(superell->a);
		}
		VSCALE(superell->a, superell->a, s->edit_state.es_scale);
	    }
	    break;

	case MENU_SUPERELL_SCALE_B:
	    /* scale vector B */
	    {
		struct rt_superell_internal *superell =
		    (struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
		RT_SUPERELL_CK_MAGIC(superell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(superell->b);
		}
		VSCALE(superell->b, superell->b, s->edit_state.es_scale);
	    }
	    break;

	case MENU_SUPERELL_SCALE_C:
	    /* scale vector C */
	    {
		struct rt_superell_internal *superell =
		    (struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
		RT_SUPERELL_CK_MAGIC(superell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(superell->c);
		}
		VSCALE(superell->c, superell->c, s->edit_state.es_scale);
	    }
	    break;

	case MENU_SUPERELL_SCALE_ABC:	/* set A, B, and C length the same */
	    {
		struct rt_superell_internal *superell =
		    (struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
		RT_SUPERELL_CK_MAGIC(superell);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15] /
			MAGNITUDE(superell->a);
		}
		VSCALE(superell->a, superell->a, s->edit_state.es_scale);
		ma = MAGNITUDE(superell->a);
		mb = MAGNITUDE(superell->b);
		VSCALE(superell->b, superell->b, ma/mb);
		mb = MAGNITUDE(superell->c);
		VSCALE(superell->c, superell->c, ma/mb);
	    }
	    break;


	case MENU_PIPE_PT_OD:	/* scale OD of one pipe segment */
	    {
		if (!es_pipe_pnt) {
		    Tcl_AppendResult(s->interp, "pscale: no pipe segment selected for scaling\n", (char *)NULL);
		    return;
		}

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    if (es_pipe_pnt->pp_od > 0.0)
			s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/es_pipe_pnt->pp_od;
		    else
			s->edit_state.es_scale = (-s->edit_state.e_para[0] * s->edit_state.e_mat[15]);
		}
		pipe_seg_scale_od(s, es_pipe_pnt, s->edit_state.es_scale);
	    }
	    break;
	case MENU_PIPE_PT_ID:	/* scale ID of one pipe segment */
	    {
		if (!es_pipe_pnt) {
		    Tcl_AppendResult(s->interp, "pscale: no pipe segment selected for scaling\n", (char *)NULL);
		    return;
		}

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    if (es_pipe_pnt->pp_id > 0.0)
			s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/es_pipe_pnt->pp_id;
		    else
			s->edit_state.es_scale = (-s->edit_state.e_para[0] * s->edit_state.e_mat[15]);
		}
		pipe_seg_scale_id(s, es_pipe_pnt, s->edit_state.es_scale);
	    }
	    break;
	case MENU_PIPE_PT_RADIUS:	/* scale bend radius at selected point */
	    {
		if (!es_pipe_pnt) {
		    Tcl_AppendResult(s->interp, "pscale: no pipe segment selected for scaling\n", (char *)NULL);
		    return;
		}

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    if (es_pipe_pnt->pp_id > 0.0)
			s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/es_pipe_pnt->pp_bendradius;
		    else
			s->edit_state.es_scale = (-s->edit_state.e_para[0] * s->edit_state.e_mat[15]);
		}
		pipe_seg_scale_radius(s, es_pipe_pnt, s->edit_state.es_scale);
	    }
	    break;
	case MENU_PIPE_SCALE_OD:	/* scale entire pipe OD */
	    if (s->edit_state.e_inpara) {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		struct wdb_pipe_pnt *ps;

		RT_PIPE_CK_MAGIC(pipeip);

		ps = BU_LIST_FIRST(wdb_pipe_pnt, &pipeip->pipe_segs_head);
		BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "wdb_pipe_pnt");

		if (ps->pp_od > 0.0) {
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/ps->pp_od;
		} else {
		    while (ps->l.magic != BU_LIST_HEAD_MAGIC && ps->pp_od <= 0.0)
			ps = BU_LIST_NEXT(wdb_pipe_pnt, &ps->l);

		    if (ps->l.magic == BU_LIST_HEAD_MAGIC) {
			Tcl_AppendResult(s->interp, "Entire pipe solid has zero OD!\n", (char *)NULL);
			return;
		    }

		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/ps->pp_od;
		}
	    }
	    pipe_scale_od(s, &s->edit_state.es_int, s->edit_state.es_scale);
	    break;
	case MENU_PIPE_SCALE_ID:	/* scale entire pipe ID */
	    if (s->edit_state.e_inpara) {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		struct wdb_pipe_pnt *ps;

		RT_PIPE_CK_MAGIC(pipeip);

		ps = BU_LIST_FIRST(wdb_pipe_pnt, &pipeip->pipe_segs_head);
		BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "wdb_pipe_pnt");

		if (ps->pp_id > 0.0) {
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/ps->pp_id;
		} else {
		    while (ps->l.magic != BU_LIST_HEAD_MAGIC && ps->pp_id <= 0.0)
			ps = BU_LIST_NEXT(wdb_pipe_pnt, &ps->l);

		    /* Check if entire pipe has zero ID */
		    if (ps->l.magic == BU_LIST_HEAD_MAGIC)
			s->edit_state.es_scale = (-s->edit_state.e_para[0] * s->edit_state.e_mat[15]);
		    else
			s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/ps->pp_id;
		}
	    }
	    pipe_scale_id(s, &s->edit_state.es_int, s->edit_state.es_scale);
	    break;
	case MENU_PIPE_SCALE_RADIUS:	/* scale entire pipr bend radius */
	    if (s->edit_state.e_inpara) {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		struct wdb_pipe_pnt *ps;

		RT_PIPE_CK_MAGIC(pipeip);

		ps = BU_LIST_FIRST(wdb_pipe_pnt, &pipeip->pipe_segs_head);
		BU_CKMAG(ps, WDB_PIPESEG_MAGIC, "wdb_pipe_pnt");

		if (ps->pp_bendradius > 0.0) {
		    s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/ps->pp_bendradius;
		} else {
		    while (ps->l.magic != BU_LIST_HEAD_MAGIC && ps->pp_bendradius <= 0.0)
			ps = BU_LIST_NEXT(wdb_pipe_pnt, &ps->l);

		    /* Check if entire pipe has zero ID */
		    if (ps->l.magic == BU_LIST_HEAD_MAGIC)
			s->edit_state.es_scale = (-s->edit_state.e_para[0] * s->edit_state.e_mat[15]);
		    else
			s->edit_state.es_scale = s->edit_state.e_para[0] * s->edit_state.e_mat[15]/ps->pp_bendradius;
		}
	    }
	    pipe_scale_radius(s, &s->edit_state.es_int, s->edit_state.es_scale);
	    break;
	case MENU_PART_H:
	    /* scale vector H */
	    {
		struct rt_part_internal *part =
		    (struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

		RT_PART_CK_MAGIC(part);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(part->part_H);
		}
		VSCALE(part->part_H, part->part_H, s->edit_state.es_scale);
	    }
	    break;

	case MENU_PART_v:
	    /* scale v end radius */
	    {
		struct rt_part_internal *part =
		    (struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

		RT_PART_CK_MAGIC(part);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / part->part_vrad;
		}
		part->part_vrad *= s->edit_state.es_scale;
	    }
	    break;

	case MENU_PART_h:
	    /* scale h end radius */
	    {
		struct rt_part_internal *part =
		    (struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

		RT_PART_CK_MAGIC(part);
		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / part->part_hrad;
		}
		part->part_hrad *= s->edit_state.es_scale;
	    }
	    break;
	case MENU_METABALL_SET_THRESHOLD:
	    {
		struct rt_metaball_internal *ball =
		    (struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
		RT_METABALL_CK_MAGIC(ball);
		ball->threshold = s->edit_state.e_para[0];
	    }
	    break;
	case MENU_METABALL_SET_METHOD:
	    {
		struct rt_metaball_internal *ball =
		    (struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
		RT_METABALL_CK_MAGIC(ball);
		ball->method = s->edit_state.e_para[0];
	    }
	    break;
	case MENU_METABALL_PT_SET_GOO:
	    {
		if (!es_metaball_pnt || !s->edit_state.e_inpara) {
		    Tcl_AppendResult(s->interp, "pscale: no metaball point selected for scaling goo\n", (char *)NULL);
		    return;
		}
		es_metaball_pnt->sweat *= *s->edit_state.e_para * ((s->edit_state.es_scale > -SMALL_FASTF) ? s->edit_state.es_scale : 1.0);
	    }
	    break;
	case MENU_METABALL_PT_FLDSTR:
	    {
		if (!es_metaball_pnt || !s->edit_state.e_inpara) {
		    Tcl_AppendResult(s->interp, "pscale: no metaball point selected for scaling strength\n", (char *)NULL);
		    return;
		}
		es_metaball_pnt->fldstr *= *s->edit_state.e_para * ((s->edit_state.es_scale > -SMALL_FASTF) ? s->edit_state.es_scale : 1.0);
	    }
	    break;
    }
}


/*
 * A great deal of magic takes place here, to accomplish solid editing.
 *
 * Called from mged main loop after any event handlers:
 * if (sedraw > 0) sedit(s);
 * to process any residual events that the event handlers were too
 * lazy to handle themselves.
 *
 * A lot of processing is deferred to here, so that the "p" command
 * can operate on an equal footing to mouse events.
 */
void
sedit(struct mged_state *s)
{
    struct rt_arb_internal *arb;
    fastf_t *eqp;
    static vect_t work;
    size_t i;
    static int pnt5;		/* ECMD_ARB_SETUP_ROTFACE, special arb7 case */
    static float la, lb, lc, ld;	/* TGC: length of vectors */
    mat_t mat;
    mat_t mat1;
    mat_t edit;
    point_t rot_point;

    if (s->dbip == DBI_NULL)
	return;

    sedraw = 0;
    ++s->update_views;

    switch (es_edflag) {

	case IDLE:
	    /* do nothing more */
	    --s->update_views;
	    break;

	case ECMD_DSP_SCALE_X:
	    dsp_scale(s, (struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr, MSX);
	    break;
	case ECMD_DSP_SCALE_Y:
	    dsp_scale(s, (struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr, MSY);
	    break;
	case ECMD_DSP_SCALE_ALT:
	    dsp_scale(s, (struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr, MSZ);
	    break;
	case ECMD_DSP_FNAME:
	    {
		struct rt_dsp_internal *dsp =
		    (struct rt_dsp_internal *)s->edit_state.es_int.idb_ptr;
		const char *fname;
		struct stat stat_buf;
		b_off_t need_size;
		struct bu_vls message = BU_VLS_INIT_ZERO;

		RT_DSP_CK_MAGIC(dsp);

		/* Pop-up the Tk file browser */
		fname = get_file_name(s, bu_vls_addr(&dsp->dsp_name));
		if (! fname) break;

		if (stat(fname, &stat_buf)) {
		    bu_vls_printf(&message, "Cannot get status of file %s\n", fname);
		    Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
		    bu_vls_free(&message);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}

		need_size = dsp->dsp_xcnt * dsp->dsp_ycnt * 2;
		if (stat_buf.st_size < need_size) {
		    bu_vls_printf(&message, "File (%s) is too small, adjust the file size parameters first", fname);
		    Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
		    bu_vls_free(&message);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}
		bu_vls_strcpy(&dsp->dsp_name, fname);

		break;
	    }

	case ECMD_EBM_FSIZE:	/* set file size */
	    {
		struct rt_ebm_internal *ebm =
		    (struct rt_ebm_internal *)s->edit_state.es_int.idb_ptr;
		struct stat stat_buf;
		b_off_t need_size;

		RT_EBM_CK_MAGIC(ebm);

		if (s->edit_state.e_inpara == 2) {
		    if (stat(ebm->name, &stat_buf)) {
			Tcl_AppendResult(s->interp, "Cannot get status of ebm data source ", ebm->name, (char *)NULL);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    need_size = s->edit_state.e_para[0] * s->edit_state.e_para[1] * sizeof(unsigned char);
		    if (stat_buf.st_size < need_size) {
			Tcl_AppendResult(s->interp, "File (", ebm->name,
					 ") is too small, set data source name first", (char *)NULL);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    ebm->xdim = s->edit_state.e_para[0];
		    ebm->ydim = s->edit_state.e_para[1];
		} else if (s->edit_state.e_inpara > 0) {
		    Tcl_AppendResult(s->interp, "width and length of data source are required\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}
	    }
	    break;

	case ECMD_EBM_FNAME:
	    {
		struct rt_ebm_internal *ebm =
		    (struct rt_ebm_internal *)s->edit_state.es_int.idb_ptr;
		const char *fname;
		struct stat stat_buf;
		b_off_t need_size;

		RT_EBM_CK_MAGIC(ebm);

		fname = get_file_name(s, ebm->name);
		if (fname) {
		    struct bu_vls message = BU_VLS_INIT_ZERO;

		    if (stat(fname, &stat_buf)) {
			bu_vls_printf(&message, "Cannot get status of file %s\n", fname);
			Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
			bu_vls_free(&message);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    need_size = ebm->xdim * ebm->ydim * sizeof(unsigned char);
		    if (stat_buf.st_size < need_size) {
			bu_vls_printf(&message, "File (%s) is too small, adjust the file size parameters first", fname);
			Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
			bu_vls_free(&message);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    bu_strlcpy(ebm->name, fname, RT_EBM_NAME_LEN);
		}

		break;
	    }

	case ECMD_EBM_HEIGHT:	/* set extrusion depth */
	    {
		struct rt_ebm_internal *ebm =
		    (struct rt_ebm_internal *)s->edit_state.es_int.idb_ptr;

		RT_EBM_CK_MAGIC(ebm);

		if (s->edit_state.e_inpara == 1)
		    ebm->tallness = s->edit_state.e_para[0];
		else if (s->edit_state.e_inpara > 0) {
		    Tcl_AppendResult(s->interp,
				     "extrusion depth required\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		} else if (s->edit_state.es_scale > 0.0) {
		    ebm->tallness *= s->edit_state.es_scale;
		    s->edit_state.es_scale = 0.0;
		}
	    }
	    break;

	case ECMD_VOL_CSIZE:	/* set voxel size */
	    {
		struct rt_vol_internal *vol =
		    (struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;

		RT_VOL_CK_MAGIC(vol);

		if (s->edit_state.e_inpara == 3) {
		    VMOVE(vol->cellsize, s->edit_state.e_para);
		} else if (s->edit_state.e_inpara > 0 && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x, y, and z cell sizes are required\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		} else if (s->edit_state.es_scale > 0.0) {
		    VSCALE(vol->cellsize, vol->cellsize, s->edit_state.es_scale);
		    s->edit_state.es_scale = 0.0;
		}
	    }
	    break;

	case ECMD_VOL_FSIZE:	/* set file size */
	    {
		struct rt_vol_internal *vol =
		    (struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;
		struct stat stat_buf;
		b_off_t need_size;

		RT_VOL_CK_MAGIC(vol);

		if (s->edit_state.e_inpara == 3) {
		    if (stat(vol->name, &stat_buf)) {
			Tcl_AppendResult(s->interp, "Cannot get status of file ", vol->name, (char *)NULL);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    need_size = s->edit_state.e_para[0] * s->edit_state.e_para[1] * s->edit_state.e_para[2] * sizeof(unsigned char);
		    if (stat_buf.st_size < need_size) {
			Tcl_AppendResult(s->interp, "File (", vol->name,
					 ") is too small, set file name first", (char *)NULL);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    vol->xdim = s->edit_state.e_para[0];
		    vol->ydim = s->edit_state.e_para[1];
		    vol->zdim = s->edit_state.e_para[2];
		} else if (s->edit_state.e_inpara > 0) {
		    Tcl_AppendResult(s->interp, "x, y, and z file sizes are required\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}
	    }
	    break;

	case ECMD_VOL_THRESH_LO:
	    {
		struct rt_vol_internal *vol =
		    (struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;

		RT_VOL_CK_MAGIC(vol);

		i = vol->lo;
		if (s->edit_state.e_inpara) {
		    i = s->edit_state.e_para[0];
		} else if (s->edit_state.es_scale > 0.0) {
		    i = vol->lo * s->edit_state.es_scale;
		    if (i == vol->lo && s->edit_state.es_scale > 1.0) {
			i++;
		    } else if (i == vol->lo && s->edit_state.es_scale < 1.0) {
			i--;
		    }
		}

		if (i > 255)
		    i = 255;

		vol->lo = i;
		break;
	    }

	case ECMD_VOL_THRESH_HI:
	    {
		struct rt_vol_internal *vol =
		    (struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;

		RT_VOL_CK_MAGIC(vol);

		i = vol->hi;
		if (s->edit_state.e_inpara) {
		    i = s->edit_state.e_para[0];
		} else if (s->edit_state.es_scale > 0.0) {
		    i = vol->hi * s->edit_state.es_scale;
		    if (i == vol->hi && s->edit_state.es_scale > 1.0) {
			i++;
		    } else if (i == vol->hi && s->edit_state.es_scale < 1.0) {
			i--;
		    }
		}

		if (i > 255)
		    i = 255;

		vol->hi = i;
		break;
	    }

	case ECMD_VOL_FNAME:
	    {
		struct rt_vol_internal *vol =
		    (struct rt_vol_internal *)s->edit_state.es_int.idb_ptr;
		const char *fname;
		struct stat stat_buf;
		b_off_t need_size;

		RT_VOL_CK_MAGIC(vol);

		fname = get_file_name(s, vol->name);
		if (fname) {
		    struct bu_vls message = BU_VLS_INIT_ZERO;

		    if (stat(fname, &stat_buf)) {
			bu_vls_printf(&message, "Cannot get status of file %s\n", fname);
			Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
			bu_vls_free(&message);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    need_size = vol->xdim * vol->ydim * vol->zdim * sizeof(unsigned char);
		    if (stat_buf.st_size < need_size) {
			bu_vls_printf(&message, "File (%s) is too small, adjust the file size parameters first", fname);
			Tcl_SetResult(s->interp, bu_vls_addr(&message), TCL_VOLATILE);
			bu_vls_free(&message);
			mged_print_result(s, TCL_ERROR);
			return;
		    }
		    bu_strlcpy(vol->name, fname, RT_VOL_NAME_LEN);
		}

		break;
	    }

	case ECMD_BOT_MODE:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		const char *radio_result;
		char mode[10];
		int ret_tcl = TCL_ERROR;
		int old_mode;

		RT_BOT_CK_MAGIC(bot);
		old_mode = bot->mode;
		sprintf(mode, " %d", old_mode - 1);
		if (dm_get_pathname(DMP)) {
		    ret_tcl = Tcl_VarEval(s->interp, "cad_radio", " .bot_mode_radio ",
			    bu_vls_addr(dm_get_pathname(DMP)), " _bot_mode_result",
			    " \"BOT Mode\"", "  \"Select the desired mode\"", mode,
			    " { surface volume plate plate/nocosine }",
			    " { \"In surface mode, each triangle represents part of a zero thickness surface and no volume is enclosed\" \"In volume mode, the triangles are expected to enclose a volume and that volume becomes the solid\" \"In plate mode, each triangle represents a plate with a specified thickness\" \"In plate/nocosine mode, each triangle represents a plate with a specified thickness, but the LOS thickness reported by the raytracer is independent of obliquity angle\" } ", (char *)NULL);
		}
		if (ret_tcl != TCL_OK) {
		    Tcl_AppendResult(s->interp, "Mode selection failed!\n", (char *)NULL);
		    break;
		}
		radio_result = Tcl_GetVar(s->interp, "_bot_mode_result", TCL_GLOBAL_ONLY);
		bot->mode = atoi(radio_result) + 1;
		if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
		    if (old_mode != RT_BOT_PLATE && old_mode != RT_BOT_PLATE_NOCOS) {
			/* need to create some thicknesses */
			bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
			bot->face_mode = bu_bitv_new(bot->num_faces);
		    }
		} else {
		    if (old_mode == RT_BOT_PLATE || old_mode == RT_BOT_PLATE_NOCOS) {
			/* free the per face memory */
			bu_free((char *)bot->thickness, "BOT thickness");
			bot->thickness = (fastf_t *)NULL;
			bu_free((char *)bot->face_mode, "BOT face_mode");
			bot->face_mode = (struct bu_bitv *)NULL;
		    }
		}
	    }
	    break;
	case ECMD_BOT_ORIENT:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		const char *radio_result;
		char orient[10];
		int ret_tcl = TCL_ERROR;

		RT_BOT_CK_MAGIC(bot);
		sprintf(orient, " %d", bot->orientation - 1);
		if (dm_get_pathname(DMP)) {
		    ret_tcl = Tcl_VarEval(s->interp, "cad_radio", " .bot_orient_radio ",
			    bu_vls_addr(dm_get_pathname(DMP)), " _bot_orient_result",
			    " \"BOT Face Orientation\"", "  \"Select the desired orientation\"", orient,
			    " { none right-hand-rule left-hand-rule }",
			    " { \"No orientation means that there is no particular order for the vertices of the triangles\" \"right-hand-rule means that the vertices of each triangle are ordered such that the right-hand-rule produces an outward pointing normal\"  \"left-hand-rule means that the vertices of each triangle are ordered such that the left-hand-rule produces an outward pointing normal\" } ", (char *)NULL);
		}
		if (ret_tcl != TCL_OK) {
		    Tcl_AppendResult(s->interp, "Face orientation selection failed!\n", (char *)NULL);
		    break;
		}
		radio_result = Tcl_GetVar(s->interp, "_bot_orient_result", TCL_GLOBAL_ONLY);
		bot->orientation = atoi(radio_result) + 1;
	    }
	    break;
	case ECMD_BOT_THICK:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		size_t face_no = 0;
		int face_state = 0;

		RT_BOT_CK_MAGIC(bot);

		if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
		    if (Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ", "$mged_gui(mged,screen) ", "{Not Plate Mode} ",
				    "{Cannot edit face thickness in a non-plate BOT} ", "\"\" ", "0 ", "OK ",
				    (char *)NULL) != TCL_OK)
		    {
			bu_log("cad_dialog failed: %s\n", Tcl_GetStringResult(s->interp));
		    }
		    break;
		}

		if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
		    /* setting thickness for all faces */
		    if (!s->edit_state.e_inpara)
			break;

		    (void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ",
				      "$mged_gui(mged,screen) ", "{Setting Thickness for All Faces} ",
				      "{No face is selected, so this operation will modify all the faces in this BOT} ",
				      "\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL);
		    if (atoi(Tcl_GetStringResult(s->interp)))
			break;

		    for (i=0; i<bot->num_faces; i++)
			bot->thickness[i] = s->edit_state.e_para[0];
		} else {
		    /* setting thickness for just one face */
		    if (!s->edit_state.e_inpara)
			break;

		    face_state = -1;
		    for (i=0; i < bot->num_faces; i++) {
			if (bot_verts[0] == bot->faces[i*3] &&
			    bot_verts[1] == bot->faces[i*3+1] &&
			    bot_verts[2] == bot->faces[i*3+2])
			{
			    face_no = i;
			    face_state = 0;
			    break;
			}
		    }
		    if (face_state > -1) {
			bu_log("Cannot find face with vertices %d %d %d!\n",
			       V3ARGS(bot_verts));
			break;
		    }

		    bot->thickness[face_no] = s->edit_state.e_para[0];
		}
	    }
	    break;
	case ECMD_BOT_FLAGS:
	    {
		int ret_tcl = TCL_ERROR;
		const char *dialog_result;
		char cur_settings[11];
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;

		RT_BOT_CK_MAGIC(bot);

		bu_strlcpy(cur_settings, " { 0 0 }", sizeof(cur_settings));

		if (bot->bot_flags & RT_BOT_USE_NORMALS) {
		    cur_settings[3] = '1';
		}
		if (bot->bot_flags & RT_BOT_USE_FLOATS) {
		    cur_settings[5] = '1';
		}

		if (dm_get_pathname(DMP)) {
		    ret_tcl = Tcl_VarEval(s->interp,
			    "cad_list_buts",
			    " .bot_list_flags ",
			    bu_vls_addr(dm_get_pathname(DMP)),
			    " _bot_flags_result ",
			    cur_settings,
			    " \"BOT Flags\"",
			    " \"Select the desired flags\"",
			    " { {Use vertex normals} {Use single precision ray-tracing} }",
			    " { {This selection indicates that surface normals at hit points should be interpolated from vertex normals} {This selection indicates that the prepped form of the BOT triangles should use single precision to save memory} } ",
			    (char *)NULL);
		}
		if (ret_tcl != TCL_OK) {
		    bu_log("ERROR: cad_list_buts: %s\n", Tcl_GetStringResult(s->interp));
		    break;
		}
		dialog_result = Tcl_GetVar(s->interp, "_bot_flags_result", TCL_GLOBAL_ONLY);

		if (dialog_result[0] == '1') {
		    bot->bot_flags |= RT_BOT_USE_NORMALS;
		} else {
		    bot->bot_flags &= ~RT_BOT_USE_NORMALS;
		}
		if (dialog_result[2] == '1') {
		    bot->bot_flags |= RT_BOT_USE_FLOATS;
		} else {
		    bot->bot_flags &= ~RT_BOT_USE_FLOATS;
		}
	    }
	    break;
	case ECMD_BOT_FMODE:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		char fmode[10];
		const char *radio_result;
		size_t face_no = 0;
		int face_state = 0;
		int ret_tcl = TCL_ERROR;

		RT_BOT_CK_MAGIC(bot);

		if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
		    (void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ", "$mged_gui(mged,screen) ", "{Not Plate Mode} ",
				      "{Cannot edit face mode in a non-plate BOT} ", "\"\" ", "0 ", "OK ",
				      (char *)NULL);
		    break;
		}

		if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
		    /* setting mode for all faces */
		    (void)Tcl_VarEval(s->interp, "cad_dialog ", ".bot_err ",
				      "$mged_gui(mged,screen) ", "{Setting Mode for All Faces} ",
				      "{No face is selected, so this operation will modify all the faces in this BOT} ",
				      "\"\" ", "0 ", "OK ", "CANCEL ", (char *)NULL);
		    if (atoi(Tcl_GetStringResult(s->interp)))
			break;

		    face_state = -2;
		} else {
		    /* setting thickness for just one face */
		    face_state = -1;
		    for (i=0; i < bot->num_faces; i++) {
			if (bot_verts[0] == bot->faces[i*3] &&
			    bot_verts[1] == bot->faces[i*3+1] &&
			    bot_verts[2] == bot->faces[i*3+2])
			{
			    face_no = i;
			    face_state = 0;
			    break;
			}
		    }
		    if (face_state < 0) {
			bu_log("Cannot find face with vertices %d %d %d!\n",
			       V3ARGS(bot_verts));
			break;
		    }
		}

		if (face_state > -1)
		    sprintf(fmode, " %d", BU_BITTEST(bot->face_mode, face_no)?1:0);
		else
		    sprintf(fmode, " %d", BU_BITTEST(bot->face_mode, 0)?1:0);

		if (dm_get_pathname(DMP)) {
		    ret_tcl = Tcl_VarEval(s->interp, "cad_radio", " .bot_fmode_radio ", bu_vls_addr(dm_get_pathname(DMP)),
			    " _bot_fmode_result ", "\"BOT Face Mode\"",
			    " \"Select the desired face mode\"", fmode,
			    " { {Thickness centered about hit point} {Thickness appended to hit point} }",
			    " { {This selection will place the plate thickness centered about the hit point} {This selection will place the plate thickness rayward of the hit point} } ",
			    (char *)NULL);
		}
		if (ret_tcl != TCL_OK) {
		    bu_log("ERROR: cad_radio: %s\n", Tcl_GetStringResult(s->interp));
		    break;
		}
		radio_result = Tcl_GetVar(s->interp, "_bot_fmode_result", TCL_GLOBAL_ONLY);

		if (face_state > -1) {
		    if (atoi(radio_result))
			BU_BITSET(bot->face_mode, face_no);
		    else
			BU_BITCLR(bot->face_mode, face_no);
		} else {
		    if (atoi(radio_result)) {
			for (i=0; i<bot->num_faces; i++)
			    BU_BITSET(bot->face_mode, i);
		    } else
			bu_bitv_clear(bot->face_mode);
		}
	    }
	    break;
	case ECMD_BOT_FDEL:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;

		int j, face_no;

		RT_BOT_CK_MAGIC(bot);

		if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
		    bu_log("No Face selected!\n");
		    return;
		}

		face_no = -1;
		for (i=0; i < bot->num_faces; i++) {
		    if (bot_verts[0] == bot->faces[i*3] &&
			bot_verts[1] == bot->faces[i*3+1] &&
			bot_verts[2] == bot->faces[i*3+2])
		    {
			face_no = i;
			break;
		    }
		}
		if (face_no < 0) {
		    bu_log("Cannot find selected face!\n");
		    return;
		}
		bot->num_faces--;
		for (i=face_no; i<bot->num_faces; i++) {
		    j = i + 1;
		    bot->faces[3*i] = bot->faces[3*j];
		    bot->faces[3*i + 1] = bot->faces[3*j + 1];
		    bot->faces[3*i + 2] = bot->faces[3*j + 2];
		    if (bot->thickness)
			bot->thickness[i] = bot->thickness[j];
		}

		if (bot->face_mode) {
		    struct bu_bitv *new_bitv;

		    new_bitv = bu_bitv_new(bot->num_faces);
		    for (i=0; i<(size_t)face_no; i++) {
			if (BU_BITTEST(bot->face_mode, i))
			    BU_BITSET(new_bitv, i);
		    }
		    for (i=face_no; i<bot->num_faces; i++) {
			j = i+1;
			if (BU_BITTEST(bot->face_mode, j))
			    BU_BITSET(new_bitv, i);
		    }
		    bu_bitv_free(bot->face_mode);
		    bot->face_mode = new_bitv;
		}
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
	    }
	    break;
	case ECMD_EXTR_SKT_NAME:
	    {
		struct rt_extrude_internal *extr =
		    (struct rt_extrude_internal *)s->edit_state.es_int.idb_ptr;
		const char *sketch_name;
		int ret_tcl;
		struct directory *dp;
		struct rt_db_internal tmp_ip;
		struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

		RT_EXTRUDE_CK_MAGIC(extr);

		bu_vls_printf(&tcl_cmd, "cad_input_dialog .get_sketch_name $mged_gui(mged,screen) {Select Sketch} {Enter the name of the sketch to be extruded} final_sketch_name %s 0 {{summary \"Enter sketch name\"}} APPLY DISMISS",
			      extr->sketch_name);
		ret_tcl = Tcl_Eval(s->interp, bu_vls_addr(&tcl_cmd));
		if (ret_tcl != TCL_OK) {
		    bu_log("ERROR: %s\n", Tcl_GetStringResult(s->interp));
		    bu_vls_free(&tcl_cmd);
		    break;
		}

		if (atoi(Tcl_GetStringResult(s->interp)) == 1)
		    break;

		bu_vls_free(&tcl_cmd);

		sketch_name = Tcl_GetVar(s->interp, "final_sketch_name", TCL_GLOBAL_ONLY);
		if (extr->sketch_name)
		    bu_free((char *)extr->sketch_name, "extr->sketch_name");
		extr->sketch_name = bu_strdup(sketch_name);

		if (extr->skt) {
		    /* free the old sketch */
		    RT_DB_INTERNAL_INIT(&tmp_ip);
		    tmp_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		    tmp_ip.idb_type = ID_SKETCH;
		    tmp_ip.idb_ptr = (void *)extr->skt;
		    tmp_ip.idb_meth = &OBJ[ID_SKETCH];
		    rt_db_free_internal(&tmp_ip);
		}

		if ((dp = db_lookup(s->dbip, sketch_name, 0)) == RT_DIR_NULL) {
		    bu_log("Warning: %s does not exist!\n",
			   sketch_name);
		    extr->skt = (struct rt_sketch_internal *)NULL;
		} else {
		    /* import the new sketch */

		    if (rt_db_get_internal(&tmp_ip, dp, s->dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
			bu_log("rt_extrude_import: ERROR: Cannot import sketch (%.16s) for extrusion\n",
			       sketch_name);
			extr->skt = (struct rt_sketch_internal *)NULL;
		    } else {
			extr->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
		    }
		}
	    }
	    break;
	case ECMD_EXTR_MOV_H:
	    {
		struct rt_extrude_internal *extr =
		    (struct rt_extrude_internal *)s->edit_state.es_int.idb_ptr;

		RT_EXTRUDE_CK_MAGIC(extr);
		if (s->edit_state.e_inpara) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model coordinates */
			MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_para);
			VSUB2(extr->h, work, extr->V);
		    } else {
			VSUB2(extr->h, s->edit_state.e_para, extr->V);
		    }
		}

		/* check for zero H vector */
		if (MAGNITUDE(extr->h) <= SQRT_SMALL_FASTF) {
		    Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    VSET(extr->h, 0.0, 0.0, 1.0);
		    break;
		}
	    }
	    break;
	case ECMD_EXTR_SCALE_H:
	    {
		struct rt_extrude_internal *extr =
		    (struct rt_extrude_internal *)s->edit_state.es_int.idb_ptr;

		RT_EXTRUDE_CK_MAGIC(extr);

		if (s->edit_state.e_inpara) {
		    /* take s->edit_state.e_mat[15] (path scaling) into account */
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(extr->h);
		    VSCALE(extr->h, extr->h, s->edit_state.es_scale);
		} else if (s->edit_state.es_scale > 0.0) {
		    VSCALE(extr->h, extr->h, s->edit_state.es_scale);
		    s->edit_state.es_scale = 0.0;
		}
	    }
	    break;
	case ECMD_ARB_MAIN_MENU:
	    /* put up control (main) menu for GENARB8s */
	    menu_state->ms_flag = 0;
	    es_edflag = IDLE;
	    mmenu_set(s, MENU_L1, cntrl_menu);
	    break;

	case ECMD_ARB_SPECIFIC_MENU:
	    /* put up specific arb edit menus */
	    menu_state->ms_flag = 0;
	    es_edflag = IDLE;
	    switch (es_menu) {
		case MENU_ARB_MV_EDGE:
		    mmenu_set(s, MENU_L1, which_menu[s->edit_state.e_type-4]);
		    break;
		case MENU_ARB_MV_FACE:
		    mmenu_set(s, MENU_L1, which_menu[s->edit_state.e_type+1]);
		    break;
		case MENU_ARB_ROT_FACE:
		    mmenu_set(s, MENU_L1, which_menu[s->edit_state.e_type+6]);
		    break;
		default:
		    Tcl_AppendResult(s->interp, "Bad menu item.\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
	    }
	    break;

	case ECMD_ARB_MOVE_FACE:
	    /* move face through definite point */
	    if (s->edit_state.e_inpara) {
		arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
		RT_ARB_CK_MAGIC(arb);

		if (mged_variables->mv_context) {
		    /* apply s->edit_state.e_invmat to convert to real model space */
		    MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_para);
		} else {
		    VMOVE(work, s->edit_state.e_para);
		}
		/* change D of planar equation */
		es_peqn[es_menu][W]=VDOT(&es_peqn[es_menu][0], work);
		/* find new vertices, put in record in vector notation */

		(void)rt_arb_calc_points(arb, s->edit_state.e_type, (const plane_t *)es_peqn, &s->tol.tol);
	    }
	    break;
	case ECMD_ARB_SETUP_ROTFACE:
	    arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	    RT_ARB_CK_MAGIC(arb);

	    /* check if point 5 is in the face */
	    pnt5 = 0;
	    for (i=0; i<4; i++) {
		if (rt_arb_vertices[s->edit_state.e_type-4][es_menu*4+i]==5)
		    pnt5=1;
	    }

	    /* special case for arb7 */
	    if (s->edit_state.e_type == ARB7  && pnt5) {
		Tcl_AppendResult(s->interp, "\nFixed vertex is point 5.\n", (char *)NULL);
		fixv = 5;
	    } else {
		fixv = get_rotation_vertex(s);
	    }

	    pr_prompt(s);
	    fixv--;
	    es_edflag = ECMD_ARB_ROTATE_FACE;
	    view_state->vs_flag = 1;	/* draw arrow, etc. */
	    set_e_axes_pos(s, 1);
	    break;

	case ECMD_ARB_ROTATE_FACE:
	    /* rotate a GENARB8 defining plane through a fixed vertex */

	    arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	    RT_ARB_CK_MAGIC(arb);

	    if (s->edit_state.e_inpara) {
		static mat_t invsolr;
		static vect_t tempvec;
		static float rota, fb_a;

		/*
		 * Keyboard parameters in degrees.
		 * First, cancel any existing rotations,
		 * then perform new rotation
		 */
		bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);
		eqp = &es_peqn[es_menu][0];	/* es_menu==plane of interest */
		VMOVE(work, eqp);
		MAT4X3VEC(eqp, invsolr, work);

		if (s->edit_state.e_inpara == 3) {
		    /* 3 params:  absolute X, Y, Z rotations */
		    /* Build completely new rotation change */
		    MAT_IDN(s->edit_state.model_changes);
		    bn_mat_angles(s->edit_state.model_changes,
				  s->edit_state.e_para[0],
				  s->edit_state.e_para[1],
				  s->edit_state.e_para[2]);
		    MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

		    /* Borrow s->edit_state.incr_change matrix here */
		    bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
		    if (mged_variables->mv_context) {
			/* calculate rotations about keypoint */
			bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, s->edit_state.e_keypoint);

			/* We want our final matrix (mat) to xform the original solid
			 * to the position of this instance of the solid, perform the
			 * current edit operations, then xform back.
			 * mat = s->edit_state.e_invmat * edit * s->edit_state.e_mat
			 */
			bn_mat_mul(mat1, edit, s->edit_state.e_mat);
			bn_mat_mul(mat, s->edit_state.e_invmat, mat1);
			MAT_IDN(s->edit_state.incr_change);
			/* work contains original es_peqn[es_menu][0] */
			MAT4X3VEC(eqp, mat, work);
		    } else {
			VMOVE(work, eqp);
			MAT4X3VEC(eqp, s->edit_state.model_changes, work);
		    }
		} else if (s->edit_state.e_inpara == 2) {
		    /* 2 parameters:  rot, fb were given */
		    rota= s->edit_state.e_para[0] * DEG2RAD;
		    fb_a  = s->edit_state.e_para[1] * DEG2RAD;

		    /* calculate normal vector (length = 1) from rot, struct fb */
		    es_peqn[es_menu][0] = cos(fb_a) * cos(rota);
		    es_peqn[es_menu][1] = cos(fb_a) * sin(rota);
		    es_peqn[es_menu][2] = sin(fb_a);
		} else {
		    Tcl_AppendResult(s->interp, "Must be < rot fb | xdeg ydeg zdeg >\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}

		/* point notation of fixed vertex */
		VMOVE(tempvec, arb->pt[fixv]);

		/* set D of planar equation to anchor at fixed vertex */
		/* es_menu == plane of interest */
		es_peqn[es_menu][W]=VDOT(eqp, tempvec);

		/* Clear out solid rotation */
		MAT_IDN(s->edit_state.model_changes);

	    } else {
		/* Apply incremental changes */
		static vect_t tempvec;

		eqp = &es_peqn[es_menu][0];
		VMOVE(work, eqp);
		MAT4X3VEC(eqp, s->edit_state.incr_change, work);

		/* point notation of fixed vertex */
		VMOVE(tempvec, arb->pt[fixv]);

		/* set D of planar equation to anchor at fixed vertex */
		/* es_menu == plane of interest */
		es_peqn[es_menu][W]=VDOT(eqp, tempvec);
	    }

	    (void)rt_arb_calc_points(arb, s->edit_state.e_type, (const plane_t *)es_peqn, &s->tol.tol);
	    MAT_IDN(s->edit_state.incr_change);

	    /* no need to calc_planes again */
	    replot_editing_solid(s);

	    s->edit_state.e_inpara = 0;
	    return;

	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    {
		mat_t scalemat;

		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
		es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		if (s->edit_state.e_inpara) {
		    /* accumulate the scale factor */
		    s->edit_state.es_scale = s->edit_state.e_para[0] / s->edit_state.acc_sc_sol;
		    s->edit_state.acc_sc_sol = s->edit_state.e_para[0];
		}

		bn_mat_scale_about_pnt(scalemat, s->edit_state.e_keypoint, s->edit_state.es_scale);
		bn_mat_mul(mat1, scalemat, s->edit_state.e_mat);
		bn_mat_mul(mat, s->edit_state.e_invmat, mat1);
		transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);

		/* reset solid scale factor */
		s->edit_state.es_scale = 1.0;
	    }
	    break;

	case STRANS:
	    /* translate solid */
	    {
		vect_t delta;

		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
		es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		if (s->edit_state.e_inpara) {
		    /* Need vector from current vertex/keypoint
		     * to desired new location.
		     */
		    if (mged_variables->mv_context) {
			/* move solid so that s->edit_state.e_keypoint is at position s->edit_state.e_para */
			vect_t raw_para;

			MAT4X3PNT(raw_para, s->edit_state.e_invmat, s->edit_state.e_para);
			MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_keypoint);
			VSUB2(delta, work, raw_para);
			MAT_IDN(mat);
			MAT_DELTAS_VEC_NEG(mat, delta);
		    } else {
			/* move solid to position s->edit_state.e_para */
			/* move solid to position s->edit_state.e_para */
			MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_keypoint);
			VSUB2(delta, work, s->edit_state.e_para);
			MAT_IDN(mat);
			MAT_DELTAS_VEC_NEG(mat, delta);
		    }
		    transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);
		}
	    }
	    break;
	case ECMD_VTRANS:
	    /* translate a vertex */
	    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
	    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
	    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
	    bot_verts[0] = -1;
	    bot_verts[1] = -1;
	    bot_verts[2] = -1;
	    if (s->edit_state.e_mvalid) {
		/* Mouse parameter:  new position in model space */
		VMOVE(s->edit_state.e_para, s->edit_state.e_mparam);
		s->edit_state.e_inpara = 1;
	    }
	    if (s->edit_state.e_inpara) {


		/* Keyboard parameter:  new position in model space.
		 * XXX for now, splines only here */
		struct rt_nurb_internal *sip =
		    (struct rt_nurb_internal *) s->edit_state.es_int.idb_ptr;
		struct face_g_snurb *surf;
		fastf_t *fp;

		RT_NURB_CK_MAGIC(sip);
		surf = sip->srfs[spl_surfno];
		NMG_CK_SNURB(surf);
		fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
		if (mged_variables->mv_context) {
		    /* apply s->edit_state.e_invmat to convert to real model space */
		    MAT4X3PNT(fp, s->edit_state.e_invmat, s->edit_state.e_para);
		} else {
		    VMOVE(fp, s->edit_state.e_para);
		}
	    }
	    break;

	case ECMD_CLINE_SCALE_H:
	    /*
	     * Scale height vector
	     */
	    {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

		RT_CLINE_CK_MAGIC(cli);

		if (s->edit_state.e_inpara) {
		    s->edit_state.e_para[0] *= s->edit_state.e_mat[15];
		    s->edit_state.es_scale = s->edit_state.e_para[0] / MAGNITUDE(cli->h);
		    VSCALE(cli->h, cli->h, s->edit_state.es_scale);
		} else if (s->edit_state.es_scale > 0.0) {
		    VSCALE(cli->h, cli->h, s->edit_state.es_scale);
		    s->edit_state.es_scale = 0.0;
		}
	    }
	    break;

	case ECMD_CLINE_SCALE_R:
	    /*
	     * Scale radius
	     */
	    {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

		RT_CLINE_CK_MAGIC(cli);

		if (s->edit_state.e_inpara)
		    cli->radius = s->edit_state.e_para[0];
		else if (s->edit_state.es_scale > 0.0) {
		    cli->radius *= s->edit_state.es_scale;
		    s->edit_state.es_scale = 0.0;
		}
	    }
	    break;

	case ECMD_CLINE_SCALE_T:
	    /*
	     * Scale plate thickness
	     */
	    {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

		RT_CLINE_CK_MAGIC(cli);

		if (s->edit_state.e_inpara)
		    cli->thickness = s->edit_state.e_para[0];
		else if (s->edit_state.es_scale > 0.0) {
		    cli->thickness *= s->edit_state.es_scale;
		    s->edit_state.es_scale = 0.0;
		}
	    }
	    break;

	case ECMD_CLINE_MOVE_H:
	    /*
	     * Move end of height vector
	     */
	    {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

		RT_CLINE_CK_MAGIC(cli);

		if (s->edit_state.e_inpara) {
		    if (mged_variables->mv_context) {
			MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_para);
			VSUB2(cli->h, work, cli->v);
		    } else
			VSUB2(cli->h, s->edit_state.e_para, cli->v);
		}
		/* check for zero H vector */
		if (MAGNITUDE(cli->h) <= SQRT_SMALL_FASTF) {
		    Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    VSET(cli->h, 0.0, 0.0, 1.0);
		    break;
		}
	    }
	    break;

	case ECMD_TGC_MV_H:
	    /*
	     * Move end of H of tgc, keeping plates perpendicular
	     * to H vector.
	     */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

		RT_TGC_CK_MAGIC(tgc);
		if (s->edit_state.e_inpara) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model coordinates */
			MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_para);
			VSUB2(tgc->h, work, tgc->v);
		    } else {
			VSUB2(tgc->h, s->edit_state.e_para, tgc->v);
		    }
		}

		/* check for zero H vector */
		if (MAGNITUDE(tgc->h) <= SQRT_SMALL_FASTF) {
		    Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    VSET(tgc->h, 0.0, 0.0, 1.0);
		    break;
		}

		/* have new height vector -- redefine rest of tgc */
		la = MAGNITUDE(tgc->a);
		lb = MAGNITUDE(tgc->b);
		lc = MAGNITUDE(tgc->c);
		ld = MAGNITUDE(tgc->d);

		/* find 2 perpendicular vectors normal to H for new A, B */
		bn_vec_perp(tgc->b, tgc->h);
		VCROSS(tgc->a, tgc->b, tgc->h);
		VUNITIZE(tgc->a);
		VUNITIZE(tgc->b);

		/* Create new C, D from unit length A, B, with previous len */
		VSCALE(tgc->c, tgc->a, lc);
		VSCALE(tgc->d, tgc->b, ld);

		/* Restore original vector lengths to A, B */
		VSCALE(tgc->a, tgc->a, la);
		VSCALE(tgc->b, tgc->b, lb);
	    }
	    break;

	case ECMD_TGC_MV_HH:
	    /* Move end of H of tgc - leave ends alone */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

		RT_TGC_CK_MAGIC(tgc);
		if (s->edit_state.e_inpara) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model coordinates */
			MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_para);
			VSUB2(tgc->h, work, tgc->v);
		    } else {
			VSUB2(tgc->h, s->edit_state.e_para, tgc->v);
		    }
		}

		/* check for zero H vector */
		if (MAGNITUDE(tgc->h) <= SQRT_SMALL_FASTF) {
		    Tcl_AppendResult(s->interp, "Zero H vector not allowed, resetting to +Z\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    VSET(tgc->h, 0.0, 0.0, 1.0);
		    break;
		}
	    }
	    break;

	case PSCALE:
	    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
	    bot_verts[0] = -1;
	    bot_verts[1] = -1;
	    bot_verts[2] = -1;
	    pscale(s);
	    break;

	case PTARB:	/* move an ARB point */
	case EARB:   /* edit an ARB edge */
	    if (s->edit_state.e_inpara) {
		if (mged_variables->mv_context) {
		    /* apply s->edit_state.e_invmat to convert to real model space */
		    MAT4X3PNT(work, s->edit_state.e_invmat, s->edit_state.e_para);
		} else {
		    VMOVE(work, s->edit_state.e_para);
		}
		editarb(s, work);
	    }
	    break;

	case SROT:
	    /* rot solid about vertex */
	    {
		es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
		es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
		es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
		bot_verts[0] = -1;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		if (s->edit_state.e_inpara) {
		    static mat_t invsolr;
		    /*
		     * Keyboard parameters:  absolute x, y, z rotations,
		     * in degrees.  First, cancel any existing rotations,
		     * then perform new rotation
		     */
		    bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);

		    /* Build completely new rotation change */
		    MAT_IDN(s->edit_state.model_changes);
		    bn_mat_angles(s->edit_state.model_changes,
				  s->edit_state.e_para[0],
				  s->edit_state.e_para[1],
				  s->edit_state.e_para[2]);
		    /* Borrow s->edit_state.incr_change matrix here */
		    bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
		    MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

		    /* Apply new rotation to solid */
		    /* Clear out solid rotation */
		    MAT_IDN(s->edit_state.model_changes);
		} else {
		    /* Apply incremental changes already in s->edit_state.incr_change */
		}
		/* Apply changes to solid */
		/* xlate keypoint to origin, rotate, then put back. */
		switch (mged_variables->mv_rotate_about) {
		    case 'v':       /* View Center */
			VSET(work, 0.0, 0.0, 0.0);
			MAT4X3PNT(rot_point, view_state->vs_gvp->gv_view2model, work);
			break;
		    case 'e':       /* Eye */
			VSET(work, 0.0, 0.0, 1.0);
			MAT4X3PNT(rot_point, view_state->vs_gvp->gv_view2model, work);
			break;
		    case 'm':       /* Model Center */
			VSETALL(rot_point, 0.0);
			break;
		    case 'k':       /* Key Point */
		    default:
			VMOVE(rot_point, s->edit_state.e_keypoint);
			break;
		}

		if (mged_variables->mv_context) {
		    /* calculate rotations about keypoint */
		    bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, rot_point);

		    /* We want our final matrix (mat) to xform the original solid
		     * to the position of this instance of the solid, perform the
		     * current edit operations, then xform back.
		     * mat = s->edit_state.e_invmat * edit * s->edit_state.e_mat
		     */
		    bn_mat_mul(mat1, edit, s->edit_state.e_mat);
		    bn_mat_mul(mat, s->edit_state.e_invmat, mat1);
		} else {
		    MAT4X3PNT(work, s->edit_state.e_invmat, rot_point);
		    bn_mat_xform_about_pnt(mat, s->edit_state.incr_change, work);
		}
		transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);

		MAT_IDN(s->edit_state.incr_change);
	    }
	    break;

	case ECMD_EXTR_ROT_H:
	    /* rotate height vector */
	    {
		struct rt_extrude_internal *extr =
		    (struct rt_extrude_internal *)s->edit_state.es_int.idb_ptr;

		RT_EXTRUDE_CK_MAGIC(extr);
		if (s->edit_state.e_inpara) {
		    static mat_t invsolr;
		    /*
		     * Keyboard parameters:  absolute x, y, z rotations,
		     * in degrees.  First, cancel any existing rotations,
		     * then perform new rotation
		     */
		    bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);

		    /* Build completely new rotation change */
		    MAT_IDN(s->edit_state.model_changes);
		    bn_mat_angles(s->edit_state.model_changes,
				  s->edit_state.e_para[0],
				  s->edit_state.e_para[1],
				  s->edit_state.e_para[2]);
		    /* Borrow s->edit_state.incr_change matrix here */
		    bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
		    MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

		    /* Apply new rotation to solid */
		    /* Clear out solid rotation */
		    MAT_IDN(s->edit_state.model_changes);
		} else {
		    /* Apply incremental changes already in s->edit_state.incr_change */
		}

		if (mged_variables->mv_context) {
		    /* calculate rotations about keypoint */
		    bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, s->edit_state.e_keypoint);

		    /* We want our final matrix (mat) to xform the original solid
		     * to the position of this instance of the solid, perform the
		     * current edit operations, then xform back.
		     * mat = s->edit_state.e_invmat * edit * s->edit_state.e_mat
		     */
		    bn_mat_mul(mat1, edit, s->edit_state.e_mat);
		    bn_mat_mul(mat, s->edit_state.e_invmat, mat1);
		    MAT4X3VEC(extr->h, mat, extr->h);
		} else {
		    MAT4X3VEC(extr->h, s->edit_state.incr_change, extr->h);
		}

		MAT_IDN(s->edit_state.incr_change);
	    }
	    break;

	case ECMD_TGC_ROT_H:
	    /* rotate height vector */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

		RT_TGC_CK_MAGIC(tgc);
		if (s->edit_state.e_inpara) {
		    static mat_t invsolr;
		    /*
		     * Keyboard parameters:  absolute x, y, z rotations,
		     * in degrees.  First, cancel any existing rotations,
		     * then perform new rotation
		     */
		    bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);

		    /* Build completely new rotation change */
		    MAT_IDN(s->edit_state.model_changes);
		    bn_mat_angles(s->edit_state.model_changes,
				  s->edit_state.e_para[0],
				  s->edit_state.e_para[1],
				  s->edit_state.e_para[2]);
		    /* Borrow s->edit_state.incr_change matrix here */
		    bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
		    MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

		    /* Apply new rotation to solid */
		    /* Clear out solid rotation */
		    MAT_IDN(s->edit_state.model_changes);
		} else {
		    /* Apply incremental changes already in s->edit_state.incr_change */
		}

		if (mged_variables->mv_context) {
		    /* calculate rotations about keypoint */
		    bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, s->edit_state.e_keypoint);

		    /* We want our final matrix (mat) to xform the original solid
		     * to the position of this instance of the solid, perform the
		     * current edit operations, then xform back.
		     * mat = s->edit_state.e_invmat * edit * s->edit_state.e_mat
		     */
		    bn_mat_mul(mat1, edit, s->edit_state.e_mat);
		    bn_mat_mul(mat, s->edit_state.e_invmat, mat1);
		    MAT4X3VEC(tgc->h, mat, tgc->h);
		} else {
		    MAT4X3VEC(tgc->h, s->edit_state.incr_change, tgc->h);
		}

		MAT_IDN(s->edit_state.incr_change);
	    }
	    break;

	case ECMD_TGC_ROT_AB:
	    /* rotate surfaces AxB and CxD (tgc) */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;

		RT_TGC_CK_MAGIC(tgc);
		if (s->edit_state.e_inpara) {
		    static mat_t invsolr;
		    /*
		     * Keyboard parameters:  absolute x, y, z rotations,
		     * in degrees.  First, cancel any existing rotations,
		     * then perform new rotation
		     */
		    bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);

		    /* Build completely new rotation change */
		    MAT_IDN(s->edit_state.model_changes);
		    bn_mat_angles(s->edit_state.model_changes,
				  s->edit_state.e_para[0],
				  s->edit_state.e_para[1],
				  s->edit_state.e_para[2]);
		    /* Borrow s->edit_state.incr_change matrix here */
		    bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
		    MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

		    /* Apply new rotation to solid */
		    /* Clear out solid rotation */
		    MAT_IDN(s->edit_state.model_changes);
		} else {
		    /* Apply incremental changes already in s->edit_state.incr_change */
		}

		if (mged_variables->mv_context) {
		    /* calculate rotations about keypoint */
		    bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, s->edit_state.e_keypoint);

		    /* We want our final matrix (mat) to xform the original solid
		     * to the position of this instance of the solid, perform the
		     * current edit operations, then xform back.
		     * mat = s->edit_state.e_invmat * edit * s->edit_state.e_mat
		     */
		    bn_mat_mul(mat1, edit, s->edit_state.e_mat);
		    bn_mat_mul(mat, s->edit_state.e_invmat, mat1);
		    MAT4X3VEC(tgc->a, mat, tgc->a);
		    MAT4X3VEC(tgc->b, mat, tgc->b);
		    MAT4X3VEC(tgc->c, mat, tgc->c);
		    MAT4X3VEC(tgc->d, mat, tgc->d);
		} else {
		    MAT4X3VEC(tgc->a, s->edit_state.incr_change, tgc->a);
		    MAT4X3VEC(tgc->b, s->edit_state.incr_change, tgc->b);
		    MAT4X3VEC(tgc->c, s->edit_state.incr_change, tgc->c);
		    MAT4X3VEC(tgc->d, s->edit_state.incr_change, tgc->d);
		}
		MAT_IDN(s->edit_state.incr_change);
	    }
	    break;

	case ECMD_HYP_ROT_H:
	    /* rotate hyperboloid height vector */
	    {
		struct rt_hyp_internal *hyp =
		    (struct rt_hyp_internal *)s->edit_state.es_int.idb_ptr;

		RT_HYP_CK_MAGIC(hyp);
		if (s->edit_state.e_inpara) {
		    static mat_t invsolr;
		    /*
		     * Keyboard parameters:  absolute x, y, z rotations,
		     * in degrees.  First, cancel any existing rotations,
		     * then perform new rotation
		     */
		    bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);

		    /* Build completely new rotation change */
		    MAT_IDN(s->edit_state.model_changes);
		    bn_mat_angles(s->edit_state.model_changes,
				  s->edit_state.e_para[0],
				  s->edit_state.e_para[1],
				  s->edit_state.e_para[2]);
		    /* Borrow s->edit_state.incr_change matrix here */
		    bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
		    MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

		    /* Apply new rotation to solid */
		    /* Clear out solid rotation */
		    MAT_IDN(s->edit_state.model_changes);
		} else {
		    /* Apply incremental changes already in s->edit_state.incr_change */
		}

		if (mged_variables->mv_context) {
		    /* calculate rotations about keypoint */
		    bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, s->edit_state.e_keypoint);

		    /* We want our final matrix (mat) to xform the original solid
		     * to the position of this instance of the solid, perform the
		     * current edit operations, then xform back.
		     * mat = s->edit_state.e_invmat * edit * s->edit_state.e_mat
		     */
		    bn_mat_mul(mat1, edit, s->edit_state.e_mat);
		    bn_mat_mul(mat, s->edit_state.e_invmat, mat1);

		    MAT4X3VEC(hyp->hyp_Hi, mat, hyp->hyp_Hi);
		} else {
		    MAT4X3VEC(hyp->hyp_Hi, s->edit_state.incr_change, hyp->hyp_Hi);
		}
	    }
	    MAT_IDN(s->edit_state.incr_change);
	    break;

	case ECMD_ETO_ROT_C:
	    /* rotate ellipse semi-major axis vector */
	    {
		struct rt_eto_internal *eto =
		    (struct rt_eto_internal *)s->edit_state.es_int.idb_ptr;

		RT_ETO_CK_MAGIC(eto);
		if (s->edit_state.e_inpara) {
		    static mat_t invsolr;
		    /*
		     * Keyboard parameters:  absolute x, y, z rotations,
		     * in degrees.  First, cancel any existing rotations,
		     * then perform new rotation
		     */
		    bn_mat_inv(invsolr, s->edit_state.acc_rot_sol);

		    /* Build completely new rotation change */
		    MAT_IDN(s->edit_state.model_changes);
		    bn_mat_angles(s->edit_state.model_changes,
				  s->edit_state.e_para[0],
				  s->edit_state.e_para[1],
				  s->edit_state.e_para[2]);
		    /* Borrow s->edit_state.incr_change matrix here */
		    bn_mat_mul(s->edit_state.incr_change, s->edit_state.model_changes, invsolr);
		    MAT_COPY(s->edit_state.acc_rot_sol, s->edit_state.model_changes);

		    /* Apply new rotation to solid */
		    /* Clear out solid rotation */
		    MAT_IDN(s->edit_state.model_changes);
		} else {
		    /* Apply incremental changes already in s->edit_state.incr_change */
		}

		if (mged_variables->mv_context) {
		    /* calculate rotations about keypoint */
		    bn_mat_xform_about_pnt(edit, s->edit_state.incr_change, s->edit_state.e_keypoint);

		    /* We want our final matrix (mat) to xform the original solid
		     * to the position of this instance of the solid, perform the
		     * current edit operations, then xform back.
		     * mat = s->edit_state.e_invmat * edit * s->edit_state.e_mat
		     */
		    bn_mat_mul(mat1, edit, s->edit_state.e_mat);
		    bn_mat_mul(mat, s->edit_state.e_invmat, mat1);

		    MAT4X3VEC(eto->eto_C, mat, eto->eto_C);
		} else {
		    MAT4X3VEC(eto->eto_C, s->edit_state.incr_change, eto->eto_C);
		}
	    }
	    MAT_IDN(s->edit_state.incr_change);
	    break;

	case ECMD_NMG_EPICK:
	    /* XXX Nothing to do here (yet), all done in mouse routine. */
	    break;
	case ECMD_NMG_EMOVE:
	    {
		point_t new_pt;

		if (!es_eu) {
		    Tcl_AppendResult(s->interp, "No edge selected!\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}
		NMG_CK_EDGEUSE(es_eu);

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for edge move\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		if (!nmg_find_fu_of_eu(es_eu) && *es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		    struct loopuse *lu;
		    fastf_t area;
		    plane_t pl;

		    /* this edge is in a wire loop
		     * keep the loop planar
		     */
		    lu = es_eu->up.lu_p;
		    NMG_CK_LOOPUSE(lu);

		    /* get plane equation for loop */
		    area = nmg_loop_plane_area(lu, pl);
		    if (area > 0.0) {
			vect_t view_z_dir;
			vect_t view_dir;
			fastf_t dist;

			/* Get view direction vector */
			VSET(view_z_dir, 0.0, 0.0, 1.0);
			MAT4X3VEC(view_dir, view_state->vs_gvp->gv_view2model, view_z_dir);

			/* intersect line through new_pt with plane of loop */
			if (bg_isect_line3_plane(&dist, new_pt, view_dir, pl, &s->tol.tol) < 1) {
			    /* line does not intersect plane, don't do an esplit */
			    Tcl_AppendResult(s->interp, "Edge Move: Cannot place new point in plane of loop\n", (char *)NULL);
			    mged_print_result(s, TCL_ERROR);
			    break;
			}
			VJOIN1(new_pt, new_pt, dist, view_dir);
		    }
		}

		if (nmg_move_edge_thru_pnt(es_eu, new_pt, &s->tol.tol) < 0) {
		    VPRINT("Unable to hit", new_pt);
		}
	    }
	    break;

	case ECMD_NMG_EKILL:
	    {
		struct model *m;
		struct edge_g_lseg *eg;

		if (!es_eu) {
		    Tcl_AppendResult(s->interp, "No edge selected!\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}
		NMG_CK_EDGEUSE(es_eu);

		m = nmg_find_model(&es_eu->l.magic);

		if (*es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		    struct loopuse *lu;
		    struct edgeuse *prev_eu, *next_eu;

		    lu = es_eu->up.lu_p;
		    NMG_CK_LOOPUSE(lu);

		    if (*lu->up.magic_p != NMG_SHELL_MAGIC) {
			/* Currently can only kill wire edges or edges in wire loops */
			Tcl_AppendResult(s->interp, "Currently, we can only kill wire edges or edges in wire loops\n", (char *)NULL);
			mged_print_result(s, TCL_ERROR);
			es_edflag = IDLE;
			break;
		    }

		    prev_eu = BU_LIST_PPREV_CIRC(edgeuse, &es_eu->l);
		    NMG_CK_EDGEUSE(prev_eu);

		    if (prev_eu == es_eu) {
			/* only one edge left in the loop
			 * make it an edge to/from same vertex
			 */
			if (es_eu->vu_p->v_p == es_eu->eumate_p->vu_p->v_p) {
			    /* refuse to delete last edge that runs
			     * to/from same vertex
			     */
			    Tcl_AppendResult(s->interp, "Cannot delete last edge running to/from same vertex\n", (char *)NULL);
			    mged_print_result(s, TCL_ERROR);
			    break;
			}
			NMG_CK_EDGEUSE(es_eu->eumate_p);
			nmg_movevu(es_eu->eumate_p->vu_p, es_eu->vu_p->v_p);
			break;
		    }

		    next_eu = BU_LIST_PNEXT_CIRC(edgeuse, &es_eu->l);
		    NMG_CK_EDGEUSE(next_eu);

		    nmg_movevu(next_eu->vu_p, es_eu->vu_p->v_p);
		    if (nmg_keu(es_eu)) {
			/* Should never happen! */
			bu_exit(EXIT_FAILURE,  "sedit(s): killed edge and emptied loop!\n");
		    }
		    es_eu = prev_eu;
		    nmg_rebound(m, &s->tol.tol);

		    /* fix edge geometry for modified edge (next_eu) */
		    eg = next_eu->g.lseg_p;
		    NMG_CK_EDGE_G_LSEG(eg);
		    VMOVE(eg->e_pt, next_eu->vu_p->v_p->vg_p->coord);
		    VSUB2(eg->e_dir, next_eu->eumate_p->vu_p->v_p->vg_p->coord, next_eu->vu_p->v_p->vg_p->coord);

		    break;
		} else if (*es_eu->up.magic_p == NMG_SHELL_MAGIC) {
		    /* wire edge, just kill it */
		    (void)nmg_keu(es_eu);
		    es_eu = (struct edgeuse *)NULL;
		    nmg_rebound(m, &s->tol.tol);
		}
	    }

	    /* fall through */

	case ECMD_NMG_ESPLIT:
	    {
		struct vertex *v=(struct vertex *)NULL;
		struct edge_g_lseg *eg;
		struct model *m;
		point_t new_pt;
		fastf_t area;
		plane_t pl;

		if (!es_eu) {
		    Tcl_AppendResult(s->interp, "No edge selected!\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}
		NMG_CK_EDGEUSE(es_eu);
		m = nmg_find_model(&es_eu->l.magic);
		NMG_CK_MODEL(m);
		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for edge split\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		if (*es_eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		    struct loopuse *lu;

		    lu = es_eu->up.lu_p;
		    NMG_CK_LOOPUSE(lu);

		    /* Currently, can only split wire edges or edges in wire loops */
		    if (*lu->up.magic_p != NMG_SHELL_MAGIC) {
			Tcl_AppendResult(s->interp, "Currently, we can only split wire edges or edges in wire loops\n", (char *)NULL);
			es_edflag = IDLE;
			mged_print_result(s, TCL_ERROR);
			break;
		    }

		    /* get plane equation for loop */
		    area = nmg_loop_plane_area(lu, pl);
		    if (area > 0.0) {
			vect_t view_z_dir;
			vect_t view_dir;
			fastf_t dist;

			/* Get view direction vector */
			VSET(view_z_dir, 0.0, 0.0, 1.0);
			MAT4X3VEC(view_dir, view_state->vs_gvp->gv_view2model, view_z_dir);

			/* intersect line through new_pt with plane of loop */
			if (bg_isect_line3_plane(&dist, new_pt, view_dir, pl, &s->tol.tol) < 1) {
			    /* line does not intersect plane, don't do an esplit */
			    Tcl_AppendResult(s->interp, "Edge Split: Cannot place new point in plane of loop\n", (char *)NULL);
			    mged_print_result(s, TCL_ERROR);
			    break;
			}
			VJOIN1(new_pt, new_pt, dist, view_dir);
		    }
		}
		es_eu = nmg_esplit(v, es_eu, 0);
		nmg_vertex_gv(es_eu->vu_p->v_p, new_pt);
		nmg_rebound(m, &s->tol.tol);
		eg = es_eu->g.lseg_p;
		NMG_CK_EDGE_G_LSEG(eg);
		VMOVE(eg->e_pt, new_pt);
		VSUB2(eg->e_dir, es_eu->eumate_p->vu_p->v_p->vg_p->coord, new_pt);
	    }
	    break;
	case ECMD_NMG_LEXTRU:
	    {
		fastf_t dist;
		point_t to_pt;
		vect_t extrude_vec;
		struct loopuse *new_lu;
		struct faceuse *fu;
		struct model *m;
		plane_t new_lu_pl;
		fastf_t area;

		if (s->edit_state.e_mvalid) {
		    VMOVE(to_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(to_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(to_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara == 1) {
		    VJOIN1(to_pt, lu_keypoint, s->edit_state.e_para[0], lu_pl);
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for loop extrusion\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}

		VSUB2(extrude_vec, to_pt, lu_keypoint);

		if (bg_isect_line3_plane(&dist, to_pt, extrude_vec, lu_pl, &s->tol.tol) < 1) {
		    Tcl_AppendResult(s->interp, "Cannot extrude parallel to plane of loop\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}

		if (BU_LIST_NON_EMPTY(&es_s->fu_hd)) {
		    struct nmgregion *r;

		    r = es_s->r_p;
		    (void) nmg_ks(es_s);
		    es_s = nmg_msv(r);
		}

		new_lu = nmg_dup_loop(lu_copy, &es_s->l.magic, (long **)0);
		area = nmg_loop_plane_area(new_lu, new_lu_pl);
		if (area < 0.0) {
		    Tcl_AppendResult(s->interp, "loop to be extruded as no area!\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}

		if (VDOT(extrude_vec, new_lu_pl) > 0.0) {
		    plane_t tmp_pl;

		    fu = nmg_mf(new_lu->lumate_p);
		    NMG_CK_FACEUSE(fu);
		    HREVERSE(tmp_pl, new_lu_pl);
		    nmg_face_g(fu, tmp_pl);
		} else {
		    fu = nmg_mf(new_lu);
		    NMG_CK_FACEUSE(fu);
		    nmg_face_g(fu, new_lu_pl);
		}

		(void)nmg_extrude_face(fu, extrude_vec, s->vlfree, &s->tol.tol);

		nmg_fix_normals(fu->s_p, s->vlfree, &s->tol.tol);

		m = nmg_find_model(&fu->l.magic);
		nmg_rebound(m, &s->tol.tol);
		(void)nmg_ck_geometry(m, s->vlfree, &s->tol.tol);

		es_eu = (struct edgeuse *)NULL;

		replot_editing_solid(s);
		view_state->vs_flag = 1;
	    }
	    break;
	case ECMD_PIPE_PICK:
	    {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt;

		RT_PIPE_CK_MAGIC(pipeip);

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for segment selection\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		es_pipe_pnt = find_pipe_pnt_nearest_pnt(s, &pipeip->pipe_segs_head, new_pt);
		if (!es_pipe_pnt) {
		    Tcl_AppendResult(s->interp, "No PIPE segment selected\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		} else
		    rt_pipe_pnt_print(es_pipe_pnt, s->dbip->dbi_base2local);
	    }
	    break;
	case ECMD_PIPE_SPLIT:
	    {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt;

		RT_PIPE_CK_MAGIC(pipeip);

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for segment split\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		if (!es_pipe_pnt) {
		    Tcl_AppendResult(s->interp, "No pipe segment selected\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}

		pipe_split_pnt(&pipeip->pipe_segs_head, es_pipe_pnt, new_pt);
	    }
	    break;
	case ECMD_PIPE_PT_MOVE:
	    {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt;

		RT_PIPE_CK_MAGIC(pipeip);

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for segment movement\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		if (!es_pipe_pnt) {
		    Tcl_AppendResult(s->interp, "No pipe segment selected\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}

		pipe_move_pnt(s, pipeip, es_pipe_pnt, new_pt);
	    }
	    break;
	case ECMD_PIPE_PT_ADD:
	    {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt;

		RT_PIPE_CK_MAGIC(pipeip);

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for 'append segment'\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		es_pipe_pnt = pipe_add_pnt(pipeip, es_pipe_pnt, new_pt);
	    }
	    break;
	case ECMD_PIPE_PT_INS:
	    {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt;

		RT_PIPE_CK_MAGIC(pipeip);

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for 'prepend segment'\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		pipe_ins_pnt(pipeip, es_pipe_pnt, new_pt);
	    }
	    break;
	case ECMD_PIPE_PT_DEL:
	    {
		if (!es_pipe_pnt) {
		    Tcl_AppendResult(s->interp, "No pipe segment selected\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}
		es_pipe_pnt = pipe_del_pnt(s, es_pipe_pnt);
	    }
	    break;
	case ECMD_ARS_PICK_MENU:
	    /* put up point pick menu for ARS solid */
	    menu_state->ms_flag = 0;
	    es_edflag = ECMD_ARS_PICK;
	    mmenu_set(s, MENU_L1, ars_pick_menu);
	    break;
	case ECMD_ARS_EDIT_MENU:
	    /* put up main ARS edit menu */
	    menu_state->ms_flag = 0;
	    es_edflag = IDLE;
	    mmenu_set(s, MENU_L1, ars_menu);
	    break;
	case ECMD_ARS_PICK:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		point_t pick_pt;
		vect_t view_dir;
		vect_t z_dir;
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t selected_pt;

		RT_ARS_CK_MAGIC(ars);

		if (s->edit_state.e_mvalid) {
		    VMOVE(pick_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(pick_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(pick_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for 'pick point'\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara)
		    break;

		/* Get view direction vector */
		VSET(z_dir, 0.0, 0.0, 1.0);
		MAT4X3VEC(view_dir, view_state->vs_gvp->gv_view2model, z_dir);
		find_ars_nearest_pnt(&es_ars_crv, &es_ars_col, ars, pick_pt, view_dir);
		VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
		VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
		bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
		       es_ars_col, es_ars_crv, V3ARGS(selected_pt));

		bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		mged_print_result(s, TCL_ERROR);
		bu_vls_free(&tmp_vls);
	    }
	    break;
	case ECMD_ARS_NEXT_PT:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t selected_pt;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv >= 0 && es_ars_col >= 0) {
		    es_ars_col++;
		    if ((size_t)es_ars_col >= ars->pts_per_curve)
			es_ars_col = 0;
		    VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
		    VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
		    bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
			   es_ars_col, es_ars_crv, V3ARGS(selected_pt));

		    bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
		    Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    bu_vls_free(&tmp_vls);
		}
	    }
	    break;
	case ECMD_ARS_PREV_PT:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t selected_pt;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv >= 0 && es_ars_col >= 0) {
		    es_ars_col--;
		    if (es_ars_col < 0)
			es_ars_col = ars->pts_per_curve - 1;
		    VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
		    VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
		    bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
			   es_ars_col, es_ars_crv, V3ARGS(selected_pt));

		    bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
		    Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    bu_vls_free(&tmp_vls);
		}
	    }
	    break;
	case ECMD_ARS_NEXT_CRV:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t selected_pt;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv >= 0 && es_ars_col >= 0) {
		    es_ars_crv++;
		    if ((size_t)es_ars_crv >= ars->ncurves)
			es_ars_crv = 0;
		    VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
		    VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
		    bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
			   es_ars_col, es_ars_crv, V3ARGS(selected_pt));

		    bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
		    Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    bu_vls_free(&tmp_vls);
		}
	    }
	    break;
	case ECMD_ARS_PREV_CRV:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t selected_pt;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv >= 0 && es_ars_col >= 0) {
		    es_ars_crv--;
		    if (es_ars_crv < 0)
			es_ars_crv = ars->ncurves - 1;
		    VMOVE(es_pt, &ars->curves[es_ars_crv][es_ars_col*3]);
		    VSCALE(selected_pt, es_pt, s->dbip->dbi_base2local);
		    bu_log("Selected point #%d from curve #%d (%f %f %f)\n",
			   es_ars_col, es_ars_crv, V3ARGS(selected_pt));

		    bu_vls_printf(&tmp_vls, "Selected point #%d from curve #%d (%f %f %f)\n", es_ars_col, es_ars_crv, V3ARGS(selected_pt));
		    Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    bu_vls_free(&tmp_vls);
		}
	    }
	    break;
	case ECMD_ARS_DUP_CRV:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t **curves;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    bu_log("No ARS point selected\n");
		    break;
		}

		curves = (fastf_t **)bu_malloc((ars->ncurves+1) * sizeof(fastf_t *),
					       "new curves");

		for (i=0; i<ars->ncurves+1; i++) {
		    size_t j, k;

		    curves[i] = (fastf_t *)bu_malloc(ars->pts_per_curve * 3 * sizeof(fastf_t),
						     "new curves[i]");

		    if (i <= (size_t)es_ars_crv)
			k = i;
		    else
			k = i - 1;

		    for (j=0; j<ars->pts_per_curve*3; j++)
			curves[i][j] = ars->curves[k][j];
		}

		for (i=0; i<ars->ncurves; i++)
		    bu_free((void *)ars->curves[i], "ars->curves[i]");
		bu_free((void *)ars->curves, "ars->curves");

		ars->curves = curves;
		ars->ncurves++;
	    }
	    break;
	case ECMD_ARS_DUP_COL:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t **curves;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    bu_log("No ARS point selected\n");
		    break;
		}

		curves = (fastf_t **)bu_malloc(ars->ncurves * sizeof(fastf_t *),
					       "new curves");

		for (i=0; i<ars->ncurves; i++) {
		    size_t j, k;

		    curves[i] = (fastf_t *)bu_malloc((ars->pts_per_curve + 1) * 3 * sizeof(fastf_t),
						     "new curves[i]");

		    for (j=0; j<ars->pts_per_curve+1; j++) {
			if (j <= (size_t)es_ars_col)
			    k = j;
			else
			    k = j - 1;

			curves[i][j*3] = ars->curves[i][k*3];
			curves[i][j*3+1] = ars->curves[i][k*3+1];
			curves[i][j*3+2] = ars->curves[i][k*3+2];
		    }
		}

		for (i=0; i<ars->ncurves; i++)
		    bu_free((void *)ars->curves[i], "ars->curves[i]");
		bu_free((void *)ars->curves, "ars->curves");

		ars->curves = curves;
		ars->pts_per_curve++;
	    }
	    break;
	case ECMD_ARS_DEL_CRV:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t **curves;
		int k;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    bu_log("No ARS point selected\n");
		    break;
		}

		if (es_ars_crv == 0 || (size_t)es_ars_crv == ars->ncurves-1) {
		    bu_log("Cannot delete first or last curve\n");
		    break;
		}

		curves = (fastf_t **)bu_malloc((ars->ncurves - 1) * sizeof(fastf_t *),
					       "new curves");

		k = 0;
		for (i=0; i<ars->ncurves; i++) {
		    size_t j;

		    if (i == (size_t)es_ars_crv)
			continue;

		    curves[k] = (fastf_t *)bu_malloc(ars->pts_per_curve * 3 * sizeof(fastf_t),
						     "new curves[k]");

		    for (j=0; j<ars->pts_per_curve*3; j++)
			curves[k][j] = ars->curves[i][j];

		    k++;
		}

		for (i=0; i<ars->ncurves; i++)
		    bu_free((void *)ars->curves[i], "ars->curves[i]");
		bu_free((void *)ars->curves, "ars->curves");

		ars->curves = curves;
		ars->ncurves--;

		if ((size_t)es_ars_crv >= ars->ncurves)
		    es_ars_crv = ars->ncurves - 1;
	    }
	    break;
	case ECMD_ARS_DEL_COL:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		fastf_t **curves;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    bu_log("No ARS point selected\n");
		    break;
		}

		if (es_ars_col == 0 || (size_t)es_ars_col == ars->ncurves - 1) {
		    bu_log("Cannot delete first or last column\n");
		    break;
		}

		if (ars->pts_per_curve < 3) {
		    bu_log("Cannot create an ARS with less than two points per curve\n");
		    break;
		}

		curves = (fastf_t **)bu_malloc(ars->ncurves * sizeof(fastf_t *),
					       "new curves");

		for (i=0; i<ars->ncurves; i++) {
		    size_t j, k;


		    curves[i] = (fastf_t *)bu_malloc((ars->pts_per_curve - 1) * 3 * sizeof(fastf_t),
						     "new curves[i]");

		    k = 0;
		    for (j=0; j<ars->pts_per_curve; j++) {
			if (j == (size_t)es_ars_col)
			    continue;

			curves[i][k*3] = ars->curves[i][j*3];
			curves[i][k*3+1] = ars->curves[i][j*3+1];
			curves[i][k*3+2] = ars->curves[i][j*3+2];
			k++;
		    }
		}

		for (i=0; i<ars->ncurves; i++)
		    bu_free((void *)ars->curves[i], "ars->curves[i]");
		bu_free((void *)ars->curves, "ars->curves");

		ars->curves = curves;
		ars->pts_per_curve--;

		if ((size_t)es_ars_col >= ars->pts_per_curve)
		    es_ars_col = ars->pts_per_curve - 1;
	    }
	    break;
	case ECMD_ARS_MOVE_COL:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt = VINIT_ZERO;
		vect_t diff;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    bu_log("No ARS point selected\n");
		    break;
		}

		if (s->edit_state.e_mvalid) {
		    vect_t view_dir;
		    plane_t view_pl;
		    fastf_t dist;

		    /* construct a plane perpendicular to view direction
		     * that passes through ARS point being moved
		     */
		    VSET(view_dir, 0.0, 0.0, 1.0);
		    MAT4X3VEC(view_pl, view_state->vs_gvp->gv_view2model, view_dir);
		    VUNITIZE(view_pl);
		    view_pl[W] = VDOT(view_pl, &ars->curves[es_ars_crv][es_ars_col*3]);

		    /* project s->edit_state.e_mparam onto the plane */
		    dist = DIST_PNT_PLANE(s->edit_state.e_mparam, view_pl);
		    VJOIN1(new_pt, s->edit_state.e_mparam, -dist, view_pl);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}

		VSUB2(diff, new_pt, &ars->curves[es_ars_crv][es_ars_col*3]);

		for (i=0; i<ars->ncurves; i++)
		    VADD2(&ars->curves[i][es_ars_col*3],
			  &ars->curves[i][es_ars_col*3], diff);

	    }
	    break;
	case ECMD_ARS_MOVE_CRV:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt = VINIT_ZERO;
		vect_t diff;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    bu_log("No ARS point selected\n");
		    break;
		}

		if (s->edit_state.e_mvalid) {
		    vect_t view_dir;
		    plane_t view_pl;
		    fastf_t dist;

		    /* construct a plane perpendicular to view direction
		     * that passes through ARS point being moved
		     */
		    VSET(view_dir, 0.0, 0.0, 1.0);
		    MAT4X3VEC(view_pl, view_state->vs_gvp->gv_view2model, view_dir);
		    VUNITIZE(view_pl);
		    view_pl[W] = VDOT(view_pl, &ars->curves[es_ars_crv][es_ars_col*3]);

		    /* project s->edit_state.e_mparam onto the plane */
		    dist = DIST_PNT_PLANE(s->edit_state.e_mparam, view_pl);
		    VJOIN1(new_pt, s->edit_state.e_mparam, -dist, view_pl);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}

		VSUB2(diff, new_pt, &ars->curves[es_ars_crv][es_ars_col*3]);

		for (i=0; i<ars->pts_per_curve; i++)
		    VADD2(&ars->curves[es_ars_crv][i*3],
			  &ars->curves[es_ars_crv][i*3], diff);

	    }
	    break;
	case ECMD_ARS_MOVE_PT:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt = VINIT_ZERO;

		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    bu_log("No ARS point selected\n");
		    break;
		}

		if (s->edit_state.e_mvalid) {
		    vect_t view_dir;
		    plane_t view_pl;
		    fastf_t dist;

		    /* construct a plane perpendicular to view direction
		     * that passes through ARS point being moved
		     */
		    VSET(view_dir, 0.0, 0.0, 1.0);
		    MAT4X3VEC(view_pl, view_state->vs_gvp->gv_view2model, view_dir);
		    VUNITIZE(view_pl);
		    view_pl[W] = VDOT(view_pl, &ars->curves[es_ars_crv][es_ars_col*3]);

		    /* project s->edit_state.e_mparam onto the plane */
		    dist = DIST_PNT_PLANE(s->edit_state.e_mparam, view_pl);
		    VJOIN1(new_pt, s->edit_state.e_mparam, -dist, view_pl);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}

		VMOVE(&ars->curves[es_ars_crv][es_ars_col*3], new_pt);
	    }
	    break;
	case ECMD_BOT_MOVEV:
	    {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		int vert;
		point_t new_pt = VINIT_ZERO;

		RT_BOT_CK_MAGIC(bot);

		if (bot_verts[0] < 0) {
		    bu_log("No BOT point selected\n");
		    break;
		}

		if (bot_verts[1] >= 0 && bot_verts[2] >= 0) {
		    bu_log("A triangle is selected, not a BOT point!\n");
		    break;
		}

		if (bot_verts[1] >= 0) {
		    bu_log("An edge is selected, not a BOT point!\n");
		    break;
		}

		vert = bot_verts[0];
		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}

		VMOVE(&bot->vertices[vert*3], new_pt);
	    }
	    break;
	case ECMD_BOT_MOVEE:
	    {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		int v1, v2;
		vect_t diff;
		point_t new_pt = VINIT_ZERO;

		RT_BOT_CK_MAGIC(bot);

		if (bot_verts[0] < 0 || bot_verts[1] < 0) {
		    Tcl_AppendResult(s->interp, "No BOT edge selected\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}

		if (bot_verts[2] >= 0) {
		    bu_log("A triangle is selected, not a BOT edge!\n");
		    break;
		}
		v1 = bot_verts[0];
		v2 = bot_verts[1];
		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}


		VSUB2(diff, new_pt, &bot->vertices[v1*3]);
		VMOVE(&bot->vertices[v1*3], new_pt);
		VADD2(&bot->vertices[v2*3], &bot->vertices[v2*3], diff);
	    }
	    break;
	case ECMD_BOT_MOVET:
	    {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		int v1, v2, v3;
		point_t new_pt = VINIT_ZERO;
		vect_t diff;

		RT_BOT_CK_MAGIC(bot);

		if (bot_verts[0] < 0 || bot_verts[1] < 0 || bot_verts[2] < 0) {
		    Tcl_AppendResult(s->interp, "No BOT triangle selected\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		}
		v1 = bot_verts[0];
		v2 = bot_verts[1];
		v3 = bot_verts[2];

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    if (mged_variables->mv_context) {
			/* apply s->edit_state.e_invmat to convert to real model space */
			MAT4X3PNT(new_pt, s->edit_state.e_invmat, s->edit_state.e_para);
		    } else {
			VMOVE(new_pt, s->edit_state.e_para);
		    }
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for point movement\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}

		VSUB2(diff, new_pt, &bot->vertices[v1*3]);
		VMOVE(&bot->vertices[v1*3], new_pt);
		VADD2(&bot->vertices[v2*3], &bot->vertices[v2*3], diff);
		VADD2(&bot->vertices[v3*3], &bot->vertices[v3*3], diff);
	    }
	    break;
	case ECMD_BOT_PICKV:
	case ECMD_BOT_PICKE:
	case ECMD_BOT_PICKT:
	    break;

	case ECMD_METABALL_PT_PICK:
	    {
		struct rt_metaball_internal *metaball=
		    (struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
		point_t new_pt;
		struct wdb_metaball_pnt *ps;
		struct wdb_metaball_pnt *nearest=(struct wdb_metaball_pnt *)NULL;
		struct bn_tol tmp_tol;
		fastf_t min_dist = MAX_FASTF;
		vect_t dir;

		RT_METABALL_CK_MAGIC(metaball);

		if (s->edit_state.e_mvalid) {
		    VMOVE(new_pt, s->edit_state.e_mparam);
		} else if (s->edit_state.e_inpara == 3) {
		    VMOVE(new_pt, s->edit_state.e_para);
		} else if (s->edit_state.e_inpara && s->edit_state.e_inpara != 3) {
		    Tcl_AppendResult(s->interp, "x y z coordinates required for control point selection\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    break;
		} else if (!s->edit_state.e_mvalid && !s->edit_state.e_inpara) {
		    break;
		}

		tmp_tol.magic = BN_TOL_MAGIC;
		tmp_tol.dist = 0.0;
		tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
		tmp_tol.perp = 0.0;
		tmp_tol.para = 1.0 - tmp_tol.perp;

		/* get a direction vector in model space corresponding to z-direction in view */
		VSET(work, 0.0, 0.0, 1.0);
		MAT4X3VEC(dir, view_state->vs_gvp->gv_view2model, work);

		for (BU_LIST_FOR(ps, wdb_metaball_pnt, &metaball->metaball_ctrl_head)) {
		    fastf_t dist;

		    dist = bg_dist_line3_pnt3(new_pt, dir, ps->coord);
		    if (dist < min_dist) {
			min_dist = dist;
			nearest = ps;
		    }
		}

		es_metaball_pnt = nearest;

		if (!es_metaball_pnt) {
		    Tcl_AppendResult(s->interp, "No METABALL control point selected\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		} else {
		    rt_metaball_pnt_print(es_metaball_pnt, s->dbip->dbi_base2local);
		}
	    }
	    break;
	case ECMD_METABALL_PT_MOV:
	    if (!es_metaball_pnt) {
		bu_log("Must select a point to move"); break; }
	    if (s->edit_state.e_inpara != 3) {
		bu_log("Must provide dx dy dz"); break; }
	    VADD2(es_metaball_pnt->coord, es_metaball_pnt->coord, s->edit_state.e_para);
	    break;
	case ECMD_METABALL_PT_DEL:
	    {
		struct wdb_metaball_pnt *tmp = es_metaball_pnt, *p;

		if (es_metaball_pnt == NULL) {
		    bu_log("No point selected");
		    break;
		}
		p = BU_LIST_PREV(wdb_metaball_pnt, &es_metaball_pnt->l);
		if (p->l.magic == BU_LIST_HEAD_MAGIC) {
		    es_metaball_pnt = BU_LIST_NEXT(wdb_metaball_pnt, &es_metaball_pnt->l);
		    /* 0 point metaball... allow it for now. */
		    if (es_metaball_pnt->l.magic == BU_LIST_HEAD_MAGIC)
			es_metaball_pnt = NULL;
		} else
		    es_metaball_pnt = p;
		BU_LIST_DQ(&tmp->l);
		free(tmp);
		if (!es_metaball_pnt)
		    bu_log("WARNING: Last point of this metaball has been deleted.");
	    }
	    break;
	case ECMD_METABALL_PT_ADD:
	    {
		struct rt_metaball_internal *metaball= (struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
		struct wdb_metaball_pnt *n = (struct wdb_metaball_pnt *)malloc(sizeof(struct wdb_metaball_pnt));

		if (s->edit_state.e_inpara != 3) {
		    bu_log("Must provide x y z");
		    bu_free(n, "wdb_metaball_pnt n");
		    break;
		}

		es_metaball_pnt = BU_LIST_FIRST(wdb_metaball_pnt, &metaball->metaball_ctrl_head);
		VMOVE(n->coord, s->edit_state.e_para);
		n->l.magic = WDB_METABALLPT_MAGIC;
		n->fldstr = 1.0;
		BU_LIST_APPEND(&es_metaball_pnt->l, &n->l);
		es_metaball_pnt = n;
	    }
	    break;

	default:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		bu_vls_printf(&tmp_vls, "sedit(s):  unknown edflag = %d.\n", es_edflag);
		Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		mged_print_result(s, TCL_ERROR);
		bu_vls_free(&tmp_vls);
	    }
    }

    /* must re-calculate the face plane equations for arbs */
    if (s->edit_state.es_int.idb_type == ID_ARB8) {
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if (rt_arb_calc_planes(&error_msg, arb, s->edit_state.e_type, es_peqn, &s->tol.tol) < 0)
	    Tcl_AppendResult(s->interp, bu_vls_addr(&error_msg), (char *)0);
	bu_vls_free(&error_msg);
    }

    /* If the keypoint changed location, find about it here */
    if (!s->edit_state.e_keyfixed)
	get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    set_e_axes_pos(s, 0);
    replot_editing_solid(s);

    if (s->update_views) {
	dm_set_dirty(DMP, 1);
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "active_edit_callback");
	(void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    s->edit_state.e_inpara = 0;
    s->edit_state.e_mvalid = 0;
}


void
update_edit_absolute_tran(struct mged_state *s, vect_t view_pos)
{
    vect_t model_pos;
    vect_t ea_view_pos;
    vect_t diff;
    fastf_t inv_Viewscale = 1/view_state->vs_gvp->gv_scale;

    MAT4X3PNT(model_pos, view_state->vs_gvp->gv_view2model, view_pos);
    VSUB2(diff, model_pos, s->edit_state.e_axes_pos);
    VSCALE(s->edit_state.k.tra_m_abs, diff, inv_Viewscale);
    VMOVE(s->edit_state.k.tra_m_abs_last, s->edit_state.k.tra_m_abs);

    MAT4X3PNT(ea_view_pos, view_state->vs_gvp->gv_model2view, s->edit_state.e_axes_pos);
    VSUB2(s->edit_state.k.tra_v_abs, view_pos, ea_view_pos);
    VMOVE(s->edit_state.k.tra_v_abs_last, s->edit_state.k.tra_v_abs);
}


/*
 * Mouse (pen) press in graphics area while doing Solid Edit.
 * mousevec [X] and [Y] are in the range -1.0...+1.0, corresponding
 * to viewspace.
 *
 * In order to allow the "p" command to do the same things that
 * a mouse event can, the preferred strategy is to store the value
 * corresponding to what the "p" command would give in s->edit_state.e_mparam,
 * set s->edit_state.e_mvalid = 1, set sedraw = 1, and return, allowing sedit(s)
 * to actually do the work.
 */
void
sedit_mouse(struct mged_state *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    vect_t tr_temp = VINIT_ZERO;	/* temp translation vector */
    vect_t temp = VINIT_ZERO;
    vect_t raw_kp = VINIT_ZERO;        	/* s->edit_state.e_keypoint with s->edit_state.e_invmat applied */
    vect_t raw_mp = VINIT_ZERO;        	/* raw model position */
    mat_t mat;

    if (es_edflag <= 0)
	return;

    switch (es_edflag) {
	case SSCALE:
	case PSCALE:
	case ECMD_DSP_SCALE_X:
	case ECMD_DSP_SCALE_Y:
	case ECMD_DSP_SCALE_ALT:
	case ECMD_VOL_CSIZE:
	case ECMD_VOL_THRESH_LO:
	case ECMD_VOL_THRESH_HI:
	case ECMD_EBM_HEIGHT:
	case ECMD_EXTR_SCALE_H:
	case ECMD_CLINE_SCALE_H:
	case ECMD_CLINE_SCALE_T:
	case ECMD_CLINE_SCALE_R:
	    /* use mouse to get a scale factor */
	    s->edit_state.es_scale = 1.0 + 0.25 * ((fastf_t)
				     (mousevec[Y] > 0 ? mousevec[Y] : -mousevec[Y]));
	    if (mousevec[Y] <= 0)
		s->edit_state.es_scale = 1.0 / s->edit_state.es_scale;

	    /* accumulate scale factor */
	    s->edit_state.acc_sc_sol *= s->edit_state.es_scale;

	    s->edit_state.k.sca_abs = s->edit_state.acc_sc_sol - 1.0;
	    if (s->edit_state.k.sca_abs > 0)
		s->edit_state.k.sca_abs /= 3.0;

	    sedit(s);

	    return;
	case STRANS:
	    /*
	     * Use mouse to change solid's location.
	     * Project solid's keypoint into view space,
	     * replace X, Y (but NOT Z) components, and
	     * project result back to model space.
	     * Then move keypoint there.
	     */
	    {
		point_t pt;
		vect_t delta;

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];
		MAT4X3PNT(pt, view_state->vs_gvp->gv_view2model, pos_view);

		/* Need vector from current vertex/keypoint
		 * to desired new location.
		 */
		MAT4X3PNT(raw_mp, s->edit_state.e_invmat, pt);
		MAT4X3PNT(raw_kp, s->edit_state.e_invmat, s->edit_state.curr_e_axes_pos);
		VSUB2(delta, raw_kp, raw_mp);
		MAT_IDN(mat);
		MAT_DELTAS_VEC_NEG(mat, delta);
		transform_editing_solid(s, &s->edit_state.es_int, mat, &s->edit_state.es_int, 1);
	    }

	    break;
	case ECMD_VTRANS:
	    /*
	     * Use mouse to change a vertex location.
	     * Project vertex (in solid keypoint) into view space,
	     * replace X, Y (but NOT Z) components, and
	     * project result back to model space.
	     * Leave desired location in s->edit_state.e_mparam.
	     */

	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(s->edit_state.e_mparam, s->edit_state.e_invmat, temp);
	    s->edit_state.e_mvalid = 1;	/* s->edit_state.e_mparam is valid */
	    /* Leave the rest to code in sedit(s) */

	    break;
	case ECMD_TGC_MV_H:
	case ECMD_TGC_MV_HH:
	    /* Use mouse to change location of point V+H */
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)s->edit_state.es_int.idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];
		/* Do NOT change pos_view[Z] ! */
		MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
		MAT4X3PNT(tr_temp, s->edit_state.e_invmat, temp);
		VSUB2(tgc->h, tr_temp, tgc->v);
	    }

	    break;
	case ECMD_EXTR_MOV_H:
	    /* Use mouse to change location of point V+H */
	    {
		struct rt_extrude_internal *extr =
		    (struct rt_extrude_internal *)s->edit_state.es_int.idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];
		/* Do NOT change pos_view[Z] ! */
		MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
		MAT4X3PNT(tr_temp, s->edit_state.e_invmat, temp);
		VSUB2(extr->h, tr_temp, extr->V);
	    }

	    break;
	case ECMD_CLINE_MOVE_H:
	    {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)s->edit_state.es_int.idb_ptr;

		RT_CLINE_CK_MAGIC(cli);

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];
		/* Do NOT change pos_view[Z] ! */
		MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
		MAT4X3PNT(tr_temp, s->edit_state.e_invmat, temp);
		VSUB2(cli->h, tr_temp, cli->v);
	    }

	    break;
	case PTARB:
	    /* move an arb point to indicated point */
	    /* point is located at es_values[es_menu*3] */
	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(pos_model, s->edit_state.e_invmat, temp);
	    editarb(s, pos_model);

	    break;
	case EARB:
	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(pos_model, s->edit_state.e_invmat, temp);
	    editarb(s, pos_model);

	    break;
	case ECMD_ARB_MOVE_FACE:
	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(pos_model, s->edit_state.e_invmat, temp);
	    /* change D of planar equation */
	    es_peqn[es_menu][W]=VDOT(&es_peqn[es_menu][0], pos_model);
	    /* calculate new vertices, put in record as vectors */
	    {
		struct rt_arb_internal *arb=
		    (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;

		RT_ARB_CK_MAGIC(arb);

		(void)rt_arb_calc_points(arb, s->edit_state.e_type, (const plane_t *)es_peqn, &s->tol.tol);
	    }

	    break;
	case ECMD_BOT_PICKV:
	    {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		int tmp_vert;
		char tmp_msg[256];
		point_t selected_pt;

		RT_BOT_CK_MAGIC(bot);

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];

		tmp_vert = rt_bot_find_v_nearest_pt2(bot, pos_view, view_state->vs_gvp->gv_model2view);
		if (tmp_vert < 0) {
		    Tcl_AppendResult(s->interp, "ECMD_BOT_PICKV: unable to find a vertex!\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}

		bot_verts[0] = tmp_vert;
		bot_verts[1] = -1;
		bot_verts[2] = -1;
		VSCALE(selected_pt, &bot->vertices[tmp_vert*3], s->dbip->dbi_base2local);
		sprintf(tmp_msg, "picked point at (%g %g %g), vertex #%d\n", V3ARGS(selected_pt), tmp_vert);
		Tcl_AppendResult(s->interp, tmp_msg, (char *)NULL);
		mged_print_result(s, TCL_OK);
	    }
	    break;
	case ECMD_BOT_PICKE:
	    {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		int vert1, vert2;
		char tmp_msg[256];
		point_t from_pt, to_pt;

		RT_BOT_CK_MAGIC(bot);

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];

		if (rt_bot_find_e_nearest_pt2(&vert1, &vert2, bot, pos_view, view_state->vs_gvp->gv_model2view)) {
		    Tcl_AppendResult(s->interp, "ECMD_BOT_PICKE: unable to find an edge!\n", (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}

		bot_verts[0] = vert1;
		bot_verts[1] = vert2;
		bot_verts[2] = -1;
		VSCALE(from_pt, &bot->vertices[vert1*3], s->dbip->dbi_base2local);
		VSCALE(to_pt, &bot->vertices[vert2*3], s->dbip->dbi_base2local);
		sprintf(tmp_msg, "picked edge from (%g %g %g) to (%g %g %g)\n", V3ARGS(from_pt), V3ARGS(to_pt));
		Tcl_AppendResult(s->interp, tmp_msg, (char *)NULL);
		mged_print_result(s, TCL_OK);
	    }
	    break;
	case ECMD_BOT_PICKT:
	    {
		struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;
		point_t start_pt, tmp;
		vect_t dir;
		size_t i;
		int hits, ret_tcl;
		int v1, v2, v3;
		point_t pt1, pt2, pt3;
		struct bu_vls vls = BU_VLS_INIT_ZERO;

		RT_BOT_CK_MAGIC(bot);

		VSET(tmp, mousevec[X], mousevec[Y], 0.0);
		MAT4X3PNT(start_pt, view_state->vs_gvp->gv_view2model, tmp);
		VSET(tmp, 0, 0, 1);
		MAT4X3VEC(dir, view_state->vs_gvp->gv_view2model, tmp);

		bu_vls_strcat(&vls, " {");
		hits = 0;
		for (i=0; i<bot->num_faces; i++) {
		    v1 = bot->faces[i*3];
		    v2 = bot->faces[i*3+1];
		    v3 = bot->faces[i*3+2];
		    VMOVE(pt1, &bot->vertices[v1*3]);
		    VMOVE(pt2, &bot->vertices[v2*3]);
		    VMOVE(pt3, &bot->vertices[v3*3]);

		    if (bg_does_ray_isect_tri(start_pt, dir, pt1, pt2, pt3, tmp)) {
			hits++;
			bu_vls_printf(&vls, " { %d %d %d }", v1, v2, v3);
		    }
		}
		bu_vls_strcat(&vls, " } ");

		if (hits == 0) {
		    bot_verts[0] = -1;
		    bot_verts[1] = -1;
		    bot_verts[2] = -1;
		    bu_vls_free(&vls);
		}
		if (hits == 1) {
		    sscanf(bu_vls_addr(&vls), " { { %d %d %d", &bot_verts[0], &bot_verts[1], &bot_verts[2]);
		    bu_vls_free(&vls);
		} else {
		    Tcl_LinkVar(s->interp, "bot_v1", (char *)&bot_verts[0], TCL_LINK_INT);
		    Tcl_LinkVar(s->interp, "bot_v2", (char *)&bot_verts[1], TCL_LINK_INT);
		    Tcl_LinkVar(s->interp, "bot_v3", (char *)&bot_verts[2], TCL_LINK_INT);

		    ret_tcl = Tcl_VarEval(s->interp, "bot_face_select ", bu_vls_addr(&vls), (char *)NULL);
		    bu_vls_free(&vls);
		    if (ret_tcl != TCL_OK) {
			bu_log("bot_face_select failed: %s\n", Tcl_GetStringResult(s->interp));
			bot_verts[0] = -1;
			bot_verts[1] = -1;
			bot_verts[2] = -1;
			break;
		    }
		}
	    }
	    break;
	case ECMD_NMG_EPICK:
	    /* XXX Should just leave desired location in s->edit_state.e_mparam for sedit(s) */
	    {
		struct model *m =
		    (struct model *)s->edit_state.es_int.idb_ptr;
		struct edge *e;
		struct bn_tol tmp_tol;
		NMG_CK_MODEL(m);

		/* Picking an edge should not depend on tolerances! */
		tmp_tol.magic = BN_TOL_MAGIC;
		tmp_tol.dist = 0.0;
		tmp_tol.dist_sq = tmp_tol.dist * tmp_tol.dist;
		tmp_tol.perp = 0.0;
		tmp_tol.para = 1 - tmp_tol.perp;

		MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
		pos_view[X] = mousevec[X];
		pos_view[Y] = mousevec[Y];
		if ((e = nmg_find_e_nearest_pt2(&m->magic, pos_view,
						view_state->vs_gvp->gv_model2view, s->vlfree, &tmp_tol)) == (struct edge *)NULL) {
		    Tcl_AppendResult(s->interp, "ECMD_NMG_EPICK: unable to find an edge\n",
				     (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    return;
		}
		es_eu = e->eu_p;
		NMG_CK_EDGEUSE(es_eu);

		{
		    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

		    bu_vls_printf(&tmp_vls,
				  "edgeuse selected = %p (%g %g %g) <-> (%g %g %g)\n",
				  (void *)es_eu, V3ARGS(es_eu->vu_p->v_p->vg_p->coord),
				  V3ARGS(es_eu->eumate_p->vu_p->v_p->vg_p->coord));
		    Tcl_AppendResult(s->interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    mged_print_result(s, TCL_ERROR);
		    bu_vls_free(&tmp_vls);
		}
	    }

	    break;
	case ECMD_NMG_LEXTRU:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_ESPLIT:
	case ECMD_PIPE_PICK:
	case ECMD_PIPE_SPLIT:
	case ECMD_PIPE_PT_MOVE:
	case ECMD_PIPE_PT_ADD:
	case ECMD_PIPE_PT_INS:
	case ECMD_ARS_PICK:
	case ECMD_ARS_MOVE_PT:
	case ECMD_ARS_MOVE_CRV:
	case ECMD_ARS_MOVE_COL:
	case ECMD_BOT_MOVEV:
	case ECMD_BOT_MOVEE:
	case ECMD_BOT_MOVET:
	case ECMD_METABALL_PT_PICK:
	case ECMD_METABALL_PT_MOV:
	case ECMD_METABALL_PT_ADD:

	    MAT4X3PNT(pos_view, view_state->vs_gvp->gv_model2view, s->edit_state.curr_e_axes_pos);
	    pos_view[X] = mousevec[X];
	    pos_view[Y] = mousevec[Y];
	    MAT4X3PNT(temp, view_state->vs_gvp->gv_view2model, pos_view);
	    MAT4X3PNT(s->edit_state.e_mparam, s->edit_state.e_invmat, temp);
	    s->edit_state.e_mvalid = 1;

	    break;
	default:
	    Tcl_AppendResult(s->interp, "mouse press undefined in this solid edit mode\n", (char *)NULL);
	    mged_print_result(s, TCL_ERROR);
	    return;
    }

    update_edit_absolute_tran(s, pos_view);
    sedit(s);
}


void
sedit_abs_scale(struct mged_state *s)
{
    fastf_t old_acc_sc_sol;

    if (es_edflag != SSCALE && es_edflag != PSCALE)
	return;

    old_acc_sc_sol = s->edit_state.acc_sc_sol;

    if (-SMALL_FASTF < s->edit_state.k.sca_abs && s->edit_state.k.sca_abs < SMALL_FASTF)
	s->edit_state.acc_sc_sol = 1.0;
    else if (s->edit_state.k.sca_abs > 0.0)
	s->edit_state.acc_sc_sol = 1.0 + s->edit_state.k.sca_abs * 3.0;
    else {
	if ((s->edit_state.k.sca_abs - MGED_SMALL_SCALE) < -1.0)
	    s->edit_state.k.sca_abs = -1.0 + MGED_SMALL_SCALE;

	s->edit_state.acc_sc_sol = 1.0 + s->edit_state.k.sca_abs;
    }

    s->edit_state.es_scale = s->edit_state.acc_sc_sol / old_acc_sc_sol;
    sedit(s);
}


/*
 * Object Edit
 */
void
objedit_mouse(struct mged_state *s, const vect_t mousevec)
{
    fastf_t scale = 1.0;
    vect_t pos_view;	 	/* Unrotated view space pos */
    vect_t pos_model;	/* Rotated screen space pos */
    vect_t tr_temp;		/* temp translation vector */
    vect_t temp;

    MAT_IDN(s->edit_state.incr_change);
    if (movedir & SARROW) {
	/* scaling option is in effect */
	scale = 1.0 + (fastf_t)(mousevec[Y]>0 ?
				mousevec[Y] : -mousevec[Y]);
	if (mousevec[Y] <= 0)
	    scale = 1.0 / scale;

	/* switch depending on scaling option selected */
	switch (edobj) {

	    case BE_O_SCALE:
		/* global scaling */
		s->edit_state.incr_change[15] = 1.0 / scale;

		s->edit_state.acc_sc_obj /= s->edit_state.incr_change[15];
		s->edit_state.k.sca_abs = s->edit_state.acc_sc_obj - 1.0;
		if (s->edit_state.k.sca_abs > 0.0)
		    s->edit_state.k.sca_abs /= 3.0;
		break;

	    case BE_O_XSCALE:
		/* local scaling ... X-axis */
		s->edit_state.incr_change[0] = scale;
		/* accumulate the scale factor */
		s->edit_state.acc_sc[0] *= scale;
		s->edit_state.k.sca_abs = s->edit_state.acc_sc[0] - 1.0;
		if (s->edit_state.k.sca_abs > 0.0)
		    s->edit_state.k.sca_abs /= 3.0;
		break;

	    case BE_O_YSCALE:
		/* local scaling ... Y-axis */
		s->edit_state.incr_change[5] = scale;
		/* accumulate the scale factor */
		s->edit_state.acc_sc[1] *= scale;
		s->edit_state.k.sca_abs = s->edit_state.acc_sc[1] - 1.0;
		if (s->edit_state.k.sca_abs > 0.0)
		    s->edit_state.k.sca_abs /= 3.0;
		break;

	    case BE_O_ZSCALE:
		/* local scaling ... Z-axis */
		s->edit_state.incr_change[10] = scale;
		/* accumulate the scale factor */
		s->edit_state.acc_sc[2] *= scale;
		s->edit_state.k.sca_abs = s->edit_state.acc_sc[2] - 1.0;
		if (s->edit_state.k.sca_abs > 0.0)
		    s->edit_state.k.sca_abs /= 3.0;
		break;
	}

	/* Have scaling take place with respect to keypoint,
	 * NOT the view center.
	 */
	VMOVE(temp, s->edit_state.e_keypoint);
	MAT4X3PNT(pos_model, s->edit_state.model_changes, temp);
	wrt_point(s->edit_state.model_changes, s->edit_state.incr_change, s->edit_state.model_changes, pos_model);

	MAT_IDN(s->edit_state.incr_change);
	new_edit_mats(s);
    } else if (movedir & (RARROW|UARROW)) {
	mat_t oldchanges;	/* temporary matrix */

	/* Vector from object keypoint to cursor */
	VMOVE(temp, s->edit_state.e_keypoint);
	MAT4X3PNT(pos_view, view_state->vs_model2objview, temp);

	if (movedir & RARROW)
	    pos_view[X] = mousevec[X];
	if (movedir & UARROW)
	    pos_view[Y] = mousevec[Y];

	MAT4X3PNT(pos_model, view_state->vs_gvp->gv_view2model, pos_view);/* NOT objview */
	MAT4X3PNT(tr_temp, s->edit_state.model_changes, temp);
	VSUB2(tr_temp, pos_model, tr_temp);
	MAT_DELTAS_VEC(s->edit_state.incr_change, tr_temp);
	MAT_COPY(oldchanges, s->edit_state.model_changes);
	bn_mat_mul(s->edit_state.model_changes, s->edit_state.incr_change, oldchanges);

	MAT_IDN(s->edit_state.incr_change);
	new_edit_mats(s);

	update_edit_absolute_tran(s, pos_view);
    } else {
	Tcl_AppendResult(s->interp, "No object edit mode selected;  mouse press ignored\n", (char *)NULL);
	return;
    }
}


void
oedit_abs_scale(struct mged_state *s)
{
    fastf_t scale;
    vect_t temp;
    vect_t pos_model;
    mat_t incr_mat;

    MAT_IDN(incr_mat);

    if (-SMALL_FASTF < s->edit_state.k.sca_abs && s->edit_state.k.sca_abs < SMALL_FASTF)
	scale = 1;
    else if (s->edit_state.k.sca_abs > 0.0)
	scale = 1.0 + s->edit_state.k.sca_abs * 3.0;
    else {
	if ((s->edit_state.k.sca_abs - MGED_SMALL_SCALE) < -1.0)
	    s->edit_state.k.sca_abs = -1.0 + MGED_SMALL_SCALE;

	scale = 1.0 + s->edit_state.k.sca_abs;
    }

    /* switch depending on scaling option selected */
    switch (edobj) {

	case BE_O_SCALE:
	    /* global scaling */
	    incr_mat[15] = s->edit_state.acc_sc_obj / scale;
	    s->edit_state.acc_sc_obj = scale;
	    break;

	case BE_O_XSCALE:
	    /* local scaling ... X-axis */
	    incr_mat[0] = scale / s->edit_state.acc_sc[0];
	    /* accumulate the scale factor */
	    s->edit_state.acc_sc[0] = scale;
	    break;

	case BE_O_YSCALE:
	    /* local scaling ... Y-axis */
	    incr_mat[5] = scale / s->edit_state.acc_sc[1];
	    /* accumulate the scale factor */
	    s->edit_state.acc_sc[1] = scale;
	    break;

	case BE_O_ZSCALE:
	    /* local scaling ... Z-axis */
	    incr_mat[10] = scale / s->edit_state.acc_sc[2];
	    /* accumulate the scale factor */
	    s->edit_state.acc_sc[2] = scale;
	    break;
    }

    /* Have scaling take place with respect to keypoint,
     * NOT the view center.
     */
    VMOVE(temp, s->edit_state.e_keypoint);
    MAT4X3PNT(pos_model, s->edit_state.model_changes, temp);
    wrt_point(s->edit_state.model_changes, incr_mat, s->edit_state.model_changes, pos_model);

    new_edit_mats(s);
}


void
vls_solid(struct mged_state *s, struct bu_vls *vp, struct rt_db_internal *ip, const mat_t mat)
{
    struct rt_db_internal intern;
    int id;

    RT_DB_INTERNAL_INIT(&intern);

    if (s->dbip == DBI_NULL)
	return;

    BU_CK_VLS(vp);
    RT_CK_DB_INTERNAL(ip);

    id = ip->idb_type;
    transform_editing_solid(s, &intern, mat, (struct rt_db_internal *)ip, 0);

    if (id != ID_ARS && id != ID_POLY && id != ID_BOT) {
	if (OBJ[id].ft_describe(vp, &intern, 1 /*verbose*/, s->dbip->dbi_base2local) < 0)
	    Tcl_AppendResult(s->interp, "vls_solid: describe error\n", (char *)NULL);
    } else {
	if (OBJ[id].ft_describe(vp, &intern, 0 /* not verbose */, s->dbip->dbi_base2local) < 0)
	    Tcl_AppendResult(s->interp, "vls_solid: describe error\n", (char *)NULL);
    }

    if (id == ID_PIPE && es_pipe_pnt) {
	struct rt_pipe_internal *pipeip;
	struct wdb_pipe_pnt *ps=(struct wdb_pipe_pnt *)NULL;
	int seg_no = 0;

	pipeip = (struct rt_pipe_internal *)ip->idb_ptr;
	RT_PIPE_CK_MAGIC(pipeip);

	for (BU_LIST_FOR(ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	    seg_no++;
	    if (ps == es_pipe_pnt)
		break;
	}

	if (ps == es_pipe_pnt)
	    rt_vls_pipe_pnt(vp, seg_no, &intern, s->dbip->dbi_base2local);
    }

    rt_db_free_internal(&intern);
}


static void
init_oedit_guts(struct mged_state *s)
{
    int id;
    const char *strp="";

    /* for safety sake */
    es_menu = 0;
    es_edflag = -1;
    MAT_IDN(s->edit_state.e_mat);

    if (s->dbip == DBI_NULL || !illump) {
	return;
    }

    /*
     * Check for a processed region
     */
    if (illump->s_old.s_Eflag) {
	/* Have a processed (E'd) region - NO key solid.
	 * Use the 'center' as the key
	 */
	VMOVE(s->edit_state.e_keypoint, illump->s_center);

	/* The s_center takes the s->edit_state.e_mat into account already */
    }

    /* Not an evaluated region - just a regular path ending in a solid */
    if (!illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(s->interp, "init_oedit(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(s->interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);
	}
	rt_db_free_internal(&s->edit_state.es_int);
	button(s, BE_REJECT);
	return;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->edit_state.es_int);
    id = s->edit_state.es_int.idb_type;

    if (id == ID_ARB8) {
	struct rt_arb_internal *arb;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	s->edit_state.e_type = rt_arb_std_type(&s->edit_state.es_int, &s->tol.tol);
    }

    /* Save aggregate path matrix */
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, s->edit_state.e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->edit_state.e_invmat, s->edit_state.e_mat);

    get_solid_keypoint(s, &s->edit_state.e_keypoint, &strp, &s->edit_state.es_int, s->edit_state.e_mat);
    init_oedit_vars(s);
}


static void
init_oedit_vars(struct mged_state *s)
{
    set_e_axes_pos(s, 1);

    VSETALL(s->edit_state.k.rot_m_abs, 0.0);
    VSETALL(s->edit_state.k.rot_o_abs, 0.0);
    VSETALL(s->edit_state.k.rot_v_abs, 0.0);
    VSETALL(s->edit_state.k.rot_m_abs_last, 0.0);
    VSETALL(s->edit_state.k.rot_o_abs_last, 0.0);
    VSETALL(s->edit_state.k.rot_v_abs_last, 0.0);
    VSETALL(s->edit_state.k.tra_m_abs, 0.0);
    VSETALL(s->edit_state.k.tra_v_abs, 0.0);
    VSETALL(s->edit_state.k.tra_m_abs_last, 0.0);
    VSETALL(s->edit_state.k.tra_v_abs_last, 0.0);
    s->edit_state.k.sca_abs = 0.0;
    s->edit_state.acc_sc_sol = 1.0;
    s->edit_state.acc_sc_obj = 1.0;
    VSETALL(s->edit_state.acc_sc, 1.0);

    VSETALL(s->edit_state.k.rot_m, 0.0);
    VSETALL(s->edit_state.k.rot_o, 0.0);
    VSETALL(s->edit_state.k.rot_v, 0.0);
    VSETALL(s->edit_state.k.tra_m, 0.0);
    VSETALL(s->edit_state.k.tra_v, 0.0);

    MAT_IDN(s->edit_state.model_changes);
    MAT_IDN(s->edit_state.acc_rot_sol);
}


void
init_oedit(struct mged_state *s)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /* do real initialization work */
    init_oedit_guts(s);

    s->edit_state.e_edclass = EDIT_CLASS_NULL;

    /* begin edit callback */
    bu_vls_strcpy(&vls, "begin_edit_callback {}");
    (void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


void oedit_reject(struct mged_state *s);

static void
oedit_apply(struct mged_state *s, int continue_editing)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    /* matrices used to accept editing done from a depth
     * >= 2 from the top of the illuminated path
     */
    mat_t topm;	/* accum matrix from pathpos 0 to i-2 */
    mat_t inv_topm;	/* inverse */
    mat_t deltam;	/* final "changes":  deltam = (inv_topm)(s->edit_state.model_changes)(topm) */
    mat_t tempm;

    if (!illump || !illump->s_u_data)
	return;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    switch (ipathpos) {
	case 0:
	    moveHobj(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
		     s->edit_state.model_changes);
	    break;
	case 1:
	    moveHinstance(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos-1),
			  DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
			  s->edit_state.model_changes);
	    break;
	default:
	    MAT_IDN(topm);
	    MAT_IDN(inv_topm);
	    MAT_IDN(deltam);
	    MAT_IDN(tempm);

	    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, topm, ipathpos-1, &rt_uniresource);

	    bn_mat_inv(inv_topm, topm);

	    bn_mat_mul(tempm, s->edit_state.model_changes, topm);
	    bn_mat_mul(deltam, inv_topm, tempm);

	    moveHinstance(s, DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos-1),
			  DB_FULL_PATH_GET(&bdata->s_fullpath, ipathpos),
			  deltam);
	    break;
    }

    /*
     * Redraw all solids affected by this edit.
     * Regenerate a new control list which does not
     * include the solids about to be replaced,
     * so we can safely fiddle the displaylist.
     */
    s->edit_state.model_changes[15] = 1000000000;	/* => small ratio */

    /* Now, recompute new chunks of displaylist */
    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
    while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (sp->s_iflag == DOWN)
		continue;
	    (void)replot_original_solid(s, sp);

	    if (continue_editing == DOWN) {
		sp->s_iflag = DOWN;
	    }
	}

	gdlp = next_gdlp;
    }
}


void
oedit_accept(struct mged_state *s)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;

    if (s->dbip == DBI_NULL)
	return;

    if (s->dbip->dbi_read_only) {
	oedit_reject(s);

	gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
	while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (sp->s_iflag == DOWN)
		    continue;
		(void)replot_original_solid(s, sp);
		sp->s_iflag = DOWN;
	    }

	    gdlp = next_gdlp;
	}

	bu_log("Sorry, this database is READ-ONLY\n");
	pr_prompt(s);

	return;
    }

    oedit_apply(s, DOWN); /* finished editing */
    oedit_reject(s);
}


void
oedit_reject(struct mged_state *s)
{
    rt_db_free_internal(&s->edit_state.es_int);
}


/*
 * Gets the A, B, C of a planar equation from the command line and puts the
 * result into the array es_peqn[] at the position pointed to by the variable
 * 'es_menu' which is the plane being redefined. This function is only callable
 * when in solid edit and rotating the face of a GENARB8.
 */
int
f_eqn(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    short int i;
    vect_t tempvec;
    struct rt_arb_internal *arb;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 4 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help eqn");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->edit_state.global_editing_state != ST_S_EDIT) {
	Tcl_AppendResult(interp, "Eqn: must be in solid edit\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (s->edit_state.es_int.idb_type != ID_ARB8) {
	Tcl_AppendResult(interp, "Eqn: type must be GENARB8\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (es_edflag != ECMD_ARB_ROTATE_FACE) {
	Tcl_AppendResult(interp, "Eqn: must be rotating a face\n", (char *)NULL);
	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* get the A, B, C from the command line */
    for (i=0; i<3; i++)
	es_peqn[es_menu][i]= atof(argv[i+1]);
    VUNITIZE(&es_peqn[es_menu][0]);

    VMOVE(tempvec, arb->pt[fixv]);
    es_peqn[es_menu][W]=VDOT(es_peqn[es_menu], tempvec);

    if (rt_arb_calc_points(arb, s->edit_state.e_type, (const plane_t *)es_peqn, &s->tol.tol))
	return CMD_BAD;

    /* draw the new version of the solid */
    replot_editing_solid(s);

    /* update display information */
    view_state->vs_flag = 1;

    return TCL_OK;
}


/* Hooks from buttons.c */

/*
 * Copied from sedit_accept - modified to optionally leave
 * solid edit state.
 */
static int
sedit_apply(struct mged_state *s, int accept_flag)
{
    struct directory *dp;

    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;

    /* make sure we are in solid edit mode */
    if (!illump) {
	return TCL_OK;
    }

    if (lu_copy) {
	struct model *m;

	m = nmg_find_model(&lu_copy->l.magic);
	nmg_km(m);
	lu_copy = (struct loopuse *)NULL;
    }

    /* write editing changes out to disc */
    if (!illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    dp = LAST_SOLID(bdata);
    if (!dp) {
	/* sanity check, unexpected error */
	return TCL_ERROR;
    }

    /* make sure that any BOT solid is minimally legal */
    if (s->edit_state.es_int.idb_type == ID_BOT) {
	struct rt_bot_internal *bot = (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;

	RT_BOT_CK_MAGIC(bot);
	if (bot->mode == RT_BOT_SURFACE || bot->mode == RT_BOT_SOLID) {
	    /* make sure facemodes and thicknesses have been freed */
	    if (bot->thickness) {
		bu_free((char *)bot->thickness, "BOT thickness");
		bot->thickness = NULL;
	    }
	    if (bot->face_mode) {
		bu_free((char *)bot->face_mode, "BOT face_mode");
		bot->face_mode = NULL;
	    }
	} else {
	    /* make sure face_modes and thicknesses exist */
	    if (!bot->thickness)
		bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
	    if (!bot->face_mode) {
		bot->face_mode = bu_bitv_new(bot->num_faces);
	    }
	}
    }

    /* Scale change on export is 1.0 -- no change */
    if (rt_db_put_internal(dp, s->dbip, &s->edit_state.es_int, &rt_uniresource) < 0) {
	Tcl_AppendResult(s->interp, "sedit_apply(", dp->d_namep,
			 "):  solid export failure\n", (char *)NULL);
	if (accept_flag) {
	    rt_db_free_internal(&s->edit_state.es_int);
	}
	return TCL_ERROR;				/* FAIL */
    }

    if (accept_flag) {
	menu_state->ms_flag = 0;
	movedir = 0;
	es_edflag = -1;
	s->edit_state.e_edclass = EDIT_CLASS_NULL;

	rt_db_free_internal(&s->edit_state.es_int);
    } else {
	/* XXX hack to restore s->edit_state.es_int after rt_db_put_internal blows it away */
	/* Read solid description into s->edit_state.es_int again! Gaak! */
	if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			       s->dbip, NULL, &rt_uniresource) < 0) {
	    Tcl_AppendResult(s->interp, "sedit_apply(",
			     LAST_SOLID(bdata)->d_namep,
			     "):  solid reimport failure\n", (char *)NULL);
	    rt_db_free_internal(&s->edit_state.es_int);
	    return TCL_ERROR;
	}
    }


    return TCL_OK;
}


void
sedit_accept(struct mged_state *s)
{
    if (s->dbip == DBI_NULL)
	return;

    if (not_state(s, ST_S_EDIT, "Solid edit accept"))
	return;

    if (s->dbip->dbi_read_only) {
	sedit_reject(s);
	bu_log("Sorry, this database is READ-ONLY\n");
	pr_prompt(s);
	return;
    }

    if (sedraw > 0)
	sedit(s);

    (void)sedit_apply(s, 1);
}


void
sedit_reject(struct mged_state *s)
{
    if (not_state(s, ST_S_EDIT, "Solid edit reject") || !illump) {
	return;
    }

    if (sedraw > 0)
	sedit(s);

    es_eu = (struct edgeuse *)NULL;	/* Reset es_eu */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL; /* Reset es_pipe_pnt */
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
    bot_verts[0] = -1;
    bot_verts[1] = -1;
    bot_verts[2] = -1;
    es_ars_crv = (-1);
    es_ars_col = (-1);

    if (lu_copy) {
	struct model *m;
	m = nmg_find_model(&lu_copy->l.magic);
	nmg_km(m);
	lu_copy = (struct loopuse *)NULL;
    }

    /* Restore the original solid everywhere */
    {
	struct display_list *gdlp;
	struct display_list *next_gdlp;
	struct bv_scene_obj *sp;
	if (!illump->s_u_data)
	    return;
	struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

	gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)ged_dl(s->gedp));
	while (BU_LIST_NOT_HEAD(gdlp, (struct bu_list *)ged_dl(s->gedp))) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (!sp->s_u_data)
		    continue;
		struct ged_bv_data *bdatas = (struct ged_bv_data *)sp->s_u_data;
		if (LAST_SOLID(bdatas) == LAST_SOLID(bdata))
		    (void)replot_original_solid(s, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    menu_state->ms_flag = 0;
    movedir = 0;
    es_edflag = -1;
    s->edit_state.e_edclass = EDIT_CLASS_NULL;

    rt_db_free_internal(&s->edit_state.es_int);
}


int
mged_param(struct mged_state *s, Tcl_Interp *interp, int argc, fastf_t *argvect)
{
    int i;

    CHECK_DBI_NULL;

    if (es_edflag <= 0) {
	Tcl_AppendResult(interp,
			 "A solid editor option not selected\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    s->edit_state.e_inpara = 0;
    for (i = 0; i < argc; i++) {
	s->edit_state.e_para[ s->edit_state.e_inpara++ ] = argvect[i];
    }

    if (PARAM_1ARG) {
	if (s->edit_state.e_inpara != 1) {
	    Tcl_AppendResult(interp, "ERROR: only one argument needed\n", (char *)NULL);
	    s->edit_state.e_inpara = 0;
	    return TCL_ERROR;
	}

	if (es_menu == MENU_PIPE_PT_OD || es_menu == MENU_PIPE_PT_ID || es_menu == MENU_PIPE_SCALE_ID
	    || es_menu == MENU_METABALL_SET_THRESHOLD || es_menu == MENU_METABALL_SET_METHOD
	    || es_menu == MENU_METABALL_PT_SET_GOO)
	{
	    if (s->edit_state.e_para[0] < 0.0) {
		Tcl_AppendResult(interp, "ERROR: SCALE FACTOR < 0\n", (char *)NULL);
		s->edit_state.e_inpara = 0;
		return TCL_ERROR;
	    }
	} else {
	    if (s->edit_state.e_para[0] <= 0.0) {
		Tcl_AppendResult(interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
		s->edit_state.e_inpara = 0;
		return TCL_ERROR;
	    }
	}
    } else if (PARAM_2ARG) {
	if (s->edit_state.e_inpara != 2) {
	    Tcl_AppendResult(interp, "ERROR: two arguments needed\n", (char *)NULL);
	    s->edit_state.e_inpara = 0;
	    return TCL_ERROR;
	}

	if (s->edit_state.e_para[0] <= 0.0) {
	    Tcl_AppendResult(interp, "ERROR: X SIZE <= 0\n", (char *)NULL);
	    s->edit_state.e_inpara = 0;
	    return TCL_ERROR;
	} else if (s->edit_state.e_para[1] <= 0.0) {
	    Tcl_AppendResult(interp, "ERROR: Y SIZE <= 0\n", (char *)NULL);
	    s->edit_state.e_inpara = 0;
	    return TCL_ERROR;
	}
    } else {
	if (s->edit_state.e_inpara != 3) {
	    Tcl_AppendResult(interp, "ERROR: three arguments needed\n", (char *)NULL);
	    s->edit_state.e_inpara = 0;
	    return TCL_ERROR;
	}
    }

    /* check if need to convert input values to the base unit */
    switch (es_edflag) {

	case STRANS:
	case ECMD_VTRANS:
	case PSCALE:
	case EARB:
	case ECMD_ARB_MOVE_FACE:
	case ECMD_TGC_MV_H:
	case ECMD_TGC_MV_HH:
	case PTARB:
	case ECMD_NMG_ESPLIT:
	case ECMD_NMG_EMOVE:
	case ECMD_NMG_LEXTRU:
	case ECMD_PIPE_PICK:
	case ECMD_PIPE_SPLIT:
	case ECMD_PIPE_PT_MOVE:
	case ECMD_PIPE_PT_ADD:
	case ECMD_PIPE_PT_INS:
	case ECMD_ARS_PICK:
	case ECMD_ARS_MOVE_PT:
	case ECMD_ARS_MOVE_CRV:
	case ECMD_ARS_MOVE_COL:
	case ECMD_VOL_CSIZE:
	case ECMD_DSP_SCALE_X:
	case ECMD_DSP_SCALE_Y:
	case ECMD_DSP_SCALE_ALT:
	case ECMD_EBM_HEIGHT:
	case ECMD_EXTR_SCALE_H:
	case ECMD_CLINE_SCALE_H:
	case ECMD_CLINE_SCALE_T:
	case ECMD_CLINE_SCALE_R:
	case ECMD_CLINE_MOVE_H:
	case ECMD_EXTR_MOV_H:
	case ECMD_BOT_THICK:
	case ECMD_BOT_MOVET:
	case ECMD_BOT_MOVEE:
	case ECMD_BOT_MOVEV:
	case ECMD_METABALL_PT_PICK:
	case ECMD_METABALL_PT_MOV:
	case ECMD_METABALL_PT_ADD:
	    /* must convert to base units */
	    s->edit_state.e_para[0] *= s->dbip->dbi_local2base;
	    s->edit_state.e_para[1] *= s->dbip->dbi_local2base;
	    s->edit_state.e_para[2] *= s->dbip->dbi_local2base;
	    /* fall through */
	default:
	    break;
    }

    sedit(s);

    if (SEDIT_TRAN) {
	vect_t diff;
	fastf_t inv_Viewscale = 1/view_state->vs_gvp->gv_scale;

	VSUB2(diff, s->edit_state.e_para, s->edit_state.e_axes_pos);
	VSCALE(s->edit_state.k.tra_m_abs, diff, inv_Viewscale);
	VMOVE(s->edit_state.k.tra_m_abs_last, s->edit_state.k.tra_m_abs);
    } else if (SEDIT_ROTATE) {
	VMOVE(s->edit_state.k.rot_m_abs, s->edit_state.e_para);
    } else if (SEDIT_SCALE) {
	s->edit_state.k.sca_abs = s->edit_state.acc_sc_sol - 1.0;
	if (s->edit_state.k.sca_abs > 0)
	    s->edit_state.k.sca_abs /= 3.0;
    }
    return TCL_OK;
}


/* Input parameter editing changes from keyboard */
/* Format: p dx [dy dz]		*/
int
f_param(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int i;
    vect_t argvect;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 2 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help p");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    for (i = 1; i < argc && i <= 3; i++) {
	argvect[i-1] = atof(argv[i]);
    }

    return mged_param(s, interp, argc-1, argvect);
}


/*
 * Put labels on the vertices of the currently edited solid.
 * XXX This really should use import/export interface! Or be part of it.
 */
void
label_edited_solid(
    struct mged_state *s,
    int *num_lines, // NOTE - used only for BOTs
    point_t *lines, // NOTE - used only for BOTs
    struct rt_point_labels pl[],
    int max_pl,
    const mat_t xform,
    struct rt_db_internal *ip)
{
    point_t work;
    point_t pos_view;
    int npl = 0;

    RT_CK_DB_INTERNAL(ip);

    switch (ip->idb_type) {

#define POINT_LABEL(_pt, _char) { \
	VMOVE(pl[npl].pt, _pt); \
	pl[npl].str[0] = _char; \
	pl[npl++].str[1] = '\0'; }

#define POINT_LABEL_STR(_pt, _str) { \
	VMOVE(pl[npl].pt, _pt); \
	bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }

	case ID_ARS:
	    {
		struct rt_ars_internal *ars=
		    (struct rt_ars_internal *)s->edit_state.es_int.idb_ptr;

		RT_ARS_CK_MAGIC(ars);
		npl = OBJ[ip->idb_type].ft_labels(pl, max_pl, xform, &s->edit_state.es_int, &s->tol.tol);

		// Conditional additional labeling
		if (es_ars_crv >= 0 && es_ars_col >= 0) {
		    point_t ars_pt;

		    VMOVE(work, &ars->curves[es_ars_crv][es_ars_col*3]);
		    MAT4X3PNT(ars_pt, xform, work);
		    POINT_LABEL_STR(ars_pt, "pt");
		}
	    }
	    break;

	case ID_BSPLINE:
	    {
		struct rt_nurb_internal *sip =
		    (struct rt_nurb_internal *) s->edit_state.es_int.idb_ptr;
		RT_NURB_CK_MAGIC(sip);

		// Conditional labeling
		struct face_g_snurb *surf;
		fastf_t *fp;
		surf = sip->srfs[spl_surfno];
		NMG_CK_SNURB(surf);
		fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
		MAT4X3PNT(pos_view, xform, fp);
		POINT_LABEL(pos_view, 'V');

		fp = &RT_NURB_GET_CONTROL_POINT(surf, 0, 0);
		MAT4X3PNT(pos_view, xform, fp);
		POINT_LABEL_STR(pos_view, " 0,0");
		fp = &RT_NURB_GET_CONTROL_POINT(surf, 0, surf->s_size[1]-1);
		MAT4X3PNT(pos_view, xform, fp);
		POINT_LABEL_STR(pos_view, " 0,u");
		fp = &RT_NURB_GET_CONTROL_POINT(surf, surf->s_size[0]-1, 0);
		MAT4X3PNT(pos_view, xform, fp);
		POINT_LABEL_STR(pos_view, " v,0");
		fp = &RT_NURB_GET_CONTROL_POINT(surf, surf->s_size[0]-1, surf->s_size[1]-1);
		MAT4X3PNT(pos_view, xform, fp);
		POINT_LABEL_STR(pos_view, " u,v");

	    }
	    break;
	case ID_NMG:
	    /* New way only */
	    {
#ifndef NO_MAGIC_CHECKING
		struct model *m =
		    (struct model *) s->edit_state.es_int.idb_ptr;
		NMG_CK_MODEL(m);
#endif

		// Conditional labeling
		if (es_eu) {
		    point_t cent;
		    NMG_CK_EDGEUSE(es_eu);
		    VADD2SCALE(cent,
			       es_eu->vu_p->v_p->vg_p->coord,
			       es_eu->eumate_p->vu_p->v_p->vg_p->coord,
			       0.5);
		    MAT4X3PNT(pos_view, xform, cent);
		    POINT_LABEL_STR(pos_view, " eu");
		}
	    }
	    break;
	case ID_PIPE:
	    {
#ifndef NO_MAGIC_CHECKING
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)s->edit_state.es_int.idb_ptr;

		RT_PIPE_CK_MAGIC(pipeip);
#endif

		// Conditional labeling
		if (es_pipe_pnt) {
		    BU_CKMAG(es_pipe_pnt, WDB_PIPESEG_MAGIC, "wdb_pipe_pnt");

		    MAT4X3PNT(pos_view, xform, es_pipe_pnt->pp_coord);
		    POINT_LABEL_STR(pos_view, "pt");
		}
	    }
	    break;
	case ID_BOT:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)s->edit_state.es_int.idb_ptr;

		RT_BOT_CK_MAGIC(bot);

		// Conditional labeling
		if (bot_verts[2] > -1 &&
		    bot_verts[1] > -1 &&
		    bot_verts[0] > -1)
		{
		    /* editing a face */
		    point_t mid_pt;
		    point_t p1, p2, p3;
		    fastf_t one_third = 1.0/3.0;

		    MAT4X3PNT(p1, xform, &bot->vertices[bot_verts[0]*3]);
		    MAT4X3PNT(p2, xform, &bot->vertices[bot_verts[1]*3]);
		    MAT4X3PNT(p3, xform, &bot->vertices[bot_verts[2]*3]);
		    VADD3(mid_pt, p1, p2, p3);

		    VSCALE(mid_pt, mid_pt, one_third);

		    *num_lines = 3;
		    VMOVE(lines[0], mid_pt);
		    VMOVE(lines[1], p1);
		    VMOVE(lines[2], mid_pt);
		    VMOVE(lines[3], p2);
		    VMOVE(lines[4], mid_pt);
		    VMOVE(lines[5], p3);
		} else if (bot_verts[1] > -1 && bot_verts[0] > -1) {
		    /* editing an edge */
		    point_t mid_pt;

		    VBLEND2(mid_pt, 0.5, &bot->vertices[bot_verts[0]*3],
			    0.5, &bot->vertices[bot_verts[1]*3]);

		    MAT4X3PNT(pos_view, xform, mid_pt);
		    POINT_LABEL_STR(pos_view, "edge");
		}
		if (bot_verts[0] > -1) {
		    /* editing something, always label the vertex (this is the keypoint) */
		    MAT4X3PNT(pos_view, xform, &bot->vertices[bot_verts[0]*3]);
		    POINT_LABEL_STR(pos_view, "pt");
		}
	    }
	    break;
	case ID_METABALL:
	    {
#ifndef NO_MAGIC_CHECKING
		struct rt_metaball_internal *metaball =
		    (struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;

		RT_METABALL_CK_MAGIC(metaball);
#endif

		if (es_metaball_pnt) {
		    BU_CKMAG(es_metaball_pnt, WDB_METABALLPT_MAGIC, "wdb_metaball_pnt");

		    MAT4X3PNT(pos_view, xform, es_metaball_pnt->coord);
		    POINT_LABEL_STR(pos_view, "pt");
		}
	    }

	    break;

	default:
	    if (OBJ[ip->idb_type].ft_labels)
		npl = OBJ[ip->idb_type].ft_labels(pl, max_pl, xform, &s->edit_state.es_int, &s->tol.tol);
	    break;

    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}


/* -------------------------------- */
void
sedit_vpick(struct mged_state *s, point_t v_pos)
{
    point_t m_pos;
    int surfno, u, v;

    MAT4X3PNT(m_pos, view_state->vs_objview2model, v_pos);

    if (nurb_closest2d(&surfno, &u, &v,
		       (struct rt_nurb_internal *)s->edit_state.es_int.idb_ptr,
		       m_pos, view_state->vs_model2objview) >= 0) {
	spl_surfno = surfno;
	spl_ui = u;
	spl_vi = v;
	get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);
    }
    chg_state(s, ST_S_VPICK, ST_S_EDIT, "Vertex Pick Complete");
    view_state->vs_flag = 1;
}


#define DIST2D(P0, P1) sqrt(((P1)[X] - (P0)[X])*((P1)[X] - (P0)[X]) + \
				((P1)[Y] - (P0)[Y])*((P1)[Y] - (P0)[Y]))

#define DIST3D(P0, P1) sqrt(((P1)[X] - (P0)[X])*((P1)[X] - (P0)[X]) + \
				((P1)[Y] - (P0)[Y])*((P1)[Y] - (P0)[Y]) + \
				((P1)[Z] - (P0)[Z])*((P1)[Z] - (P0)[Z]))

/*
 * Given a pointer (vhead) to vlist point coordinates, a reference
 * point (ref_pt), and a transformation matrix (mat), pass back in
 * "closest_pt" the original, untransformed 3 space coordinates of
 * the point nearest the reference point after all points have been
 * transformed into 2 space projection plane coordinates.
 */
int
nurb_closest2d(
    int *surface,
    int *uval,
    int *vval,
    const struct rt_nurb_internal *spl,
    const point_t ref_pt,
    const mat_t mat)
{
    struct face_g_snurb *srf;
    point_t ref_2d;
    fastf_t *mesh;
    fastf_t d;
    fastf_t c_dist;		/* closest dist so far */
    int c_surfno;
    int c_u, c_v;
    int u, v;
    int i;

    RT_NURB_CK_MAGIC(spl);

    c_dist = INFINITY;
    c_surfno = c_u = c_v = -1;

    /* transform reference point to 2d */
    MAT4X3PNT(ref_2d, mat, ref_pt);

    for (i = 0; i < spl->nsrf; i++) {
	int advance;

	srf = spl->srfs[i];
	NMG_CK_SNURB(srf);
	mesh = srf->ctl_points;
	advance = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	for (v = 0; v < srf->s_size[0]; v++) {
	    for (u = 0; u < srf->s_size[1]; u++) {
		point_t cur;
		/* XXX 4-tuples? */
		MAT4X3PNT(cur, mat, mesh);
		d = DIST2D(ref_2d, cur);
		if (d < c_dist) {
		    c_dist = d;
		    c_surfno = i;
		    c_u = u;
		    c_v = v;
		}
		mesh += advance;
	    }
	}
    }
    if (c_surfno < 0) return -1;		/* FAIL */
    *surface = c_surfno;
    *uval = c_u;
    *vval = c_v;

    return 0;				/* success */
}


int
f_keypoint(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    CHECK_DBI_NULL;

    if (argc < 1 || 4 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "help keypoint");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((s->edit_state.global_editing_state != ST_S_EDIT) && (s->edit_state.global_editing_state != ST_O_EDIT)) {
	state_err(s, "keypoint assignment");
	return TCL_ERROR;
    }

    switch (--argc) {
	case 0:
	    {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		point_t key;

		VSCALE(key, s->edit_state.e_keypoint, s->dbip->dbi_base2local);
		bu_vls_printf(&tmp_vls, "%s (%g, %g, %g)\n", s->edit_state.e_keytag, V3ARGS(key));
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	    }

	    break;
	case 3:
	    VSET(s->edit_state.e_keypoint,
		 atof(argv[1]) * s->dbip->dbi_local2base,
		 atof(argv[2]) * s->dbip->dbi_local2base,
		 atof(argv[3]) * s->dbip->dbi_local2base);
	    s->edit_state.e_keytag = "user-specified";
	    s->edit_state.e_keyfixed = 1;
	    break;
	case 1:
	    if (BU_STR_EQUAL(argv[1], "reset")) {
		s->edit_state.e_keytag = "";
		s->edit_state.e_keyfixed = 0;
		get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag,
				   &s->edit_state.es_int, s->edit_state.e_mat);
		break;
	    }
	    /* fall through */
	default:
	    Tcl_AppendResult(interp, "Usage: 'keypoint [<x y z> | reset]'\n", (char *)NULL);
	    return TCL_ERROR;
    }

    view_state->vs_flag = 1;
    return TCL_OK;
}


int
f_get_sedit_menus(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct menu_item *mip = (struct menu_item *)NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->edit_state.global_editing_state != ST_S_EDIT)
	return TCL_ERROR;

    switch (s->edit_state.es_int.idb_type) {
	case ID_ARB8:
	    {
		struct bu_vls vls2 = BU_VLS_INIT_ZERO;

		/* title */
		bu_vls_printf(&vls, "{{ARB MENU} {}}");

		/* build "move edge" menu */
		mip = which_menu[s->edit_state.e_type-4];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[1].menu_string, bu_vls_addr(&vls2));
		bu_vls_trunc(&vls2, 0);

		/* build "move face" menu */
		mip = which_menu[s->edit_state.e_type+1];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[2].menu_string, bu_vls_addr(&vls2));
		bu_vls_trunc(&vls2, 0);

		/* build "rotate face" menu */
		mip = which_menu[s->edit_state.e_type+6];
		/* submenu title */
		bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		bu_vls_printf(&vls, " {{%s} {%s}}", cntrl_menu[3].menu_string, bu_vls_addr(&vls2));
		bu_vls_free(&vls2);
	    }

	    break;
	case ID_ARS:
	    {
		struct bu_vls vls2 = BU_VLS_INIT_ZERO;

		/* build ARS PICK MENU Tcl list */

		mip = ars_pick_menu;
		/* title */
		bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);
		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

		mip = ars_menu;
		/* title */
		bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

		/* pick vertex menu */
		bu_vls_printf(&vls, " {{%s} {%s}}", (++mip)->menu_string,
			      bu_vls_addr(&vls2));

		for (++mip; mip->menu_func != NULL; ++mip)
		    bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

		bu_vls_free(&vls2);
	    }

	    break;
	default:
	    switch (s->edit_state.es_int.idb_type) {
		case ID_TGC:
		    mip = tgc_menu;
		    break;
		case ID_TOR:
		    mip = tor_menu;
		    break;
		case ID_ELL:
		    mip = ell_menu;
		    break;
		case ID_SUPERELL:
		    mip = superell_menu;
		    break;
		case ID_BSPLINE:
		    mip = spline_menu;
		    break;
		case ID_RPC:
		    mip = rpc_menu;
		    break;
		case ID_RHC:
		    mip = rhc_menu;
		    break;
		case ID_EPA:
		    mip = epa_menu;
		    break;
		case ID_EHY:
		    mip = ehy_menu;
		    break;
		case ID_HYP:
		    mip = hyp_menu;
		    break;
		case ID_ETO:
		    mip = eto_menu;
		    break;
		case ID_NMG:
		    mip = nmg_menu;
		    break;
		case ID_PIPE:
		    mip = pipe_menu;
		    break;
		case ID_METABALL:
		    mip = metaball_menu;
		    break;
		case ID_VOL:
		    mip = vol_menu;
		    break;
		case ID_EBM:
		    mip = ebm_menu;
		    break;
		case ID_DSP:
		    mip = dsp_menu;
		    break;
		case ID_PARTICLE:
		    mip = part_menu;
		    break;
		case ID_BOT:
		    mip = bot_menu;
		    break;
		case ID_EXTRUDE:
		    mip = extr_menu;
		    break;
		case ID_CLINE:
		    mip = cline_menu;
		    break;
	    }

	    if (mip == (struct menu_item *)NULL)
		break;

	    /* title */
	    bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

	    for (++mip; mip->menu_func != NULL; ++mip)
		bu_vls_printf(&vls, " {{%s} {}}", mip->menu_string);

	    break;
    }

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)0);
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_get_sedit(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int status;
    struct rt_db_internal ces_int;
    Tcl_Obj *pto;
    Tcl_Obj *pnto;

    if (argc < 1 || 2 < argc) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel get_sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->edit_state.global_editing_state != ST_S_EDIT || !illump) {
	Tcl_AppendResult(interp, "get_sed: must be in solid edit state", (char *)0);
	return TCL_ERROR;
    }

    if (illump || !illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    if (argc == 1) {
	struct bu_vls logstr = BU_VLS_INIT_ZERO;

	/* get solid type and parameters */
	RT_CK_DB_INTERNAL(&s->edit_state.es_int);
	RT_CK_FUNCTAB(s->edit_state.es_int.idb_meth);
	status = s->edit_state.es_int.idb_meth->ft_get(&logstr, &s->edit_state.es_int, (char *)0);
	Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
	pto = Tcl_GetObjResult(interp);

	bu_vls_free(&logstr);

	pnto = Tcl_NewObj();
	/* insert solid name, type and parameters */
	Tcl_AppendStringsToObj(pnto, LAST_SOLID(bdata)->d_namep, " ",
			       Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

	Tcl_SetObjResult(interp, pnto);
	return status;
    }

    if (argv[1][0] != '-' || argv[1][1] != 'c') {
	Tcl_AppendResult(interp, "Usage: get_sed [-c]", (char *)0);
	return TCL_ERROR;
    }

    /* apply matrices along the path */
    RT_DB_INTERNAL_INIT(&ces_int);
    transform_editing_solid(s, &ces_int, s->edit_state.e_mat, &s->edit_state.es_int, 0);

    /* get solid type and parameters */
    RT_CK_DB_INTERNAL(&ces_int);
    RT_CK_FUNCTAB(ces_int.idb_meth);
    {
	struct bu_vls logstr = BU_VLS_INIT_ZERO;

	status = ces_int.idb_meth->ft_get(&logstr, &ces_int, (char *)0);
	Tcl_AppendResult(interp, bu_vls_addr(&logstr), (char *)0);
	bu_vls_free(&logstr);
    }
    pto = Tcl_GetObjResult(interp);

    pnto = Tcl_NewObj();
    /* insert full pathname */
    {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	db_path_to_vls(&str, &bdata->s_fullpath);
	Tcl_AppendStringsToObj(pnto, bu_vls_addr(&str), NULL);
	bu_vls_free(&str);
    }

    /* insert solid type and parameters */
    Tcl_AppendStringsToObj(pnto, " ", Tcl_GetStringFromObj(pto, (int *)0), (char *)0);

    Tcl_SetObjResult(interp, pnto);

    rt_db_free_internal(&ces_int);

    return status;
}


int
f_put_sedit(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    const struct rt_functab *ftp;
    uint32_t save_magic;
    int context;

    /*XXX needs better argument checking */
    if (argc < 6) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "helpdevel put_sed");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (s->edit_state.global_editing_state != ST_S_EDIT) {
	Tcl_AppendResult(interp, "put_sed: must be in solid edit state", (char *)0);
	return TCL_ERROR;
    }

    /* look for -c */
    if (argv[1][0] == '-' && argv[1][1] == 'c') {
	context = 1;
	--argc;
	++argv;
    } else
	context = 0;

    ftp = rt_get_functab_by_label(argv[1]);
    if (ftp == NULL ||
	ftp->ft_parsetab == (struct bu_structparse *)NULL) {
	Tcl_AppendResult(interp, "put_sed: ", argv[1],
			 " object type is not supported for db get",
			 (char *)0);
	return TCL_ERROR;
    }

    RT_CK_FUNCTAB(s->edit_state.es_int.idb_meth);
    if (s->edit_state.es_int.idb_meth != ftp) {
	Tcl_AppendResult(interp,
			 "put_sed: idb_meth type mismatch",
			 (char *)0);
    }

    save_magic = *((uint32_t *)s->edit_state.es_int.idb_ptr);
    *((uint32_t *)s->edit_state.es_int.idb_ptr) = ftp->ft_internal_magic;
    {
	int ret;
	struct bu_vls vlog = BU_VLS_INIT_ZERO;

	ret = bu_structparse_argv(&vlog, argc-2, argv+2, ftp->ft_parsetab, (char *)s->edit_state.es_int.idb_ptr, NULL);
	Tcl_AppendResult(interp, bu_vls_addr(&vlog), (char *)NULL);
	bu_vls_free(&vlog);
	if (ret != BRLCAD_OK)
	    return TCL_ERROR;
    }
    *((uint32_t *)s->edit_state.es_int.idb_ptr) = save_magic;

    if (context)
	transform_editing_solid(s, &s->edit_state.es_int, s->edit_state.e_invmat, &s->edit_state.es_int, 1);

    /* must re-calculate the face plane equations for arbs */
    if (s->edit_state.es_int.idb_type == ID_ARB8) {
	struct rt_arb_internal *arb;
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;

	arb = (struct rt_arb_internal *)s->edit_state.es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if (rt_arb_calc_planes(&error_msg, arb, s->edit_state.e_type, es_peqn, &s->tol.tol) < 0)
	    Tcl_AppendResult(interp, bu_vls_addr(&error_msg), (char *)0);
	bu_vls_free(&error_msg);
    }

    if (!s->edit_state.e_keyfixed)
	get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    set_e_axes_pos(s, 0);
    replot_editing_solid(s);

    return TCL_OK;
}


int
f_sedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->edit_state.global_editing_state != ST_S_EDIT || !illump)
	return TCL_ERROR;

    if (argc != 1) {
	bu_vls_printf(&vls, "helpdevel sed_reset");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* free old copy */
    rt_db_free_internal(&s->edit_state.es_int);

    /* reset */
    es_pipe_pnt = (struct wdb_pipe_pnt *)NULL;
    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL;
    es_s = (struct shell *)NULL;
    es_eu = (struct edgeuse *)NULL;

    /* read in a fresh copy */
    if (!illump || !illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;
    if (rt_db_get_internal(&s->edit_state.es_int, LAST_SOLID(bdata),
			   s->dbip, NULL, &rt_uniresource) < 0) {
	if (bdata->s_fullpath.fp_len > 0) {
	    Tcl_AppendResult(interp, "sedit_reset(",
		    LAST_SOLID(bdata)->d_namep,
		    "):  solid import failure\n", (char *)NULL);
	} else {
	    Tcl_AppendResult(interp, "sedit_reset(NULL):  solid import failure\n", (char *)NULL);

	}
	return TCL_ERROR;				/* FAIL */
    }
    RT_CK_DB_INTERNAL(&s->edit_state.es_int);
    replot_editing_solid(s);

    /* Establish initial keypoint */
    s->edit_state.e_keytag = "";
    get_solid_keypoint(s, &s->edit_state.e_keypoint, &s->edit_state.e_keytag, &s->edit_state.es_int, s->edit_state.e_mat);

    /* Reset relevant variables */
    MAT_IDN(s->edit_state.acc_rot_sol);
    VSETALL(s->edit_state.k.rot_m_abs, 0.0);
    VSETALL(s->edit_state.k.rot_o_abs, 0.0);
    VSETALL(s->edit_state.k.rot_v_abs, 0.0);
    VSETALL(s->edit_state.k.rot_m_abs_last, 0.0);
    VSETALL(s->edit_state.k.rot_o_abs_last, 0.0);
    VSETALL(s->edit_state.k.rot_v_abs_last, 0.0);
    VSETALL(s->edit_state.k.tra_m_abs, 0.0);
    VSETALL(s->edit_state.k.tra_v_abs, 0.0);
    VSETALL(s->edit_state.k.tra_m_abs_last, 0.0);
    VSETALL(s->edit_state.k.tra_v_abs_last, 0.0);
    s->edit_state.k.sca_abs = 0.0;
    s->edit_state.acc_sc_sol = 1.0;
    VSETALL(s->edit_state.k.rot_m, 0.0);
    VSETALL(s->edit_state.k.rot_o, 0.0);
    VSETALL(s->edit_state.k.rot_v, 0.0);
    VSETALL(s->edit_state.k.tra_m, 0.0);
    VSETALL(s->edit_state.k.tra_v, 0.0);

    set_e_axes_pos(s, 1);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_sedit_apply(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    CHECK_DBI_NULL;

    if (not_state(s, ST_S_EDIT, "Solid edit apply")) {
	return TCL_ERROR;
    }

    if (sedraw > 0)
	sedit(s);

    init_sedit_vars(s);
    (void)sedit_apply(s, 0);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_oedit_reset(ClientData clientData, Tcl_Interp *interp, int argc, const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->edit_state.global_editing_state != ST_O_EDIT)
	return TCL_ERROR;

    if (argc != 1) {
	bu_vls_printf(&vls, "helpdevel oed_reset");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    oedit_reject(s);
    init_oedit_guts(s);

    new_edit_mats(s);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


int
f_oedit_apply(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;
    const char *strp = "";

    CHECK_DBI_NULL;
    oedit_apply(s, UP); /* apply changes, but continue editing */

    if (!illump->s_u_data)
	return TCL_ERROR;
    struct ged_bv_data *bdata = (struct ged_bv_data *)illump->s_u_data;

    /* Save aggregate path matrix */
    MAT_IDN(s->edit_state.e_mat);
    (void)db_path_to_mat(s->dbip, &bdata->s_fullpath, s->edit_state.e_mat, bdata->s_fullpath.fp_len-1, &rt_uniresource);

    /* get the inverse matrix */
    bn_mat_inv(s->edit_state.e_invmat, s->edit_state.e_mat);

    get_solid_keypoint(s, &s->edit_state.e_keypoint, &strp, &s->edit_state.es_int, s->edit_state.e_mat);
    init_oedit_vars(s);
    new_edit_mats(s);
    s->update_views = 1;
    dm_set_dirty(DMP, 1);

    /* active edit callback */
    bu_vls_printf(&vls, "active_edit_callback");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
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
