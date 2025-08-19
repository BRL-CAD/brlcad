/*                        B O T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "nmg.h"
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
    void *tie; /* FIXME: horrible blind cast, points to one in rt_bot_internal */

#ifdef USE_OPENCL
    struct clt_bot_specific clt_header;
    struct clt_tri_specific *clt_triangles;
    cl_double *clt_normals;
#endif
};


// this is really close to struct tri_specific in plane.h
// TODO: see if that ever gets serialized, and if it doesn't
// then change that to this - memory coherency win
// sizeof(triangle_s) should be 15/16 * (4/8) bytes
typedef struct _triangle_s {
    point_t A;
    vect_t AB;
    vect_t AC;
    vect_t face_norm;
    fastf_t face_norm_scalar;
    fastf_t *norms;
    size_t face_id;
} triangle_s;

// BoT specific editing info
struct rt_bot_edit {
    int bot_verts[3];           /* vertices for the BOT solid */
};

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

RT_EXPORT extern void rt_bot_internal_free(struct rt_bot_internal *bot);

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
RT_EXPORT extern int rt_bot_sort_faces(struct rt_bot_internal *bot,
				       size_t tris_per_piece);
RT_EXPORT extern int rt_bot_decimate(struct rt_bot_internal *bot,
				     fastf_t max_chord_error,
				     fastf_t max_normal_error,
				     fastf_t min_edge_length);
RT_EXPORT extern size_t rt_bot_decimate_gct(struct rt_bot_internal *bot, fastf_t feature_size);

/* Function to convert plate mode BoT to volumetric BoT */
RT_EXPORT extern int rt_bot_plate_to_vol(struct rt_bot_internal **obot, struct rt_bot_internal *bot, int round_outer_edges, int quiet_mode, fastf_t max_area_delta, fastf_t min_tri_threshold);



/* Container to hold various settings we may need to control the bot repair
 * process.  May also be updated in the future to contain diagnostic info
 * to report back to the caller - this struct is expected to change in
 * response to the evolving setting needs of the repair process as various
 * algorithms are explored.
 */
struct rt_bot_repair_info {
    fastf_t max_hole_area;
    fastf_t max_hole_area_percent;
    int strict;
};

/* For now the default upper hole size limit will be 5 percent of the mesh
 * area, but calling codes should not rely on that value to remain consistent
 * between versions.
 *
 * By default, don't return a mesh that can't pass the lint solid raytracing
 * tests.  This isn't always desirable - sometimes manifold is enough even if
 * the mesh is not otherwise well behaved - so it is an user settable param.
 */
#define RT_BOT_REPAIR_INFO_INIT {0.0, 5.0, 1};

/* Function to attempt repairing a non-manifold BoT.  Returns 1 if ibot was
 * already manifold (obot will contain NULL), 0 if a manifold BoT was created
 * (*obot will be the new manifold BoT) and -1 for other cases to indicate
 * error.
 */
RT_EXPORT extern int rt_bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *ibot, struct rt_bot_repair_info *i);

/* Test whether a bot is "inside-out".  This function is aware of
 * CCW vs CW BoT orientation settings, and will interpret the
 * results of the test accordingly.  The idea is for this function
 * to return "1" in the same situations that would result in an
 * OpenGL shaded drawing of the BoT showing black faces due to
 * incorrect orientation. */
RT_EXPORT extern int rt_bot_inside_out(struct rt_bot_internal *bot);

/* Test whether a solid BoT has faces that are <tol distance away when shooting
 * "into" the solid.  This is (at the moment) a ray interrogation based test,
 * so it is not absolutely guaranteed to find all near self-intersections.
 *
 * It is intended to catch situations such as boolean operations on meshes
 * producing exceedingly thin volumes.
 *
 * Returns 1 if a problem is found, else 0.  If ofaces is non-NULL, store the
 * indices of the specific faces found to be problematic. */
RT_EXPORT extern int rt_bot_thin_check(struct bu_ptbl *ofaces, struct rt_bot_internal *bot, struct rt_i *rtip, fastf_t tol, int verbose);

/* Function to remove a set of faces from a BoT and produce a new BoT */
RT_EXPORT struct rt_bot_internal *
rt_bot_remove_faces(struct bu_ptbl *rm_face_indices, const struct rt_bot_internal *obot);

/* Function to copy an rt_bot_internal structure */
RT_EXPORT struct rt_bot_internal *
rt_bot_dup(const struct rt_bot_internal *bot);

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
