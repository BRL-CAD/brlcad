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
	}
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

	GET_MODEL(m);
	GET_REGION(r);

	m->magic = NMG_MODEL_MAGIC;
	m->r_p = r;
	m->ma_p = (struct model_a *)NULL;

	r->magic = NMG_REGION_MAGIC;
	r->m_p = m;
	r->next = r->last = r;
	r->ra_p = (struct nmgregion_a *)NULL;
	r->s_p = (struct shell *)NULL;

	return(m);
}

/*	N M G _ M R
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

	r->magic = NMG_REGION_MAGIC;
	r->s_p = (struct shell *)NULL;
	(void)nmg_msv(r);

	r->m_p = m;

	/* new region goes at "head" of list of regions */
	DLLINS(m->r_p, r);

	return(r);
}
/*	N M G _ M S V
 *	Make Shell, Vertex Use, Vertex
 *	Create a new shell in a specified region.  The shell will consist
 *	of a single vertexuse and vertex (which are also created).
 */
struct shell *nmg_msv(r_p)
struct nmgregion	*r_p;
{
	struct shell *s;

	NMG_CK_REGION(r_p);

	GET_SHELL(s);

	/* set up shell */
	s->magic = NMG_SHELL_MAGIC;
	s->r_p = r_p;
	DLLINS(r_p->s_p, s);

	s->sa_p = (struct shell_a *)NULL;
	s->vu_p = nmg_mvvu(&s->magic);
	s->fu_p = (struct faceuse *)NULL;
	s->lu_p = (struct loopuse *)NULL;
	s->eu_p = (struct edgeuse *)NULL;

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

	if (*upptr != NMG_SHELL_MAGIC &&
	    *upptr != NMG_LOOPUSE_MAGIC &&
	    *upptr != NMG_EDGEUSE_MAGIC) {
		rt_log("in %s at %d magic not shell, loop, or edge (%d)\n",
		    __FILE__, __LINE__, *upptr);
		rt_bomb("Cannot build vertexuse without parent");
	}

	GET_VERTEXUSE(vu);

	vu->magic = NMG_VERTEXUSE_MAGIC;
	vu->v_p = v;
	vu->vua_p = (struct vertexuse_a *)NULL;
	DLLINS(v->vu_p, vu);
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
	v->vg_p = (struct vertex_g *)NULL;
	return(nmg_mvu(v, upptr));
}


/*	N M G _ M E
 *	Make edge
 *	Make a new edge between a pair of vertices in a shell.
 *	A new vertex will be made for any NULL vertex pointer parameters.
 *	If we need to make a new vertex and the shell still has its vertexuse
 *	we re-use that vertex rather than freeing and re-allocating.
 *
 *	Explicit Return:
 *		edgeuse whose vertex use is v1, whose eumate has vertex v2
 */
struct edgeuse *nmg_me(v1, v2, s)
struct vertex *v1, *v2;
struct shell *s;
{
	struct edge *e;
	struct edgeuse *eu1, *eu2;

	if (v1) NMG_CK_VERTEX(v1);
	if (v2) NMG_CK_VERTEX(v2);
	NMG_CK_SHELL(s);

	GET_EDGE(e);
	GET_EDGEUSE(eu1);
	GET_EDGEUSE(eu2);

	e->magic = NMG_EDGE_MAGIC;
	e->eu_p = eu1;
	e->eg_p = (struct edge_g *)NULL;

	eu1->magic = eu2->magic = NMG_EDGEUSE_MAGIC;

	eu1->radial_p = eu1->eumate_p = eu2;
	eu2->radial_p = eu2->eumate_p = eu1;

	eu1->e_p = eu2->e_p = e;
	eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;

	if (v1)
		eu1->vu_p = nmg_mvu(v1, &eu1->magic);
	else if (s->vu_p) {
		/* steal the vertex from the shell */
		eu1->vu_p = s->vu_p;
		s->vu_p->up.eu_p = eu1;
		s->vu_p = (struct vertexuse *)NULL;
	}
	else
		eu1->vu_p = nmg_mvvu(&eu1->magic);


	if (v2)
		eu2->vu_p = nmg_mvu(v2, &eu2->magic);
	else if (s->vu_p) {
		/* steal the vertex from the shell */
		eu2->vu_p = s->vu_p;
		s->vu_p->up.eu_p = eu2;
		s->vu_p = (struct vertexuse *)NULL;
	}
	else
		eu2->vu_p = nmg_mvvu(&eu2->magic);

	if (s->vu_p)
		nmg_kvu(s->vu_p);

	/* link the edgeuses to the parent shell */
	eu1->up.s_p = eu2->up.s_p = s;

	if (s->vu_p)
		nmg_kvu(s->vu_p);

	DLLINS(eu2->up.s_p->eu_p, eu2);
	DLLINS(eu1->up.s_p->eu_p, eu1);

	return(eu1);
}



/*	N M G _ M E o n V U
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
		rt_bomb("cannot make edge vertexuse not sole element of object");
	}

	GET_EDGE(e);
	GET_EDGEUSE(eu1);
	GET_EDGEUSE(eu2);

	e->magic = NMG_EDGE_MAGIC;
	e->eu_p = eu1;
	e->eg_p = (struct edge_g *)NULL;

	eu1->magic = eu2->magic = NMG_EDGEUSE_MAGIC;

	eu1->radial_p = eu1->eumate_p = eu2;
	eu2->radial_p = eu2->eumate_p = eu1;

	eu1->e_p = eu2->e_p = e;
	eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;


	/* link edgeuses to parent */
	if (*vu->up.magic_p == NMG_SHELL_MAGIC) {
		eu1->vu_p = vu;
		eu2->vu_p = nmg_mvu(vu->v_p, &eu2->magic);
		eu2->up.s_p = eu1->up.s_p = vu->up.s_p;
		if (vu->up.s_p->vu_p != vu)
			rt_bomb("vetexuse parent shell disowns vertexuse!\n");
		vu->up.s_p->vu_p = (struct vertexuse *)NULL;
		vu->up.eu_p = eu1;
		DLLINS(eu2->up.s_p->eu_p, eu2);
		DLLINS(eu1->up.s_p->eu_p, eu1);
	}
	else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		/* do a little consistency checking */
		if (vu->up.lu_p->lumate_p->magic != NMG_LOOPUSE_MAGIC ||
		    *vu->up.lu_p->lumate_p->down.magic_p != 
		    NMG_VERTEXUSE_MAGIC)
			rt_bomb("mate of vertex-loop is not vertex-loop!\n");

		/* edgeuses point at vertexuses */
		eu1->vu_p = vu;
		eu2->vu_p = vu->up.lu_p->lumate_p->down.vu_p;

		/* edgeuses point at parent loopuses */
		eu1->up.lu_p = eu1->vu_p->up.lu_p;
		eu2->up.lu_p = eu2->vu_p->up.lu_p;

		/* loopuses point at edgeuses */
		vu->up.lu_p->lumate_p->down.eu_p = (struct edgeuse *)NULL;
		vu->up.lu_p->down.eu_p = (struct edgeuse *)NULL;
		DLLINS(vu->up.lu_p->lumate_p->down.eu_p, eu2);
		DLLINS(vu->up.lu_p->down.eu_p, eu1);


		/* vertexuses point at edgeusees */
		eu1->vu_p->up.eu_p = eu1;
		eu2->vu_p->up.eu_p = eu2;
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
	struct edgeuse *p1, *p2, *feu;

	NMG_CK_SHELL(s);

	GET_LOOP(l);
	GET_LOOPUSE(lu1);
	GET_LOOPUSE(lu2);

	l->magic = NMG_LOOP_MAGIC;
	l->lu_p = lu1;
	l->lg_p = (struct loop_g *)NULL;

	lu1->magic = lu2->magic = NMG_LOOPUSE_MAGIC;
	lu1->up.s_p = lu2->up.s_p = s;
	lu1->l_p = lu2->l_p = l;
	lu1->lua_p = lu2->lua_p = (struct loopuse_a *)NULL;
	lu1->down.magic_p = lu2->down.magic_p = (long *)NULL;
	lu1->orientation = lu2->orientation = OT_UNSPEC;

	lu1->lumate_p = lu2;
	lu2->lumate_p = lu1;

	/* make loop on single vertex */
	if (!s->eu_p && s->vu_p) {
		lu1->down.vu_p = s->vu_p;
		s->vu_p->up.lu_p = lu1;
		lu2->down.vu_p = nmg_mvu(s->vu_p->v_p, &lu2->magic);
		s->vu_p = (struct vertexuse *)NULL;
		DLLINS(s->lu_p, lu2);
		DLLINS(s->lu_p, lu1);
		return(lu1);
	}

	feu = s->eu_p;

	do {
		/* bogosity check */
		if (s->eu_p->up.s_p != s || s->eu_p->eumate_p->up.s_p != s)
			rt_bomb("edgeuse mates don't have proper parent!");

		/* get the first edgeuse */
		DLLRM(s->eu_p, p1);
		if (s->eu_p == p1) {
			rt_log("in %s at %d edgeuse mate not in this shell\n",
			    __FILE__, __LINE__);
			rt_bomb("");
		}

		/* pick out its mate */
		if (s->eu_p == p1->eumate_p) {
			DLLRM(s->eu_p, p2);
			if (s->eu_p == p2) s->eu_p = (struct edgeuse *)NULL;
		} else {
			DLLRM(p1->eumate_p, p2);
			p1->eumate_p = p2;
		}

		DLLINS(lu1->down.eu_p, p1);
		DLLINS(lu2->down.eu_p, p2);

		/* we want to insert the next new edgeuse(s) in the tail of
		 * the list, not the head.
		 */
		lu1->down.eu_p = lu1->down.eu_p->next;
		lu2->down.eu_p = lu2->down.eu_p->next;

		p1->up.lu_p = lu1;
		p2->up.lu_p = lu2;

	} while (s->eu_p && s->eu_p->vu_p->v_p == p2->vu_p->v_p);

	/*	printf("p2v %x feu %x\n", p2->vu_p->v_p, feu->vu_p->v_p); */

	if (p2->vu_p->v_p != feu->vu_p->v_p) {
		rt_log("Edge(use)s do not form proper loop!\n");
		nmg_pr_s(s, (char *)NULL);
		rt_log("Edge(use)s do not form proper loop!\n");
		rt_bomb("Bye\n");
		/* re-link edgeuses back to shell */
		do {
			DLLRM(lu1->down.eu_p, p1);
			if (lu1->down.eu_p == p1)
				lu1->down.eu_p = (struct edgeuse *)NULL;
			DLLINS(s->eu_p, p1);
		} while (lu1->down.eu_p);
		do {
			DLLRM(lu2->down.eu_p, p2);
			if (lu2->down.eu_p == p2)
				lu2->down.eu_p = (struct edgeuse *)NULL;
			DLLINS(s->eu_p, p2);
		} while (lu2->down.eu_p);

		return((struct loopuse *)NULL);
	}

	DLLINS(s->lu_p, lu2);
	DLLINS(s->lu_p, lu1);

	return(lu1);
}

/*	N M G _ M O V E V U
 *	Move a vertexuse to a new vertex
 */
void nmg_movevu(vu, v)
struct vertexuse *vu;
struct vertex *v;
{
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(v);

	if (vu->next == vu->last && vu->next == vu) {
		/* last vertexuse on vertex */
		if (vu->v_p) {
			if (vu->v_p->vg_p) FREE_VERTEX_G(vu->v_p->vg_p);
			FREE_VERTEX(vu->v_p);
		}
	} else {
		vu->next->last = vu->last;
		vu->last->next = vu->next;

		/* make sure vertex isn't pointing at this vu */
		if (vu->v_p->vu_p == vu)
			vu->v_p->vu_p = vu->next;
	}

	vu->v_p = v;
	DLLINS(v->vu_p, vu);
}

/*	N M G _ K V U
 *	Kill vertexuse
 */
void nmg_kvu(vu)
register struct vertexuse *vu;
{

	NMG_CK_VERTEXUSE(vu);

	/* ditch any attributes */
	if (vu->vua_p) FREE_VERTEXUSE_A(vu->vua_p);

	if (vu->next == vu->last && vu->next == vu) {
		/* last vertexuse on vertex */
		if (vu->v_p) {
			if (vu->v_p->vg_p) FREE_VERTEX_G(vu->v_p->vg_p);
			FREE_VERTEX(vu->v_p);
		}
	} else {
		vu->next->last = vu->last;
		vu->last->next = vu->next;

		/* make sure vertex isn't pointing at this vu */
		if (vu->v_p->vu_p == vu)
			vu->v_p->vu_p = vu->next;
	}

	/* erase existence of this vertexuse from parent */
	if (*vu->up.magic_p == NMG_SHELL_MAGIC)
		vu->up.s_p->vu_p = (struct vertexuse *)NULL;
	else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC)
		vu->up.lu_p->down.vu_p = (struct vertexuse *)NULL;
	else if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC)
		vu->up.eu_p->vu_p = (struct vertexuse *)NULL;
	else
		rt_bomb("killing vertexuse of unknown parent?\n");

	FREE_VERTEXUSE(vu);
}


/*	N M G _ K F U
 *	Kill Faceuse
 *	delete a faceuse and its mate from the parent shell.  Any children
 *	found are brutally murdered as well.  If this is the last child of
 *	a shell, we null-out the shell's downward pointer
 */
void nmg_kfu(fu1)
struct faceuse *fu1;
{
	struct faceuse *fu2, *p;
	NMG_CK_FACEUSE(fu1);
	fu2 = fu1->fumate_p;
	NMG_CK_FACEUSE(fu2);


	/* kill off the children (infanticide?)*/
	while (fu1->lu_p)
		nmg_klu(fu1->lu_p);

	/* kill the geometry */
	if (fu1->f_p != fu2->f_p)
		rt_bomb("faceuse mates do not share face!\n");
	if (fu1->f_p) {
		if (fu1->f_p->fg_p) {
			NMG_CK_FACE_G(fu1->f_p->fg_p);
			FREE_FACE_G(fu1->f_p->fg_p);
		}
		FREE_FACE(fu1->f_p);
		fu1->f_p = fu2->f_p = (struct face *)NULL;
	}

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
	fu1->s_p->fu_p = fu1;
	DLLRM(fu1->s_p->fu_p, p);
	if (p == fu1->s_p->fu_p)
		rt_bomb("faceuse mate not in parent?\n");

	fu2->s_p->fu_p = fu2;
	DLLRM(fu2->s_p->fu_p, p);
	if (p == fu2->s_p->fu_p)
		fu2->s_p->fu_p = (struct faceuse *)NULL;

	FREE_FACEUSE(fu1);
	FREE_FACEUSE(fu2);
}


/*	N M G _ K L U
 *	Kill loopuse
 *	if the loop contains any edgeuses they are placed in the parent shell
 *	before the loop is deleted.
 */
void nmg_klu(lu1)
struct loopuse *lu1;
{
	struct loopuse *lu2, *p;
#if 0
	struct edgeuse *eu1, *eu2;
#endif
	struct faceuse *fu;
	struct shell *s;

	NMG_CK_LOOPUSE(lu1);
	lu2 = lu1->lumate_p;
	NMG_CK_LOOPUSE(lu2);


	if (*lu1->up.magic_p != *lu2->up.magic_p)
		rt_bomb("loopuses do not have same type of parent!\n");

	if (*lu1->down.magic_p != *lu2->down.magic_p)
		rt_bomb("loopuses do not have same type of child!\n");

	/* deal with the children */
	if (*lu1->down.magic_p == NMG_VERTEXUSE_MAGIC) {
		nmg_kvu(lu1->down.vu_p);
		nmg_kvu(lu2->down.vu_p);
	}
	else if (*lu1->down.magic_p == NMG_EDGEUSE_MAGIC) {
#if 0
		/* get the parent shell */
		if (*lu1->up.magic_p == NMG_SHELL_MAGIC)
			s = lu1->up.s_p;
		else if (*lu1->up.magic_p == NMG_FACEUSE_MAGIC) {
			if (lu1->up.fu_p->s_p->magic != NMG_SHELL_MAGIC)
				rt_bomb("faceuse of loopuse does't have shell for parent\n");
			else
				s = lu1->up.fu_p->s_p;
		} else {
			rt_bomb("Cannot identify parent\n");
		}

		/* move all edgeuses (&mates) to shell (in order so that we
		 * can )
		 */
		while (lu1->down.eu_p) {
			/* move edgeuse & mate to parent shell */
			DLLRM(lu1->down.eu_p, eu1);
			if (lu1->down.eu_p == eu1)
				lu1->down.eu_p = (struct edgeuse *)NULL;

			if (eu1->eumate_p->up.lu_p != lu2)
				rt_bomb("edgeuse mates don't share loop\n");

			lu2->down.eu_p = eu1->eumate_p;
			DLLRM(lu2->down.eu_p, eu2);
			if (lu2->down.eu_p == eu2)
				lu2->down.eu_p = (struct edgeuse *)NULL;

			eu1->up.s_p = eu2->up.s_p = s;
			DLLINS(s->eu_p, eu2);
			DLLINS(s->eu_p, eu1);
		}
		if (lu2->down.eu_p)
			rt_bomb("loopuse mates don't have same # of edges\n");
#else
		/* delete all edgeuse in the loopuse (&mate) */
		while (lu1->down.eu_p)
			nmg_keu(lu1->down.eu_p);
#endif
	}
	else
		rt_bomb("unknown type for loopuse child\n");


	/* disconnect from parent's list */
	if (*lu1->up.magic_p == NMG_SHELL_MAGIC) {
		s = lu1->up.s_p;
		s->lu_p = lu1;
		DLLRM(s->lu_p, p);
		if (s->lu_p == p) s->lu_p = (struct loopuse *)NULL;

		s->lu_p = lu2;
		DLLRM(s->lu_p, p);
		if (s->lu_p == p) s->lu_p = (struct loopuse *)NULL;
	}
	else if (*lu1->up.magic_p == NMG_FACEUSE_MAGIC) {
		fu = lu1->up.fu_p;
		fu->lu_p = lu1;
		DLLRM(fu->lu_p, p);
		if (fu->lu_p == p) fu->lu_p = (struct loopuse *)NULL;

		fu = lu2->up.fu_p;
		fu->lu_p = lu2;
		DLLRM(fu->lu_p, p);
		if (fu->lu_p == p) fu->lu_p = (struct loopuse *)NULL;
	}
	else {
		rt_log("in %s at %d Unknown parent for loopuse\n", __FILE__,
		    __LINE__);
		rt_bomb("bye");
	}

	if (lu1->lua_p) {
		NMG_CK_LOOPUSE_A(lu1->lua_p);
		FREE_LOOPUSE_A(lu1->lua_p);
	}
	if (lu2->lua_p) {
		NMG_CK_LOOPUSE_A(lu2->lua_p);
		FREE_LOOPUSE_A(lu2->lua_p);
	}

	if (lu1->l_p != lu2->l_p)
		rt_bomb("loopmates do not share loop!\n");

	if (lu1->l_p) {
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


/*	N M G _ M F
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
		rt_bomb("loop must be child of shell for making face");
	}
	lu2 = lu1->lumate_p;
	NMG_CK_LOOPUSE(lu2);
	if (lu2->up.magic_p != lu1->up.magic_p) {
		rt_bomb("loopuse mate does not have same parent\n");
	}

	s = lu1->up.s_p;

	GET_FACE(f);
	GET_FACEUSE(fu1);
	GET_FACEUSE(fu2);

	f->fu_p = fu1;
	f->fg_p = (struct face_g *)NULL;
	f->magic = NMG_FACE_MAGIC;

	fu1->magic = fu2->magic = NMG_FACEUSE_MAGIC;
	fu1->s_p = fu2->s_p = s;
	fu1->fumate_p = fu2;
	fu2->fumate_p = fu1;
	fu1->orientation = fu2->orientation = OT_UNSPEC;
	fu1->f_p = fu2->f_p = f;
	fu1->fua_p = fu2->fua_p = (struct faceuse_a *)NULL;


	/* move the loopuses from the shell to the faceuses */
	s->lu_p = lu1;
	DLLRM(s->lu_p, fu1->lu_p);
	if (s->lu_p == fu1->lu_p)
		rt_bomb("loopuses don't have same parent?\n");
	lu1->up.fu_p = fu1;
	lu1->orientation = OT_SAME;

	s->lu_p = lu2;
	DLLRM(s->lu_p, fu2->lu_p)
	    if (s->lu_p == fu2->lu_p)
		s->lu_p = (struct loopuse *)NULL;
	lu2->up.fu_p = fu2;
	lu2->orientation = OT_SAME;

	/* connect the faces to the parent shell */
	DLLINS(s->fu_p, fu2);
	DLLINS(s->fu_p, fu1);

	return(fu1);
}


/*	N M G _ K E U
 *	Delete an edgeuse & it's mate on a shell/loop.
 *
 *	This routine DOES null-out the
 *	parent's edgeuse pointer when the last edgeuse is being
 *	deleted.
 */
void nmg_keu(eu1)
register struct edgeuse *eu1;
{
	register struct edgeuse *eu2, *tmpeu;

	/* prevent mishaps */
	NMG_CK_EDGEUSE(eu1);

	eu2 = eu1->eumate_p;
	NMG_CK_EDGEUSE(eu2);

	if (eu1->e_p != eu2->e_p) {
		rt_log("In %s at %d Edgeuse pair does not share edge\n",
		    __FILE__, __LINE__);
		rt_bomb("Dying in nmg_keu");
	}
	NMG_CK_EDGE(eu1->e_p);

	/* unlink from radial linkages (if any)
	 */
	if (eu1->radial_p != eu2) {
		NMG_CK_EDGEUSE(eu1->radial_p);
		NMG_CK_EDGEUSE(eu2->radial_p);

		eu1->radial_p->radial_p = eu2->radial_p;
		eu2->radial_p->radial_p = eu1->radial_p;

		/* since there is a use of this edge left, make sure the edge
		 * structure points to a remaining edgeuse
		 */
		if (eu1->e_p->eu_p == eu1 || eu1->e_p->eu_p == eu2)
			eu1->e_p->eu_p = eu1->radial_p;
	} else {
		/* since these two edgeuses are the only use of this edge,
		 * we need to free the edge (since all uses are about 
		 * to disappear).
		 */
		if (eu1->e_p->eg_p) FREE_EDGE_G(eu1->e_p->eg_p);
		FREE_EDGE(eu1->e_p);
	}

	/* remove the edgeuses from their parents */
	if (*eu1->up.magic_p == NMG_LOOPUSE_MAGIC) {

		if (eu1->up.lu_p->lumate_p != eu2->up.lu_p ||
		    eu1->up.lu_p != eu2->up.lu_p->lumate_p ) {
			rt_log("In %s at %d %s\n", __FILE__, __LINE__,
			    "edgeuse mates don't belong to loopuse mates");
			rt_bomb("bye");
		}

		/* remove the edgeuses from their parents */
		eu1->up.lu_p->down.eu_p = eu1;
		DLLRM(eu1->up.lu_p->down.eu_p, tmpeu);
		if (tmpeu == eu1->up.lu_p->down.eu_p)
			eu1->up.lu_p->down.eu_p = (struct edgeuse *)NULL;

		eu2->up.lu_p->down.eu_p = eu2;
		DLLRM(eu2->up.lu_p->down.eu_p, tmpeu);
		if (tmpeu == eu2->up.lu_p->down.eu_p)
			eu2->up.lu_p->down.eu_p = (struct edgeuse *)NULL;

		/* if deleting this edge would cause parent loop to become
		 * non-contiguous or if there are no more edges left in loop,
		 * we must kill the parent loopuses.
		 *
		 *if (eu2->vu_p->v_p != eu1->vu_p->v_p || 
		 *    !eu1->up.lu_p->down.eu_p)
		 *	nmg_klu(eu1->up.lu_p);
		 */

	} else if (*eu1->up.magic_p == NMG_SHELL_MAGIC) {

		if (eu1->up.s_p != eu2->up.s_p) {
			rt_log("in %s at %d edguses don't share parent\n",
			    __FILE__, __LINE__);
			rt_bomb("bye");
		}

		/* unlink edgeuses from the parent shell */
		eu1->up.s_p->eu_p = eu1;
		DLLRM(eu1->up.s_p->eu_p, tmpeu);
		eu2->up.s_p->eu_p = eu2;
		DLLRM(eu2->up.s_p->eu_p, tmpeu);
		if (eu2->up.s_p->eu_p == tmpeu) {
			eu2->up.s_p->eu_p = (struct edgeuse *)NULL;
		}
	}


	/* get rid of any attributes */
	if (eu1->eua_p) {
		if (eu1->eua_p == eu2->eua_p) {
			FREE_EDGEUSE_A(eu1->eua_p);
			eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;
		}
		else {
			FREE_EDGEUSE_A(eu1->eua_p);
			FREE_EDGEUSE_A(eu2->eua_p);
			eu1->eua_p = eu2->eua_p = (struct edgeuse_a *)NULL;
		}
	} else if (eu2->eua_p) {
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
	struct shell *tmps;

	NMG_CK_SHELL(s);

	while (s->fu_p)
		nmg_kfu(s->fu_p);
	while (s->lu_p)
		nmg_klu(s->lu_p);
	while (s->eu_p)
		nmg_keu(s->eu_p);
	while (s->vu_p)
		nmg_kvu(s->vu_p);

	s->r_p->s_p = s;
	DLLRM(s->r_p->s_p, tmps);
	if (s->r_p->s_p == tmps)
		s->r_p->s_p = (struct shell *)NULL;

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
	struct nmgregion *tmpr;

	NMG_CK_REGION(r);

	while (r->s_p)
		nmg_ks(r->s_p);

	r->m_p->r_p = r;
	DLLRM(r->m_p->r_p, tmpr);
	if (r->m_p->r_p == tmpr)
		r->m_p->r_p = (struct nmgregion *)NULL;

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

	while (m->r_p)
		nmg_kr(m->r_p);

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
	struct edgeuse *eu;
	struct vertex_g *vg;

	if (l->lg_p) {
		NMG_CK_LOOP_G(l->lg_p);
	} else {
		GET_LOOP_G(l->lg_p);
		l->lg_p->magic = NMG_LOOP_G_MAGIC;
	}

	l->lg_p->max_pt[X] = l->lg_p->max_pt[Y] = 
	    l->lg_p->max_pt[Z] = -MAX_FASTF;
	l->lg_p->min_pt[X] = l->lg_p->min_pt[Y] = 
	    l->lg_p->min_pt[Z] = MAX_FASTF;

	eu = l->lu_p->down.eu_p;
	do {
		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);

		VMINMAX(l->lg_p->min_pt, l->lg_p->max_pt, vg->coord);

		eu = eu->next;
	} while (eu != l->lu_p->down.eu_p);
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
	struct face_g *fg;
	struct loopuse *lu;

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

	lu = f->fu_p->lu_p;
	do {
		/*		if (!lu->l_p->lg_p) */
		nmg_loop_g(lu->l_p);

		VMIN(fg->min_pt, lu->l_p->lg_p->min_pt);
		VMAX(fg->max_pt, lu->l_p->lg_p->max_pt);

		lu = lu->next;
	} while (lu != f->fu_p->lu_p);
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
	if (s->fu_p) {
		fu = s->fu_p;
		do {
			if (!fu->f_p->fg_p)
				nmg_face_bb(fu->f_p);

			fg = fu->f_p->fg_p;
			VMIN(sa->min_pt, fg->min_pt);
			VMAX(sa->max_pt, fg->max_pt);

			if (fu->next != s->fu_p &&
			    fu->next->f_p == fu->f_p)
				fu = fu->next->next;
			else
				fu = fu->next;
		} while (fu != s->fu_p);
	}
	if (s->lu_p) {
		lu = s->fu_p->lu_p;
		do {
			if (!lu->l_p->lg_p)
				nmg_loop_g(lu->l_p);

			VMIN(sa->min_pt, lu->l_p->lg_p->min_pt);
			VMAX(sa->max_pt, lu->l_p->lg_p->max_pt);

			lu = lu->next;
		} while (lu != s->fu_p->lu_p);

	}
	if (s->eu_p) {
		eu = s->eu_p;
		do {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);

			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);

			VMINMAX(sa->min_pt, sa->max_pt, vg->coord);

			eu = eu->next;
		} while (eu != s->eu_p);
	}
	if (s->vu_p) {
		NMG_CK_VERTEXUSE(s->vu_p);
		NMG_CK_VERTEX(s->vu_p->v_p);
		NMG_CK_VERTEX_G(s->vu_p->v_p->vg_p);
		vg = s->vu_p->v_p->vg_p;

		VMINMAX(sa->min_pt, sa->max_pt, vg->coord);
	}


	if (!s->fu_p && !s->lu_p && !s->eu_p && !s->vu_p) {
		rt_log("At %d in %s shell has no children\n",
		    __LINE__, __FILE__);
		rt_bomb("exiting\n");
	}

}


/*	N M G _ E U S P L I T
 *	Split an edgeuse
 *	Make a new edge, and a vertex.  If v is non-null it is taken as a
 *	pointer to an existing vertex to use as the start of the new edge.
 *	If v is null, then a new vertex is created for the begining of the
 *	new edge.
 *
 *	In either case, the new edge will exist as the "next" edge after
 *	the edge passed as a parameter.
 *
 *	Before:
 *		oldeu
 *		.------------->
 *
 *		 =============== (edge)
 *
 *		  <-------------.
 *		   oldeu->eumate
 *
 *	After:
 *		oldeu(cw)   eu1
 *		.-------> .----->
 *
 *	   (edge)========= ~~~~~~~(new edge)
 *
 *		  <-------. <-----.
 *		 oldeumate  eu1mate
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

	/* if this edge has uses other than this edge and its mate, we must
	 * separate these two edgeuses from the existing edge, and create
	 * a new edge for them.  Then we can insert a new vertex in this
	 * new edge without fear of damaging some other object.
	 */
	if (oldeu->radial_p != oldeu->eumate_p)
		nmg_unglueedge(oldeu);


	if (*oldeu->up.magic_p == NMG_SHELL_MAGIC) {
		/* set the shell's edge pointer so that a "make edge" 
		 * operation will insert a new edge in the "appropriate" place
		 */
		if (oldeu->next != oldeu->eumate_p)
			oldeu->up.s_p->eu_p = oldeu->next;
		else
			oldeu->up.s_p->eu_p = oldeu->next->next;

		/* we make an edge from the new vertex to the vertex at the
		 * other end of the edge given
		 */
		if (v)
			eu1 = nmg_me(v,
			    oldeu->eumate_p->vu_p->v_p, oldeu->up.s_p);
		else
			eu1 = nmg_me((struct vertex *)NULL,
			    oldeu->eumate_p->vu_p->v_p, oldeu->up.s_p);

		/* now move the end of the old edge over to the new vertex */
		nmg_movevu(oldeu->eumate_p->vu_p, eu1->vu_p->v_p);
		return(eu1);
	}
	else if (*oldeu->up.magic_p != NMG_LOOPUSE_MAGIC) {
		rt_log("in %s at %d invalid edgeuse parent\n",
			__FILE__, __LINE__);
		rt_bomb("routine nmg_eusplit");
	}

	/* now we know we are in a loop */

	/* get a parent shell pointer so we can make a new edge */
	if (*oldeu->up.lu_p->up.magic_p == NMG_SHELL_MAGIC)
		s = oldeu->up.lu_p->up.s_p;
	else {
		s = oldeu->up.lu_p->up.fu_p->s_p;
		NMG_CK_SHELL(s);
	}

	/* make a new edge on the vertex */
	if (v) {
		eu1 = nmg_me(v, v, s);
		eu2 = eu1->eumate_p;
	} else {
		eu1 = nmg_me((struct vertex *)NULL, (struct vertex *)NULL, s);
		eu2 = eu1->eumate_p;
		nmg_movevu(eu2->vu_p, eu1->vu_p->v_p);
	}

	/* pick out the edge from the shell */
	DLLRM(s->eu_p, eu1);
	DLLRM(s->eu_p, eu2);
	if (s->eu_p == eu2)
		s->eu_p = (struct edgeuse *)NULL;

	oldeumate = oldeu->eumate_p;

	/* insert the new edge(uses) in the loop
	 * eu1 will become the mate to oldeumate on the existing edge.
	 * eu2 will become the mate of oldeu on the new edge.
	 * orientation and parentage are copied copied.
	 */
	eu1->last = oldeu;
	eu1->next = oldeu->next;
	eu1->next->last = eu1;
	oldeu->next = eu1;
	eu1->up.magic_p = oldeu->up.magic_p;
	eu1->orientation = oldeu->orientation;
	eu1->eua_p = (struct edgeuse_a *)NULL;
	eu1->eumate_p = eu1->radial_p = oldeumate;
	oldeumate->radial_p = oldeumate->eumate_p = eu1;

	eu2->next = oldeumate->next;
	eu2->last = oldeumate;
	eu2->next->last = eu2;
	oldeumate->next = eu2;
	eu2->up.magic_p = oldeumate->up.magic_p;
	eu2->orientation = oldeumate->orientation;
	eu2->eua_p = (struct edgeuse_a *)NULL;
	eu2->eumate_p = eu2->radial_p = oldeu;
	oldeu->radial_p = oldeu->eumate_p = eu2;

	/* straighten out ownership of the edge.
	 * oldeu keeps the edge it had (and shares with eu2).
	 * oldeumate and eu1 share the new edge.
	 */
	oldeumate->e_p = eu1->e_p;
	eu2->e_p = oldeu->e_p;

	return(eu1);
}

/*	N M G _ M O V E E U
 *	Move a pair of edgeuses onto a new edge (glue edgeuse).
 *	the edgeuse eusrc and its mate are moved to the edge
 *	used by eudst.  eusrc is made to be immediately radial to eudst.
 *	if eusrc does not share the same vertices as eudst, we bomb.
 */
void nmg_moveeu(eudst, eusrc)
struct edgeuse *eudst, *eusrc;
{
	struct edge *e;

	NMG_CK_EDGEUSE(eudst);
	NMG_CK_EDGEUSE(eusrc);

	if (eusrc->e_p == eudst->e_p && eusrc->radial_p == eudst)
		return;


	/* make sure vertices are shared */
	if ( ! ( (eudst->eumate_p->vu_p->v_p == eusrc->vu_p->v_p &&
	    eudst->vu_p->v_p == eusrc->eumate_p->vu_p->v_p) ||
	    (eudst->vu_p->v_p == eusrc->vu_p->v_p &&
	    eudst->eumate_p->vu_p->v_p == eusrc->eumate_p->vu_p->v_p) ) ) {
		/* edgeuses do NOT share verticies. */
	    	rt_bomb("edgeuses do not share vertices, cannot share edge\n");
	}

	e = eusrc->e_p;
	eusrc->eumate_p->e_p = eusrc->e_p = eudst->e_p;

	/* if we're not deleting the edge, make sure it will be able
	 * to reference the remaining uses, otherwise, take care of disposing
	 * of the (now unused) edge
	 */
	if (eusrc->radial_p != eusrc->eumate_p) {
		/* this is NOT the only use of the eusrc edge! */
		if (e->eu_p == eusrc || e->eu_p == eusrc->eumate_p)
			e->eu_p = eusrc->radial_p;

		/* disconnect from the list of uses of this edge */
		eusrc->radial_p->radial_p = eusrc->eumate_p->radial_p;
		eusrc->eumate_p->radial_p->radial_p = eusrc->radial_p;
	} else {
		/* this is the only use of the eusrc edge */
		if (e->eg_p) FREE_EDGE_G(e->eg_p);
		FREE_EDGE(e);
	}

	eusrc->radial_p = eudst;
	eusrc->eumate_p->radial_p = eudst->radial_p;

	eudst->radial_p->radial_p = eusrc->eumate_p;
	eudst->radial_p = eusrc;
}




/*	N M G _ U N G L U E 
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
/*	N M G _ J V
 *	Join two vertexes into one
 */
void nmg_jv(v1, v2)
struct vertex *v1, *v2;
{
	struct vertexuse *vu1, *vu2;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);

	if (v1 == v2) return;

	/* tell all the vertexuses about their new vertex */
	vu1 = vu2 = v2->vu_p;
	do {
		vu2->v_p = v1;
		vu2 = vu2->next;
	} while (vu2 != vu1);

	/* link vertexuse list for v2 to v1's vertexuse list */
	vu1 = v1->vu_p;

	vu1->last->next = vu2->next;
	vu2->next->last = vu1->last;
	vu2->next = vu1;
	vu1->last = vu2;

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
/* #define DEBUG_INS */
#ifdef DEBUG_INS
		rt_log("nmg_tbl Inserting %8x\n", p);
#endif
		pp.l = p;

		if (b->blen == 0) (void)nmg_tbl(b, TBL_INIT, p);
		if (b->end >= b->blen)
			b->buffer = (long **)rt_realloc(b->buffer,
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
			
			b->buffer = (long **)rt_realloc(b->buffer,
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
		rt_bomb("bye");
	}
	return(-1);/* this is here to keep lint happy */
}


/*	N M G _ M L V
 *	Make a new loop (with specified orientation) and
 *	vertex in shell or face.
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

	GET_LOOP(l);
	GET_LOOPUSE(lu1);
	GET_LOOPUSE(lu2);
	if (v) {
		NMG_CK_VERTEX(v);
	}

	l->magic = NMG_LOOP_MAGIC;
	l->lu_p = lu1;
	l->lg_p = (struct loop_g *)NULL;

	lu1->magic = lu2->magic = NMG_LOOPUSE_MAGIC;

	lu1->l_p = lu2->l_p = l;
	lu1->lua_p = lu2->lua_p = (struct loopuse_a *)NULL;
	if (*p.magic_p == NMG_SHELL_MAGIC) {
		if (p.s->vu_p) {
			lu1->down.vu_p = p.s->vu_p;
			p.s->vu_p->up.lu_p = lu1;
			p.s->vu_p = (struct vertexuse *)NULL;
			if (v) nmg_movevu(lu1->down.vu_p, v);
		}
		else {
			if (v) lu1->down.vu_p = nmg_mvu(v, &lu1->magic);
			else lu1->down.vu_p = nmg_mvvu(&lu1->magic);
		}
		lu2->down.vu_p = nmg_mvu(lu1->down.vu_p->v_p, &lu2->magic);
		lu1->up.magic_p = lu2->up.magic_p = magic;

		lu1->lumate_p = lu2;
		lu2->lumate_p = lu1;

		DLLINS(p.s->lu_p, lu2);
		DLLINS(p.s->lu_p, lu1);
	} else if (*p.magic_p == NMG_FACEUSE_MAGIC) {
		if (v) lu1->down.vu_p = nmg_mvu(v, &lu1->magic);
		else lu1->down.vu_p = nmg_mvvu(&lu1->magic);

		lu2->down.vu_p = nmg_mvu(lu1->down.vu_p->v_p, &lu2->magic);
		lu1->up.fu_p = p.fu;
		lu2->up.fu_p = p.fu->fumate_p;

		lu1->lumate_p = lu2;
		lu2->lumate_p = lu1;

		DLLINS(p.fu->fumate_p->lu_p, lu2);
		DLLINS(p.fu->lu_p, lu1);
	} else {
		rt_bomb("unknown parent for loopuse!\n");
	}
	lu1->orientation = lu2->orientation = orientation;

	return(lu1);
}

/*	N M G _ M O V E L T O F
 *	move first shell loopuse to an existing face
 */
void nmg_moveltof(fu, s)
struct faceuse *fu;
struct shell *s;
{
	struct loopuse *lu;

	NMG_CK_SHELL(s);
	NMG_CK_FACEUSE(fu);
	if (fu->s_p != s) {
		rt_log("in %s at %d Cannot move loop to face in another shell\n",
		    __FILE__, __LINE__);
	}
	NMG_CK_LOOPUSE(s->lu_p);

	DLLRM(s->lu_p, lu);
	if (s->lu_p == lu) {
		rt_bomb("Loopuses don't share parent shell!\n");
	}

	DLLINS(fu->lu_p, lu);

	DLLRM(s->lu_p, lu);
	if (s->lu_p == lu)
		s->lu_p = (struct loopuse *)NULL;

	fu = fu->fumate_p;
	DLLINS(fu->lu_p, lu);
}

/*	N M G _ C F A C E
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
 */
struct faceuse *nmg_cface(s, verts, n)
struct shell *s;
struct vertex *verts[];
unsigned n;
{
	struct faceuse *fu;
	struct edgeuse *eu;
	unsigned i;

	NMG_CK_SHELL(s);
	if (n < 1) {
		rt_bomb("trying to make bogus face\n");
	}

	if (verts) {
		fu = nmg_mf(nmg_mlv(&s->magic, verts[0], OT_SAME));
		eu = nmg_meonvu(fu->lu_p->down.vu_p);

		if (!verts[0])
			verts[0] = eu->vu_p->v_p;

		for (i = 1 ; i < n ; ++i) {
			eu = nmg_eusplit(verts[i], fu->lu_p->down.eu_p);
			if (!verts[i])
				verts[i] = eu->vu_p->v_p;
		}

	} else {
		fu = nmg_mf(nmg_mlv(&s->magic, (struct vertex *)NULL, OT_SAME));
		(void)nmg_meonvu(fu->lu_p->down.vu_p);

		eu = fu->lu_p->down.eu_p;
		while (--n) {
			(void)nmg_eusplit((struct vertex *)NULL, eu);
		}
	}
	return (fu);
}

/*	N M G _ E S P L I T
 *	Split an edge.
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
			rt_bomb("Something's awry in nmg_esplit\n");
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

/*	N M G _ E I N S
 *	insert a new (zero length) edge at the begining of an existing edge
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
 *	.--A--> .---> .--eu-->
 *		 \   /
 *		  >.<
 *		 /   \
 *	  <-A'--. <---. <-eu'--.
 */
struct edgeuse *nmg_eins(eu)
struct edgeuse *eu;
{
	struct edgeuse *eu1, *eu2, *eulist;
	struct shell *s;

	NMG_CK_EDGEUSE(eu);

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

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		DLLRM(s->eu_p, eu1);
		DLLRM(s->eu_p, eu2);
		if (s->eu_p == eu2)
			s->eu_p = (struct edgeuse *)NULL;

		eulist = eu;
		DLLINS(eulist, eu1);
		eulist = eu->eumate_p->next;
		DLLINS(eulist, eu2);
		eu1->up.lu_p = eu->up.lu_p;
		eu2->up.lu_p = eu->eumate_p->up.lu_p;
	}
	else {
		rt_bomb("Cannot yet insert null edge in shell\n");
	}
	return(eu1);
}
#if 0
struct vertex *nmg_find_v_in_face(pt, fu)
point_t pt;
struct faceuse *fu;
{
	struct loopuse *lu;
	struct edgeuse *eu;

	NMG_CK_FACEUSE(fu);
	lu = fu->lu_p;
	do {
		NMG_CK_LOOPUSE(lu);
		if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
			if (VAPPROXEQUAL(pt, lu->down.vu_p->v_p->vg_p->coord,
			    VDIVIDE_TOL))
				return(lu->down.vu_p->v_p);
		}
		else if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = lu->down.eu_p;
			do {
				if (VAPPROXEQUAL(pt, eu->vu_p->v_p->vg_p->coord,
				    VDIVIDE_TOL))
					return(eu->vu_p->v_p);
				eu = eu->next;
			} while (eu != lu->down.eu_p);
		} else
			rt_bomb("Bogus child of loop\n");

		lu = lu->next;
	} while (lu != fu->lu_p);
	return ((struct vertex *)NULL);
}
#endif

/*	F I N D _ V U _ I N _ F A C E
 *
 *	try to find a vertex(use) in a face wich appoximately matches the
 *	coordinates given
 */
struct vertexuse *nmg_find_vu_in_face(pt, fu)
point_t pt;
struct faceuse *fu;
{
	struct loopuse *lu;
	struct edgeuse *eu;

	NMG_CK_FACEUSE(fu);
	lu = fu->lu_p;
	do {
		NMG_CK_LOOPUSE(lu);
		if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
			if (VAPPROXEQUAL(pt, lu->down.vu_p->v_p->vg_p->coord,
			    VDIVIDE_TOL))
				return(lu->down.vu_p);
		}
		else if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = lu->down.eu_p;
			do {
				if (VAPPROXEQUAL(pt,
				    eu->vu_p->v_p->vg_p->coord, VDIVIDE_TOL))
					return(eu->vu_p);
				eu = eu->next;
			} while (eu != lu->down.eu_p);
		} else
			rt_bomb("Bogus child of loop\n");

		lu = lu->next;
	} while (lu != fu->lu_p);
	return ((struct vertexuse *)NULL);
}

/*	N M G _ G L U E F A C E S
 *
 *	given a shell containing "n" faces whose outward oriented faceuses are
 *	enumerated in "fulist", glue the edges of the faces together
 *
 *
 *
 */
nmg_gluefaces(fulist, n)
struct faceuse *fulist[];
int n;
{
	struct shell	*s;
	struct edgeuse	*eu;
	int		i;
	int		f_no;		/* Face number */
	
	NMG_CK_FACEUSE(fulist[0]);
	s = fulist[0]->s_p;
	NMG_CK_SHELL(s);
	for (i = 0 ; i < n ; ++i) {
		NMG_CK_FACEUSE(fulist[i]);
		if (fulist[i]->s_p != s) {
			rt_log("in %s at %d faceuses don't share parent\n",
				__FILE__, __LINE__);
			rt_bomb("nmg_gluefaces\n");
		}
		NMG_CK_LOOPUSE(fulist[i]->lu_p);
		if (*fulist[i]->lu_p->down.magic_p != NMG_EDGEUSE_MAGIC) {
			rt_bomb("Cannot glue edges of face on vertex\n");
		} else {
			NMG_CK_EDGEUSE(fulist[i]->lu_p->down.eu_p);
		}
	}

	for (i=0 ; i < n ; ++i) {
	    eu = fulist[i]->lu_p->down.eu_p;
	    do {

		for (f_no=i+1 ; eu->radial_p == eu->eumate_p && f_no < n ;
		    ++f_no) {
			register struct edgeuse	*eu2;

			eu2 = fulist[f_no]->lu_p->down.eu_p;
			do {
			    if (EDGESADJ(eu, eu2))
			    	nmg_moveeu(eu, eu2);

			    eu2 = eu2->next;
			} while (eu2 != fulist[f_no]->lu_p->down.eu_p);
		}

		eu = eu->next;
	    } while (eu != fulist[i]->lu_p->down.eu_p);
	}
}

/*
 *			N M G _ S _ T O _ V L I S T
 *
 *  poly_markers = 0 for vectors, 1 for polygon markers
 */
nmg_s_to_vlist( vhead, s, poly_markers )
struct vlhead	*vhead;
struct shell	*s;
int		poly_markers;
{
	struct faceuse	*fu;
	struct face_g	*fg;
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertexuse *vu;
	struct vertex	*v;
	register struct vertex_g *vg;
	int		isfirst;
	struct vertex_g	*first_vg;

	NMG_CK_SHELL(s);
	if (s->vu_p)  {
		if (s->fu_p || s->lu_p || s->eu_p) {
			rt_bomb("nmg_s_to_vlist:  shell with vu also has other pointers\n");
		}
		vu = s->vu_p;
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
		return;
	}
	if( !(s->fu_p) )  {
		rt_log("nmg_s_to_vlist: shell with no faces?\n");
		return;
	}
	if (s->lu_p || s->eu_p || s->vu_p) {
		rt_bomb("nmg_s_to_vlist:  shell with fu also has other pointers\n");
	}

	fu = s->fu_p;
	do {
	    /* Consider this face */
	    NMG_CK_FACEUSE(fu);
	    NMG_CK_FACE(fu->f_p);
	    fg = fu->f_p->fg_p;
	    NMG_CK_FACE_G(fg);
	    if (fu->orientation == OT_SAME) {
		lu = fu->lu_p;
		do {
			/* Consider this loop */
			NMG_CK_LOOPUSE(lu);
			eu = lu->down.eu_p;
			NMG_CK_EDGEUSE(eu);
			isfirst = 1;
			first_vg = (struct vertex_g *)0;
			do {
				/* Consider this edge */
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE(vu);
				v = vu->v_p;
				NMG_CK_VERTEX(v);
				vg = v->vg_p;
				if( vg ) {
					NMG_CK_VERTEX_G(vg);
					if (isfirst) {
						if( poly_markers) {
							/* Insert a "start polygon, normal" marker */
							ADD_VL( vhead, fg->N, 2 );
						}
						ADD_VL( vhead, vg->coord, 0 );
						isfirst = 0;
						first_vg = vg;
					} else {
						ADD_VL( vhead, vg->coord, 1 );
					}
				}
				eu = eu->next;
			} while(eu != lu->down.eu_p);

			/* Draw back to the first vertex used */
			if( !isfirst && first_vg )  {
				ADD_VL( vhead, first_vg->coord, 1 );
			}

			lu = lu->next;
		}while (lu != fu->lu_p);
	    }
	    fu = fu->next;
	} while (fu != s->fu_p);
}


/*	F I N D E U
 *
 *	find an edgeuse in a shell between a pair of verticies
 */
static struct edgeuse *findeu(v1, v2, s)
struct vertex *v1, *v2;
struct shell *s;
{
	struct vertexuse *vu;
	struct edgeuse *eu;

	NMG_CK_VERTEX(v1);
	NMG_CK_VERTEX(v2);
	NMG_CK_SHELL(s);

	vu = v1->vu_p;
	do {
		if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC && 
		    vu->up.eu_p->eumate_p->vu_p->v_p == v2  &&
		    vu->up.eu_p->eumate_p == vu->up.eu_p->radial_p) {

			VPRINT("checking ", vu->v_p->vg_p->coord);
			VPRINT("and ", vu->up.eu_p->eumate_p->vu_p->v_p->vg_p->coord);

			eu = vu->up.eu_p;
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
			    eu->up.lu_p->up.fu_p->s_p == s)
			    	return(eu);
		}
		vu = vu->next;
	} while (vu != v1->vu_p);

	return((struct edgeuse *)NULL);
}



/*	N M G _ C M F A C E
 *	Create a face for a manifold shell from a list of vertices
 *
 *	"verts" is an array of "n" pointers to pointers to (struct vertex).
 *	"s" is the
 *	parent shell for the new face.  The face will consist of a single loop
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
 */
struct faceuse *nmg_cmface(s, verts, n)
struct shell *s;
struct vertex **verts[];
unsigned n;
{
	struct faceuse *fu;
	struct edgeuse *eu, *eur, *euold;
	unsigned i;

	NMG_CK_SHELL(s);

	if (n < 1) {
		rt_bomb("trying to make bogus face\n");
	}

	/* make sure verts points to some real storage */
	if (!verts) {
		rt_log("in %s at %d, null pointer to array start\n",
			__FILE__, __LINE__);
		rt_bomb("nmg_cmface\n");
	}

	/* validate each of the pointers in verts */
	for (i=0 ; i < n ; ++i) {
		if (verts[i]) {
			if (*verts[i]) {
				/* validate the structure pointers */
				NMG_CK_VERTEX(*verts[i]);
				NMG_CK_VERTEX_G((*verts[i])->vg_p);
			}
		} else {
			rt_log("in %s at %d, null ptr to ptr to struct vertex\n",
				__FILE__, __LINE__);
			rt_bomb("nmg_cmface\n");
		}
	}


	fu = nmg_mf(nmg_mlv(&s->magic, *verts[0], OT_SAME));
	eu = nmg_meonvu(fu->lu_p->down.vu_p);

	if (!(*verts[0]))
		*verts[0] = eu->vu_p->v_p;

	for (i = 1 ; i < n ; ++i) {

		euold = fu->lu_p->down.eu_p;

		/* look for pre-existing edge between these
		 * verticies
		 */
		if (*verts[i]) {
			/* look for an existing edge to share */
			eur = findeu(*verts[i-1], *verts[i], s);
			eu = nmg_eusplit(*verts[i], euold);
			if (eur) nmg_moveeu(eur, eu);
		} else {
			eu = nmg_eusplit(*verts[i], euold);
			*verts[i] = eu->vu_p->v_p;
		}
	}
	return (fu);
}


void nmg_vvg(vg)
struct vertex_g *vg;
{
	NMG_CK_VERTEX_G(vg);
}

void nmg_vvertex(v, vup)
struct vertex *v;
struct vertexuse *vup;
{
	struct vertexuse *vu;

	NMG_CK_VERTEX(v);

	vu = vup;
	do {
		NMG_CK_VERTEXUSE(vu);
		if (vu->v_p != v)
			rt_bomb("a vertexuse in my list doesn't share my vertex\n");
		vu = vu->next;
	} while (vu != vup);

	if (v->vg_p) nmg_vvg(v->vg_p);
}

void nmg_vvua(vua)
struct vertexuse_a *vua;
{
	NMG_CK_VERTEXUSE_A(vua);
}
void nmg_vvu(vu, up)
struct vertexuse *vu;
union {
	struct shell	*s;
	struct loopuse	*lu;
	struct edgeuse	*eu;
	long		*magic_p;
} up;
{
	NMG_CK_VERTEXUSE(vu);
	if (vu->up.magic_p != up.magic_p)
		rt_bomb("vertexuse denies parent\n");

	if (!vu->next)
		rt_bomb("vertexuse has null next pointer\n");

	if (vu->next->magic != NMG_VERTEXUSE_MAGIC)
		rt_bomb("vertexuse next is bad vertexuse\n");

	if (vu->next->last != vu)
		rt_bomb("vertexuse not last of next vertexuse\n");

	nmg_vvertex(vu->v_p, vu);

	if (vu->vua_p) nmg_vvua(vu->vua_p);
}



void nmg_veua(eua)
struct edgeuse_a *eua;
{
	NMG_CK_EDGEUSE_A(eua);
}

void nmg_veg(eg)
struct edge_g *eg;
{
	NMG_CK_EDGE_G(eg);
}

void nmg_vedge(e, eup)
struct edge *e;
struct edgeuse *eup;
{
	struct edgeuse *eu;
	int is_use = 0;		/* flag: eup is in edge's use list */

	NMG_CK_EDGE(e);
	NMG_CK_EDGEUSE(eup);

	if (!e->eu_p) rt_bomb("edge has null edgeuse pointer\n");

	eu = eup;
	do {
		if (eu == eup || eu->eumate_p == eup)
			is_use = 1;

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

void nmg_veu(eup, up)
struct edgeuse *eup;
union {
	struct shell	*s;
	struct loopuse	*lu;
	long 		*magic_p;
} up;
{
	struct edgeuse *eu;
	
	eu = eup;
	do {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != up.magic_p)
			rt_bomb("edgeuse denies parentage\n");

		if (!eu->next)
			rt_bomb("edgeuse has Null \"next\" pointer\n");
		if (eu->next->magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse next is bad edgeuse\n");
		if (eu->next->last != eu)
		    if (eu->next->last)
			rt_bomb("next edgeuse has last that points elsewhere\n");
		    else
			rt_bomb("next edgeuse has NULL last\n");

		if (eu->eumate_p->magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse mate is bad edgeuse\n");
		else if (eu->eumate_p->eumate_p != eu)
			rt_bomb("edgeuse mate spurns edgeuse\n");

		if (eu->radial_p->magic != NMG_EDGEUSE_MAGIC)
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

		eu = eu->next;
	} while (eu != eup);
}
void nmg_vlg(lg)
struct loop_g *lg;
{
	int i;
	
	NMG_CK_LOOP_G(lg);

	for (i=0 ; i < ELEMENTS_PER_PT ; ++i)
		if (lg->min_pt[i] > lg->max_pt[i])
			rt_bomb("loop geom min_pt greater than max_pt\n");
}
void nmg_vloop(l, lup)
struct loop *l;
struct loopuse *lup;
{
	struct loopuse *lu;

	NMG_CK_LOOP(l);
	NMG_CK_LOOPUSE(lup);

	if (!l->lu_p) rt_bomb("loop has null loopuse pointer\n");

	for (lu=lup ; lu && lu != l->lu_p && lu->next != lup ; lu = lu->next);
	
	if (l->lu_p != lu)
		for (lu=lup->lumate_p ; lu && lu != l->lu_p && lu->next != lup->lumate_p ; lu = lu->next);

	if (l->lu_p != lu) rt_bomb("can't get to parent loopuse from loop\n");

	if (l->lg_p) nmg_vlg(l->lg_p);
}

void nmg_vlua(lua)
struct loopuse_a *lua;
{
	NMG_CK_LOOPUSE_A(lua);
}

void nmg_vlu(lulist, up)
struct loopuse *lulist;
union {
	struct shell	*s;
	struct faceuse	*fu;
	long		*magic_p;
} up;
{
	struct loopuse *lu;
	
	lu = lulist;
	do {
		NMG_CK_LOOPUSE(lu);

		if (lu->up.magic_p != up.magic_p)
			rt_bomb("loopuse denies parentage\n");

		if (!lu->next)
			rt_bomb("loopuse has null next pointer\n");
		else if (lu->next->last != lu)
			rt_bomb("next loopuse has last pointing somewhere else\n");

		if (!lu->lumate_p)
			rt_bomb("loopuse has null mate pointer\n");

		if (lu->lumate_p->magic != NMG_LOOPUSE_MAGIC)
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

		if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC)
			nmg_veu(lu->down.eu_p, lu);
		else if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC)
			nmg_vvu(lu->down.vu_p, lu);

		lu = lu->next;
	} while (lu != lulist);
}
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


void nmg_vface(f, fup)
struct face *f;
struct faceuse *fup;
{
	struct faceuse *fu;

	NMG_CK_FACE(f);
	NMG_CK_FACEUSE(fup);

	/* make sure we can get back to the parent faceuse from the face */
	if (!f->fu_p) rt_bomb("face has null faceuse pointer\n");

	for (fu = fup; fu && fu != f->fu_p && fu->next != fup; fu = fu->next);

	if (f->fu_p != fu) rt_bomb("can't get to parent faceuse from face\n");
	
	if (f->fg_p) nmg_vfg(f->fg_p);
}


void nmg_vfua(fua)
struct faceuse_a *fua;
{
	NMG_CK_FACEUSE_A(fua);
}

/*	N M G _ V F U
 *
 *	Validate a list of faceuses
 */
void nmg_vfu(fulist, s)
struct faceuse *fulist;
struct shell *s;
{
	struct faceuse *fu;
	
	fu = fulist;
	do {
		NMG_CK_FACEUSE(fu);
		if (fu->s_p != s) {
			rt_log("faceuse claims shell parent (%8x) instead of (%8x)\n",
				fu->s_p, s);
			rt_bomb("nmg_vfu\n");
		}

		if (!fu->next) {
			rt_bomb("faceuse next is NULL\n");
		} else if (fu->next->last != fu) {
			rt_bomb("faceuse->next->last != faceuse\n");
		}

		if (!fu->fumate_p)
			rt_bomb("null faceuse fumate_p pointer\n");

		if (fu->fumate_p->magic != NMG_FACEUSE_MAGIC)
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
		
		NMG_CK_LOOPUSE(fu->lu_p);
		nmg_vlu(fu->lu_p, fu);
		
		fu = fu->next;
	} while (fu != fulist);
}


/*	N M G _ S H E L L
 *
 *	validate a list of shells and all elements under them
 */
void nmg_vshell(sp, r)
struct shell *sp;
struct nmgregion *r;
{
	struct shell *s;
	pointp_t lpt, hpt;

	s = sp;
	do {
		NMG_CK_SHELL(s);
		if (s->r_p != r) {
			rt_log("shell's r_p (%8x) doesn't point to parent (%8x)\n",
				s->r_p, r);
			rt_bomb("nmg_vshell");
		}

		if (!s->next) {
			rt_bomb("nmg_vshell: Shell's next ptr is null\n");
		} else if (s->next->last != s) {
			rt_log("next shell's last(%8x) is not me (%8x)\n",
				s->next->last, s);
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
			if (s->fu_p || s->lu_p || s->eu_p) {
				rt_log("shell (%8x) with vertexuse (%8x) has other children\n",
					s, s->vu_p);
				rt_bomb("");
			}
		}

		if (s->fu_p) {
			NMG_CK_FACEUSE(s->fu_p);
			nmg_vfu(s->fu_p, s);
		}

		if (s->lu_p) {
			NMG_CK_LOOPUSE(s->lu_p);
			nmg_vlu(s->lu_p, s);
		}

		if (s->eu_p) {
			NMG_CK_EDGEUSE(s->eu_p);
			nmg_veu(s->eu_p, s);
		}



		s = s->next;
	} while (s != sp);
}



/*	N M G _ V R E G I O N
 *
 *	validate a list of nmgregions and all elements under them
 */
void nmg_vregion(rp, m)
struct nmgregion *rp;
struct model *m;
{
	struct nmgregion *r;
	r = rp;
	do {
		NMG_CK_REGION(r);
		if (r->m_p != m) {
			rt_log("nmgregion pointer m_p %8x should be %8x\n",
				r->m_p, m);
			rt_bomb("nmg_vregion\n");
		}
		if (r->ra_p) {
			NMG_CK_REGION_A(r->ra_p);
		}

		if (r->s_p) {
			NMG_CK_SHELL(r->s_p);
			nmg_vshell(r->s_p, r);
		}

		if (r->next->last != r) {
			rt_bomb("next nmgregion's last is not me\n");
		}
		r = r->next;
	} while (r != rp);
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
	NMG_CK_REGION(m->r_p);
	nmg_vregion(m->r_p, m);
}
#if 0

/* ToDo:
 * esqueeze
 * Convenience Routines:
 * make edge,loop (close an open edgelist to make loop)
 */
#endif
