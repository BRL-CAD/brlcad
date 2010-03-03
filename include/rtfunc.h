/*                        R T F U N C . H
 * BRL-CAD
 *
 * Copyright (c) 2010 United States Government as represented by
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
/** @addtogroup rt */
/** @{ */
/** @file rtfunc.h
 *
 * @brief Primitive manipulation functions from former functab
 * callback table.
 *
 * As this is a relatively new set of interfaces, consider these
 * functions preliminary (i.e. DEPRECATED) and subject to change until
 * this message goes away.
 *
 */

/**
 *
 */
RT_EXPORT extern int rt_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip);

/**
 *
 */
RT_EXPORT extern int rt_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead);

/**
 *
 */
RT_EXPORT extern int rt_piece_shot(struct rt_piecestate *psp, struct rt_piecelist *plp, double dist_corr, struct xray *rp, struct application *ap, struct seg *seghead);

/**
 *
 */
RT_EXPORT extern void rt_piece_hitsegs(struct rt_piecestate *psp, struct seg *seghead, struct application *ap);

/**
 *
 */
RT_EXPORT extern void rt_print(const struct soltab *stp);

/**
 *
 */
RT_EXPORT extern void rt_norm(struct hit *hitp, struct soltab *stp, struct xray *rp);

/**
 *
 */
RT_EXPORT extern void rt_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp);

/**
 *
 */
RT_EXPORT extern void rt_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp);

/**
 *
 */
RT_EXPORT extern int rt_class();

/**
 *
 */
RT_EXPORT extern void rt_free(struct soltab *stp);

/**
 *
 */
RT_EXPORT extern int rt_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);

/**
 *
 */
RT_EXPORT extern void rt_vshot(struct soltab *stp[], struct xray *rp[], struct seg segp[], int n, struct application *ap);

/**
 *
 */
RT_EXPORT extern int rt_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);

/**
 *
 */
RT_EXPORT extern int rt_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol);

/**
 *
 */
RT_EXPORT extern int rt_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);

/**
 *
 */
RT_EXPORT extern int rt_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp);

/**
 *
 */
RT_EXPORT extern int rt_import4(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);

/**
 *
 */
RT_EXPORT extern int rt_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp);

/**
 *
 */
RT_EXPORT extern void rt_ifree(struct rt_db_internal *ip);

/**
 *
 */
RT_EXPORT extern int rt_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr);

/**
 *
 */
RT_EXPORT extern int rt_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, char **argv);

/**
 *
 */
RT_EXPORT extern int rt_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local, struct resource *resp, struct db_i *db_i);

/**
 *
 */
RT_EXPORT extern void rt_make(const struct rt_functab *ftp, struct rt_db_internal *intern);

/**
 *
 */
RT_EXPORT extern int rt_xform(struct rt_db_internal *op, const mat_t mat, struct rt_db_internal *ip, int release, struct db_i *dbip, struct resource *resp);

/**
 *
 */
RT_EXPORT extern int rt_params(struct pc_pc_set *ps, const struct rt_db_internal *ip);

/**
 *
 */
RT_EXPORT extern int rt_mirror(struct rt_db_internal *ip, const plane_t *plane);

#endif  /* __RTFUNC_H__ */
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
