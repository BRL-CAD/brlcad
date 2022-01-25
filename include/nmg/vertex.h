/*                       V E R T E X . H
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
/** @addtogroup nmg_vertex */
/** @{ */
/** @file nmg/vertex.h */

#ifndef NMG_VERTEX_H
#define NMG_VERTEX_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_VERTEX(_p)             NMG_CKMAG(_p, NMG_VERTEX_MAGIC, "vertex")
#define NMG_CK_VERTEX_G(_p)           NMG_CKMAG(_p, NMG_VERTEX_G_MAGIC, "vertex_g")
#define NMG_CK_VERTEXUSE(_p)          NMG_CKMAG(_p, NMG_VERTEXUSE_MAGIC, "vertexuse")
#define NMG_CK_VERTEXUSE_A_PLANE(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, "vertexuse_a_plane")

#define GET_VERTEX(p, m)            {NMG_GETSTRUCT(p, vertex); NMG_INCR_INDEX(p, m);}
#define GET_VERTEX_G(p, m)          {NMG_GETSTRUCT(p, vertex_g); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE(p, m)         {NMG_GETSTRUCT(p, vertexuse); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_PLANE(p, m) {NMG_GETSTRUCT(p, vertexuse_a_plane); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_CNURB(p, m) {NMG_GETSTRUCT(p, vertexuse_a_cnurb); NMG_INCR_INDEX(p, m);}
#define FREE_VERTEX(p)            NMG_FREESTRUCT(p, vertex)
#define FREE_VERTEX_G(p)          NMG_FREESTRUCT(p, vertex_g)
#define FREE_VERTEXUSE(p)         NMG_FREESTRUCT(p, vertexuse)
#define FREE_VERTEXUSE_A_PLANE(p) NMG_FREESTRUCT(p, vertexuse_a_plane)
#define FREE_VERTEXUSE_A_CNURB(p) NMG_FREESTRUCT(p, vertexuse_a_cnurb)

/**
 * @brief Given a vertex \b v, set the coordinates of the vertex geometry
 * associated with \b v to \b pt
 *
 * If a vertex has no associated vertex_g structure, one is created, associated
 * with v and added to v's parent nmg model.
 */
NMG_EXPORT extern void nmg_vertex_gv(struct vertex *v,
                                     const point_t pt);

/**
 * @brief Given a vertex \b v, set the coordinates of the vertex geometry
 * associated with \b v to (\b x,\b y,\b z)
 *
 * Works like nmg_vertex_gv, except it doesn't require the data to be in a
 * point_t container.  (Mostly useful for quick and dirty programs.)
 */
NMG_EXPORT extern void nmg_vertex_g(struct vertex *v,
                                    fastf_t x,
                                    fastf_t y,
                                    fastf_t z);
/**
 * @brief Given a vertex use \b vu, associate a normal vector \b norm with that use
 *
 * If the vertexuse does not already have a vertexuse_a_plane attribute associated
 * with it, this routine will add one.
 *
 * Remember that only vertexuse instances may have an associated normal -
 * because a vertex may be used multiple times in different faces with
 * different normals, we cannot reliably associate a normal uniquely with a
 * vertex definition.  Only when the vertex is used in some particular
 * application (i.e. a vertexuse as part of a face) can we associate a normal.
 */
NMG_EXPORT extern void nmg_vertexuse_nv(struct vertexuse *vu,
                                        const vect_t norm);

/**
 * @brief Given a vertex use \b vu, change its vertex association from it's
 * current vertex to \b v
 *
 * This has the effect of "relocating" the vertexuse to the position associated
 * with \b v.
 *
 * If the vertex previously used by \b vu has no more associated vertexuse
 * structures (i.e. the vertex is no longer referenced by the NMG model), this
 * routine will free the old vertex structure.
 */
NMG_EXPORT extern void nmg_movevu(struct vertexuse *vu,
                                  struct vertex *v);

/**
 * @brief Kill vertexuse \b vu, and null out parent's vu_p.
 *
 * This routine is not intended for general use by applications, because it
 * requires cooperation on the part of the caller to properly dispose of or fix
 * the now *quite* illegal parent.  (Illegal because the parent's vu_p is
 * NULL).  It exists primarily as a support routine for "mopping up" after
 * nmg_klu(), nmg_keu(), nmg_ks(), and nmg_mv_vu_between_shells().
 *
 * It is also used in a particularly ugly way in nmg_cut_loop() and
 * nmg_split_lu_at_vu() as part of their method for obtaining an "empty"
 * loopuse/loop set.
 *
 * It is worth noting that all these callers ignore the return code, because
 * they *all* exist to intentionally empty out the parent, but the return code
 * is provided anyway, in the name of [CTJ] symmetry.
 *
 * @retval 0 If all is well in the parent
 * @retval 1 If parent is empty, and is thus "illegal"
 */
NMG_EXPORT extern int nmg_kvu(struct vertexuse *vu);

/**
 * @brief Join two vertexes into one.
 *
 * \b v1 inherits all the vertexuses presently pointing to \b v2, and \b v2 is
 * then destroyed.
 *
 * Note that this is not a "joining" in the geometric sense; the position of v1
 * is not adjusted in any way to make it closer to that of v2.  The merge is
 * strictly topological, and all vertexuses that referenced v2 will now have a
 * new geometric position, if the x,y,z coordinates of v1 differed from those
 * of v2.
 */
NMG_EXPORT extern void nmg_jv(struct vertex *v1,
                              struct vertex *v2);

/**
 * @brief Build the set of pointers to all vertex structures in an NMG model
 * that are "below" the data structure pointed to by magic_p, where magic_p is
 * a pointer to the magic entry of any NMG data structure in the model.
 *
 * For "raw" geometric struts, the magic entry will be the first entry in the
 * struct - for example, a loop pointed to by l would have a magic_p key of
 * &l->magic.  For the use structures, the magic key is found within the leading
 * bu_list - for example, a faceuse pointed to by *fu would have a magic_p key
 * at &fu->l.magic
 *
 * Each vertex pointer will be listed exactly once - i.e. uniqueness within
 * the table may be assumed.
 *
 * This set provides the caller with an easy way to answer the question "which
 * vertices are used by this part of the model".
 *
 * @param[out] tab a bu_ptbl holding struct vertex pointers.
 *
 * @param magic_p pointer to an NMG data structure's magic entry.
 *
 * @param vlfree list of available vlist segments to be reused by debug drawing routines.
 */
NMG_EXPORT extern void nmg_vertex_tabulate(struct bu_ptbl *tab,
                                           const uint32_t *magic_p,
                                           struct bu_list *vlfree);

/**
 * @brief Build the set of pointers to all vertexuse normal structures in an
 * NMG model that are "below" the data structure pointed to by magic_p, where
 * magic_p is a pointer to the magic entry of any NMG data structure in the
 * model.
 *
 * The return type for vertexuse normals in the output table is struct
 * vertexuse_a_plane *
 *
 * For "raw" geometric struts, the magic entry will be the first entry in the
 * struct - for example, a loop pointed to by l would have a magic_p key of
 * &l->magic.  For the use structures, the magic key is found within the leading
 * bu_list - for example, a faceuse pointed to by *fu would have a magic_p key
 * at &fu->l.magic
 *
 * Each vertexuse_a_plane pointer will be listed exactly once - i.e. uniqueness
 * within the table may be assumed.
 *
 * @param[out] tab a bu_ptbl holding struct vertexuse_plane_a pointers.
 *
 * @param magic_p pointer to an NMG data structure's magic entry.
 *
 * @param vlfree list of available vlist segments to be reused by debug drawing routines.
 */
NMG_EXPORT extern void nmg_vertexuse_normal_tabulate(struct bu_ptbl *tab,
                                                     const uint32_t *magic_p,
                                                     struct bu_list *vlfree);

/**
 * @brief Check if the given vertexuse \b vu is in the table given by \b b or if \b vu
 * references a vertex which is referenced by a vertexuse in the table
 *
 * @retval 1 Match found
 * @retval 0 No match found
 */
NMG_EXPORT extern int nmg_in_or_ref(struct vertexuse *vu,
                                    struct bu_ptbl *b);


/**
 * @brief Given a vertex and a list of faces (not more than three) that should
 * intersect at the vertex, calculate a new location for the vertex and modify
 * the vertex_g geometry associated with the vertex to the new location.
 *
 * @retval  0 Success
 * @retval  1 Failure
 */
NMG_EXPORT extern int nmg_simple_vertex_solve(struct vertex *new_v,
                                              const struct bu_ptbl *faces,
                                              const struct bn_tol *tol);

/**
 * @brief Move vertex so it is at the intersection of the newly created faces.
 *
 * This routine is used by "nmg_extrude_shell" to move vertices. Each plane has
 * already been moved a distance inward and the surface normals have been
 * reversed.
 *
 * If approximate is non-zero, then the coordinates of the new vertex may be
 * calculated as the point with minimum distance to all the faces that
 * intersect at the vertex for vertices where more than three faces intersect.
 *
 * @retval  0 Success
 * @retval  1 Failure
 */
NMG_EXPORT extern int nmg_in_vert(struct vertex *new_v,
                                  const int approximate,
                                  struct bu_list *vlfree,
                                  const struct bn_tol *tol);

/**
 * @brief Fuse together any vertices that are geometrically identical, within
 * distance tolerance.
 *
 * This function may be passed a pointer to an NMG object's magic entry or a
 * pointer to a bu_ptbl structure containing a list of pointers to NMG vertex
 * structures.
 *
 * For "raw" geometric struts, the magic entry will be the first entry in the
 * struct - for example, a loop pointed to by l would have a magic_p key of
 * &l->magic.  For the use structures, the magic key is found within the leading
 * bu_list - for example, a faceuse pointed to by *fu would have a magic_p key
 * at &fu->l.magic
 *
 * If a pointer to a bu_ptbl structure was passed into this function, the
 * calling function is responsible for freeing the table.
 *
 * @param magic_p pointer to either an NMG data structure's magic entry or to a bu_ptbl.
 *
 * @param vlfree list of available vlist segments to be reused by debug drawing routines.
 *
 * @param tol distance tolerances to use when fusing vertices.
 */
NMG_EXPORT extern int nmg_vertex_fuse(const uint32_t *magic_p, struct bu_list *vlfree,
                                      const struct bn_tol *tol);


__END_DECLS

#endif  /* NMG_VERTEX_H */
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
