/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file primitives/mirror.c
 *
 * Routine(s) to mirror objects.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"
#include "vmath.h"
#include "nurb.h"


/* FIXME: temporary until all mirror functions are migrated and the
 * functab is utilized.
 */
#define RT_DECLARE_MIRROR(name) extern int rt_##name##_mirror(struct rt_db_internal *ip, const plane_t plane)

RT_DECLARE_MIRROR(tor);
RT_DECLARE_MIRROR(tgc);
RT_DECLARE_MIRROR(ell);
RT_DECLARE_MIRROR(arb);
RT_DECLARE_MIRROR(half);
RT_DECLARE_MIRROR(grip);
RT_DECLARE_MIRROR(poly);
RT_DECLARE_MIRROR(bspline);
RT_DECLARE_MIRROR(arbn);
RT_DECLARE_MIRROR(pipe);
RT_DECLARE_MIRROR(particle);
RT_DECLARE_MIRROR(rpc);
RT_DECLARE_MIRROR(rhc);
RT_DECLARE_MIRROR(epa);
RT_DECLARE_MIRROR(eto);
RT_DECLARE_MIRROR(hyp);
RT_DECLARE_MIRROR(nmg);
RT_DECLARE_MIRROR(ars);
RT_DECLARE_MIRROR(ebm);
RT_DECLARE_MIRROR(dsp);
RT_DECLARE_MIRROR(vol);
RT_DECLARE_MIRROR(superell);
RT_DECLARE_MIRROR(comb);
RT_DECLARE_MIRROR(bot);
RT_DECLARE_MIRROR(nurb);


/*
  FIXME: missing mirror implementations

  RT_DECLARE_MIRROR(brep);
  RT_DECLARE_MIRROR(cline);
  RT_DECLARE_MIRROR(ehy);
  RT_DECLARE_MIRROR(extrude);
  RT_DECLARE_MIRROR(hf);
  RT_DECLARE_MIRROR(metaball);
  RT_DECLARE_MIRROR(pnts);
  RT_DECLARE_MIRROR(rec);
  RT_DECLARE_MIRROR(revolve);
  RT_DECLARE_MIRROR(sketch);
  RT_DECLARE_MIRROR(submodel);
*/


/**
 * Mirror an object about some axis at a specified point on the axis.
 * It is the caller's responsibility to retain and free the internal.
 *
 * Returns the modified internal object or NULL on error.
 **/
struct rt_db_internal *
rt_mirror(struct db_i *dbip,
	  struct rt_db_internal *ip,
	  point_t mirror_pt,
	  vect_t mirror_dir,
	  struct resource *resp)
{
    int id;
    int err;
    static fastf_t tol_dist_sq = 0.005 * 0.005;
    plane_t plane;

    RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);

    if (!NEAR_EQUAL(MAGSQ(mirror_dir), 1.0, tol_dist_sq)) {
	bu_log("ERROR: mirror direction is invalid\n");
	return NULL;
    }

    if (!resp) {
	resp=&rt_uniresource;
    }

    /* not the best, but consistent until v6 */
    id = ip->idb_type;

    /* set up the plane direction */
    VUNITIZE(mirror_dir);
    VMOVE(plane, mirror_dir);

    /* determine the plane offset */
    plane[W] = VDOT(mirror_pt, mirror_dir);

    switch (id) {
	case ID_TOR: {
	    err = rt_tor_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_TGC:
	case ID_REC: {
	    err = rt_tgc_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ELL:
	case ID_SPH: {
	    err = rt_ell_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ARB8: {
	    err = rt_arb_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_HALF: {
	    err = rt_half_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_GRIP: {
	    err = rt_grip_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_POLY: {
	    err = rt_poly_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_BSPLINE: {
	    err = rt_nurb_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ARBN: {
	    err = rt_arbn_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_PIPE: {
	    err = rt_pipe_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_PARTICLE: {
	    err = rt_particle_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_RPC: {
	    err = rt_rpc_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_RHC: {
	    err = rt_rhc_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_EPA: {
	    err = rt_epa_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ETO: {
	    err = rt_eto_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_HYP: {
	    err = rt_hyp_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_NMG: {
	    err = rt_nmg_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_ARS: {
	    err = rt_ars_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_EBM: {
	    err = rt_ebm_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_DSP: {
	    err = rt_dsp_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_VOL: {
	    err = rt_vol_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_SUPERELL: {
	    err = rt_superell_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_COMBINATION: {
	    err = rt_comb_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	case ID_BOT: {
	    err = rt_bot_mirror(ip, plane);
	    return err ? NULL : ip;
	}
	default: {
	    bu_log("Unknown or unsupported object type (id==%d)\n", id);
	}
    }

    /* sanity in case object is unknown */
    return NULL;
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
