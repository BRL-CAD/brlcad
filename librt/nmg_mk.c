/*
 *		N M G _ M K . C
 *
 *	Support routines for Non-Manifold Geometry
 *
 *	Naming convention
 *		nmg_m* routines "make" NMG structures.
 *		nmg_m* routines "kill" (delete) NMG structures.
 *		nmg_c* routines "create" things using NMG structures.
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
 *	edges of loops of the same face must not overlap
 *	the "magic" member of each struct is the first item.  In this
 *		way, we can have routines work with diferent kinds of
 *		structs (by passing around pointers to the magic number)
 *		and still keep lint happy
 *
 *
 *  Author -
 *	Lee A. Butler
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
#include "raytrace.h"
#include "nmg.h"

/*
 *			N M G _ I D E N T I F Y _ M A G I C
 *
 *  Given a number which has been found in the magic number field of
 *  a structure (which is typically the first entry),
 *  determine what kind of structure this magic number pertains to.
 *  This is called by the macro NMG_CK_MAGIC() to provide a "hint"
 *  as to what sort of pointer error might have been made.
 */
char *nmg_identify_magic( magic )
long	magic;
{
	switch(magic)  {
	default:
		return("Unknown");
	case NMG_MODEL_MAGIC:
		return("model");
	case NMG_MODEL_A_MAGIC:
		return("model_a");
	case NMG_REGION_MAGIC:
		return("region");
	case NMG_REGION_A_MAGIC:
		return("region_a");
	case NMG_SHELL_MAGIC:
		return("shell");
	case NMG_SHELL_A_MAGIC:
		return("shell_a");
	case NMG_FACE_MAGIC:
		return("face");
	case NMG_FACE_G_MAGIC:
		return("face_g");
	case NMG_FACEUSE_MAGIC:
		return("faceuse");
	case NMG_FACEUSE_A_MAGIC:
		return("faceuse_a");
	case NMG_LOOP_MAGIC:
		return("loop");
	case NMG_LOOP_G_MAGIC:
		return("loop_g");
	case NMG_LOOPUSE_MAGIC:
		return("loopuse");
	case NMG_LOOPUSE_A_MAGIC:
		return("loopuse_a");
	case NMG_EDGE_MAGIC:
		return("edge");
	case NMG_EDGE_G_MAGIC:
		return("edge_g");
	case NMG_EDGEUSE_MAGIC:
		return("edgeuse");
	case NMG_EDGEUSE_A_MAGIC:
		return("edgeuse_a");
	case NMG_VERTEX_MAGIC:
		return("vertex");
	case NMG_VERTEX_G_MAGIC:
		return("vertex_g");
	case NMG_VERTEXUSE_MAGIC:
		return("vertexuse");
	case NMG_VERTEXUSE_A_MAGIC:
		return("vertexuse_a");
	case NMG_LIST_MAGIC:
		return("nmg_list");
	}
}

/*	N M G _ M M
 *
 *	Make Model
 *	Create a new model Essentially
 *	this creates a minimal model system.
 */
struct model *nmg_mm()
{
	struct model *m;

	GET_MODEL(m);

	m->magic = NMG_MODEL_MAGIC;
	m->ma_p = (struct model_a *)NULL;
	NMG_LIST_INIT( &m->r_hd );

	return(m);
}
/*	N M G _ M M R
 *
 *	Make Model, Region
 *	Create a new model, and an "empty" region to go with it.  Essentially
 *	this creates a minimal model system.
 */
struct model *nmg_mmr()
{
	struct model *m;
	struct nmgregion *r;

	m = nmg_mm();
	GET_REGION(r);
	r->l.magic = NMG_REGION_MAGIC;

	r->m_p = m;

	r->ra_p = (struct nmgregion_a *)NULL;
	NMG_LIST_INIT( &r->s_hd );

	NMG_LIST_APPEND( &m->r_hd, &r->l );

	return(m);
}

/*
 *			N M G _ M R S V
 *
 *	Make new region, shell, vertex in model
 *	Create a new (2nd, 3rd, ...) region in model consisting of a minimal
 *	shell.
 */
struct nmgregion *nmg_mrsv(m)
struct model *m;
{
	struct nmgregion *r;

	NMG_CK_MODEL(m);

	GET_REGION(r);
	r->l.magic = NMG_REGION_MAGIC;
	r->m_p = m;

	NMG_LIST_INIT( &r->s_hd );
	(void)nmg_msv(r);

	/* new region goes at "head" of list of regions in model */
	NMG_LIST_APPEND( &m->r_hd, &r->l );

	return(r);
}

/*
 *			N M G _ M S V
 *
 *	Make Shell, Vertex Use, Vertex
 *	Create a new shell in a specified region.  The shell will consist
 *	of a single vertexuse and vertex (which are also created).
 */
struct shell *nmg_msv(r_p)
struct nmgregion	*r_p;
{
	struct shell 		*s;
	struct vertexuse	*vu;

	NMG_CK_REGION(r_p);

	/* set up shell */
	GET_SHELL(s);
	s->l.magic = NMG_SHELL_MAGIC;

	s->r_p = r_p;
	NMG_LIST_APPEND( &r_p->s_hd, &s->l );

	s->sa_p = (struct shell_a *)NULL;
	NMG_LIST_INIT( &s->fu_hd );
	NMG_LIST_INIT( &s->lu_hd );
	NMG_LIST_INIT( &s->eu_hd );

	vu = nmg_mvvu(&s->l.magic);
	s->vu_p = vu;
	return(s);
}

/*	N M G _ M V U
 *	Make Vertexuse on existing vertex
 */
struct vertexuse *nmg_mvu(v, upptr)
struct vertex *v;
long *upptr;		/* pointer to parent struct */
{
	struct vertexuse *vu;

	NMG_CK_VERTEX(v);

	if (*upptr != NMG_SHELL_MAGIC &&
	    *upptr != NMG_LOOPUSE_MAGIC &&
	    *upptr != NMG_EDGEUSE_MAGIC) {
		rt_log("in %s at %d magic not shell, loop, or edge (%d)\n",
		    __FILE__, __LINE__, *upptr);
		rt_bomb("nmg_mvu() Cannot build vertexuse without parent");
	}

	GET_VERTEXUSE(vu);
	vu->l.magic = NMG_VERTEXUSE_MAGIC;

	vu->v_p = v;
	vu->vua_p = (struct vertexuse_a *)NULL;
	NMG_LIST_APPEND( &v->vu_hd, &vu->l );
	vu->up.magic_p = upptr;

	return(vu);
}

/*	N M G _ M V V U
 *	Make Vertex, Vertexuse
 */
struct vertexuse *nmg_mvvu(upptr)
long *upptr;
{
	struct vertex *v;

	GET_VERTEX(v);
	v->magic = NMG_VERTEX_MAGIC;
	NMG_LIST_INIT( &v->vu_hd );
	v->vg_p = (struct vertex_g *)NULL;
	return(nmg_mvu(v, upptr));
}


/*
 *			N M G _ M E
 *
 *	Make edge
 *	Make a new edge between a pair of vertices in a shell.
 *
 *	A new vertex will be made for any NULL vertex pointer parameters.
 *	If we need to make a new vertex and the shell still has its vertexuse
 *	we re-use that vertex rather than freeing and re-allocating.
 *
 *	If both vertices were specified, and the shell also had a
 *	vertexuse pointer, the vertexuse in the shell is killed.
 *
 *	Explicit Return:
 *		edgeuse whose vertex use is v1, whose eumate has vertex v2
 *
 *  Implicit Returns -
 *	1)  If the shell had a lone vertex in vu_p, it is destroyed,
 *	even if both vertices were specified.
 *	2)  The returned edgeuse is the first item on the shell's
 *	eu_hd list, followed immediately by the mate.
 */
struct edgeuse *nmg_me(v1, v2, s)
struct vertex *v1, *v2;
struct shell *s;
{
	struct edge *e;
	struct edgeuse *eu1, *eu2;
	struct vertexuse	*vu;

	if (v1) NMG_CK_VERTEX(v1);
	if (v2) NMG_CK_VERTEX(v2);
	NMG_CK_SHELL(s);

	GET_EDGE(e);
	e->magic = NMG_EDGE_MAGIC;
	e->eg_p = (struct edge_g *)NULL;

	GET_EDGEUSE(eu1);
	GET_EDGEUSE(eu2);
	eu1->l.magic = eu2->l.magic = NMG_EDGEUSE_MAGIC;

	e->eu_p = eu1;

	eu1->radial_p = eu1->eumate_p = eu2;
	eu2->radial_p = eu2->eumate_p = eu1;

	eu1->e_p = eu2->e_p = e;
	eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;

	if (v1)
		eu1->vu_p = nmg_mvu(v1, &eu1->l.magic);
	else if (s->vu_p)  {
		/* steal the vertex from the shell */
		vu = s->vu_p;
		s->vu_p = (struct vertexuse *)NULL;
		eu1->vu_p = vu;
		vu->up.eu_p = eu1;
	}
	else
		eu1->vu_p = nmg_mvvu(&eu1->l.magic);


	if (v2)
		eu2->vu_p = nmg_mvu(v2, &eu2->l.magic);
	else if (s->vu_p)  {
		/* steal the vertex from the shell */
		vu = s->vu_p;
		s->vu_p = (struct vertexuse *)NULL;
		eu2->vu_p = vu;
		vu->up.eu_p = eu2;
	}
	else
		eu2->vu_p = nmg_mvvu(&eu2->l.magic);

	if( s->vu_p )  {
		/* Ensure shell no longer has any stored vertexuse */
		nmg_kvu( s->vu_p );
		s->vu_p = (struct vertexuse *)NULL;
	}

	/* link the edgeuses to the parent shell */
	eu1->up.s_p = eu2->up.s_p = s;

	/* shell's eu_head, eu1, eu2, ... */
	NMG_LIST_APPEND( &s->eu_hd, &eu1->l );
	NMG_LIST_APPEND( &eu1->l, &eu2->l );

	return(eu1);
}



/*
 *			N M G _ M E o n V U
 *
 * Make edge on vertexuse.
 * Vertexuse must be sole element of either a shell or a loopuse.
 */
struct edgeuse *nmg_meonvu(vu)
struct vertexuse *vu;
{
	struct edge *e;
	struct edgeuse *eu1, *eu2;

	NMG_CK_VERTEXUSE(vu);
	if (*vu->up.magic_p != NMG_SHELL_MAGIC &&
	    *vu->up.magic_p != NMG_LOOPUSE_MAGIC) {
		rt_log("Error in %s at %d vertexuse not for shell/loop\n", 
		    __FILE__, __LINE__);
		rt_bomb("nmg_meonvu() cannot make edge, vertexuse not sole element of object");
	}

	GET_EDGE(e);
	e->magic = NMG_EDGE_MAGIC;
	e->eg_p = (struct edge_g *)NULL;

	GET_EDGEUSE(eu1);
	GET_EDGEUSE(eu2);
	eu1->l.magic = eu2->l.magic = NMG_EDGEUSE_MAGIC;

	e->eu_p = eu1;

	eu1->radial_p = eu1->eumate_p = eu2;
	eu2->radial_p = eu2->eumate_p = eu1;

	eu1->e_p = eu2->e_p = e;
	eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;

	/* link edgeuses to parent */
	if (*vu->up.magic_p == NMG_SHELL_MAGIC) {
		struct shell	*s;

		eu1->vu_p = vu;
		eu2->vu_p = nmg_mvu(vu->v_p, &eu2->l.magic);
		s = eu2->up.s_p = eu1->up.s_p = vu->up.s_p;
		if( s->vu_p != vu )
			rt_bomb("nmg_meonvu() vetexuse parent shell disowns vertexuse!\n");
		s->vu_p = (struct vertexuse *)NULL;	/* remove from shell */
		vu->up.eu_p = eu1;
		NMG_LIST_APPEND( &s->eu_hd, &eu2->l );
		NMG_LIST_APPEND( &s->eu_hd, &eu1->l );
	}
	else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		struct loopuse		*lu;
		struct loopuse		*lumate;
		struct vertexuse	*vumate;

		lu = vu->up.lu_p;
		NMG_CK_LOOPUSE(lu);
		lumate = lu->lumate_p;
		NMG_CK_LOOPUSE(lumate);

		/* do a little consistency checking */
		if( lu == lumate )  rt_bomb("nmg_meonvu() lu mate is lu\n");
		if( NMG_LIST_FIRST_MAGIC(&lumate->down_hd) != NMG_VERTEXUSE_MAGIC )
			rt_bomb("nmg_meonvu() mate of vertex-loop is not vertex-loop!\n");
		vumate = NMG_LIST_FIRST(vertexuse, &lumate->down_hd);
		NMG_CK_VERTEXUSE(vumate);
		if( vu == vumate )  rt_bomb("nmg_meonvu() vu mate is vu\n");
		NMG_CK_VERTEX(vu->v_p);
		NMG_CK_VERTEX(vumate->v_p);

		/* edgeuses point at vertexuses */
		eu1->vu_p = vu;
		eu2->vu_p = vumate;

		/* edgeuses point at parent loopuses */
		eu1->up.lu_p = lu;
		eu2->up.lu_p = lumate;

		/* Fix forw & back pointers after "abuse", above */
		NMG_LIST_INIT( &lu->down_hd );
		NMG_LIST_INIT( &lumate->down_hd );
		/* loopuses now point down at edgeuses */
		NMG_LIST_APPEND( &lumate->down_hd, &eu2->l );
		NMG_LIST_APPEND( &lu->down_hd, &eu1->l );

		/* vertexuses point up at edgeuses */
		vu->up.eu_p = eu1;
		vumate->up.eu_p = eu2;
	}
	return(eu1);
}

/*	N M G _ M L
 *
 *	Make Loop from edgeuse list
 *
 *	Pass a pointer to a shell.  The edgeuse child of the shell
 *	is taken as the head of a list of edge(use)s which will form
 *	the new loop.  The loop is created from the first N contiguous
 *	edges.  Thus the end of the new loop is 
 *	delineated by the "next" edge(use)being:
 * 
 * 	A) the first object in the list (no other edgeuses in
 * 		shell list)
 *	B) non-contiguous with the previous edge
 * 
 * A loop is created from this list of edges.  The edges must
 * form a true circuit, or we dump core on your disk.  If we
 * succeed, then the edgeuses are moved from the parameter list
 * to the loop, and the loops are inserted into the shell
 */
struct loopuse *nmg_ml(s)
struct shell *s;
{
	struct loop *l;
	struct loopuse *lu1, *lu2;
	struct edgeuse	*p1;
	struct edgeuse	*p2;
	struct edgeuse	*feu;

	NMG_CK_SHELL(s);

	GET_LOOP(l);
	l->magic = NMG_LOOP_MAGIC;
	l->lg_p = (struct loop_g *)NULL;

	GET_LOOPUSE(lu1);
	lu1->l.magic = NMG_LOOPUSE_MAGIC;
	NMG_LIST_INIT( &lu1->down_hd );

	GET_LOOPUSE(lu2);
	lu2->l.magic = NMG_LOOPUSE_MAGIC;
	NMG_LIST_INIT( &lu2->down_hd );

	l->lu_p = lu1;

	lu1->up.s_p = lu2->up.s_p = s;
	lu1->l_p = lu2->l_p = l;
	lu1->lua_p = lu2->lua_p = (struct loopuse_a *)NULL;
	lu1->orientation = lu2->orientation = OT_UNSPEC;

	lu1->lumate_p = lu2;
	lu2->lumate_p = lu1;

	/* make loop on single vertex */
	if( NMG_LIST_IS_EMPTY( &s->eu_hd ) && s->vu_p )  {
		struct vertexuse	*vu1;
		struct vertexuse	*vu2;

		vu1 = s->vu_p;		/* take vertex from shell */
		s->vu_p = (struct vertexuse *)NULL;

		NMG_LIST_SET_DOWN_TO_VERT( &lu1->down_hd, vu1 );
		vu1->up.lu_p = lu1;

		vu2 = nmg_mvu(vu1->v_p, &lu2->l.magic);
		/* vu2->up.lu_p = lu2; done by nmg_mvu() */
		NMG_LIST_SET_DOWN_TO_VERT( &lu2->down_hd, vu2 );

		/* head, lu1, lu2, ... */
		NMG_LIST_APPEND( &s->lu_hd, &lu1->l );
		NMG_LIST_APPEND( &lu1->l, &lu2->l );
		return(lu1);
	}

	feu = NMG_LIST_FIRST( edgeuse, &s->eu_hd );

	p2 = (struct edgeuse *)NULL;
	while( NMG_LIST_NON_EMPTY( &s->eu_hd ) )  {
		p1 = NMG_LIST_FIRST( edgeuse, &s->eu_hd );
		p2 = p1->eumate_p;
		NMG_CK_EDGEUSE(p1);
		NMG_CK_EDGEUSE(p2);

		/* bogosity check */
		if (p1->up.s_p != s || p2->up.s_p != s)
			rt_bomb("nmg_ml() edgeuse mates don't have proper parent!");

		/* dequeue the first edgeuse */
		NMG_LIST_DEQUEUE( &p1->l );
		if( NMG_LIST_IS_EMPTY( &s->eu_hd ) )  {
			rt_log("in %s at %d edgeuse mate not in this shell\n",
			    __FILE__, __LINE__);
			rt_bomb("nmg_ml");
		}

		/* dequeue the edgeuse's mate */
		NMG_LIST_DEQUEUE( &p2->l );

		/*  Insert the next new edgeuse(s) at tail of the loop's list
		 *  (ie, insert just before the head).
		 *  ... p1, head.
		 */
		NMG_LIST_INSERT( &lu1->down_hd, &p1->l );
		NMG_LIST_INSERT( &lu2->down_hd, &p2->l );

		p1->up.lu_p = lu1;
		p2->up.lu_p = lu2;

		/* If p2's vertex does not match next one comming, quit */
		if( NMG_LIST_IS_EMPTY( &s->eu_hd ) )  break;
		p1 = NMG_LIST_FIRST( edgeuse, &s->eu_hd );
		if( p1->vu_p->v_p != p2->vu_p->v_p )  {
			break;
		}
	}

	/*	printf("p2v %x feu %x\n", p2->vu_p->v_p, feu->vu_p->v_p); */

	if( p2 && p2->vu_p->v_p != feu->vu_p->v_p) {
		rt_log("Edge(use)s do not form proper loop!\n");
		nmg_pr_s(s, (char *)NULL);
		rt_log("Edge(use)s do not form proper loop!\n");
		rt_bomb("nmg_ml\n");
	}

	/* Head, lu1, lu2, ... */
	NMG_LIST_APPEND( &s->lu_hd, &lu2->l );
	NMG_LIST_APPEND( &s->lu_hd, &lu1->l );

	return(lu1);
}

/*	N M G _ M O V E V U
 *	Move a vertexuse to a new vertex
 */
void nmg_movevu(vu, v)
struct vertexuse *vu;
struct vertex *v;
{
	struct vertex	*oldv;

	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(v);

	oldv = vu->v_p;
	NMG_CK_VERTEX(oldv);

	NMG_LIST_DEQUEUE( &vu->l );
	if( NMG_LIST_IS_EMPTY( &oldv->vu_hd ) )  {
		/* last vertexuse on vertex */
		if (oldv->vg_p) FREE_VERTEX_G(oldv->vg_p);
		FREE_VERTEX(oldv);
	}
	NMG_LIST_APPEND( &v->vu_hd, &vu->l );
	vu->v_p = v;
}

/*	N M G _ K V U
 *	Kill vertexuse
 */
void nmg_kvu(vu)
register struct vertexuse *vu;
{
	struct vertex	*v;

	NMG_CK_VERTEXUSE(vu);

	/* ditch any attributes */
	if (vu->vua_p) FREE_VERTEXUSE_A(vu->vua_p);

	v = vu->v_p;
	NMG_CK_VERTEX(v);

	NMG_LIST_DEQUEUE( &vu->l );
	if( NMG_LIST_IS_EMPTY( &v->vu_hd ) )  {
		/* last vertexuse on vertex */
		if (v->vg_p) FREE_VERTEX_G(v->vg_p);
		FREE_VERTEX(v);
	}

	/* erase existence of this vertexuse from parent */
	if (*vu->up.magic_p == NMG_SHELL_MAGIC)  {
		vu->up.s_p->vu_p = (struct vertexuse *)NULL;
	} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		/* Reset the hack */
		NMG_LIST_INIT( &vu->up.lu_p->down_hd );
	} else if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC)  {
		vu->up.eu_p->vu_p = (struct vertexuse *)NULL;
	} else
		rt_bomb("nmg_kvu() killing vertexuse of unknown parent?\n");

	FREE_VERTEXUSE(vu);
}


/*	N M G _ K F U
 *	Kill Faceuse
 *
 *	delete a faceuse and its mate from the parent shell.
 *	Any children found are brutally murdered as well.
 *	The faceuses are dequeued from the parent shell's list here.
 */
void nmg_kfu(fu1)
struct faceuse *fu1;
{
	struct faceuse *fu2;
	struct face	*f1;
	struct face	*f2;

	NMG_CK_FACEUSE(fu1);
	fu2 = fu1->fumate_p;
	NMG_CK_FACEUSE(fu2);
	f1 = fu1->f_p;
	f2 = fu2->f_p;
	NMG_CK_FACE(f1);
	NMG_CK_FACE(f2);
	if( f1 != f2 )
		rt_bomb("nmg_kfu() faceuse mates do not share face!\n");

	/* kill off the children (infanticide?)*/
	while( NMG_LIST_NON_EMPTY( &fu1->lu_hd ) )  {
		nmg_klu( NMG_LIST_FIRST( loopuse, &fu1->lu_hd ) );
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
	NMG_LIST_DEQUEUE( &fu1->l );
	if( NMG_LIST_IS_EMPTY( &fu1->s_p->fu_hd ) )
		rt_bomb("nmg_kfu() faceuse mate not in parent?\n");
	NMG_LIST_DEQUEUE( &fu2->l );

	FREE_FACEUSE(fu1);
	FREE_FACEUSE(fu2);
}


/*	N M G _ K L U
 *	Kill loopuse
 *
 *	if the loop contains any edgeuses or vertexuses they are killed
 *	before the loop is deleted.
 *
 *	We support the concept of killing a loop with no children to
 *	support the routine "nmg_demote_lu"
 */
void nmg_klu(lu1)
struct loopuse *lu1;
{
	struct loopuse *lu2;

	NMG_CK_LOOPUSE(lu1);
	lu2 = lu1->lumate_p;
	NMG_CK_LOOPUSE(lu2);

	if (lu1->l_p != lu2->l_p)
		rt_bomb("nmg_klu() loopmates do not share loop!\n");

	if (*lu1->up.magic_p != *lu2->up.magic_p)
		rt_bomb("nmg_klu() loopuses do not have same type of parent!\n");

	if( NMG_LIST_FIRST_MAGIC( &lu1->down_hd ) != 0 )  {
		long	magic1;
		/* deal with the children */
		magic1 = NMG_LIST_FIRST_MAGIC( &lu1->down_hd );
		if( magic1 != NMG_LIST_FIRST_MAGIC( &lu2->down_hd ) )
			rt_bomb("nmg_klu() loopuses do not have same type of child!\n");

		if( magic1 == NMG_VERTEXUSE_MAGIC )  {
			/* Follow the vertex-loop hack downward,
			 * nmg_kvu() will clean up */
			nmg_kvu( NMG_LIST_FIRST(vertexuse, &lu1->down_hd) );
			nmg_kvu( NMG_LIST_FIRST(vertexuse, &lu2->down_hd) );
		}
		else if ( magic1 == NMG_EDGEUSE_MAGIC) {
			/* delete all edgeuse in the loopuse (&mate) */
			while( NMG_LIST_NON_EMPTY( &lu1->down_hd ) )  {
				nmg_keu(NMG_LIST_FIRST(edgeuse, &lu1->down_hd) );
			}
		}
		else
			rt_bomb("nmg_klu: unknown type for loopuse child\n");
	}


	/* disconnect from parent's list */
	if (*lu1->up.magic_p == NMG_SHELL_MAGIC) {
		NMG_LIST_DEQUEUE( &lu1->l );
		NMG_LIST_DEQUEUE( &lu2->l );
	}
	else if (*lu1->up.magic_p == NMG_FACEUSE_MAGIC) {
		NMG_LIST_DEQUEUE( &lu1->l );
		NMG_LIST_DEQUEUE( &lu2->l );
	}
	else {
		rt_log("in %s at %d Unknown parent for loopuse\n", __FILE__,
		    __LINE__);
		rt_bomb("nmg_klu");
	}

	if (lu1->lua_p) {
		NMG_CK_LOOPUSE_A(lu1->lua_p);
		FREE_LOOPUSE_A(lu1->lua_p);
	}
	if (lu2->lua_p) {
		NMG_CK_LOOPUSE_A(lu2->lua_p);
		FREE_LOOPUSE_A(lu2->lua_p);
	}

	/* XXX when would this not be set? */
/**	if (lu1->l_p) **/ {
		NMG_CK_LOOP(lu1->l_p);
		if (lu1->l_p->lg_p) {
			NMG_CK_LOOP_G(lu1->l_p->lg_p);
			FREE_LOOP_G(lu1->l_p->lg_p);
		}
		FREE_LOOP(lu1->l_p);
		lu1->l_p = lu2->l_p = (struct loop *)NULL;
	}
	FREE_LOOPUSE(lu1);
	FREE_LOOPUSE(lu2);
}

/*
 *			N M G _ M F
 *
 *	Make Face from loop
 *	make a face from a pair of loopuses.  The loopuses must be direct
 *	children of a shell.  The new face will be a child of the same shell.
 */
struct faceuse *nmg_mf(lu1)
struct loopuse *lu1;
{
	struct face *f;
	struct faceuse *fu1, *fu2;
	struct loopuse *lu2;
	struct shell *s;

	NMG_CK_LOOPUSE(lu1);
	if (*lu1->up.magic_p != NMG_SHELL_MAGIC) {
		rt_bomb("nmg_mf() loop must be child of shell for making face");
	}
	lu2 = lu1->lumate_p;
	NMG_CK_LOOPUSE(lu2);
	if (lu2->up.magic_p != lu1->up.magic_p) {
		rt_bomb("nmg_mf() loopuse mate does not have same parent\n");
	}

	s = lu1->up.s_p;
	NMG_CK_SHELL(s);

	GET_FACE(f);
	GET_FACEUSE(fu1);
	GET_FACEUSE(fu2);

	f->fu_p = fu1;
	f->fg_p = (struct face_g *)NULL;
	f->magic = NMG_FACE_MAGIC;

	fu1->l.magic = fu2->l.magic = NMG_FACEUSE_MAGIC;
	NMG_LIST_INIT(&fu1->lu_hd);
	NMG_LIST_INIT(&fu2->lu_hd);
	fu1->s_p = fu2->s_p = s;
	fu1->fumate_p = fu2;
	fu2->fumate_p = fu1;
	fu1->orientation = fu2->orientation = OT_UNSPEC;
	fu1->f_p = fu2->f_p = f;
	fu1->fua_p = fu2->fua_p = (struct faceuse_a *)NULL;

	/* move the loopuses from the shell to the faceuses */
	NMG_LIST_DEQUEUE( &lu1->l );
	NMG_LIST_DEQUEUE( &lu2->l );
	NMG_LIST_APPEND( &fu1->lu_hd, &lu1->l );
	NMG_LIST_APPEND( &fu2->lu_hd, &lu2->l );

	lu1->up.fu_p = fu1;
	lu1->orientation = OT_SAME;
	lu2->up.fu_p = fu2;
	lu2->orientation = OT_SAME;

	/* connect the faces to the parent shell:  head, fu1, fu2... */
	NMG_LIST_APPEND( &s->fu_hd, &fu1->l );
	NMG_LIST_APPEND( &fu1->l, &fu2->l );
	return(fu1);
}

/*
 *			N M G _ K E U
 *
 *	Delete an edgeuse & it's mate on a shell/loop.
 */
void nmg_keu(eu1)
register struct edgeuse *eu1;
{
	register struct edgeuse *eu2;
	struct edge		*e;

	/* prevent mishaps */
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
		/* XXX This may be OK for wire edges / wire loops */
		if( lu1 == lu2 )  rt_bomb("nmg_keu() edgeuses on same loop\n");
		if (lu1->lumate_p != lu2 || lu1 != lu2->lumate_p ) {
			rt_log("lu1=x%x, mate=x%x\n", lu1, lu1->lumate_p);
			rt_log("lu2=x%x, mate=x%x\n", lu2, lu2->lumate_p);
		    	rt_bomb("nmg_keu() edgeuse mates don't belong to loopuse mates\n");
		}

		/* remove the edgeuses from their parents */
		NMG_LIST_DEQUEUE( &eu1->l );
		NMG_LIST_DEQUEUE( &eu2->l );

		/* if deleting this edge would cause parent loop to become
		 * non-contiguous or if there are no more edges left in loop,
		 * we must kill the parent loopuses.  (or demote it)
		 *
		 *if (eu2->vu_p->v_p != eu1->vu_p->v_p || 
		 *    !lu1->down.eu_p)
		 *	nmg_klu(lu1);
		 */
	} else if (*eu1->up.magic_p == NMG_SHELL_MAGIC) {
		if (eu1->up.s_p != eu2->up.s_p) {
			rt_bomb("nmg_keu() edguses don't share parent shell\n");
		}

		/* unlink edgeuses from the parent shell */
		NMG_LIST_DEQUEUE( &eu1->l );
		NMG_LIST_DEQUEUE( &eu2->l );
	}


	/* get rid of any attributes */
	if (eu1->eua_p) {
		if (eu1->eua_p == eu2->eua_p) {
			FREE_EDGEUSE_A(eu1->eua_p);
			eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;
		}
		else {
			FREE_EDGEUSE_A(eu1->eua_p);
			eu1->eua_p = (struct edgeuse_a *)NULL;
		}
	}
	if (eu2->eua_p) {
		FREE_EDGEUSE_A(eu2->eua_p);
		eu2->eua_p = (struct edgeuse_a *)NULL;
	}


	/* kill the vertexuses associated with these edgeuses */
	if (eu1->vu_p) {
		nmg_kvu(eu1->vu_p);
		eu1->vu_p = (struct vertexuse  *)NULL;
	}
	if (eu2->vu_p) {
		nmg_kvu(eu2->vu_p);
		eu2->vu_p = (struct vertexuse  *)NULL;
	}

	FREE_EDGEUSE(eu1);
	FREE_EDGEUSE(eu2);
}

/*	N M G _ K S
 *
 *	Kill a shell and all children
 */
void nmg_ks(s)
struct shell *s;
{

	NMG_CK_SHELL(s);

	while( NMG_LIST_NON_EMPTY( &s->fu_hd ) )
		nmg_kfu( NMG_LIST_FIRST(faceuse, &s->fu_hd) );
	while( NMG_LIST_NON_EMPTY( &s->lu_hd ) )
		nmg_klu( NMG_LIST_FIRST(loopuse, &s->lu_hd) );
	while( NMG_LIST_NON_EMPTY( &s->eu_hd ) )
		nmg_keu( NMG_LIST_FIRST(edgeuse, &s->eu_hd) );
	if( s->vu_p )
		nmg_kvu( s->vu_p );

	NMG_LIST_DEQUEUE( &s->l );

	if (s->sa_p) {
		FREE_SHELL_A(s->sa_p);
	}

	FREE_SHELL(s);
}

/*	N M G _ K R
 *	Kill a region and all shells in it
 */
void nmg_kr(r)
struct nmgregion *r;
{

	NMG_CK_REGION(r);

	while( NMG_LIST_NON_EMPTY( &r->s_hd ) )
		nmg_ks( NMG_LIST_FIRST( shell, &r->s_hd ) );

	NMG_LIST_DEQUEUE( &r->l );

	if (r->ra_p) {
		FREE_REGION_A(r->ra_p);
	}
	FREE_REGION(r);
}

/*	N M G _ K M
 *	Kill an entire model
 */
void nmg_km(m)
struct model *m;
{
	NMG_CK_MODEL(m);

	while( NMG_LIST_NON_EMPTY( &m->r_hd ) )
		nmg_kr( NMG_LIST_FIRST( nmgregion, &m->r_hd ) );

	if (m->ma_p) {
		FREE_MODEL_A(m->ma_p);
	}
	FREE_MODEL(m);
}

/*	N M G _ V E R T E X _ G V
 *	Associate some coordinates with a vertex
 */
void nmg_vertex_gv(v, pt)
struct vertex *v;
pointp_t	pt;
{
	struct vertex_g *vg;

	NMG_CK_VERTEX(v);

	if (v->vg_p) {
		NMG_CK_VERTEX_G(v->vg_p);
	}
	else {
		GET_VERTEX_G(vg);

		vg->magic = NMG_VERTEX_G_MAGIC;
		v->vg_p = vg;
	}
	VMOVE( vg->coord, pt );
}

/*	N M G _ L O O P _ G
 *	Build the bounding box for a loop
 */
void nmg_loop_g(l)
struct loop *l;
{
	struct edgeuse	*eu;
	struct vertex_g	*vg;
	struct loop_g	*lg;
	long		magic1;

	NMG_CK_LOOPUSE(l->lu_p);

	if (lg = l->lg_p) {
		NMG_CK_LOOP_G(lg);
	} else {
		GET_LOOP_G(l->lg_p);
		lg = l->lg_p;
		lg->magic = NMG_LOOP_G_MAGIC;
	}
	VSETALL( lg->max_pt, -INFINITY );
	VSETALL( lg->min_pt,  INFINITY );

	magic1 = NMG_LIST_FIRST_MAGIC( &l->lu_p->down_hd );
	if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( NMG_LIST( eu, edgeuse, &l->lu_p->down_hd ) )  {
			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);
			VMINMAX(lg->min_pt, lg->max_pt, vg->coord);
		}
	} else if (magic1 == NMG_VERTEXUSE_MAGIC) {
		struct vertexuse	*vu;
		vu = NMG_LIST_FIRST(vertexuse, &l->lu_p->down_hd);
		NMG_CK_VERTEXUSE(vu);
		NMG_CK_VERTEX(vu->v_p);
		vg = vu->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		VMOVE(lg->min_pt, vg->coord);
		VMOVE(lg->max_pt, vg->coord);
	} else
		rt_bomb("nmg_loop_g() loopuse has bad child\n");
}

/*	N M G _ F A C E _ G
 *	Assign plane equation to face and compute bounding box
 */
void nmg_face_g(fu, p)
struct faceuse *fu;
plane_t p;
{
	int i;
	struct face_g *fg;

	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);

	fu->orientation = OT_SAME;
	fu->fumate_p->orientation = OT_OPPOSITE;

	fg = fu->f_p->fg_p;
	if (fg) {
		NMG_CK_FACE_G(fu->f_p->fg_p);
	} else {
		GET_FACE_G(fu->f_p->fg_p);
		fg = fu->f_p->fg_p;
		fg->magic = NMG_FACE_G_MAGIC;
	}

	for (i=0 ; i < ELEMENTS_PER_PLANE ; i++)
		fg->N[i] = p[i];

	nmg_face_bb(fu->f_p);
}

/*	N M G _ F A C E _ B B
 *	Build the bounding box for a face
 */
void nmg_face_bb(f)
struct face *f;
{
	struct face_g	*fg;
	struct loopuse	*lu;
	struct faceuse	*fu;

	NMG_CK_FACE(f);

	if (f->fg_p ) {
		NMG_CK_FACE_G(f->fg_p);
	}
	else {
		GET_FACE_G(f->fg_p);
		f->fg_p->magic = NMG_FACE_G_MAGIC;
	}

	fg = f->fg_p;
	fg->max_pt[X] = fg->max_pt[Y] = fg->max_pt[Z] = -MAX_FASTF;
	fg->min_pt[X] = fg->min_pt[Y] = fg->min_pt[Z] = MAX_FASTF;

	/* we compute the extent of the face by looking at the extent of
	 * each of the loop children.
	 */
	fu = f->fu_p;
	for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
		nmg_loop_g(lu->l_p);

		if (lu->orientation != OT_BOOLPLACE) {
			VMIN(fg->min_pt, lu->l_p->lg_p->min_pt);
			VMAX(fg->max_pt, lu->l_p->lg_p->max_pt);
		}
	}
}

/*	N M G _ S H E L L _ A
 *	Build the bounding box for a shell
 */
void nmg_shell_a(s)
struct shell *s;
{
	struct shell_a *sa;
	struct vertex_g *vg;
	struct face_g *fg;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	NMG_CK_SHELL(s);

	if (s->sa_p) {
		NMG_CK_SHELL_A(s->sa_p);
	} else {
		GET_SHELL_A(s->sa_p);
		s->sa_p->magic = NMG_SHELL_A_MAGIC;
	}
	sa = s->sa_p;

	/* */
	for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
		if (!fu->f_p->fg_p)
			nmg_face_bb(fu->f_p);

		fg = fu->f_p->fg_p;
		VMIN(sa->min_pt, fg->min_pt);
		VMAX(sa->max_pt, fg->max_pt);

		/* If next faceuse shares this face, skip it */
		if( NMG_LIST_MORE(fu, faceuse, &fu->l) &&
		    ( NMG_LIST_NEXT(faceuse, &fu->l)->f_p == fu->f_p ) )  {
			fu = NMG_LIST_PNEXT(faceuse,fu);
		}
	}
	for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
		if (!lu->l_p->lg_p)
			nmg_loop_g(lu->l_p);

		VMIN(sa->min_pt, lu->l_p->lg_p->min_pt);
		VMAX(sa->max_pt, lu->l_p->lg_p->max_pt);
	}
	for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
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


	if( NMG_LIST_IS_EMPTY( &s->fu_hd ) &&
	    NMG_LIST_IS_EMPTY( &s->lu_hd ) &&
	    NMG_LIST_IS_EMPTY( &s->eu_hd ) && !s->vu_p )  {
		rt_log("At %d in %s shell has no children\n",
		    __LINE__, __FILE__);
		rt_bomb("nmg_shell_a\n");
	}
}

/*	N M G _ R E G I O N _ A
 *
 *	build attributes/extents for all shells in a region
 */
void nmg_region_a(r)
struct nmgregion *r;
{
	register struct shell	*s;
	struct nmgregion_a	*ra;

	NMG_CK_REGION(r);
	if( r->ra_p )  {
		ra = r->ra_p;
		NMG_CK_REGION_A(ra);
	} else {
		GET_REGION_A(ra);
		r->ra_p = ra;
	}

	ra->magic = NMG_REGION_A_MAGIC;

	VSETALL(ra->max_pt, -MAX_FASTF);
	VSETALL(ra->min_pt, MAX_FASTF);

	for( NMG_LIST( s, shell, &r->s_hd ) )  {
		nmg_shell_a(s);
		NMG_CK_SHELL_A(s->sa_p);
		VMIN(ra->min_pt, s->sa_p->min_pt);
		VMAX(ra->max_pt, s->sa_p->max_pt);
	}
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
 *	In either case, the new edge will exist as the "next" edge after
 *	the edge passed as a parameter.
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
struct edgeuse *nmg_eusplit(v, oldeu)
struct vertex *v;
struct edgeuse *oldeu;
{
	struct edgeuse	*eu1,
			*eu2,
			*oldeumate;
	struct shell *s;

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
		if( NMG_LIST_PNEXT(edgeuse, oldeu) != oldeumate )  {
			NMG_LIST_DEQUEUE( &oldeumate->l );
			NMG_LIST_APPEND( &oldeu->l, &oldeumate->l );
		}
		NMG_LIST_DEQUEUE( &eu1->l );
		NMG_LIST_DEQUEUE( &eu2->l );
		NMG_LIST_APPEND( &oldeumate->l, &eu1->l );
		NMG_LIST_APPEND( &eu1->l, &eu2->l );

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
		rt_log("in %s at %d invalid edgeuse parent\n",
			__FILE__, __LINE__);
		rt_bomb("nmg_eusplit");
	}

	/* now we know we are in a loop */

	/* get a parent shell pointer so we can make a new edge */
	if (*oldeu->up.lu_p->up.magic_p == NMG_SHELL_MAGIC)
		s = oldeu->up.lu_p->up.s_p;
	else
		s = oldeu->up.lu_p->up.fu_p->s_p;
	NMG_CK_SHELL(s);

	/* make a new edge on the vertex */
	if (v) {
		/* An edge on the single vertex "V" */
		eu1 = nmg_me(v, v, s);
		eu2 = eu1->eumate_p;
	} else {
		/* An edge between two new vertices */
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
	NMG_LIST_DEQUEUE( &eu1->l );
	NMG_LIST_DEQUEUE( &eu2->l );
	NMG_LIST_APPEND( &oldeu->l, &eu1->l );
	NMG_LIST_APPEND( &oldeumate->l, &eu2->l );

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
 *			N M G _ M O V E E U
 *
 *	Move a pair of edgeuses onto a new edge (glue edgeuse).
 *	the edgeuse eusrc and its mate are moved to the edge
 *	used by eudst.  eusrc is made to be immediately radial to eudst.
 *	if eusrc does not share the same vertices as eudst, we bomb.
 */
void nmg_moveeu(eudst, eusrc)
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
		/* this is the only use of the eusrc edge */
		if (e->eg_p) FREE_EDGE_G(e->eg_p);
		FREE_EDGE(e);
	}

	eusrc->radial_p = eudst;
	eusrc_mate->radial_p = eudst->radial_p;

	eudst->radial_p->radial_p = eusrc_mate;
	eudst->radial_p = eusrc;
}

/*	N M G _ U N G L U E 
 *
 *	If edgeuse is part of a shared edge (more than one pair of edgeuses
 *	on the edge), it and its mate are "unglued" from the edge, and 
 *	associated with a new edge structure.
 */
void nmg_unglueedge(eu)
struct edgeuse *eu;
{
#if UNGLUE_MAKES_VERTICES
	struct vertex *v1, *v2;
	struct vertex_g *vg1, *vg2;
#endif
	struct edge *e;

	NMG_CK_EDGEUSE(eu);

	/* if we're already a single edge, just return */
	if (eu->radial_p == eu->eumate_p)
		return;

	GET_EDGE(e);

	e->magic = NMG_EDGE_MAGIC;
	e->eg_p = (struct edge_g *)NULL;
	e->eu_p = eu;
#if UNGLUE_MAKES_VERTICES
	GET_VERTEX(v1);
	GET_VERTEX(v2);
	GET_VERTEX_G(vg1);
	GET_VERTEX_G(vg2);

	/* we want a pair of new vertices that are identical to the old
	 * ones for the newly separated edge.
	 */
	v1->vu_p = v2->vu_p = (struct vertexuse *)NULL;
	v1->magic = v2->magic = NMG_VERTEX_MAGIC;

	/* if there was vertex geometry, copy it */
	if (eu->vu_p->v_p->vg_p) {
		bcopy((char *)eu->vu_p->v_p->vg_p, (char *)vg1,
		    sizeof(struct vertex_g));
		v1->vg_p = vg1;
	} else {
		v1->vg_p = (struct vertex_g *)NULL;
		FREE_VERTEX_G(vg1);
	}

	if (eu->eumate_p->vu_p->v_p->vg_p) {
		bcopy((char *)eu->eumate_p->vu_p->v_p->vg_p, (char *)vg2,
		    sizeof(struct vertex_g));
		v2->vg_p = vg2;
	} else {
		v2->vg_p = (struct vertex_g *)NULL;
		FREE_VERTEX_G(vg2);
	}

	/* now move the vertexuses to the new (but identical) verteces. */
	nmg_movevu(eu->vu_p, v1);
	nmg_movevu(eu->eumate_p->vu_p, v1);
#endif

	/* make sure the edge isn't pointing at this edgeuse */
	if (eu->e_p->eu_p == eu || eu->e_p->eu_p == eu->eumate_p ) {
		eu->e_p->eu_p = eu->e_p->eu_p->radial_p;
	}

	/* unlink edgeuses from old edge */
	eu->radial_p->radial_p = eu->eumate_p->radial_p;
	eu->eumate_p->radial_p->radial_p = eu->radial_p;
	eu->eumate_p->radial_p = eu;
	eu->radial_p = eu->eumate_p;

	eu->eumate_p->e_p = eu->e_p = e;

}

/*
 *			N M G _ J V
 *
 *	Join two vertexes into one.
 *	v1 inherits all the vertexuses presently pointing to v2,
 *	and v2 is then destroyed.
 */
void nmg_jv(v1, v2)
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
	vu = NMG_LIST_FIRST(vertexuse, &v2->vu_hd );
	while( NMG_LIST_MORE( vu, vertexuse, &v2->vu_hd ) )  {
		register struct vertexuse	*vunext;

		NMG_CK_VERTEXUSE(vu);
		vunext = NMG_LIST_PNEXT(vertexuse, vu);
		NMG_LIST_DEQUEUE( &vu->l );
		NMG_LIST_INSERT( &v1->vu_hd, &vu->l );
		vu->v_p = v1;		/* "up" to new vertex */
		vu = vunext;
	}

	if (v2->vg_p) FREE_VERTEX_G(v2->vg_p);
	FREE_VERTEX(v2);
}

/*	N M G _ T B L
 *	maintain a table of pointers (to magic numbers/structs)
 */
int nmg_tbl(b, func, p)
struct nmg_ptbl *b;
int func;
long *p;
{
	if (func == TBL_INIT) {
		b->buffer = (long **)rt_calloc(b->blen=64,
						sizeof(p), "pointer table");
		b->end = 0;
		return(0);
	} else if (func == TBL_RST) {
		b->end = 0;
		return(0);
	} else if (func == TBL_INS) {
		register int i;
		union {
			struct loopuse *lu;
			struct edgeuse *eu;
			struct vertexuse *vu;
			long *l;
		} pp;

		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl Inserting %8x\n", p);

		pp.l = p;

		if (b->blen == 0) (void)nmg_tbl(b, TBL_INIT, p);
		if (b->end >= b->blen)
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
			    sizeof(p)*(b->blen += 64),
			    "pointer table" );

		b->buffer[i=b->end++] = p;
		return(i);
	} else if (func == TBL_LOC) {
		/* we do this a great deal, so make it go as fast as possible.
		 * this is the biggest argument I can make for changing to an
		 * ordered list.  Someday....
		 */
		register int k= -1, end = b->end;
		register long **pp = b->buffer;

		while (++k < end)
			if (pp[k] == p) return(k);

		return(-1);
	} else if (func == TBL_RM) {
		/* we go backwards down the list looking for occurrences
		 * of p to delete.  We do it backwards to reduce the amount
		 * of data moved when there is more than one occurrence of p
		 * in the list.  A pittance savings, unless you're doing a
		 * lot of it.
		 */
		register int end = b->end, j, k, l;
		register long **pp = b->buffer;

		for (l = b->end-1 ; l >= 0 ; --l)
			if (pp[l] == p){
				/* delete occurrence(s) of p */

				j=l+1;
				while (pp[l-1] == p) --l;

				end -= j - l;
				for(k=l ; j < end ;)
					b->buffer[k++] = b->buffer[j++];
			}

		b->end = end;
		return(0);
	} else if (func == TBL_CAT) {
		union {
			long *l;
			struct nmg_ptbl *t;
		} d;

		d.l = p;

		if ((b->blen - b->end) < d.t->end) {
			
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
				sizeof(p)*(b->blen += d.t->blen),
				"pointer table (CAT)");
		}
		bcopy(d.t->buffer, &b->buffer[b->end], d.t->end*sizeof(p));
		return(0);
	} else if (func == TBL_FREE) {
		bzero((char *)b->buffer, b->blen * sizeof(p));
		rt_free((char *)b->buffer, "pointer table");
		bzero(b, sizeof(struct nmg_ptbl));
		return (0);
	} else {
		rt_log("Unknown table function %d\n", func);
		rt_bomb("nmg_tbl");
	}
	return(-1);/* this is here to keep lint happy */
}


/*
 *			N M G _ M L V
 *
 *	Make a new loop (with specified orientation) and vertex,
 *	in a shell or face.
 *	If the vertex 'v' is NULL, the shell's lone vertex is used,
 *	or a new vertex is created.
 * 
 *  Implicit return -
 *	The loopuse is inserted at the head of the appropriate list.
 */
struct loopuse *nmg_mlv(magic, v, orientation)
long *magic;
struct vertex *v;
char orientation;
{
	struct loop *l;
	struct loopuse *lu1, *lu2;
	union {
		struct shell *s;
		struct faceuse *fu;
		long *magic_p;
	} p;

	p.magic_p = magic;

	if (v) {
		NMG_CK_VERTEX(v);
	}

	GET_LOOP(l);
	l->magic = NMG_LOOP_MAGIC;
	l->lg_p = (struct loop_g *)NULL;

	GET_LOOPUSE(lu1);
	lu1->l.magic = NMG_LOOPUSE_MAGIC;
	NMG_LIST_INIT( &lu1->down_hd );

	GET_LOOPUSE(lu2);
	lu2->l.magic = NMG_LOOPUSE_MAGIC;
	NMG_LIST_INIT( &lu2->down_hd );

	l->lu_p = lu1;

	lu1->l_p = lu2->l_p = l;
	lu1->lua_p = lu2->lua_p = (struct loopuse_a *)NULL;
	if (*p.magic_p == NMG_SHELL_MAGIC) {
		struct shell		*s;
		struct vertexuse	*vu1, *vu2;

		s = p.s;
		if ( vu1 = s->vu_p ) {
			/* Use shell's lone vertex */
			s->vu_p = (struct vertexuse *)NULL;
			vu1->up.lu_p = lu1;
			if (v) nmg_movevu(vu1, v);
		}
		else {
			if (v) vu1 = nmg_mvu(v, &lu1->l.magic);
			else vu1 = nmg_mvvu(&lu1->l.magic);
		}
		NMG_CK_VERTEXUSE(vu1);
		NMG_LIST_SET_DOWN_TO_VERT(&lu1->down_hd, vu1);

		vu2 = nmg_mvu(vu1->v_p, &lu2->l.magic);
		NMG_CK_VERTEXUSE(vu2);
		NMG_LIST_SET_DOWN_TO_VERT(&lu2->down_hd, vu2);
		lu1->up.s_p = lu2->up.s_p = s;

		lu1->lumate_p = lu2;
		lu2->lumate_p = lu1;

		NMG_LIST_INSERT( &s->lu_hd, &lu1->l );
		NMG_LIST_INSERT( &lu1->l, &lu2->l );
	} else if (*p.magic_p == NMG_FACEUSE_MAGIC) {
		struct vertexuse	*vu1, *vu2;

		if (v) vu1 = nmg_mvu(v, &lu1->l.magic);
		else vu1 = nmg_mvvu(&lu1->l.magic);
		NMG_LIST_SET_DOWN_TO_VERT(&lu1->down_hd, vu1);
		/* vu1->up.lu_p = lu1; done by nmg_mvu/nmg_mvvu */

		vu2 = nmg_mvu(vu1->v_p, &lu2->l.magic);
		NMG_LIST_SET_DOWN_TO_VERT(&lu2->down_hd, vu2);
		/* vu2->up.lu_p = lu2; done by nmg_mvu() */

		lu1->up.fu_p = p.fu;
		lu2->up.fu_p = p.fu->fumate_p;

		lu1->lumate_p = lu2;
		lu2->lumate_p = lu1;

		NMG_LIST_INSERT( &p.fu->fumate_p->lu_hd, &lu2->l );
		NMG_LIST_INSERT( &p.fu->lu_hd, &lu1->l );
	} else {
		rt_bomb("nmg_mlv() unknown parent for loopuse!\n");
	}
	lu1->orientation = lu2->orientation = orientation;

	return(lu1);
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
		rt_log("in %s at %d Cannot move loop to face in another shell\n",
		    __FILE__, __LINE__);
	}
	lu1 = NMG_LIST_FIRST(loopuse, &s->lu_hd);
	NMG_CK_LOOPUSE(lu1);
	NMG_LIST_DEQUEUE( &lu1->l );

	lu2 = NMG_LIST_FIRST(loopuse, &s->lu_hd);
	NMG_CK_LOOPUSE(lu2);
	NMG_LIST_DEQUEUE( &lu2->l );

	NMG_LIST_APPEND( &fu->lu_hd, &lu1->l );
	NMG_LIST_APPEND( &fu->fumate_p->lu_hd, &lu2->l );
}

/*
 *			N M G _ C F A C E
 *
 *	Create a face from a list of vertices
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
 *	The vertices should be listed in "clockwise" order if this is
 *	an ordinary face, and in "counterclockwise" order if this is
 *	an interior ("hole" or "subtracted") face.
 *	See the comments in nmg_cmface() for more details.
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.
 *	Therefore, the callers vertices are inserted in reverse order,
 *	to effect the proper vertex order in the final face loop.
 */
struct faceuse *nmg_cface(s, verts, n)
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
		lu = nmg_mlv(&s->l.magic, verts[n-1], OT_SAME);
		fu = nmg_mf(lu);
		vu = NMG_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);

		if (!verts[n-1])
			verts[n-1] = eu->vu_p->v_p;

		for (i = n-2 ; i >= 0 ; i--) {
			eu = NMG_LIST_FIRST(edgeuse, &lu->down_hd);
			eu = nmg_eusplit(verts[i], eu);
			if (!verts[i])
				verts[i] = eu->vu_p->v_p;
		}

	} else {
		lu = nmg_mlv(&s->l.magic, (struct vertex *)NULL, OT_SAME);
		fu = nmg_mf(lu);
		vu = NMG_LIST_FIRST(vertexuse, &lu->down_hd);
		eu = nmg_meonvu(vu);
		while (--n) {
			(void)nmg_eusplit((struct vertex *)NULL, eu);
		}
	}
	return (fu);
}

/*
 *			N M G _ E S P L I T
 *
 *	Split an edge.
 *
 *	Actually, we split each edgeuse pair of the given edge, and combine
 *	the new edgeuses together onto new edges.  
 *
 *	Explicit return:
 *		pointer to the new edge which took the place of the parameter
 *	edge.
 */
struct edge *nmg_esplit(v, e)
struct vertex *v;
struct edge *e;
{
	struct edgeuse	*eu,	/* placeholder edgeuse */
			*eur,	/* radial edgeuse of placeholder */
			*eu2,	/* new edgeuse (next of eur) */
			*neu1, *neu2; /* new (split) edgeuses */
	int 		notdone=1;
	struct vertex	*v1, *v2;

	eu = e->eu_p;
	neu1 = neu2 = (struct edgeuse *)NULL;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	v1 = eu->vu_p->v_p;

	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	v2 = eu->eumate_p->vu_p->v_p;

	/* one at a time, we peel out & split an edgeuse pair of this edge.
	 * when we split an edge that didn't need to be peeled out, we know
	 * we've split the last edge
	 */
	do {
		eur = eu->radial_p;
		eu2 = nmg_eusplit(v, eur);
		NMG_CK_EDGEUSE(eur);
		NMG_CK_EDGEUSE(eu2);
		NMG_TEST_EDGEUSE(eur);
		NMG_TEST_EDGEUSE(eu2);
		
		if (!v) v = eu2->vu_p->v_p;

		if (eu2->e_p == e || eur->e_p == e) notdone = 0;

		
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
			rt_log("in %s at %d ", __FILE__, __LINE__);
			rt_bomb("nmg_esplit() Something's awry\n");
		}
	} while (notdone);

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


	return(eu->e_p);
}

/*
 *				N M G _ E I N S
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
struct edgeuse *nmg_eins(eu)
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
		NMG_LIST_DEQUEUE( &eu1->l );
		NMG_LIST_DEQUEUE( &eu2->l );

		NMG_LIST_INSERT( &eu->l, &eu1->l );
		NMG_LIST_APPEND( &eumate->l, &eu2->l );

		eu1->up.lu_p = eu->up.lu_p;
		eu2->up.lu_p = eumate->up.lu_p;
	}
	else {
		rt_bomb("nmg_eins() Cannot yet insert null edge in shell\n");
	}
	return(eu1);
}

/*
 *			F I N D _ V U _ I N _ F A C E
 *
 *	try to find a vertex(use) in a face wich appoximately matches the
 *	coordinates given.  
 *	
 */
struct vertexuse *nmg_find_vu_in_face(pt, fu, tol)
point_t		pt;
struct faceuse	*fu;
fastf_t		tol;
{
	register struct loopuse	*lu;
	struct edgeuse		*eu;
	vect_t			delta;
	register pointp_t	pp;
	int			magic1;

	NMG_CK_FACEUSE(fu);

	for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		magic1 = NMG_LIST_FIRST_MAGIC( &lu->down_hd );
		if (magic1 == NMG_VERTEXUSE_MAGIC) {
			struct vertexuse	*vu;
			vu = NMG_LIST_FIRST(vertexuse, &lu->down_hd);
			pp = vu->v_p->vg_p->coord;
			VSUB2(delta, pp, pt);
			if ( MAGSQ(delta) < tol)
				return(vu);
		}
		else if (magic1 == NMG_EDGEUSE_MAGIC) {
			for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
				pp = eu->vu_p->v_p->vg_p->coord;
				VSUB2(delta, pp, pt);
				if ( MAGSQ(delta) < tol)
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
			rt_log("in %s at %d faceuses don't share parent\n",
				__FILE__, __LINE__);
			rt_bomb("nmg_gluefaces\n");
		}
		lu = NMG_LIST_FIRST( loopuse, &fu->lu_hd );
		NMG_CK_LOOPUSE(lu);
		if( NMG_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC) {
			/* Not an edgeuse, probably a vertexuse */
			rt_bomb("nmg_cluefaces() Cannot glue edges of face on vertex\n");
		} else {
			eu = NMG_LIST_FIRST( edgeuse, &lu->down_hd );
			NMG_CK_EDGEUSE(eu);
		}
	}

	for (i=0 ; i < n ; ++i) {
		lu = NMG_LIST_FIRST( loopuse, &fulist[i]->lu_hd );
		for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
			for( f_no = i+1; f_no < n; f_no++ )  {
				struct loopuse		*lu2;
				register struct edgeuse	*eu2;

				if( eu->radial_p != eu->eumate_p )  break;

				lu2 = NMG_LIST_FIRST(loopuse,
					&fulist[f_no]->lu_hd);
				for( NMG_LIST( eu2, edgeuse, &lu2->down_hd ) )  {
					if (EDGESADJ(eu, eu2))
					    	nmg_moveeu(eu, eu2);
				}
			}
		}
	}
}

/*
 *			N M G _ V U _ T O _ V L I S T
 *
 *  Plot a single vertexuse
 */
nmg_vu_to_vlist( vhead, vu )
struct vlhead		*vhead;
struct vertexuse	*vu;
{
	struct vertex	*v;
	register struct vertex_g *vg;

	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	if( vg )  {
		/* Only thing in this shell is a point */
		NMG_CK_VERTEX_G(vg);
		ADD_VL( vhead, vg->coord, 0 );
		ADD_VL( vhead, vg->coord, 1 );
	}
}

/*
 *			N M G _ E U _ T O _ V L I S T
 *
 *  Plot a list of edgeuses.  The last edge is joined back to the first.
 */
nmg_eu_to_vlist( vhead, eu_hd )
struct vlhead	*vhead;
struct nmg_list	*eu_hd;
{
	struct edgeuse	*eu;
	struct vertexuse *vu;
	struct vertex	*v;
	register struct vertex_g *vg;
	struct vertex_g	*first_vg;

	/* Consider all the edges in the wire edge list */
	first_vg = (struct vertex_g *)0;
	for( NMG_LIST( eu, edgeuse, eu_hd ) )  {
		/* Consider this edge */
		NMG_CK_EDGEUSE(eu);
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE(vu);
		v = vu->v_p;
		NMG_CK_VERTEX(v);
		vg = v->vg_p;
		if( !vg ) {
			continue;
		}
		NMG_CK_VERTEX_G(vg);
		if( !first_vg )  {
			/* move */
			ADD_VL( vhead, vg->coord, 0 );
			first_vg = vg;
		} else {
			/* Draw */
			ADD_VL( vhead, vg->coord, 1 );
		}
	}
	if( first_vg )  {
		/* Draw back to first vertex used */
		ADD_VL( vhead, first_vg->coord, 1 );	/* draw */
	}
}

/*
 *			N M G _ L U _ T O _ V L I S T
 *
 *  Plot a list of loopuses.
 */
nmg_lu_to_vlist( vhead, lu_hd, poly_markers, normal )
struct vlhead	*vhead;
struct nmg_list	*lu_hd;
int		poly_markers;
vectp_t		normal;
{
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse *vu;
	struct vertex	*v;
	register struct vertex_g *vg;

	for( NMG_LIST( lu, loopuse, lu_hd ) )  {
		int		isfirst;
		struct vertex_g	*first_vg;
		point_t		centroid;
		int		npoints;

		/* Consider this loop */
		NMG_CK_LOOPUSE(lu);
		if( NMG_LIST_FIRST_MAGIC(&lu->down_hd)==NMG_VERTEXUSE_MAGIC )  {
			/* loop of a single vertex */
			vu = NMG_LIST_FIRST(vertexuse, &lu->down_hd);
			nmg_vu_to_vlist( vhead, vu );
			continue;
		}
		/* Consider all the edges in the loop */
		isfirst = 1;
		first_vg = (struct vertex_g *)0;
		npoints = 0;
		VSETALL( centroid, 0 );
		for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
			/* Consider this edge */
			NMG_CK_EDGEUSE(eu);
			vu = eu->vu_p;
			NMG_CK_VERTEXUSE(vu);
			v = vu->v_p;
			NMG_CK_VERTEX(v);
			vg = v->vg_p;
			if( !vg ) {
				continue;
			}
			NMG_CK_VERTEX_G(vg);
			VADD2( centroid, centroid, vg->coord );
			npoints++;
			if (isfirst) {
				if( poly_markers) {
					/* Insert a "start polygon, normal" marker */
					ADD_VL( vhead, normal, 2 );
					ADD_VL( vhead, vg->coord, 3 );	/* move */
				} else {
					/* move */
					ADD_VL( vhead, vg->coord, 0 );
				}
				isfirst = 0;
				first_vg = vg;
			} else {
				if( poly_markers) {
					ADD_VL( vhead, vg->coord, 4 );
				} else {
					/* Draw */
					ADD_VL( vhead, vg->coord, 1 );
				}
			}
		}

		/* Draw back to the first vertex used */
		if( !isfirst && first_vg )  {
			if( poly_markers )  {
				/* Draw, end polygon */
				ADD_VL( vhead, first_vg->coord, 5 );
			} else {
				/* Draw */
				ADD_VL( vhead, first_vg->coord, 1 );
			}
		}
		if( poly_markers > 1 && npoints > 2 )  {
			/* Draw surface normal as a little vector */
			double	f;
			vect_t	tocent;
			f = 1.0 / npoints;
			VSCALE( centroid, centroid, f );
			ADD_VL( vhead, centroid, 0 );	/* move */
			VSUB2( tocent, first_vg->coord, centroid );
			f = MAGNITUDE( tocent ) * 0.5;
			VSCALE( tocent, normal, f );
			VADD2( centroid, centroid, tocent );
			ADD_VL( vhead, centroid, 1 );	/* draw */
		}
	}
}

/*
 *			N M G _ S _ T O _ V L I S T
 *
 *  Plot the entire contents of a shell.
 *
 *  poly_markers =
 *	0 for vectors
 *	1 for polygons
 *	2 for polygons and surface normals drawn with vectors
 */
void
nmg_s_to_vlist( vhead, s, poly_markers )
struct vlhead	*vhead;
struct shell	*s;
int		poly_markers;
{
	struct faceuse	*fu;
	struct face_g	*fg;
	vect_t		normal;

	NMG_CK_SHELL(s);

	/* faces */
	for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
		/* Consider this face */
		NMG_CK_FACEUSE(fu);
		NMG_CK_FACE(fu->f_p);
		fg = fu->f_p->fg_p;
		NMG_CK_FACE_G(fg);
		if (fu->orientation != OT_SAME)  continue;
	   	nmg_lu_to_vlist( vhead, &fu->lu_hd, poly_markers, fg->N );
	}

	/* wire loops.  poly_markers=0 so wires are always drawn as vectors */
	VSETALL(normal, 0);
	nmg_lu_to_vlist( vhead, &s->lu_hd, 0, normal );

	/* wire edges */
	nmg_eu_to_vlist( vhead, &s->eu_hd );

	/* single vertices */
	if (s->vu_p)  {
		nmg_vu_to_vlist( vhead, s->vu_p );
	}
}

/*
 *			N M G _ R _ T O _ V L I S T
 */
void
nmg_r_to_vlist( vhead, r, poly_markers )
struct vlhead	*vhead;
struct nmgregion	*r;
int		poly_markers;
{
	register struct shell	*s;

	NMG_CK_REGION( r );
	for( NMG_LIST( s, shell, &r->s_hd ) )  {
		nmg_s_to_vlist( vhead, s, poly_markers );
	}
}

/*
 *			F I N D E U
 *
 *	find an edgeuse in a shell between a pair of verticies
 */
static struct edgeuse *findeu(v1, v2, s, eup)
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
		rt_log("looking for edge between %8x and %8x other than %8x/%8x\n",
		v1, v2, eup, eup->eumate_p);

	for( NMG_LIST( vu, vertexuse, &v1->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (!vu->up.magic_p) {
			rt_log("in %s at %d vertexuse has null parent\n",
				__FILE__, __LINE__);
			rt_bomb("findeu");
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
	    	rt_log("findeu search failed\n");

	return((struct edgeuse *)NULL);
}

/*
 *			N M G _ C M F A C E
 *
 *	Create a face for a manifold shell from a list of vertices
 *
 *	"verts" is an array of "n" pointers to pointers to (struct vertex).
 *	"s" is the parent shell for the new face.
 *	The face will consist of a single loop
 *	made from edges between the n vertices.  Before an edge is created
 *	between a pair of verticies, we check to see if there is already an
 *	edge with a single use-pair (in this shell) between the two verticies.
 *	If such an edge can be found, the newly created edge will "use-share"
 *	the existing edge.  This greatly facilitates the construction of
 *	manifold shells from a series of points/faces.
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
 *	The vertices should be listed in "clockwise" order if this is
 *	an ordinary face, and in "counterclockwise" order if this is
 *	an interior ("hole" or "subtracted") face.
 *	Note that while this routine makes only topology, without
 *	reference to geometry, by following the clockwise rule,
 *	finding the surface normal
 *	of ordinary faces can be done using the following procedure.
 *
 *
 *			C                   D
 *	                *-------------------*
 *	                |                   |
 *	                |   ^...........>   |
 *	   ^     N      |   .           .   |
 *	   |      \     |   .           .   |
 *	   |       \    |   . clockwise .   |
 *	   |C-B     \   |   .           .   |
 *	   |         \  |   .           v   |
 *	   |          \ |   <............   |
 *	               \|                   |
 *	                *-------------------*
 *	                B                   A
 *			       ----->
 *				A-B
 *
 *	If the points are given in the order A B C D (eg, clockwise),
 *	then the outward pointing surface normal N = (A-B) x (C-B).
 *	This is the "right hand rule".
 *
 *	Note that this routine inserts new vertices (by edge use splitting)
 *	at the head of the loop, which reverses the order.
 *	Therefore, the callers vertices are inserted in reverse order,
 *	to effect the proper vertex order in the final face loop.
 */
struct faceuse *nmg_cmface(s, verts, n)
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
	vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd);
	NMG_CK_VERTEXUSE(vu);
	eu = nmg_meonvu(vu);
	NMG_CK_EDGEUSE(eu);

	if (!(*verts[n-1]))  {
		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		*verts[n-1] = eu->vu_p->v_p;
	}

	for (i = n-2 ; i >= 0 ; i--) {
		lu = NMG_LIST_FIRST( loopuse, &fu->lu_hd );
		NMG_CK_LOOPUSE(lu);
		euold = NMG_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(euold);

		if (rt_g.NMG_debug & DEBUG_CMFACE)
			rt_log("euold: %8x\n", euold);

		/* look for pre-existing edge between these
		 * verticies
		 */
		if (*verts[i]) {
			/* look for an existing edge to share */
			eur = findeu(*verts[i+1], *verts[i], s, euold);
			eu = nmg_eusplit(*verts[i], euold);
			if (eur) {
				nmg_moveeu(eur, eu);

				if (rt_g.NMG_debug & DEBUG_CMFACE)
				rt_log("found another edgeuse (%8x) between %8x and %8x\n",
					eur, *verts[i+1], *verts[i]);
			}
			else {
				if (rt_g.NMG_debug & DEBUG_CMFACE)
				    rt_log("didn't find edge from verts[%d]%8x to verts[%d]%8x\n",
					i+1, *verts[i+1], i, *verts[i]);
			}
		} else {
			if (rt_g.NMG_debug & DEBUG_CMFACE)
				rt_log("*verts[%d] is null\t", i);

			eu = nmg_eusplit(*verts[i], euold);
			*verts[i] = eu->vu_p->v_p;

			if (rt_g.NMG_debug & DEBUG_CMFACE)
			rt_log("*verts[%d] is now %8x\n", i, *verts[i]);
		}
	}

	if (eur = findeu(*verts[n-1], *verts[0], s, euold))
		nmg_moveeu(eur, euold);
	else 
	    if (rt_g.NMG_debug & DEBUG_CMFACE)
		rt_log("didn't find edge from verts[%d]%8x to verts[%d]%8x\n",
			n-1, *verts[n-1], 0, *verts[0]);

	return (fu);
}

/*
 *			N M G _ V V G
 *
 *  Verify vertex geometry
 */
void nmg_vvg(vg)
struct vertex_g *vg;
{
	NMG_CK_VERTEX_G(vg);
}

/*
 *			N M G _ V V E R T E X
 *
 *  Verify a vertex
 */
void nmg_vvertex(v, vup)
struct vertex *v;
struct vertexuse *vup;
{
	struct vertexuse *vu;

	NMG_CK_VERTEX(v);

	for( NMG_LIST( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (vu->v_p != v)
			rt_bomb("nmg_vvertex() a vertexuse in my list doesn't share my vertex\n");
	}
	if (v->vg_p) nmg_vvg(v->vg_p);
}

/* Verify vertex attributes */
void nmg_vvua(vua)
struct vertexuse_a *vua;
{
	NMG_CK_VERTEXUSE_A(vua);
}

/*
 *			N M G _ V V U
 *
 *  Verify vertexuse
 */
void nmg_vvu(vu, up_magic_p)
struct vertexuse *vu;
long		*up_magic_p;
{
	NMG_CK_VERTEXUSE(vu);
	if (vu->up.magic_p != up_magic_p)
		rt_bomb("nmg_vvu() vertexuse denies parent\n");

	if (!vu->l.forw)
		rt_bomb("nmg_vvu() vertexuse has null forw pointer\n");

	if( NMG_LIST_FIRST_MAGIC( &vu->l ) != NMG_VERTEXUSE_MAGIC)
		rt_bomb("vertexuse forw is bad vertexuse\n");

	if (NMG_LIST_PNEXT_PLAST(vertexuse,vu) != vu )
		rt_bomb("vertexuse not back of next vertexuse\n");

	nmg_vvertex(vu->v_p, vu);

	if (vu->vua_p) nmg_vvua(vu->vua_p);
}

/* Verify edgeuse attributes */
void nmg_veua(eua)
struct edgeuse_a *eua;
{
	NMG_CK_EDGEUSE_A(eua);
}

/* Verify edge geometry */
void nmg_veg(eg)
struct edge_g *eg;
{
	NMG_CK_EDGE_G(eg);
}

/*
 *			N M G _ V E D G E
 *
 *  Verify edge
 */
void nmg_vedge(e, eup)
struct edge *e;
struct edgeuse *eup;
{
	struct edgeuse *eu;
	int is_use = 0;		/* flag: eup is in edge's use list */

	NMG_CK_EDGE(e);
	NMG_CK_EDGEUSE(eup);
	NMG_CK_VERTEXUSE(eup->vu_p);
	NMG_CK_VERTEX(eup->vu_p->v_p);
	NMG_CK_EDGEUSE(eup->eumate_p);
	NMG_CK_VERTEXUSE(eup->eumate_p->vu_p);
	NMG_CK_VERTEX(eup->eumate_p->vu_p->v_p);

	if (!e->eu_p) rt_bomb("edge has null edgeuse pointer\n");

	eu = eup;
	do {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGEUSE(eu->eumate_p);
		if (eu == eup || eu->eumate_p == eup)
			is_use = 1;

		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		if (eu->vu_p->v_p == eup->vu_p->v_p) {
			if (eu->eumate_p->vu_p->v_p != eup->eumate_p->vu_p->v_p)
				rt_bomb("edgeuse mate does not have correct vertex\n");
		} else if (eu->vu_p->v_p == eup->eumate_p->vu_p->v_p) {
			if (eu->eumate_p->vu_p->v_p != eup->vu_p->v_p)
				rt_bomb("edgeuse does not have correct vertex\n");
		} else
			rt_bomb("edgeuse does not share vertex endpoint\n");

		eu = eu->eumate_p->radial_p;
	} while (eu != eup);

	if (!is_use)
		rt_bomb("Cannot get from edge to parent edgeuse\n");

	if (e->eg_p) nmg_veg(e->eg_p);
}

/*
 *			N M G _ V E U
 *
 *  Verify edgeuse
 */
void nmg_veu(hp, up_magic_p)
struct nmg_list	*hp;
long	*up_magic_p;
{
	struct edgeuse	*eu;
	struct edgeuse	*eunext;
	
	for( NMG_LIST( eu, edgeuse, hp ) )  {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != up_magic_p)
			rt_bomb("edgeuse denies parentage\n");

		if (!eu->l.forw)
			rt_bomb("edgeuse has Null \"forw\" pointer\n");
		eunext = NMG_LIST_PNEXT_CIRC( edgeuse, eu );
		if (eunext->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse forw is bad edgeuse\n");
		if (NMG_LIST_PLAST_CIRC(edgeuse,eunext) != eu )  {
		    if (eunext->l.back)
			rt_bomb("next edgeuse has back that points elsewhere\n");
		    else
			rt_bomb("next edgeuse has NULL back\n");
		}
		if (eu->eumate_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse mate is bad edgeuse\n");
		else if (eu->eumate_p->eumate_p != eu)
			rt_bomb("edgeuse mate spurns edgeuse\n");

		if (eu->radial_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse radial is bad edgeuse\n");
		else if (eu->radial_p->radial_p != eu)
			rt_bomb("edgeuse radial denies knowing edgeuse\n");

		nmg_vedge(eu->e_p, eu);
		
		if (eu->eua_p) nmg_veua(eu->eua_p);

		switch (eu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: break;
		case OT_OPPOSITE: break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("unknown loopuse orintation\n");
					break;
		}

		nmg_vvu(eu->vu_p, eu);
	}
}

/*
 *			N M G _ V L G
 *
 *  Verify loop geometry
 */
void nmg_vlg(lg)
struct loop_g *lg;
{
	int i;
	
	NMG_CK_LOOP_G(lg);

	for (i=0 ; i < ELEMENTS_PER_PT ; ++i)
		if (lg->min_pt[i] > lg->max_pt[i])
			rt_bomb("loop geom min_pt greater than max_pt\n");
}

/*
 *			N M G _ V L O O P
 *
 *  Verify loop
 */
void nmg_vloop(l, lup)
struct loop *l;
struct loopuse *lup;
{
	struct loopuse *lu;

	NMG_CK_LOOP(l);
	NMG_CK_LOOPUSE(lup);

	if (!l->lu_p) rt_bomb("loop has null loopuse pointer\n");

#if 0
	for (lu=lup ; lu && lu != l->lu_p && lu->next != lup ; lu = lu->next);
	
	if (l->lu_p != lu)
		for (lu=lup->lumate_p ; lu && lu != l->lu_p && lu->next != lup->lumate_p ; lu = lu->next);

	if (l->lu_p != lu) rt_bomb("can't get to parent loopuse from loop\n");
#endif

	if (l->lg_p) nmg_vlg(l->lg_p);
}

/* Verify loop attributes */
void nmg_vlua(lua)
struct loopuse_a *lua;
{
	NMG_CK_LOOPUSE_A(lua);
}

/*
 *			N M G _ V L U
 *
 *  Verify loopuse
 */
void nmg_vlu(hp, up)
struct nmg_list	*hp;
{
	struct loopuse *lu;

	for( NMG_LIST( lu, loopuse, hp ) )  {
		NMG_CK_LOOPUSE(lu);

		if (*lu->up.magic_p != hp->magic)
			rt_bomb("loopuse denies parentage\n");

		if (!lu->l.forw)
			rt_bomb("loopuse has null forw pointer\n");
		else if (NMG_LIST_PNEXT_PLAST(loopuse,lu) != lu )
			rt_bomb("forw loopuse has back pointing somewhere else\n");

		if (!lu->lumate_p)
			rt_bomb("loopuse has null mate pointer\n");

		if (lu->lumate_p->l.magic != NMG_LOOPUSE_MAGIC)
			rt_bomb("loopuse mate is bad loopuse\n");

		if (lu->lumate_p->lumate_p != lu)
			rt_bomb("lumate spurns loopuse\n");

		switch (lu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: break;
		case OT_OPPOSITE	: break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("unknown loopuse orintation\n");
					break;
		}
		if (lu->lumate_p->orientation != lu->orientation)
			rt_bomb("loopuse and mate have different orientation\n");

		if (!lu->l_p)
			rt_bomb("loopuse has Null loop pointer\n");
		else {
			nmg_vloop(lu->l_p, lu);
		}

		if (lu->lua_p) nmg_vlua(lu->lua_p);

		if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC)
			nmg_veu( &lu->down_hd, lu);
		else if (NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
			nmg_vvu(NMG_LIST_FIRST(vertexuse,&lu->down_hd), lu);
		else
			rt_bomb("nmg_vlu bad magic\n");
	}
}

/*
 *			N M G _ V F G
 *
 *  Verify face geometry
 */
void nmg_vfg(fg)
struct face_g *fg;
{
	int i;

	NMG_CK_FACE_G(fg);

	for (i=0 ; i < ELEMENTS_PER_PT ; ++i)
		if (fg->min_pt[i] > fg->max_pt[i])
			rt_bomb("face geom min_pt greater than max_pt\n");

	if (fg->N[X]==0.0 && fg->N[Y]==0.0 && fg->N[Z]==0.0 &&
	    fg->N[H]!=0.0) {
		rt_log("bad NMG plane equation %fX + %fY + %fZ = %f\n",
			fg->N[X], fg->N[Y], fg->N[Z], fg->N[H]);
		rt_bomb("Bad NMG geometry\n");
	}
}

/*
 *			N M G _ V F A C E
 *
 *  Verify face
 */
void nmg_vface(f, fup)
struct face *f;
struct faceuse *fup;
{
	struct faceuse *fu;

	NMG_CK_FACE(f);
	NMG_CK_FACEUSE(fup);

	/* make sure we can get back to the parent faceuse from the face */
	if (!f->fu_p) rt_bomb("face has null faceuse pointer\n");

#if 0
	for (fu = fup; fu && fu != f->fu_p && fu->forw != fup; fu = fu->forw);

	if (f->fu_p != fu) rt_bomb("can't get to parent faceuse from face\n");
#endif
	
	if (f->fg_p) nmg_vfg(f->fg_p);
}

/* Verify faceuse attributes */
void nmg_vfua(fua)
struct faceuse_a *fua;
{
	NMG_CK_FACEUSE_A(fua);
}

/*	N M G _ V F U
 *
 *	Validate a list of faceuses
 */
void nmg_vfu(hp, s)
struct nmg_list	*hp;
struct shell *s;
{
	struct faceuse *fu;

	for( NMG_LIST( fu, faceuse, hp ) )  {
		NMG_CK_FACEUSE(fu);
		if (fu->s_p != s) {
			rt_log("faceuse claims shell parent (%8x) instead of (%8x)\n",
				fu->s_p, s);
			rt_bomb("nmg_vfu\n");
		}

		if (!fu->l.forw) {
			rt_bomb("faceuse forw is NULL\n");
		} else if (fu->l.forw->back != (struct nmg_list *)fu) {
			rt_bomb("faceuse->forw->back != faceuse\n");
		}

		if (!fu->fumate_p)
			rt_bomb("null faceuse fumate_p pointer\n");

		if (fu->fumate_p->l.magic != NMG_FACEUSE_MAGIC)
			rt_bomb("faceuse mate is bad faceuse ptr\n");

		if (fu->fumate_p->fumate_p != fu)
			rt_bomb("faceuse mate spurns faceuse!\n");

		switch (fu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: if (fu->fumate_p->orientation != OT_OPPOSITE)
					rt_bomb("faceuse of \"SAME\" orientation has mate that is not \"OPPOSITE\" orientation");
				break;
		case OT_OPPOSITE:  if (fu->fumate_p->orientation != OT_SAME)
					rt_bomb("faceuse of \"OPPOSITE\" orientation has mate that is not \"SAME\" orientation");
				break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("unknown faceuse orintation\n"); break;
		}

		if (fu->fua_p) nmg_vfua(fu->fua_p);
		
		NMG_CK_FACE(fu->f_p);
		nmg_vface(fu->f_p, fu);
		
		nmg_vlu( &fu->lu_hd, fu);
	}
}


/*	N M G _ S H E L L
 *
 *	validate a list of shells and all elements under them
 */
void nmg_vshell(hp, r)
struct nmg_list	*hp;
struct nmgregion *r;
{
	struct shell *s;
	pointp_t lpt, hpt;

	for( NMG_LIST( s, shell, hp ) )  {
		NMG_CK_SHELL(s);
		if (s->r_p != r) {
			rt_log("shell's r_p (%8x) doesn't point to parent (%8x)\n",
				s->r_p, r);
			rt_bomb("nmg_vshell");
		}

		if (!s->l.forw) {
			rt_bomb("nmg_vshell: Shell's forw ptr is null\n");
		} else if (s->l.forw->back != (struct nmg_list *)s) {
			rt_log("forw shell's back(%8x) is not me (%8x)\n",
				s->l.forw->back, s);
			rt_bomb("nmg_vshell\n");
		}

		if (s->sa_p) {
			NMG_CK_SHELL_A(s->sa_p);
			/* we make sure that all values of min_pt
			 * are less than or equal to the values of max_pt
			 */
			lpt = s->sa_p->min_pt;
			hpt = s->sa_p->max_pt;
			if (lpt[0] > hpt[0] || lpt[1] > hpt[1] ||
			    lpt[2] > hpt[2]) {
				rt_log("Bad min_pt/max_pt for shell(%8x)'s extent\n");
				rt_log("Min_pt %g %g %g\n", lpt[0], lpt[1],
					lpt[2]);
				rt_log("Max_pt %g %g %g\n", hpt[0], hpt[1],
					hpt[2]);
			}
		}

		/* now we check out the "children"
		 */

		if (s->vu_p) {
			if( NMG_LIST_NON_EMPTY( &s->fu_hd ) ||
			    NMG_LIST_NON_EMPTY( &s->lu_hd ) ||
			    NMG_LIST_NON_EMPTY( &s->eu_hd ) )  {
				rt_log("shell (%8x) with vertexuse (%8x) has other children\n",
					s, s->vu_p);
				rt_bomb("");
			}
		}

		nmg_vfu( &s->fu_hd, s);
		nmg_vlu( &s->lu_hd, s);
		nmg_veu( &s->eu_hd, s);
	}
}



/*
 *			N M G _ V R E G I O N
 *
 *	validate a list of nmgregions and all elements under them
 */
void nmg_vregion(hp, m)
struct nmg_list	*hp;
struct model *m;
{
	struct nmgregion *r;

	for( NMG_LIST( r, nmgregion, hp ) )  {
		NMG_CK_REGION(r);
		if (r->m_p != m) {
			rt_log("nmgregion pointer m_p %8x should be %8x\n",
				r->m_p, m);
			rt_bomb("nmg_vregion\n");
		}
		if (r->ra_p) {
			NMG_CK_REGION_A(r->ra_p);
		}

		nmg_vshell( &r->s_hd, r);

		if( NMG_LIST_PNEXT_PLAST(nmgregion, r) != r )  {
			rt_bomb("forw nmgregion's back is not me\n");
		}
	}
}

/*	N M G _ V M O D E L
 *
 *	validate an NMG model and all elements in it.
 */
void nmg_vmodel(m)
struct model *m;
{
	NMG_CK_MODEL(m);
	if (m->ma_p) {
		NMG_CK_MODEL_A(m->ma_p);
	}
	nmg_vregion( &m->r_hd, m);
}
#if 0

/* ToDo:
 * esqueeze
 * Convenience Routines:
 * make edge,loop (close an open edgelist to make loop)
 */
#endif
