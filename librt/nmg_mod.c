/*
 *			N M G _ M O D . C
 *
 *  Routines for modifying n-Manifold Geometry data structures.
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
 *	This software is Copyright (C) 1991 by the United States Army.
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

/*
 *			N M G _ F U _ P L A N E E Q N
 *
 *  Given a convex face that has been constructed with edges listed in
 *  counter-clockwise (CCW) order, compute the surface normal and plane
 *  equation for this face.
 *
 *
 *			D                   C
 *	                *-------------------*
 *	                |                   |
 *	                |   .<...........   |
 *	   ^     N      |   .           ^   |     ^
 *	   |      \     |   .  counter- .   |     |
 *	   |       \    |   .   clock   .   |     |
 *	   |C-B     \   |   .   wise    .   |     |C-B
 *	   |         \  |   v           .   |     |
 *	   |          \ |   ...........>.   |     |
 *	               \|                   |
 *	                *-------------------*
 *	                A                   B
 *			      <-----
 *				A-B
 *
 *  If the vertices in the loop are given in the order A B C D
 *  (e.g., counter-clockwise),
 *  then the outward pointing surface normal can be computed as:
 *
 *		N = (C-B) x (A-B)
 *
 *  This is the "right hand rule".
 *  For reference, note that a vector which points "into" the loop
 *  can be subsequently found by taking the cross product of the
 *  surface normal and any edge vector, e.g.:
 *
 *		Left = N x (B-A)
 *	or	Left = N x (C-B)
 *
 *  This routine will skip on past edges that start and end on
 *  the same vertex, in an attempt to avoid trouble.
 *  However, the loop *must* be convex for this routine to work.
 *  Otherwise, the surface normal may be inadvertently reversed.
 *
 *  Returns -
 *	0	OK
 *	-1	failure
 */
int
nmg_fu_planeeqn( fu, tol )
struct faceuse		*fu;
CONST struct rt_tol	*tol;
{
	struct edgeuse		*eu, *eu_final, *eu_next;
	struct loopuse		*lu;
	plane_t			plane;
	struct vertex		*a, *b, *c;

	NMG_CK_FACEUSE(fu);
	lu = RT_LIST_FIRST(loopuse, &fu->lu_hd);
	NMG_CK_LOOPUSE(lu);

	if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
		return -1;
	eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);
	a = eu->vu_p->v_p;
	NMG_CK_VERTEX(a);

	eu_next = eu;
	do {
		eu_next = RT_LIST_PNEXT_CIRC(edgeuse, eu_next);
		NMG_CK_EDGEUSE(eu_next);
		if( eu_next == eu )  return -1;
		b = eu_next->vu_p->v_p;
		NMG_CK_VERTEX(b);
	} while( b == a );

	eu_final = eu_next;
	do {
		eu_final = RT_LIST_PNEXT_CIRC(edgeuse, eu_final);
		NMG_CK_EDGEUSE(eu_final);
		if( eu_final == eu )  return -1;
		c = eu_final->vu_p->v_p;
		NMG_CK_VERTEX(c);
	} while( c == b );

	if (rt_mk_plane_3pts(plane,
	    a->vg_p->coord, b->vg_p->coord, c->vg_p->coord, tol) < 0 ) {
		rt_log("nmg_fu_planeeqn(): rt_mk_plane_3pts failed on (%g,%g,%g) (%g,%g,%g) (%g,%g,%g)\n",
			V3ARGS( a->vg_p->coord ),
			V3ARGS( b->vg_p->coord ),
			V3ARGS( c->vg_p->coord ) );
	    	HPRINT("plane", plane);
		return(-1);
	}
	if (plane[0] == 0.0 && plane[1] == 0.0 && plane[2] == 0.0) {
		rt_log("nmg_fu_planeeqn():  Bad plane equation from rt_mk_plane_3pts\n" );
	    	HPRINT("plane", plane);
		return(-1);
	}
	nmg_face_g( fu, plane);
	return(0);
}

/*
 *			N M G _ C F A C E
 *
 *	Create a loop within a face, given a list of vertices.
 *
 *	"verts" is an array of "n" pointers to (struct vertex).  "s" is the
 *	parent shell for the new face.  The face will consist of a single loop
 *	made from edges between the n vertices.
 *
 *	If verts is a null pointer (no vertex list), all vertices of the face
 *	will be new points.  Otherwise, verts is a pointer to a list of
 *	vertices to use in creating the face/loop.  Null entries within the
 *	list will cause a new vertex to be created for that point.  Such new
 *	vertices will be inserted into the list for return to the caller.
 *
 *	The vertices should be listed in
 *	"counter-clockwise" (CCW) order if this is an ordinary face (loop),
 *	and in "clockwise" (CW) order if this is an interior
 * 	("hole" or "subtracted") face (loop).
 *	This routine makes only topology, without reference to any geometry.
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.
 *	Therefore, the caller's vertices are traversed in reverse order
 *	to counter this behavior, and
 *	to effect the proper vertex order in the final face loop.
 */
struct faceuse *
nmg_cface(s, verts, n)
struct shell *s;
struct vertex *verts[];
int n;
{
	struct faceuse *fu;
	struct edgeuse *eu;
	struct loopuse	*lu;
	struct vertexuse *vu;
	int i;

	NMG_CK_SHELL(s);
	if (n < 1) {
		rt_log("nmg_cface(s=x%x, verts=x%x, n=%d.)\n",
			s, verts, n );
		rt_bomb("nmg_cface() trying to make bogus face\n");
	}

	if (verts) {
		for (i=0 ; i < n ; ++i) {
			if (verts[i]) {
				NMG_CK_VERTEX(verts[i]);
			}
		}
		lu = nmg_mlv(&s->l.magic, verts[n-1], OT_SAME);
		fu = nmg_mf(lu);
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);

		if (!verts[n-1])
			verts[n-1] = eu->vu_p->v_p;

		for (i = n-2 ; i >= 0 ; i--) {
			eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
			eu = nmg_eusplit(verts[i], eu);
			if (!verts[i])
				verts[i] = eu->vu_p->v_p;
		}

	} else {
		lu = nmg_mlv(&s->l.magic, (struct vertex *)NULL, OT_SAME);
		fu = nmg_mf(lu);
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);
		while (--n) {
			(void)nmg_eusplit((struct vertex *)NULL, eu);
		}
	}
	return (fu);
}

/*
 *	N M G _ A D D _ L O O P _ T O _ F A C E
 *
 *	Create a new loop within a face, given a list of vertices.
 *	Modified version of nmg_cface().
 *
 *	"verts" is an array of "n" pointers to (struct vertex).  "s" is the
 *	parent shell for the new face.  The face will consist of a single loop
 *	made from edges between the n vertices.
 *
 *	If verts is a null pointer (no vertex list), all vertices of the face
 *	will be new points.  Otherwise, verts is a pointer to a list of
 *	vertices to use in creating the face/loop.  Null entries within the
 *	list will cause a new vertex to be created for that point.  Such new
 *	vertices will be inserted into the list for return to the caller.
 *
 *	The vertices should be listed in "counter-clockwise" (CCW) order if
 *	this is an ordinary face (loop), and in "clockwise" (CW) order if
 *	this is an interior ("hole" or "subtracted") face (loop).  This
 *	routine makes only topology, without reference to any geometry.
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.  Therefore, the
 *	caller's vertices are traversed in reverse order to counter this
 *	behavior, and to effect the proper vertex order in the final face
 *	loop.
 */

struct faceuse *
nmg_add_loop_to_face(s, fu, verts, n, dir)
struct shell *s;
struct faceuse *fu;
struct vertex *verts[];
int n, dir;
{
	int i, j;
	struct edgeuse *eu;
	struct loopuse *lu;
	struct vertexuse *vu;

	NMG_CK_SHELL(s);
	if (n < 1) {
		rt_log("nmg_add_loop_to_face(s=x%x, verts=x%x, n=%d.)\n",
			s, verts, n );
		rt_bomb("nmg_add_loop_to_face: request to make 0 faces\n");
	}

	if (verts) {
		if (!fu) {
			lu = nmg_mlv(&s->l.magic, verts[n-1], dir);
			fu = nmg_mf(lu);
		} else {
			lu = nmg_mlv(&fu->l.magic, verts[n-1], dir);
		}
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);
		if (!verts[n-1])
			verts[n-1] = eu->vu_p->v_p;

		for (i = n-2 ; i >= 0 ; i--) {
			eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
			eu = nmg_eusplit(verts[i], eu);
			if (!verts[i])
				verts[i] = eu->vu_p->v_p;
		}
	} else {
		lu = nmg_mlv(&s->l.magic, (struct vertex *)NULL, dir);
		fu = nmg_mf(lu);
		vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);
		while (--n) {
			(void)nmg_eusplit((struct vertex *)NULL, eu);
		}
	}
	return (fu);
}

/*
 *			N M G _ C M F A C E
 *
 *	Create a face with exactly one exterior loop (and no holes),
 *	given a list of pointers to vertices.
 *	Intended to help create a "3-manifold" shell, where
 *	each edge has only two faces alongside of it.
 *	(Shades of winged edges!)
 *
 *	"verts" is an array of "n" pointers to pointers to (struct vertex).
 *	"s" is the parent shell for the new face.
 *
 *	The new face will consist of a single loop
 *	made from n edges between the n vertices.  Before an edge is created
 *	between a pair of verticies, we check to see if there is already an
 *	edge with exactly one edgeuse+mate (in this shell)
 *	that runs between the two verticies.
 *	If such an edge can be found, the newly created edgeuses will just
 *	use the existing edge.
 *	This means that no special call to nmg_gluefaces() is needed later.
 *
 *	If a pointer in verts is a pointer to a null vertex pointer, a new
 *	vertex is created.  In this way, new verticies can be created
 *	conveniently within a user's list of known verticies
 *
 *	verts		pointers to struct vertex	    vertex structs
 *
 *	-------		--------
 *   0	|  +--|-------->|   +--|--------------------------> (struct vertex)
 *	-------		--------	---------
 *   1	|  +--|------------------------>|   +---|---------> (struct vertex)
 *	-------		--------	---------
 *   2	|  +--|-------->|   +--|--------------------------> (struct vertex)
 *	-------		--------
 *  ...
 *	-------				---------
 *   n	|  +--|------------------------>|   +---|---------> (struct vertex)
 *	-------				---------
 *
 *
 *	The vertices *must* be listed in "counter-clockwise" (CCW) order.
 *	This routine makes only topology, without reference to any geometry.
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.
 *	Therefore, the caller's vertices are traversed in reverse order
 *	to counter this behavior, and
 *	to effect the proper vertex order in the final face loop.
 *
 *	Also note that this routine uses one level more of indirection
 *	in the verts[] array than nmg_cface().
 */
struct faceuse *
nmg_cmface(s, verts, n)
struct shell	*s;
struct vertex	**verts[];
int		n;
{
	struct faceuse *fu;
	struct edgeuse *eu, *eur, *euold;
	struct loopuse	*lu;
	struct vertexuse	*vu;
	int i;

	NMG_CK_SHELL(s);

	if (n < 1) {
		rt_log("nmg_cmface(s=x%x, verts=x%x, n=%d.)\n",
			s, verts, n );
		rt_bomb("nmg_cmface() trying to make bogus face\n");
	}

	/* make sure verts points to some real storage */
	if (!verts) {
		rt_log("nmg_cmface(s=x%x, verts=x%x, n=%d.) null pointer to array start\n",
			s, verts, n );
		rt_bomb("nmg_cmface\n");
	}

	/* validate each of the pointers in verts */
	for (i=0 ; i < n ; ++i) {
		if (verts[i]) {
			if (*verts[i]) {
				/* validate the vertex pointer */
				NMG_CK_VERTEX(*verts[i]);
			}
		} else {
			rt_log("nmg_cmface(s=x%x, verts=x%x, n=%d.) verts[%d]=NULL\n",
				s, verts, n, i );
			rt_bomb("nmg_cmface\n");
		}
	}

	lu = nmg_mlv(&s->l.magic, *verts[n-1], OT_SAME);
	fu = nmg_mf(lu);
	fu->orientation = OT_SAME;
	fu->fumate_p->orientation = OT_OPPOSITE;
	vu = RT_LIST_FIRST( vertexuse, &lu->down_hd);
	NMG_CK_VERTEXUSE(vu);
	eu = nmg_meonvu(vu);
	NMG_CK_EDGEUSE(eu);

	if (!(*verts[n-1]))  {
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		*verts[n-1] = eu->vu_p->v_p;
	}

	for (i = n-2 ; i >= 0 ; i--) {
		euold = RT_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(euold);

		if (rt_g.NMG_debug & DEBUG_CMFACE)
			rt_log("nmg_cmface() euold: %8x\n", euold);

		/* look for pre-existing edge between these
		 * verticies
		 */
		if (*verts[i]) {
			/* look for an existing edge to share */
			eur = nmg_findeu(*verts[i+1], *verts[i], s, euold);
			eu = nmg_eusplit(*verts[i], euold);
			if (eur) {
				nmg_moveeu(eur, eu);

				if (rt_g.NMG_debug & DEBUG_CMFACE)
					rt_log("nmg_cmface() found another edgeuse (%8x) between %8x and %8x\n",
						eur, *verts[i+1], *verts[i]);
			} else {
				if (rt_g.NMG_debug & DEBUG_CMFACE)
				    rt_log("nmg_cmface() didn't find edge from verts[%d]%8x to verts[%d]%8x\n",
					i+1, *verts[i+1], i, *verts[i]);
			}
		} else {
			eu = nmg_eusplit(*verts[i], euold);
			*verts[i] = eu->vu_p->v_p;

			if (rt_g.NMG_debug & DEBUG_CMFACE)  {
				rt_log("nmg_cmface() *verts[%d] was null, is now %8x\n",
					i, *verts[i]);
			}
		}
	}

	if (eur = nmg_findeu(*verts[n-1], *verts[0], s, euold))  {
		nmg_moveeu(eur, euold);
	} else  {
	    if (rt_g.NMG_debug & DEBUG_CMFACE)
		rt_log("nmg_cmface() didn't find edge from verts[%d]%8x to verts[%d]%8x\n",
			n-1, *verts[n-1], 0, *verts[0]);
	}
	return (fu);
}

/*
 *			N M G _ E U S P L I T
 *
 *	Split an edgeuse by inserting a vertex into middle of the edgeuse.
 *
 *	Make a new edge, and a vertex.  If v is non-null it is taken as a
 *	pointer to an existing vertex to use as the start of the new edge.
 *	If v is null, then a new vertex is created for the begining of the
 *	new edge.
 *
 *	In either case, the new edge will exist as the "next" edgeuse after
 *	the edgeuse passed as a parameter.
 *
 *  Explicit return -
 *	edgeuse of new edge, starting at v.
 *
 *  List on entry -
 *
 *		       oldeu
 *		  .------------->
 *		 /
 *		A =============== B (edge)
 *				 /
 *		  <-------------.
 *		      oldeumate
 *
 *  List on return -
 *
 *		     oldeu(cw)    eu1
 *		    .------->   .----->
 *		   /           /
 *	   (edge) A ========= V ~~~~~~~ B (new edge)
 *			     /         /
 *		    <-------.   <-----.
 *		       mate	 mate
 */
struct edgeuse *
nmg_eusplit(v, oldeu)
struct vertex *v;
struct edgeuse *oldeu;
{
	struct edgeuse	*eu1,
			*eu2,
			*oldeumate;
	struct shell *s;
	struct loopuse	*lu;

	NMG_CK_EDGEUSE(oldeu);
	if (v) {
		NMG_CK_VERTEX(v);
	}
	oldeumate = oldeu->eumate_p;
	NMG_CK_EDGEUSE( oldeumate );

	/* if this edge has uses other than this edge and its mate, we must
	 * separate these two edgeuses from the existing edge, and create
	 * a new edge for them.  Then we can insert a new vertex in this
	 * new edge without fear of damaging some other object.
	 */
	if (oldeu->radial_p != oldeumate)
		nmg_unglueedge(oldeu);

	if (*oldeu->up.magic_p == NMG_SHELL_MAGIC) {
		s = oldeu->up.s_p;
		NMG_CK_SHELL(s);

		/*
		 *  Make an edge from the new vertex ("V") to vertex at
		 *  other end of the edge given ("B").
		 *  The new vertex "V" may be NULL, which will cause the
		 *  shell's lone vertex to be used, or a new one obtained.
		 *  New edges will be placed at head of shell's edge list.
		 */
		eu1 = nmg_me(v, oldeumate->vu_p->v_p, s);
		eu2 = eu1->eumate_p;

		/*
		 *  The situation is now:
		 *
		 *      eu1			       oldeu
		 *  .----------->		  .------------->
		 * /				 /
		 *V ~~~~~~~~~~~~~ B (new edge)	A =============== B (edge)
		 *		 /				 /
		 *  <-----------.		  <-------------.
		 *      eu2			      oldeumate
		 */

		/* Make oldeumate start at "V", not "B" */
		nmg_movevu(oldeumate->vu_p, eu1->vu_p->v_p);

		/*
		 *  Enforce rigid ordering in shell's edge list:
		 *	oldeu, oldeumate, eu1, eu2
		 *  This is to keep edges & mates "close to each other".
		 */
		if( RT_LIST_PNEXT(edgeuse, oldeu) != oldeumate )  {
			RT_LIST_DEQUEUE( &oldeumate->l );
			RT_LIST_APPEND( &oldeu->l, &oldeumate->l );
		}
		RT_LIST_DEQUEUE( &eu1->l );
		RT_LIST_DEQUEUE( &eu2->l );
		RT_LIST_APPEND( &oldeumate->l, &eu1->l );
		RT_LIST_APPEND( &eu1->l, &eu2->l );

		/*
		 *	     oldeu(cw)    eu1
		 *	    .------->   .----->
		 *	   /           /
		 * (edge) A ========= V ~~~~~~~ B (new edge)
		 *		     /         /
		 *	    <-------.   <-----.
		 *	    oldeumate     eu2
		 */
		return(eu1);
	}
	else if (*oldeu->up.magic_p != NMG_LOOPUSE_MAGIC) {
		rt_log("nmg_eusplit() in %s at %d invalid edgeuse parent\n",
			__FILE__, __LINE__);
		rt_bomb("nmg_eusplit\n");
	}

	/* now we know we are in a loop */

	lu = oldeu->up.lu_p;
	NMG_CK_LOOPUSE(lu);

	/* get a parent shell pointer so we can make a new edge */
	if (*lu->up.magic_p == NMG_SHELL_MAGIC)
		s = lu->up.s_p;
	else if (*lu->up.magic_p == NMG_FACEUSE_MAGIC)
		s = lu->up.fu_p->s_p;
	else
		rt_bomb("nmg_eusplit() bad lu->up\n");
	NMG_CK_SHELL(s);

	nmg_ck_list( &s->eu_hd, "eusplit A" );
	nmg_ck_list( &s->lu_hd, "eusplit lu A" );

	/* make a new edge on the vertex */
	if (v) {
		/* An edge on the single vertex "V" */
		eu1 = nmg_me(v, v, s);
		eu2 = eu1->eumate_p;
	} else {
		/* An edge between two new vertices */
		nmg_ck_list( &s->eu_hd, "eusplit B" );
		nmg_ck_list( &s->lu_hd, "eusplit lu B" );
		eu1 = nmg_me((struct vertex *)NULL, (struct vertex *)NULL, s);
		eu2 = eu1->eumate_p;
		/* Make both ends of edge use same vertex.
		 * The second vertex is freed automaticly.
		 */
		nmg_movevu(eu2->vu_p, eu1->vu_p->v_p);
	}

	/*
	 *  The current situation is now:
	 *
	 *	      eu1			       oldeu
	 *	  .------------->		  .------------->
	 *	 /				 /
	 *	V ~~~~~~~~~~~~~~~ V (new edge)	A =============== B (edge)
	 *			 /				 /
	 *	  <-------------.		  <-------------.
	 *	      eu2			      oldeumate
	 *
	 *  Goals:
	 *  eu1 will become the mate to oldeumate on the existing edge.
	 *  eu2 will become the mate of oldeu on the new edge.
	 */
	RT_LIST_DEQUEUE( &eu1->l );
	RT_LIST_DEQUEUE( &eu2->l );
	RT_LIST_APPEND( &oldeu->l, &eu1->l );
	RT_LIST_APPEND( &oldeumate->l, &eu2->l );

	/*
	 *  The situation is now:
	 *
	 *		       oldeu      eu1			>>>loop>>>
	 *		    .------->   .----->
	 *		   /           /
	 *	   (edge) A ========= V ~~~~~~~ B (new edge)
	 *			     /         /
	 *		    <-------.   <-----.	
	 *		       eu2      oldeumate		<<<loop<<<
	 */

	/* Copy parentage (loop affiliation) and orientation */
	eu1->up.magic_p = oldeu->up.magic_p;
	eu1->orientation = oldeu->orientation;
	eu1->eua_p = (struct edgeuse_a *)NULL;

	eu2->up.magic_p = oldeumate->up.magic_p;
	eu2->orientation = oldeumate->orientation;
	eu2->eua_p = (struct edgeuse_a *)NULL;

	/* Build mate relationship */
	eu1->eumate_p = oldeumate;
	oldeumate->eumate_p = eu1;
	eu2->eumate_p = oldeu;
	oldeu->eumate_p = eu2;

	/* Build radial relationship */
	eu1->radial_p = oldeumate;
	oldeumate->radial_p = eu1;
	eu2->radial_p = oldeu;
	oldeu->radial_p = eu2;

	/* Associate oldeumate with new edge, and eu2 with old edge. */
	oldeumate->e_p = eu1->e_p;
	eu2->e_p = oldeu->e_p;

	return(eu1);
}

/*
 *			N M G _ M O V E L T O F
 *
 *	move first pair of shell loopuses to an existing face
 */
void nmg_moveltof(fu, s)
struct faceuse *fu;
struct shell *s;
{
	struct loopuse	*lu1, *lu2;

	NMG_CK_SHELL(s);
	NMG_CK_FACEUSE(fu);
	if (fu->s_p != s) {
		rt_log("nmg_moveltof() in %s at %d. Cannot move loop to face in another shell\n",
		    __FILE__, __LINE__);
	}
	lu1 = RT_LIST_FIRST(loopuse, &s->lu_hd);
	NMG_CK_LOOPUSE(lu1);
	RT_LIST_DEQUEUE( &lu1->l );

	lu2 = RT_LIST_FIRST(loopuse, &s->lu_hd);
	NMG_CK_LOOPUSE(lu2);
	RT_LIST_DEQUEUE( &lu2->l );

	RT_LIST_APPEND( &fu->lu_hd, &lu1->l );
	RT_LIST_APPEND( &fu->fumate_p->lu_hd, &lu2->l );
}

/*
 *			N M G _ E S P L I T
 *
 *	Split an edge by inserting a vertex into middle of *all* of the
 *	uses of this edge, and combine the new edgeuses together onto the
 *	new edge.
 *	A more powerful version of nmg_eusplit(), which does only one use.
 *
 *	Makes a new edge, and a vertex.  If v is non-null it is taken as a
 *	pointer to an existing vertex to use as the start of the new edge.
 *	If v is null, then a new vertex is created for the begining of the
 *	new edge.
 *
 *	In either case, the new edgeuse will exist as the "next" edgeuse
 *	after the edgeuse passed as a parameter.
 *
 *	Note that eu->e_p changes value upon return, because the old
 *	edge is incrementally replaced by two new ones.
 *
 *  Explicit return -
 *	Pointer to the edgeuse which starts at the newly created
 *	vertex (V), and runs to B.
 *
 *  Implicit returns -
 *	ret_eu->vu_p->v_p gives the new vertex ("v", if non-null), and
 *	ret_eu->e_p is the new edge that runs from V to B.
 *
 *	The new vertex created will also be eu->eumate_p->vu_p->v_p.
 *
 *  Edge on entry -
 *
 *			eu
 *		  .------------->
 *		 /
 *		A =============== B (edge)
 *				 /
 *		  <-------------.
 *		    eu->eumate_p
 *
 *  Edge on return -
 *
 *			eu	  ret_eu
 *		    .------->   .--------->
 *		   /           /
 *	(newedge) A ========= V ~~~~~~~~~~~ B (new edge)
 *			     /             /
 *		    <-------.   <---------.
 */
struct edgeuse *
nmg_esplit(v, eu)
struct vertex	*v;
struct edgeuse	*eu;
{
	struct edge	*e;	/* eu->e_p */
	struct edgeuse	*eur,	/* radial edgeuse of eu */
			*eu2,	/* new edgeuse (next of eur) */
			*neu1, *neu2; /* new (split) edgeuses */
	int 		notdone=1;
	struct vertex	*v1, *v2;	/* start and end of eu */

	neu1 = neu2 = (struct edgeuse *)NULL;

	NMG_CK_EDGEUSE(eu);
	e = eu->e_p;
	NMG_CK_EDGE(e);

	NMG_CK_VERTEXUSE(eu->vu_p);
	v1 = eu->vu_p->v_p;
	NMG_CK_VERTEX(v1);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	v2 = eu->eumate_p->vu_p->v_p;
	NMG_CK_VERTEX(v2);

	/* v1 and v2 are the endpoints of the original edge "e" */
	if( v1 == v2 )
		rt_log("WARNING: nmg_esplit() on edge from&to v=x%x\n", v1);
	if( v && ( v == v1 || v == v2 ) )
		rt_log("WARNING: nmg_esplit(v=x%x) vertex is already an edge vertex\n", v);

	/* one at a time, we peel out & split an edgeuse pair of this edge.
	 * when we split an edge that didn't need to be peeled out, we know
	 * we've split the last edge
	 */
	do {
		eur = eu->radial_p;
		/* eur could runs from v1 to v2 */
		eu2 = nmg_eusplit(v, eur);
		/* Now, eur runs from v1 to v, eu2 runs from v to v2 */
		NMG_CK_EDGEUSE(eur);
		NMG_CK_EDGEUSE(eu2);
		NMG_TEST_EDGEUSE(eur);
		NMG_TEST_EDGEUSE(eu2);
		
		if (!v)  {
			/* "v" is the vertex "e" was just split at */
			v = eu2->vu_p->v_p;
			NMG_CK_VERTEX(v);
		}

		if (eu2->e_p == e || eur->e_p == e) notdone = 0;
		
		NMG_CK_VERTEX(eur->vu_p->v_p);
		if (eur->vu_p->v_p == v1) {
			if (neu1) {
				nmg_moveeu(neu1, eur);
				nmg_moveeu(neu2, eu2);
			}
			neu1 = eur->eumate_p;
			neu2 = eu2->eumate_p;
		} else if (eur->vu_p->v_p == v2) {
			if (neu1) {
				nmg_moveeu(neu2, eur);
				nmg_moveeu(neu1, eu2);
			}
			neu2 = eur->eumate_p;
			neu1 = eu2->eumate_p;
		} else {
			rt_log("nmg_esplit(v=x%x, e=x%x)\n", v, e);
			rt_log("nmg_esplit: eur->vu_p->v_p=x%x, v1=x%x, v2=x%x\n", eur->vu_p->v_p, v1, v2 );
			rt_bomb("nmg_esplit() eur->vu_p->v_p is neither v1 nor v2\n");
		}
	} while (notdone);
	/* Here, "e" pointer is invalid -- it no longer exists */

	/* Error checking loops */
	eu = neu1;
	do {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGEUSE(eu->eumate_p);
		NMG_TEST_EDGEUSE(eu);
		NMG_TEST_EDGEUSE(eu->eumate_p);
		eu = eu->radial_p->eumate_p;
	} while (eu != neu1);
	eu = neu2;
	do {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGEUSE(eu->eumate_p);
		NMG_TEST_EDGEUSE(eu);
		NMG_TEST_EDGEUSE(eu->eumate_p);
		eu = eu->radial_p->eumate_p;
	} while (eu != neu2);

	/* Find an edgeuse that runs from v to v2 */
	if( neu2->vu_p->v_p == v && neu2->eumate_p->vu_p->v_p == v2 )
		return neu2;

	rt_bomb("nmg_esplit() unable to find eu starting at new v\n");
}

/*
 *			N M G _ E B R E A K
 *
 *  Like nmg_esplit(), split an edge into two parts, but where the
 *  two resultant parts share the original edge geometry.
 *  If the original edge had no edge geometry, then none is created here.
 *
 *  The return is the return of nmg_esplit().
 */
struct edgeuse *
nmg_ebreak(v, eu)
struct vertex	*v;
struct edgeuse	*eu;
{
	struct edgeuse	*new_eu;
	struct edge_g	*eg;

	NMG_CK_EDGEUSE(eu);
	eg = eu->e_p->eg_p;

	/* nmg_esplit() will delete eu->e_p, so if geom is present, save it! */
	if( eg )  {
		NMG_CK_EDGE_G(eg);
		eg->usage++;
	}

	new_eu = nmg_esplit(v, eu);	/* Do the hard work */
	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(new_eu);

	/* If there wasn't any edge geometry before, nothing more to do */
	if( !eg ) return new_eu;

	/* Both these edges should be fresh, without geometry yet. */
	if( eu->e_p->eg_p )   {
		rt_log("old eg=x%x, new_eg=x%x\n", eg, eu->e_p->eg_p);
		nmg_pr_eg(eg, "old_");
		nmg_pr_eg(eu->e_p->eg_p, "new_");
		rt_bomb("nmg_ebreak() eu grew geometry?\n");
	}
	if( new_eu->e_p->eg_p )  rt_bomb("nmg_ebreak() new_eu grew geometry?\n");

	/* Make sure the two edges share the same geometry. */
	NMG_CK_EDGE_G(eg);
	eu->e_p->eg_p = eg;		/* eg->usage++ was done above */
	new_eu->e_p->eg_p = eg;
	eg->usage++;
	return new_eu;
}

/*
 *			N M G _ E 2 B R E A K
 *
 *  Given two edges that are known to intersect someplace other than
 *  at any of their endpoints, break both of them and
 *  insert a shared vertex.
 *  Return a pointer to the new vertex.
 */
struct vertex *
nmg_e2break( eu1, eu2 )
struct edgeuse	*eu1;
struct edgeuse	*eu2;
{
	struct vertex		*v;
	struct edgeuse		*new_eu;

	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);

	new_eu = nmg_ebreak( NULL, eu1 );
	v = new_eu->vu_p->v_p;
	NMG_CK_VERTEX(v);
	(void)nmg_ebreak( v, eu2 );

	return v;
}

/*
 *			N M G _ E I N S
 *
 *	Insert a new (zero length) edge at the begining of (ie, before)
 *	an existing edgeuse
 *	Perhaps this is what nmg_esplit and nmg_eusplit should have been like?
 *
 *	Before:
 *	.--A--> .--eu-->
 *		 \
 *		  >.
 *		 /
 *	  <-A'--. <-eu'-.
 *
 *
 *	After:
 *
 *               eu1     eu
 *	.--A--> .---> .--eu-->
 *		 \   /
 *		  >.<
 *		 /   \
 *	  <-A'--. <---. <-eu'--.
 *	          eu2     eumate
 */
struct edgeuse *
nmg_eins(eu)
struct edgeuse *eu;
{
	struct edgeuse	*eumate;
	struct edgeuse	*eu1, *eu2;
	struct shell	*s;

	NMG_CK_EDGEUSE(eu);
	eumate = eu->eumate_p;
	NMG_CK_EDGEUSE(eumate);

	if (*eu->up.magic_p == NMG_SHELL_MAGIC) {
		s = eu->up.s_p;
		NMG_CK_SHELL(s);
	}
	else {
		struct loopuse *lu;
		
		lu = eu->up.lu_p;
		NMG_CK_LOOPUSE(lu);
		if (*lu->up.magic_p == NMG_SHELL_MAGIC) {
			s = lu->up.s_p;
			NMG_CK_SHELL(s);
		} else {
			struct faceuse *fu;
			fu = lu->up.fu_p;
			NMG_CK_FACEUSE(fu);
			s = fu->s_p;
			NMG_CK_SHELL(s);
		}
	}

	eu1 = nmg_me(eu->vu_p->v_p, eu->vu_p->v_p, s);
	eu2 = eu1->eumate_p;

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		RT_LIST_DEQUEUE( &eu1->l );
		RT_LIST_DEQUEUE( &eu2->l );

		RT_LIST_INSERT( &eu->l, &eu1->l );
		RT_LIST_APPEND( &eumate->l, &eu2->l );

		eu1->up.lu_p = eu->up.lu_p;
		eu2->up.lu_p = eumate->up.lu_p;
	}
	else {
		rt_bomb("nmg_eins() Cannot yet insert null edge in shell\n");
	}
	return(eu1);
}

/*
 *			N M G _ F I N D _ V U _ I N _ F A C E
 *
 *	try to find a vertex(use) in a face wich appoximately matches the
 *	coordinates given.  
 *	
 */
struct vertexuse *
nmg_find_vu_in_face(pt, fu, tol)
CONST point_t		pt;
struct faceuse		*fu;
CONST struct rt_tol	*tol;
{
	register struct loopuse	*lu;
	struct edgeuse		*eu;
	vect_t			delta;
	struct vertex		*v;
	register struct vertex_g *vg;
	int			magic1;

	NMG_CK_FACEUSE(fu);
	RT_CK_TOL(tol);

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
		if (magic1 == NMG_VERTEXUSE_MAGIC) {
			struct vertexuse	*vu;
			vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
			v = vu->v_p;
			NMG_CK_VERTEX(v);
			if( !(vg = v->vg_p) )  continue;
			NMG_CK_VERTEX_G(vg);
			VSUB2(delta, vg->coord, pt);
			if ( MAGSQ(delta) < tol->dist_sq)
				return(vu);
		}
		else if (magic1 == NMG_EDGEUSE_MAGIC) {
			for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
				v = eu->vu_p->v_p;
				NMG_CK_VERTEX(v);
				if( !(vg = v->vg_p) )  continue;
				NMG_CK_VERTEX_G(vg);
				VSUB2(delta, vg->coord, pt);
				if ( MAGSQ(delta) < tol->dist_sq)
					return(eu->vu_p);
			}
		} else
			rt_bomb("nmg_find_vu_in_face() Bogus child of loop\n");
	}
	return ((struct vertexuse *)NULL);
}

/*
 *			N M G _ G L U E F A C E S
 *
 *	given a shell containing "n" faces whose outward oriented faceuses are
 *	enumerated in "fulist", glue the edges of the faces together
 *
 */
void
nmg_gluefaces(fulist, n)
struct faceuse *fulist[];
int n;
{
	struct shell	*s;
	struct loopuse	*lu;
	struct edgeuse	*eu;
	int		i;
	int		f_no;		/* Face number */
	
	NMG_CK_FACEUSE(fulist[0]);
	s = fulist[0]->s_p;
	NMG_CK_SHELL(s);

	/* First, perform some checks */
	for (i = 0 ; i < n ; ++i) {
		register struct faceuse	*fu;

		fu = fulist[i];
		NMG_CK_FACEUSE(fu);
		if (fu->s_p != s) {
			rt_log("nmg_gluefaces() in %s at %d. faceuses don't share parent\n",
				__FILE__, __LINE__);
			rt_bomb("nmg_gluefaces\n");
		}
		lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC) {
			/* Not an edgeuse, probably a vertexuse */
			rt_bomb("nmg_gluefaces() Cannot glue edges of face on vertex\n");
		} else {
			eu = RT_LIST_FIRST( edgeuse, &lu->down_hd );
			NMG_CK_EDGEUSE(eu);
		}
	}

	for (i=0 ; i < n ; ++i) {
		lu = RT_LIST_FIRST( loopuse, &fulist[i]->lu_hd );
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			for( f_no = i+1; f_no < n; f_no++ )  {
				struct loopuse		*lu2;
				register struct edgeuse	*eu2;

				if( eu->radial_p != eu->eumate_p )  break;

				lu2 = RT_LIST_FIRST(loopuse,
					&fulist[f_no]->lu_hd);
				for( RT_LIST_FOR( eu2, edgeuse, &lu2->down_hd ) )  {
					if (EDGESADJ(eu, eu2))
					    	nmg_moveeu(eu, eu2);
				}
			}
		}
	}
}


/*
 *			N M G _ F I N D E U
 *
 *	find an edgeuse in a shell between a pair of verticies
 */
struct edgeuse *
nmg_findeu(v1, v2, s, eup)
struct vertex *v1, *v2;
struct shell *s;
struct edgeuse *eup;
{
	register struct vertexuse	*vu;
	register struct edgeuse		*eu;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);
	NMG_CK_SHELL(s);

	if (rt_g.NMG_debug & DEBUG_FINDEU)
		rt_log("nmg_findeu() looking for edge between %8x and %8x other than %8x/%8x\n",
		v1, v2, eup, eup->eumate_p);

	for( RT_LIST_FOR( vu, vertexuse, &v1->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (!vu->up.magic_p) {
			rt_log("nmg_findeu() in %s at %d vertexuse has null parent\n",
				__FILE__, __LINE__);
			rt_bomb("nmg_findeu\n");
		}

		if (*vu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;

		if (rt_g.NMG_debug & DEBUG_FINDEU )  {
			rt_log("checking edgeuse %8x vertex pair (%8x, %8x)\n",
				vu->up.eu_p, vu->up.eu_p->vu_p->v_p,
				vu->up.eu_p->eumate_p->vu_p->v_p);
		}

		/* look for an edgeuse pair (other than the one we have)
		 * on the vertices we want
		 * the edgeuse pair should be a dangling edge
		 */
		eu = vu->up.eu_p;
		if( eu != eup && eu->eumate_p != eup &&
		    eu->eumate_p->vu_p->v_p == v2  &&
		    eu->eumate_p == eu->radial_p) {

		    	/* if the edgeuse we have found is a part of a face
		    	 * in the proper shell, we've found what we're looking
		    	 * for.
		    	 */
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
			    eu->up.lu_p->up.fu_p->s_p == s) {

			    	if (rt_g.NMG_debug & DEBUG_FINDEU)
				    	rt_log("Found %8x/%8x\n",
				    		eu, eu->eumate_p);

			    	if (eup->up.lu_p->up.fu_p->orientation ==
			    	    eu->up.lu_p->up.fu_p->orientation)
				    	return(eu);
			    	else
			    		return(eu->eumate_p);
			    }
		    	else
		    		if (rt_g.NMG_debug & DEBUG_FINDEU)
		    		rt_log("ignoring an edge because it has wrong parent\n");

		}
	}

	if (rt_g.NMG_debug & DEBUG_FINDEU)
	    	rt_log("nmg_findeu search failed\n");

	return((struct edgeuse *)NULL);
}

/*
 *			N M G _ J L
 *
 *  Join two loops together which share a common edge,
 *  such that both occurances of the common edge are deleted.
 */
void
nmg_jl(lu, eu)
struct loopuse *lu;
struct edgeuse *eu;
{
	struct edgeuse *eu_r, *nexteu;

	NMG_CK_LOOPUSE(lu);

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);
	eu_r = eu->radial_p;
	NMG_CK_EDGEUSE(eu_r);
	NMG_CK_EDGEUSE(eu_r->eumate_p);

	if (eu->up.lu_p != lu)
		rt_bomb("nmg_jl: edgeuse is not child of loopuse?\n");

	if (*eu_r->up.magic_p != NMG_LOOPUSE_MAGIC)
		rt_bomb("nmg_jl: radial edgeuse not part of loopuse\n");

	if (eu_r->up.lu_p == lu)
		rt_bomb("nmg_jl: trying to join a loop to itself\n");

	if (lu->up.magic_p != eu_r->up.lu_p->up.magic_p)
		rt_bomb("nmg_jl: loopuses do not share parent\n");

	if (eu_r->up.lu_p->orientation != lu->orientation)  {
		rt_log("nmg_jl: eu_r->up = %s, lu = %s\n",
			nmg_orientation(eu_r->up.lu_p->orientation),
			nmg_orientation(lu->orientation) );
		rt_bomb("nmg_jl: can't join loops of different orientation!\n");
	}

	if (eu->radial_p->eumate_p->radial_p->eumate_p != eu ||
	    eu->eumate_p->radial_p->eumate_p->radial_p != eu)
	    	rt_bomb("nmg_jl: edgeuses must be sole uses of edge to join loops\n");

	/*
	 * Remove all the edgeuses "ahead" of our radial and insert them
	 * "behind" the current edgeuse.
	 */
	nexteu = RT_LIST_PNEXT_CIRC(edgeuse, eu_r);
	while (nexteu != eu_r) {
		RT_LIST_DEQUEUE(&nexteu->l);
		RT_LIST_INSERT(&eu->l, &nexteu->l);
		nexteu->up.lu_p = eu->up.lu_p;

		RT_LIST_DEQUEUE(&nexteu->eumate_p->l);
		RT_LIST_APPEND(&eu->eumate_p->l, &nexteu->eumate_p->l);
		nexteu->eumate_p->up.lu_p = eu->eumate_p->up.lu_p;

		nexteu = RT_LIST_PNEXT_CIRC(edgeuse, eu_r);
	}

	/*
	 * The other loop just has the one edgeuse/edge left in it.
	 * Delete the other loop.
	 */
	nmg_klu(eu_r->up.lu_p);

	/*
	 * Kill the one remaining use of the "shared" edge and
	 * voila: one contiguous loop.
	 */
	nmg_keu(eu);
}

/*
 *			N M G _ J O I N _ 2 L O O P S
 *
 *  Intended to join an interior and exterior loop together,
 *  by building a bridge between the two indicated vertices.
 *
 *  This routine can be used to join two exterior loops which do not
 *  overlap, and it can also be used to join an exterior loop with
 *  a loop of oposite orientation that lies entirely within it.
 *  This restriction is important, but not checked for.
 *
 *  If the two vertexuses reference distinct vertices, then two new
 *  edges are built to bridge the loops together.
 *  If the two vertexuses share the same vertex, then it is even easier.
 *
 *  Returns the replacement for vu2.
 */
struct vertexuse *
nmg_join_2loops( vu1, vu2 )
struct vertexuse	*vu1;
struct vertexuse	*vu2;
{
	struct edgeuse	*eu1, *eu2;
	struct edgeuse	*new_eu;
	struct edgeuse	*first_new_eu;
	struct edgeuse	*second_new_eu;
	struct edgeuse	*final_eu2;
	struct loopuse	*lu1, *lu2;
	int		new_orient;

	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);
	eu1 = vu1->up.eu_p;
	eu2 = vu2->up.eu_p;
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	lu1 = eu1->up.lu_p;
	lu2 = eu2->up.lu_p;
	NMG_CK_LOOPUSE(lu1);
	NMG_CK_LOOPUSE(lu2);

	if( lu1 == lu2 || lu1->l_p == lu2->l_p )
		rt_bomb("nmg_join_2loops: can't join loop to itself\n");

	if( lu1->up.fu_p != lu2->up.fu_p )
		rt_bomb("nmg_join_2loops: can't join loops in different faces\n");

	/* Joining same & opp gives same.  */
	if( lu1->orientation != lu2->orientation )  {
		new_orient = OT_SAME;
	} else {
		new_orient = lu1->orientation;
	}

	if( vu1->v_p != vu2->v_p )  {
		/*
		 *  Start by taking a jaunt from vu1 to vu2 and back.
		 */
		/* insert 0 length edge, before eu1 */
		first_new_eu = nmg_eins(eu1);
		/* split the new edge, and connect it to vertex 2 */
		second_new_eu = nmg_eusplit( vu2->v_p, first_new_eu );
		first_new_eu = RT_LIST_PPREV_CIRC(edgeuse, second_new_eu);
		/* Make the two new edgeuses share just one edge */
		nmg_moveeu( second_new_eu, first_new_eu );
		/* first_new_eu is eu that enters shared vertex */
		vu1 = second_new_eu->vu_p;
	} else {
		second_new_eu = eu1;
		first_new_eu = RT_LIST_PPREV_CIRC(edgeuse, second_new_eu);
		NMG_CK_EDGEUSE(second_new_eu);
	}
	/* second_new_eu is eu that departs from shared vertex */
	vu2 = second_new_eu->vu_p;	/* replacement for original vu2 */
	if( vu1->v_p != vu2->v_p )  rt_bomb("nmg_join_2loops: jaunt failed\n");

	/*
	 *  Gobble edges off of loop2, and insert them into loop1,
	 *  between first_new_eu and second_new_eu.
	 *  The final edge from loop 2 will then be followed by
	 *  second_new_eu.
	 */
	final_eu2 = RT_LIST_PPREV_CIRC(edgeuse, eu2 );
	while( RT_LIST_NON_EMPTY( &lu2->down_hd ) )  {
		eu2 = RT_LIST_PNEXT_CIRC(edgeuse, final_eu2);

		RT_LIST_DEQUEUE(&eu2->l);
		RT_LIST_INSERT(&second_new_eu->l, &eu2->l);
		eu2->up.lu_p = lu1;

		RT_LIST_DEQUEUE(&eu2->eumate_p->l);
		RT_LIST_APPEND(&second_new_eu->eumate_p->l, &eu2->eumate_p->l);
		eu2->eumate_p->up.lu_p = lu1->lumate_p;
	}

	lu1->orientation = lu1->lumate_p->orientation = new_orient;

	/* Kill entire (null) loop associated with lu2 */
	nmg_klu(lu2);

	nmg_veu( &lu1->down_hd, &lu1->l.magic );	/* XXX sanity */

	return vu2;
}

/*			N M G _ S I M P L I F Y _ L O O P
 *
 *	combine adjacent loops within the same parent
 */
void
nmg_simplify_loop(lu)
struct loopuse *lu;
{
	struct edgeuse *eu, *eu_r, *tmpeu;

	NMG_CK_LOOPUSE(lu);
	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		return;

	eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
	while (RT_LIST_NOT_HEAD(eu, &lu->down_hd) ) {

		NMG_CK_EDGEUSE(eu);

		eu_r = eu->radial_p;
		NMG_CK_EDGEUSE(eu_r);

		/* if the radial edge is part of a loop, and the loop of
		 * the other edge is a part of the same object (face)
		 * as the loop containing the current edge, and my
		 * edgeuse mate is radial to my radial`s edgeuse
		 * mate, and the radial edge is a part of a loop other
		 * than the one "eu" is a part of 
		 * then this is a "worthless" edge between these two loops.
		 */
		if (*eu_r->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    eu_r->up.lu_p->up.magic_p == lu->up.magic_p &&
		    eu->eumate_p->radial_p == eu->radial_p->eumate_p &&
		    eu_r->up.lu_p != lu) {

		    	/* save a pointer to where we've already been
		    	 * so that when eu becomes an invalid pointer, we
		    	 * still know where to pick up from.
		    	 */
		    	tmpeu = RT_LIST_PLAST(edgeuse, eu);

			nmg_jl(lu, eu);

		    	/* Since all the new edges will have been appended
		    	 * after tmpeu, we can pick up processing with the
		    	 * edgeuse immediately after tmpeu
		    	 */
		    	eu = tmpeu;

		    	if (rt_g.NMG_debug &(DEBUG_PLOTEM|DEBUG_PL_ANIM) &&
			    *lu->up.magic_p == NMG_FACEUSE_MAGIC ) {
		    	    	static int fno=0;

				nmg_pl_2fu("After_joinloop%d.pl", fno++,
				    lu->up.fu_p, lu->up.fu_p->fumate_p, 0);
					
		    	}
		}
		eu = RT_LIST_PNEXT(edgeuse, eu);
	}
}


/*
 *			N M G _ K I L L _ S N A K E S
 *
 */
void
nmg_kill_snakes(lu)
struct loopuse *lu;
{
	struct edgeuse *eu, *eu_r;
	struct vertexuse *vu;
	struct vertex *v;

	NMG_CK_LOOPUSE(lu);
	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		return;

	eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);
	while (RT_LIST_NOT_HEAD(eu, &lu->down_hd) ) {

		NMG_CK_EDGEUSE(eu);

		eu_r = eu->radial_p;
		NMG_CK_EDGEUSE(eu_r);

		/* if the radial edge is a part of the same loop, and
		 * this edge is not used by anyplace else, and the radial edge
		 * is also the next edge, this MAY be the tail of a snake!
		 */

		if (*eu_r->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    eu_r->up.lu_p == eu->up.lu_p &&
		    eu->eumate_p->radial_p == eu->radial_p->eumate_p &&
		    RT_LIST_PNEXT_CIRC(edgeuse, eu) == eu_r) {

		    	/* if there are no other uses of the vertex
		    	 * between these two edgeuses, then this is
		    	 * indeed the tail of a snake
		    	 */
			v = eu->eumate_p->vu_p->v_p;
			vu = RT_LIST_FIRST(vertexuse, &v->vu_hd);
			while (RT_LIST_NOT_HEAD(vu, &v->vu_hd) &&
			      (vu->up.eu_p == eu->eumate_p ||
			       vu->up.eu_p == eu_r) )
				vu = RT_LIST_PNEXT(vertexuse, vu);

			if (! RT_LIST_NOT_HEAD(vu, &v->vu_hd) ) {
				/* this is the tail of a snake! */
				nmg_keu(eu_r);
				nmg_keu(eu);
				eu = RT_LIST_FIRST(edgeuse, &lu->down_hd);

			    	if (rt_g.NMG_debug &(DEBUG_PLOTEM|DEBUG_PL_ANIM) &&
				    *lu->up.magic_p == NMG_FACEUSE_MAGIC ) {
			    	    	static int fno=0;

					nmg_pl_2fu("After_joinloop%d.pl", fno++,
					    lu->up.fu_p, lu->up.fu_p->fumate_p, 0);

			    	}


			} else
				eu = RT_LIST_PNEXT(edgeuse, eu);
		} else
			eu = RT_LIST_PNEXT(edgeuse, eu);
	}
}


/*			N M G _ S I M P L I F Y _ F A C E
 *
 *
 *	combine adjacent loops within a face which serve no apparent purpose
 *	by remaining separate and distinct.  Kill "wire-snakes" in face.
 */
void
nmg_simplify_face(fu)
struct faceuse *fu;
{
	struct loopuse *lu, *lu2;
	int overlap;

	NMG_CK_FACEUSE(fu);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd))
		nmg_simplify_loop(lu);

	
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd))
		nmg_kill_snakes(lu);

#if 0
	/* An exterior loop in face that:
	 *	1) has an extent which does not overlap the extent of another
	 *		loop in this face.
	 *	2) does not share a vertex with another loop of this face.
	 *
	 * defines the boundary of a separated "face patch" and should 
	 * become a separate face.
	 */

	/* a face of one loop cannot be subdivided */
	if (RT_LIST_NEXT(loopuse, &fu->lu_hd) ==
	    RT_LIST_LAST(loopuse, &fu->lu_hd))
	    	return;


	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		overlap = 0;
		for (RT_LIST_FOR(lu2, loopuse, &fu->lu_hd)) {
			if (lu == lu2) continue;
			if (V3RPP_OVERLAP(
			    lu->lg_p->min_pt, lu->lg_p->max_pt,
			    lu2->lg_p->min_pt, lu2->lg_p->max_pt) ) {
				overlap
			}
		}


	}
#endif


}



#if 0
/*XXX	Group sets of loops within a face into overlapping units.
 *	This will allow us to separate dis-associated groups into separate
 *	(but equal ;-) faces
 */

struct groupie {
	struct rt_list	l;
	struct loopuse *lu;
}

struct loopgroup {
	struct rt_list	l;
	struct rt_list	groupies;
} groups;


struct loopgroup *
group(lu, groups)
struct loopuse *lu;
struct rt_list *groups;
{
	struct loopgroup *group;
	struct groupie *groupie;

	for (RT_LIST_FOR(group, loopgroup, groups)) {
		for (RT_LIST_FOR(groupie, groupie, &groupies)) {
			if (groupie->lu == lu)
				return(group);
		}
	}
	return NULL;
}

void
new_loop_group(lu, groups)
struct loopuse *lu;
struct rt_list *groups;
{
	struct loopgroup *lg;
	struct groupie *groupie;

	lg = (struct loopgroup *)rt_calloc(sizeof(struct loopgroup), "loopgroup");
	groupie = (struct groupie *)rt_calloc(sizeof(struct groupie), "groupie");
	groupie->lu = lu;

	RT_LIST_INIT(&lg->groupies);
	RT_LIST_APPEND(&lg->groupies, &groupie->l);
	RT_LIST_APPEND(groups, &lg.l);
}

void
merge_groups(lg1, lg2);
struct loopgroup *lg1, *lg2;
{
	struct groupie *groupie;

	while (RT_LIST_WHILE(groupie, groupie, &lg2->groupies)) {
		RT_LIST_DEQUEUE(&(groupie->l));
		RT_LIST_APPEND(&(lg1->groupies), &(groupie->l))
	}
	RT_DEQUEUE(&(lg2->l));
	rt_free((char *)lg2, "free loopgroup 2 of merge");
}
void
free_groups(head)
struct rt_list *head;
{
	while
}

	RT_LIST_INIT(&groups);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

		/* build loops out of exterior loops only */
		if (lu->orientation == OT_OPPOSITE)
			continue;

		if (group(lu) == NULL)
			new_loop_group(lu, &groups);

		for (RT_LIST_FOR(lu2, loopuse, &fu->lu_hd)) {
			if (lu == lu2 ||
			    group(lu, &groups) == group(lu2, &groups))
				continue;
			if (loops_touch_or_overlap(lu, lu2))
				merge_groups(group(lu), group(lu2));
		}



	}
#endif

/*
 *			N M G _ S I M P L I F Y _ S H E L L
 */
void
nmg_simplify_shell(s)
struct shell *s;
{
	struct faceuse *fu;
	NMG_CK_SHELL(s);

	for (RT_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		nmg_simplify_face(fu);
	}
}

/*			N M G _ C U T _ L O O P
 *
 *	Divide a loop of edges between two vertexuses
 *
 *	Make a new loop between the two vertexes, and split it and
 *	the loop of the vertexuses at the same time.
 *
 *	Old Loop      New loop	Resulting loops
 *
 *	    v1		v1	    v1
 *	    |	        |	    |\
 *	    V	        V	    V V
 *	*---*---*	*	*---* *---*
 *	|	|	|	|   | |   |
 *	|	|	|	|   | |   |
 *	*---*---*	*	*---* *---*
 *	    ^		^	    ^ ^
 *	    |	        |	    |/
 *	   v2		v2	    v2
 *
 *  Returns the new loopuse pointer.
 */
struct loopuse *
nmg_cut_loop(vu1, vu2)
struct vertexuse *vu1, *vu2;
{
	struct loopuse *lu, *oldlu;
	struct edgeuse *eu1, *eu2, *eunext, *neweu, *eu;
	struct model	*m;
	FILE		*fd;
	char		name[32];
	static int	i=0;

	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);

	eu1 = vu1->up.eu_p;
	eu2 = vu2->up.eu_p;
	NMG_CK_EDGEUSE(eu1);
	NMG_CK_EDGEUSE(eu2);
	oldlu = eu1->up.lu_p;
	NMG_CK_LOOPUSE(oldlu);
	if (eu2->up.lu_p != oldlu) {
		rt_log("nmg_cut_loop() at %d in %s vertices should be decendants of same loop\n",
			__LINE__, __FILE__);
		rt_bomb("nmg_cut_loop\n");
	}
	NMG_CK_FACEUSE(oldlu->up.fu_p);
	m = oldlu->up.fu_p->s_p->r_p->m_p;
	NMG_CK_MODEL(m);

	if (rt_g.NMG_debug & DEBUG_CUTLOOP) {
		rt_log("\tnmg_cut_loop\n");
		if (rt_g.NMG_debug & DEBUG_PLOTEM) {
			long		*tab;
			tab = (long *)rt_calloc( m->maxindex, sizeof(long),
				"nmg_cut_loop flag[] 1" );

			(void)sprintf(name, "Before_cutloop%d.pl", ++i);
			rt_log("nmg_cut_loop() plotting %s\n", name);
			if ((fd = fopen(name, "w")) == (FILE *)NULL) {
				(void)perror(name);
				exit(-1);
			}

			nmg_pl_fu(fd, oldlu->up.fu_p, tab, 100, 100, 100);
			nmg_pl_fu(fd, oldlu->up.fu_p->fumate_p, tab, 100, 100, 100);
			(void)fclose(fd);
			rt_free( (char *)tab, "nmg_cut_loop flag[] 1" );
		}
	}

	nmg_ck_lueu(oldlu, "oldlu (fresh)");

	/* make a new loop structure for the new loop & throw away
	 * the vertexuse we don't need
	 */
	lu = nmg_mlv(oldlu->up.magic_p, (struct vertex *)NULL,
		oldlu->orientation);

	nmg_kvu(RT_LIST_FIRST(vertexuse, &lu->down_hd));
	nmg_kvu(RT_LIST_FIRST(vertexuse, &lu->lumate_p->down_hd));
	/* nmg_kvu() does RT_LIST_INIT() on down_hd */

	/* move the edges into one of the uses of the new loop */
	for (eu = eu2 ; eu != eu1 ; eu = eunext) {
		eunext = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);

		RT_LIST_DEQUEUE(&eu->l);
		RT_LIST_INSERT(&lu->down_hd, &eu->l);
		RT_LIST_DEQUEUE(&eu->eumate_p->l);
		RT_LIST_APPEND(&lu->lumate_p->down_hd, &eu->eumate_p->l);
		eu->up.lu_p = lu;
		eu->eumate_p->up.lu_p = lu->lumate_p;
	}
	nmg_ck_lueu(lu, "lu check1");	/*LABLABLAB*/

	/* make an edge to "cap off" the new loop */
	neweu = nmg_me(eu1->vu_p->v_p, eu2->vu_p->v_p, nmg_eups(eu1));

	/* move the new edgeuse into the new loopuse */
	RT_LIST_DEQUEUE(&neweu->l);
	RT_LIST_INSERT(&lu->down_hd, &neweu->l);
	neweu->up.lu_p = lu;

	/* move the new edgeuse mate into the new loopuse mate */
	RT_LIST_DEQUEUE(&neweu->eumate_p->l);
	RT_LIST_APPEND(&lu->lumate_p->down_hd, &neweu->eumate_p->l);
	neweu->eumate_p->up.lu_p = lu->lumate_p;

	nmg_ck_lueu(lu, "lu check2");	/*LABLABLAB*/


	/* now we go back and close up the loop we just ripped open */
	eunext = nmg_me(eu2->vu_p->v_p, eu1->vu_p->v_p, nmg_eups(eu1));

	RT_LIST_DEQUEUE(&eunext->l);
	RT_LIST_INSERT(&eu1->l, &eunext->l);
	RT_LIST_DEQUEUE(&eunext->eumate_p->l);
	RT_LIST_APPEND(&eu1->eumate_p->l, &eunext->eumate_p->l);
	eunext->up.lu_p = eu1->up.lu_p;
	eunext->eumate_p->up.lu_p = eu1->eumate_p->up.lu_p;


	/* make sure new edgeuses are radial to each other */
	nmg_moveeu(neweu, eunext);

	nmg_ck_lueu(oldlu, "oldlu");
	nmg_ck_lueu(lu, "lu");	/*LABLABLAB*/


	if (rt_g.NMG_debug & DEBUG_CUTLOOP && rt_g.NMG_debug & DEBUG_PLOTEM) {
		long		*tab;
		tab = (long *)rt_calloc( m->maxindex, sizeof(long),
			"nmg_cut_loop flag[] 2" );

		(void)sprintf(name, "After_cutloop%d.pl", i);
		rt_log("nmg_cut_loop() plotting %s\n", name);
		if ((fd = fopen(name, "w")) == (FILE *)NULL) {
			(void)perror(name);
			exit(-1);
		}

		nmg_pl_fu(fd, oldlu->up.fu_p, tab, 100, 100, 100);
		nmg_pl_fu(fd, oldlu->up.fu_p->fumate_p, tab, 100, 100, 100);
		(void)fclose(fd);
		rt_free( (char *)tab, "nmg_cut_loop flag[] 2" );
	}

	nmg_loop_g(oldlu->l_p);
	nmg_loop_g(lu->l_p);
	return lu;
}

/*
 *			N M G _ S P L I T _ L U _ A T _ V U
 *
 *  In a loop which has at least two distinct uses of a vertex,
 *  split off the edges from "split_vu" to the second occurance of
 *  the vertex into a new loop.
 *  The bounding boxes of both old and new loops will be updated.
 *
 *  Intended primarily for use by nmg_split_touchingloops().
 *
 *  Returns -
 *	NULL	Error
 *	*lu	Loopuse of new loop, on success.
 */
struct loopuse *
nmg_split_lu_at_vu( lu, split_vu )
struct loopuse		*lu;
struct vertexuse	*split_vu;
{
	struct edgeuse		*eu;
	struct vertexuse	*vu;
	struct loopuse		*newlu;
	struct loopuse		*newlumate;
	struct vertex		*split_v;
	int			iteration;

	split_v = split_vu->v_p;
	NMG_CK_VERTEX(split_v);

	/*
	 *  The vertexuse will appear exactly once in the loop, so
	 *  find the edgeuse which has the indicated vertexuse.
	 */
	if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return (struct loopuse *)0;	/* FAIL */

	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		if( vu == split_vu )  goto begin;
	}
	/* Could not find indicated vertex */
	return (struct loopuse *)0;		/* FAIL */

begin:
	/* Make a new loop in the same face */
	newlu = nmg_mlv( lu->up.magic_p, (struct vertex *)NULL, lu->orientation);
	NMG_CK_LOOPUSE(newlu);
	newlumate = newlu->lumate_p;
	NMG_CK_LOOPUSE(newlumate);

	/* Throw away unneeded lone vertexuse */
	nmg_kvu(RT_LIST_FIRST(vertexuse, &newlu->down_hd));
	nmg_kvu(RT_LIST_FIRST(vertexuse, &newlumate->down_hd));
	/* nmg_kvu() does RT_LIST_INIT() on down_hd */

	/* Move edges & mates into new loop until vertex is repeated */
	for( iteration=0; iteration < 10000; iteration++ )  {
		struct edgeuse	*eunext;
		eunext = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);

		RT_LIST_DEQUEUE(&eu->l);
		RT_LIST_INSERT(&newlu->down_hd, &eu->l);
		RT_LIST_DEQUEUE(&eu->eumate_p->l);
		RT_LIST_APPEND(&newlumate->down_hd, &eu->eumate_p->l);

		/* Change edgeuse & mate up pointers */
		eu->up.lu_p = newlu;
		eu->eumate_p->up.lu_p = newlumate;

		/* Advance to next edgeuse */
		eu = eunext;

		/* When split_vertex is encountered, stop */
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		if( vu->v_p == split_v )  break;
	}
	if( iteration >= 10000 )  rt_bomb("nmg_split_lu_at_vu:  infinite loop\n");

	/* Create new bounding boxes for both old & new loops */
	nmg_loop_g(lu->l_p);
	nmg_loop_g(newlu->l_p);

	return newlu;
}

/*
 *			N M G _ S P L I T _ T O U C H I N G L O O P S
 *
 *  Search through all the vertices in a loop.
 *  Whenever there are two distinct uses of one vertex in the loop,
 *  split off all the edges between them into a new loop.
 */
void
nmg_split_touchingloops( lu )
struct loopuse	*lu;
{
	struct edgeuse		*eu;
	struct vertexuse	*vu;
	struct vertex		*v;

top:
	if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		return;

	/* For each edgeuse, get vertexuse and vertex */
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		struct vertexuse	*tvu;

		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);

		/*
		 *  For each vertexuse on vertex list,
		 *  check to see if it points up to the this loop.
		 *  If so, then there is a duplicated vertex.
		 *  Ordinarily, the vertex list will be *very* short,
		 *  so this strategy is likely to be faster than
		 *  a table-based approach, for most cases.
		 */
		for( RT_LIST_FOR( tvu, vertexuse, &v->vu_hd ) )  {
			struct edgeuse		*teu;
			struct loopuse		*tlu;
			struct loopuse		*newlu;

			if( tvu == vu )  continue;
			if( *tvu->up.magic_p != NMG_EDGEUSE_MAGIC )  continue;
			teu = tvu->up.eu_p;
			NMG_CK_EDGEUSE(teu);
			if( *teu->up.magic_p != NMG_LOOPUSE_MAGIC )  continue;
			tlu = teu->up.lu_p;
			NMG_CK_LOOPUSE(tlu);
			if( tlu != lu )  continue;
			/*
			 *  Repeated vertex exists,
			 *  Split loop into two loops
			 */
			newlu = nmg_split_lu_at_vu( lu, vu );
			NMG_CK_LOOPUSE(newlu);

			/* Ensure there are no duplications in new loop */
			nmg_split_touchingloops(newlu);

			/* There is no telling where we will be in the
			 * remainder of original loop, check 'em all.
			 */
			goto top;
		}
	}
}

#if 0
/*			N M G _ E U _ S Q
 *
 *	squeeze an edgeuse out of a list
 *
 *	All uses of the edge being "Squeezed" must be followed by
 *	the same "next" edge
 *
 */
nmg_eu_sq(eu)
struct edgeuse *eu;
{
	struct edgeuse *matenext;
	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGEUSE(eu->eumate_p);

	/* foreach use of this edge, there must be exactly one use of the
	 * previous edge.  There may not be any "extra" uses of the
	 * previous edge
	 */



	matenext = RT_LIST_PNEXT_CIRC(eu->eumate_p);
	NMG_CK_EDGEUSE(matenext);

	RT_LIST_DEQUEUE(eu);
	RT_LIST_DEQUEUE(matenext);

}
#endif

/*
 *			N M G _ M O V E _ F U _ F U
 *
 *  Move everything from the source faceuse into the destination faceuse.
 */
void
nmg_move_fu_fu( dest_fu, src_fu )
register struct faceuse	*dest_fu;
register struct faceuse	*src_fu;
{
	register struct loopuse	*lu;

	NMG_CK_FACEUSE(dest_fu);
	NMG_CK_FACEUSE(src_fu);

	if( dest_fu->orientation != src_fu->orientation )
		rt_bomb("nmg_move_fu_fu: differing orientations\n");

	/* Move all loopuses from src to dest faceuse */
	while( RT_LIST_WHILE( lu, loopuse, &src_fu->lu_hd ) )  {
		RT_LIST_DEQUEUE( &(lu->l) );
		RT_LIST_INSERT( &(dest_fu->lu_hd), &(lu->l) );
		lu->up.fu_p = dest_fu;
	}
}

/*
 *			N M G _ M E R G E _ 2 F A C E S
 *
 *  Move everything from the source face and mate into the
 *  destination face and mate, taking into account face orientations.
 */
void
nmg_merge_2faces(dest_fu, src_fu)
register struct faceuse	*dest_fu;
register struct faceuse	*src_fu;
{
	NMG_CK_FACEUSE(dest_fu);
	NMG_CK_FACEUSE(src_fu);

	if( dest_fu->orientation == src_fu->orientation )  {
		nmg_move_fu_fu(dest_fu, src_fu);
		nmg_move_fu_fu(dest_fu->fumate_p, src_fu->fumate_p);
	} else {
		nmg_move_fu_fu(dest_fu, src_fu->fumate_p);
		nmg_move_fu_fu(dest_fu->fumate_p, src_fu);
	}

	nmg_kfu(src_fu);
}

/*
 *			N M G _ E U _ W I T H _ V U _ I N _ L U
 *
 *  Find an edgeuse starting at a given vertexuse within a loop(use).
 */
struct edgeuse *
nmg_eu_with_vu_in_lu( lu, vu )
struct loopuse		*lu;
struct vertexuse	*vu;
{
	register struct edgeuse	*eu;

	NMG_CK_LOOPUSE(lu);
	NMG_CK_VERTEXUSE(vu);
	if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
		rt_bomb("nmg_eu_with_vu_in_lu: loop has no edges!\n");
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		if( eu->vu_p == vu )  return eu;
	}
	rt_bomb("nmg_eu_with_vu_in_lu:  Unable to find vu!\n");
	/* NOTREACHED */
	return((struct edgeuse *)NULL);
}

/*
 *			N M G _ M O V E _ E G
 *
 *  For every edge in shell 's', change all occurances of edge geometry
 *  structure 'old_eg' to be 'new_eg'.
 */
void
nmg_move_eg( old_eg, new_eg, s )
struct edge_g	*old_eg;
struct edge_g	*new_eg;
struct shell	*s;
{
	struct  faceuse		*fu;
	struct face		*f;
	struct loopuse		*lu;
	struct loop		*l;
	register struct edgeuse		*eu;
	register struct edge		*e;

	NMG_CK_EDGE_G(old_eg);
	NMG_CK_EDGE_G(new_eg);
	NMG_CK_SHELL(s);

	/* Faces in shell */
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		f = fu->f_p;
		NMG_CK_FACE(f);
		/* Loops in face */
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			l = lu->l_p;
			NMG_CK_LOOP(l);
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
				/* Loop of Lone vertex */
				continue;
			}
			for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
				NMG_CK_EDGEUSE(eu);
				e = eu->e_p;
				NMG_CK_EDGE(e);
				if(e->eg_p == old_eg)  {
					nmg_use_edge_g( e, new_eg );
				}
			}
		}
	}
	/* Wire loops in shell */
	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		l = lu->l_p;
		NMG_CK_LOOP(l);
		if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
			/* Wire loop of Lone vertex */
			continue;
		}
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			NMG_CK_EDGEUSE(eu);
			e = eu->e_p;
			NMG_CK_EDGE(e);
			if(e->eg_p == old_eg)  {
				nmg_use_edge_g( e, new_eg );
			}
		}
	}
	/* Wire edges in shell */
	for( RT_LIST_FOR( eu, edgeuse, &s->eu_hd ) )  {
		NMG_CK_EDGEUSE(eu);
		e = eu->e_p;
		NMG_CK_EDGE(e);
		if(e->eg_p == old_eg)  {
			nmg_use_edge_g( e, new_eg );
		}
	}
}

/*
 *			N M G _ D U P _ L O O P
 *
 *  Called by nmg_dup_face
 */
struct loopuse *
nmg_dup_loop(lu, parent, trans_tbl)
struct loopuse *lu;
long	*parent;		/* fu or shell ptr */
long	**trans_tbl;
{
	struct loopuse *new_lu = (struct loopuse *)NULL;
	struct vertexuse *new_vu = (struct vertexuse *)NULL;
	struct vertexuse *old_vu = (struct vertexuse *)NULL;
	struct vertex *old_v = (struct vertex *)NULL;
	struct edgeuse *new_eu = (struct edgeuse *)NULL;
	struct edgeuse *eu = (struct edgeuse *)NULL;
	int i=1;

	NMG_CK_LOOPUSE(lu);

	/* if loop is just a vertex, that's simple to duplicate */
	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		old_vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
		old_v = old_vu->v_p;

		if (NMG_INDEX_TEST(trans_tbl, old_v)) {
			/* this vertex already exists in the new model */
			new_lu = nmg_mlv(parent,
				(struct vertex *)NMG_INDEX_VALUE(trans_tbl, old_v->index),
				lu->orientation);
			rt_log("nmg_dup_loop() existing vertex in new model\n");
		} else {
			/* make a new vertex */
			rt_log("nmg_dup_loop() new vertex in new model\n");
			new_lu = nmg_mlv(parent,
				(struct vertex *)NULL,
				lu->orientation);

			new_vu = RT_LIST_FIRST(vertexuse, &new_lu->down_hd);
			trans_tbl[old_v->index] = (long *)new_vu->v_p;

			if (old_v->vg_p) {
				new_vu = RT_LIST_FIRST(vertexuse, &new_lu->down_hd);
				nmg_vertex_gv(new_vu->v_p, old_vu->v_p->vg_p->coord);
			}
		}
		return;
	}

	/* This loop is an edge-loop.  This is a little more work
	 * First order of business is to duplicate the vertex/edge makeup.
	 */
	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {

		NMG_CK_EDGEUSE(eu);
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		old_v = eu->vu_p->v_p;
		if (new_lu == (struct loopuse *)NULL) {
			/* this is the first edge in the new loop */
			if (NMG_INDEX_VALUE(trans_tbl, old_v->index)) {
				struct vertex *ck_v;
				ck_v = (struct vertex *)NMG_INDEX_VALUE(trans_tbl, old_v->index);
				NMG_CK_VERTEX( ck_v );
			}

			new_lu = nmg_mlv(parent,
				(struct vertex *)NMG_INDEX_VALUE(trans_tbl, old_v->index),
				lu->orientation);
			new_vu = RT_LIST_FIRST(vertexuse,
				&new_lu->down_hd);

			NMG_CK_VERTEXUSE(new_vu);
			NMG_CK_VERTEX(new_vu->v_p);

			if (!trans_tbl[old_v->index])
				trans_tbl[old_v->index] =
					(long *)new_vu->v_p;

			new_eu = nmg_meonvu(new_vu);

			if (old_v->vg_p) {
				NMG_CK_VERTEX_G(old_v->vg_p);
				nmg_vertex_gv(new_vu->v_p,
					eu->vu_p->v_p->vg_p->coord);
			}

		} else {
			/* not the first edge in new loop */
			new_eu = RT_LIST_LAST(edgeuse, &new_lu->down_hd);
			NMG_CK_EDGEUSE(new_eu);

			new_eu = nmg_eusplit(
				(struct vertex *)trans_tbl[old_v->index],
				new_eu);

			new_vu = new_eu->vu_p;

			if (!trans_tbl[old_v->index])
				trans_tbl[old_v->index] = (long *)new_vu->v_p;

			if (old_v->vg_p)
				nmg_vertex_gv(new_vu->v_p,
					eu->vu_p->v_p->vg_p->coord);

		}
		/* XXX ought to do something with edges & trans_tbl here? */
	}

	/* Now that we've got all the right topology created and the
	 * vertex geometries are in place we can create the edge geometries.
	 */
	for(RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		nmg_edge_g(eu->e_p);
		/* XXX ought to do something with edges & trans_tbl here? */
	}
	nmg_loop_g(new_lu->l_p);
	return (new_lu);
}

/*
 *			N M G _ D U P _ F A C E
 *
 *  Construct a duplicate of a face, 
 */
struct faceuse *
nmg_dup_face(fu, s)
struct faceuse *fu;
struct shell *s;
{
	struct loopuse *new_lu, *lu;
	struct faceuse *new_fu = (struct faceuse *)NULL;
	struct model	*m;
	long		**trans_tbl;

	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_SHELL(s);

	m = nmg_find_model( (long *)s );
	trans_tbl = (long **)rt_calloc(m->maxindex, sizeof(long *),
			"nmg_dup_face trans_tbl");

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (new_fu) {
			new_lu = nmg_dup_loop(lu, new_fu, trans_tbl);
		} else {
			new_lu = nmg_dup_loop(lu, s, trans_tbl);
			new_fu = nmg_mf(new_lu);
		}
	}

	if (fu->f_p->fg_p) {
		nmg_face_g(new_fu, fu->f_p->fg_p->N);
	}
	new_fu->orientation = fu->orientation;
	new_fu->fumate_p->orientation = fu->fumate_p->orientation;

	rt_free((char *)trans_tbl, "nmg_dup_face trans_tbl");

	return(new_fu);
}
