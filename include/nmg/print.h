/*                        P R I N T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup nmg
 * @brief
 * NMG printing
 */
/** @{ */
/** @file nmg/print.h */

#ifndef NMG_PRINT_H
#define NMG_PRINT_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/model.h"

__BEGIN_DECLS

/** Print a plane equation. */
#define PLPRINT(_s, _pl) bu_log("%s %gx + %gy + %gz = %g\n", (_s), \
                                (_pl)[0], (_pl)[1], (_pl)[2], (_pl)[3])

NMG_EXPORT extern char *nmg_orientation(int orientation);
NMG_EXPORT extern void nmg_pr_orient(int orientation,
				     const char *h);
NMG_EXPORT extern void nmg_pr_m(const struct model *m);
NMG_EXPORT extern void nmg_pr_r(const struct nmgregion *r,
				char *h);
NMG_EXPORT extern void nmg_pr_sa(const struct shell_a *sa,
				 char *h);
NMG_EXPORT extern void nmg_pr_lg(const struct loop_g *lg,
				 char *h);
NMG_EXPORT extern void nmg_pr_fg(const uint32_t *magic,
				 char *h);
NMG_EXPORT extern void nmg_pr_s(const struct shell *s,
				char *h);
NMG_EXPORT extern void  nmg_pr_s_briefly(const struct shell *s,
					 char *h);
NMG_EXPORT extern void nmg_pr_f(const struct face *f,
				char *h);
NMG_EXPORT extern void nmg_pr_fu(const struct faceuse *fu,
				 char *h);
NMG_EXPORT extern void nmg_pr_fu_briefly(const struct faceuse *fu,
					 char *h);
NMG_EXPORT extern void nmg_pr_l(const struct loop *l,
				char *h);
NMG_EXPORT extern void nmg_pr_lu(const struct loopuse *lu,
				 char *h);
NMG_EXPORT extern void nmg_pr_lu_briefly(const struct loopuse *lu,
					 char *h);
NMG_EXPORT extern void nmg_pr_eg(const uint32_t *eg,
				 char *h);
NMG_EXPORT extern void nmg_pr_e(const struct edge *e,
				char *h);
NMG_EXPORT extern void nmg_pr_eu(const struct edgeuse *eu,
				 char *h);
NMG_EXPORT extern void nmg_pr_eu_briefly(const struct edgeuse *eu,
					 char *h);
NMG_EXPORT extern void nmg_pr_eu_endpoints(const struct edgeuse *eu,
					   char *h);
NMG_EXPORT extern void nmg_pr_vg(const struct vertex_g *vg,
				 char *h);
NMG_EXPORT extern void nmg_pr_v(const struct vertex *v,
				char *h);
NMG_EXPORT extern void nmg_pr_vu(const struct vertexuse *vu,
				 char *h);
NMG_EXPORT extern void nmg_pr_vu_briefly(const struct vertexuse *vu,
					 char *h);
NMG_EXPORT extern void nmg_pr_vua(const uint32_t *magic_p,
				  char *h);
NMG_EXPORT extern void nmg_euprint(const char *str,
				   const struct edgeuse *eu);
NMG_EXPORT extern void nmg_pr_ptbl(const char *title,
				   const struct bu_ptbl *tbl,
				   int verbose);
NMG_EXPORT extern void nmg_pr_ptbl_vert_list(const char *str,
					     const struct bu_ptbl *tbl,
					     const fastf_t *mag);
NMG_EXPORT extern void nmg_pr_one_eu_vecs(const struct edgeuse *eu,
					  const vect_t xvec,
					  const vect_t yvec,
					  const vect_t zvec,
					  const struct bn_tol *tol);
NMG_EXPORT extern void nmg_pr_fu_around_eu_vecs(const struct edgeuse *eu,
						const vect_t xvec,
						const vect_t yvec,
						const vect_t zvec,
						const struct bn_tol *tol);
NMG_EXPORT extern void nmg_pr_fu_around_eu(const struct edgeuse *eu,
					   const struct bn_tol *tol);
NMG_EXPORT extern void nmg_pl_lu_around_eu(const struct edgeuse *eu, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pr_fus_in_fg(const uint32_t *fg_magic);

/* From nmg_rt_isect.c */
NMG_EXPORT extern const char * nmg_rt_inout_str(int code);

NMG_EXPORT extern void nmg_rt_print_hitlist(struct bu_list *hd);

NMG_EXPORT extern void nmg_rt_print_hitmiss(struct nmg_hitmiss *a_hit);



__END_DECLS

#endif  /* NMG_PRINT_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
