/*                      N M G _ C O P Y . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
/** @file primitives/nmg/nmg_copy.c
 *
 * Support routine for n-Manifold Geometry:
 * Deep copy of NMG model structure
 *
 */
/** @} */

#include "common.h"

#include <string.h>

#include "nurb.h"
#include "raytrace.h"


static struct face_g_plane *
nmg_construct_face_g_plane(const struct face_g_plane *original, void **structArray)
{
    struct face_g_plane *ret;

    NMG_GETSTRUCT(ret, face_g_plane);

    ret->magic = NMG_FACE_G_PLANE_MAGIC;

    BU_LIST_INIT(&ret->f_hd);
    HMOVE(ret->N, original->N);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct face_g_snurb *
nmg_construct_face_g_snurb(const struct face_g_snurb *original, void **structArray)
{
    struct face_g_snurb *ret;

    NMG_GETSTRUCT(ret, face_g_snurb);

    ret->l.magic = NMG_FACE_G_SNURB_MAGIC;

    BU_LIST_INIT(&ret->f_hd);

    ret->order[0] = original->order[0];
    ret->order[1] = original->order[1];

    ret->u.magic  = NMG_KNOT_VECTOR_MAGIC;
    ret->u.k_size = original->u.k_size;
    ret->u.knots = (fastf_t *)bu_malloc(ret->u.k_size * sizeof(fastf_t),
					"nmg_construct_face_g_snurb(): u.knots");
    memcpy(ret->u.knots, original->u.knots, ret->u.k_size * sizeof(fastf_t));
    ret->v.magic  = NMG_KNOT_VECTOR_MAGIC;
    ret->v.k_size = original->v.k_size;
    ret->v.knots = (fastf_t *)bu_malloc(ret->v.k_size * sizeof(fastf_t),
					"nmg_construct_face_g_snurb(): v.knots");
    memcpy(ret->v.knots, original->v.knots, ret->v.k_size * sizeof(fastf_t));

    ret->s_size[0]  = original->s_size[0];
    ret->s_size[1]  = original->s_size[1];
    ret->pt_type    = original->pt_type;
    ret->ctl_points
	= (fastf_t *)bu_malloc(original->s_size[0] * original->s_size[1] * RT_NURB_EXTRACT_COORDS(ret->pt_type) * sizeof(fastf_t),
			       "nmg_construct_face_g_snurb(): ctl_points");
    memcpy(ret->ctl_points, original->ctl_points, original->s_size[0] * original->s_size[1] * RT_NURB_EXTRACT_COORDS(ret->pt_type) * sizeof(fastf_t));

    ret->dir = original->dir;
    VMOVE(ret->min_pt, original->min_pt);
    VMOVE(ret->max_pt, original->max_pt);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct face *
nmg_construct_face(struct faceuse *parent, const struct face *original, void **structArray)
{
    struct face *ret;

    NMG_GETSTRUCT(ret, face);

    ret->l.magic   = NMG_FACE_MAGIC;
    ret->fu_p      = parent;
    ret->g.magic_p = (uint32_t *)NULL;
    ret->flip      = original->flip;

    VMOVE(ret->min_pt, original->min_pt);
    VMOVE(ret->max_pt, original->max_pt);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    switch (*original->g.magic_p) {
	case NMG_FACE_G_PLANE_MAGIC:
	    ret->g.plane_p = (struct face_g_plane *)structArray[original->g.plane_p->index];

	    if (ret->g.plane_p == NULL)
		ret->g.plane_p = nmg_construct_face_g_plane(original->g.plane_p, structArray);

	    BU_LIST_INSERT(&ret->g.plane_p->f_hd, &ret->l);
	    break;

	case NMG_FACE_G_SNURB_MAGIC:
	    ret->g.snurb_p = (struct face_g_snurb *)structArray[original->g.plane_p->index];

	    if (ret->g.snurb_p == NULL)
		ret->g.snurb_p = nmg_construct_face_g_snurb(original->g.snurb_p, structArray);

	    BU_LIST_INSERT(&ret->g.snurb_p->f_hd, &ret->l);
    }

    return ret;
}


static struct vertex_g *
nmg_construct_vertex_g(const struct vertex_g *original, void **structArray)
{
    struct vertex_g *ret;

    NMG_GETSTRUCT(ret, vertex_g);

    ret->magic = NMG_VERTEX_G_MAGIC;

    VMOVE(ret->coord, original->coord);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct vertex *
nmg_construct_vertex(const struct vertex *original, void **structArray)
{
    struct vertex *ret;

    NMG_GETSTRUCT(ret, vertex);

    ret->magic = NMG_VERTEX_MAGIC;

    BU_LIST_INIT(&ret->vu_hd);

    ret->vg_p               = (struct vertex_g*)NULL;
    ret->index              = original->index;
    structArray[ret->index] = ret;

    if (original->vg_p != NULL) {
	ret->vg_p = (struct vertex_g *)structArray[original->vg_p->index];

	if (ret->vg_p == NULL)
	    ret->vg_p = nmg_construct_vertex_g(original->vg_p, structArray);
    }

    return ret;
}


static struct vertexuse_a_plane *
nmg_construct_vertexuse_a_plane(const struct vertexuse_a_plane *original, void **structArray)
{
    struct vertexuse_a_plane *ret;

    NMG_GETSTRUCT(ret, vertexuse_a_plane);

    ret->magic            = NMG_VERTEXUSE_A_PLANE_MAGIC;

    VMOVE(ret->N, original->N);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct vertexuse_a_cnurb *
nmg_construct_vertexuse_a_cnurb(const struct vertexuse_a_cnurb *original, void **structArray)
{
    struct vertexuse_a_cnurb *ret;

    NMG_GETSTRUCT(ret, vertexuse_a_cnurb);

    ret->magic = NMG_VERTEXUSE_A_CNURB_MAGIC;

    VMOVE(ret->param, original->param);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct vertexuse *
nmg_construct_vertexuse(void *parent, const struct vertexuse *original, void **structArray)
{
    struct vertexuse *ret;

    NMG_GETSTRUCT(ret, vertexuse);

    ret->l.magic            = NMG_VERTEXUSE_MAGIC;
    ret->up.magic_p         = (uint32_t *)parent;
    ret->v_p                = (struct vertex*)NULL;
    ret->a.magic_p          = NULL;
    ret->index              = original->index;
    structArray[ret->index] = ret;

    if (original->v_p != NULL) {
	ret->v_p = (struct vertex *)structArray[original->v_p->index];

	if (ret->v_p == NULL)
	    ret->v_p = nmg_construct_vertex(original->v_p, structArray);

	BU_LIST_INSERT(&ret->v_p->vu_hd, &(ret->l));
    }

    if (original->a.magic_p != NULL) {
	switch (*original->a.magic_p) {
	    case NMG_VERTEXUSE_A_PLANE_MAGIC:
		ret->a.plane_p = (struct vertexuse_a_plane *)structArray[original->a.plane_p->index];
		if (ret->a.plane_p == NULL)
		    ret->a.plane_p = nmg_construct_vertexuse_a_plane(original->a.plane_p, structArray);
		break;
	    case NMG_VERTEXUSE_A_CNURB_MAGIC:
		ret->a.cnurb_p = (struct vertexuse_a_cnurb *)structArray[original->a.cnurb_p->index];
		if (ret->a.cnurb_p == NULL)
		    ret->a.cnurb_p = nmg_construct_vertexuse_a_cnurb(original->a.cnurb_p, structArray);
		break;
	    default:
		/* FIXME: any more cases? any action to take? */
		break;
	}
    }

    return ret;
}


static struct edge *
nmg_construct_edge(struct edgeuse *parent, const struct edge *original, void **structArray)
{
    struct edge *ret;

    NMG_GETSTRUCT(ret, edge);

    ret->magic              = NMG_EDGE_MAGIC;
    ret->eu_p               = parent;
    ret->is_real            = original->is_real;
    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct edge_g_lseg *
nmg_construct_edge_g_lseg(const struct edge_g_lseg *original, void **structArray)
{
    struct edge_g_lseg *ret;

    NMG_GETSTRUCT(ret, edge_g_lseg);

    ret->l.magic = NMG_EDGE_G_LSEG_MAGIC;

    BU_LIST_INIT(&ret->eu_hd2);

    VMOVE(ret->e_pt, original->e_pt);
    VMOVE(ret->e_dir, original->e_dir);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct edge_g_cnurb *
nmg_construct_edge_g_cnurb(const struct edge_g_cnurb *original, void **structArray)
{
    struct edge_g_cnurb *ret;

    NMG_GETSTRUCT(ret, edge_g_cnurb);

    ret->l.magic = NMG_EDGE_G_CNURB_MAGIC;

    BU_LIST_INIT(&ret->eu_hd2);

    ret->order = original->order;

    ret->k.magic  = NMG_KNOT_VECTOR_MAGIC;
    ret->k.k_size = original->k.k_size;
    ret->k.knots = (fastf_t *)bu_malloc(ret->k.k_size * sizeof(fastf_t), "nmg_construct_edge_g_cnurb(): k.knots");
    memcpy(ret->k.knots, original->k.knots, ret->k.k_size * sizeof(fastf_t));

    ret->c_size     = original->c_size;
    ret->pt_type    = original->pt_type;
    ret->ctl_points = (fastf_t *)bu_malloc(ret->c_size * RT_NURB_EXTRACT_COORDS(ret->pt_type) * sizeof(fastf_t),
					   "nmg_construct_edge_g_cnurb(): ctl_points");
    memcpy(ret->ctl_points, original->ctl_points, ret->c_size * RT_NURB_EXTRACT_COORDS(ret->pt_type) * sizeof(fastf_t));

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct edgeuse *
nmg_construct_edgeuse(void *parent, const struct edgeuse *original, void **structArray)
{
    struct edgeuse *ret;

    NMG_GETSTRUCT(ret, edgeuse);

    ret->l.magic = NMG_EDGEUSE_MAGIC;

    BU_LIST_INIT(&ret->l2);
    ret->l2.magic = NMG_EDGEUSE2_MAGIC;

    ret->up.magic_p         = (uint32_t *)parent;
    ret->eumate_p           = (struct edgeuse*)NULL;
    ret->radial_p           = (struct edgeuse*)NULL;
    ret->e_p                = (struct edge*)NULL;
    ret->orientation        = original->orientation;
    ret->vu_p               = (struct vertexuse*) NULL;
    ret->g.magic_p          = NULL;
    ret->index              = original->index;
    structArray[ret->index] = ret;

    if (original->eumate_p != NULL) {
	ret->eumate_p = (struct edgeuse *)structArray[original->eumate_p->index];

	/* because it's tricky to choose the right parent for the mate
	 * wait until it's created and set eumate_p afterwards
	 */
	if (ret->eumate_p != NULL)
	    ret->eumate_p->eumate_p = ret;
    }

    if (original->radial_p != NULL) {
	ret->radial_p = (struct edgeuse *)structArray[original->radial_p->index];

	/* because it's tricky to choose the right parent wait until
	 * it's created and set it afterwards
	 */
	if (ret->radial_p != NULL)
	    ret->radial_p->radial_p = ret;
    }

    if (original->e_p != NULL) {
	ret->e_p = (struct edge *)structArray[original->e_p->index];

	if (ret->e_p == 0)
	    ret->e_p = nmg_construct_edge(ret, original->e_p, structArray);
    }

    if (original->vu_p != NULL) {
	ret->vu_p = (struct vertexuse *)structArray[original->vu_p->index];

	if (ret->vu_p == 0)
	    ret->vu_p = nmg_construct_vertexuse(ret, original->vu_p, structArray);
    }

    if (original->g.magic_p != NULL) {
	switch (*original->g.magic_p) {
	    case NMG_EDGE_G_LSEG_MAGIC:
		ret->g.lseg_p = (struct edge_g_lseg *)structArray[original->g.lseg_p->index];
		if (ret->g.lseg_p == NULL)
		    ret->g.lseg_p = nmg_construct_edge_g_lseg(original->g.lseg_p, structArray);
		BU_LIST_INSERT(&ret->g.lseg_p->eu_hd2, &(ret->l2));
		break;
	    case NMG_EDGE_G_CNURB_MAGIC:
		ret->g.cnurb_p = (struct edge_g_cnurb *)structArray[original->g.cnurb_p->index];
		if (ret->g.cnurb_p == NULL)
		    ret->g.cnurb_p = nmg_construct_edge_g_cnurb(original->g.cnurb_p, structArray);
		BU_LIST_INSERT(&ret->g.cnurb_p->eu_hd2, &(ret->l2));
		break;
	    default:
		/* FIXME: any more cases? any action to take? */
		break;
	}
    }

    return ret;
}


static struct loop_g *
nmg_construct_loop_g(const struct loop_g *original, void **structArray)
{
    struct loop_g *ret;

    NMG_GETSTRUCT(ret, loop_g);

    ret->magic = NMG_LOOP_G_MAGIC;

    VMOVE(ret->min_pt, original->min_pt);
    VMOVE(ret->max_pt, original->max_pt);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct loop *
nmg_construct_loop(struct loopuse *parent, const struct loop *original, void **structArray)
{
    struct loop *ret;

    NMG_GETSTRUCT(ret, loop);

    ret->magic              = NMG_LOOP_MAGIC;
    ret->lu_p               = parent;
    ret->lg_p               = (struct loop_g*)NULL;
    ret->index              = original->index;
    structArray[ret->index] = ret;

    if (original->lg_p != NULL) {
	ret->lg_p = (struct loop_g *)structArray[original->lg_p->index];

	if (ret->lg_p == NULL)
	    ret->lg_p = nmg_construct_loop_g(original->lg_p, structArray);
    }

    return ret;
}


static struct loopuse *
nmg_construct_loopuse(void *parent, const struct loopuse *original, void **structArray)
{
    struct loopuse *ret;

    NMG_GETSTRUCT(ret, loopuse);

    ret->l.magic     = NMG_LOOPUSE_MAGIC;
    ret->up.magic_p  = (uint32_t *)parent;
    ret->lumate_p    = (struct loopuse *)NULL;
    ret->orientation = original->orientation;
    ret->l_p         = (struct loop *)NULL;

    BU_LIST_INIT(&ret->down_hd);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    if (original->lumate_p != NULL) {
	ret->lumate_p = (struct loopuse *)structArray[original->lumate_p->index];

	/* because it's tricky to choose the right parent for the mate
	 * wait until it's created and set eumate_p afterwards
	 */
	if (ret->lumate_p != NULL)
	    ret->lumate_p->lumate_p = ret;
    }

    if (original->l_p != NULL) {
	ret->l_p = (struct loop *)structArray[original->l_p->index];

	if (ret->l_p == 0)
	    ret->l_p = nmg_construct_loop(ret, original->l_p, structArray);
    }

    switch (BU_LIST_FIRST_MAGIC(&original->down_hd)) {
	case NMG_VERTEXUSE_MAGIC: {
	    const struct vertexuse *originalVertexUse = BU_LIST_FIRST(vertexuse, &original->down_hd);
	    struct vertexuse       *newVertexUse      = (struct vertexuse *)structArray[originalVertexUse->index];

	    if (newVertexUse == NULL)
		newVertexUse = nmg_construct_vertexuse(ret, originalVertexUse, structArray);

	    BU_LIST_INSERT(&ret->down_hd, &newVertexUse->l);
	}
	    break;
	case NMG_EDGEUSE_MAGIC: {
	    const struct edgeuse *originalEdgeUse;
	    for (BU_LIST_FOR(originalEdgeUse, edgeuse, &original->down_hd)) {
		struct edgeuse *newEdgeUse = (struct edgeuse *)structArray[originalEdgeUse->index];
		if (newEdgeUse == NULL)
		    newEdgeUse = nmg_construct_edgeuse(ret, originalEdgeUse, structArray);
		BU_LIST_INSERT(&ret->down_hd, &newEdgeUse->l);
	    }
	}
	    break;
	default:
	    /* FIXME: any more cases? any action to take? */
	    break;
    }

    return ret;
}


static struct faceuse *
nmg_construct_faceuse(struct shell *parent, const struct faceuse *original, void **structArray)
{
    struct faceuse       *ret;
    const struct loopuse *originalLoopUse;

    NMG_GETSTRUCT(ret, faceuse);

    ret->l.magic     = NMG_FACEUSE_MAGIC;
    ret->s_p         = parent;
    ret->fumate_p    = (struct faceuse *)NULL;
    ret->orientation = original->orientation;
    ret->outside     = original->outside;
    ret->f_p         = (struct face *)NULL;

    BU_LIST_INIT(&ret->lu_hd);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    if (original->fumate_p != NULL) {
	ret->fumate_p = (struct faceuse *)structArray[original->fumate_p->index];

	if (ret->fumate_p == NULL)
	    ret->fumate_p = nmg_construct_faceuse(parent, original->fumate_p, structArray);
    }

    if (original->f_p != NULL) {
	ret->f_p = (struct face *)structArray[original->f_p->index];

	if (ret->f_p == 0)
	    ret->f_p = nmg_construct_face(ret, original->f_p, structArray);
    }

    for (BU_LIST_FOR(originalLoopUse, loopuse, &original->lu_hd)) {
	struct loopuse *newLoopUse = (struct loopuse *)structArray[originalLoopUse->index];

	if (newLoopUse == NULL)
	    newLoopUse = nmg_construct_loopuse(ret, originalLoopUse, structArray);

	BU_LIST_INSERT(&ret->lu_hd, &newLoopUse->l);
    }

    return ret;
}


static struct shell_a *
nmg_construct_shell_a(const struct shell_a *original, void **structArray)
{
    struct shell_a *ret;

    NMG_GETSTRUCT(ret, shell_a);

    ret->magic = NMG_SHELL_A_MAGIC;

    VMOVE(ret->min_pt, original->min_pt);
    VMOVE(ret->max_pt, original->max_pt);

    ret->index              = original->index;
    structArray[ret->index] = ret;

    return ret;
}


static struct shell *
nmg_construct_shell(const struct shell *original)
{
    struct shell         *ret;
    void             **structArray;
    const struct faceuse *originalFaceUse;
    const struct loopuse *originalLoopUse;
    const struct edgeuse *originalEdgeUse;

    structArray = (genptr_t*)bu_calloc(original->maxindex, sizeof(genptr_t), "nmg_clone_shell() structArray");
    ret = nmg_ms();

    ret->magic = NMG_SHELL_MAGIC;
    ret->sa_p    = (struct shell_a *)NULL;

    BU_LIST_INIT(&ret->fu_hd);
    BU_LIST_INIT(&ret->lu_hd);
    BU_LIST_INIT(&ret->eu_hd);

    ret->vu_p               = (struct vertexuse *) NULL;
    ret->index              = original->index;
    ret->maxindex = original->maxindex;
    structArray[ret->index] = ret;

    if (original->sa_p != NULL) {
	const struct shell_a *originalAttributes = original->sa_p;
	struct shell_a       *newAttributes      = (struct shell_a *)structArray[originalAttributes->index];

	if (newAttributes == NULL)
	    newAttributes = nmg_construct_shell_a(originalAttributes, structArray);

	ret->sa_p = newAttributes;
    }

    for (BU_LIST_FOR(originalFaceUse, faceuse, &original->fu_hd)) {
	struct faceuse *newFaceUse = (struct faceuse *)structArray[originalFaceUse->index];

	if (newFaceUse == NULL)
	    newFaceUse = nmg_construct_faceuse(ret, originalFaceUse, structArray);

	BU_LIST_INSERT(&ret->fu_hd, &newFaceUse->l);
    }

    for (BU_LIST_FOR(originalLoopUse, loopuse, &original->lu_hd)) {
	struct loopuse *newLoopUse = (struct loopuse *)structArray[originalLoopUse->index];

	if (newLoopUse == NULL)
	    newLoopUse = nmg_construct_loopuse(ret, originalLoopUse, structArray);

	BU_LIST_INSERT(&ret->lu_hd, &newLoopUse->l);
    }

    for (BU_LIST_FOR(originalEdgeUse, edgeuse, &original->eu_hd)) {
	struct edgeuse *newEdgeUse = (struct edgeuse *)structArray[originalEdgeUse->index];

	if (newEdgeUse == NULL)
	    newEdgeUse = nmg_construct_edgeuse(ret, originalEdgeUse, structArray);

	BU_LIST_INSERT(&ret->eu_hd, &newEdgeUse->l);
    }

    if (original->vu_p != 0) {
	ret->vu_p = (struct vertexuse *)structArray[original->vu_p->index];

	if (ret->vu_p == NULL)
	    ret->vu_p = nmg_construct_vertexuse(ret, original->vu_p, structArray);
    }

    bu_free(structArray, "nmg_clone_shell() structArray");

    return ret;
}


/**
 * Makes a deep copy of a NMG shell structure.
 */
struct shell *
nmg_clone_shell(const struct shell *original)
{
    return nmg_construct_shell(original);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
