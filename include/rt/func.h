/*                        R T F U N C . H
 * BRL-CAD
 *
 * Copyright (c) 2010-2025 United States Government as represented by
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
/** @addtogroup rt_obj
 *
 * @brief Primitive manipulation functions from former functab
 * callback table.
 *
 * As this is a relatively new set of interfaces, consider these
 * functions preliminary (i.e. DEPRECATED) and subject to change until
 * this message goes away.
 *
 */
#ifndef RT_FUNC_H
#define RT_FUNC_H

#include "common.h"

#include "bu/list.h"
#include "bu/parse.h"
#include "bu/vls.h"
#include "bg/plane.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/application.h"
#include "rt/functab.h"
#include "rt/hit.h"
#include "rt/piece.h"
#include "rt/resource.h"
#include "rt/seg.h"
#include "rt/soltab.h"
#include "rt/tol.h"
#include "rt/db_internal.h"
#include "rt/db_instance.h"
#include "rt/rt_instance.h"
#include "rt/xray.h"
#include "pc.h"

/** @{ */
/** @file rt/func.h */

__BEGIN_DECLS

/**
 * prep an object for ray tracing
 */
RT_EXPORT extern int rt_obj_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip);

/**
 * shoot a ray at an object that has been prepped for ray tracing
 */
RT_EXPORT extern int rt_obj_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead);

/**
 * TBD.
 */
RT_EXPORT extern int rt_obj_piece_shot(struct rt_piecestate *psp, struct rt_piecelist *plp, double dist_corr, struct xray *rp, struct application *ap, struct seg *seghead);

/**
 * TBD.
 */
RT_EXPORT extern int rt_obj_piece_hitsegs(struct rt_piecestate *psp, struct seg *seghead, struct application *ap);

/**
 * print an objects parameters in debug/diagnostic form
 */
RT_EXPORT extern int rt_obj_print(const struct soltab *stp);

/**
 * calculate a normal on an object that has been hit via rt_shot()
 */
RT_EXPORT extern int rt_obj_norm(struct hit *hitp, struct soltab *stp, struct xray *rp);

/**
 * calculate object uv parameterization for a given hit point
 */
RT_EXPORT extern int rt_obj_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp);

/**
 * calculate object curvature for a given hit point
 */
RT_EXPORT extern int rt_obj_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp);

/**
 * DEPRECATED: Unimplemented.
 */
DEPRECATED RT_EXPORT extern int rt_obj_class(void);

/**
 * release the memory used by a solid
 */
RT_EXPORT extern int rt_obj_free(struct soltab *stp);

/**
 * obtain a vlist wireframe representation of an object for plotting purposes
 */
RT_EXPORT extern int rt_obj_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

/**
 * shoot an array of rays at a set of homogeneous objects.
 */
RT_EXPORT extern int rt_obj_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap);

/**
 * tessellate an object (into NMG form)
 */
RT_EXPORT extern int rt_obj_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol);

/**
 * tessellate an object (into NURBS NMG form)
 */
RT_EXPORT extern int rt_obj_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol);

/**
 * v4/v5 object import from disk
 */
RT_EXPORT extern int rt_obj_import(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);

/**
 * v4/v5 object export to disk
 */
RT_EXPORT extern int rt_obj_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp);

/**
 * free the internal representation of an object
 */
RT_EXPORT extern int rt_obj_ifree(struct rt_db_internal *ip);

/**
 * db object 'get' support, obtain a tcl list representation
 */
RT_EXPORT extern int rt_obj_get(struct bu_vls *logstr, const struct rt_db_internal *ip, const char *attr);

/**
 * db object 'adjust' support, modify a tcl list representation
 */
RT_EXPORT extern int rt_obj_adjust(struct bu_vls *logstr, struct rt_db_internal *ip, int argc, const char **argv);

/**
 * describe an object in text form (used by the 'l' command)
 */
RT_EXPORT extern int rt_obj_describe(struct bu_vls *logstr, const struct rt_db_internal *ip, int verbose, double mm2local);

/**
 * create a 'default' object
 */
RT_EXPORT extern int rt_obj_make(const struct rt_functab *ftp, struct rt_db_internal *ip);

/**
 * apply a matrix transformation to an object (translation, rotation, scale)
 */
RT_EXPORT extern int rt_obj_xform(struct rt_db_internal *op, const mat_t mat, struct rt_db_internal *ip, int release, struct db_i *dbip);

/**
 * obtain parameters for an object in libpc form
 */
RT_EXPORT extern int rt_obj_params(struct pc_pc_set *ps, const struct rt_db_internal *ip);

/**
 * mirror an object about a plane
 */
RT_EXPORT extern int rt_obj_mirror(struct rt_db_internal *ip, const plane_t *plane);

/**
 * if `stp` is prepped, serialize; otherwise, deserialize from `external`
 */
RT_EXPORT extern int rt_obj_prep_serialize(struct soltab *stp, const struct rt_db_internal *ip, struct bu_external *external, size_t *version);

/**
 * Stopping-criteria parameters for rt_crofton_shoot() and rt_crofton_sample().
 *
 * All fields default to zero.  Behaviour when all three are zero (or the
 * pointer is NULL) is identical to the historical default: fire 2 000 rays
 * and stop when two successive 1 %-threshold iterations agree.
 *
 * When one or more non-zero fields are provided, sampling continues until
 * the FIRST criterion that is met:
 *
 *   n_rays > 0        Stop once this many rays have been fired in total.
 *
 *   stability_mm > 0  Stop once the estimate is "stable" to within the
 *                     given linear dimension.  Stability is measured as the
 *                     change in equivalent-sphere radius between successive
 *                     iterations: r_sa = sqrt(SA / (4*pi)) for surface area
 *                     and r_v = cbrt(3*V / (4*pi)) for volume.  Sampling
 *                     stops when both |Δr_sa| and |Δr_v| (for whichever
 *                     outputs are requested) are < stability_mm.
 *
 *   time_ms > 0       Stop once this many wall-clock milliseconds have
 *                     elapsed, returning the best estimate accumulated so far.
 *
 * When multiple fields are non-zero the first criterion to fire wins,
 * giving callers fine control over the accuracy / speed trade-off.
 */
struct rt_crofton_params {
    size_t n_rays;       /**< max total rays; 0 = no limit via this criterion */
    double stability_mm; /**< equivalent-radius stability target (mm); 0 = disabled */
    double time_ms;      /**< wall-clock time budget (ms); 0 = disabled */
};


/**
 * Run the Cauchy-Crofton ray-sampling estimator on an already-prepared
 * raytrace instance.  The caller owns @p rtip and must call rt_free_rti
 * after this function returns.
 *
 * @param rtip          Prepared raytrace instance (rt_prep_parallel must
 *                      have been called first).
 * @param params        Stopping criteria.  NULL or all-zero → 2 000-ray default.
 * @param out_surf_area Receives the estimated surface area (mm^2).
 * @param out_volume    Receives the estimated volume (mm^3).
 * @return  0 on success, -1 on bad arguments.
 */
RT_EXPORT extern int rt_crofton_shoot(struct rt_i *rtip, const struct rt_crofton_params *params, double *out_surf_area, double *out_volume);


/**
 * Cauchy-Crofton surface-area and/or volume estimator for a primitive.
 *
 * Creates a temporary in-memory raytrace of @p ip and fires random chord
 * rays until the stopping criteria in @p params are satisfied (or all
 * criteria are zero / params is NULL, in which case the 2 000-ray default
 * is used).  Stores the estimated surface area in @p *area (if non-NULL)
 * and the estimated volume in @p *vol (if non-NULL).
 *
 * This is the primary high-level entry point.  Primitives that require
 * controlled accuracy (e.g. TGC TEC case, triaxial ELL, EHY r1≠r2, HYP,
 * superell, ETO/TOR spindle) should call this function directly with
 * appropriate params.  Callers that already have a prepared rt_i should
 * call rt_crofton_shoot() instead.
 */
RT_EXPORT extern void rt_crofton_sample(fastf_t *area, fastf_t *vol,
					const struct rt_db_internal *ip,
					const struct rt_crofton_params *params);

__END_DECLS

#endif  /* RT_FUNC_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
