/*                       S H E L L . H
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
 * NMG shell definitions
 */
/** @{ */
/** @file nmg/shell.h */

#ifndef NMG_SHELL_H
#define NMG_SHELL_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

#define NMG_CK_SHELL(_p)              NMG_CKMAG(_p, NMG_SHELL_MAGIC, "shell")
#define NMG_CK_SHELL_A(_p)            NMG_CKMAG(_p, NMG_SHELL_A_MAGIC, "shell_a")

/**
 * When a shell encloses volume, it's done entirely by the list of
 * faceuses.
 *
 * The wire loopuses (each of which heads a list of edges) define a
 * set of connected line segments which form a closed path, but do not
 * enclose either volume or surface area.
 *
 * The wire edgeuses are disconnected line segments.  There is a
 * special interpretation to the eu_hd list of wire edgeuses.  Unlike
 * edgeuses seen in loops, the eu_hd list contains eu1, eu1mate, eu2,
 * eu2mate, ..., where each edgeuse and its mate comprise a
 * *non-connected* "wire" edge which starts at eu1->vu_p->v_p and ends
 * at eu1mate->vu_p->v_p.  There is no relationship between the pairs
 * of edgeuses at all, other than that they all live on the same
 * linked list.
 */
struct shell {
    struct bu_list l;           /**< @brief shells, in region's s_hd list */
    struct nmgregion *r_p;      /**< @brief owning region */
    struct shell_a *sa_p;       /**< @brief attribs */

    struct bu_list fu_hd;       /**< @brief list of face uses in shell */
    struct bu_list lu_hd;       /**< @brief wire loopuses (edge groups) */
    struct bu_list eu_hd;       /**< @brief wire list (shell has wires) */
    struct vertexuse *vu_p;     /**< @brief internal ptr to single vertexuse */
    long index;                 /**< @brief struct # in this model */
};

struct shell_a {
    uint32_t magic;
    point_t min_pt;             /**< @brief minimums of bounding box */
    point_t max_pt;             /**< @brief maximums of bounding box */
    long index;                 /**< @brief struct # in this model */
};

#define GET_SHELL(p, m)             {NMG_GETSTRUCT(p, shell); NMG_INCR_INDEX(p, m);}
#define GET_SHELL_A(p, m)           {NMG_GETSTRUCT(p, shell_a); NMG_INCR_INDEX(p, m);}

#define FREE_SHELL(p)             NMG_FREESTRUCT(p, shell)
#define FREE_SHELL_A(p)           NMG_FREESTRUCT(p, shell_a)

NMG_EXPORT extern int nmg_shell_is_empty(const struct shell *s);
NMG_EXPORT extern struct shell *nmg_find_s_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct shell *nmg_find_s_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct shell *nmg_find_s_of_vu(const struct vertexuse *vu);


NMG_EXPORT extern struct shell *nmg_extrude_cleanup(struct shell *is, const int is_void, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern void nmg_hollow_shell(struct shell *s, const fastf_t thick, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);
NMG_EXPORT extern struct shell *nmg_extrude_shell(struct shell *s, const fastf_t dist, const int normal_ward, const int approximate, struct bu_list *vlfree, const struct bn_tol *tol);

NMG_EXPORT extern void nmg_close_shell(struct shell *s, struct bu_list *vlfree,
                                       const struct bn_tol *tol);

NMG_EXPORT extern struct shell *nmg_dup_shell(struct shell *s,
                                              long ***copy_tbl,
                                              struct bu_list *vlfree,
                                              const struct bn_tol *tol);
NMG_EXPORT extern void nmg_glue_face_in_shell(const struct faceuse *fu,
                                              struct shell *s,
                                              const struct bn_tol *tol);
NMG_EXPORT extern int nmg_open_shells_connect(struct shell *dst,
                                              struct shell *src,
                                              const long **copy_tbl,
                                              struct bu_list *vlfree,
                                              const struct bn_tol *tol);
NMG_EXPORT extern int nmg_simplify_shell_edges(struct shell *s,
                                               const struct bn_tol *tol);
NMG_EXPORT extern char *nmg_shell_manifolds(struct shell *sp,
                                            char *tbl);


__END_DECLS

#endif  /* NMG_SHELL_H */
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
