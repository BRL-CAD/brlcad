/*                        N M G _ M K . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file primitives/nmg/nmg_mk.c
 *
 * Support routines for n-Manifold Geometry.
 *
 * Naming convention
 * nmg_m* routines "make" NMG structures.
 * nmg_k* routines "kill" (delete) NMG structures.
 *
 * in each of the above cases the letters or words following are an
 * attempt at a mnemonic representation for what is manipulated
 *
 * m	Model
 * r	Region
 * s	shell
 * f	face
 * fu	faceuse
 * l	loop
 * lu	loopuse
 * e	edge
 * eu	edgeuse
 * v	vertex
 * vu	vertexuse
 *
 *
 * Rules:
 *
 * edges of loops of the same face must not overlap
 * XXX - What does "overlap" mean? ctj
 * the "magic" member of each struct is the first item.
 *
 * All routines which create and destroy the NMG data structures are
 * contained in this module.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"


/************************************************************************
 *									*
 *			"Make" Routines					*
 *									*
 * The subroutines create new topological entities and return a		*
 * pointer to the new entity.						*
 *									*
 ************************************************************************/


/**
 *  The nmg_m*() routines are used to create a topology-only object
 * which at first has no geometry associated with it at all.
 * A topology-only object can be used to answer questions like:
 *	is this vertex ON this shell?
 *	is this vertex ON this face?
 *	is this vertex ON this edge?
 *	Is this face ON this shell?
 * and many more.
 *
 * After the topology has been built, most applications will proceed to
 * associate geometry with the topology, primarily by supplying
 * Cartesian coordinates for each struct vertex, and by supplying
 * or computing face normals for the planar faces.  (Linear edge geometry
 * is optional, as it is implicit from the vertex geometry).
 *
 * Objects which have been fully populated with geometry can be used to
 * answer questions about where things are located and how large they are.
 *
 * The abstract objects are:
 *	model, nmgregion, shell, face, loop, edge and vertex.
 * The uses of those objects are:
 * 	faceuse, loopuse, edgeuse and vertexuse.
 * Geometry can optionally be associated with the abstract objects:
 *	face_g		(plane equation, bounding box)
 *	loop_g		(just a bounding box, for planar faces)
 *	edge_g		(to track edge subdivision heritage, for linear edges)
 *	vertex_g	(Cartesian coordinates)
 * The uses of those objects can optionally have attributes:
 *	nmgregion_a	(region bounding box)	[nmgregions have no uses]
 *	shell_a		(shell bounding box)	[shells have no uses]
 *	vertexuse_a	(special surface normal, for normal interpolation)
 *
 * Consider for example a simple cube.
 *
 * As a topology-only object, it would have the following structures:
 *
 * 1 model structure
 *		This is the handle which everything else hangs on.
 *		The model structure r_hd references 1 region structure.
 * 1 nmgregion structure.
 *		The region structure s_hd references 1 shell structure.
 *		Also, m_p references the model.
 * 1 shell structure.
 *		The shell structure fu_hd references 12 faceuse structures.
 *		One for each side of each of the 6 faces of the cube.
 *		Also, r_p references the nmgregion.
 * 12 faceuse structures.
 *		Each faceuse structure lu_hd references 1 loopuse structure.
 *		Also, 1 face structure and 1 faceuse structure (its mate),
 *		plus s_p references the shell.
 * 6 face structures.
 *		Each face structure fu_p references a random choice of 1 of
 *		the two parent faceuse structures sharing this face, and is
 *		in turn referenced by that faceuse and its mate.
 * 12 loopuse structures
 *		Each loopuse structure down_hd references 4 edgeuse structures.
 *		Also, 1 loop structure, and 1 loopuse structure (its mate).
 *		The four edgeuse structures define the perimeter of the
 *		surface area that comprises this face.
 *		Because their orientation is OT_SAME, each loopuse "claims"
 *		all the surface area inside it for the face.
 *		(OT_OPPOSITE makes a hole, claiming surface area outside).
 *		Plus, "up" references the parent object (faceuse, here).
 * 6 loop structures
 *		Each loop structure references a random choice of 1 of its
 *		parent loopuse structures and is in turn referenced by that
 *		loopuse and its mate.
 * 48 edgeuse structures
 *		Each edgeuse structure references 1 vertexuse structure,
 *		1 edge structure, and 2 other edgeuse structures (its mate
 *		and the next edgeuse radial to this edgeuse).
 *		(if this edge was NOT used in another face, then the
 *		radial pointer and mate pointer would point to the SAME
 *		edgeuse)
 *		To find all edgeuses around a given edge, follow radial to
 *		mate to radial to mate until you are back to the original
 *		edgeuse.
 *		Plus, "up" references the parent object (loopuse, here).
 * 12 edge structures
 *		Each edge structure references a random choice of one of
 *		its parent edgeuse structures and is in turn referenced
 *		by that edgeuse, the mate of that edgeuse, the radial of
 *		that edgeuse and the mate of the radial. (In this simple
 *		case of the cube, there are 4 edgeuses per edge).
 * 48 vertexuse structures.
 *		Each vertexuse structure references one vertex structure
 *		and is in turn enroled as a member of the linked list
 *		headed by that vertex structure.
 *		Each vertexuse is cited by exactly one edgeuse.
 *		Also, "up" references the parent object (edgeuse, here).
 * 8 vertex structures
 *		Each vertex structure references 6 vertexuse structures
 *		via its linked list. (In the case of the cube,
 *		there are three faces meeting at each vertex, and each of
 *		those faces has two faceuse structs of one loopuse each. Each
 *		loopuse will cite the vertex via one edgeuse, so 3*2 = 6).
 *
 * As well as each "use" pointing down to what it uses, the "use" points
 * up to the structure that uses it.  So working up from any abstract object
 * or its use, the top of the tree (struct model) can be found.
 * Working down from the struct model, all elements of the object can be
 * visited.
 *
 * The object just described contains no geometry.  There is no way to tell
 * where in space the object lies or how big it is.
 *
 * To add geometry, the following structures are needed:
 * 8 vertex_g structures
 *		each vertex_g structure contains a point in space (point_t)
 *		and is referenced by 1 vertex structure.
 * 12 edge_g structures (completely optional)
 *		each edge_g structure contains the parametric definition of
 *		the line which contains the line segment which is the edge,
 *		given as a point in space and a direction vector.  It is
 *		referenced by 1 edge structure. (In general, it is referenced
 *		by all edges sharing that line).
 *		In a simple case the point would be the same as one of
 *		the vertex_g points and the direction would be the normalized
 *		(unit) vector of the difference between the two vertex_g points.
 * 6 loop_g structures
 *		Each loop_g structure contains a bounding box for the loop.
 *		It is referenced by 1 loop structure.
 * 6 face_g_plane structures
 *		Each face_g_plane structure contains a plane equation and
 *		a bounding box.  It is referenced by one face structure.
 *		The plane equation is calculated from the vertex_g data
 *		by nmg_fu_planeeqn().
 *		See h/vmath.h for the definition of a plane equation.
 * 1 shell_a structure
 *		Each shell_a structure contains the bounding box for the
 *		shell.  It is referenced by 1 shell structure.
 * 1 nmgregion_a structure
 *		Each nmgregion_a structure contains the bounding box for the
 *		region.  It is referenced by 1 region structure.
 *
 */


/**
 * n m g _ m m
 *
 * Make Model
 *
 * Create a new model.  The region list is empty.  Creates a new model
 * structure.  The model region structure list is empty.
 *
 * Returns -
 * (struct model *)
 *
 * Method:
 *
 * Use BU_GET() to allocate memory and then set all components.
 * BU_GET() is used instead of the standard GET_*() macros because
 * they all expect a model pointer to get the maxindex from.  Here we
 * simply set maxindex to 1.
 *
 * N.B.:
 *
 * "maxindex" is a misnomer.  It is the value of the NEXT index
 * assigned.  This allows "ptab"s to be allocated easly using maxindex
 * and the index value of the structures to be the actual index into
 * the "ptab".
 */
struct model *
nmg_mm(void)
{
    struct model *m;

    BU_GET(m, struct model);

    BU_LIST_INIT(&m->r_hd);
    m->index = 0;
    m->maxindex = 1;
    m->magic = NMG_MODEL_MAGIC;	/* Model Structure is GOOD */

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_mm() returns model 0x%p\n", m);
    }

    return m;
}


/**
 * n m g _ m m r
 *
 * Make Model and Region.
 *
 * Create a new model, and an "empty" region to go with it.
 * Essentially this creates a minimal model system.
 *
 * Returns -
 * (struct model *)
 *
 * Implicit Return -
 * The new region is found with BU_LIST_FIRST(nmgregion, &m->r_hd);
 */
struct model *
nmg_mmr(void)
{
    struct model *m;
    struct nmgregion *r;

    m = nmg_mm();
    GET_REGION(r, m);

    r->m_p = m;

    r->ra_p = (struct nmgregion_a *)NULL;
    BU_LIST_INIT(&r->s_hd);
    r->l.magic = NMG_REGION_MAGIC;	/* Region Structure is GOOD */

    BU_LIST_APPEND(&m->r_hd, &r->l);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_mmr() returns model 0x%p with region 0x%p\n", m, r);
    }

    return m;
}


/**
 * n m g _ m r s v
 *
 * Make new region, shell, vertex in model as well as the required
 * "uses".  Create a new region in model consisting of a minimal
 * shell.
 *
 * Returns -
 * (struct nmgregion *)
 *
 * Implicit Returns -
 * Region is also found with r=BU_LIST_FIRST(nmgregion, &m->r_hd);
 * The new shell is found with s=BU_LIST_FIRST(shell, &r->s_hd);
 * The new vertexuse is s->vu_p;
 */
struct nmgregion *
nmg_mrsv(struct model *m)
{
    struct nmgregion *r;

    NMG_CK_MODEL(m);

    GET_REGION(r, m);
    r->m_p = m;
    r->ra_p = (struct nmgregion_a *) NULL;

    BU_LIST_INIT(&r->s_hd);
    r->l.magic = NMG_REGION_MAGIC;	/* Region struct is GOOD */

    (void)nmg_msv(r);

    /* new region goes at "head" of list of regions in model */
    BU_LIST_APPEND(&m->r_hd, &r->l);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_mrsv(m=0x%p) returns r=0x%p\n", m, r);
    }

    return r;
}


/**
 * n m g _ m v u
 *
 * Make Vertexuse on existing vertex
 *
 * This is a support routine for this module, and is not intended for
 * general use, as it requires lots of cooperation from the caller.
 * (Like setting the parent's down pointer appropriately).
 *
 * This means that a vu is created but is not attached to the parent
 * structure.  This is "bad" and requires the caller to fix.
 *
 * upptr is a pointer to parent struct
 */
static struct vertexuse *
nmg_mvu(struct vertex *v, uint32_t *upptr, struct model *m)
{
    struct vertexuse *vu;

    NMG_CK_VERTEX(v);
    NMG_CK_MODEL(m);

    if (*upptr != NMG_SHELL_MAGIC &&
	*upptr != NMG_LOOPUSE_MAGIC &&
	*upptr != NMG_EDGEUSE_MAGIC) {
	bu_log("nmg_mvu() in %s:%d magic not shell, loop, or edge.  Was 0x%lx (%s)\n",
	       __FILE__, __LINE__,
	       *upptr, bu_identify_magic(*upptr));
	bu_bomb("nmg_mvu() Cannot build vertexuse without parent\n");
    }

    GET_VERTEXUSE(vu, m);

    vu->v_p = v;
    vu->a.plane_p = (struct vertexuse_a_plane *)NULL;
    BU_LIST_APPEND(&v->vu_hd, &vu->l);
    vu->up.magic_p = upptr;
    vu->l.magic = NMG_VERTEXUSE_MAGIC;	/* Vertexuse struct is GOOD */

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_mvu(v=0x%p, up=0x%p) returns vu=0x%p\n",
	       v, upptr, vu);
    }
    return vu;
}


/**
 * n m g _ m v v u
 *
 * Make Vertex, Vertexuse
 *
 * This is a support routine for this module, and is not intended for
 * general use, as it requires lots of cooperation from the caller.
 * (Like setting the parent's down pointer appropriately).
 *
 * This means that a vu is created but is not attached to the parent
 * structure.  This is "bad" and requires the caller to fix.
 */
static struct vertexuse *
nmg_mvvu(uint32_t *upptr, struct model *m)
{
    struct vertex *v;
    struct vertexuse *ret_vu;

    NMG_CK_MODEL(m);
    GET_VERTEX(v, m);
    BU_LIST_INIT(&v->vu_hd);
    v->vg_p = (struct vertex_g *)NULL;
    v->magic = NMG_VERTEX_MAGIC;	/* Vertex struct is GOOD */
    ret_vu = nmg_mvu(v, upptr, m);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_mvvu(upptr=0x%p, m=0x%p) returns vu=0x%p\n", upptr, m, ret_vu);
    }

    return ret_vu;
}


/**
 * n m g _ m s v
 *
 * Make Shell, Vertex Use, Vertex
 *
 * Create a new shell in a specified region.  The shell will consist
 * of a single vertexuse and vertex (which are also created).
 *
 * Returns -
 * (struct shell *)
 *
 * Implicit Returns -
 * The new shell is also found with s=BU_LIST_FIRST(shell, &r->s_hd);
 * The new vertexuse is s->vu_p;
 */
struct shell *
nmg_msv(struct nmgregion *r)
{
    struct shell *s;
    struct vertexuse *vu;

    NMG_CK_REGION(r);

    /* set up shell */
    GET_SHELL(s, r->m_p);

    s->r_p = r;
    BU_LIST_APPEND(&r->s_hd, &s->l);

    s->sa_p = (struct shell_a *)NULL;
    BU_LIST_INIT(&s->fu_hd);
    BU_LIST_INIT(&s->lu_hd);
    BU_LIST_INIT(&s->eu_hd);
    s->vu_p = (struct vertexuse *) NULL;
    s->l.magic = NMG_SHELL_MAGIC;	/* Shell Struct is GOOD */

    vu = nmg_mvvu(&s->l.magic, r->m_p);
    s->vu_p = vu;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_msv(r=0x%p) returns s=0x%p, vu=0x%p\n", r, s, s->vu_p);
    }

    return s;
}


/**
 * n m g _ m f
 *
 * Make Face from a wire loop.
 *
 * make a face from a pair of loopuses.  The loopuses must be direct
 * children of a shell.  The new face will be a child of the same
 * shell.
 *
 * Given a wire loop (by definition, a loop attached to a shell),
 * create a new face, faceuse (and mate) and move the wire loop from
 * the shell to the new faceuse (and mate).
 *
 * Implicit Returns -
 * The first faceuse is fu1=BU_LIST_FIRST(faceuse, &s->fu_hd);
 * The second faceuse follows:  fu2=BU_LIST_NEXT(faceuse, &fu1->l.magic);
 */
struct faceuse *
nmg_mf(struct loopuse *lu1)
{
    struct face *f;
    struct faceuse *fu1, *fu2;
    struct loopuse *lu2;
    struct shell *s;
    struct model *m;

    NMG_CK_LOOPUSE(lu1);
    if (*lu1->up.magic_p != NMG_SHELL_MAGIC) {
	bu_bomb("nmg_mf() loop must be child of shell for making face\n");
    }
    lu2 = lu1->lumate_p;
    NMG_CK_LOOPUSE(lu2);
    if (lu2->up.magic_p != lu1->up.magic_p) {
	bu_bomb("nmg_mf() loopuse mate does not have same parent\n");
    }

    s = lu1->up.s_p;
    NMG_CK_SHELL(s);

    m = nmg_find_model(&s->l.magic);
    GET_FACE(f, m);
    GET_FACEUSE(fu1, m);
    GET_FACEUSE(fu2, m);

    f->fu_p = fu1;
    f->g.plane_p = (struct face_g_plane *)NULL;
    f->flip = 0;
    BU_LIST_INIT(&f->l);
    f->l.magic = NMG_FACE_MAGIC;	/* Face struct is GOOD */

    BU_LIST_INIT(&fu1->lu_hd);
    BU_LIST_INIT(&fu2->lu_hd);
    fu1->s_p = fu2->s_p = s;
    fu1->fumate_p = fu2;
    fu2->fumate_p = fu1;
    fu1->orientation = fu2->orientation = OT_UNSPEC;
    fu1->f_p = fu2->f_p = f;
    fu1->l.magic =
	fu2->l.magic = NMG_FACEUSE_MAGIC; /* Faceuse structs are GOOD */

    /* move the loopuses from the shell to the faceuses */
    BU_LIST_DEQUEUE(&lu1->l);
    BU_LIST_DEQUEUE(&lu2->l);
    BU_LIST_APPEND(&fu1->lu_hd, &lu1->l);
    BU_LIST_APPEND(&fu2->lu_hd, &lu2->l);

    lu1->up.fu_p = fu1;
    lu1->orientation = OT_SAME;
    lu2->up.fu_p = fu2;
    lu2->orientation = OT_SAME;

    /* connect the faces to the parent shell:  head, fu1, fu2... */
    BU_LIST_APPEND(&s->fu_hd, &fu1->l);
    BU_LIST_APPEND(&fu1->l, &fu2->l);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_mf(lu1=0x%p) returns fu=0x%p\n", lu1, fu1);
    }

    return fu1;
}


/**
 * n m g _ m l v
 *
 * Make a new loop (with specified orientation) and vertex, in a shell
 * or face.
 * XXX - vertex or vertexuse? or both? ctj
 *
 * If the vertex 'v' is NULL, the shell's lone vertex is used, or a
 * new vertex is created.
 *
 * "magic" must point to the magic number of a faceuse or shell.
 *
 * If the shell has a lone vertex in it, that lone vertex *will* be
 * used.  If a non-NULL 'v' is provided, the lone vertex and 'v' will
 * be fused together.  XXX Why is this good?
 *
 * If a convenient shell does not exist, use s=nmg_msv() to make the
 * shell and vertex, then call lu=nmg_mlv(s, s->vu_p->v_p, OT_SAME),
 * followed by nmg_kvu(s->vu_p).
 *
 * Implicit returns -
 * The new vertexuse can be had by:
 * BU_LIST_FIRST(vertexuse, &lu->down_hd);
 *
 * In case the returned loopuse isn't retained, the new loopuse was
 * inserted at the +head+ of the appropriate list, e.g.:
 * lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
 * or
 * lu = BU_LIST_FIRST(loopuse, &s->lu_hd);
 *
 * N.B.  This function is made more complex than warrented by using
 * the "hack" of stealing a vertexuse structure from the shell if at
 * all possible.  A future enhancement to this function would be to
 * remove the vertexuse steal and have the caller pass in the vertex
 * from the shell followed by a call to nmg_kvu(s->vu_p).  The v==NULL
 * convention is used only in nmg_mod.c.
 */
struct loopuse *
nmg_mlv(uint32_t *magic, struct vertex *v, int orientation)
{
    struct loop *l;
    struct loopuse *lu1, *lu2;
    struct vertexuse *vu1 = NULL;
    struct vertexuse *vu2;
    struct model *m;
    /* XXX - why the new union? ctj */
    union {
	struct shell *s;
	struct faceuse *fu;
	uint32_t *magic_p;
    } p;

    p.magic_p = magic;

    if (v) {
	NMG_CK_VERTEX(v);
    }

    m = nmg_find_model(magic);
    GET_LOOP(l, m);
    GET_LOOPUSE(lu1, m);
    GET_LOOPUSE(lu2, m);

    l->lg_p = (struct loop_g *)NULL;
    l->lu_p = lu1;
    l->magic = NMG_LOOP_MAGIC;	/* Loop struct is GOOD */

    BU_LIST_INIT(&lu1->down_hd);
    BU_LIST_INIT(&lu2->down_hd);
    lu1->l_p = lu2->l_p = l;
    lu1->orientation = lu2->orientation = orientation;

    lu1->lumate_p = lu2;
    lu2->lumate_p = lu1;

    /* The only thing left to do to make the loopuses good is to */
    /* set the "up" pointer and "l.magic". */
    if (*p.magic_p == NMG_SHELL_MAGIC) {
	struct shell *s = p.s;

	/* First, finish setting up the loopuses */
	lu1->up.s_p = lu2->up.s_p = s;

	lu1->l.magic = lu2->l.magic =
	    NMG_LOOPUSE_MAGIC;	/* Loopuse structs are GOOD */

	BU_LIST_INSERT(&s->lu_hd, &lu1->l);
	BU_LIST_INSERT(&lu1->l, &lu2->l);

	/* Second, build the vertices */
	/* This "if" degenerates to the "else" clause if no stealing */
	vu1 = s->vu_p;
	if (vu1) {
	    /* Use shell's lone vertex */
	    s->vu_p = (struct vertexuse *)NULL;
	    vu1->up.lu_p = lu1;
	    if (v) nmg_movevu(vu1, v);
	} else {
	    if (v) vu1 = nmg_mvu(v, &lu1->l.magic, m);
	    else vu1 = nmg_mvvu(&lu1->l.magic, m);
	}
	NMG_CK_VERTEXUSE(vu1);
	RT_LIST_SET_DOWN_TO_VERT(&lu1->down_hd, vu1);
	/* vu1->up.lu_p = lu1; done by nmg_mvu/nmg_mvvu */

	vu2 = nmg_mvu(vu1->v_p, &lu2->l.magic, m);
	NMG_CK_VERTEXUSE(vu2);
	RT_LIST_SET_DOWN_TO_VERT(&lu2->down_hd, vu2);
	/* vu2->up.lu_p = lu2; done by nmg_mvu() */
    } else if (*p.magic_p == NMG_FACEUSE_MAGIC) {
	/* First, finish setting up the loopuses */
	lu1->up.fu_p = p.fu;
	lu2->up.fu_p = p.fu->fumate_p;
	lu1->l.magic = lu2->l.magic =
	    NMG_LOOPUSE_MAGIC;	/* Loopuse structs are GOOD */

	BU_LIST_INSERT(&p.fu->fumate_p->lu_hd, &lu2->l);
	BU_LIST_INSERT(&p.fu->lu_hd, &lu1->l);

	/* Second, build the vertices */
	if (v) vu1 = nmg_mvu(v, &lu1->l.magic, m);
	else vu1 = nmg_mvvu(&lu1->l.magic, m);
	RT_LIST_SET_DOWN_TO_VERT(&lu1->down_hd, vu1);
	/* vu1->up.lu_p = lu1; done by nmg_mvu/nmg_mvvu */

	vu2 = nmg_mvu(vu1->v_p, &lu2->l.magic, m);
	RT_LIST_SET_DOWN_TO_VERT(&lu2->down_hd, vu2);
	/* vu2->up.lu_p = lu2; done by nmg_mvu() */
    } else {
	bu_bomb("nmg_mlv() unknown parent for loopuse!\n");
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_mlv(up=0x%p, v=0x%p, %s) returns lu=0x%p on vu=0x%p\n",
	       magic, v, nmg_orientation(orientation),
	       lu1, vu1);
    }
    return lu1;
}


/**
 * n m g _ m e
 *
 * Make wire edge.
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
 * Explicit Return -
 * An edgeuse in shell "s" whose vertexuse refers to vertex v1.
 * The edgeuse mate's vertexuse refers to vertex v2
 *
 * Implicit Returns -
 * 1) If the shell had a lone vertex in vu_p, it is destroyed,
 * even if both vertices were specified.
 * 2) The returned edgeuse is the first item on the shell's
 * eu_hd list, followed immediately by the mate.
 */
struct edgeuse *
nmg_me(struct vertex *v1, struct vertex *v2, struct shell *s)
{
    struct edge *e;
    struct edgeuse *eu1;
    struct edgeuse *eu2;
    struct vertexuse *vu;
    struct model *m;

    if (v1) NMG_CK_VERTEX(v1);
    if (v2) NMG_CK_VERTEX(v2);
    NMG_CK_SHELL(s);

    m = nmg_find_model(&s->l.magic);
    GET_EDGE(e, m);
    GET_EDGEUSE(eu1, m);
    GET_EDGEUSE(eu2, m);

    BU_LIST_INIT(&eu1->l2);
    BU_LIST_INIT(&eu2->l2);
    eu1->l2.magic = eu2->l2.magic = NMG_EDGEUSE2_MAGIC;

    e->eu_p = eu1;
    /* e->is_real = XXX; */
    e->magic = NMG_EDGE_MAGIC;	/* Edge struct is GOOD */

    eu1->radial_p = eu1->eumate_p = eu2;
    eu2->radial_p = eu2->eumate_p = eu1;

    eu1->e_p = eu2->e_p = e;
    eu1->orientation = eu2->orientation = OT_NONE;
    /* XXX - why not OT_UNSPEC? ctj */
    eu1->vu_p = eu2->vu_p = (struct vertexuse *) NULL;

    eu1->l.magic = eu2->l.magic =
	NMG_EDGEUSE_MAGIC;	/* Edgeuse structs are GOOD */
    /* Not really, edgeuses require vertexuses, but we've got to */
    /* call nmg_mvvu() or nmg_mvu() before we can set vu_p so we */
    /* NULL out vu_p now. */

    /* link the edgeuses to the parent shell */
    eu1->up.s_p = eu2->up.s_p = s;

    if (v1) {
	eu1->vu_p = nmg_mvu(v1, &eu1->l.magic, m);
    } else if (s->vu_p) {
	/* This clause of the if statment dies when no vertex stealing */
	/* steal the vertex from the shell */
	vu = s->vu_p;
	s->vu_p = (struct vertexuse *)NULL;
	eu1->vu_p = vu;
	vu->up.eu_p = eu1;
    } else {
	eu1->vu_p = nmg_mvvu(&eu1->l.magic, m);
    }

    if (v2) {
	eu2->vu_p = nmg_mvu(v2, &eu2->l.magic, m);
    } else if (s->vu_p) {
	/* This clause of the if statment dies when no vertex stealing */
	/* steal the vertex from the shell */
	vu = s->vu_p;
	s->vu_p = (struct vertexuse *)NULL;
	eu2->vu_p = vu;
	vu->up.eu_p = eu2;
    } else {
	eu2->vu_p = nmg_mvvu(&eu2->l.magic, m);
    }

    /* This if statment dies when no vertex stealing */
    if (s->vu_p) {
	/* Ensure shell no longer has any stored vertexuse */
	(void)nmg_kvu(s->vu_p);
	s->vu_p = (struct vertexuse *)NULL;
    }

    /* shell's eu_head, eu1, eu2, ... */
    BU_LIST_APPEND(&s->eu_hd, &eu1->l);
    BU_LIST_APPEND(&eu1->l, &eu2->l);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_me(v1=0x%p, v2=0x%p, s=0x%p) returns eu=0x%p\n", v1, v2, s, eu1);
    }

    return eu1;
}


/**
 * n m g _ m e o n v u
 *
 * Make an edge on vertexuse.
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
struct edgeuse *
nmg_meonvu(struct vertexuse *vu)
{
    struct edge *e;
    struct edgeuse *eu1, *eu2;
    struct model *m;

    NMG_CK_VERTEXUSE(vu);

    m = nmg_find_model(vu->up.magic_p);
    GET_EDGE(e, m);
    GET_EDGEUSE(eu1, m);
    GET_EDGEUSE(eu2, m);

    BU_LIST_INIT(&eu1->l2);
    BU_LIST_INIT(&eu2->l2);
    eu1->l2.magic = eu2->l2.magic = NMG_EDGEUSE2_MAGIC;

    e->eu_p = eu1;
    e->is_real = 0;
    e->magic = NMG_EDGE_MAGIC;

    eu1->radial_p = eu1->eumate_p = eu2;
    eu2->radial_p = eu2->eumate_p = eu1;
    eu1->e_p = eu2->e_p = e;
    eu1->orientation = eu2->orientation = OT_NONE;
    /* XXX Why not OT_UNSPEC? */
    eu1->vu_p = vu;
    /* vu->up needs to be set but we can't do that until we recover */
    /* the shell or loopuse from the up pointer. */

    eu2->vu_p = (struct vertexuse *) NULL;

    /* link edgeuses to parent */
    if (*vu->up.magic_p == NMG_SHELL_MAGIC) {
	struct shell *s;

	s = eu2->up.s_p = eu1->up.s_p = vu->up.s_p;
	eu1->l.magic = eu2->l.magic =
	    NMG_EDGEUSE_MAGIC;	/* Edgeuse structs are GOOD */
	/* eu2 is fake good till it has a real vu */

	vu->up.eu_p = eu1;	/* vu is good again */

	if (s->vu_p != vu)
	    bu_bomb("nmg_meonvu() vetexuse parent shell disowns vertexuse!\n");
	s->vu_p = (struct vertexuse *)NULL;	/* remove from shell */

	eu2->vu_p = nmg_mvu(vu->v_p, &eu2->l.magic, m);
	BU_LIST_APPEND(&s->eu_hd, &eu2->l);
	BU_LIST_APPEND(&s->eu_hd, &eu1->l);
    } else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu;
	struct loopuse *lumate;
	struct vertexuse *vumate;

	lu = vu->up.lu_p;
	NMG_CK_LOOPUSE(lu);
	lumate = lu->lumate_p;
	NMG_CK_LOOPUSE(lumate);

	/* do a little consistency checking */
	if (lu == lumate) bu_bomb("nmg_meonvu() lu mate is lu\n");
	if (BU_LIST_FIRST_MAGIC(&lumate->down_hd) != NMG_VERTEXUSE_MAGIC)
	    bu_bomb("nmg_meonvu() mate of vertex-loop is not vertex-loop!\n");
	vumate = BU_LIST_FIRST(vertexuse, &lumate->down_hd);
	NMG_CK_VERTEXUSE(vumate);
	if (vu == vumate) bu_bomb("nmg_meonvu() vu mate is vu\n");
	NMG_CK_VERTEX(vu->v_p);
	NMG_CK_VERTEX(vumate->v_p);

	/* edgeuses point at vertexuses */
	eu2->vu_p = vumate;

	/* edgeuses point at parent loopuses */
	eu1->up.lu_p = lu;
	eu2->up.lu_p = lumate;

	eu1->l.magic = eu2->l.magic =
	    NMG_EDGEUSE_MAGIC;	/* Edgeuse structs are GOOD */

	/* Fix forw & back pointers after "abuse", above */
	/*
	 * The down_hd can be a POINTER to a vertexuse or
	 * the head of a linked list.  In this case we are
	 * changing from a pointer to a linked list so we
	 * initialize the linked list head then add the loopuses
	 * to that list.
	 */
	BU_LIST_INIT(&lu->down_hd);
	BU_LIST_INIT(&lumate->down_hd);
	/* Add edgeuses to loopuses linked lists */
	BU_LIST_APPEND(&lumate->down_hd, &eu2->l);
	BU_LIST_APPEND(&lu->down_hd, &eu1->l);

	/* vertexuses point up at edgeuses */
	vu->up.eu_p = eu1;
	vumate->up.eu_p = eu2;
    } else {
	bu_bomb("nmg_meonvu() cannot make edge, vertexuse not sole element of object\n");
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_meonvu(vu=0x%p) returns eu=0x%p\n", vu, eu1);
    }

    return eu1;
}


/**
 * n m g _ m l
 *
 * Make wire loop from wire edgeuse list
 *
 * Passed a pointer to a shell.  The wire edgeuse child of the shell
 * is taken as the head of a list of edge(use)s which will form the
 * new loop.  The loop is created from the first N contiguous edges.
 * Thus the end of the new loop is * delineated by the "next"
 * edge(use)being:
 *
 * A) the first object in the list (no other edgeuses in shell list)
 * B) non-contiguous with the previous edge
 *
 * A loop is created from this list of edges.  The edges must form a
 * true circuit, or we dump core on your disk.  If we succeed, then
 * the edgeuses are moved from the shell edgeuse list to the loop, and
 * the two loopuses are inserted into the shell.
 */
struct loopuse *
nmg_ml(struct shell *s)
{
    struct loop *l;
    struct loopuse *lu1, *lu2;
    struct edgeuse *p1;
    struct edgeuse *p2;
    struct edgeuse *feu;
    struct model *m;

    NMG_CK_SHELL(s);
    /* If loop on single vertex */
    if (BU_LIST_IS_EMPTY(&s->eu_hd) && s->vu_p) {
	NMG_CK_VERTEXUSE(s->vu_p);
	NMG_CK_VERTEX(s->vu_p->v_p);
	lu1 = nmg_mlv(&s->l.magic, s->vu_p->v_p, OT_UNSPEC);
	/* (void) nmg_kvu(s->vu_p); */

	if (rt_g.NMG_debug & DEBUG_BASIC) {
	    bu_log("nmg_ml(s=0x%p) returns lu of single vertex=0x%p\n", s, lu1);
	}

	return lu1;
    }

    m = nmg_find_model(&s->l.magic);
    GET_LOOP(l, m);
    GET_LOOPUSE(lu1, m);
    GET_LOOPUSE(lu2, m);

    l->lg_p = (struct loop_g *)NULL;
    l->lu_p = lu1;
    l->magic = NMG_LOOP_MAGIC;	/* loop struct is GOOD */

    BU_LIST_INIT(&lu1->down_hd);
    BU_LIST_INIT(&lu2->down_hd);
    lu1->l_p = lu2->l_p = l;
    lu1->orientation = lu2->orientation = OT_UNSPEC;
    lu1->lumate_p = lu2;
    lu2->lumate_p = lu1;
    lu1->up.s_p = lu2->up.s_p = s;
    lu1->l.magic = lu2->l.magic =
	NMG_LOOPUSE_MAGIC;	/* Loopuse structs are GOOD */

    /* Save the first edgeuse so we can tell if the loop closes */
    feu = BU_LIST_FIRST(edgeuse, &s->eu_hd);
    if (feu) {
	NMG_CK_EDGEUSE(feu);
	NMG_CK_VERTEXUSE(feu->vu_p);
	NMG_CK_VERTEX(feu->vu_p->v_p);
    }

    /* Safety catch in case eu_hd is empty */
    p2 = (struct edgeuse *)NULL;
    while (BU_LIST_NON_EMPTY(&s->eu_hd)) {
	p1 = BU_LIST_FIRST(edgeuse, &s->eu_hd);
	NMG_CK_EDGEUSE(p1);
	p2 = p1->eumate_p;
	NMG_CK_EDGEUSE(p2);

	/* bogosity check */
	if (p1->up.s_p != s || p2->up.s_p != s)
	    bu_bomb("nmg_ml() edgeuse mates don't have proper parent!\n");

	/* dequeue the first edgeuse */
	BU_LIST_DEQUEUE(&p1->l);
	if (BU_LIST_IS_EMPTY(&s->eu_hd)) {
	    bu_log("nmg_ml() in %s at %d edgeuse mate not in this shell\n",
		   __FILE__, __LINE__);
	    bu_bomb("nmg_ml\n");
	}

	/* dequeue the edgeuse's mate */
	BU_LIST_DEQUEUE(&p2->l);

	/* Insert the next new edgeuse(s) at tail of the loop's list
	 * (ie, insert just before the head).
	 * head, ....., p2, p1, (tail)
	 */
	BU_LIST_INSERT(&lu1->down_hd, &p1->l);
	BU_LIST_INSERT(&lu2->down_hd, &p2->l);

	p1->up.lu_p = lu1;
	p2->up.lu_p = lu2;

	/* If p2's vertex does not match next one comming, quit */
	if (BU_LIST_IS_EMPTY(&s->eu_hd)) break;
	p1 = BU_LIST_FIRST(edgeuse, &s->eu_hd);
	NMG_CK_EDGEUSE(p1);
	NMG_CK_VERTEXUSE(p1->vu_p);
	NMG_CK_VERTEX(p1->vu_p->v_p);
	NMG_CK_VERTEXUSE(p2->vu_p);
	NMG_CK_VERTEX(p2->vu_p->v_p);
	if (p1->vu_p->v_p != p2->vu_p->v_p) {
	    break;
	}
    }

    if (p2) {
	NMG_CK_EDGEUSE(p2);
	NMG_CK_VERTEXUSE(p2->vu_p);
	NMG_CK_VERTEX(p2->vu_p->v_p);
    }
    if (p2 && p2->vu_p->v_p != feu->vu_p->v_p) {
	bu_log("nmg_ml() Edge(use)s do not form proper loop!\n");
	nmg_pr_s(s, (char *)NULL);
	bu_log("nmg_ml() Edge(use)s do not form proper loop!\n");
	bu_bomb("nmg_ml\n");
    }

    /* Head, lu1, lu2, ... */
    BU_LIST_APPEND(&s->lu_hd, &lu2->l);
    BU_LIST_APPEND(&s->lu_hd, &lu1->l);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_ml(s=0x%p) returns lu=0x%p\n", s, lu1);
    }

    return lu1;
}


/************************************************************************
 *									*
 *				"Kill" Routines				*
 *									*
 * These subroutines delete (kill) a topological entity,		*
 * eliminating any related subordinate entities where appropriate.	*
 *									*
 * If the superior entity becomes empty as a result of a "kill"	*
 * operation, and that superior entity isn't allowed to *be* empty,	*
 * then a TRUE (1) return code is given, indicating that the		*
 * caller needs to immediately delete the superior entity before	*
 * performing any other NMG operations.				*
 *									*
 * During this interval, the superior entity is in an "illegal"	*
 * state that the verify routines (nmg_vshell, etc.) and subsequent	*
 * NMG processing routines will not accept, so that code which fails	*
 * to honor the return codes will "blow up" downstream.		*
 * Unfortunately, in some cases the problem won't be detected for	*
 * quite a while, but this is better than having subsequent code	*
 * encountering entities that *look* valid, but shouldn't be.		*
 *									*
 ************************************************************************/


/**
 * n m g _ k v u
 *
 * Kill vertexuse, and null out parent's vu_p.
 *
 * This routine is not intented for general use by applications,
 * because it requires cooperation on the part of the caller to
 * properly dispose of or fix the now *quite* illegal parent.
 * (Illegal because the parent's vu_p is NULL).  It exists primarily
 * as a support routine for "mopping up" after nmg_klu(), nmg_keu(),
 * nmg_ks(), and nmg_mv_vu_between_shells().
 *
 * It is also used in a particularly ugly way in nmg_cut_loop() and
 * nmg_split_lu_at_vu() as part of their method for obtaining an
 * "empty" loopuse/loop set.
 *
 * It is worth noting that all these callers ignore the return code,
 * because they *all* exist to intentionally empty out the parent, but
 * the return code is provided anyway, in the name of [CTJ] symmetry.
 *
 * Returns -
 * 0 If all is well in the parent
 * 1 If parent is empty, and is thus "illegal"
 */
int
nmg_kvu(struct vertexuse *vu)
{
    struct vertex *v;
    int ret = 0;

    if (!vu)
	return 1;

    NMG_CK_VERTEXUSE(vu);

    if (vu->a.magic_p) {
	NMG_CK_VERTEXUSE_A_EITHER(vu->a.magic_p);
	switch (*vu->a.magic_p) {
	    case NMG_VERTEXUSE_A_PLANE_MAGIC:
		FREE_VERTEXUSE_A_PLANE(vu->a.plane_p);
		break;
	    case NMG_VERTEXUSE_A_CNURB_MAGIC:
		FREE_VERTEXUSE_A_CNURB(vu->a.cnurb_p);
		break;
	}
    }

    v = vu->v_p;
    if (v) {
	NMG_CK_VERTEX(v);

	BU_LIST_DEQUEUE(&vu->l);
	if (BU_LIST_IS_EMPTY(&v->vu_hd)) {
	    /* last vertexuse on vertex */
	    if (v->vg_p) FREE_VERTEX_G(v->vg_p);
	    FREE_VERTEX(v);
	}
    }

    /* erase existence of this vertexuse from parent */
    if (vu->up.magic_p != NULL) {
	if (*vu->up.magic_p == NMG_SHELL_MAGIC) {
	    if (vu->up.s_p) {
		vu->up.s_p->vu_p = (struct vertexuse *)NULL;
		ret = nmg_shell_is_empty(vu->up.s_p);
	    }
	} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	    if (vu->up.lu_p) {
		/* Reset the hack */
		BU_LIST_INIT(&vu->up.lu_p->down_hd);
		ret = 1;
	    }
	} else if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
	    if (vu->up.eu_p) {
		vu->up.eu_p->vu_p = (struct vertexuse *)NULL;
		ret = 1;
	    }
	} else
	    bu_bomb("nmg_kvu() killing vertexuse of unknown parent?\n");
    }

    FREE_VERTEXUSE(vu);
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_kvu(vu=0x%p) ret=%d\n", vu, ret);
    }
    return ret;
}


/**
 * n m g _ k f g
 *
 * Internal routine to release face geometry when no more faces use
 * it.
 */
static void
nmg_kfg(uint32_t *magic_p)
{
    switch (*magic_p) {
	case NMG_FACE_G_PLANE_MAGIC: {
	    /* If face_g is not referred to by any other face, free it */
	    struct face_g_plane *pp;
	    pp = (struct face_g_plane *)magic_p;
	    if (BU_LIST_NON_EMPTY(&(pp->f_hd))) return;
	    FREE_FACE_G_PLANE(pp);
	}
	    break;
	case NMG_FACE_G_SNURB_MAGIC: {
	    /* If face_g is not referred to by any other face, free it */
	    struct face_g_snurb *sp;
	    sp = (struct face_g_snurb *)magic_p;
	    if (BU_LIST_NON_EMPTY(&(sp->f_hd))) return;
	    bu_free((char *)sp->u.knots, "nmg_kfg snurb u knots[]");
	    bu_free((char *)sp->v.knots, "nmg_kfg snurb v knots[]");
	    bu_free((char *)sp->ctl_points, "nmg_kfg snurb ctl_points[]");
	    FREE_FACE_G_SNURB(sp);
	}
	    break;
	default:
	    bu_bomb("nmg_kfg() bad magic\n");
    }
}


/**
 * n m g _ k f u
 *
 * Kill Faceuse
 *
 * delete a faceuse and its mate from the parent shell.
 *
 * Any children found are brutally murdered as well.  The faceuses are
 * dequeued from the parent shell's list here.
 *
 * Returns -
 * 0 If all is well
 * 1 If parent shell is now empty, and is thus "illegal"
 */
int
nmg_kfu(struct faceuse *fu1)
{
    struct faceuse *fu2;
    struct face *f1;
    struct face *f2;
    struct shell *s;
    int ret;

    if (!fu1)
	return 1;

    NMG_CK_FACEUSE(fu1);
    fu2 = fu1->fumate_p;
    NMG_CK_FACEUSE(fu2);
    f1 = fu1->f_p;
    f2 = fu2->f_p;
    NMG_CK_FACE(f1);
    NMG_CK_FACE(f2);
    if (f1 != f2)
	bu_bomb("nmg_kfu() faceuse mates do not share face!\n");
    s = fu1->s_p;
    NMG_CK_SHELL(s);

    /* kill off the children (infanticide?)*/
    while (BU_LIST_NON_EMPTY(&fu1->lu_hd)) {
	(void)nmg_klu(BU_LIST_FIRST(loopuse, &fu1->lu_hd));
    }

    /* Release the face geometry */
    if (f1->g.magic_p) {
	/* Disassociate this face from face_g */
	BU_LIST_DEQUEUE(&f1->l);
	nmg_kfg(f1->g.magic_p);
    }
    FREE_FACE(f1);
    fu1->f_p = fu2->f_p = (struct face *)NULL;

    /* remove ourselves from the parent list */
    BU_LIST_DEQUEUE(&fu1->l);
    if (BU_LIST_IS_EMPTY(&s->fu_hd))
	bu_bomb("nmg_kfu() faceuse mate not in parent shell?\n");
    BU_LIST_DEQUEUE(&fu2->l);

    FREE_FACEUSE(fu1);
    FREE_FACEUSE(fu2);
    ret = nmg_shell_is_empty(s);
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_kfu(fu1=0x%p) fu2=0x%p ret=%d\n", fu1, fu2, ret);
    }
    return ret;
}


/**
 * n m g _ k l u
 *
 * Kill loopuse, loopuse mate, and loop.
 *
 * if the loop contains any edgeuses or vertexuses they are killed
 * before the loop is deleted.
 *
 * We support the concept of killing a loop with no children to
 * support the routine "nmg_demote_lu"
 *
 * Returns -
 * 0 If all is well
 * 1 If parent is empty, and is thus "illegal"
 */
int
nmg_klu(struct loopuse *lu1)
{
    struct loopuse *lu2;
    uint32_t magic1;
    int ret = 0;

    if (!lu1)
	return 1;

    NMG_CK_LOOPUSE(lu1);
    lu2 = lu1->lumate_p;
    NMG_CK_LOOPUSE(lu2);

    if (lu1->l_p != lu2->l_p)
	bu_bomb("nmg_klu() loopmates do not share loop!\n");

    if (*lu1->up.magic_p != *lu2->up.magic_p)
	bu_bomb("nmg_klu() loopuses do not have same type of parent!\n");

    /* deal with the children */
    magic1 = BU_LIST_FIRST_MAGIC(&lu1->down_hd);
    if (magic1 != BU_LIST_FIRST_MAGIC(&lu2->down_hd))
	bu_bomb("nmg_klu() loopuses do not have same type of child!\n");

    if (magic1 == NMG_VERTEXUSE_MAGIC) {
	/* Follow the vertex-loop hack downward,
	 * nmg_kvu() will clean up */
	(void)nmg_kvu(BU_LIST_FIRST(vertexuse, &lu1->down_hd));
	(void)nmg_kvu(BU_LIST_FIRST(vertexuse, &lu2->down_hd));
    } else if (magic1 == NMG_EDGEUSE_MAGIC) {
	/* delete all edgeuse in the loopuse (&mate) */
	while (BU_LIST_NON_EMPTY(&lu1->down_hd)) {
	    (void)nmg_keu(BU_LIST_FIRST(edgeuse, &lu1->down_hd));
	}
    } else if (magic1 == BU_LIST_HEAD_MAGIC) {
	/* down_hd list is empty, no problem */
    } else {
	bu_log("nmg_klu(0x%p) magic=%s\n", lu1, bu_identify_magic(magic1));
	bu_bomb("nmg_klu: unknown type for loopuse child\n");
    }

    /* disconnect from parent's list */
    if (*lu1->up.magic_p == NMG_SHELL_MAGIC) {
	BU_LIST_DEQUEUE(&lu1->l);
	BU_LIST_DEQUEUE(&lu2->l);
	ret = nmg_shell_is_empty(lu1->up.s_p);
    } else if (*lu1->up.magic_p == NMG_FACEUSE_MAGIC) {
	BU_LIST_DEQUEUE(&lu1->l);
	BU_LIST_DEQUEUE(&lu2->l);
	ret = BU_LIST_IS_EMPTY(&lu1->up.fu_p->lu_hd);
    } else {
	bu_bomb("nmg_klu() unknown parent for loopuse\n");
    }

    NMG_CK_LOOP(lu1->l_p);
    if (lu1->l_p->lg_p) {
	NMG_CK_LOOP_G(lu1->l_p->lg_p);
	FREE_LOOP_G(lu1->l_p->lg_p);
    }
    FREE_LOOP(lu1->l_p);
    FREE_LOOPUSE(lu1);
    FREE_LOOPUSE(lu2);
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_klu(lu1=0x%p) lu2=0x%p ret=%d\n", lu1, lu2, ret);
    }
    return ret;
}


/**
 * n m g _ k e g
 *
 * Internal routine to kill an edge geometry structure (of either
 * type), if all the edgeuses on its list have vanished.  Regardless,
 * the edgeuse's geometry pointer is cleared.
 *
 * This routine does only a single edgeuse.  If the edgeuse mate has
 * geometry to be killed, make a second call.  Sometimes only one of
 * the two needs to release the geometry.
 *
 * Returns -
 * 0 If the old edge geometry (eu->g.magic_p) has other uses.
 * 1 If the old edge geometry has been destroyed. Caller beware!
 *
 * NOT INTENDED FOR GENERAL USE!
 * However, nmg_mod.c needs it for nmg_eusplit().  (Drat!)
 */
/* static */ int
nmg_keg(struct edgeuse *eu)
{
    if (!eu)
	return 1;

    NMG_CK_EDGEUSE(eu);

    if (!eu->g.magic_p)
	return 1; /* no geometry to kill */

    switch (*eu->g.magic_p) {
	case NMG_EDGE_G_LSEG_MAGIC: {
	    struct edge_g_lseg *lp;
	    lp = eu->g.lseg_p;
	    eu->g.magic_p = NULL;
	    if (BU_LIST_NON_EMPTY(&lp->eu_hd2)) return 0;
	    FREE_EDGE_G_LSEG(lp);
	    break;
	}

	case NMG_EDGE_G_CNURB_MAGIC: {
	    struct edge_g_cnurb *eg;
	    eg = eu->g.cnurb_p;
	    eu->g.magic_p = NULL;
	    if (BU_LIST_NON_EMPTY(&eg->eu_hd2)) return 0;
	    if (eg->order != 0) {
		bu_free((char *)eg->k.knots, "nmg_keg cnurb knots[]");
		bu_free((char *)eg->ctl_points, "nmg_keg cnurb ctl_points[]");
	    }
	    FREE_EDGE_G_CNURB(eg);
	    break;
	}
    }

    return 1;		/* edge geometry has been destroyed */
}


/**
 * n m g _ k e u
 *
 * Delete an edgeuse & its mate from a shell or loop.
 *
 * Returns -
 * 0 If all is well
 * 1 If the parent now has no edgeuses, and is thus "illegal" and in
 * need of being deleted.  (The lu / shell deletion can't be handled
 * at this level, and must be done by the caller).
 */
int
nmg_keu(register struct edgeuse *eu1)
{
    register struct edgeuse *eu2;
    struct edge *e;
    int ret = 0;

    if (!eu1)
	return 0;

    NMG_CK_EDGEUSE(eu1);
    e = eu1->e_p;
    NMG_CK_EDGE(e);

    eu2 = eu1->eumate_p;
    NMG_CK_EDGEUSE(eu2);
    NMG_CK_EDGE(eu2->e_p);

    if (e != eu2->e_p) {
	bu_bomb("nmg_keu() edgeuse pair does not share edge\n");
    }

    /* unlink from radial linkages (if any) */
    if (eu1->radial_p != eu2) {
	NMG_CK_EDGEUSE(eu1->radial_p);
	NMG_CK_EDGEUSE(eu2->radial_p);

	eu1->radial_p->radial_p = eu2->radial_p;
	eu2->radial_p->radial_p = eu1->radial_p;

	NMG_CK_EDGEUSE(eu1->radial_p);
	NMG_CK_EDGEUSE(eu2->radial_p);

	/* since there is a use of this edge left, make sure the edge
	 * structure points to a remaining edgeuse
	 */
	if (e->eu_p == eu1 || e->eu_p == eu2)
	    e->eu_p = eu1->radial_p;
	NMG_CK_EDGEUSE(e->eu_p);
    } else {
	/* since these two edgeuses are the only use of this edge,
	 * we need to free the edge (since all uses are about
	 * to disappear).
	 */
	FREE_EDGE(e);
	e = (struct edge *)NULL;
	eu1->e_p = e;	/* sanity */
	eu2->e_p = e;
    }

    /* handle geometry, if any */
    if (eu1->g.magic_p) {
	/* Dequeue edgeuse from geometry's list of users */
	BU_LIST_DEQUEUE(&eu1->l2);

	/* Release the edgeuse's geometry pointer */
	nmg_keg(eu1);
    }

    if (eu2->g.magic_p) {
	/* Dequeue edgeuse from geometry's list of users */
	BU_LIST_DEQUEUE(&eu2->l2);

	/* Release the edgeuse's geometry pointer */
	nmg_keg(eu2);
    }

    /* remove the edgeuses from their parents */
    if (*eu1->up.magic_p == NMG_LOOPUSE_MAGIC) {
	struct loopuse *lu1, *lu2;
	lu1 = eu1->up.lu_p;
	lu2 = eu2->up.lu_p;
	NMG_CK_LOOPUSE(lu1);
	NMG_CK_LOOPUSE(lu2);

	if (lu1 == lu2) bu_bomb("nmg_keu() edgeuses on same loopuse\n");
	if (lu1->lumate_p != lu2 || lu1 != lu2->lumate_p) {
	    bu_log("nmg_keu() lu1=0x%p, mate=0x%p\n", lu1, lu1->lumate_p);
	    bu_log("nmg_keu() lu2=0x%p, mate=0x%p\n", lu2, lu2->lumate_p);
	    bu_bomb("nmg_keu() edgeuse mates don't belong to loopuse mates\n");
	}

	/* remove the edgeuses from their parents */
	BU_LIST_DEQUEUE(&eu1->l);
	BU_LIST_DEQUEUE(&eu2->l);

	/* If loopuse list is empty, caller needs to delete it. */
	if (BU_LIST_IS_EMPTY(&lu1->down_hd)) ret = 1;
    } else if (*eu1->up.magic_p == NMG_SHELL_MAGIC) {
	if (eu1->up.s_p != eu2->up.s_p) {
	    bu_bomb("nmg_keu() edgeuses don't share parent shell\n");
	}

	/* unlink edgeuses from the parent shell */
	BU_LIST_DEQUEUE(&eu1->l);
	BU_LIST_DEQUEUE(&eu2->l);
	ret = nmg_shell_is_empty(eu1->up.s_p);
    } else {
	bu_bomb("nmg_keu() bad up pointer\n");
    }

    /* kill the vertexuses associated with these edgeuses */
    if (eu1->vu_p) {
	(void)nmg_kvu(eu1->vu_p);
    }
    if (eu2->vu_p) {
	(void)nmg_kvu(eu2->vu_p);
    }

    FREE_EDGEUSE(eu1);
    FREE_EDGEUSE(eu2);

    if (e) {
	NMG_CK_EDGE(e);
	NMG_CK_EDGEUSE(e->eu_p);
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_keu(eu1=0x%p) eu2=0x%p ret=%d\n", eu1, eu2, ret);
    }
    return ret;
}


/**
 * n m g _ k s
 *
 * Kill a shell and all children
 *
 * Returns -
 * 0 If all is well
 * 1 If parent nmgregion is now empty.  While not "illegal",
 * an empty region is probably worthy of note.
 */
int
nmg_ks(struct shell *s)
{
    struct nmgregion *r;

    if (!s)
	return 0;

    NMG_CK_SHELL(s);
    r = s->r_p;
    if (r)
	NMG_CK_REGION(r);

    while (BU_LIST_NON_EMPTY(&s->fu_hd))
	(void)nmg_kfu(BU_LIST_FIRST(faceuse, &s->fu_hd));
    while (BU_LIST_NON_EMPTY(&s->lu_hd))
	(void)nmg_klu(BU_LIST_FIRST(loopuse, &s->lu_hd));
    while (BU_LIST_NON_EMPTY(&s->eu_hd))
	(void)nmg_keu(BU_LIST_FIRST(edgeuse, &s->eu_hd));
    if (s->vu_p)
	nmg_kvu(s->vu_p);

    BU_LIST_DEQUEUE(&s->l);

    if (s->sa_p) {
	FREE_SHELL_A(s->sa_p);
    }

    FREE_SHELL(s);
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_ks(s=0x%p)\n", s);
    }

    if (r && BU_LIST_IS_EMPTY(&r->s_hd))
	return 1;

    return 0;
}


/**
 * n m g _ k r
 *
 * Kill a region and all shells in it.
 *
 * Returns -
 * 0 If all is well
 * 1 If model is now empty.  While not "illegal",
 * an empty model is probably worthy of note.
 */
int
nmg_kr(struct nmgregion *r)
{
    struct model *m;

    if (!r)
	return 0;

    NMG_CK_REGION(r);
    m = r->m_p;
    if (m)
	NMG_CK_MODEL(m);

    while (BU_LIST_NON_EMPTY(&r->s_hd))
	(void)nmg_ks(BU_LIST_FIRST(shell, &r->s_hd));

    BU_LIST_DEQUEUE(&r->l);

    if (r->ra_p) {
	FREE_REGION_A(r->ra_p);
    }
    FREE_REGION(r);
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_kr(r=0x%p)\n", r);
    }

    if (m && BU_LIST_IS_EMPTY(&m->r_hd)) {
	m->maxindex = 1;	/* Reset when last region is killed */
	return 1;
    }
    return 0;
}


/**
 * n m g _ k m
 *
 * Kill an entire model.  Nothing is left.
 */
void
nmg_km(struct model *m)
{
    if (!m)
	return;

    NMG_CK_MODEL(m);

    while (BU_LIST_NON_EMPTY(&m->r_hd))
	(void)nmg_kr(BU_LIST_FIRST(nmgregion, &m->r_hd));

    FREE_MODEL(m);
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_km(m=0x%p)\n", m);
    }
}


/************************************************************************
 *									*
 *			"Geometry" and "Attribute" Routines		*
 *									*
 ************************************************************************/


/**
 * n m g _ v e r t e x _ g v
 *
 * Associate point_t ("vector") coordinates with a vertex
 */
void
nmg_vertex_gv(struct vertex *v, const fastf_t *pt)
{
    struct vertex_g *vg;
    struct model *m;

    NMG_CK_VERTEX(v);

    vg = v->vg_p;
    if (vg) {
	NMG_CK_VERTEX_G(v->vg_p);
    } else {
	m = nmg_find_model(
	    &BU_LIST_NEXT(vertexuse, &v->vu_hd)->l.magic);
	GET_VERTEX_G(vg, m);

	vg->magic = NMG_VERTEX_G_MAGIC;
	v->vg_p = vg;
    }
    VMOVE(vg->coord, pt);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_vertex_gv(v=0x%p, pt=(%g %g %g))\n", v, V3ARGS(pt));
    }
}


/**
 * n m g _ v e r t e x _ g
 *
 * a version that can take x, y, z coords and doesn't need a point
 * array.  Mostly useful for quick and dirty programs.
 */
void
nmg_vertex_g(register struct vertex *v, fastf_t x, fastf_t y, fastf_t z)
{
    point_t pt;

    pt[0] = x;
    pt[1] = y;
    pt[2] = z;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_vertex_g(v=0x%p, pt=(%g %g %g))\n", v, x, y, z);
    }

    nmg_vertex_gv(v, pt);
}


/**
 * n m g _ v e r t e x u s e _ n v
 *
 * Assign a normal vector to a vertexuse
 */
void
nmg_vertexuse_nv(struct vertexuse *vu, const fastf_t *norm)
{
    struct model *m;

    NMG_CK_VERTEXUSE(vu);

    if (!vu->a.magic_p) {
	struct vertexuse_a_plane *vua;
	m = nmg_find_model(&vu->l.magic);
	GET_VERTEXUSE_A_PLANE(vua, m);
	vua->magic = NMG_VERTEXUSE_A_PLANE_MAGIC;
	vu->a.plane_p = vua;
    } else if (*vu->a.magic_p == NMG_VERTEXUSE_A_CNURB_MAGIC) {
	/* Assigning a normal vector to a cnurb vua is illegal */
	bu_bomb("nmg_vertexuse_nv() Illegal assignment of normal vector to edge_g_cnurb vertexuse\n");
    } else {
	NMG_CK_VERTEXUSE_A_PLANE(vu->a.plane_p);
    }

    VMOVE(vu->a.plane_p->N, norm);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_vertexuse_nv(vu=0x%p, norm=(%g %g %g))\n", vu, V3ARGS(norm));
    }
}


/**
 * n m g _ v e r t e x u s e _ a _ c n u r b
 *
 * Given a vertex with associated geometry in model space which lies
 * on a face_g_snurb surface, it will have a corresponding set of (u,
 * v) or (u, v, w) parameters on that surface.  Build the association
 * here.
 *
 * Note that all vertexuses of a single vertex which are all used by
 * the same face_g_snurb will have the same "param" value, but will
 * have individual vertexuse_a_cnurb structures.
 */
void
nmg_vertexuse_a_cnurb(struct vertexuse *vu, const fastf_t *uvw)
{
    struct vertexuse_a_cnurb *vua;
    struct model *m;

    NMG_CK_VERTEXUSE(vu);

    if (vu->a.magic_p) bu_bomb("nmg_vertexuse_a_cnurb() vu has attribute already\n");
    NMG_CK_EDGEUSE(vu->up.eu_p);
    if (vu->up.eu_p->g.magic_p) NMG_CK_EDGE_G_CNURB(vu->up.eu_p->g.cnurb_p);

    m = nmg_find_model(&vu->l.magic);
    GET_VERTEXUSE_A_CNURB(vua, m);
    VMOVE(vua->param, uvw);
    vua->magic = NMG_VERTEXUSE_A_CNURB_MAGIC;

    vu->a.cnurb_p = vua;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_vertexuse_a_cnurb(vu=0x%p, param=(%g %g %g)) vua=0x%p\n",
	       vu, V3ARGS(uvw), vua);
    }
}


/**
 * n m g _ e d g e _ g
 *
 * Compute the equation of the line formed by the endpoints of the
 * edge.
 *
 * XXX This isn't the best name.  nmg_edge_g_lseg() ?
 */
void
nmg_edge_g(struct edgeuse *eu)
{
    struct model *m;
    struct edge_g_lseg *eg_p = (struct edge_g_lseg *)NULL;
    struct edge *e;
    struct edgeuse *eu2;
    pointp_t pt;
    int found_eg=0;

    NMG_CK_EDGEUSE(eu);
    e = eu->e_p;
    NMG_CK_EDGE(e);
    NMG_CK_VERTEXUSE(eu->vu_p);
    NMG_CK_VERTEX(eu->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

    NMG_CK_EDGEUSE(eu->eumate_p);
    NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
    NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

    if (eu->vu_p->v_p == eu->eumate_p->vu_p->v_p)
	bu_bomb("nmg_edge_g(): Warning - edge runs from+to same vertex, 0 len!\n");

    eg_p = eu->g.lseg_p;
    if (eg_p) {
	NMG_CK_EDGE_G_LSEG(eg_p);
	bu_bomb("nmg_edge_g() geometry already assigned\n");
    }

    /* Search all other uses of this edge for an existing edge_g_lseg */
    eu2 = eu->eumate_p->radial_p;
    while (eu2 != eu) {
	if (eu2->g.magic_p && *eu2->g.magic_p == NMG_EDGE_G_LSEG_MAGIC) {
	    eg_p = eu2->g.lseg_p;
	    found_eg = 1;
	    break;
	}
	eu2 = eu2->eumate_p->radial_p;
    }

    if (!eg_p) {
	/* Make new edge_g structure */
	m = nmg_find_model(&eu->l.magic);
	GET_EDGE_G_LSEG(eg_p, m);
	BU_LIST_INIT(&eg_p->eu_hd2);
	eg_p->l.magic = NMG_EDGE_G_LSEG_MAGIC;

	/* copy the point from the vertex of one of our edgeuses */
	pt = eu->vu_p->v_p->vg_p->coord;
	VMOVE(eg_p->e_pt, pt);

	/* compute the direction from the endpoints of the edgeuse(s) */
	pt = eu->eumate_p->vu_p->v_p->vg_p->coord;
	VSUB2(eg_p->e_dir, eg_p->e_pt, pt);

	/* If the edge vector is essentially 0 magnitude we're in trouble.
	 * Warn the user and create an arbitrary vector we can use.
	 */
	if (VNEAR_ZERO(eg_p->e_dir, SMALL_FASTF)) {
	    pointp_t pt2 = eu->vu_p->v_p->vg_p->coord;
	    VPRINT("nmg_edge_g(): e_dir too small", eg_p->e_dir);
	    bu_log("nmg_edge_g(): (%g %g %g) -> (%g %g %g)",
		   pt[X],  pt[Y],  pt[Z],
		   pt2[X], pt2[Y], pt2[Z]);

	    VSET(eg_p->e_dir, 1.0, 0.0, 0.0);
	    VPRINT("nmg_edge_g(): Forcing e_dir to", eg_p->e_dir);
	    bu_bomb("nmg_edge_g():  0 length edge\n");
	}
    }

    /* Dequeue edgeuses from their current list (should point to themselves), add to new list */
    BU_LIST_DEQUEUE(&eu->l2);
    BU_LIST_DEQUEUE(&eu->eumate_p->l2);

    /* Associate edgeuse with this geometry */
    BU_LIST_INSERT(&eg_p->eu_hd2, &eu->l2);
    BU_LIST_INSERT(&eg_p->eu_hd2, &eu->eumate_p->l2);
    eu->g.lseg_p = eg_p;
    eu->eumate_p->g.lseg_p = eg_p;

    if (!found_eg) {
	/* No uses of this edge have geometry, get them all */
	eu2 = eu->eumate_p->radial_p;
	while (eu2 != eu) {
	    eu2->g.lseg_p = eg_p;
	    BU_LIST_INSERT(&eg_p->eu_hd2, &eu2->l2);
	    eu2->eumate_p->g.lseg_p = eg_p;
	    BU_LIST_INSERT(&eg_p->eu_hd2, &eu2->eumate_p->l2);

	    eu2 = eu2->eumate_p->radial_p;
	}
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_edge_g(eu=0x%p) eg=0x%p\n", eu, eg_p);
    }
}


/**
 * n m g _ e d g e _ g _ c n u r b
 *
 * For an edgeuse associated with a face_g_snurb surface, create a
 * spline curve in the parameter space of the snurb which describes
 * the path from the start vertex to the end vertex.
 *
 * The parameters of the end points are taken from the vertexuse
 * attributes at either end of the edgeuse.
 */
void
nmg_edge_g_cnurb(struct edgeuse *eu, int order, int n_knots, fastf_t *kv, int n_pts, int pt_type, fastf_t *points)
{
    struct model *m;
    struct edge_g_cnurb *eg;
    struct edge *e;
    struct faceuse *fu;

    NMG_CK_EDGEUSE(eu);
    e = eu->e_p;
    NMG_CK_EDGE(e);
    NMG_CK_VERTEXUSE(eu->vu_p);
    NMG_CK_VERTEX(eu->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

    NMG_CK_EDGEUSE(eu->eumate_p);
    NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
    NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

    if (eu->g.cnurb_p) {
	bu_bomb("nmg_edge_g_cnurb() geometry already assigned\n");
    }
    fu = nmg_find_fu_of_eu(eu);
    NMG_CK_FACEUSE(fu);
    NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);

    /* Make new edge_g structure */
    m = nmg_find_model(&eu->l.magic);
    GET_EDGE_G_CNURB(eg, m);
    BU_LIST_INIT(&eg->eu_hd2);

    eg->order = order;
    if (n_knots > 0 && kv) {
	eg->k.k_size = n_knots;
	eg->k.knots = kv;
    } else {
	/* Give a default curve, no interior knots */
	rt_nurb_kvknot(&eg->k, order, 0.0, 1.0, n_knots - (2 * order), (struct resource *)NULL);
    }

    if (n_pts < 2) bu_bomb("nmg_edge_g_cnurb() n_pts < 2\n");
    eg->c_size = n_pts;
    eg->pt_type = pt_type;
    if (points) {
	eg->ctl_points = points;
    } else {
	int ncoord = RT_NURB_EXTRACT_COORDS(pt_type);

	eg->ctl_points = (fastf_t *)bu_calloc(
	    ncoord * n_pts,
	    sizeof(fastf_t),
	    "cnurb ctl_points[]");

	/*
	 * As a courtesy, set first and last point to
	 * the PARAMETER values of the edge's vertices.
	 */
	NMG_CK_VERTEXUSE_A_CNURB(eu->vu_p->a.cnurb_p);
	NMG_CK_VERTEXUSE_A_CNURB(eu->eumate_p->vu_p->a.cnurb_p);
	switch (ncoord) {
	    case 4:
		eg->ctl_points[3] = 1;
		eg->ctl_points[ (n_pts-1)*ncoord + 3] = 1;
		/* fall through... */
	    case 3:
		VMOVE(eg->ctl_points, eu->vu_p->a.cnurb_p->param);
		VMOVE(&eg->ctl_points[ (n_pts-1)*ncoord ],
		      eu->eumate_p->vu_p->a.cnurb_p->param);
		break;
	    case 2:
		V2MOVE(eg->ctl_points, eu->vu_p->a.cnurb_p->param);
		V2MOVE(&eg->ctl_points[ (n_pts-1)*ncoord ],
		       eu->eumate_p->vu_p->a.cnurb_p->param);
		break;
	    default:
		bu_bomb("nmg_edge_g_cnurb() bad ncoord?\n");
	}
    }

    /* Dequeue edgeuses from their current list (should point to themselves), add to new list */
    BU_LIST_DEQUEUE(&eu->l2);
    BU_LIST_DEQUEUE(&eu->eumate_p->l2);

    /* Associate edgeuse with this geometry */
    BU_LIST_INSERT(&eg->eu_hd2, &eu->l2);
    BU_LIST_INSERT(&eg->eu_hd2, &eu->eumate_p->l2);
    eu->g.cnurb_p = eg;
    eu->eumate_p->g.cnurb_p = eg;

    eg->l.magic = NMG_EDGE_G_CNURB_MAGIC;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_edge_g_cnurb(eu=0x%p, order=%d, n_knots=%d, kv=0x%p, n_pts=%d, pt_type=0x%x, points=0x%p) eg=0x%p\n",
	       eu, order, n_knots, eg->k.knots,
	       n_pts, pt_type, eg->ctl_points, eg);
    }
}


/**
 * n m g _ e d g e _ g _ c n u r b _ p l i n e a r
 *
 * For an edgeuse associated with a face_g_snurb surface, create a
 * spline "curve" in the parameter space of the snurb which describes
 * a STRAIGHT LINE in parameter space from the u, v parameters of the
 * start vertex to the end vertex.
 *
 * The parameters of the end points are found in the vertexuse
 * attributes at either end of the edgeuse, which should have already
 * been established.
 *
 * This is a special case of nmg_edge_g_cnurb(), and should be used
 * when the path through parameter space is known to be a line
 * segment.  This permits the savings of a lot of memory, both in core
 * and on disk, by eliminating a knot vector (typ. 64 bytes or more)
 * and a ctl_point[] array (typ. 16 bytes or more).
 *
 * This special condition is indicated by order == 0.  See nmg.h for
 * details.
 */
void
nmg_edge_g_cnurb_plinear(struct edgeuse *eu)
{
    struct model *m;
    struct edge_g_cnurb *eg;
    struct edge *e;
    struct faceuse *fu;

    NMG_CK_EDGEUSE(eu);
    e = eu->e_p;
    NMG_CK_EDGE(e);
    NMG_CK_VERTEXUSE(eu->vu_p);
    NMG_CK_VERTEX(eu->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

    NMG_CK_EDGEUSE(eu->eumate_p);
    NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
    NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

    NMG_CK_VERTEXUSE_A_CNURB(eu->vu_p->a.cnurb_p);
    NMG_CK_VERTEXUSE_A_CNURB(eu->eumate_p->vu_p->a.cnurb_p);

    if (eu->g.cnurb_p) {
	bu_bomb("nmg_edge_g_cnurb_plinear() geometry already assigned\n");
    }
    fu = nmg_find_fu_of_eu(eu);
    NMG_CK_FACEUSE(fu);
    NMG_CK_FACE_G_SNURB(fu->f_p->g.snurb_p);

    /* Make new edge_g structure */
    m = nmg_find_model(&eu->l.magic);
    GET_EDGE_G_CNURB(eg, m);
    BU_LIST_INIT(&eg->eu_hd2);

    eg->order = 0;		/* SPECIAL FLAG */

    /* Dequeue edgeuses from their current list (should point to themselves), add to new list */
    BU_LIST_DEQUEUE(&eu->l2);
    BU_LIST_DEQUEUE(&eu->eumate_p->l2);

    /* Associate edgeuse with this geometry */
    BU_LIST_INSERT(&eg->eu_hd2, &eu->l2);
    BU_LIST_INSERT(&eg->eu_hd2, &eu->eumate_p->l2);
    eu->g.cnurb_p = eg;
    eu->eumate_p->g.cnurb_p = eg;

    eg->l.magic = NMG_EDGE_G_CNURB_MAGIC;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_edge_g_cnurb_plinear(eu=0x%p) order=0, eg=0x%p\n",
	       eu, eg);
    }
}


/**
 * n m g _ u s e _ e d g e _ g
 *
 * Associate edgeuse 'eu' with the edge_g_X structure given as
 * 'magic_p'.  If the edgeuse is already associated with some
 * geometry, release that first.  Note that, to start with, the two
 * edgeuses may be using different original geometries.
 *
 * Also do the edgeuse mate.
 *
 * Returns -
 * 0 If the old edge geometry (eu->g.magic_p) has other uses.
 * 1 If the old edge geometry has been destroyed. Caller beware!
 */
int
nmg_use_edge_g(struct edgeuse *eu, uint32_t *magic_p)
{
    struct edge_g_lseg *old;
    /* eg->eu_hd2 is a pun for eu_hd2 in either _lseg or _cnurb */
    struct edge_g_lseg *eg = (struct edge_g_lseg *)magic_p;
    int ndead = 0;

    if (!magic_p) return 0;	/* Don't use a null new geom */

    NMG_CK_EDGEUSE(eu);
    NMG_CK_EDGE_G_LSEG(eg);
    if (eu == eu->eumate_p) bu_bomb("nmg_use_edge_g() eu == eumate_p!\n");

    old = eu->g.lseg_p;	/* This may be NULL.  For printing only. */

    /* Handle edgeuse */
    if (eu->g.lseg_p != eg && eu->g.lseg_p) {

	NMG_CK_EDGE_G_LSEG(eu->g.lseg_p);

	BU_LIST_DEQUEUE(&eu->l2);
	ndead += nmg_keg(eu);
	eu->g.magic_p = NULL;
    }
    if (eu->g.lseg_p != eg) {
	BU_LIST_INSERT(&eg->eu_hd2, &(eu->l2));
	eu->g.magic_p = magic_p;
    }

    /* Handle edgeuse mate separately */
    if (eu->eumate_p->g.lseg_p != eg && eu->eumate_p->g.lseg_p) {
	struct edgeuse *mate = eu->eumate_p;

	NMG_CK_EDGEUSE(mate);
	NMG_CK_EDGE_G_LSEG(mate->g.lseg_p);

	BU_LIST_DEQUEUE(&mate->l2);
	ndead += nmg_keg(mate);
	mate->g.magic_p = NULL;
    }

    if (eu->eumate_p->g.lseg_p != eg) {
	BU_LIST_INSERT(&eg->eu_hd2, &(eu->eumate_p->l2));
	eu->eumate_p->g.magic_p = magic_p;
    }
    if (eu->g.magic_p != eu->eumate_p->g.magic_p) bu_bomb("nmg_use_edge_g() eu and mate not using same geometry?\n");

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_use_edge_g(eu=0x%p, magic_p=0x%p) old_eg=0x%p, ret=%d\n",
	       eu, magic_p, old, ndead);
    }
    return ndead;
}


/**
 * n m g _ l o o p _ g
 *
 * Build the bounding box for a loop.
 *
 * The bounding box is guaranteed never to have zero thickness.
 *
 * XXX This really isn't loop geometry, this is a loop attribute.
 * This routine really should be called nmg_loop_bb(), unless it gets
 * something more to do.
 */
void
nmg_loop_g(struct loop *l, const struct bn_tol *tol)
{
    struct edgeuse *eu;
    struct vertex_g *vg;
    struct loop_g *lg;
    struct loopuse *lu;
    struct model *m;
    uint32_t magic1;
    fastf_t thickening;

    NMG_CK_LOOP(l);
    BN_CK_TOL(tol);
    lu = l->lu_p;
    NMG_CK_LOOPUSE(lu);

    lg = l->lg_p;
    if (lg) {
	NMG_CK_LOOP_G(lg);
    } else {
	m = nmg_find_model(lu->up.magic_p);
	GET_LOOP_G(l->lg_p, m);
	lg = l->lg_p;
	lg->magic = NMG_LOOP_G_MAGIC;
    }
    VSETALL(lg->max_pt, -INFINITY);
    VSETALL(lg->min_pt,  INFINITY);

    magic1 = BU_LIST_FIRST_MAGIC(&lu->down_hd);
    if (magic1 == NMG_EDGEUSE_MAGIC) {
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    vg = eu->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg);
	    VMINMAX(lg->min_pt, lg->max_pt, vg->coord);
	    if (!eu->g.magic_p && eu->vu_p->v_p != eu->eumate_p->vu_p->v_p)
		nmg_edge_g(eu);
	}
    } else if (magic1 == NMG_VERTEXUSE_MAGIC) {
	struct vertexuse *vu;
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);
	vg = vu->v_p->vg_p;
	NMG_CK_VERTEX_G(vg);
	VMOVE(lg->min_pt, vg->coord);
	VMOVE(lg->max_pt, vg->coord);
    } else {
	bu_log("nmg_loop_g() loopuse down is %s (0x%lx)\n",
	       bu_identify_magic(magic1), magic1);
	bu_bomb("nmg_loop_g() loopuse has bad child\n");
    }

    /* Pad the dimension of the loop bounding box which is less than
     * distance tolerance so that the resulting dimension will be
     * at least distance tolerance. 
     */
    thickening = 0.5 * tol->dist;
    if (NEAR_ZERO(lg->max_pt[X] - lg->min_pt[X], tol->dist)) {
        lg->min_pt[X] -= thickening;
        lg->max_pt[X] += thickening;
    }
    if (NEAR_ZERO(lg->max_pt[Y] - lg->min_pt[Y], tol->dist)) {
        lg->min_pt[Y] -= thickening;
        lg->max_pt[Y] += thickening;
    }
    if (NEAR_ZERO(lg->max_pt[Z] - lg->min_pt[Z], tol->dist)) {
        lg->min_pt[Z] -= thickening;
        lg->max_pt[Z] += thickening;
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_loop_g(l=0x%p, tol=0x%p)\n", l, tol);
    }

}


/**
 * n m g _ f a c e _ g
 *
 * Assign plane equation to face.
 * XXX Should probably be called nmg_face_g_plane()
 *
 * In the interest of modularity this no longer calls nmg_face_bb().
 */
void
nmg_face_g(struct faceuse *fu, const fastf_t *p)
{
    int i;
    struct face_g_plane *fg;
    struct face *f;
    struct model *m;

    NMG_CK_FACEUSE(fu);
    f = fu->f_p;
    NMG_CK_FACE(f);

    fu->orientation = OT_SAME;
    fu->fumate_p->orientation = OT_OPPOSITE;

    fg = f->g.plane_p;
    if (fg) {
	/* Face already has face_g_plane associated with it */
	NMG_CK_FACE_G_PLANE(fg);
    } else {
	m = nmg_find_model(&fu->l.magic);
	GET_FACE_G_PLANE(f->g.plane_p, m);
	f->flip = 0;
	fg = f->g.plane_p;
	fg->magic = NMG_FACE_G_PLANE_MAGIC;
	BU_LIST_INIT(&fg->f_hd);
	BU_LIST_APPEND(&fg->f_hd, &f->l);
    }

    if (f->flip) {
	for (i=0; i < ELEMENTS_PER_PLANE; i++)
	    fg->N[i] = -p[i];
    } else {
	HMOVE(fg->N, p);
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_face_g(fu=0x%p, p=(%g %g %g %g))\n", fu, V4ARGS(p));
    }
}


/**
 * n m g _ f a c e _ n e w _ p l a n e
 *
 * Assign plane equation to this face. If other faces use current
 * geometry for this face, then make a new geometry for this face.
 */
void
nmg_face_new_g(struct faceuse *fu, const fastf_t *pl)
{
    struct face *f;
    struct face *f_tmp;
    struct face_g_plane *fg;
    struct model *m;
    int use_count=0;

    NMG_CK_FACEUSE(fu);
    f = fu->f_p;
    NMG_CK_FACE(f);
    fg = f->g.plane_p;

    /* if this face has no geometry, just call nmg_face_g() */
    if (!fg) {
	nmg_face_g(fu, pl);
	return;
    }

    /* count uses of this face geometry */
    for (BU_LIST_FOR (f_tmp, face, &fg->f_hd))
	use_count++;

    /* if this is the only use, just call nmg_face_g() */
    if (use_count < 2) {
	nmg_face_g(fu, pl);
	return;
    }

    /* There is at least one other use of this face geometry */

    fu->orientation = OT_SAME;
    fu->fumate_p->orientation = OT_OPPOSITE;

    /* dequeue this face from fg's face list */
    BU_LIST_DEQUEUE(&f->l);

    /* get a new geometry sructure */
    m = nmg_find_model(&fu->l.magic);
    GET_FACE_G_PLANE(f->g.plane_p, m);
    f->flip = 0;
    fg = f->g.plane_p;
    fg->magic = NMG_FACE_G_PLANE_MAGIC;
    BU_LIST_INIT(&fg->f_hd);
    BU_LIST_APPEND(&fg->f_hd, &f->l);

    HMOVE(fg->N, pl);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_face_new_g(fu=0x%p, pl=(%g %g %g %g))\n", fu, V4ARGS(pl));
    }
}


/**
 * n m g _ f a c e _ g _ s n u r b
 *
 * Create a new NURBS surface to be the geometry for an NMG face.
 *
 * If either of the knot vector arrays or the ctl_points arrays are
 * given as non-null, then simply swipe the caller's arrays.  The
 * caller must have allocated them with bu_malloc() or malloc().  If
 * the pointers are NULL, then the necessary storage is allocated
 * here.
 *
 * This is the NMG parallel to rt_nurb_new_snurb().
 */
void
nmg_face_g_snurb(struct faceuse *fu, int u_order, int v_order, int n_u_knots, int n_v_knots, fastf_t *ukv, fastf_t *vkv, int n_rows, int n_cols, int pt_type, fastf_t *mesh)
{
    struct face_g_snurb *fg;
    struct face *f;
    struct model *m;

    NMG_CK_FACEUSE(fu);
    f = fu->f_p;
    NMG_CK_FACE(f);

    fu->orientation = OT_SAME;
    fu->fumate_p->orientation = OT_OPPOSITE;

    fg = f->g.snurb_p;
    if (fg) {
	/* Face already has geometry associated with it */
	bu_bomb("nmg_face_g_snurb() face already has geometry\n");
    }

    m = nmg_find_model(&fu->l.magic);
    GET_FACE_G_SNURB(f->g.snurb_p, m);
    fg = f->g.snurb_p;

    fg->order[0] = u_order;
    fg->order[1] = v_order;
    fg->u.magic = NMG_KNOT_VECTOR_MAGIC;
    fg->v.magic = NMG_KNOT_VECTOR_MAGIC;
    fg->u.k_size = n_u_knots;
    fg->v.k_size = n_v_knots;

    if (ukv) {
	fg->u.knots = ukv;
    } else {
	fg->u.knots = (fastf_t *)bu_calloc(n_u_knots, sizeof(fastf_t), "u.knots[]");
    }
    if (vkv) {
	fg->v.knots = vkv;
    } else {
	fg->v.knots = (fastf_t *)bu_calloc(n_v_knots, sizeof(fastf_t), "v.knots[]");
    }

    fg->s_size[0] = n_rows;
    fg->s_size[1] = n_cols;
    fg->pt_type = pt_type;

    if (mesh) {
	fg->ctl_points = mesh;
    } else {
	int nwords;
	nwords = n_rows * n_cols * RT_NURB_EXTRACT_COORDS(pt_type);
	fg->ctl_points = (fastf_t *)bu_calloc(
	    nwords, sizeof(fastf_t), "snurb ctl_points[]");
    }

    f->flip = 0;
    BU_LIST_INIT(&fg->f_hd);
    BU_LIST_APPEND(&fg->f_hd, &f->l);
    fg->l.magic = NMG_FACE_G_SNURB_MAGIC;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_face_g_snurb(fu=0x%p, u_order=%d, v_order=%d, n_u_knots=%d, n_v_knots=%d, ukv=0x%p, vkv=0x%p, n_rows=%d, n_cols=%d, pt_type=0x%x, mesh=0x%p) fg=0x%p\n",
	       fu, u_order, v_order, n_u_knots, n_v_knots,
	       fg->u.knots, fg->v.knots,
	       n_rows, n_cols, (unsigned int)pt_type, fg->ctl_points, fg);
    }
}


/**
 * n m g _ f a c e _ b b
 *
 * Build the bounding box for a face
 */
void
nmg_face_bb(struct face *f, const struct bn_tol *tol)
{
    struct loopuse *lu;
    struct faceuse *fu;

    BN_CK_TOL(tol);
    NMG_CK_FACE(f);
    fu = f->fu_p;
    NMG_CK_FACEUSE(fu);

    f->max_pt[X] = f->max_pt[Y] = f->max_pt[Z] = -MAX_FASTF;
    f->min_pt[X] = f->min_pt[Y] = f->min_pt[Z] = MAX_FASTF;

    /* compute the extent of the face by looking at the extent of
     * each of the loop children.
     */
    for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	nmg_loop_g(lu->l_p, tol);

	if (lu->orientation != OT_BOOLPLACE) {
	    VMIN(f->min_pt, lu->l_p->lg_p->min_pt);
	    VMAX(f->max_pt, lu->l_p->lg_p->max_pt);
	}
    }

    /* Note, calculating the bounding box for face_g_snurbs
     * from the extents of the the loop does not work
     * since the loops are most likely in parametric space
     * thus we need to calcualte the bounding box for the
     * face_g_snurb here instead.  There may be a more efficient
     * way, and one may need some time to take a good look at
     * this
     */

    if (*fu->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC) {
	rt_nurb_s_bound(fu->f_p->g.snurb_p, fu->f_p->g.snurb_p->min_pt,
			fu->f_p->g.snurb_p->max_pt);
	VMIN(f->min_pt, fu->f_p->g.snurb_p->min_pt);
	VMAX(f->max_pt, fu->f_p->g.snurb_p->max_pt);
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_face_bb(f=0x%p, tol=0x%p)\n", f, tol);
    }
}


/**
 * n m g _ s h e l l _ a
 *
 * Build the bounding box for a shell
 */
void
nmg_shell_a(struct shell *s, const struct bn_tol *tol)
{
    struct shell_a *sa;
    struct vertex_g *vg;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct model *m;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (s->sa_p) {
	NMG_CK_SHELL_A(s->sa_p);
    } else {
	m = nmg_find_model(&s->l.magic);
	GET_SHELL_A(s->sa_p, m);
	s->sa_p->magic = NMG_SHELL_A_MAGIC;
    }
    sa = s->sa_p;

    VSETALL(sa->max_pt, -MAX_FASTF);
    VSETALL(sa->min_pt, MAX_FASTF);

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	struct face *f;

	f = fu->f_p;
	NMG_CK_FACE(f);
	nmg_face_bb(f, tol);

	VMIN(sa->min_pt, f->min_pt);
	VMAX(sa->max_pt, f->max_pt);

	/* If next faceuse shares this face, skip it */
	if (BU_LIST_NOT_HEAD(fu, &fu->l) &&
	    (BU_LIST_NEXT(faceuse, &fu->l)->f_p == f)) {
	    fu = BU_LIST_PNEXT(faceuse, fu);
	}
    }
    for (BU_LIST_FOR (lu, loopuse, &s->lu_hd)) {
	nmg_loop_g(lu->l_p, tol);

	VMIN(sa->min_pt, lu->l_p->lg_p->min_pt);
	VMAX(sa->max_pt, lu->l_p->lg_p->max_pt);
    }
    for (BU_LIST_FOR (eu, edgeuse, &s->eu_hd)) {
	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGE(eu->e_p);
	vg = eu->vu_p->v_p->vg_p;
	NMG_CK_VERTEX_G(vg);
	VMINMAX(sa->min_pt, sa->max_pt, vg->coord);
    }
    if (s->vu_p) {
	NMG_CK_VERTEXUSE(s->vu_p);
	NMG_CK_VERTEX(s->vu_p->v_p);
	if (s->vu_p->v_p->vg_p) {
	    NMG_CK_VERTEX_G(s->vu_p->v_p->vg_p);
	    vg = s->vu_p->v_p->vg_p;
	    VMINMAX(sa->min_pt, sa->max_pt, vg->coord);
	}
    }


    if (BU_LIST_IS_EMPTY(&s->fu_hd) &&
	BU_LIST_IS_EMPTY(&s->lu_hd) &&
	BU_LIST_IS_EMPTY(&s->eu_hd) && !s->vu_p) {
	bu_log("nmg_shell_a() at %d in %s. Shell has no children\n",
	       __LINE__, __FILE__);
	bu_bomb("nmg_shell_a\n");
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_shell_a(s=0x%p, tol=0x%p)\n", s, tol);
    }
}


/**
 * n m g _ r e g i o n _ a
 *
 * build attributes/extents for all shells in a region
 *
 */
void
nmg_region_a(struct nmgregion *r, const struct bn_tol *tol)
{
    register struct shell *s;
    struct nmgregion_a *ra;

    NMG_CK_REGION(r);
    BN_CK_TOL(tol);
    if (r->ra_p) {
	ra = r->ra_p;
	NMG_CK_REGION_A(ra);
    } else {
	GET_REGION_A(ra, r->m_p);
	r->ra_p = ra;
    }

    ra->magic = NMG_REGION_A_MAGIC;

    VSETALL(ra->max_pt, -MAX_FASTF);
    VSETALL(ra->min_pt, MAX_FASTF);

    for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	nmg_shell_a(s, tol);
	NMG_CK_SHELL_A(s->sa_p);
	VMIN(ra->min_pt, s->sa_p->min_pt);
	VMAX(ra->max_pt, s->sa_p->max_pt);
    }

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_region_a(r=0x%p, tol=0x%p)\n", r, tol);
    }
}


/************************************************************************
 *									*
 *				"Demote" Routines			*
 *									*
 * As part of the boolean operations, sometimes it is desirable	*
 * to "demote" entities down the "chain of being".  The sequence is:	*
 *									*
 *	face --> wire loop --> wire edge (line segment) --> vertex	*
 *									*
 ************************************************************************/


/**
 * n m g _ d e m o t e _ l u
 *
 * Demote a loopuse of edgeuses to a bunch of wire edges in the shell.
 *
 * Returns -
 * 0 If all is well (edges moved to shell, loopuse deleted).
 * 1 If parent is empty, and is thus "illegal".  Still successful.
 */
int
nmg_demote_lu(struct loopuse *lu1)
{
    struct edgeuse *eu1;
    struct shell *s;
    int ret_val;

    NMG_CK_LOOPUSE(lu1);

    if (rt_g.NMG_debug & DEBUG_CLASSIFY)
	bu_log("nmg_demote_lu(0x%p)\n", lu1);

    if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC) {
	bu_bomb("nmg_demote_lu() demoting loopuse of a single vertex\n");
    }

    if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) != NMG_EDGEUSE_MAGIC)
	bu_bomb("nmg_demote_lu: bad loopuse child\n");

    /* get the parent shell */
    s = nmg_find_s_of_lu(lu1);
    NMG_CK_SHELL(s);

    /* move all edgeuses (&mates) to shell
     */
    while (BU_LIST_NON_EMPTY(&lu1->down_hd)) {

	eu1 = BU_LIST_FIRST(edgeuse, &lu1->down_hd);
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGE(eu1->e_p);
	NMG_CK_EDGEUSE(eu1->eumate_p);
	NMG_CK_EDGE(eu1->eumate_p->e_p);

	BU_LIST_DEQUEUE(&eu1->eumate_p->l);
	BU_LIST_APPEND(&s->eu_hd, &eu1->eumate_p->l);

	BU_LIST_DEQUEUE(&eu1->l);
	BU_LIST_APPEND(&s->eu_hd, &eu1->l);

	eu1->up.s_p = eu1->eumate_p->up.s_p = s;
    }
    /* lu1 is in an illegal state here, with a null edge list */

    if (BU_LIST_NON_EMPTY(&lu1->lumate_p->down_hd))
	bu_bomb("nmg_demote_lu: loopuse mates don't have same # of edges\n");

    ret_val = nmg_klu(lu1);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_demote_lu(lu=0x%p) returns %d\n", lu1, ret_val);
    }

    return ret_val;
}


/**
 * n m g _ d e m o t e _ e u
 *
 * Demote a wire edge into a pair of self-loop verticies
 *
 *
 * Returns -
 * 0 If all is well
 * 1 If shell is empty, and is thus "illegal".
 */
int
nmg_demote_eu(struct edgeuse *eu)
{
    struct shell *s;
    struct vertex *v;
    int ret_val;

    if (*eu->up.magic_p != NMG_SHELL_MAGIC)
	bu_bomb("nmg_demote_eu() up is not shell\n");
    s = eu->up.s_p;
    NMG_CK_SHELL(s);

    NMG_CK_EDGEUSE(eu);
    v = eu->vu_p->v_p;
    if (!nmg_is_vertex_a_selfloop_in_shell(v, s))
	(void)nmg_mlv(&s->l.magic, v, OT_SAME);

    NMG_CK_EDGEUSE(eu->eumate_p);
    v = eu->eumate_p->vu_p->v_p;
    if (!nmg_is_vertex_a_selfloop_in_shell(v, s))
	(void)nmg_mlv(&s->l.magic, v, OT_SAME);

    (void)nmg_keu(eu);

    ret_val = nmg_shell_is_empty(s);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_demote_eu(eu=0x%p) returns %d\n", eu, ret_val);
    }

    return ret_val;
}


/************************************************************************
 *									*
 *				"Modify" Routines			*
 *									*
 * These routines would go in nmg_mod.c, except that they create	*
 * or delete fundamental entities directly as part of their operation.	*
 * Thus, they are part of the make/kill purpose of this module.	*
 *									*
 ************************************************************************/


/**
 * n m g _ m o v e v u
 *
 * Move a vertexuse from an old vertex to a new vertex.  If this was
 * the last use, the old vertex is destroyed.
 *
 * XXX nmg_jvu() as a better name?
 */
void
nmg_movevu(struct vertexuse *vu, struct vertex *v)
{
    struct vertex *oldv;

    NMG_CK_VERTEXUSE(vu);
    NMG_CK_VERTEX(v);

    oldv = vu->v_p;
    NMG_CK_VERTEX(oldv);

    BU_LIST_DEQUEUE(&vu->l);
    if (BU_LIST_IS_EMPTY(&oldv->vu_hd)) {
	/* last vertexuse on vertex */
	if (oldv->vg_p) FREE_VERTEX_G(oldv->vg_p);
	FREE_VERTEX(oldv);
    }
    BU_LIST_APPEND(&v->vu_hd, &vu->l);
    vu->v_p = v;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_movevu(vu=0x%p, v=0x%p)\n", vu, v);
    }
}


/**
 * n m g _ j e
 *
 * Move a pair of edgeuses onto a single edge (glue edgeuse).  The
 * edgeuse eusrc and its mate are moved to the edge used by eudst.
 * eusrc is made to be immediately radial to eudst.  if eusrc does not
 * share the same vertices as eudst, we bomb.
 *
 * The edgeuse geometry pointers are not changed by this operation.
 *
 * This routine was formerly called nmg_moveeu().
 */
void
nmg_je(struct edgeuse *eudst, struct edgeuse *eusrc)
{
    struct edgeuse *eudst_mate;
    struct edgeuse *eusrc_mate;
    struct edge *e;

    NMG_CK_EDGEUSE(eudst);
    NMG_CK_EDGEUSE(eusrc);
    eudst_mate = eudst->eumate_p;
    eusrc_mate = eusrc->eumate_p;
    NMG_CK_EDGEUSE(eudst_mate);
    NMG_CK_EDGEUSE(eusrc_mate);

    /* protect the morons from themselves.  Don't let them
     * move an edgeuse to itself or its mate
     */
    if (eusrc == eudst || eusrc_mate == eudst) {
	bu_log("nmg_je() moving edgeuse to itself\n");
	return;
    }

    if (eusrc->e_p == eudst->e_p &&
	(eusrc->radial_p == eudst || eudst->radial_p == eusrc)) {
	bu_log("nmg_je() edgeuses already share edge\n");
	return;
    }

    /* make sure vertices are shared */
    if (! ((eudst_mate->vu_p->v_p == eusrc->vu_p->v_p &&
	    eudst->vu_p->v_p == eusrc_mate->vu_p->v_p) ||
	   (eudst->vu_p->v_p == eusrc->vu_p->v_p &&
	    eudst_mate->vu_p->v_p == eusrc_mate->vu_p->v_p))) {
	/* edgeuses do NOT share verticies. */
	bu_log("eusrc (v=0x%p) (%g %g %g)\n", eusrc->vu_p->v_p, V3ARGS(eusrc->vu_p->v_p->vg_p->coord));
	bu_log("eusrc_mate (v=0x%p) (%g %g %g)\n", eusrc_mate->vu_p->v_p, V3ARGS(eusrc_mate->vu_p->v_p->vg_p->coord));
	bu_log("eudst (v=0x%p) (%g %g %g)\n", eudst->vu_p->v_p, V3ARGS(eudst->vu_p->v_p->vg_p->coord));
	bu_log("eudst_mate (v=0x%p) (%g %g %g)\n", eudst_mate->vu_p->v_p, V3ARGS(eudst_mate->vu_p->v_p->vg_p->coord));
	bu_bomb("nmg_je() edgeuses do not share vertices, cannot share edge\n");
    }

    e = eusrc->e_p;
    eusrc_mate->e_p = eusrc->e_p = eudst->e_p;

    /* if we're not deleting the edge, make sure it will be able
     * to reference the remaining uses, otherwise, take care of disposing
     * of the (now unused) edge
     */
    if (eusrc->radial_p != eusrc_mate) {
	/* this is NOT the only use of the eusrc edge! */
	if (e->eu_p == eusrc || e->eu_p == eusrc_mate)
	    e->eu_p = eusrc->radial_p;

	/* disconnect from the list of uses of this edge */
	eusrc->radial_p->radial_p = eusrc_mate->radial_p;
	eusrc_mate->radial_p->radial_p = eusrc->radial_p;
    } else {
	/* FIXME: this used to call FREE_EDGE() here but is now a
	 * potential memory leak.  need to evaluate callers of
	 * nmg_je() to make sure they are managing memory release
	 * properly and/or re-add release of memory in here if needed.
	 */

	/* this is the only use of the eusrc edge. kill/free edge but not
         * here. the edge should be freed by the calling function which can
         * determine this edge should be freed by its edgeuse pointer
         * pointing to itself and the magic number set to zero.
         */
        e->eu_p = (struct edgeuse *)e;
        e->magic = 0;
    }

    eusrc->radial_p = eudst;
    eusrc_mate->radial_p = eudst->radial_p;

    eudst->radial_p->radial_p = eusrc_mate;
    eudst->radial_p = eusrc;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_je(eudst=0x%p, eusrc=0x%p)\n", eudst, eusrc);
    }
}


/**
 * n m g _ u n g l u e e d g e
 *
 * If edgeuse is part of a shared edge (more than one pair of edgeuses
 * on the edge), it and its mate are "unglued" from the edge, and
 * associated with a new edge structure.
 *
 * Primarily a support routine for nmg_eusplit()
 *
 * If the original edge had edge geometry, that is *not* duplicated
 * here, because it is not easy (or appropriate) for nmg_eusplit() to
 * know whether the new vertex lies on the previous edge geometry or
 * not.  Hence having the nmg_ebreak() interface, which preserves the
 * ege geometry across a split, and nmg_esplit() which does not.
 */
void
nmg_unglueedge(struct edgeuse *eu)
{
    struct edge *old_e;
    struct edge *new_e;
    struct model *m;

    NMG_CK_EDGEUSE(eu);
    old_e = eu->e_p;
    NMG_CK_EDGE(old_e);

    /* if we're already a single edge, just return */
    if (eu->radial_p == eu->eumate_p) {
	if (rt_g.NMG_debug & DEBUG_BASIC) {
	    bu_log("nmg_unglueedge(eu=0x%p) (nothing unglued)\n", eu);
	}
	return;
    }

    m = nmg_find_model(&eu->l.magic);
    GET_EDGE(new_e, m);		/* create new edge */

    new_e->magic = NMG_EDGE_MAGIC;
    new_e->eu_p = eu;

    /* make sure the old edge isn't pointing at this edgeuse */
    if (old_e->eu_p == eu || old_e->eu_p == eu->eumate_p) {
	old_e->eu_p = old_e->eu_p->radial_p;
    }

    /* unlink edgeuses from old edge */
    eu->radial_p->radial_p = eu->eumate_p->radial_p;
    eu->eumate_p->radial_p->radial_p = eu->radial_p;
    eu->eumate_p->radial_p = eu;
    eu->radial_p = eu->eumate_p;

    /* Associate edgeuse and mate with new edge */
    eu->eumate_p->e_p = eu->e_p = new_e;

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_unglueedge(eu=0x%p)\n", eu);
    }
}


/**
 * n m g _ j v
 *
 * Join two vertexes into one.
 *
 * v1 inherits all the vertexuses presently pointing to v2, and v2 is
 * then destroyed.
 */
void
nmg_jv(register struct vertex *v1, register struct vertex *v2)
{
    register struct vertexuse *vu;

    NMG_CK_VERTEX(v1);
    NMG_CK_VERTEX(v2);

    if (v1 == v2) return;

    /*
     * Walk the v2 list, unlinking vertexuse structs,
     * and adding them to the *end* of the v1 list
     * (which preserves relative ordering).
     */
    vu = BU_LIST_FIRST(vertexuse, &v2->vu_hd);
    while (BU_LIST_NOT_HEAD(vu, &v2->vu_hd)) {
	register struct vertexuse *vunext;

	NMG_CK_VERTEXUSE(vu);
	vunext = BU_LIST_PNEXT(vertexuse, vu);
	BU_LIST_DEQUEUE(&vu->l);
	BU_LIST_INSERT(&v1->vu_hd, &vu->l);
	vu->v_p = v1;		/* "up" to new vertex */
	vu = vunext;
    }

    /* Kill vertex v2 */
    if (v2->vg_p) {
	if (!v1->vg_p) {
	    v1->vg_p = v2->vg_p;
	} else {
	    FREE_VERTEX_G(v2->vg_p);
	}
    }
    FREE_VERTEX(v2);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_jv(v1=0x%p, v2=0x%p)\n", v1, v2);
    }
}


/**
 * n m g _ j f g
 *
 * Join two faces, so that they share one underlying face geometry.
 * The loops of the two faces remains unchanged.
 *
 * If one of the faces does not have any geometry, then it is made to
 * share the geometry of the other.
 */
void
nmg_jfg(struct face *f1, struct face *f2)
{
    struct face_g_plane *fg1;
    struct face_g_plane *fg2;
    struct face *f;

    NMG_CK_FACE(f1);
    NMG_CK_FACE(f2);
    fg1 = f1->g.plane_p;
    fg2 = f2->g.plane_p;
    if (fg2 && !fg1) {
	/* Make f1 share existing geometry of f2 */
	NMG_CK_FACE_G_PLANE(fg1);
	f1->g.plane_p = fg2;
	f1->flip = f2->flip;
	BU_LIST_INSERT(&fg2->f_hd, &f1->l);

	if (rt_g.NMG_debug & DEBUG_BASIC) {
	    bu_log("nmg_jfg(f1=0x%p, f2=0x%p)\n", f1, f2);
	}
	return;
    }
    if (fg1 && !fg2) {
	/* Make f2 share existing geometry of f1 */
	NMG_CK_FACE_G_PLANE(fg1);
	f2->g.plane_p = fg1;
	f2->flip = f1->flip;
	BU_LIST_INSERT(&fg1->f_hd, &f2->l);

	if (rt_g.NMG_debug & DEBUG_BASIC) {
	    bu_log("nmg_jfg(f1=0x%p, f2=0x%p)\n", f1, f2);
	}
	return;
    }

    NMG_CK_FACE_G_PLANE(fg1);
    NMG_CK_FACE_G_PLANE(fg2);

    if (fg1 == fg2) {
	if (rt_g.NMG_debug & DEBUG_BASIC) {
	    bu_log("nmg_jfg(f1=0x%p, f2=0x%p)\n", f1, f2);
	}
	return;
    }

    /* Unhook all the faces on fg2 list, and add to fg1 list */
    while (BU_LIST_NON_EMPTY(&fg2->f_hd)) {
	f = BU_LIST_FIRST(face, &fg2->f_hd);
	BU_LIST_DEQUEUE(&f->l);
	NMG_CK_FACE(f);
	f->g.plane_p = fg1;
	/* flip flag is left unchanged here, on purpose */
	BU_LIST_INSERT(&fg1->f_hd, &f->l);
    }

    /* fg2 list is now empty, release that face geometry */
    FREE_FACE_G_PLANE(fg2);

    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_jfg(f1=0x%p, f2=0x%p)\n", f1, f2);
    }
}


/**
 * n m g _ j e g
 *
 * Join two edge geometries.
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
void
nmg_jeg(struct edge_g_lseg *dest_eg, struct edge_g_lseg *src_eg)
{
    register struct edgeuse *eu;

    NMG_CK_EDGE_G_LSEG(src_eg);
    NMG_CK_EDGE_G_LSEG(dest_eg);
    if (rt_g.NMG_debug & DEBUG_BASIC) {
	bu_log("nmg_jeg(src_eg=0x%p, dest_eg=0x%p)\n",
	       src_eg, dest_eg);
    }

    while (BU_LIST_NON_EMPTY(&src_eg->eu_hd2)) {
	struct bu_list *midway;	/* &eu->l2, midway into edgeuse */

	NMG_CK_EDGE_G_LSEG(src_eg);

	/* Obtain an eu from src_eg */
	midway = BU_LIST_FIRST(bu_list, &src_eg->eu_hd2);
	NMG_CKMAG(midway, NMG_EDGEUSE2_MAGIC, "edgeuse2 [l2]");
	eu = BU_LIST_MAIN_PTR(edgeuse, midway, l2);
	NMG_CK_EDGEUSE(eu);

	if (eu->g.lseg_p != src_eg) {
	    bu_log("nmg_jeg() eu=0x%p, eu->g=0x%p != src_eg=0x%p??  dest_eg=0x%p\n",
		   eu, eu->g.lseg_p, src_eg, dest_eg);
	    bu_bomb("nmg_jeg() edge geometry fumble\n");
	}

	/* Associate eu and mate with dest_eg. src_eg freed when unused. */
	if (nmg_use_edge_g(eu, &dest_eg->l.magic))
	    break;		/* src_eg destroyed */
    }
}


/**
 * n m g _ k e u _ z l
 *
 * Kill zero length edgeuse from a shell and
 * return the number of edgeuse killed. If the
 * shell becomes empty, this function will bomb.
 *
 */
int
nmg_keu_zl(struct shell *s, const struct bn_tol *tol) 
{
    int empty_loop = 0;
    int empty_face = 0;
    int empty_shell = 0;
    int eu_killed = 0;
    struct loopuse *lu;
    struct faceuse *fu;
    struct edgeuse *eu;

    NMG_CK_SHELL(s);

    eu_killed = 0;
    empty_shell = 0;
    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
    while(BU_LIST_NOT_HEAD(fu, &s->fu_hd)) {
	NMG_CK_FACEUSE(fu);
	if (fu->orientation != OT_SAME) {
	    fu = BU_LIST_PNEXT(faceuse, fu);
	    continue;
	}
        empty_face = 0;
	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
        while(BU_LIST_NOT_HEAD(lu, &fu->lu_hd)) {
            NMG_CK_LOOPUSE(lu);
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
	        bu_bomb("loopuse does not contains edgeuse\n");
	    }
	    empty_loop = 0;
	    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    while(BU_LIST_NOT_HEAD(eu, &lu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
	        if ((eu->vu_p->v_p->vg_p == eu->eumate_p->vu_p->v_p->vg_p) ||
		     bn_pt3_pt3_equal(eu->vu_p->v_p->vg_p->coord, 
                                     eu->eumate_p->vu_p->v_p->vg_p->coord, tol)) {
		    /* fuse the two vertices */
		    nmg_jv(eu->vu_p->v_p, eu->eumate_p->vu_p->v_p);

		    eu_killed++;
		    if (nmg_keu(eu)) {
		        empty_loop = 1;
		    }
		    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	        } else {
		    eu = BU_LIST_PNEXT(edgeuse, eu);
	        } 
	    }
	    if (empty_loop) {
	        if (nmg_klu(lu)) {
		    empty_face = 1;
	        }
	        lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    } else {
	        lu = BU_LIST_PNEXT(loopuse, lu);
	    }
        }

        if (empty_face) {
	    if (nmg_kfu(fu)) {
	        empty_shell = 1;
	    }
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
        } else {
	    fu = BU_LIST_PNEXT(faceuse, fu);
        }
    }

    if (empty_shell) {
	bu_bomb("nmg_keu_zl(): removing zero length edgeuse resulted in an empty shell\n");
    }

    return eu_killed;
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

