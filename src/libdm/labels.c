/*                          L A B E L S . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
#include "bio.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "db.h"
#include "mater.h"
#include "nmg.h"
#include "rtgeom.h"
#include "nurb.h"
#include "solid.h"
#include "dm.h"

/*
 * Put labels on the vertices of the currently edited solid.
 * XXX This really should use import/export interface!!! Or be part of it.
 *
 * This code was lifted from mged/edsol.c.
 * XXX This should probably live in librt.
 */
void
dm_label_primitive(struct rt_wdb *wdbp,
		   struct rt_point_labels pl[],
		   int max_pl,
		   const mat_t xform,
		   struct rt_db_internal *ip)
{
    int i;
    point_t work;
    point_t pos_view;
    int npl = 0;


#define POINT_LABEL(_pt, _char) {		\
	if (npl+1 < max_pl) {			\
	    VMOVE(pl[npl].pt, _pt);		\
	    pl[npl].str[0] = _char;		\
	    pl[npl++].str[1] = '\0';		\
	}					\
    }

#define POINT_LABEL_STR(_pt, _str) {				\
	VMOVE(pl[npl].pt, _pt);					\
	bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }


    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_major_type != DB5_MAJORTYPE_BRLCAD)
	/* silently ignore */
	return;

    switch (ip->idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_TOR: {
	    struct rt_tor_internal *tor =
		(struct rt_tor_internal *)ip->idb_ptr;
	    fastf_t r3, r4;
	    vect_t adir;

	    RT_TOR_CK_MAGIC(tor);

	    bn_vec_ortho(adir, tor->h);

	    MAT4X3PNT(pos_view, xform, tor->v);
	    POINT_LABEL(pos_view, 'V');

	    r3 = tor->r_a - tor->r_h;
	    VJOIN1(work, tor->v, r3, adir);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'I');

	    r4 = tor->r_a + tor->r_h;
	    VJOIN1(work, tor->v, r4, adir);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'O');

	    VJOIN1(work, tor->v, tor->r_a, adir);
	    VADD2(work, work, tor->h);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'H');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_TGC: {
	    struct rt_tgc_internal *tgc =
		(struct rt_tgc_internal *)ip->idb_ptr;

	    RT_TGC_CK_MAGIC(tgc);
	    MAT4X3PNT(pos_view, xform, tgc->v);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work, tgc->v, tgc->a);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'A');

	    VADD2(work, tgc->v, tgc->b);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'B');

	    VADD3(work, tgc->v, tgc->h, tgc->c);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'C');

	    VADD3(work, tgc->v, tgc->h, tgc->d);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'D');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_SPH:
	case DB5_MINORTYPE_BRLCAD_ELL: {
	    struct rt_ell_internal *ell =
		(struct rt_ell_internal *)ip->idb_ptr;

	    RT_ELL_CK_MAGIC(ell);

	    MAT4X3PNT(pos_view, xform, ell->v);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work, ell->v, ell->a);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'A');

	    VADD2(work, ell->v, ell->b);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'B');

	    VADD2(work, ell->v, ell->c);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'C');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_ARB8: {
	    int arbType;
	    struct rt_arb_internal *arb=
		(struct rt_arb_internal *)ip->idb_ptr;

	    RT_ARB_CK_MAGIC(arb);
	    arbType = rt_arb_std_type(ip, &wdbp->wdb_tol);

	    switch (arbType) {
		case ARB8:
		    for (i=0; i<8; i++) {
			MAT4X3PNT(pos_view, xform, arb->pt[i]);
			POINT_LABEL(pos_view, (unsigned char)i+'1');
		    }

		    break;
		case ARB7:
		    for (i=0; i<7; i++) {
			MAT4X3PNT(pos_view, xform, arb->pt[i]);
			POINT_LABEL(pos_view, (unsigned char)i+'1');
		    }

		    break;
		case ARB6:
		    for (i=0; i<5; i++) {
			MAT4X3PNT(pos_view, xform, arb->pt[i]);
			POINT_LABEL(pos_view, (unsigned char)i+'1');
		    }
		    MAT4X3PNT(pos_view, xform, arb->pt[6]);
		    POINT_LABEL(pos_view, '6');

		    break;
		case ARB5:
		    for (i=0; i<5; i++) {
			MAT4X3PNT(pos_view, xform, arb->pt[i]);
			POINT_LABEL(pos_view, (unsigned char)i+'1');
		    }

		    break;
		case ARB4:
		    for (i=0; i<3; i++) {
			MAT4X3PNT(pos_view, xform, arb->pt[i]);
			POINT_LABEL(pos_view, (unsigned char)i+'1');
		    }
		    MAT4X3PNT(pos_view, xform, arb->pt[4]);
		    POINT_LABEL(pos_view, '4');

		    break;
	    }
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_ARS: {
	    /*XXX Needs work */
	    struct rt_ars_internal *ars=
		(struct rt_ars_internal *)ip->idb_ptr;
	    int ars_crv = 0;
	    int ars_col = 0;

	    RT_ARS_CK_MAGIC(ars);

	    MAT4X3PNT(pos_view, xform, ars->curves[0])

		if (ars_crv >= 0 && ars_col >= 0) {
		    point_t ars_pt;

		    VMOVE(work, &ars->curves[ars_crv][ars_col*3]);
		    MAT4X3PNT(ars_pt, xform, work);
		    POINT_LABEL_STR(ars_pt, "pt");
		}
	}
	    POINT_LABEL(pos_view, 'V');

	    break;
	case DB5_MINORTYPE_BRLCAD_HALF:
	    break;
	case DB5_MINORTYPE_BRLCAD_REC:
	    break;
	case DB5_MINORTYPE_BRLCAD_POLY:
	    break;
	case DB5_MINORTYPE_BRLCAD_BSPLINE: {
	    /*XXX Needs work */
	    struct rt_nurb_internal *sip =
		(struct rt_nurb_internal *) ip->idb_ptr;
	    struct face_g_snurb *surf;
	    fastf_t *fp;
	    int spl_surfno = 0;
	    int spl_ui = 0;
	    int spl_vi = 0;

	    RT_NURB_CK_MAGIC(sip);
	    surf = sip->srfs[spl_surfno];
	    NMG_CK_SNURB(surf);
	    fp = &RT_NURB_GET_CONTROL_POINT(surf, spl_ui, spl_vi);
	    MAT4X3PNT(pos_view, xform, fp);
	    POINT_LABEL(pos_view, 'V');

	    fp = &RT_NURB_GET_CONTROL_POINT(surf, 0, 0);
	    MAT4X3PNT(pos_view, xform, fp);
	    POINT_LABEL_STR(pos_view, " 0, 0");
	    fp = &RT_NURB_GET_CONTROL_POINT(surf, 0, surf->s_size[1]-1);
	    MAT4X3PNT(pos_view, xform, fp);
	    POINT_LABEL_STR(pos_view, " 0, u");
	    fp = &RT_NURB_GET_CONTROL_POINT(surf, surf->s_size[0]-1, 0);
	    MAT4X3PNT(pos_view, xform, fp);
	    POINT_LABEL_STR(pos_view, " v, 0");
	    fp = &RT_NURB_GET_CONTROL_POINT(surf, surf->s_size[0]-1, surf->s_size[1]-1);
	    MAT4X3PNT(pos_view, xform, fp);
	    POINT_LABEL_STR(pos_view, " u, v");
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_NMG:
	    /*XXX Needs work */
#if 0
	    /* New way only */
	    {
#ifndef NO_MAGIC_CHECKING
		struct model *m =
		    (struct model *) ip->idb_ptr;
		NMG_CK_MODEL(m);
#endif

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
#endif

	    break;
	case DB5_MINORTYPE_BRLCAD_EBM:
	    break;
	case DB5_MINORTYPE_BRLCAD_VOL:
	    break;
	case DB5_MINORTYPE_BRLCAD_ARBN:
	    break;
	case DB5_MINORTYPE_BRLCAD_PIPE:
	    /*XXX Needs work */
#if 0
	    {
#ifndef NO_MAGIC_CHECKING
		struct rt_pipe_internal *pipe =
		    (struct rt_pipe_internal *)ip->idb_ptr;

		RT_PIPE_CK_MAGIC(pipe);
#endif

		if (es_pipept) {
		    BU_CKMAG(es_pipept, WDB_PIPESEG_MAGIC, "wdb_pipept");

		    MAT4X3PNT(pos_view, xform, es_pipept->pp_coord);
		    POINT_LABEL_STR(pos_view, "pt");
		}
	    }
#endif

	    break;
	case DB5_MINORTYPE_BRLCAD_PARTICLE: {
	    struct rt_part_internal *part =
		(struct rt_part_internal *)ip->idb_ptr;
	    vect_t Ru, ortho;

	    RT_PART_CK_MAGIC(part);

	    MAT4X3PNT(pos_view, xform, part->part_V);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work, part->part_V, part->part_H);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'H');

	    VMOVE(Ru, part->part_H);
	    VUNITIZE(Ru);
	    bn_vec_ortho(ortho, Ru);
	    VSCALE(work, ortho, part->part_vrad);
	    VADD2(work, part->part_V, work);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'v');

	    VSCALE(work, ortho, part->part_hrad);
	    VADD3(work, part->part_V, part->part_H, work);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'h');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_RPC: {
	    struct rt_rpc_internal *rpc =
		(struct rt_rpc_internal *)ip->idb_ptr;
	    vect_t Ru;

	    RT_RPC_CK_MAGIC(rpc);

	    MAT4X3PNT(pos_view, xform, rpc->rpc_V);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work, rpc->rpc_V, rpc->rpc_B);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'B');

	    VADD2(work, rpc->rpc_V, rpc->rpc_H);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'H');

	    VCROSS(Ru, rpc->rpc_B, rpc->rpc_H);
	    VUNITIZE(Ru);
	    VSCALE(Ru, Ru, rpc->rpc_r);
	    VADD2(work, rpc->rpc_V, Ru);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'r');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_RHC: {
	    struct rt_rhc_internal *rhc =
		(struct rt_rhc_internal *)ip->idb_ptr;
	    vect_t Ru;

	    RT_RHC_CK_MAGIC(rhc);

	    MAT4X3PNT(pos_view, xform, rhc->rhc_V);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work, rhc->rhc_V, rhc->rhc_B);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'B');

	    VADD2(work, rhc->rhc_V, rhc->rhc_H);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'H');

	    VCROSS(Ru, rhc->rhc_B, rhc->rhc_H);
	    VUNITIZE(Ru);
	    VSCALE(Ru, Ru, rhc->rhc_r);
	    VADD2(work, rhc->rhc_V, Ru);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'r');

	    VMOVE(work, rhc->rhc_B);
	    VUNITIZE(work);
	    VSCALE(work, work,
		   MAGNITUDE(rhc->rhc_B) + rhc->rhc_c);
	    VADD2(work, work, rhc->rhc_V);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'c');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_EPA: {
	    struct rt_epa_internal *epa =
		(struct rt_epa_internal *)ip->idb_ptr;
	    vect_t A, B;

	    RT_EPA_CK_MAGIC(epa);

	    MAT4X3PNT(pos_view, xform, epa->epa_V);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work, epa->epa_V, epa->epa_H);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'H');

	    VSCALE(A, epa->epa_Au, epa->epa_r1);
	    VADD2(work, epa->epa_V, A);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'A');

	    VCROSS(B, epa->epa_Au, epa->epa_H);
	    VUNITIZE(B);
	    VSCALE(B, B, epa->epa_r2);
	    VADD2(work, epa->epa_V, B);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'B');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_EHY: {
	    struct rt_ehy_internal *ehy =
		(struct rt_ehy_internal *)ip->idb_ptr;
	    vect_t A, B;

	    RT_EHY_CK_MAGIC(ehy);

	    MAT4X3PNT(pos_view, xform, ehy->ehy_V);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work, ehy->ehy_V, ehy->ehy_H);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'H');

	    VSCALE(A, ehy->ehy_Au, ehy->ehy_r1);
	    VADD2(work, ehy->ehy_V, A);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'A');

	    VCROSS(B, ehy->ehy_Au, ehy->ehy_H);
	    VUNITIZE(B);
	    VSCALE(B, B, ehy->ehy_r2);
	    VADD2(work, ehy->ehy_V, B);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'B');

	    VMOVE(work, ehy->ehy_H);
	    VUNITIZE(work);
	    VSCALE(work, work,
		   MAGNITUDE(ehy->ehy_H) + ehy->ehy_c);
	    VADD2(work, ehy->ehy_V, work);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'c');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_ETO: {
	    struct rt_eto_internal *eto =
		(struct rt_eto_internal *)ip->idb_ptr;
	    fastf_t ch, cv, dh, dv, cmag, phi;
	    vect_t Au, Nu;

	    RT_ETO_CK_MAGIC(eto);

	    MAT4X3PNT(pos_view, xform, eto->eto_V);
	    POINT_LABEL(pos_view, 'V');

	    VMOVE(Nu, eto->eto_N);
	    VUNITIZE(Nu);
	    bn_vec_ortho(Au, Nu);
	    VUNITIZE(Au);

	    cmag = MAGNITUDE(eto->eto_C);
	    /* get horizontal and vertical components of C and Rd */
	    cv = VDOT(eto->eto_C, Nu);
	    ch = sqrt(cmag*cmag - cv*cv);
	    /* angle between C and Nu */
	    phi = acos(cv / cmag);
	    dv = -eto->eto_rd * sin(phi);
	    dh = eto->eto_rd * cos(phi);

	    VJOIN2(work, eto->eto_V, eto->eto_r+ch, Au, cv, Nu);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'C');

	    VJOIN2(work, eto->eto_V, eto->eto_r+dh, Au, dv, Nu);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'D');

	    VJOIN1(work, eto->eto_V, eto->eto_r, Au);
	    MAT4X3PNT(pos_view, xform, work);
	    POINT_LABEL(pos_view, 'r');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_GRIP:
	    break;
	case DB5_MINORTYPE_BRLCAD_JOINT:
	    break;
	case DB5_MINORTYPE_BRLCAD_HF:
	    break;
	case DB5_MINORTYPE_BRLCAD_DSP:
	    break;
	case DB5_MINORTYPE_BRLCAD_SKETCH:
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    break;
	case DB5_MINORTYPE_BRLCAD_SUBMODEL:
	    break;
	case DB5_MINORTYPE_BRLCAD_CLINE: {
	    struct rt_cline_internal *cli =
		(struct rt_cline_internal *)ip->idb_ptr;
	    point_t work1;

	    RT_CLINE_CK_MAGIC(cli);

	    MAT4X3PNT(pos_view, xform, cli->v);
	    POINT_LABEL(pos_view, 'V');

	    VADD2(work1, cli->v, cli->h);
	    MAT4X3PNT(pos_view, xform, work1);
	    POINT_LABEL(pos_view, 'H');
	}

	    break;
	case DB5_MINORTYPE_BRLCAD_BOT:
	    /*XXX Needs work */
	    break;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    break;
	default:
	    break;
    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}


int
dm_draw_labels(struct dm *dmp,
	       struct rt_wdb *wdbp,
	       char *name,
	       mat_t viewmat,
	       int *labelsColor,
	       int (*labelsHook)(),
	       ClientData labelsHookClientdata)
{
#define MAX_PL 8+1
    struct rt_point_labels pl[MAX_PL];
    struct rt_db_internal intern;
    struct directory *dp;
    int i;
    struct db_tree_state ts;
    struct db_full_path path;

    if (labelsHook != (int (*)())0)
	return labelsHook(dmp, wdbp, name,
			  viewmat, labelsColor,
			  labelsHookClientdata);

    if (wdbp == (struct rt_wdb *)NULL ||
	name == (char *)NULL)
	return BRLCAD_ERROR;

    db_full_path_init(&path);
    ts = wdbp->wdb_initial_tree_state;     /* struct copy */
    ts.ts_dbip = wdbp->dbip;
    ts.ts_resp = &rt_uniresource;
    MAT_IDN(ts.ts_mat);

    if (db_follow_path_for_state(&ts, &path, name, 1))
	return BRLCAD_OK;

    dp = DB_FULL_PATH_CUR_DIR(&path);

    rt_db_get_internal(&intern, dp, wdbp->dbip, ts.ts_mat, &rt_uniresource);

    dm_label_primitive(wdbp, pl, MAX_PL, viewmat, &intern);

    DM_SET_FGCOLOR(dmp,
		   (unsigned char)labelsColor[0],
		   (unsigned char)labelsColor[1],
		   (unsigned char)labelsColor[2],
		   1, 1.0);

    for (i=0; i<MAX_PL; i++) {
	if (pl[i].str[0] == '\0')
	    break;

	DM_DRAW_STRING_2D(dmp, pl[i].str,
			  (((int)(pl[i].pt[X]*GED_MAX))+15)*INV_GED,
			  (((int)(pl[i].pt[Y]*GED_MAX))+15)*INV_GED, 0, 1);
    }

    db_free_full_path(&path);
    rt_db_free_internal(&intern);

    return BRLCAD_OK;
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
