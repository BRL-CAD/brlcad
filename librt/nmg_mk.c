/*
 *			N M G _ M K . C
 *
 *	Support routines for n-Manifold Geometry
 *
 *	Naming convention
 *		nmg_m* routines "make" NMG structures.
 *		nmg_k* routines "kill" (delete) NMG structures.
 *
 *	in each of the above cases the letters or words following are an 
 *	attempt at a mnemonic representation for what is manipulated
 *
 *	m	Model
 *	r	Region
 *	s	shell
 *	f	face
 *	fu	faceuse
 *	l	loop
 *	lu	loopuse
 *	e	edge
 *	eu	edgeuse
 *	v	vertex
 *	vu	vertexuse
 *
 *
 *	Rules:
 *
 * XXX - What does "overlap" mean? ctj
 *	edges of loops of the same face must not overlap
 *	the "magic" member of each struct is the first item.
 *
 *	All routines which create and destroy the NMG data structures
 *	are contained in this module.
 *
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

static struct vertexuse *nmg_mvu RT_ARGS( (struct vertex *v, long *upptr,
					 struct model *m) );
static struct vertexuse *nmg_mvvu RT_ARGS( (long *upptr, struct model *m) );


/************************************************************************
 *									*
 *			"Make" Routines					*
 *									*
 *  The subroutines create new topological entities and return a	*
 *  pointer to the new entity.						*
 *									*
 ************************************************************************/

/*
 *  The nmg_m*() routines are used to create a topology-only object
 *  which at first has no geometry associated with it at all.
 *  A topology-only object can be used to answer questions like:
 *	is this vertex ON this shell?
 *	is this vertex ON this face?
 *	is this vertex ON this edge?
 *	Is this face ON this shell?
 * and many more.
 *
 *  After the topology has been built, most applications will proceed to
 *  associate geometry with the topology, primarily by supplying
 *  Cartesian coordinates for each struct vertex, and by supplying
 *  or computing face normals for the planar faces.  (Linear edge geometry
 *  is optional, as it is implicit from the vertex geometry).
 *
 *  Objects which have been fully populated with geometry can be used to
 *  answer questions about where things are located and how large they are.
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
 *	model_a		(not used)
 *	nmgregion_a	(region bounding box)	[nmgregions have no uses]
 *	shell_a		(shell bounding box)	[shells have no uses]
 *	faceuse_a	(not used)
 *	loopuse_a	(not used)
 *	edgeuse_a	(not used)
 *	vertexuse_a	(special surface normal, for normal interpolation)
 *
 * Consider for example a simple cube.
 *
 * As a topology-only object, it would have the following structures:
 *
 *	1 model structure
 *		This is the handle which everything else hangs on.
 *		The model structure r_hd references 1 region structure.
 *	1 nmgregion structure.
 *		The region structure s_hd references 1 shell structure.
 *		Also, m_p references the model.
 *	1 shell structure.
 *		The shell structure fu_hd references 12 faceuse structures.
 *		One for each side of each of the 6 faces of the cube.
 *		Also, r_p references the nmgregion.
 *	12 faceuse structures.
 *		Each faceuse structure lu_hd references 1 loopuse structure.
 *		Also, 1 face structure and 1 faceuse structure (its mate),
 *		plus s_p references the shell.
 *	6 face structures.
 *		Each face structure fu_p references a random choice of 1 of
 *		the two parent faceuse structures sharing this face, and is
 *		in turn referenced by that faceuse and it's mate.
 *	12 loopuse structures
 *		Each loopuse structure down_hd references 4 edgeuse structures.
 *		Also, 1 loop structure, and 1 loopuse structure (its mate).
 *		The four edgeuse structures define the perimeter of the
 *		surface area that comprises this face.
 *		Because their orientation is OT_SAME, each loopuse "claims"
 *		all the surface area inside it for the face.
 *		(OT_OPPOSITE makes a hole, claiming surface area outside).
 *		Plus, "up" references the parent object (faceuse, here).
 *	6 loop structures
 *		Each loop structure references a random choice of 1 of it's
 *		parent loopuse structures and is in turn referenced by that
 *		loopuse and it's mate.
 *	48 edgeuse structures
 *		Each edgeuse structure references 1 vertexuse structure,
 *		1 edge structure, and 2 other edgeuse structures (it's mate
 *		and the next edgeuse radial to this edgeuse).
 *		(if this edge was NOT used in another face, then the
 *		radial pointer and mate pointer would point to the SAME
 *		edgeuse)
 *		To find all edgeuses around a given edge, follow radial to
 *		mate to radial to mate until you are back to the original
 *		edgeuse.
 *		Plus, "up" references the parent object (loopuse, here).
 *	12 edge structures
 *		Each edge structure references a random choice of one of
 *		it's parent edgeuse structures and is in turn referenced
 *		by that edgeuse, the mate of that edgeuse, the radial of
 *		that edgeuse and the mate of the radial. (In this simple
 *		case of the cube, there are 4 edgeuses per edge).
 *	48 vertexuse structures.
 *		Each vertexuse structure references one vertex structure
 *		and is in turn enroled as a member of the linked list
 *		headed by that vertex structure.
 *		Each vertexuse is cited by exactly one edgeuse.
 *		Also, "up" references the parent object (edgeuse, here).
 *	8 vertex structures
 *		Each vertex structure references 6 vertexuse structures
 *		via it's linked list. (In the case of the cube,
 *		there are three faces meeting at each vertex, and each of
 *		those faces has two faceuse structs of one loopuse each. Each
 *		loopuse will cite the vertex via one edgeuse, so 3*2 = 6).
 *
 * As well as each "use" pointing down to what it uses, the "use" points
 * up to the structure that uses it.  So working up from any abstract object
 * or it's use, the top of the tree (struct model) can be found.
 * Working down from the struct model, all elements of the object can be
 * visited.
 *
 * The object just described contains no geometry.  There is no way to tell
 * where in space the object lies or how big it is.
 *
 * To add geometry, the following structures are needed:
 *	8 vertex_g structures
 *		each vertex_g structure contains a point in space (point_t)
 *		and is referenced by 1 vertex structure.
 *	12 edge_g structures (completely optional)
 *		each edge_g structure contains the parametric definition of
 *		the line which contains the line segment which is the edge,
 *		given as a point in space and a direction vector.  It is
 *		referenced by 1 edge structure. (In general, it is referenced
 *		by all edges sharing that line).
 *		In a simple case the point would be the same as one of
 *		the vertex_g points and the direction would be the normalized
 *		(unit) vector of the difference between the two vertex_g points.
 *	6 loop_g structures
 *		Each loop_g structure contains a bounding box for the loop.
 *		It is referenced by 1 loop structure.
 *	6 face_g structures
 *		Each face_g structure contains a plane equation and
 *		a bounding box.  It is referenced by one face structure.
 *		The plane equation is calculated from the vertex_g data
 *		by nmg_fu_planeeqn().
 *		See h/vmath.h for the definition of a plane equation.
 *	1 shell_a structure
 *		Each shell_a structure contains the bounding box for the
 *		shell.  It is referenced by 1 shell structure.
 *	1 nmgregion_a structure
 *		Each nmgregion_a structure contains the bounding box for the
 *		region.  It is referenced by 1 region structure.
 *
 */

/*
 *			N M G _ M M
 *
 *	Make Model
 *	Create a new model.  The region list is empty.
 *	Creates a new model structure.  The model region structure list
 *	is empty.
 *
 *  Returns -
 *	(struct model *)
 *
 *  Method:
 *	Use NMG_GETSTRUCT to allocate memory and then set all components.
 *	NMG_GETSTRUCT is used instead of the standard GET_name because
 *	all of the GET_name macros expect a model pointer to get the
 *	maxindex from.  So here we use NMG_GETSTRUCT so that we can
 *	set the maxindex and index by hand.
 *
 *  N.B.:
 *	"maxindex" is a misnomer.  It is the value of the NEXT index
 *	assigned.  This allows "ptab"s to be allocated easly using
 *	maxindex and the index value of the structures to be the actual
 *	index into the "ptab".
 */
struct model *
nmg_mm()
{
	struct model *m;

	NMG_GETSTRUCT( m, model );

	m->ma_p = (struct model_a *)NULL;
	RT_LIST_INIT( &m->r_hd );
	m->index = 0;
	m->maxindex = 1;
	m->magic = NMG_MODEL_MAGIC;	/* Model Structure is GOOD */

	return(m);
}

/*
 *			N M G _ M M R
 *
 *	Make Model and Region
 *	Create a new model, and an "empty" region to go with it.  Essentially
 *	this creates a minimal model system.
 *
 *  Returns -
 *	(struct model *)
 *
 *  Implicit Return -
 *	The new region is found with RT_LIST_FIRST( nmgregion, &m->r_hd );
 */
struct model *
nmg_mmr()
{
	struct model *m;
	struct nmgregion *r;

	m = nmg_mm();
	GET_REGION(r, m);

	r->m_p = m;

	r->ra_p = (struct nmgregion_a *)NULL;
	RT_LIST_INIT( &r->s_hd );
	r->l.magic = NMG_REGION_MAGIC;	/* Region Structure is GOOD */

	RT_LIST_APPEND( &m->r_hd, &r->l );

	return(m);
}

/*
 *			N M G _ M R S V
 *
 *	Make new region, shell, vertex in model as well as the
 *	required "uses".
 *	Create a new region in model consisting of a minimal shell.
 *
 *  Returns -
 *	(struct nmgregion *)
 *
 *  Implicit Returns -
 *	Region is also found with r=RT_LIST_FIRST( nmgregion, &m->r_hd );
 *	The new shell is found with s=RT_LIST_FIRST( shell, &r->s_hd );
 *	The new vertexuse is s->vu_p;
 */
struct nmgregion *
nmg_mrsv(m)
struct model *m;
{
	struct nmgregion *r;

	NMG_CK_MODEL(m);

	GET_REGION(r, m);
	r->m_p = m;
	r->ra_p = (struct nmgregion_a *) NULL;

	RT_LIST_INIT( &r->s_hd );
	r->l.magic = NMG_REGION_MAGIC;	/* Region struct is GOOD */

	(void)nmg_msv(r);

	/* new region goes at "head" of list of regions in model */
	RT_LIST_APPEND( &m->r_hd, &r->l );

	return(r);
}

/*
 *			N M G _ M S V
 *
 *	Make Shell, Vertex Use, Vertex
 *	Create a new shell in a specified region.  The shell will consist
 *	of a single vertexuse and vertex (which are also created).
 *
 *  Returns -
 *	(struct shell *)
 *
 *  Implicit Returns -
 *	The new shell is also found with s=RT_LIST_FIRST( shell, &r->s_hd );
 *	The new vertexuse is s->vu_p;
 */
struct shell *
nmg_msv(r)
struct nmgregion	*r;
{
	struct shell 		*s;
	struct vertexuse	*vu;

	NMG_CK_REGION(r);

	/* set up shell */
	GET_SHELL(s, r->m_p);

	s->r_p = r;
	RT_LIST_APPEND( &r->s_hd, &s->l );

	s->sa_p = (struct shell_a *)NULL;
	RT_LIST_INIT( &s->fu_hd );
	RT_LIST_INIT( &s->lu_hd );
	RT_LIST_INIT( &s->eu_hd );
	s->vu_p = (struct vertexuse *) NULL;
	s->l.magic = NMG_SHELL_MAGIC;	/* Shell Struct is GOOD */

	vu = nmg_mvvu(&s->l.magic, r->m_p);
	s->vu_p = vu;
	return(s);
}

/*
 *			N M G _ M F
 *
 *	Make Face from a wire loop.
 *	make a face from a pair of loopuses.  The loopuses must be direct
 *	children of a shell.  The new face will be a child of the same shell.
 *
 * Given a wire loop (by definition, a loop attached to a shell), create
 * a new face, faceuse (and mate) and move the wire loop from the shell
 * to the new faceuse (and mate).
 *
 *  Implicit Returns -
 *	The first faceuse is fu1=RT_LIST_FIRST( faceuse, &s->fu_hd );
 *	The second faceuse follows:  fu2=RT_LIST_NEXT( faceuse, &fu1->l.magic );
 */
struct faceuse *
nmg_mf(lu1)
struct loopuse *lu1;
{
	struct face *f;
	struct faceuse *fu1, *fu2;
	struct loopuse *lu2;
	struct shell	*s;
	struct model	*m;

	NMG_CK_LOOPUSE(lu1);
	if (*lu1->up.magic_p != NMG_SHELL_MAGIC) {
		rt_bomb("nmg_mf() loop must be child of shell for making face\n");
	}
	lu2 = lu1->lumate_p;
	NMG_CK_LOOPUSE(lu2);
	if (lu2->up.magic_p != lu1->up.magic_p) {
		rt_bomb("nmg_mf() loopuse mate does not have same parent\n");
	}

	s = lu1->up.s_p;
	NMG_CK_SHELL(s);

	m = nmg_find_model( &s->l.magic );
	GET_FACE(f, m);
	GET_FACEUSE(fu1, m);
	GET_FACEUSE(fu2, m);

	f->fu_p = fu1;
	f->fg_p = (struct face_g *)NULL;
	f->magic = NMG_FACE_MAGIC;	/* Face struct is GOOD */

	RT_LIST_INIT(&fu1->lu_hd);
	RT_LIST_INIT(&fu2->lu_hd);
	fu1->s_p = fu2->s_p = s;
	fu1->fumate_p = fu2;
	fu2->fumate_p = fu1;
	fu1->orientation = fu2->orientation = OT_UNSPEC;
	fu1->f_p = fu2->f_p = f;
	fu1->fua_p = fu2->fua_p = (struct faceuse_a *)NULL;
	fu1->l.magic = 
	    fu2->l.magic = NMG_FACEUSE_MAGIC; /* Faceuse structs are GOOD */

	/* move the loopuses from the shell to the faceuses */
	RT_LIST_DEQUEUE( &lu1->l );
	RT_LIST_DEQUEUE( &lu2->l );
	RT_LIST_APPEND( &fu1->lu_hd, &lu1->l );
	RT_LIST_APPEND( &fu2->lu_hd, &lu2->l );

	lu1->up.fu_p = fu1;
	lu1->orientation = OT_SAME;
	lu2->up.fu_p = fu2;
	lu2->orientation = OT_SAME;

	/* connect the faces to the parent shell:  head, fu1, fu2... */
	RT_LIST_APPEND( &s->fu_hd, &fu1->l );
	RT_LIST_APPEND( &fu1->l, &fu2->l );
	return(fu1);
}

/*
 *			N M G _ M L V
 *
 * XXX - vertex or vertexuse? or both? ctj 
 *	Make a new loop (with specified orientation) and vertex,
 *	in a shell or face.
 *	If the vertex 'v' is NULL, the shell's lone vertex is used,
 *	or a new vertex is created.
 *
 *  "magic" must point to the magic number of a faceuse or shell.
 *
 *  If the shell has a lone vertex in it, that lone vertex *will*
 *  be used.  If a non-NULL 'v' is provided, the lone vertex and
 *  'v' will be fused together.  XXX Why is this good?
 *
 *  If a convenient shell does not exist, use s=nmg_msv() to make
 *  the shell and vertex, then call lu=nmg_mlv(s,NULL,).
 * 
 *  Implicit returns -
 *	The new vertexuse can be had by:
 *		RT_LIST_FIRST(vertexuse, &lu->down_hd);
 *
 *	In case the returned loopuse isn't retained, the new loopuse was
 *	inserted at the +head+ of the appropriate list, e.g.:
 *		lu = RT_LIST_FIRST(loopuse, &fu->lu_hd);
 *	or
 *		lu = RT_LIST_FIRST(loopuse, &s->lu_hd);
 *
 * N.B.  This function is made more complex than warrented by using
 * the "hack" of stealing a vertexuse structure from the shell if
 * at all possible.  A future enhancement to this function would be
 * to remove the vertexuse steal and have the caller pass in the
 * vertex from the shell followed by a call to nmg_kvu(s->v_p).
 */
struct loopuse *
nmg_mlv(magic, v, orientation)
long		*magic;
struct vertex	*v;
int		orientation;
{
	struct loop	*l;
	struct loopuse	*lu1, *lu2;
	struct model	*m;
	/* XXX - why the new union? ctj */
	union {
		struct shell *s;
		struct faceuse *fu;
		long *magic_p;
	} p;

	p.magic_p = magic;

	if (v) {
		NMG_CK_VERTEX(v);
	}

	m = nmg_find_model( magic );
	GET_LOOP(l, m);
	GET_LOOPUSE(lu1, m);
	GET_LOOPUSE(lu2, m);

	l->lg_p = (struct loop_g *)NULL;
	l->lu_p = lu1;
	l->magic = NMG_LOOP_MAGIC;	/* Loop struct is GOOD */

	RT_LIST_INIT( &lu1->down_hd );
	RT_LIST_INIT( &lu2->down_hd );
	lu1->lua_p = lu2->lua_p = (struct loopuse_a *)NULL;
	lu1->l_p = lu2->l_p = l;
	lu1->orientation = lu2->orientation = orientation;

	lu1->lumate_p = lu2;
	lu2->lumate_p = lu1;

	/* The only thing left to do to make the loopuses good is to */
	/* set the "up" pointer and "l.magic". */
	if (*p.magic_p == NMG_SHELL_MAGIC) {
		struct shell		*s;
		struct vertexuse	*vu1, *vu2;

		s = p.s;

		/* First, finish setting up the loopuses */
		lu1->up.s_p = lu2->up.s_p = s;

		lu1->l.magic = lu2->l.magic =
		    NMG_LOOPUSE_MAGIC;	/* Loopuse structs are GOOD */

		RT_LIST_INSERT( &s->lu_hd, &lu1->l );
		RT_LIST_INSERT( &lu1->l, &lu2->l );

		/* Second, build the vertices */
		/* This "if" degenerates to the "else" clause if no stealing */
		if ( vu1 = s->vu_p ) {
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
		struct vertexuse	*vu1, *vu2;

		/* First, finish setting up the loopuses */
		lu1->up.fu_p = p.fu;
		lu2->up.fu_p = p.fu->fumate_p;
		lu1->l.magic = lu2->l.magic =
		    NMG_LOOPUSE_MAGIC;	/* Loopuse structs are GOOD */

		RT_LIST_INSERT( &p.fu->fumate_p->lu_hd, &lu2->l );
		RT_LIST_INSERT( &p.fu->lu_hd, &lu1->l );

		/* Second, build the vertices */
		if (v) vu1 = nmg_mvu(v, &lu1->l.magic, m);
		else vu1 = nmg_mvvu(&lu1->l.magic, m);
		RT_LIST_SET_DOWN_TO_VERT(&lu1->down_hd, vu1);
		/* vu1->up.lu_p = lu1; done by nmg_mvu/nmg_mvvu */

		vu2 = nmg_mvu(vu1->v_p, &lu2->l.magic, m);
		RT_LIST_SET_DOWN_TO_VERT(&lu2->down_hd, vu2);
		/* vu2->up.lu_p = lu2; done by nmg_mvu() */
	} else {
		rt_bomb("nmg_mlv() unknown parent for loopuse!\n");
	}

	return(lu1);
}

/*			N M G _ M V U
 *
 *	Make Vertexuse on existing vertex
 *
 *  This is a support routine for this module, and is not intended for
 *  general use, as it requires lots of cooperation from the caller.
 *  (Like setting the parent's down pointer appropriately).
 *
 *  This means that a vu is created but is not attached to the parent
 *  structure.  This is "bad" and requires the caller to fix.
 */
static struct vertexuse *
nmg_mvu(v, upptr, m)
struct vertex	*v;
long		*upptr;		/* pointer to parent struct */
struct model	*m;
{
	struct vertexuse *vu;

	NMG_CK_VERTEX(v);
	NMG_CK_MODEL(m);

	if (*upptr != NMG_SHELL_MAGIC &&
	    *upptr != NMG_LOOPUSE_MAGIC &&
	    *upptr != NMG_EDGEUSE_MAGIC) {
		rt_log("nmg_mvu() in %s at %d magic not shell, loop, or edge.  Was x%x (%s)\n",
		    __FILE__, __LINE__,
		    *upptr, rt_identify_magic(*upptr) );
		rt_bomb("nmg_mvu() Cannot build vertexuse without parent\n");
	}

	GET_VERTEXUSE(vu, m);

	vu->v_p = v;
	vu->vua_p = (struct vertexuse_a *)NULL;
	RT_LIST_APPEND( &v->vu_hd, &vu->l );
	vu->up.magic_p = upptr;
	vu->l.magic = NMG_VERTEXUSE_MAGIC;	/* Vertexuse struct is GOOD */

	return(vu);
}

/*			N M G _ M V V U
 *
 *	Make Vertex, Vertexuse
 *
 *  This is a support routine for this module, and is not intended for
 *  general use, as it requires lots of cooperation from the caller.
 *  (Like setting the parent's down pointer appropriately).
 *
 *  This means that a vu is created but is not attached to the parent
 *  structure.  This is "bad" and requires the caller to fix.
 */
static struct vertexuse *
nmg_mvvu(upptr, m)
long		*upptr;
struct model	*m;
{
	struct vertex	*v;

	NMG_CK_MODEL(m);
	GET_VERTEX(v, m);
	RT_LIST_INIT( &v->vu_hd );
	v->vg_p = (struct vertex_g *)NULL;
	v->magic = NMG_VERTEX_MAGIC;	/* Vertex struct is GOOD */
	return(nmg_mvu(v, upptr, m));
}


/*
 *			N M G _ M E
 *
 *	Make wire edge
 *	Make a new edge between a pair of vertices in a shell.
 *
 *	A new vertex will be made for any NULL vertex pointer parameters.
 *	If we need to make a new vertex and the shell still has its vertexuse
 *	we re-use that vertex rather than freeing and re-allocating.
 *
 *	If both vertices were specified, and the shell also had a
 *	vertexuse pointer, the vertexuse in the shell is killed.
 *	XXX Why?
 *
 *  Explicit Return -
 *	An edgeuse in shell "s" whose vertexuse refers to vertex v1.
 *	The edgeuse mate's vertexuse refers to vertex v2
 *
 *  Implicit Returns -
 *	1)  If the shell had a lone vertex in vu_p, it is destroyed,
 *	even if both vertices were specified.
 *	2)  The returned edgeuse is the first item on the shell's
 *	eu_hd list, followed immediately by the mate.
 */
struct edgeuse *
nmg_me(v1, v2, s)
struct vertex *v1, *v2;
struct shell *s;
{
	struct edge		*e;
	struct edgeuse		*eu1;
	struct edgeuse		*eu2;
	struct vertexuse	*vu;
	struct model		*m;

	if (v1) NMG_CK_VERTEX(v1);
	if (v2) NMG_CK_VERTEX(v2);
	NMG_CK_SHELL(s);

	m = nmg_find_model( &s->l.magic );
	GET_EDGE(e, m);
	GET_EDGEUSE(eu1, m);
	GET_EDGEUSE(eu2, m);

	e->eg_p = (struct edge_g *)NULL;
	e->eu_p = eu1;
	/* e->is_real = XXX; */
	e->magic = NMG_EDGE_MAGIC;	/* Edge struct is GOOD */

	eu1->radial_p = eu1->eumate_p = eu2;
	eu2->radial_p = eu2->eumate_p = eu1;

	eu1->e_p = eu2->e_p = e;
	eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;
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

	if (v1)  {
		eu1->vu_p = nmg_mvu(v1, &eu1->l.magic, m);
	} else if (s->vu_p)  {
		/* This clause of the if statment dies when no vertex stealing */
		/* steal the vertex from the shell */
		vu = s->vu_p;
		s->vu_p = (struct vertexuse *)NULL;
		eu1->vu_p = vu;
		vu->up.eu_p = eu1;
	} else {
		eu1->vu_p = nmg_mvvu(&eu1->l.magic, m);
	}

	if (v2)  {
		eu2->vu_p = nmg_mvu(v2, &eu2->l.magic, m);
	} else if (s->vu_p)  {
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
	if( s->vu_p )  {
		/* Ensure shell no longer has any stored vertexuse */
		(void)nmg_kvu( s->vu_p );
		s->vu_p = (struct vertexuse *)NULL;
	}

	/* shell's eu_head, eu1, eu2, ... */
	RT_LIST_APPEND( &s->eu_hd, &eu1->l );
	RT_LIST_APPEND( &eu1->l, &eu2->l );

	return(eu1);
}

/*
 *			N M G _ M E o n V U
 *
 *  Make an edge on vertexuse.
 *  The new edge runs from and to that vertex.
 *
 *  If the vertexuse was the shell's sole vertexuse, then the new edge
 *  is a wire edge in the shell's eu_hd list.
 *
 *  If the vertexuse was part of a loop-of-a-single-vertex, either
 *  as a loop in a face or as a wire-loop in the shell, the
 *  loop becomes a regular loop with one edge that runs from and to
 *  the original vertex.
 */
struct edgeuse *
nmg_meonvu(vu)
struct vertexuse *vu;
{
	struct edge *e;
	struct edgeuse *eu1, *eu2;
	struct model	*m;

	NMG_CK_VERTEXUSE(vu);

	m = nmg_find_model( vu->up.magic_p );
	GET_EDGE(e, m);
	GET_EDGEUSE(eu1, m);
	GET_EDGEUSE(eu2, m);
	e->eg_p = (struct edge_g *)NULL;
	e->eu_p = eu1;
	/* e->is_real = XXX; */
	e->magic = NMG_EDGE_MAGIC;

	eu1->radial_p = eu1->eumate_p = eu2;
	eu2->radial_p = eu2->eumate_p = eu1;
	eu1->e_p = eu2->e_p = e;
	eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;
	eu1->orientation = eu2->orientation = OT_NONE;
	/* XXX Why not OT_UNSPEC? */
	eu1->vu_p = vu;
	/* vu->up needs to be set but we can't do that until we recovered *
	/* the shell or loopuse from the up pointer. */

	eu2->vu_p = (struct vertexuse *) NULL;

	/* link edgeuses to parent */
	if (*vu->up.magic_p == NMG_SHELL_MAGIC) {
		struct shell	*s;

		s = eu2->up.s_p = eu1->up.s_p = vu->up.s_p;
		eu1->l.magic = eu2->l.magic =
		    NMG_EDGEUSE_MAGIC;	/* Edgeuse structs are GOOD */
		/* eu2 is fake good till it has a real vu */

		vu->up.eu_p = eu1;	/* vu is good again */

		if( s->vu_p != vu )
			rt_bomb("nmg_meonvu() vetexuse parent shell disowns vertexuse!\n");
		s->vu_p = (struct vertexuse *)NULL;	/* remove from shell */

		eu2->vu_p = nmg_mvu(vu->v_p, &eu2->l.magic, m);
		RT_LIST_APPEND( &s->eu_hd, &eu2->l );
		RT_LIST_APPEND( &s->eu_hd, &eu1->l );
	} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		struct loopuse		*lu;
		struct loopuse		*lumate;
		struct vertexuse	*vumate;

		lu = vu->up.lu_p;
		NMG_CK_LOOPUSE(lu);
		lumate = lu->lumate_p;
		NMG_CK_LOOPUSE(lumate);

		/* do a little consistency checking */
		if( lu == lumate )  rt_bomb("nmg_meonvu() lu mate is lu\n");
		if( RT_LIST_FIRST_MAGIC(&lumate->down_hd) != NMG_VERTEXUSE_MAGIC )
			rt_bomb("nmg_meonvu() mate of vertex-loop is not vertex-loop!\n");
		vumate = RT_LIST_FIRST(vertexuse, &lumate->down_hd);
		NMG_CK_VERTEXUSE(vumate);
		if( vu == vumate )  rt_bomb("nmg_meonvu() vu mate is vu\n");
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
		RT_LIST_INIT( &lu->down_hd );
		RT_LIST_INIT( &lumate->down_hd );
		/* Add edgeuses to loopuses linked lists */
		RT_LIST_APPEND( &lumate->down_hd, &eu2->l );
		RT_LIST_APPEND( &lu->down_hd, &eu1->l );

		/* vertexuses point up at edgeuses */
		vu->up.eu_p = eu1;
		vumate->up.eu_p = eu2;
	} else {
		rt_bomb("nmg_meonvu() cannot make edge, vertexuse not sole element of object\n");
	}
	return(eu1);
}

/*			N M G _ M L
 *
 *	Make wire loop from wire edgeuse list
 *
 *	Passed a pointer to a shell.  The wire edgeuse child of the shell
 *	is taken as the head of a list of edge(use)s which will form
 *	the new loop.  The loop is created from the first N contiguous
 *	edges.  Thus the end of the new loop is 
 *	delineated by the "next" edge(use)being:
 * 
 * 	A) the first object in the list (no other edgeuses in
 * 		shell list)
 *	B) non-contiguous with the previous edge
 * 
 *	A loop is created from this list of edges.  The edges must
 *	form a true circuit, or we dump core on your disk.  If we
 *	succeed, then the edgeuses are moved from the shell edgeuse list
 *	to the loop, and the two loopuses are inserted into the shell.
 */
struct loopuse *
nmg_ml(s)
struct shell *s;
{
	struct loop *l;
	struct loopuse *lu1, *lu2;
	struct edgeuse	*p1;
	struct edgeuse	*p2;
	struct edgeuse	*feu;
	struct model	*m;

	NMG_CK_SHELL(s);
	/* If loop on single vertex */
	if( RT_LIST_IS_EMPTY( &s->eu_hd ) && s->vu_p )  {
		NMG_CK_VERTEXUSE(s->vu_p);
		NMG_CK_VERTEX(s->vu_p->v_p);
		lu1 = nmg_mlv(&s->l.magic, s->vu_p->v_p, OT_UNSPEC);
		/* (void) nmg_kvu(s->vu_p); */
		return lu1;
	}

	m = nmg_find_model( &s->l.magic );
	GET_LOOP(l, m);
	GET_LOOPUSE(lu1, m);
	GET_LOOPUSE(lu2, m);

	l->lg_p = (struct loop_g *)NULL;
	l->lu_p = lu1;
	l->magic = NMG_LOOP_MAGIC;	/* loop struct is GOOD */

	RT_LIST_INIT( &lu1->down_hd );
	RT_LIST_INIT( &lu2->down_hd );
	lu1->lua_p = lu2->lua_p = (struct loopuse_a *)NULL;
	lu1->l_p = lu2->l_p = l;
	lu1->orientation = lu2->orientation = OT_UNSPEC;
	lu1->lumate_p = lu2;
	lu2->lumate_p = lu1;
	lu1->up.s_p = lu2->up.s_p = s;
	lu1->l.magic = lu2->l.magic =
	    NMG_LOOPUSE_MAGIC;	/* Loopuse structs are GOOD */

	/* Save the first edgeuse so we can tell if the loop closes */
	feu = RT_LIST_FIRST( edgeuse, &s->eu_hd );
	if (feu){
		NMG_CK_EDGEUSE(feu);
		NMG_CK_VERTEXUSE(feu->vu_p);
		NMG_CK_VERTEX(feu->vu_p->v_p);
	}

	/* Safety catch in case eu_hd is empty */
	p2 = (struct edgeuse *)NULL;
	while( RT_LIST_NON_EMPTY( &s->eu_hd ) )  {
		p1 = RT_LIST_FIRST( edgeuse, &s->eu_hd );
		NMG_CK_EDGEUSE(p1);
		p2 = p1->eumate_p;
		NMG_CK_EDGEUSE(p2);

		/* bogosity check */
		if (p1->up.s_p != s || p2->up.s_p != s)
			rt_bomb("nmg_ml() edgeuse mates don't have proper parent!\n");

		/* dequeue the first edgeuse */
		RT_LIST_DEQUEUE( &p1->l );
		if( RT_LIST_IS_EMPTY( &s->eu_hd ) )  {
			rt_log("nmg_ml() in %s at %d edgeuse mate not in this shell\n",
			    __FILE__, __LINE__);
			rt_bomb("nmg_ml\n");
		}

		/* dequeue the edgeuse's mate */
		RT_LIST_DEQUEUE( &p2->l );

		/*  Insert the next new edgeuse(s) at tail of the loop's list
		 *  (ie, insert just before the head).
		 *  head, .....,p2, p1, (tail)
		 */
		RT_LIST_INSERT( &lu1->down_hd, &p1->l );
		RT_LIST_INSERT( &lu2->down_hd, &p2->l );

		p1->up.lu_p = lu1;
		p2->up.lu_p = lu2;

		/* If p2's vertex does not match next one comming, quit */
		if( RT_LIST_IS_EMPTY( &s->eu_hd ) )  break;
		p1 = RT_LIST_FIRST( edgeuse, &s->eu_hd );
		NMG_CK_EDGEUSE(p1);
		NMG_CK_VERTEXUSE(p1->vu_p);
		NMG_CK_VERTEX(p1->vu_p->v_p);
		NMG_CK_VERTEXUSE(p2->vu_p);
		NMG_CK_VERTEX(p2->vu_p->v_p);
		if( p1->vu_p->v_p != p2->vu_p->v_p )  {
			break;
		}
	}

	if (p2) {
		NMG_CK_EDGEUSE(p2);
		NMG_CK_VERTEXUSE(p2->vu_p);
		NMG_CK_VERTEX(p2->vu_p->v_p);
	}
	if( p2 && p2->vu_p->v_p != feu->vu_p->v_p) {
		rt_log("nmg_ml() Edge(use)s do not form proper loop!\n");
		nmg_pr_s(s, (char *)NULL);
		rt_log("nmg_ml() Edge(use)s do not form proper loop!\n");
		rt_bomb("nmg_ml\n");
	}

	/* Head, lu1, lu2, ... */
	RT_LIST_APPEND( &s->lu_hd, &lu2->l );
	RT_LIST_APPEND( &s->lu_hd, &lu1->l );

	return(lu1);
}

/************************************************************************
 *									*
 *				"Kill" Routines				*
 *									*
 *  These subroutines delete (kill) a topological entity,		*
 *  eliminating any related subordinate entities where appropriate.	*
 *									*
 *  If the superior entity becomes empty as a result of a "kill"	*
 *  operation, and that superior entity isn't allowed to *be* empty,	*
 *  then a TRUE (1) return code is given, indicating that the		*
 *  caller needs to immediately delete the superior entity before	*
 *  performing any other NMG operations.				*
 *									*
 *  During this interval, the superior entity is in an "illegal"	*
 *  state that the verify routines (nmg_vshell, etc.) and subsequent	*
 *  NMG processing routines will not accept, so that code which fails	*
 *  to honor the return codes will "blow up" downstream.		*
 *  Unfortunately, in some cases the problem won't be detected for	*
 *  quite a while, but this is better than having subsequent code	*
 *  encountering entities that *look* valid, but shouldn't be.		*
 *									*
 ************************************************************************/

/*			N M G _ K V U
 *
 *	Kill vertexuse, and null out parent's vu_p.
 *
 *  This routine is not intented for general use by applications,
 *  because it requires cooperation on the part of the caller
 *  to properly dispose of or fix the now *quite* illegal parent.
 *  (Illegal because the parent's vu_p is NULL).
 *  It exists primarily as a support routine for "mopping up" after
 *  nmg_klu(), nmg_keu(), nmg_ks(), and nmg_mv_vu_between_shells().
 *
 *  It is also used in a particularly ugly way in 
 *  nmg_cut_loop() and nmg_split_lu_at_vu()
 *  as part of their method for obtaining an "empty" loopuse/loop set.
 *
 *  It is worth noting that all these callers ignore the return code,
 *  because they *all* exist to intentionally empty out the parent, but
 *  the return code is provided anyway, in the name of [CTJ] symmetry.
 *
 *  Returns -
 *	0	If all is well in the parent
 *	1	If parent is empty, and is thus "illegal"
 */
int
nmg_kvu(vu)
register struct vertexuse *vu;
{
	struct vertex	*v;
	int		ret = 0;

	NMG_CK_VERTEXUSE(vu);

	if (vu->vua_p) FREE_VERTEXUSE_A(vu->vua_p);

	v = vu->v_p;
	NMG_CK_VERTEX(v);

	RT_LIST_DEQUEUE( &vu->l );
	if( RT_LIST_IS_EMPTY( &v->vu_hd ) )  {
		/* last vertexuse on vertex */
		if (v->vg_p) FREE_VERTEX_G(v->vg_p);
		FREE_VERTEX(v);
	}

	/* erase existence of this vertexuse from parent */
	if (*vu->up.magic_p == NMG_SHELL_MAGIC)  {
		vu->up.s_p->vu_p = (struct vertexuse *)NULL;
		ret = nmg_shell_is_empty(vu->up.s_p);
	} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		/* Reset the hack */
		RT_LIST_INIT( &vu->up.lu_p->down_hd );
		ret = 1;
	} else if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC)  {
		vu->up.eu_p->vu_p = (struct vertexuse *)NULL;
		ret = 1;
	} else
		rt_bomb("nmg_kvu() killing vertexuse of unknown parent?\n");

	FREE_VERTEXUSE(vu);
	return ret;
}


/*			N M G _ K F U
 *
 *	Kill Faceuse
 *
 *	delete a faceuse and its mate from the parent shell.
 *	Any children found are brutally murdered as well.
 *	The faceuses are dequeued from the parent shell's list here.
 *
 *  Returns -
 *	0	If all is well
 *	1	If parent shell is now empty, and is thus "illegal"
 */
int
nmg_kfu(fu1)
struct faceuse *fu1;
{
	struct faceuse *fu2;
	struct face	*f1;
	struct face	*f2;
	struct shell	*s;

	NMG_CK_FACEUSE(fu1);
	fu2 = fu1->fumate_p;
	NMG_CK_FACEUSE(fu2);
	f1 = fu1->f_p;
	f2 = fu2->f_p;
	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);
	if( f1 != f2 )
		rt_bomb("nmg_kfu() faceuse mates do not share face!\n");
	s = fu1->s_p;
	NMG_CK_SHELL(s);

	/* kill off the children (infanticide?)*/
	while( RT_LIST_NON_EMPTY( &fu1->lu_hd ) )  {
		(void)nmg_klu( RT_LIST_FIRST( loopuse, &fu1->lu_hd ) );
	}

	/* kill the geometry */
	if (f1->fg_p) {
		NMG_CK_FACE_G(f1->fg_p);
		FREE_FACE_G(f1->fg_p);
	}
	FREE_FACE(f1);
	fu1->f_p = fu2->f_p = (struct face *)NULL;

	/* kill the attributes */
	if (fu1->fua_p) {
		NMG_CK_FACEUSE_A(fu1->fua_p);
		FREE_FACEUSE_A(fu1->fua_p);
	}
	if (fu2->fua_p) {
		NMG_CK_FACEUSE_A(fu2->fua_p);
		FREE_FACEUSE_A(fu2->fua_p);
	}

	/* remove ourselves from the parent list */
	RT_LIST_DEQUEUE( &fu1->l );
	if( RT_LIST_IS_EMPTY( &s->fu_hd ) )
		rt_bomb("nmg_kfu() faceuse mate not in parent shell?\n");
	RT_LIST_DEQUEUE( &fu2->l );

	FREE_FACEUSE(fu1);
	FREE_FACEUSE(fu2);
	return nmg_shell_is_empty(s);
}

/*			N M G _ K L U
 *
 *	Kill loopuse, loopuse mate, and loop.
 *
 *	if the loop contains any edgeuses or vertexuses they are killed
 *	before the loop is deleted.
 *
 *	We support the concept of killing a loop with no children to
 *	support the routine "nmg_demote_lu"
 *
 *  Returns -
 *	0	If all is well
 *	1	If parent is empty, and is thus "illegal"
 */
int
nmg_klu(lu1)
struct loopuse *lu1;
{
	struct loopuse *lu2;
	long	magic1;
	int	ret = 0;

	NMG_CK_LOOPUSE(lu1);
	lu2 = lu1->lumate_p;
	NMG_CK_LOOPUSE(lu2);

	if (lu1->l_p != lu2->l_p)
		rt_bomb("nmg_klu() loopmates do not share loop!\n");

	if (*lu1->up.magic_p != *lu2->up.magic_p)
		rt_bomb("nmg_klu() loopuses do not have same type of parent!\n");

	/* deal with the children */
	magic1 = RT_LIST_FIRST_MAGIC( &lu1->down_hd );
	if( magic1 != RT_LIST_FIRST_MAGIC( &lu2->down_hd ) )
		rt_bomb("nmg_klu() loopuses do not have same type of child!\n");

	if( magic1 == NMG_VERTEXUSE_MAGIC )  {
		/* Follow the vertex-loop hack downward,
		 * nmg_kvu() will clean up */
		(void)nmg_kvu( RT_LIST_FIRST(vertexuse, &lu1->down_hd) );
		(void)nmg_kvu( RT_LIST_FIRST(vertexuse, &lu2->down_hd) );
	} else if ( magic1 == NMG_EDGEUSE_MAGIC) {
		/* delete all edgeuse in the loopuse (&mate) */
		while( RT_LIST_NON_EMPTY( &lu1->down_hd ) )  {
			(void)nmg_keu(RT_LIST_FIRST(edgeuse, &lu1->down_hd) );
		}
	} else if ( magic1 == RT_LIST_HEAD_MAGIC )  {
		/* down_hd list is empty, no problem */
	} else {
		rt_log("nmg_klu(x%x) magic=%s\n", lu1, rt_identify_magic(magic1) );
		rt_bomb("nmg_klu: unknown type for loopuse child\n");
	}

	/* disconnect from parent's list */
	if (*lu1->up.magic_p == NMG_SHELL_MAGIC) {
		RT_LIST_DEQUEUE( &lu1->l );
		RT_LIST_DEQUEUE( &lu2->l );
		ret = nmg_shell_is_empty( lu1->up.s_p );
	} else if (*lu1->up.magic_p == NMG_FACEUSE_MAGIC) {
		RT_LIST_DEQUEUE( &lu1->l );
		RT_LIST_DEQUEUE( &lu2->l );
		ret = RT_LIST_IS_EMPTY( &lu1->up.fu_p->lu_hd );
	} else {
		rt_bomb("nmg_klu() unknown parent for loopuse\n");
	}

	if (lu1->lua_p) {
		NMG_CK_LOOPUSE_A(lu1->lua_p);
		FREE_LOOPUSE_A(lu1->lua_p);
	}
	if (lu2->lua_p) {
		NMG_CK_LOOPUSE_A(lu2->lua_p);
		FREE_LOOPUSE_A(lu2->lua_p);
	}

	NMG_CK_LOOP(lu1->l_p);
	if (lu1->l_p->lg_p) {
		NMG_CK_LOOP_G(lu1->l_p->lg_p);
		FREE_LOOP_G(lu1->l_p->lg_p);
	}
	FREE_LOOP(lu1->l_p);
	FREE_LOOPUSE(lu1);
	FREE_LOOPUSE(lu2);
	return ret;
}

/*
 *			N M G _ K E U
 *
 *	Delete an edgeuse & it's mate from a shell or loop.
 *
 *  Returns -
 *	0	If all is well
 *	1	If the parent now has no edgeuses, and is thus "illegal"
 *		and in need of being deleted.  (The lu / shell deletion
 *		can't be handled at this level, but must be done by the caller).
 */
int
nmg_keu(eu1)
register struct edgeuse *eu1;
{
	register struct edgeuse *eu2;
	struct edge		*e;
	int			ret = 0;

	NMG_CK_EDGEUSE(eu1);
	e = eu1->e_p;
	NMG_CK_EDGE(e);

	eu2 = eu1->eumate_p;
	NMG_CK_EDGEUSE(eu2);
	NMG_CK_EDGE(eu2->e_p);

	if (e != eu2->e_p) {
		rt_bomb("nmg_keu() edgeuse pair does not share edge\n");
	}

	/* unlink from radial linkages (if any) */
	if (eu1->radial_p != eu2) {
		NMG_CK_EDGEUSE(eu1->radial_p);
		NMG_CK_EDGEUSE(eu2->radial_p);

		eu1->radial_p->radial_p = eu2->radial_p;
		eu2->radial_p->radial_p = eu1->radial_p;

		/* since there is a use of this edge left, make sure the edge
		 * structure points to a remaining edgeuse
		 */
		if (e->eu_p == eu1 || e->eu_p == eu2)
			e->eu_p = eu1->radial_p;
	} else {
		/* since these two edgeuses are the only use of this edge,
		 * we need to free the edge (since all uses are about 
		 * to disappear).
		 */
		if (e->eg_p) FREE_EDGE_G(e->eg_p);
		FREE_EDGE(e);
	}

	/* remove the edgeuses from their parents */
	if (*eu1->up.magic_p == NMG_LOOPUSE_MAGIC) {
		struct loopuse	*lu1, *lu2;
		lu1 = eu1->up.lu_p;
		lu2 = eu2->up.lu_p;
		NMG_CK_LOOPUSE(lu1);
		NMG_CK_LOOPUSE(lu2);

		if( lu1 == lu2 )  rt_bomb("nmg_keu() edgeuses on same loopuse\n");
		if (lu1->lumate_p != lu2 || lu1 != lu2->lumate_p ) {
			rt_log("nmg_keu() lu1=x%x, mate=x%x\n", lu1, lu1->lumate_p);
			rt_log("nmg_keu() lu2=x%x, mate=x%x\n", lu2, lu2->lumate_p);
		    	rt_bomb("nmg_keu() edgeuse mates don't belong to loopuse mates\n");
		}

		/* remove the edgeuses from their parents */
		RT_LIST_DEQUEUE( &eu1->l );
		RT_LIST_DEQUEUE( &eu2->l );

		/* If loopuse list is empty, caller needs to delete it. */
		if( RT_LIST_IS_EMPTY( &lu1->down_hd ) )  ret = 1;
	} else if (*eu1->up.magic_p == NMG_SHELL_MAGIC) {
		if (eu1->up.s_p != eu2->up.s_p) {
			rt_bomb("nmg_keu() edguses don't share parent shell\n");
		}

		/* unlink edgeuses from the parent shell */
		RT_LIST_DEQUEUE( &eu1->l );
		RT_LIST_DEQUEUE( &eu2->l );
		ret = nmg_shell_is_empty( eu1->up.s_p );
	} else {
		rt_bomb("nmg_keu() bad up pointer\n");
	}

	/* get rid of any attributes */
	if (eu1->eua_p) {
		if (eu1->eua_p == eu2->eua_p) {
			FREE_EDGEUSE_A(eu1->eua_p);
		} else {
			FREE_EDGEUSE_A(eu1->eua_p);
		}
	}
	if (eu2->eua_p) {
		FREE_EDGEUSE_A(eu2->eua_p);
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
	return ret;
}

/*			N M G _ K S
 *
 *	Kill a shell and all children
 *
 *  Returns -
 *	0	If all is well
 *	1	If parent nmgregion is now empty.  While not "illegal",
 *		an empty region is probably worthy of note.
 */
int
nmg_ks(s)
struct shell *s;
{
	struct nmgregion	*r;

	NMG_CK_SHELL(s);
	r = s->r_p;
	NMG_CK_REGION(r);

	while( RT_LIST_NON_EMPTY( &s->fu_hd ) )
		(void)nmg_kfu( RT_LIST_FIRST(faceuse, &s->fu_hd) );
	while( RT_LIST_NON_EMPTY( &s->lu_hd ) )
		(void)nmg_klu( RT_LIST_FIRST(loopuse, &s->lu_hd) );
	while( RT_LIST_NON_EMPTY( &s->eu_hd ) )
		(void)nmg_keu( RT_LIST_FIRST(edgeuse, &s->eu_hd) );
	if( s->vu_p )
		nmg_kvu( s->vu_p );

	RT_LIST_DEQUEUE( &s->l );

	if (s->sa_p) {
		FREE_SHELL_A(s->sa_p);
	}

	FREE_SHELL(s);
	if( RT_LIST_IS_EMPTY( &r->s_hd ) )  return 1;
	return 0;
}

/*			N M G _ K R
 *
 *	Kill a region and all shells in it
 *
 *  Returns -
 *	0	If all is well
 *	1	If model is now empty.  While not "illegal",
 *		an empty model is probably worthy of note.
 */
int
nmg_kr(r)
struct nmgregion *r;
{
	struct model	*m;

	NMG_CK_REGION(r);
	m = r->m_p;
	NMG_CK_MODEL(m);

	while( RT_LIST_NON_EMPTY( &r->s_hd ) )
		(void)nmg_ks( RT_LIST_FIRST( shell, &r->s_hd ) );

	RT_LIST_DEQUEUE( &r->l );

	if (r->ra_p) {
		FREE_REGION_A(r->ra_p);
	}
	FREE_REGION(r);

	if( RT_LIST_IS_EMPTY( &m->r_hd ) ) {
		m->maxindex = 1;	/* Reset when last region is killed */
		return 1;
	}
	return 0;
}

/*			N M G _ K M
 *
 *	Kill an entire model.  Nothing is left.
 */
void
nmg_km(m)
struct model *m;
{
	NMG_CK_MODEL(m);

	while( RT_LIST_NON_EMPTY( &m->r_hd ) )
		(void)nmg_kr( RT_LIST_FIRST( nmgregion, &m->r_hd ) );

	if (m->ma_p) {
		FREE_MODEL_A(m->ma_p);
	}
	FREE_MODEL(m);
}

/************************************************************************
 *									*
 *			"Geometry" and "Attribute" Routines		*
 *									*
 ************************************************************************/

/*			N M G _ V E R T E X _ G V
 *
 *	Associate point_t ("vector") coordinates with a vertex
 */
void
nmg_vertex_gv(v, pt)
struct vertex	*v;
CONST point_t	pt;
{
	struct vertex_g *vg;
	struct model	*m;

	NMG_CK_VERTEX(v);

	if (vg = v->vg_p) {
		NMG_CK_VERTEX_G(v->vg_p);
	}
	else {
		m = nmg_find_model(
			&RT_LIST_NEXT(vertexuse, &v->vu_hd)->l.magic );
		GET_VERTEX_G(vg, m);

		vg->magic = NMG_VERTEX_G_MAGIC;
		v->vg_p = vg;
	}
	VMOVE( vg->coord, pt );
}

/*
 *			N M G _ V E R T E X _ G
 *
 *	a version that can take x, y, z coords and doesn't need a point 
 *	array.  Mostly useful for quick and dirty programs.
 */
void
nmg_vertex_g(v, x, y, z)
register struct vertex *v;
fastf_t x, y, z;
{
	point_t pt;
	
	pt[0] = x;
	pt[1] = y;
	pt[2] = z;

	nmg_vertex_gv(v, pt);
}

/*
 *			N M G _ E D G E _ G
 *
 *	Compute the equation of the line formed by the endpoints of the edge.
 */
void
nmg_edge_g(e)
struct edge *e;
{
	struct model *m;	
	struct edge_g *eg_p = (struct edge_g *)NULL;
	pointp_t	pt;

	NMG_CK_EDGE(e);
	NMG_CK_EDGEUSE(e->eu_p);
	NMG_CK_VERTEXUSE(e->eu_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->vu_p->v_p->vg_p);

	NMG_CK_EDGEUSE(e->eu_p->eumate_p);
	NMG_CK_VERTEXUSE(e->eu_p->eumate_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->eumate_p->vu_p->v_p->vg_p);

	if(e->eu_p->vu_p->v_p == e->eu_p->eumate_p->vu_p->v_p )
		rt_bomb("nmg_edge_g(): edge runs from+to same vertex, 0 len!\n");

	/* make sure we've got a valid edge_g structure */
	if (eg_p = e->eg_p) {
		NMG_CK_EDGE_G(eg_p);
	} else {
		m = nmg_find_model(&e->eu_p->l.magic);
		GET_EDGE_G(eg_p, m);	/* sets usage=1 */
		eg_p->magic = NMG_EDGE_G_MAGIC;
		e->eg_p = eg_p;
	}

	/* copy the point from the vertex of one of our edgeuses */
	pt = e->eu_p->vu_p->v_p->vg_p->coord;
	VMOVE(eg_p->e_pt, pt);

	/* compute the direction from the endpoints of the edgeuse(s) */
	pt = e->eu_p->eumate_p->vu_p->v_p->vg_p->coord;
	VSUB2(eg_p->e_dir, eg_p->e_pt, pt);	


	/* If the edge vector is essentially 0 magnitude we're in trouble.
	 * We warn the user and create an arbitrary vector we can use.
	 */
	if (VNEAR_ZERO(eg_p->e_dir, SMALL_FASTF)) {
		pointp_t pt2 = e->eu_p->vu_p->v_p->vg_p->coord;
		VPRINT("nmg_edge_g(): e_dir too small", eg_p->e_dir);
		rt_log("nmg_edge_g(): (%g %g %g) -> (%g %g %g)",
				pt[X],  pt[Y],  pt[Z],
				pt2[X], pt2[Y], pt2[Z]);

		VSET(eg_p->e_dir, 1.0, 0.0, 0.0);
		VPRINT("nmg_edge_g(): Forcing e_dir to", eg_p->e_dir);
		rt_bomb("nmg_edge_g():  0 length edge\n");
	}
}

/*
 *			N M G _ U S E _ E D G E _ G
 *
 *  Make a use of the edge geometry structure "eg" in edge "e",
 *  releasing the use of any previous edge geometry by "e".
 */
void
nmg_use_edge_g( e, eg )
struct edge	*e;
struct edge_g	*eg;
{
	NMG_CK_EDGE(e);
	NMG_CK_EDGE_G(eg);

	if( e->eg_p )  {
		/* Macro releases previous edge geom, if usage hits zero */
		FREE_EDGE_G(e->eg_p);
	}
	e->eg_p = eg;
	eg->usage++;
}

/*			N M G _ L O O P _ G
 *
 *	Build the bounding box for a loop
 */
void
nmg_loop_g(l)
struct loop *l;
{
	struct edgeuse	*eu;
	struct vertex_g	*vg;
	struct loop_g	*lg;
	struct loopuse	*lu;
	struct model	*m;
	long		magic1;

	NMG_CK_LOOP(l);
	lu = l->lu_p;
	NMG_CK_LOOPUSE(lu);

	if (lg = l->lg_p) {
		NMG_CK_LOOP_G(lg);
	} else {
		m = nmg_find_model( lu->up.magic_p );
		GET_LOOP_G(l->lg_p, m);
		lg = l->lg_p;
		lg->magic = NMG_LOOP_G_MAGIC;
	}
	VSETALL( lg->max_pt, -INFINITY );
	VSETALL( lg->min_pt,  INFINITY );

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);
			VMINMAX(lg->min_pt, lg->max_pt, vg->coord);
			if( !eu->e_p->eg_p )  nmg_edge_g(eu->e_p);
		}
	} else if (magic1 == NMG_VERTEXUSE_MAGIC) {
		struct vertexuse	*vu;
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		NMG_CK_VERTEXUSE(vu);
		NMG_CK_VERTEX(vu->v_p);
		vg = vu->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		VMOVE(lg->min_pt, vg->coord);
		VMOVE(lg->max_pt, vg->coord);
	} else {
		rt_log("nmg_loop_g() loopuse down is %s (x%x)\n",
			rt_identify_magic(magic1), magic1 );
		rt_bomb("nmg_loop_g() loopuse has bad child\n");
	}
}

/*			N M G _ F A C E _ G
 *
 *	Assign plane equation to face and compute bounding box
 */
void
nmg_face_g(fu, p)
struct faceuse *fu;
plane_t p;
{
	int i;
	struct face_g	*fg;
	struct face	*f;
	struct model	*m;

	NMG_CK_FACEUSE(fu);
	f = fu->f_p;
	NMG_CK_FACE(f);

	fu->orientation = OT_SAME;
	fu->fumate_p->orientation = OT_OPPOSITE;

	fg = f->fg_p;
	if (fg) {
		NMG_CK_FACE_G(f->fg_p);
	} else {
		m = nmg_find_model( &fu->l.magic );
		GET_FACE_G(f->fg_p, m);
		fg = f->fg_p;
		fg->magic = NMG_FACE_G_MAGIC;
	}

	for (i=0 ; i < ELEMENTS_PER_PLANE ; i++)
		fg->N[i] = p[i];

	nmg_face_bb(f);
}

/*			N M G _ F A C E _ B B
 *
 *	Build the bounding box for a face
 */
void
nmg_face_bb(f)
struct face *f;
{
	struct face_g	*fg;
	struct loopuse	*lu;
	struct faceuse	*fu;
	struct model	*m;

	NMG_CK_FACE(f);
	fu = f->fu_p;
	NMG_CK_FACEUSE(fu);

	if ( fg = f->fg_p ) {
		NMG_CK_FACE_G(fg);
	}
	else {
		m = nmg_find_model( &fu->l.magic );
		GET_FACE_G(fg, m);
		fg->magic = NMG_FACE_G_MAGIC;
		f->fg_p = fg;
	}

	fg->max_pt[X] = fg->max_pt[Y] = fg->max_pt[Z] = -MAX_FASTF;
	fg->min_pt[X] = fg->min_pt[Y] = fg->min_pt[Z] = MAX_FASTF;

	/* we compute the extent of the face by looking at the extent of
	 * each of the loop children.
	 */
	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		nmg_loop_g(lu->l_p);

		if (lu->orientation != OT_BOOLPLACE) {
			VMIN(fg->min_pt, lu->l_p->lg_p->min_pt);
			VMAX(fg->max_pt, lu->l_p->lg_p->max_pt);
		}
	}
}

/*			N M G _ S H E L L _ A
 *
 *	Build the bounding box for a shell
 */
void
nmg_shell_a(s)
struct shell *s;
{
	struct shell_a *sa;
	struct vertex_g *vg;
	struct face_g *fg;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct model	*m;

	NMG_CK_SHELL(s);

	if (s->sa_p) {
		NMG_CK_SHELL_A(s->sa_p);
	} else {
		m = nmg_find_model( &s->l.magic );
		GET_SHELL_A(s->sa_p, m);
		s->sa_p->magic = NMG_SHELL_A_MAGIC;
	}
	sa = s->sa_p;

	/* */
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		nmg_face_bb(fu->f_p);

		fg = fu->f_p->fg_p;
		VMIN(sa->min_pt, fg->min_pt);
		VMAX(sa->max_pt, fg->max_pt);

		/* If next faceuse shares this face, skip it */
		if( RT_LIST_NOT_HEAD(fu, &fu->l) &&
		    ( RT_LIST_NEXT(faceuse, &fu->l)->f_p == fu->f_p ) )  {
			fu = RT_LIST_PNEXT(faceuse,fu);
		}
	}
	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		nmg_loop_g(lu->l_p);

		VMIN(sa->min_pt, lu->l_p->lg_p->min_pt);
		VMAX(sa->max_pt, lu->l_p->lg_p->max_pt);
	}
	for( RT_LIST_FOR( eu, edgeuse, &s->eu_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGE(eu->e_p);
		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		VMINMAX(sa->min_pt, sa->max_pt, vg->coord);
	}
	if (s->vu_p) {
		NMG_CK_VERTEXUSE(s->vu_p);
		NMG_CK_VERTEX(s->vu_p->v_p);
		NMG_CK_VERTEX_G(s->vu_p->v_p->vg_p);
		vg = s->vu_p->v_p->vg_p;
		VMINMAX(sa->min_pt, sa->max_pt, vg->coord);
	}


	if( RT_LIST_IS_EMPTY( &s->fu_hd ) &&
	    RT_LIST_IS_EMPTY( &s->lu_hd ) &&
	    RT_LIST_IS_EMPTY( &s->eu_hd ) && !s->vu_p )  {
		rt_log("nmg_shell_a() at %d in %s. Shell has no children\n",
		    __LINE__, __FILE__);
		rt_bomb("nmg_shell_a\n");
	}
}

/*			N M G _ R E G I O N _ A
 *
 *	build attributes/extents for all shells in a region
 */
void
nmg_region_a(r)
struct nmgregion *r;
{
	register struct shell	*s;
	struct nmgregion_a	*ra;

	NMG_CK_REGION(r);
	if( r->ra_p )  {
		ra = r->ra_p;
		NMG_CK_REGION_A(ra);
	} else {
		GET_REGION_A(ra, r->m_p);
		r->ra_p = ra;
	}

	ra->magic = NMG_REGION_A_MAGIC;

	VSETALL(ra->max_pt, -MAX_FASTF);
	VSETALL(ra->min_pt, MAX_FASTF);

	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_shell_a(s);
		NMG_CK_SHELL_A(s->sa_p);
		VMIN(ra->min_pt, s->sa_p->min_pt);
		VMAX(ra->max_pt, s->sa_p->max_pt);
	}
}

/************************************************************************
 *									*
 *				"Demote" Routines			*
 *									*
 *  As part of the boolean operations, sometimes it is desirable	*
 *  to "demote" entities down the "chain of being".  The sequence is:	*
 *									*
 *	face --> wire loop --> wire edge (line segment) --> vertex	*
 *									*
 ************************************************************************/


/*
 *			N M G _ D E M O T E _ L U
 *
 *	Demote a loopuse of edgeuses to a bunch of wire edges in the shell.
 *
 *  Returns -
 *	0	If all is well (edges moved to shell, loopuse deleted).
 *	1	If parent is empty, and is thus "illegal".  Still successful.
 */
int
nmg_demote_lu(lu1)
struct loopuse *lu1;
{
	struct edgeuse *eu1;
	struct shell *s;

	NMG_CK_LOOPUSE(lu1);

	if (rt_g.NMG_debug & DEBUG_CLASSIFY)
		rt_log("nmg_demote_lu(x%x)\n", lu1);

	if (RT_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC) {
		rt_bomb("nmg_demote_lu() demoting loopuse of a single vertex\n");
	}

	if (RT_LIST_FIRST_MAGIC(&lu1->down_hd) != NMG_EDGEUSE_MAGIC)
		rt_bomb("nmg_demote_lu: bad loopuse child\n");

	/* get the parent shell */
	s = nmg_lups(lu1);
	NMG_CK_SHELL(s);

	/* move all edgeuses (&mates) to shell
	 */
	while ( RT_LIST_NON_EMPTY(&lu1->down_hd) ) {

		eu1 = RT_LIST_FIRST(edgeuse, &lu1->down_hd);
		NMG_CK_EDGEUSE(eu1);
		NMG_CK_EDGE(eu1->e_p);
		NMG_CK_EDGEUSE(eu1->eumate_p);
		NMG_CK_EDGE(eu1->eumate_p->e_p);

		RT_LIST_DEQUEUE(&eu1->eumate_p->l);
		RT_LIST_APPEND(&s->eu_hd, &eu1->eumate_p->l);

		RT_LIST_DEQUEUE(&eu1->l);
		RT_LIST_APPEND(&s->eu_hd, &eu1->l);

		eu1->up.s_p = eu1->eumate_p->up.s_p = s;
	}
	/* lu1 is in an illegal state here, with a null edge list */

	if (RT_LIST_NON_EMPTY(&lu1->lumate_p->down_hd))
		rt_bomb("nmg_demote_lu: loopuse mates don't have same # of edges\n");

	if( nmg_klu(lu1) )  return 1;	/* fu went empty */

	return(0);
}

/*			N M G _ D E M O T E _ E U
 *
 *	Demote a wire edge into a pair of self-loop verticies
 *
 *
 *  Returns -
 *	0	If all is well
 *	1	If shell is empty, and is thus "illegal".
 */
int
nmg_demote_eu(eu)
struct edgeuse *eu;
{
	struct shell	*s;
	struct vertex	*v;

	if (*eu->up.magic_p != NMG_SHELL_MAGIC)
		rt_bomb("nmg_demote_eu() up is not shell\n");
	s = eu->up.s_p;
	NMG_CK_SHELL(s);

	NMG_CK_EDGEUSE(eu);
	v = eu->vu_p->v_p;
	if( !nmg_is_vertex_a_selfloop_in_shell(v, s) )
		(void)nmg_mlv(&s->l.magic, v, OT_SAME);

	NMG_CK_EDGEUSE(eu->eumate_p);
	v = eu->eumate_p->vu_p->v_p;
	if( !nmg_is_vertex_a_selfloop_in_shell(v, s) )
		(void)nmg_mlv(&s->l.magic, v, OT_SAME);

	(void)nmg_keu(eu);
	return nmg_shell_is_empty(s);
}

/************************************************************************
 *									*
 *				"Modify" Routines			*
 *									*
 *  These routines would go in nmg_mod.c, except that they create	*
 *  or delete fundamental entities directly as part of their operation.	*
 *  Thus, they are part of the make/kill purpose of this module.	*
 *									*
 ************************************************************************/

/*			N M G _ M O V E V U
 *
 *	Move a vertexuse from an old vertex to a new vertex.
 *	If this was the last use, the old vertex is destroyed.
 */
void
nmg_movevu(vu, v)
struct vertexuse *vu;
struct vertex *v;
{
	struct vertex	*oldv;

	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(v);

	oldv = vu->v_p;
	NMG_CK_VERTEX(oldv);

	RT_LIST_DEQUEUE( &vu->l );
	if( RT_LIST_IS_EMPTY( &oldv->vu_hd ) )  {
		/* last vertexuse on vertex */
		if (oldv->vg_p) FREE_VERTEX_G(oldv->vg_p);
		FREE_VERTEX(oldv);
	}
	RT_LIST_APPEND( &v->vu_hd, &vu->l );
	vu->v_p = v;
}

/*
 *			N M G _ M O V E E U
 *
 *	Move a pair of edgeuses onto a new edge (glue edgeuse).
 *	the edgeuse eusrc and its mate are moved to the edge
 *	used by eudst.  eusrc is made to be immediately radial to eudst.
 *	if eusrc does not share the same vertices as eudst, we bomb.
 */
void
nmg_moveeu(eudst, eusrc)
struct edgeuse *eudst, *eusrc;
{
	struct edgeuse	*eudst_mate;
	struct edgeuse	*eusrc_mate;
	struct edge	*e;

	NMG_CK_EDGEUSE(eudst);
	NMG_CK_EDGEUSE(eusrc);
	eudst_mate = eudst->eumate_p;
	eusrc_mate = eusrc->eumate_p;
	NMG_CK_EDGEUSE(eudst_mate);
	NMG_CK_EDGEUSE(eusrc_mate);

	/* protect the morons from themselves.  Don't let them
	 * move an edgeuse to itself or it's mate
	 */
	if (eusrc == eudst || eusrc_mate == eudst)  {
		rt_log("nmg_moveeu() moving edgeuse to itself\n");
		return;
	}

	if (eusrc->e_p == eudst->e_p &&
	    (eusrc->radial_p == eudst || eudst->radial_p == eusrc))  {
	    	rt_log("nmg_moveeu() edgeuses already share edge\n");
		return;
	}

	/* make sure vertices are shared */
	if ( ! ( (eudst_mate->vu_p->v_p == eusrc->vu_p->v_p &&
	    eudst->vu_p->v_p == eusrc_mate->vu_p->v_p) ||
	    (eudst->vu_p->v_p == eusrc->vu_p->v_p &&
	    eudst_mate->vu_p->v_p == eusrc_mate->vu_p->v_p) ) ) {
		/* edgeuses do NOT share verticies. */
	    	VPRINT("eusrc", eusrc->vu_p->v_p->vg_p->coord);
	    	VPRINT("eusrc_mate", eusrc_mate->vu_p->v_p->vg_p->coord);
	    	VPRINT("eudst", eudst->vu_p->v_p->vg_p->coord);
	    	VPRINT("eudst_mate", eudst_mate->vu_p->v_p->vg_p->coord);
	    	rt_bomb("nmg_moveeu() edgeuses do not share vertices, cannot share edge\n");
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
		/* this is the only use of the eusrc edge.  Kill edge. */
		if (e->eg_p) FREE_EDGE_G(e->eg_p);
		FREE_EDGE(e);
	}

	eusrc->radial_p = eudst;
	eusrc_mate->radial_p = eudst->radial_p;

	eudst->radial_p->radial_p = eusrc_mate;
	eudst->radial_p = eusrc;
}

/*
 *			N M G _ U N G L U E E D G E
 *
 *	If edgeuse is part of a shared edge (more than one pair of edgeuses
 *	on the edge), it and its mate are "unglued" from the edge, and 
 *	associated with a new edge structure.
 *
 *	Primarily a support routine for nmg_eusplit()
 *
 *  If the original edge had edge geometry, that is *not* duplicated here,
 *  because it is not easy (or appropriate) for nmg_eusplit() to know
 *  whether the new vertex lies on the previous edge geometry or not.
 *  Hence having the nmg_ebreak() interface, which preserves the ege
 *  geometry across a split, and nmg_esplit() which does not.
 */
void
nmg_unglueedge(eu)
struct edgeuse *eu;
{
	struct edge	*old_e;
	struct edge	*new_e;
	struct model	*m;

	NMG_CK_EDGEUSE(eu);
	old_e = eu->e_p;
	NMG_CK_EDGE(old_e);

	/* if we're already a single edge, just return */
	if (eu->radial_p == eu->eumate_p)
		return;

	m = nmg_find_model( &eu->l.magic );
	GET_EDGE(new_e, m);		/* create new edge */

	new_e->magic = NMG_EDGE_MAGIC;
	new_e->eu_p = eu;
	new_e->eg_p = (struct edge_g *)NULL;

	/* make sure the old edge isn't pointing at this edgeuse */
	if (old_e->eu_p == eu || old_e->eu_p == eu->eumate_p ) {
		old_e->eu_p = old_e->eu_p->radial_p;
	}

	/* unlink edgeuses from old edge */
	eu->radial_p->radial_p = eu->eumate_p->radial_p;
	eu->eumate_p->radial_p->radial_p = eu->radial_p;
	eu->eumate_p->radial_p = eu;
	eu->radial_p = eu->eumate_p;

	/* Associate edgeuse and mate with new edge */
	eu->eumate_p->e_p = eu->e_p = new_e;
}

/*
 *			N M G _ J V
 *
 *	Join two vertexes into one.
 *	v1 inherits all the vertexuses presently pointing to v2,
 *	and v2 is then destroyed.
 */
void
nmg_jv(v1, v2)
register struct vertex	*v1;
register struct vertex	*v2;
{
	register struct vertexuse	*vu;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);

	if (v1 == v2) return;

	/*
	 *  Walk the v2 list, unlinking vertexuse structs,
	 *  and adding them to the *end* of the v1 list
	 *  (which preserves relative ordering).
	 */
	vu = RT_LIST_FIRST(vertexuse, &v2->vu_hd );
	while( RT_LIST_NOT_HEAD( vu, &v2->vu_hd ) )  {
		register struct vertexuse	*vunext;

		NMG_CK_VERTEXUSE(vu);
		vunext = RT_LIST_PNEXT(vertexuse, vu);
		RT_LIST_DEQUEUE( &vu->l );
		RT_LIST_INSERT( &v1->vu_hd, &vu->l );
		vu->v_p = v1;		/* "up" to new vertex */
		vu = vunext;
	}

	/* Kill vertex v2 */
	if (v2->vg_p) {
		if( !v1->vg_p )  {
			v1->vg_p = v2->vg_p;
		} else {
			FREE_VERTEX_G(v2->vg_p);
		}
	}
	FREE_VERTEX(v2);
}
