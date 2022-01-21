/*                        I N D E X . H
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
/** @addtogroup libnmg
 * @brief
 * NMG index definitions
 */
/** @{ */
/** @file nmg/index.h */

#ifndef NMG_INDEX_H
#define NMG_INDEX_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

struct nmg_struct_counts {
    /* Actual structure counts (Xuse, then X) */
    long model;
    long region;
    long region_a;
    long shell;
    long shell_a;
    long faceuse;
    long face;
    long face_g_plane;
    long face_g_snurb;
    long loopuse;
    long loop;
    long loop_a;
    long edgeuse;
    long edge;
    long edge_g_lseg;
    long edge_g_cnurb;
    long vertexuse;
    long vertexuse_a_plane;
    long vertexuse_a_cnurb;
    long vertex;
    long vertex_g;
    /* Abstractions */
    long max_structs;
    long face_loops;
    long face_edges;
    long face_lone_verts;
    long wire_loops;
    long wire_loop_edges;
    long wire_edges;
    long wire_lone_verts;
    long shells_of_lone_vert;
};

/*
 * For use with tables subscripted by NMG structure "index" values,
 * traditional test and set macros.
 *
 * A value of zero indicates unset, a value of one indicates set.
 * test-and-set returns TRUE if value was unset; in the process, value
 * has become set.  This is often used to detect the first time an
 * item is used, so an alternative name is given, for clarity.
 *
 * Note that the somewhat simpler auto-increment form:
 *      ((tab)[(p)->index]++ == 0)
 * is not used, to avoid the possibility of integer overflow from
 * repeated test-and-set operations on one item.
 */
#define NMG_INDEX_VALUE(_tab, _index)           ((_tab)[_index])
#define NMG_INDEX_TEST(_tab, _p)                ((_tab)[(_p)->index])
#define NMG_INDEX_SET(_tab, _p) {(_tab)[(_p)->index] = 1;}
#define NMG_INDEX_CLEAR(_tab, _p) {(_tab)[(_p)->index] = 0;}
#define NMG_INDEX_TEST_AND_SET(_tab, _p)        ((_tab)[(_p)->index] == 0 ? ((_tab)[(_p)->index] = 1) : 0)
#define NMG_INDEX_IS_SET(_tab, _p)              NMG_INDEX_TEST(_tab, _p)
#define NMG_INDEX_FIRST_TIME(_tab, _p)          NMG_INDEX_TEST_AND_SET(_tab, _p)
#define NMG_INDEX_ASSIGN(_tab, _p, _val) {(_tab)[(_p)->index] = _val;}
#define NMG_INDEX_GET(_tab, _p)                 ((_tab)[(_p)->index])
#define NMG_INDEX_GETP(_ty, _tab, _p)           ((struct _ty *)((_tab)[(_p)->index]))
#define NMG_INDEX_OR(_tab, _p, _val) {(_tab)[(_p)->index] |= _val;}
#define NMG_INDEX_AND(_tab, _p, _val) {(_tab)[(_p)->index] &= _val;}
#define NMG_INDEX_RETURN_IF_SET_ELSE_SET(_tab, _index) { \
        if ((_tab)[_index]) return; \
        else (_tab)[_index] = 1; \
    }

/* flags for manifold-ness */
#define NMG_0MANIFOLD  1
#define NMG_1MANIFOLD  2
#define NMG_2MANIFOLD  4
#define NMG_DANGLING   8 /* NMG_2MANIFOLD + 4th bit for special cond (UNUSED) */
#define NMG_3MANIFOLD 16

#define NMG_SET_MANIFOLD(_t, _p, _v) NMG_INDEX_OR(_t, _p, _v)
#define NMG_MANIFOLDS(_t, _p)        NMG_INDEX_VALUE(_t, (_p)->index)
#define NMG_CP_MANIFOLD(_t, _p, _q)  (_t)[(_p)->index] = (_t)[(_q)->index]

NMG_EXPORT extern int nmg_index_of_struct(const uint32_t *p);

NMG_EXPORT extern uint32_t **nmg_m_struct_count(struct nmg_struct_counts *ctr,
                                                const struct model *m);
NMG_EXPORT extern void nmg_pr_m_struct_counts(const struct model *m,
                                              const char         *str);

NMG_EXPORT extern void nmg_vls_struct_counts(struct bu_vls *str,
                                             const struct nmg_struct_counts *ctr);
NMG_EXPORT extern void nmg_pr_struct_counts(const struct nmg_struct_counts *ctr,
                                            const char *str);
__END_DECLS

#endif  /* NMG_INDEX_H */
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
