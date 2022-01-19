/*                        P L O T . H
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
 * NMG plotting
 */
/** @{ */
/** @file nmg/plot.h */

#ifndef NMG_PLOT_H
#define NMG_PLOT_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/model.h"

__BEGIN_DECLS

NMG_EXPORT extern void nmg_pl_shell(FILE *fp,
				    const struct shell *s,
				    int fancy,
				    struct bu_list *vlfree);

NMG_EXPORT extern void nmg_vu_to_vlist(struct bu_list *vhead,
				       const struct vertexuse    *vu,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_eu_to_vlist(struct bu_list *vhead,
				       const struct bu_list      *eu_hd,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_lu_to_vlist(struct bu_list *vhead,
				       const struct loopuse      *lu,
				       int                       poly_markers,
				       const vectp_t             norm,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_snurb_fu_to_vlist(struct bu_list              *vhead,
					     const struct faceuse        *fu,
					     int                 poly_markers,
					     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_s_to_vlist(struct bu_list             *vhead,
				      const struct shell *s,
				      int                        poly_markers,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_r_to_vlist(struct bu_list             *vhead,
				      const struct nmgregion     *r,
				      int                        poly_markers,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_m_to_vlist(struct bu_list     *vhead,
				      struct model       *m,
				      int                poly_markers,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_offset_eu_vert(point_t                        base,
					  const struct edgeuse   *eu,
					  const vect_t           face_normal,
					  int                    tip);
/* plot */
NMG_EXPORT extern void nmg_pl_v(FILE     *fp,
				const struct vertex *v,
				long *b);
NMG_EXPORT extern void nmg_pl_e(FILE *fp,
				const struct edge *e,
				long *b,
				int red,
				int green,
				int blue);
NMG_EXPORT extern void nmg_pl_eu(FILE *fp,
				 const struct edgeuse *eu,
				 long *b,
				 int red,
				 int green,
				 int blue);
NMG_EXPORT extern void nmg_pl_lu(FILE *fp,
				 const struct loopuse *fu,
				 long *b,
				 int red,
				 int green,
				 int blue,
				 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_fu(FILE *fp,
				 const struct faceuse *fu,
				 long *b,
				 int red,
				 int green,
				 int blue,
				 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_s(FILE *fp,
				const struct shell *s,
				struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_r(FILE *fp,
				const struct nmgregion *r,
				struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_m(FILE *fp,
				const struct model *m,
				struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_v(struct bv_vlblock *vbp,
				     const struct vertex *v,
				     long *tab,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_e(struct bv_vlblock *vbp,
				     const struct edge *e,
				     long *tab,
				     int red,
				     int green,
				     int blue,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_eu(struct bv_vlblock *vbp,
				      const struct edgeuse *eu,
				      long *tab,
				      int red,
				      int green,
				      int blue,
				      int fancy,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_euleft(struct bu_list                 *vh,
					  const struct edgeuse           *eu,
					  const point_t                  center,
					  const mat_t                    mat,
					  const vect_t                   xvec,
					  const vect_t                   yvec,
					  double                         len,
					  struct bu_list 	        *vlfree,
					  const struct bn_tol            *tol);
NMG_EXPORT extern void nmg_vlblock_around_eu(struct bv_vlblock           *vbp,
					     const struct edgeuse        *arg_eu,
					     long                        *tab,
					     int                 fancy,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);
NMG_EXPORT extern void nmg_vlblock_lu(struct bv_vlblock *vbp,
				      const struct loopuse *lu,
				      long *tab,
				      int red,
				      int green,
				      int blue,
				      int fancy,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_fu(struct bv_vlblock *vbp,
				      const struct faceuse *fu,
				      long *tab, int fancy, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_s(struct bv_vlblock *vbp,
				     const struct shell *s,
				     int fancy,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_r(struct bv_vlblock *vbp,
				     const struct nmgregion *r,
				     int fancy,
				     struct bu_list *vlfree);
NMG_EXPORT extern void nmg_vlblock_m(struct bv_vlblock *vbp,
				     const struct model *m,
				     int fancy,
				     struct bu_list *vlfree);

/* visualization helper routines */
NMG_EXPORT extern void nmg_pl_edges_in_2_shells(struct bv_vlblock        *vbp,
						long                     *b,
						const struct edgeuse     *eu,
						int                      fancy,
						struct bu_list *vlfree,
						const struct bn_tol      *tol);
NMG_EXPORT extern void nmg_pl_isect(const char           *filename,
				    const struct shell   *s,
				    struct bu_list *vlfree,
				    const struct bn_tol  *tol);
NMG_EXPORT extern void nmg_pl_comb_fu(int num1,
				      int num2,
				      const struct faceuse *fu1,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_pl_2fu(const char *str,
				  const struct faceuse *fu1,
				  const struct faceuse *fu2,
				  int show_mates,
				  struct bu_list *vlfree);
/* graphical display of classifier results */
NMG_EXPORT extern void nmg_show_broken_classifier_stuff(uint32_t *p,
							char     **classlist,
							int      all_new,
							int      fancy,
							const char       *a_string,
							struct bu_list *vlfree);
NMG_EXPORT extern void nmg_face_plot(const struct faceuse *fu, struct bu_list *vlfree);
NMG_EXPORT extern void nmg_2face_plot(const struct faceuse *fu1,
				      const struct faceuse *fu2,
				      struct bu_list *vlfree);
NMG_EXPORT extern void nmg_face_lu_plot(const struct loopuse *lu,
					const struct vertexuse *vu1,
					const struct vertexuse *vu2,
					struct bu_list *vlfree);
NMG_EXPORT extern void nmg_plot_lu_ray(const struct loopuse              *lu,
				       const struct vertexuse            *vu1,
				       const struct vertexuse            *vu2,
				       const vect_t                      left,
				       struct bu_list *vlfree);
NMG_EXPORT extern void nmg_plot_ray_face(const char *fname,
					 point_t pt,
					 const vect_t dir,
					 const struct faceuse *fu,
					 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_plot_lu_around_eu(const char          *prefix,
					     const struct edgeuse        *eu,
					     struct bu_list *vlfree,
					     const struct bn_tol *tol);
NMG_EXPORT extern int nmg_snurb_to_vlist(struct bu_list          *vhead,
					 const struct face_g_snurb       *fg,
					 int                     n_interior,
					 struct bu_list *vlfree);
NMG_EXPORT extern void nmg_cnurb_to_vlist(struct bu_list *vhead,
					  const struct edgeuse *eu,
					  int n_interior,
					  int cmd,
					  struct bu_list *vlfree);


__END_DECLS

#endif  /* NMG_PLOT_H */
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
