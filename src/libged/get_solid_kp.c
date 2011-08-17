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
 * Default keypoint in model space is established in "pt". Returns
 * GED_ERROR if unable to determine a keypoint, otherwise returns
 * GED_OK.
 */
int
_ged_get_solid_keypoint(struct ged *const gedp,
			fastf_t *const pt,
			const struct rt_db_internal *const ip,
			const fastf_t *const mat)
{
    point_t mpt;

    RT_CK_DB_INTERNAL(ip);

    switch (ip->idb_type) {
	case ID_CLINE:
	    {
		struct rt_cline_internal *cli =
		    (struct rt_cline_internal *)ip->idb_ptr;

		RT_CLINE_CK_MAGIC(cli);

		VMOVE(mpt, cli->v);
		break;
	    }
	case ID_PARTICLE:
	    {
		struct rt_part_internal *part =
		    (struct rt_part_internal *)ip->idb_ptr;

		RT_PART_CK_MAGIC(part);

		VMOVE(mpt, part->part_V);
		break;
	    }
	case ID_PIPE:
	    {
		struct rt_pipe_internal *pipeip;
		struct wdb_pipept *pipe_seg;
		bu_vls_printf(gedp->ged_result_str,
			      "getting origin of PIPE temporarily disabled");
		return GED_ERROR;
		pipeip = (struct rt_pipe_internal *)ip->idb_ptr;

		RT_PIPE_CK_MAGIC(pipeip);

		pipe_seg = BU_LIST_FIRST(wdb_pipept, &pipeip->pipe_segs_head);
		VMOVE(mpt, pipe_seg->pp_coord);
		break;
	    }
	case ID_METABALL:
	    {
		struct rt_metaball_internal *metaball =
		    (struct rt_metaball_internal *)ip->idb_ptr;
		struct wdb_metaballpt *metaballpt;

		RT_METABALL_CK_MAGIC(metaball);

		VSETALL(mpt, 0.0);
		metaballpt = BU_LIST_FIRST(wdb_metaballpt,
					   &metaball->metaball_ctrl_head);
		VMOVE(mpt, metaballpt->coord);
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
			    if (!bn_mkpoint_3planes(mpt, arbn->eqn[i],
						    arbn->eqn[j],
						    arbn->eqn[k])) {
				size_t l;

				good_vert = 1;
				for (l=0; l<arbn->neqn; l++) {
				    if (l == i || l == j || l == k)
					continue;

				    if (DIST_PT_PLANE(mpt,
					arbn->eqn[l]) >
					gedp->ged_wdbp->wdb_tol.dist) {
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
		break;
	    }
	case ID_BOT:
	    {
		struct rt_bot_internal *bot =
		    (struct rt_bot_internal *)ip->idb_ptr;

		    VMOVE(mpt, bot->vertices);
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
		break;
	    }
	case ID_HF:
	    {
		struct rt_hf_internal *hf =
		    (struct rt_hf_internal *)ip->idb_ptr;

		RT_HF_CK_MAGIC(hf);

		VMOVE(mpt, hf->v);
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
		break;
	    }
	case ID_HALF:
	    {
		struct rt_half_internal *haf =
		    (struct rt_half_internal *)ip->idb_ptr;
		RT_HALF_CK_MAGIC(haf);

		VSCALE(mpt, haf->eqn, haf->eqn[H]);
		break;
	    }
	case ID_ARB8:
	    {
		struct rt_arb_internal *arb =
		    (struct rt_arb_internal *)ip->idb_ptr;
		RT_ARB_CK_MAGIC(arb);

		VMOVE(mpt, arb->pt[0]);
		break;
	    }
	case ID_ELL:
	case ID_SPH:
	    {
		struct rt_ell_internal *ell =
		    (struct rt_ell_internal *)ip->idb_ptr;
		RT_ELL_CK_MAGIC(ell);

		VMOVE(mpt, ell->v);
		break;
	    }
	case ID_SUPERELL:
	    {
		struct rt_superell_internal *superell =
		    (struct rt_superell_internal *)ip->idb_ptr;
		RT_SUPERELL_CK_MAGIC(superell);

		VMOVE(mpt, superell->v);
		break;
	    }
	case ID_TOR:
	    {
		struct rt_tor_internal *tor =
		    (struct rt_tor_internal *)ip->idb_ptr;
		RT_TOR_CK_MAGIC(tor);

		VMOVE(mpt, tor->v);
		break;
	    }
	case ID_TGC:
	case ID_REC:
	    {
		struct rt_tgc_internal *tgc =
		    (struct rt_tgc_internal *)ip->idb_ptr;
		RT_TGC_CK_MAGIC(tgc);

		VMOVE(mpt, tgc->v);
		break;
	    }
	case ID_GRIP:
	    {
		struct rt_grip_internal *gip =
		    (struct rt_grip_internal *)ip->idb_ptr;
		RT_GRIP_CK_MAGIC(gip);
		VMOVE(mpt, gip->center);
		break;
	    }
	case ID_ARS:
	    {
		struct rt_ars_internal *ars =
		    (struct rt_ars_internal *)ip->idb_ptr;
		RT_ARS_CK_MAGIC(ars);

		VMOVE(mpt, &ars->curves[0][0]);
		break;
	    }
	case ID_RPC:
	    {
		struct rt_rpc_internal *rpc =
		    (struct rt_rpc_internal *)ip->idb_ptr;
		RT_RPC_CK_MAGIC(rpc);

		VMOVE(mpt, rpc->rpc_V);
		break;
	    }
	case ID_RHC:
	    {
		struct rt_rhc_internal *rhc =
		    (struct rt_rhc_internal *)ip->idb_ptr;
		RT_RHC_CK_MAGIC(rhc);

		VMOVE(mpt, rhc->rhc_V);
		break;
	    }
	case ID_EPA:
	    {
		struct rt_epa_internal *epa =
		    (struct rt_epa_internal *)ip->idb_ptr;
		RT_EPA_CK_MAGIC(epa);

		VMOVE(mpt, epa->epa_V);
		break;
	    }
	case ID_EHY:
	    {
		struct rt_ehy_internal *ehy =
		    (struct rt_ehy_internal *)ip->idb_ptr;
		RT_EHY_CK_MAGIC(ehy);

		VMOVE(mpt, ehy->ehy_V);
		break;
	    }
	case ID_HYP:
	    {
		struct rt_hyp_internal *hyp =
		    (struct rt_hyp_internal *)ip->idb_ptr;
		RT_HYP_CK_MAGIC(hyp);

		VMOVE(mpt, hyp->hyp_Vi);
		break;
	    }
	case ID_ETO:
	    {
		struct rt_eto_internal *eto =
		    (struct rt_eto_internal *)ip->idb_ptr;
		RT_ETO_CK_MAGIC(eto);

		VMOVE(mpt, eto->eto_V);
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
		break;
	    }
	case ID_SKETCH:
	    {
		struct rt_sketch_internal *skt =
		    (struct rt_sketch_internal *)ip->idb_ptr;
		RT_SKETCH_CK_MAGIC(skt);

		VMOVE(mpt, skt->V);
		break;
	    }
	case ID_EXTRUDE:
	    {
		struct rt_extrude_internal *extr =
		    (struct rt_extrude_internal *)ip->idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);

		if (extr->skt && extr->skt->verts) {
		    VJOIN2(mpt, extr->V, extr->skt->verts[0][0], extr->u_vec,
			   extr->skt->verts[0][2], extr->v_vec);
		} else {
		    VMOVE(mpt, extr->V);
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
		    (struct model *) ip->idb_ptr;
		NMG_CK_MODEL(m);

		/* set default first */
		VSETALL(mpt, 0.0);

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
		    break;
		}
	    }
	default:
	    VSETALL(mpt, 0.0);
	    bu_vls_printf(gedp->ged_result_str,
			  "get_solid_keypoint: unrecognized solid type");
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
