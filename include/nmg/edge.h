/*                       E D G E . H
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
/** @addtogroup nmg_edge */
/** @{ */
/** @file nmg/edge.h */

#ifndef NMG_EDGE_H
#define NMG_EDGE_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_EDGE(_p)               NMG_CKMAG(_p, NMG_EDGE_MAGIC, "edge")
#define NMG_CK_EDGE_G_LSEG(_p)        NMG_CKMAG(_p, NMG_EDGE_G_LSEG_MAGIC, "edge_g_lseg")
#define NMG_CK_EDGEUSE(_p)            NMG_CKMAG(_p, NMG_EDGEUSE_MAGIC, "edgeuse")

#define GET_EDGE(p, m)              {NMG_GETSTRUCT(p, edge); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_LSEG(p, m)       {NMG_GETSTRUCT(p, edge_g_lseg); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_CNURB(p, m)      {NMG_GETSTRUCT(p, edge_g_cnurb); NMG_INCR_INDEX(p, m);}
#define GET_EDGEUSE(p, m)           {NMG_GETSTRUCT(p, edgeuse); NMG_INCR_INDEX(p, m);}

#define FREE_EDGE(p)              NMG_FREESTRUCT(p, edge)
#define FREE_EDGE_G_LSEG(p)       NMG_FREESTRUCT(p, edge_g_lseg)
#define FREE_EDGE_G_CNURB(p)      NMG_FREESTRUCT(p, edge_g_cnurb)
#define FREE_EDGEUSE(p)           NMG_FREESTRUCT(p, edgeuse)

/**
 * @brief Compute the equation of the line formed by the endpoints of the
 * edge.
 */
NMG_EXPORT extern void nmg_edge_g(struct edgeuse *eu);

/**
 * @brief Associate edgeuse 'eu' with the edge geometry structure identified by
 * 'magic_p', where magic_p is the pointer to the magic entry of an edge_g_lseg.
 *
 * Example magic_p keys:  eu->g.magic_p  &eg->l.magic
 *
 * If the edgeuse is already associated with some geometry, that geometry is
 * first released.  Note that, to start with, the two edgeuses may be using
 * different original geometries.
 *
 * Also do the edgeuse mate.
 *
 * @retval 0 If the old edge geometry (eu->g.magic_p) has other uses.
 * @retval 1 If the old edge geometry has been destroyed. Caller beware!
 */
NMG_EXPORT extern int nmg_use_edge_g(struct edgeuse *eu,
                                     uint32_t *magic_p);

/**
 * @brief Demote a wire edge into a pair of self-loop vertices
 *
 * @retval 0 If all is well
 * @retval 1 If shell is empty, and is thus "illegal".
 */
NMG_EXPORT extern int nmg_demote_eu(struct edgeuse *eu);

/**
 * @brief Move a pair of edgeuses onto a single edge (glue edgeuse).
 *
 * The edgeuse eusrc and its mate are moved to the edge used by eudst.  eusrc
 * is made to be immediately radial to eudst.  If eusrc does not share the same
 * vertices as eudst, we bomb.
 *
 * The edgeuse geometry pointers are not changed by this operation.
 *
 * This routine was formerly called nmg_moveeu().
 */
NMG_EXPORT extern void nmg_je(struct edgeuse *eudst,
                              struct edgeuse *eusrc);

/**
 * @brief If edgeuse is part of a shared edge (more than one pair of edgeuses
 * on the edge), it and its mate are "unglued" from the edge, and associated
 * with a new edge structure.
 *
 * Primarily a support routine for nmg_eusplit()
 *
 * If the original edge had edge geometry, that is *not* duplicated here,
 * because it is not easy (or appropriate) for nmg_eusplit() to know whether
 * the new vertex lies on the previous edge geometry or not.  Hence having the
 * nmg_ebreak() interface, which preserves the edge geometry across a split, and
 * nmg_esplit() which does not.
 */
NMG_EXPORT extern void nmg_unglueedge(struct edgeuse *eu);

/**
 * @brief Join two edge geometries.
 *
 * For all edges in the model which refer to 'src_eg', change them to
 * refer to 'dest_eg'.  The source is destroyed.
 *
 * It is the responsibility of the caller to make certain that the
 * 'dest_eg' is the best one for these edges.  Outrageously wrong
 * requests will cause this routine to abort.
 *
 * This algorithm does not make sense if dest_eg is an edge_g_cnurb;
 * those only make sense in the parameter space of their associated
 * face.
 */
NMG_EXPORT extern void nmg_jeg(struct edge_g_lseg *dest_eg,
                               struct edge_g_lseg *src_eg);

/**
 * @brief Make wire edge.
 *
 * Make a new edge between a pair of vertices in a shell.
 *
 * A new vertex will be made for any NULL vertex pointer parameters.
 * If we need to make a new vertex and the shell still has its
 * vertexuse we re-use that vertex rather than freeing and
 * re-allocating.
 *
 * If both vertices were specified, and the shell also had a vertexuse
 * pointer, the vertexuse in the shell is killed.  XXX Why?
 *
 * The explicit result is an edgeuse in shell "s" whose vertexuse refers to
 * vertex v1.  The edgeuse mate's vertexuse refers to vertex v2
 *
 * Additional possible side effects:
 * -# If the shell had a lone vertex in vu_p, it is destroyed, even if both
 *    vertices were specified.
 * -# The returned edgeuse is the first item on the shell's eu_hd list,
 *    followed immediately by the mate.
 */
NMG_EXPORT extern struct edgeuse *nmg_me(struct vertex *v1,
                                         struct vertex *v2,
                                         struct shell *s);
#define nmg_mev(_v, _u) nmg_me((_v), (struct vertex *)NULL, (_u))

/**
 * @brief Make an edge on vertexuse.
 *
 * The new edge runs from and to that vertex.
 *
 * If the vertexuse was the shell's sole vertexuse, then the new edge
 * is a wire edge in the shell's eu_hd list.
 *
 * If the vertexuse was part of a loop-of-a-single-vertex, either as a
 * loop in a face or as a wire-loop in the shell, the loop becomes a
 * regular loop with one edge that runs from and to the original
 * vertex.
 */
NMG_EXPORT extern struct edgeuse *nmg_meonvu(struct vertexuse *vu);

/**
 * @brief Delete an edgeuse & its mate from a shell or loop.
 *
 * @retval 0 If all is well
 * @retval 1 If the parent now has no edgeuses, and is thus "illegal" and in
 * need of being deleted.  (The lu / shell deletion can't be handled at this
 * level, and must be done by the caller).
 */
NMG_EXPORT extern int nmg_keu(struct edgeuse *eu);

/**
 * Do two edgeuses share the same two vertices? If yes, eu's should be
 * joined.
 */
#define NMG_ARE_EUS_ADJACENT(_eu1, _eu2) (\
        ((_eu1)->vu_p->v_p == (_eu2)->vu_p->v_p && \
         (_eu1)->eumate_p->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p)  || \
        ((_eu1)->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p && \
         (_eu1)->eumate_p->vu_p->v_p == (_eu2)->vu_p->v_p))

/** Compat: Used in nmg_misc.c and nmg_mod.c */
#define EDGESADJ(_e1, _e2) NMG_ARE_EUS_ADJACENT(_e1, _e2)

/**
 * @brief If shell s2 has an edge that connects the same vertices as eu1 connects,
 * return the matching edgeuse in s2.
 *
 * This routine works properly regardless of whether eu1 is in s2 or not.
 * A convenient wrapper for nmg_findeu().
 */
NMG_EXPORT extern struct edgeuse *nmg_find_matching_eu_in_s(const struct edgeuse *eu1,
                                                            const struct shell   *s2);

/**
 * @brief Find an edgeuse in a shell between a given pair of vertex structs.
 *
 * If a given shell "s" is specified, then only edgeuses in that shell
 * will be considered, otherwise all edgeuses in the model are fair game.
 *
 * If a particular edgeuse "eup" is specified, then that edgeuse
 * and its mate will not be returned as a match.
 *
 * If "dangling_only" is true, then an edgeuse will be matched only if
 * there are no other edgeuses on the edge, i.e. the radial edgeuse is
 * the same as the mate edgeuse.
 *
 * @retval edgeuse* Edgeuse which matches the criteria
 * @retval NULL Unable to find matching edgeuse
 */
NMG_EXPORT extern struct edgeuse *nmg_findeu(const struct vertex *v1,
                                             const struct vertex *v2,
                                             const struct shell *s,
                                             const struct edgeuse *eup,
                                             int dangling_only);

/**
 * @brief An analog to nmg_findeu(), only restricted to searching a faceuse,
 * rather than to a whole shell.
 */
NMG_EXPORT extern struct edgeuse *nmg_find_eu_in_face(const struct vertex *v1,
                                                      const struct vertex *v2,
                                                      const struct faceuse *fu,
                                                      const struct edgeuse *eup,
                                                      int dangling_only);

/**
 * @brief Find an edge between a given pair of vertices.
 *
 * If a given shell "s" is specified, then only edges in that shell
 * will be considered, otherwise all edges in the model are fair game.
 *
 * If a particular edge "ep" is specified, then that edge
 * will not be returned as a match.
 *
 * @retval edgeuse* Edgeuse of an edge which matches the criteria
 * @retval NULL Unable to find matching edge
 */
NMG_EXPORT extern struct edgeuse *nmg_find_e(const struct vertex *v1,
                                             const struct vertex *v2,
                                             const struct shell *s,
                                             const struct edge *ep);

/**
 * @brief Return a pointer to the edgeuse which is the parent of this vertexuse.
 *
 * A simple helper routine, which replaces the amazingly bad sequence of:
 * nmg_find_eu_with_vu_in_lu(nmg_find_lu_of_vu(vu), vu)
 * that was being used in several places.
 */
NMG_EXPORT extern struct edgeuse *nmg_find_eu_of_vu(const struct vertexuse *vu);

/**
 * @brief Find an edgeuse starting at a given vertexuse within a loopuse.
 */
NMG_EXPORT extern struct edgeuse *nmg_find_eu_with_vu_in_lu(const struct loopuse *lu,
                                                            const struct vertexuse *vu);

/**
 * @brief Looking radially around an edge, find another edge in the same
 * face as the current edge. (this could be the mate to the current edge)
 */
NMG_EXPORT extern const struct edgeuse *nmg_faceradial(const struct edgeuse *eu);

/**
 * @brief Looking radially around an edge, find another edge which is a part
 * of a face in the same shell.
 */
NMG_EXPORT extern const struct edgeuse *nmg_radial_face_edge_in_shell(const struct edgeuse *eu);

/**
 * @brief Perform a topology search to determine if two faces (specified by
 * their faceuses) share an edge in common.  If so, return an edgeuse
 * in fu1 of that edge.
 *
 * If there are multiple edgeuses in common, ensure that they all refer
 * to the same edge_g_lseg geometry structure.  The intersection of two planes
 * (non-coplanar) must be a single line.
 *
 * Calling this routine when the two faces share face geometry
 * and have more than one edge in common gives
 * a NULL return, as there is no unique answer.
 *
 * NULL is also returned if no common edge could be found.
 */
NMG_EXPORT extern const struct edgeuse *nmg_find_edge_between_2fu(const struct faceuse *fu1,
                                                                  const struct faceuse *fu2,
                                                                  struct bu_list *vlfree,
                                                                  const struct bn_tol *tol);

/**
 * @brief A geometric search routine to find the edge that is nearest to
 * the given point, when all edges are projected into 2D using
 * the matrix 'mat'.
 * Useful for finding the edge nearest a mouse click, for example.
 */
NMG_EXPORT extern struct edge *nmg_find_e_nearest_pt2(uint32_t *magic_p,
                                                      const point_t pt2,
                                                      const mat_t mat,
                                                      struct bu_list *vlfree,
                                                      const struct bn_tol *tol);

/**
 * @brief Given an edgeuse, return two arbitrary unit-length vectors which
 * are perpendicular to each other and to the edgeuse, such that
 * they can be considered the +X and +Y axis, and the edgeuse is +Z.
 * That is, X cross Y = Z.
 *
 * Useful for erecting a coordinate system around an edge suitable
 * for measuring the angles of other edges and faces with.
 */
NMG_EXPORT extern void nmg_eu_2vecs_perp(vect_t xvec,
                                         vect_t yvec,
                                         vect_t zvec,
                                         const struct edgeuse *eu,
                                         const struct bn_tol *tol);

/**
 * @brief Given an edgeuse, if it is part of a faceuse, return the inward pointing
 * "left" vector which points into the interior of this loop, and
 * lies in the plane of the face. The left vector is unitized.
 *
 * This routine depends on the vertex ordering in an OT_SAME loopuse being
 * properly CCW for exterior loops, and CW for interior (hole) loops.
 *
 * @retval -1 if edgeuse is not part of a faceuse.
 * @retval 0 if left vector successfully computed into caller's array.
 */
NMG_EXPORT extern int nmg_find_eu_leftvec(vect_t left,
                                          const struct edgeuse *eu);

/**
 * @brief Given an edgeuse, if it is part of a faceuse, return the inward
 * pointing "left" vector which points into the interior of this loop, and
 * lies in the plane of the face. The left vector is not unitized.
 *
 * This routine depends on the vertex ordering in an OT_SAME loopuse being
 * properly CCW for exterior loops, and CW for interior (hole) loops.
 *
 * @retval -1 if edgeuse is not part of a faceuse.
 * @retval 0 if left vector successfully computed into caller's array.
 */
NMG_EXPORT extern int nmg_find_eu_left_non_unit(vect_t left,
                                                const struct edgeuse     *eu);

/**
 * @brief If there is an edgeuse of an OT_SAME faceuse on this edge, return it.
 * Only return a wire edgeuse if that is all there is.
 *
 * Useful for selecting a "good" edgeuse to pass to nmg_eu_2vecs_perp().
 */
NMG_EXPORT extern struct edgeuse *nmg_find_ot_same_eu_of_e(const struct edge *e);

/**
 * @brief Check if a vertex is in use within a list of edges
 *
 * @retval 1 If found
 * @retval 0 If not found
 */
NMG_EXPORT extern int nmg_is_vertex_in_edgelist(const struct vertex *v,
                                                const struct bu_list *hd);

/**
 * @brief Check if edge \b e is present within a list of edges
 *
 * @retval 1 If found
 * @retval 0 If not found
 */
NMG_EXPORT extern int nmg_is_edge_in_edgelist(const struct edge *e,
                                              const struct bu_list *hd);

/**
 * @brief Build the set of pointers to all edgeuse structures in an NMG model
 * that are "below" the data structure pointed to by magic_p, where magic_p is
 * a pointer to the magic entry of any NMG data structure in the model.
 *
 * For "raw" geometric struts, the magic entry will be the first entry in the
 * struct - for example, a loop pointed to by l would have a magic_p key of
 * &l->magic.  For the use structures, the magic key is found within the leading
 * bu_list - for example, a faceuse pointed to by *fu would have a magic_p key
 * at &fu->l.magic
 *
 * Each edgeuse pointer will be listed exactly once - i.e. uniqueness within
 * the table may be assumed.
 *
 * @param[out] tab a bu_ptbl holding struct edgeuse pointers.
 *
 * @param magic_p pointer to an NMG data structure's magic entry.
 *
 * @param vlfree list of available vlist segments to be reused by debug drawing routines.
 */
NMG_EXPORT extern void nmg_edgeuse_tabulate(struct bu_ptbl *tab,
                                            const uint32_t *magic_p,
                                            struct bu_list *vlfree);

/**
 * @brief Build the set of pointers to all edge structures in an NMG model
 * that are "below" the data structure pointed to by magic_p, where magic_p is
 * a pointer to the magic entry of any NMG data structure in the model.
 *
 * For "raw" geometric struts, the magic entry will be the first entry in the
 * struct - for example, a loop pointed to by l would have a magic_p key of
 * &l->magic.  For the use structures, the magic key is found within the leading
 * bu_list - for example, a faceuse pointed to by *fu would have a magic_p key
 * at &fu->l.magic
 *
 * Each edge pointer will be listed exactly once - i.e. uniqueness within
 * the table may be assumed.
 *
 * @param[out] tab a bu_ptbl holding struct edge pointers.
 *
 * @param magic_p pointer to an NMG data structure's magic entry.
 *
 * @param vlfree list of available vlist segments to be reused by debug drawing routines.
 */
NMG_EXPORT extern void nmg_edge_tabulate(struct bu_ptbl *tab,
                                         const uint32_t *magic_p,
                                         struct bu_list *vlfree);

/**
 * @brief Build the set of pointers to all edge geometry structures in an NMG
 * model that are "below" the data structure pointed to by magic_p, where
 * magic_p is a pointer to the magic entry of any NMG data structure in the
 * model.
 *
 * For "raw" geometric struts, the magic entry will be the first entry in the
 * struct - for example, a loop pointed to by l would have a magic_p key of
 * &l->magic.  For the use structures, the magic key is found within the leading
 * bu_list - for example, a faceuse pointed to by *fu would have a magic_p key
 * at &fu->l.magic
 *
 * @param[out] tab a bu_ptbl holding edge geometry pointers.
 *
 * @param magic_p pointer to an NMG data structure's magic entry.
 *
 * @param vlfree list of available vlist segments to be reused by debug drawing routines.
 *
 * Each edge geometry pointer will be listed exactly once - i.e. uniqueness
 * within the table may be assumed.  However, the type of edge geometry stored
 * is *not* necessarily unique - both edge_g_lseg and edge_g_cnurb structures will
 * be stashed by this routine.  It is the responsibility of the caller to check
 * their magic numbers to sort out the type of each pointer:
 *
 * @code
 * struct bu_ptbl edge_g_tbl;
 * bu_ptbl_init(&edge_g_tbl, 64, "&edge_g_tbl");
 * nmg_edge_g_tabulate(&edge_g_tbl, &fu->l.magic, vlfree);
 * for (int i=0; i < BU_PTBL_LEN(&edge_g_tbl); i++) {
 *    struct edge_g_lseg  *elg;
 *    struct edge_g_cnurb *ecg;
 *    long *ep = BU_PTBL_GET(&edge_g_tbl, i);
 *    switch (*ep) {
 *        case NMG_EDGE_G_LSEG_MAGIC:
 *            elg = (struct edge_g_lseg *)ep;
 *            NMG_CK_EDGE_G_LSEG(elg);
 *            break;
 *        case NMG_EDGE_G_CNURB_MAGIC:
 *            ecg = (struct edge_g_cnurb *)ep;
 *            NMG_CK_EDGE_G_CNURB(ecg);
 *            break;
 *        default
 *            bu_log("Error - invalid edge geometry type\n");
 *    }
 * }
 * @endcode
 */
NMG_EXPORT extern void nmg_edge_g_tabulate(struct bu_ptbl *tab,
                                           const uint32_t *magic_p,
                                           struct bu_list *vlfree);
/**
 * @brief Build an bu_ptbl list which cites every edgeuse pointer that uses
 * edge geometry "eg".
 *
 * The edge geometry has this information encoded within it, so this is largely
 * a convenience routine to allow the caller to avoid bu_list based iteration
 * which must understand the edge geometry containers and instead use the
 * simpler bu_ptbl iteration.
 */
NMG_EXPORT extern void nmg_edgeuse_with_eg_tabulate(struct bu_ptbl *tab,
                                                    const struct edge_g_lseg *eg);
/**
 * @brief Given a pointer to any nmg data structure,
 * build an bu_ptbl list which cites every edgeuse
 * pointer from there on "down" in the model
 * that has both vertices within tolerance of the given line.
 *
 * XXX This routine is a potential source of major trouble.
 * XXX If there are "nearby" edges that "should" be on the list but
 * XXX don't make it, then the intersection calculations might
 * XXX miss important intersections.
 * As an admittedly grubby workaround, use 10X the distance tol here,
 * just to get more candidates onto the list.
 * The caller will have to wrestle with the added fuzz.
 */
NMG_EXPORT extern void nmg_edgeuse_on_line_tabulate(struct bu_ptbl *tab,
                                                    const uint32_t *magic_p,
                                                    const point_t pt,
                                                    const vect_t dir,
                                                    struct bu_list *vlfree,
                                                    const struct bn_tol *tol);

/**
 * @brief Build lists of all edges (represented by one edgeuse on that edge)
 * and all vertices found underneath the NMG entity indicated by magic_p.
 */
NMG_EXPORT extern void nmg_e_and_v_tabulate(struct bu_ptbl *eutab,
                                            struct bu_ptbl *vtab,
                                            const uint32_t *magic_p,
                                            struct bu_list *vlfree);

/**
 * Given two edgeuses, determine if they share the same edge geometry,
 * either topologically, or within tolerance.
 *
 * @retval 0 two edge geometries are not coincident
 * @retval 1 edges geometries are everywhere coincident.
 * (For linear edge_g_lseg, the 2 are the same line, within tol.)
 */
NMG_EXPORT extern int nmg_2edgeuse_g_coincident(const struct edgeuse     *eu1,
                                                const struct edgeuse     *eu2,
                                                const struct bn_tol      *tol);

/**
 * @brief Sets the "is_real" flag on all edges at or below the pointer
 * passed. Returns the number of flags set.
 */
NMG_EXPORT extern int nmg_mark_edges_real(const uint32_t *magic_p, struct bu_list *vlfree);

/**
 * @brief Convenience wrapper to retrieve and remove the last edgeuse element
 * from the \b stack bu_ptbl.
 */
NMG_EXPORT extern struct edgeuse *nmg_pop_eu(struct bu_ptbl *stack);

/**
 * @brief Moves indicated edgeuse (mv_eu) so that it passes thru the given
 * point (pt). The direction of the edgeuse is not changed, so new edgeuse is
 * parallel to the original.
 *
 * Plane equations of all radial faces on this edge are changed and all
 * vertices (except one anchor point) in radial loops are adjusted Note that
 * the anchor point is chosen arbitrarily.
 *
 * @retval 1 failure
 * @retval 0 success
 */
NMG_EXPORT extern int nmg_move_edge_thru_pnt(struct edgeuse *mv_eu,
                                             const point_t pt,
                                             const struct bn_tol *tol);

/**
 * @brief Split an edge into multiple edges at specified vertices if they
 * are within tolerance distance.
 *
 * Returns the number of additional edges that were created.
 */
NMG_EXPORT extern int nmg_break_edge_at_verts(struct edge *e,
                                              struct bu_ptbl *verts,
                                              const struct bn_tol *tol);

/**
 * @brief Apply nmg_break_edge_at_verts() to all edge/vertex combinations present
 * in the NMG model below the NMG element with the magic number pointed to by
 * \b magic_p
 */
NMG_EXPORT extern int nmg_break_edges(uint32_t *magic_p, struct bu_list *vlfree,
                                      const struct bn_tol *tol);

/**
 * @brief Fuse vertices and edges according to \b tol
 *
 * TODO - this needs MUCH better documentation...
 *
 * If a bu_ptbl structure is passed into this function, the structure must
 * contain edgeuse. Vertices will then be fused at the shell level. If an NMG
 * structure is passed into this function, if the structure is an NMG region or
 * model, vertices will be fused at the model level. If the NMG structure
 * passed in is a shell or anything lower, vertices will be fused at the shell
 * level.
 */
NMG_EXPORT extern int nmg_edge_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
                                    const struct bn_tol *tol);

/**
 * @brief Fuse edge_g structs.
 *
 * TODO - this needs MUCH better documentation...
 */
NMG_EXPORT extern int nmg_edge_g_fuse(const uint32_t *magic_p,struct bu_list *vlfree,
                                      const struct bn_tol       *tol);

/**
 * @brief Given two edgeuses with different edge geometry but running between
 * the same two vertices, select the proper edge geometry to associate with.
 *
 * Really, there are 3 geometries to be compared here: the vector between the
 * two endpoints of this edge, and the two edge_g structures.  Rather than
 * always taking eu2 or eu1, select the one that best fits this one edge.
 *
 * Consider fu1:
 * @verbatim
                         B
                         *
                        /|
                    eg2/ |
                      /  |
                    D/   |
                    *    |
                   /     |
                A *-*----* C
                    E eg1
   @endverbatim
 *
 * At the start of a face/face intersection, eg1 runs from A to C,
 * and eg2 runs ADB.  The line of intersection with the other face
 * (fu2, not drawn) lies along eg1.
 * Assume that edge AC needs to be broken at E,
 * where E is just a little more than tol->dist away from A.
 * Existing point D is found because it *is* within tol->dist of E,
 * thanks to the cosine of angle BAC.
 * So, edge AC is broken on vertex D, and the intersection list
 * contains vertexuses A, E, and C.
 *
 * Because D and E are the same point, fu1 has become a triangle with
 * a little "spike" on the end.  If this is handled simply by re-homing
 * edge AE to eg2, it may cause trouble, because eg1 now runs EC,
 * but the geometry for eg1 runs AC.  If there are other vertices on
 * edge eg1, the problem can not be resolved simply by recomputing the
 * geometry of eg1.
 * Since E (D) is within tolerance of eg1, it is not unreasonable
 * just to leave eg1 alone.
 *
 * The issue boils down to selecting whether the existing eg1 or eg2
 * best represents the direction of the little stub edge AD (shared with AE).
 * In this case, eg2 is the correct choice, as AD (and AE) lie on line AB.
 *
 * It would be disastrous to force *all* of eg1 to use the edge geometry
 * of eg2, as the two lines are very different.
 */
NMG_EXPORT extern struct edge_g_lseg    *nmg_pick_best_edge_g(struct edgeuse *eu1,
                                                              struct edgeuse *eu2,
                                                              const struct bn_tol *tol);

/**
 * @brief Check if \b eu is part of a loop but is not correctly connected
 * topologically to other edges in the loop.
 */
NMG_EXPORT extern int nmg_eu_is_part_of_crack(const struct edgeuse *eu);

/**
 * @brief Make all the edgeuses around eu2's edge to refer to eu1's edge,
 * taking care to organize them into the proper angular orientation, so that
 * the attached faces are correctly arranged radially around the edge.
 *
 * This depends on both edges being part of face loops, with vertex and face
 * geometry already associated.
 *
 * The two edgeuses being joined might well be from separate shells, so the
 * issue of preserving (simple) faceuse orientation parity (SAME, OPPOSITE,
 * OPPOSITE, SAME, ...) can't be used here -- that only applies to faceuses
 * from the same shell.
 *
 * Some of the edgeuses around both edges may be wires.
 *
 * Call to nmg_check_radial at end has been deleted.  Note that after two
 * radial EU's have been joined a third cannot be joined to them without
 * creating unclosed space that nmg_check_radial will find.
 */
NMG_EXPORT extern void nmg_radial_join_eu(struct edgeuse *eu1,
                                          struct edgeuse *eu2,
                                          const struct bn_tol *tol);

/**
 * @brief TODO - document...
 */
NMG_EXPORT extern int nmg_break_all_es_on_v(uint32_t *magic_p,
                                            struct vertex *v,struct bu_list *vlfree,
                                            const struct bn_tol *tol);

/**
 * @brief As the first step in evaluating a boolean formula,
 * before starting to do face/face intersections, compare every
 * edge in the model with every vertex in the model.
 *
 * If the vertex is within tolerance of the edge, break the edge,
 * and enroll the new edge on a list of edges still to be processed.
 *
 * A list of edges and a list of vertices are built, and then processed.
 *
 * Space partitioning could improve the performance of this algorithm.
 * For the moment, a brute-force approach is used.
 *
 * @return
 * Number of edges broken.
 */
NMG_EXPORT extern int nmg_break_e_on_v(const uint32_t *magic_p,struct bu_list *vlfree,
                                       const struct bn_tol *tol);

/**
 * @brief Determine if the given wedge is entirely to the left or right of the
 * ray, or if it crosses.
 *
 * 0 degrees is to the rear (ON_REV), 90 degrees is to the RIGHT, 180 is
 * ON_FORW, 270 is to the LEFT.
 *
 * "halfway X" (ha, hb) have these properties:
 * @verbatim
 < 0 (==> X < 180) RIGHT
 > 0 (==> X > 180) LEFT
      ==0     (==> X == 180) ON_FORW
   @endverbatim
 *
 * @retval WEDGE_LEFT
 * @retval WEDGE_CROSSING
 * @retval WEDGE_RIGHT
 * @retval WEDGE_ON
 */
NMG_EXPORT extern int nmg_wedge_class(int        ass,    /* assessment of two edges forming wedge */
                                      double     a,
                                      double     b);

__END_DECLS

#endif  /* NMG_EDGE_H */
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
