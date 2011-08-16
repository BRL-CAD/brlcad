/*                    G E T _ S O L I D _ K P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/get_solid_kp.c
 *
 * Return the default keypoint for a solid.
 */

#include "common.h"

#include <string.h>

#include "raytrace.h"
#include "wdb.h"
#include "nurb.h"
#include "./ged_private.h"


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
 *
 * XXX This was shamelessly copied from mged/edsol.c
 */
int
_ged_get_solid_keypoint(struct ged *const gedp,
			fastf_t *const pt,
			char **strp,
			const struct rt_db_internal *const ip,
			const fastf_t *const mat)
{
    char *cp = *strp;
    point_t mpt;
    char buf[BUFSIZ];

    /* data for solid editing */
    struct rt_db_internal es_int;
    int bot_verts[3];		/* vertices for the BOT solid */
#if 0
    int es_ars_crv;	/* curve and column identifying selected ARS point */
    int es_ars_col;
    point_t es_pt;		/* coordinates of selected ARS point */
#endif
    struct edgeuse *es_eu=(struct edgeuse *)NULL;	/* Currently selected NMG edgeuse */
    struct wdb_pipept *es_pipept=(struct wdb_pipept *)NULL; /* Currently selected PIPE segment */
    struct wdb_metaballpt *es_metaballpt=(struct wdb_metaballpt *)NULL; /* Currently selected METABALL Point */
    struct bn_tol mged_tol;	/* calculation tolerance */

    RT_CK_DB_INTERNAL(ip);
    memset(buf, 0, BUFSIZ);

    switch (ip->idb_type) {
	case ID_CLINE:
	    {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)ip->idb_ptr;

		RT_CLINE_CK_MAGIC(cli);

		if (BU_STR_EQUAL(cp, "V")) {
		    VMOVE(mpt, cli->v);
		    *strp = "V";
		} else if (BU_STR_EQUAL(cp, "H")) {
		    VADD2(mpt, cli->v, cli->h);
		    *strp = "H";
		} else {
		    VMOVE(mpt, cli->v);
		    *strp = "V";
		}
		break;
	    }
	case ID_PARTICLE:
	    {
		struct rt_part_internal *part =
		    (struct rt_part_internal *)ip->idb_ptr;

		RT_PART_CK_MAGIC(part);

		if (BU_STR_EQUAL(cp, "V")) {
		    VMOVE(mpt, part->part_V);
		    *strp = "V";
		} else if (BU_STR_EQUAL(cp, "H")) {
		    VADD2(mpt, part->part_V, part->part_H);
		    *strp = "H";
		} else {
		    /* default */
		    VMOVE(mpt, part->part_V);
		    *strp = "V";
		}
		break;
	    }
	case ID_PIPE:
	    {
		struct rt_pipe_internal *pipeip =
		    (struct rt_pipe_internal *)ip->idb_ptr;
		struct wdb_pipept *pipe_seg;

		RT_PIPE_CK_MAGIC(pipeip);

		if (es_pipept == (struct wdb_pipept *)NULL) {
		    pipe_seg = BU_LIST_FIRST(wdb_pipept, &pipeip->pipe_segs_head);
		    VMOVE(mpt, pipe_seg->pp_coord);
		} else {
		    VMOVE(mpt, es_pipept->pp_coord);
		}
		*strp = "V";
		break;
	    }
	case ID_METABALL:
	    {
		struct rt_metaball_internal *metaball = (struct rt_metaball_internal *)ip->idb_ptr;

		RT_METABALL_CK_MAGIC(metaball);

		VSETALL(mpt, 0.0);
		if (es_metaballpt==NULL) {
		    snprintf(buf, BUFSIZ, "no point selected");
		} else {
		    VMOVE(mpt, es_metaballpt->coord);
		    snprintf(buf, BUFSIZ, "V %f", es_metaballpt->fldstr);
		}
		*strp = buf;
		break;
	    }
	case ID_ARBN:
	    {
		struct rt_arbn_internal *arbn =
		    (struct rt_arbn_internal *)ip->idb_ptr;
		size_t i, j, k;
		int good_vert = 0;

		RT_ARBN_CK_MAGIC(arbn);
		for (i=0; i<arbn->neqn; i++) {
		    for (j=i+1; j<arbn->neqn; j++) {
			for (k=j+1; k<arbn->neqn; k++) {
			    if (!bn_mkpoint_3planes(mpt, arbn->eqn[i], arbn->eqn[j], arbn->eqn[k])) {
				size_t l;

				good_vert = 1;
				for (l=0; l<arbn->neqn; l++) {
				    if (l == i || l == j || l == k)
					continue;

				    if (DIST_PT_PLANE(mpt, arbn->eqn[l]) > mged_tol.dist) {
					good_vert = 0;
					break;
				    }
				}

				if (good_vert)
				    break;
			    }
			    if (good_vert)
				break;
			}
			if (good_vert)
			    break;
		    }
		    if (good_vert)
			break;
		}

		*strp = "V";
		break;
	    }
	case ID_EBM:
	    {
		struct rt_ebm_internal *ebm =
		    (struct rt_ebm_internal *)ip->idb_ptr;
		point_t pnt;

		RT_EBM_CK_MAGIC(ebm);

		VSETALL(pnt, 0.0);
		MAT4X3PNT(mpt, ebm->mat, pnt);
		*strp = "V";
		break;
	    }
	case ID_BOT:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)ip->idb_ptr;

		if (bot_verts[0] > -1) {
		    VMOVE(mpt, &bot->vertices[bot_verts[0]*3]);
		} else {
		    VMOVE(mpt, bot->vertices);
		}

		break;
	    }
	case ID_DSP:
	    {
		struct rt_dsp_internal *dsp =
		    (struct rt_dsp_internal *)ip->idb_ptr;
		point_t pnt;

		RT_DSP_CK_MAGIC(dsp);

		VSETALL(pnt, 0.0);
		MAT4X3PNT(mpt, dsp->dsp_stom, pnt);
		*strp = "V";
		break;
	    }
	case ID_HF:
	    {
		struct rt_hf_internal *hf =
		    (struct rt_hf_internal *)ip->idb_ptr;

		RT_HF_CK_MAGIC(hf);

		VMOVE(mpt, hf->v);
		*strp = "V";
		break;
	    }
	case ID_VOL:
	    {
		struct rt_vol_internal *vol =
		    (struct rt_vol_internal *)ip->idb_ptr;
		point_t pnt;

		RT_VOL_CK_MAGIC(vol);

		VSETALL(pnt, 0.0);
		MAT4X3PNT(mpt, vol->mat, pnt);
		*strp = "V";
		break;
	    }
	case ID_HALF:
	    {
		struct rt_half_internal *haf =
		    (struct rt_half_internal *)ip->idb_ptr;
		RT_HALF_CK_MAGIC(haf);

		VSCALE(mpt, haf->eqn, haf->eqn[H]);
		*strp = "V";
		break;
	    }
	case ID_ARB8:
	    {
		struct rt_arb_internal *arb =
		    (struct rt_arb_internal *)ip->idb_ptr;
		RT_ARB_CK_MAGIC(arb);

		if (*cp == 'V') {
		    int vertex_number;
		    char *ptr;

		    ptr = cp + 1;
		    vertex_number = (*ptr) - '0';
		    if (vertex_number < 1 || vertex_number > 8)
			vertex_number = 1;
		    VMOVE(mpt, arb->pt[vertex_number-1]);
		    sprintf(buf, "V%d", vertex_number);
		    *strp = buf;
		    break;
		}

		/* Default */
		VMOVE(mpt, arb->pt[0]);
		*strp = "V1";

		break;
	    }
	case ID_ELL:
	case ID_SPH:
	    {
		struct rt_ell_internal *ell =
		    (struct rt_ell_internal *)ip->idb_ptr;
		RT_ELL_CK_MAGIC(ell);

		if (BU_STR_EQUAL(cp, "V")) {
		    VMOVE(mpt, ell->v);
		    *strp = "V";
		    break;
		}
		if (BU_STR_EQUAL(cp, "A")) {
		    VADD2(mpt, ell->v, ell->a);
		    *strp = "A";
		    break;
		}
		if (BU_STR_EQUAL(cp, "B")) {
		    VADD2(mpt, ell->v, ell->b);
		    *strp = "B";
		    break;
		}
		if (BU_STR_EQUAL(cp, "C")) {
		    VADD2(mpt, ell->v, ell->c);
		    *strp = "C";
		    break;
		}
		/* Default */
		VMOVE(mpt, ell->v);
		*strp = "V";
		break;
	    }
	case ID_SUPERELL:
	    {
		struct rt_superell_internal *superell =
		    (struct rt_superell_internal *)ip->idb_ptr;
		RT_SUPERELL_CK_MAGIC(superell);

		if (BU_STR_EQUAL(cp, "V")) {
		    VMOVE(mpt, superell->v);
		    *strp = "V";
		    break;
		}
		if (BU_STR_EQUAL(cp, "A")) {
		    VADD2(mpt, superell->v, superell->a);
		    *strp = "A";
		    break;
		}
		if (BU_STR_EQUAL(cp, "B")) {
		    VADD2(mpt, superell->v, superell->b);
		    *strp = "B";
		    break;
		}
		if (BU_STR_EQUAL(cp, "C")) {
		    VADD2(mpt, superell->v, superell->c);
		    *strp = "C";
		    break;
		}
		/* Default */
		VMOVE(mpt, superell->v);
		*strp = "V";
		break;
	    }
	case ID_TOR:
	    {
		struct rt_tor_internal *tor =
		    (struct rt_tor_internal *)ip->idb_ptr;
		RT_TOR_CK_MAGIC(tor);

		if (BU_STR_EQUAL(cp, "V")) {
		    VMOVE(mpt, tor->v);
		    *strp = "V";
		    break;
		}
		/* Default */
		VMOVE(mpt, tor->v);
		*strp = "V";
		break;
	    }
	case ID_TGC:
	case ID_REC:
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)ip->idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		if (BU_STR_EQUAL(cp, "V")) {
		    VMOVE(mpt, tgc->v);
		    *strp = "V";
		    break;
		}
		if (BU_STR_EQUAL(cp, "H")) {
		    VMOVE(mpt, tgc->h);
		    *strp = "H";
		    break;
		}
		if (BU_STR_EQUAL(cp, "A")) {
		    VMOVE(mpt, tgc->a);
		    *strp = "A";
		    break;
		}
		if (BU_STR_EQUAL(cp, "B")) {
		    VMOVE(mpt, tgc->b);
		    *strp = "B";
		    break;
		}
		if (BU_STR_EQUAL(cp, "C")) {
		    VMOVE(mpt, tgc->c);
		    *strp = "C";
		    break;
		}
		if (BU_STR_EQUAL(cp, "D")) {
		    VMOVE(mpt, tgc->d);
		    *strp = "D";
		    break;
		}
		/* Default */
		VMOVE(mpt, tgc->v);
		*strp = "V";
		break;
	    }
	case ID_BSPLINE:
	    {
		bu_vls_printf(gedp->ged_result_str,
			      "getting origin of BSPLINE temporarily disabled");
		return GED_ERROR;
#if 0
		struct rt_nurb_internal *sip =
		    (struct rt_nurb_internal *) es_int.idb_ptr;
		struct face_g_snurb *surf;
		fastf_t *fp;
		int spl_surfno;	/* What surf & ctl pt to edit on spline */
		int spl_ui;
		int spl_vi;

		RT_NURB_CK_MAGIC(sip);
		spl_surfno = sip->nsrf/2;
		surf = sip->srfs[spl_surfno];
		NMG_CK_SNURB(surf);
		spl_ui = surf->s_size[1]/2;
		spl_vi = surf->s_size[0]/2;

		fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
		VMOVE(mpt, fp);
		sprintf(buf, "Surf %d, index %d,%d",
			spl_surfno, spl_ui, spl_vi);
		*strp = buf;
		break;
#endif
	    }
	case ID_GRIP:
	    {
		struct rt_grip_internal *gip =
		    (struct rt_grip_internal *)ip->idb_ptr;
		RT_GRIP_CK_MAGIC(gip);
		VMOVE(mpt, gip->center);
		*strp = "C";
		break;
	    }
	case ID_ARS:
	    {
		bu_vls_printf(gedp->ged_result_str,
			      "getting origin of ARS temporarily disabled");
		return GED_ERROR;
#if 0
		struct rt_ars_internal *ars =
		    (struct rt_ars_internal *)es_int.idb_ptr;
		RT_ARS_CK_MAGIC(ars);

		if (es_ars_crv < 0 || es_ars_col < 0) {
		    VMOVE(mpt, es_pt);
		} else {
		    VMOVE(mpt, &ars->curves[es_ars_crv][es_ars_col*3]);
		}

		*strp = "V";
		break;
#endif
	    }
	case ID_RPC:
	    {
		struct rt_rpc_internal *rpc =
		    (struct rt_rpc_internal *)ip->idb_ptr;
		RT_RPC_CK_MAGIC(rpc);

		VMOVE(mpt, rpc->rpc_V);
		*strp = "V";
		break;
	    }
	case ID_RHC:
	    {
		struct rt_rhc_internal *rhc =
		    (struct rt_rhc_internal *)ip->idb_ptr;
		RT_RHC_CK_MAGIC(rhc);

		VMOVE(mpt, rhc->rhc_V);
		*strp = "V";
		break;
	    }
	case ID_EPA:
	    {
		struct rt_epa_internal *epa =
		    (struct rt_epa_internal *)ip->idb_ptr;
		RT_EPA_CK_MAGIC(epa);

		VMOVE(mpt, epa->epa_V);
		*strp = "V";
		break;
	    }
	case ID_EHY:
	    {
		struct rt_ehy_internal *ehy =
		    (struct rt_ehy_internal *)ip->idb_ptr;
		RT_EHY_CK_MAGIC(ehy);

		VMOVE(mpt, ehy->ehy_V);
		*strp = "V";
		break;
	    }
	case ID_HYP:
	    {
		struct rt_hyp_internal *hyp =
		    (struct rt_hyp_internal *)ip->idb_ptr;
		RT_HYP_CK_MAGIC(hyp);

		VMOVE(mpt, hyp->hyp_Vi);
		*strp = "V";
		break;
	    }
	case ID_ETO:
	    {
		struct rt_eto_internal *eto =
		    (struct rt_eto_internal *)ip->idb_ptr;
		RT_ETO_CK_MAGIC(eto);

		VMOVE(mpt, eto->eto_V);
		*strp = "V";
		break;
	    }
	case ID_POLY:
	    {
		struct rt_pg_face_internal *_poly;
		struct rt_pg_internal *pg =
		    (struct rt_pg_internal *)ip->idb_ptr;
		RT_PG_CK_MAGIC(pg);

		_poly = pg->poly;
		VMOVE(mpt, _poly->verts);
		*strp = "V";
		break;
	    }
	case ID_SKETCH:
	    {
		struct rt_sketch_internal *skt =
		    (struct rt_sketch_internal *)ip->idb_ptr;
		RT_SKETCH_CK_MAGIC(skt);

		VMOVE(mpt, skt->V);
		*strp = "V";
		break;
	    }
	case ID_EXTRUDE:
	    {
		struct rt_extrude_internal *extr =
		    (struct rt_extrude_internal *)ip->idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);

		if (extr->skt && extr->skt->verts) {
		    VJOIN2(mpt, extr->V, extr->skt->verts[0][0], extr->u_vec, extr->skt->verts[0][2], extr->v_vec);
		    *strp = "V1";
		} else {
		    VMOVE(mpt, extr->V);
		    *strp = "V";
		}
		break;
	    }
	case ID_NMG:
	    {
		struct vertex *v;
		struct vertexuse *vu;
		struct edgeuse *eu;
		struct loopuse *lu;
		struct faceuse *fu;
		struct shell *s;
		struct nmgregion *r;
		struct model *m =
		    (struct model *) es_int.idb_ptr;
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

		s = BU_LIST_FIRST(shell, &r->s_hd);
		if (!s)
		    break;
		NMG_CK_SHELL(s);

		if (BU_LIST_IS_EMPTY(&s->fu_hd))
		    fu = (struct faceuse *)NULL;
		else
		    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
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
		if (BU_LIST_IS_EMPTY(&s->lu_hd))
		    lu = (struct loopuse *)NULL;
		else
		    lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
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
		if (BU_LIST_IS_EMPTY(&s->eu_hd))
		    eu = (struct edgeuse *)NULL;
		else
		    eu = BU_LIST_FIRST(edgeuse, &s->eu_hd);
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
		vu = s->vu_p;
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
	    }
	default:
	    VSETALL(mpt, 0.0);
	    bu_vls_printf(gedp->ged_result_str,
			  "get_solid_keypoint: unrecognized solid type (setting keypoint to origin)");
	    *strp = "(origin)";
	    return GED_ERROR;
    }
    MAT4X3PNT(pt, mat, mpt);
    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
