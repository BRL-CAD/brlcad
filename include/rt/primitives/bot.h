/*                        B O T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @addtogroup rt_bot */
/** @{ */
/** @file rt/primitives/bot.h */

#ifndef RT_PRIMITIVES_BOT_H
#define RT_PRIMITIVES_BOT_H

#include "common.h"
#include "vmath.h"
#include "bn/tol.h"
#include "rt/geom.h"
#include "rt/defines.h"
#include "rt/tol.h"
#include "rt/view.h"
#include "rt/soltab.h"

__BEGIN_DECLS
#ifdef USE_OPENCL
/* largest data members first */
struct clt_bot_specific {
    cl_ulong offsets[5]; /* header, bvh, tris, norms. */
    cl_uint ntri;
    cl_uchar orientation;
    cl_uchar flags;
    cl_uchar pad[2];
};

struct clt_tri_specific {
    cl_double v0[3];
    cl_double v1[3];
    cl_double v2[3];
    cl_int surfno;
    cl_uchar pad[4];
};
#endif

/* Shared between bot and ars at the moment */
struct bot_specific {
    unsigned char bot_mode;
    unsigned char bot_orientation;
    unsigned char bot_flags;
    size_t bot_ntri;
    fastf_t *bot_thickness;
    struct bu_bitv *bot_facemode;
    void *bot_facelist; /* head of linked list */
    void **bot_facearray;       /* head of face array */
    size_t bot_tri_per_piece;   /* log # tri per piece. 1 << bot_ltpp is tri per piece */
    void *tie; /* FIXME: horrible blind cast, points to one in rt_bot_internal */

#ifdef USE_OPENCL
    struct clt_bot_specific clt_header;
    struct clt_tri_specific *clt_triangles;
    cl_double *clt_normals;
#endif
};

RT_EXPORT extern void rt_bot_prep_pieces(struct bot_specific    *bot,
					 struct soltab          *stp,
					 size_t                 ntri,
					 const struct bn_tol    *tol);

RT_EXPORT extern size_t rt_botface(struct soltab                *stp,
				   struct bot_specific  *bot,
				   fastf_t                      *ap,
				   fastf_t                      *bp,
				   fastf_t                      *cp,
				   size_t                       face_no,
				   const struct bn_tol  *tol);


/* bot.c */
RT_EXPORT extern size_t rt_bot_get_edge_list(const struct rt_bot_internal *bot,
					     size_t **edge_list);
RT_EXPORT extern int rt_bot_edge_in_list(const size_t v1,
					 const size_t v2,
					 const size_t edge_list[],
					 const size_t edge_count0);
RT_EXPORT extern int rt_bot_plot(struct bu_list         *vhead,
				 struct rt_db_internal  *ip,
				 const struct bg_tess_tol *ttol,
				 const struct bn_tol    *tol,
				 const struct bview *info);
RT_EXPORT extern int rt_bot_plot_poly(struct bu_list            *vhead,
				      struct rt_db_internal     *ip,
				      const struct bg_tess_tol *ttol,
				      const struct bn_tol       *tol);
RT_EXPORT extern int rt_bot_find_v_nearest_pt2(const struct rt_bot_internal *bot,
					       const point_t    pt2,
					       const mat_t      mat);
RT_EXPORT extern int rt_bot_find_e_nearest_pt2(int *vert1, int *vert2, const struct rt_bot_internal *bot, const point_t pt2, const mat_t mat);
RT_EXPORT extern fastf_t rt_bot_propget(struct rt_bot_internal *bot,
					const char *property);
RT_EXPORT extern int rt_bot_vertex_fuse(struct rt_bot_internal *bot,
					const struct bn_tol *tol);
RT_EXPORT extern int rt_bot_face_fuse(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_condense(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_smooth(struct rt_bot_internal *bot,
				   const char *bot_name,
				   struct db_i *dbip,
				   fastf_t normal_tolerance_angle);
RT_EXPORT extern int rt_bot_flip(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_sync(struct rt_bot_internal *bot);
RT_EXPORT extern struct rt_bot_list * rt_bot_split(struct rt_bot_internal *bot);
RT_EXPORT extern struct rt_bot_list * rt_bot_patches(struct rt_bot_internal *bot);
RT_EXPORT extern void rt_bot_list_free(struct rt_bot_list *headRblp,
				       int fbflag);

RT_EXPORT extern int rt_bot_same_orientation(const int *a,
					     const int *b);

RT_EXPORT extern int rt_bot_tess(struct nmgregion **r,
				 struct model *m,
				 struct rt_db_internal *ip,
				 const struct bg_tess_tol *ttol,
				 const struct bn_tol *tol);

RT_EXPORT extern struct rt_bot_internal * rt_bot_merge(size_t num_bots, const struct rt_bot_internal * const *bots);


/* defined in bot.c */
/* TODO - these global variables need to be rolled in to the rt_i structure */
RT_EXPORT extern size_t rt_bot_minpieces;
RT_EXPORT extern size_t rt_bot_tri_per_piece;
RT_EXPORT extern int rt_bot_sort_faces(struct rt_bot_internal *bot,
				       size_t tris_per_piece);
RT_EXPORT extern int rt_bot_decimate(struct rt_bot_internal *bot,
				     fastf_t max_chord_error,
				     fastf_t max_normal_error,
				     fastf_t min_edge_length);
RT_EXPORT extern size_t rt_bot_decimate_gct(struct rt_bot_internal *bot, fastf_t feature_size);


/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_BOT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
