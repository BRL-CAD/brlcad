/*                     N M G _ I N D E X . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2020 United States Government as represented by
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
/** @addtogroup nmg */
/** @{ */
/** @file primitives/nmg/nmg_index.c
 *
 * Handle counting and re-indexing of NMG data structures.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/malloc.h"
#include "nmg.h"

/**
 * Return the structure index number of an arbitrary NMG structure.
 *
 * Returns -
 * >=0 index number
 * -1 pointed at struct bu_list embedded within NMG structure.
 * -2 error:  unknown magic number
 */
int
nmg_index_of_struct(register const uint32_t *p)
{
    switch (*p) {
	case NMG_MODEL_MAGIC:
	    return ((struct model *)p)->index;
	case NMG_REGION_MAGIC:
	    return ((struct nmgregion *)p)->index;
	case NMG_REGION_A_MAGIC:
	    return ((struct nmgregion_a *)p)->index;
	case NMG_SHELL_MAGIC:
	    return ((struct shell *)p)->index;
	case NMG_SHELL_A_MAGIC:
	    return ((struct shell_a *)p)->index;
	case NMG_FACEUSE_MAGIC:
	    return ((struct faceuse *)p)->index;
	case NMG_FACE_MAGIC:
	    return ((struct face *)p)->index;
	case NMG_FACE_G_PLANE_MAGIC:
	    return ((struct face_g_plane *)p)->index;
	case NMG_FACE_G_SNURB_MAGIC:
	    return ((struct face_g_snurb *)p)->index;
	case NMG_LOOPUSE_MAGIC:
	    return ((struct loopuse *)p)->index;
	case NMG_LOOP_MAGIC:
	    return ((struct loop *)p)->index;
	case NMG_LOOP_G_MAGIC:
	    return ((struct loop_g *)p)->index;
	case NMG_EDGEUSE_MAGIC:
	    return ((struct edgeuse *)p)->index;
	case NMG_EDGEUSE2_MAGIC:
	    /* Points to l2 inside edgeuse */
	    return BU_LIST_MAIN_PTR(edgeuse, p, l2)->index;
	case NMG_EDGE_MAGIC:
	    return ((struct edge *)p)->index;
	case NMG_EDGE_G_LSEG_MAGIC:
	    return ((struct edge_g_lseg *)p)->index;
	case NMG_EDGE_G_CNURB_MAGIC:
	    return ((struct edge_g_cnurb *)p)->index;
	case NMG_VERTEXUSE_MAGIC:
	    return ((struct vertexuse *)p)->index;
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
	    return ((struct vertexuse_a_plane *)p)->index;
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
	    return ((struct vertexuse_a_cnurb *)p)->index;
	case NMG_VERTEX_MAGIC:
	    return ((struct vertex *)p)->index;
	case NMG_VERTEX_G_MAGIC:
	    return ((struct vertex_g *)p)->index;
	case BU_LIST_HEAD_MAGIC:
	    /* indicate special list head encountered */
	    return -1;
    }
    /* default */
    bu_log("nmg_index_of_struct: magicp = %p, magic = %x\n", (void *)p, *p);
    return -2;	/* indicate error */
}


#define NMG_HIGH_BIT 0x80000000

#define NMG_MARK_INDEX(_p)	((_p)->index |= NMG_HIGH_BIT)

#define NMG_ASSIGN_NEW_INDEX(_p)	\
    { if (((_p)->index & NMG_HIGH_BIT) != 0) \
	    (_p)->index = newindex++; }

/**
 * Helper routine
 */
static void
nmg_mark_edge_g(uint32_t *magic_p)
{
    if (!magic_p) bu_bomb("nmg_mark_edge_g bad magic\n");
    switch (*magic_p) {
	case NMG_EDGE_G_LSEG_MAGIC: {
	    struct edge_g_lseg *lseg = (struct edge_g_lseg *)magic_p;
	    NMG_MARK_INDEX(lseg);
	    return;
	}
	case NMG_EDGE_G_CNURB_MAGIC: {
	    struct edge_g_cnurb *cnurb = (struct edge_g_cnurb *)magic_p;
	    NMG_MARK_INDEX(cnurb);
	    return;
	}
    }
    bu_bomb("nmg_mark_edge_g() unknown magic\n");
}


/**
 * First pass:  just set the high bit on all index words
 *
 * This is a separate function largely for the benefit of global optimizers,
 * which tended to blow their brains out on such a large subroutine.
 */
void
nmg_m_set_high_bit(struct model *m)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct loopuse *lu;
    struct loop *l;
    register struct edgeuse *eu;
    struct edge *e;
    register struct vertexuse *vu;

#define MARK_VU(_vu) { \
	struct vertex *v; \
	NMG_CK_VERTEXUSE(_vu); \
	NMG_MARK_INDEX(_vu); \
	if (_vu->a.magic_p) switch (*_vu->a.magic_p) { \
		case NMG_VERTEXUSE_A_PLANE_MAGIC: \
		    NMG_MARK_INDEX(_vu->a.plane_p); \
		    break; \
		case NMG_VERTEXUSE_A_CNURB_MAGIC: \
		    NMG_MARK_INDEX(_vu->a.cnurb_p); \
		    break; \
	    } \
	v = _vu->v_p; \
	NMG_CK_VERTEX(v); \
	NMG_MARK_INDEX(v); \
	if (v->vg_p) { \
	    NMG_CK_VERTEX_G(v->vg_p); \
	    NMG_MARK_INDEX(v->vg_p); \
	} \
    }

    NMG_CK_MODEL(m);
    NMG_MARK_INDEX(m);

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	NMG_MARK_INDEX(r);
	if (r->ra_p) {
	    NMG_CK_REGION_A(r->ra_p);
	    NMG_MARK_INDEX(r->ra_p);
	}
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    NMG_CK_SHELL(s);
	    NMG_MARK_INDEX(s);
	    if (s->sa_p) {
		NMG_CK_SHELL_A(s->sa_p);
		NMG_MARK_INDEX(s->sa_p);
	    }
	    /* Faces in shell */
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		NMG_MARK_INDEX(fu);
		f = fu->f_p;
		NMG_CK_FACE(f);
		NMG_MARK_INDEX(f);
		if (f->g.magic_p) switch (*f->g.magic_p) {
			case NMG_FACE_G_PLANE_MAGIC:
			    NMG_MARK_INDEX(f->g.plane_p);
			    break;
			case NMG_FACE_G_SNURB_MAGIC:
			    NMG_MARK_INDEX(f->g.snurb_p);
			    break;
		    }
		/* Loops in face */
		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    NMG_MARK_INDEX(lu);
		    l = lu->l_p;
		    NMG_CK_LOOP(l);
		    NMG_MARK_INDEX(l);
		    if (l->lg_p) {
			NMG_CK_LOOP_G(l->lg_p);
			NMG_MARK_INDEX(l->lg_p);
		    }
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			/* Loop of Lone vertex */
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			MARK_VU(vu);
			continue;
		    }
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_MARK_INDEX(eu);
			e = eu->e_p;
			NMG_CK_EDGE(e);
			NMG_MARK_INDEX(e);
			if (eu->g.magic_p) nmg_mark_edge_g(eu->g.magic_p);
			vu = eu->vu_p;
			MARK_VU(vu);
		    }
		}
	    }
	    /* Wire loops in shell */
	    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		NMG_MARK_INDEX(lu);
		l = lu->l_p;
		NMG_CK_LOOP(l);
		NMG_MARK_INDEX(l);
		if (l->lg_p) {
		    NMG_CK_LOOP_G(l->lg_p);
		    NMG_MARK_INDEX(l->lg_p);
		}
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		    /* Wire loop of Lone vertex */
		    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		    MARK_VU(vu);
		    continue;
		}
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);
		    NMG_MARK_INDEX(eu);
		    e = eu->e_p;
		    NMG_CK_EDGE(e);
		    NMG_MARK_INDEX(e);
		    if (eu->g.magic_p) nmg_mark_edge_g(eu->g.magic_p);
		    vu = eu->vu_p;
		    MARK_VU(vu);
		}
	    }
	    /* Wire edges in shell */
	    for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
		NMG_CK_EDGEUSE(eu);
		NMG_MARK_INDEX(eu);
		e = eu->e_p;
		NMG_CK_EDGE(e);
		NMG_MARK_INDEX(e);
		if (eu->g.magic_p) nmg_mark_edge_g(eu->g.magic_p);
		vu = eu->vu_p;
		MARK_VU(vu);
	    }
	    /* Lone vertex in shell */
	    vu = s->vu_p;
	    if (vu) {
		MARK_VU(vu);
	    }
	}
    }
#undef MARK_VU
}


/**
 * Reassign index numbers to all the data structures in a model.
 * The model structure will get index 0, all others will be sequentially
 * assigned after that.
 *
 * Because the first pass has done extensive error checking,
 * the second pass can do less.
 */
void
nmg_m_reindex(struct model *m, register long int newindex)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct loopuse *lu;
    struct loop *l;
    register struct edgeuse *eu;
    struct edge *e;
    register struct vertexuse *vu;
    struct vertex *v;

#define ASSIGN_VU(_vu) { \
	NMG_CK_VERTEXUSE(_vu); \
	NMG_ASSIGN_NEW_INDEX(_vu); \
	if (_vu->a.magic_p) switch (*_vu->a.magic_p) { \
		case NMG_VERTEXUSE_A_PLANE_MAGIC: \
		    NMG_ASSIGN_NEW_INDEX(_vu->a.plane_p); \
		    break; \
		case NMG_VERTEXUSE_A_CNURB_MAGIC: \
		    NMG_ASSIGN_NEW_INDEX(_vu->a.cnurb_p); \
		    break; \
	    } \
	v = _vu->v_p; \
	NMG_CK_VERTEX(v); \
	NMG_ASSIGN_NEW_INDEX(v); \
	if (v->vg_p) NMG_ASSIGN_NEW_INDEX(v->vg_p); \
    }

    NMG_CK_MODEL(m);
    if (m->index != 0) bu_log("nmg_m_reindex() m->index=%ld\n", m->index);
    if (newindex < 0) bu_log("nmg_m_reindex() newindex(%ld) < 0\n", newindex);

    /* First pass:  set high bits */
    nmg_m_set_high_bit(m);

    /*
     * Second pass:  assign new index number
     */

    NMG_ASSIGN_NEW_INDEX(m);
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	NMG_ASSIGN_NEW_INDEX(r);
	if (r->ra_p) NMG_ASSIGN_NEW_INDEX(r->ra_p);
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    NMG_CK_SHELL(s);
	    NMG_ASSIGN_NEW_INDEX(s);
	    if (s->sa_p) NMG_ASSIGN_NEW_INDEX(s->sa_p);
	    /* Faces in shell */
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		NMG_ASSIGN_NEW_INDEX(fu);
		f = fu->f_p;
		NMG_CK_FACE(f);
		NMG_ASSIGN_NEW_INDEX(f);
		if (f->g.plane_p) switch (*f->g.magic_p) {
			case NMG_FACE_G_PLANE_MAGIC:
			    NMG_ASSIGN_NEW_INDEX(f->g.plane_p);
			    break;
			case NMG_FACE_G_SNURB_MAGIC:
			    NMG_ASSIGN_NEW_INDEX(f->g.snurb_p);
			    break;
		    }
		/* Loops in face */
		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    NMG_ASSIGN_NEW_INDEX(lu);
		    l = lu->l_p;
		    NMG_CK_LOOP(l);
		    NMG_ASSIGN_NEW_INDEX(l);
		    if (l->lg_p) NMG_ASSIGN_NEW_INDEX(l->lg_p);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			/* Loop of Lone vertex */
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			ASSIGN_VU(vu);
			continue;
		    }
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_ASSIGN_NEW_INDEX(eu);
			e = eu->e_p;
			NMG_CK_EDGE(e);
			NMG_ASSIGN_NEW_INDEX(e);
			if (eu->g.magic_p) switch (*eu->g.magic_p) {
				case NMG_EDGE_G_LSEG_MAGIC:
				    NMG_ASSIGN_NEW_INDEX(eu->g.lseg_p);
				    break;
				case NMG_EDGE_G_CNURB_MAGIC:
				    NMG_ASSIGN_NEW_INDEX(eu->g.cnurb_p);
				    break;
			    }
			vu = eu->vu_p;
			ASSIGN_VU(vu);
		    }
		}
	    }
	    /* Wire loops in shell */
	    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		NMG_ASSIGN_NEW_INDEX(lu);
		l = lu->l_p;
		NMG_CK_LOOP(l);
		NMG_ASSIGN_NEW_INDEX(l);
		if (l->lg_p) NMG_ASSIGN_NEW_INDEX(l->lg_p);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		    /* Wire loop of Lone vertex */
		    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		    ASSIGN_VU(vu);
		    continue;
		}
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);
		    NMG_ASSIGN_NEW_INDEX(eu);
		    e = eu->e_p;
		    NMG_CK_EDGE(e);
		    NMG_ASSIGN_NEW_INDEX(e);
		    if (eu->g.magic_p) switch (*eu->g.magic_p) {
			    case NMG_EDGE_G_LSEG_MAGIC:
				NMG_ASSIGN_NEW_INDEX(eu->g.lseg_p);
				break;
			    case NMG_EDGE_G_CNURB_MAGIC:
				NMG_ASSIGN_NEW_INDEX(eu->g.cnurb_p);
				break;
			}
		    vu = eu->vu_p;
		    ASSIGN_VU(vu);
		}
	    }
	    /* Wire edges in shell */
	    for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
		NMG_CK_EDGEUSE(eu);
		NMG_ASSIGN_NEW_INDEX(eu);
		e = eu->e_p;
		NMG_CK_EDGE(e);
		NMG_ASSIGN_NEW_INDEX(e);
		if (eu->g.magic_p) switch (*eu->g.magic_p) {
			case NMG_EDGE_G_LSEG_MAGIC:
			    NMG_ASSIGN_NEW_INDEX(eu->g.lseg_p);
			    break;
			case NMG_EDGE_G_CNURB_MAGIC:
			    NMG_ASSIGN_NEW_INDEX(eu->g.cnurb_p);
			    break;
		    }
		vu = eu->vu_p;
		ASSIGN_VU(vu);
	    }
	    /* Lone vertex in shell */
	    vu = s->vu_p;
	    if (vu) {
		ASSIGN_VU(vu);
	    }
	}
    }
#undef ASSIGN_VU

    if (nmg_debug & NMG_DEBUG_BASIC) {
	bu_log("nmg_m_reindex() oldmax=%ld, new%ld=>%ld\n",
	       m->maxindex, m->index, newindex);
    }
    m->maxindex = newindex;
}


void
nmg_vls_struct_counts(struct bu_vls *str, const struct nmg_struct_counts *ctr)
{
    BU_CK_VLS(str);

    bu_vls_printf(str, " Actual structure counts:\n");
    bu_vls_printf(str, "\t%6ld model\n", ctr->model);
    bu_vls_printf(str, "\t%6ld region\n", ctr->region);
    bu_vls_printf(str, "\t%6ld region_a\n", ctr->region_a);
    bu_vls_printf(str, "\t%6ld shell\n", ctr->shell);
    bu_vls_printf(str, "\t%6ld shell_a\n", ctr->shell_a);
    bu_vls_printf(str, "\t%6ld face\n", ctr->face);
    bu_vls_printf(str, "\t%6ld face_g_plane\n", ctr->face_g_plane);
    bu_vls_printf(str, "\t%6ld face_g_snurb\n", ctr->face_g_snurb);
    bu_vls_printf(str, "\t%6ld faceuse\n", ctr->faceuse);
    bu_vls_printf(str, "\t%6ld loopuse\n", ctr->loopuse);
    bu_vls_printf(str, "\t%6ld loop\n", ctr->loop);
    bu_vls_printf(str, "\t%6ld loop_g\n", ctr->loop_g);
    bu_vls_printf(str, "\t%6ld edgeuse\n", ctr->edgeuse);
    bu_vls_printf(str, "\t%6ld edge\n", ctr->edge);
    bu_vls_printf(str, "\t%6ld edge_g_lseg\n", ctr->edge_g_lseg);
    bu_vls_printf(str, "\t%6ld edge_g_cnurb\n", ctr->edge_g_cnurb);
    bu_vls_printf(str, "\t%6ld vertexuse\n", ctr->vertexuse);
    bu_vls_printf(str, "\t%6ld vertexuse_a_plane\n", ctr->vertexuse_a_plane);
    bu_vls_printf(str, "\t%6ld vertexuse_a_cnurb\n", ctr->vertexuse_a_cnurb);
    bu_vls_printf(str, "\t%6ld vertex\n", ctr->vertex);
    bu_vls_printf(str, "\t%6ld vertex_g\n", ctr->vertex_g);
    bu_vls_printf(str, " Abstractions:\n");
    bu_vls_printf(str, "\t%6ld max_structs\n", ctr->max_structs);
    bu_vls_printf(str, "\t%6ld face_loops\n", ctr->face_loops);
    bu_vls_printf(str, "\t%6ld face_edges\n", ctr->face_edges);
    bu_vls_printf(str, "\t%6ld face_lone_verts\n", ctr->face_lone_verts);
    bu_vls_printf(str, "\t%6ld wire_loops\n", ctr->wire_loops);
    bu_vls_printf(str, "\t%6ld wire_loop_edges\n", ctr->wire_loop_edges);
    bu_vls_printf(str, "\t%6ld wire_edges\n", ctr->wire_edges);
    bu_vls_printf(str, "\t%6ld wire_lone_verts\n", ctr->wire_lone_verts);
    bu_vls_printf(str, "\t%6ld shells_of_lone_vert\n", ctr->shells_of_lone_vert);
}


void
nmg_pr_struct_counts(const struct nmg_struct_counts *ctr, const char *str)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    bu_log("nmg_pr_count(%s)\n", str);

    nmg_vls_struct_counts(&vls, ctr);
    bu_log("%s", bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


/**
 * Returns -
 * Pointer to magic-number/structure-base pointer array,
 * indexed by nmg structure index.
 * Caller is responsible for freeing it.
 */
uint32_t **
nmg_m_struct_count(register struct nmg_struct_counts *ctr, const struct model *m)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct loopuse *lu;
    struct loop *l;
    struct edgeuse *eu;
    struct edge *e;
    struct vertexuse *vu;
    struct vertex *v;
    register uint32_t **ptrs;

#define NMG_UNIQ_INDEX(_p, _type)	\
    if ((_p)->index > m->maxindex) { \
	bu_log("%p (%s) has index %ld, m->maxindex=%ld\n", (void *)(_p), \
	       bu_identify_magic(*((uint32_t *)(_p))), (_p)->index, m->maxindex); \
	bu_bomb("nmg_m_struct_count index overflow\n"); \
    } \
    if (ptrs[(_p)->index] == NULL) { \
	ptrs[(_p)->index] = (uint32_t *)(_p); \
	ctr->_type++; \
    }

#define UNIQ_VU(_vu) { \
	NMG_CK_VERTEXUSE(_vu); \
	NMG_UNIQ_INDEX(_vu, vertexuse); \
	if (_vu->a.magic_p) switch (*_vu->a.magic_p) { \
		case NMG_VERTEXUSE_A_PLANE_MAGIC: \
		    NMG_UNIQ_INDEX(_vu->a.plane_p, vertexuse_a_plane); \
		    break; \
		case NMG_VERTEXUSE_A_CNURB_MAGIC: \
		    NMG_UNIQ_INDEX(_vu->a.cnurb_p, vertexuse_a_cnurb); \
		    break; \
	    } \
	v = _vu->v_p; \
	NMG_CK_VERTEX(v); \
	NMG_UNIQ_INDEX(v, vertex); \
	if (v->vg_p) { \
	    NMG_CK_VERTEX_G(v->vg_p); \
	    NMG_UNIQ_INDEX(v->vg_p, vertex_g); \
	} \
    }

    NMG_CK_MODEL(m);
    memset((char *)ctr, 0, sizeof(*ctr));

    ptrs = (uint32_t **)nmg_calloc(m->maxindex+1, sizeof(uint32_t *), "nmg_m_count ptrs[]");

    NMG_UNIQ_INDEX(m, model);
    ctr->max_structs = m->maxindex;
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	NMG_UNIQ_INDEX(r, region);
	if (r->ra_p) {
	    NMG_CK_REGION_A(r->ra_p);
	    NMG_UNIQ_INDEX(r->ra_p, region_a);
	}
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    NMG_CK_SHELL(s);
	    NMG_UNIQ_INDEX(s, shell);
	    if (s->sa_p) {
		NMG_CK_SHELL_A(s->sa_p);
		NMG_UNIQ_INDEX(s->sa_p, shell_a);
	    }
	    /* Faces in shell */
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		NMG_UNIQ_INDEX(fu, faceuse);
		f = fu->f_p;
		NMG_CK_FACE(f);
		NMG_UNIQ_INDEX(f, face);
		if (f->g.magic_p) switch (*f->g.magic_p) {
			case NMG_FACE_G_PLANE_MAGIC:
			    NMG_UNIQ_INDEX(f->g.plane_p, face_g_plane);
			    break;
			case NMG_FACE_G_SNURB_MAGIC:
			    NMG_UNIQ_INDEX(f->g.snurb_p, face_g_snurb);
			    break;
		    }
		/* Loops in face */
		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    NMG_UNIQ_INDEX(lu, loopuse);
		    l = lu->l_p;
		    NMG_CK_LOOP(l);
		    NMG_UNIQ_INDEX(l, loop);
		    if (l->lg_p) {
			NMG_CK_LOOP_G(l->lg_p);
			NMG_UNIQ_INDEX(l->lg_p, loop_g);
		    }
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			/* Loop of Lone vertex */
			ctr->face_lone_verts++;
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			UNIQ_VU(vu);
			continue;
		    }
		    ctr->face_loops++;
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			ctr->face_edges++;
			NMG_CK_EDGEUSE(eu);
			NMG_UNIQ_INDEX(eu, edgeuse);
			e = eu->e_p;
			NMG_CK_EDGE(e);
			NMG_UNIQ_INDEX(e, edge);
			if (eu->g.magic_p) switch (*eu->g.magic_p) {
				case NMG_EDGE_G_LSEG_MAGIC:
				    NMG_UNIQ_INDEX(eu->g.lseg_p, edge_g_lseg);
				    break;
				case NMG_EDGE_G_CNURB_MAGIC:
				    NMG_UNIQ_INDEX(eu->g.cnurb_p, edge_g_cnurb);
				    break;
			    }
			vu = eu->vu_p;
			UNIQ_VU(vu);
		    }
		}
	    }
	    /* Wire loops in shell */
	    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		NMG_UNIQ_INDEX(lu, loopuse);
		l = lu->l_p;
		NMG_CK_LOOP(l);
		NMG_UNIQ_INDEX(l, loop);
		if (l->lg_p) {
		    NMG_CK_LOOP_G(l->lg_p);
		    NMG_UNIQ_INDEX(l->lg_p, loop_g);
		}
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		    ctr->wire_lone_verts++;
		    /* Wire loop of Lone vertex */
		    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		    UNIQ_VU(vu);
		    continue;
		}
		ctr->wire_loops++;
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);
		    NMG_UNIQ_INDEX(eu, edgeuse);
		    e = eu->e_p;
		    NMG_CK_EDGE(e);
		    NMG_UNIQ_INDEX(e, edge);
		    if (eu->g.magic_p) switch (*eu->g.magic_p) {
			    case NMG_EDGE_G_LSEG_MAGIC:
				NMG_UNIQ_INDEX(eu->g.lseg_p, edge_g_lseg);
				break;
			    case NMG_EDGE_G_CNURB_MAGIC:
				NMG_UNIQ_INDEX(eu->g.cnurb_p, edge_g_cnurb);
				break;
			}
		    vu = eu->vu_p;
		    UNIQ_VU(vu);
		    ctr->wire_loop_edges++;
		}
	    }
	    /* Wire edges in shell */
	    for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
		NMG_CK_EDGEUSE(eu);
		ctr->wire_edges++;
		NMG_UNIQ_INDEX(eu, edgeuse);
		e = eu->e_p;
		NMG_CK_EDGE(e);
		NMG_UNIQ_INDEX(e, edge);
		if (eu->g.magic_p) switch (*eu->g.magic_p) {
			case NMG_EDGE_G_LSEG_MAGIC:
			    NMG_UNIQ_INDEX(eu->g.lseg_p, edge_g_lseg);
			    break;
			case NMG_EDGE_G_CNURB_MAGIC:
			    NMG_UNIQ_INDEX(eu->g.cnurb_p, edge_g_cnurb);
			    break;
		    }
		vu = eu->vu_p;
		UNIQ_VU(vu);
	    }
	    /* Lone vertex in shell */
	    vu = s->vu_p;
	    if (vu) {
		ctr->shells_of_lone_vert++;
		UNIQ_VU(vu);
	    }
	}
    }
    /* Caller must free them */
    return ptrs;
#undef UNIQ_VU
}


/**
 *
 * Count 'em up, and print 'em out.
 */
void
nmg_pr_m_struct_counts(const struct model *m, const char *str)
{
    struct nmg_struct_counts cnts;
    uint32_t **tab;

    NMG_CK_MODEL(m);

    tab = nmg_m_struct_count(&cnts, m);
    nmg_free((void *)tab, "nmg_m_struct_count");
    nmg_pr_struct_counts(&cnts, str);
}


/**
 * Combine two NMG model trees into one single NMG model.  The
 * first model inherits the nmgregions of the second.  The second
 * model pointer is freed before return.
 */
void
nmg_merge_models(struct model *m1, struct model *m2)
{
    struct nmgregion *r;

    NMG_CK_MODEL(m1);
    NMG_CK_MODEL(m2);

    /* first reorder the first model to "compress" the
     * number space if possible.
     */
    nmg_m_reindex(m1, 0);

    if (m1 == m2) /* nothing to do */
	return;

    /* now re-order the second model starting with an index number
     * of m1->maxindex.
     *
     * We might get away with using m1->maxindex-1, since the first
     * value is assigned to the second model structure, and we will
     * shortly be freeing the second model struct.
     */

    nmg_m_reindex(m2, m1->maxindex);
    m1->maxindex = m2->maxindex;		/* big enough for both */

    /* Rehome all the regions in m2, and move them from m2 to m1 */
    for (BU_LIST_FOR(r, nmgregion, &(m2->r_hd))) {
	NMG_CK_REGION(r);
	r->m_p = m1;
    }
    BU_LIST_APPEND_LIST(&(m1->r_hd), &(m2->r_hd));

    /* If there are any manifold tables, when the models are
     * merged they become invalid and need to be regenerated.
     * To avoid confusion, free them here so it is known they
     * need to be regenerated.
     */
    if (m1->manifolds) {
	nmg_free((char *)m1->manifolds, "free manifolds table");
	m1->manifolds = (char *)NULL;
    }
    if (m2->manifolds) {
	nmg_free((char *)m2->manifolds, "free manifolds table");
	m2->manifolds = (char *)NULL;
    }

    FREE_MODEL(m2);
}


#define CHECK_INDEX(_p) if ((_p)->index > maxindex) maxindex = (_p)->index
#define CHECK_VU_INDEX(_vu) {\
	NMG_CK_VERTEXUSE(_vu); \
	CHECK_INDEX(_vu); \
	if (_vu->a.magic_p) switch (*_vu->a.magic_p) { \
		case NMG_VERTEXUSE_A_PLANE_MAGIC: \
		    CHECK_INDEX(_vu->a.plane_p); \
		    break; \
		case NMG_VERTEXUSE_A_CNURB_MAGIC: \
		    CHECK_INDEX(_vu->a.cnurb_p); \
		    break; \
	    } \
	v = _vu->v_p; \
	NMG_CK_VERTEX(v); \
	CHECK_INDEX(v); \
	if (v->vg_p) CHECK_INDEX(v->vg_p); \
    }

long
nmg_find_max_index(const struct model *m)
{
    long maxindex=0;
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;
    struct loopuse *lu;
    struct loop *l;
    register struct edgeuse *eu;
    struct edge *e;
    register struct vertexuse *vu;
    struct vertex *v;

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	CHECK_INDEX(r);
	if (r->ra_p) CHECK_INDEX(r->ra_p);
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    NMG_CK_SHELL(s);
	    CHECK_INDEX(s);
	    if (s->sa_p) CHECK_INDEX(s->sa_p);
	    /* Faces in shell */
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		CHECK_INDEX(fu);
		f = fu->f_p;
		NMG_CK_FACE(f);
		CHECK_INDEX(f);
		if (f->g.plane_p) switch (*f->g.magic_p) {
			case NMG_FACE_G_PLANE_MAGIC:
			    CHECK_INDEX(f->g.plane_p);
			    break;
			case NMG_FACE_G_SNURB_MAGIC:
			    CHECK_INDEX(f->g.snurb_p);
			    break;
		    }
		/* Loops in face */
		for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		    NMG_CK_LOOPUSE(lu);
		    CHECK_INDEX(lu);
		    l = lu->l_p;
		    NMG_CK_LOOP(l);
		    CHECK_INDEX(l);
		    if (l->lg_p) CHECK_INDEX(l->lg_p);
		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			/* Loop of Lone vertex */
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			CHECK_VU_INDEX(vu);
			continue;
		    }
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			CHECK_INDEX(eu);
			e = eu->e_p;
			NMG_CK_EDGE(e);
			CHECK_INDEX(e);
			if (eu->g.magic_p) switch (*eu->g.magic_p) {
				case NMG_EDGE_G_LSEG_MAGIC:
				    CHECK_INDEX(eu->g.lseg_p);
				    break;
				case NMG_EDGE_G_CNURB_MAGIC:
				    CHECK_INDEX(eu->g.cnurb_p);
				    break;
			    }
			vu = eu->vu_p;
			CHECK_VU_INDEX(vu);
		    }
		}
	    }
	    /* Wire loops in shell */
	    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		CHECK_INDEX(lu);
		l = lu->l_p;
		NMG_CK_LOOP(l);
		CHECK_INDEX(l);
		if (l->lg_p) CHECK_INDEX(l->lg_p);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		    /* Wire loop of Lone vertex */
		    vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		    CHECK_VU_INDEX(vu);
		    continue;
		}
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);
		    CHECK_INDEX(eu);
		    e = eu->e_p;
		    NMG_CK_EDGE(e);
		    CHECK_INDEX(e);
		    if (eu->g.magic_p) switch (*eu->g.magic_p) {
			    case NMG_EDGE_G_LSEG_MAGIC:
				CHECK_INDEX(eu->g.lseg_p);
				break;
			    case NMG_EDGE_G_CNURB_MAGIC:
				CHECK_INDEX(eu->g.cnurb_p);
				break;
			}
		    vu = eu->vu_p;
		    CHECK_VU_INDEX(vu);
		}
	    }
	    /* Wire edges in shell */
	    for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
		NMG_CK_EDGEUSE(eu);
		CHECK_INDEX(eu);
		e = eu->e_p;
		NMG_CK_EDGE(e);
		CHECK_INDEX(e);
		if (eu->g.magic_p) switch (*eu->g.magic_p) {
			case NMG_EDGE_G_LSEG_MAGIC:
			    CHECK_INDEX(eu->g.lseg_p);
			    break;
			case NMG_EDGE_G_CNURB_MAGIC:
			    CHECK_INDEX(eu->g.cnurb_p);
			    break;
		    }
		vu = eu->vu_p;
		CHECK_VU_INDEX(vu);
	    }
	    /* Lone vertex in shell */
	    vu = s->vu_p;
	    if (vu) {
		CHECK_VU_INDEX(vu);
	    }
	}
    }
    return maxindex;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
