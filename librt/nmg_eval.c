/*			N M G _ E V A L . C
 *
 *	Evaluate boolean operations on NMG objects
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSnmg_eval[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/*
 *			N M G _ T O S S _ L O O P S
 *
 *	throw away all loops in the loopuse list "lu"
 */
static void nmg_toss_loops(lu, n)
struct loopuse **lu;
int n;
{
	int				i;
	struct faceuse			*fu;	
	register struct shell		*s;
	register struct nmgregion	*r;


	for (i=0 ; i < n ; ++i) {
		fu = lu[i]->up.fu_p;
		nmg_klu(lu[i]);
		lu[i] = (struct loopuse *)NULL;
		if (!fu->lu_p) {
			s = fu->s_p;
			nmg_kfu(fu);
			if (!s->fu_p && !s->lu_p && !s->eu_p && !s->vu_p) {
				r = s->r_p;
				nmg_ks(s);
				if (!r->s_p && r->next != r) nmg_kr(r);
			}
		}
	}
}

/*
 *			N M G _ R E V E R S E _ F A C E
 *
 *  Reverse the orientation of a face.
 *  Manipulate both the topological and geometric aspects of the face.
 */
void
nmg_reverse_face( fu )
register struct faceuse	*fu;
{
	register struct faceuse	*fumate;
	register vectp_t	v;

	NMG_CK_FACEUSE(fu);
	fumate = fu->fumate_p;
	NMG_CK_FACEUSE(fumate);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G(fu->f_p->fg_p);

	/* reverse face normal vector */
	v = fu->f_p->fg_p->N;
	VREVERSE(v, v);
	v[H] *= -1.0;

	/* switch which face is "outside" face */
	if (fu->orientation == OT_SAME)  {
		if (fumate->orientation != OT_OPPOSITE)  {
			rt_log("nmg_reverse_face(fu=x%x) fu:SAME, fumate:%d\n",
				fu, fumate->orientation);
			rt_bomb("nmg_reverse_face() orientation clash\n");
		} else {
			fu->orientation = OT_OPPOSITE;
			fumate->orientation = OT_SAME;
		}
	} else if (fu->orientation == OT_OPPOSITE) {
		if (fumate->orientation != OT_SAME)  {
			rt_log("nmg_reverse_face(fu=x%x) fu:OPPOSITE, fumate:%d\n",
				fu, fumate->orientation);
			rt_bomb("nmg_reverse_face() orientation clash\n");
		} else {
			fu->orientation = OT_SAME;
			fumate->orientation = OT_OPPOSITE;
		}
	} else {
		/* Unoriented face? */
		rt_log("ERROR nmg_reverse_face(fu=x%x), orientation=%d.\n",
			fu, fu->orientation );
	}
}

/*
 *			N M G _ M V _ F U _ B E T W E E N _ S H E L L S
 *
 *  Move faceuse from 'src' shell to 'dest' shell.
 */
void
nmg_mv_fu_between_shells( dest, src, fu )
struct shell		*dest;
register struct shell	*src;
register struct faceuse	*fu;
{
	register struct faceuse	*fumate;

	NMG_CK_FACEUSE(fu);
	fumate = fu->fumate_p;
	NMG_CK_FACEUSE(fumate);

	if (fu->s_p != src) {
		rt_log("nmg_mv_fu_between_shells(dest=x%x, src=x%x, fu=x%x), fu->s_p=x%x isnt src shell\n",
			dest, src, fu, fu->s_p );
		rt_bomb("fu->s_p isnt source shell\n");
	}
	if (fumate->s_p != src) {
		rt_log("nmg_mv_fu_between_shells(dest=x%x, src=x%x, fu=x%x), fumate->s_p=x%x isn't src shell\n",
			dest, src, fu, fumate->s_p );
		rt_bomb("fumate->s_p isnt source shell\n");
	}

	/* Remove fu from src shell */
	src->fu_p = fu;
	DLLRM(src->fu_p, fu);
	if (src->fu_p == fu) {
		/* This was the last fu in the list, bad news */
		rt_log("nmg_mv_fu_between_shells(dest=x%x, src=x%x, fu=x%x), fumate=x%x not in src shell\n",
			dest, src, fu, fumate );
		rt_bomb("src shell emptied before finding fumate\n");
	}

	/* Remove fumate from src shell */
	src->fu_p = fumate;
	DLLRM(src->fu_p, fumate);
	if (src->fu_p == fumate) {
		/* This was the last fu in the list, tidy up */
		src->fu_p = (struct faceuse *)NULL;
	}

	/* Add fu and fumate to dest shell */
	DLLINS(dest->fu_p, fu);
	fu->s_p = dest;
	DLLINS(dest->fu_p, fumate);
	fumate->s_p = dest;
}

/*	S U B T R A C T I O N
 *
 *	reshuffle the faces of two shells to perform a subtraction of
 *	one shell from the other.  Shell "sB" disappears and shell "sA"
 *	contains:
 *		the faces that were in sA outside of sB
 *		the faces that were in sB inside of sA (normals flipped)
 */
void subtraction(sA, AinB, AonB, AoutB, sB, BinA, BonA, BoutA)
struct shell *sA, *sB;
struct nmg_ptbl *AinB, *AonB, *AoutB, *BinA, *BonA, *BoutA;
{
	struct nmg_ptbl faces;
	struct loopuse *lu;
	struct faceuse *fu;
	union {
		struct loopuse **lu;
		long **l;
	} p;
	int i;

	(void)nmg_tbl(&faces, TBL_INIT, (long *)NULL);

	/* first we toss out the unwanted faces from both shells */
	p.l = AinB->buffer;
	nmg_toss_loops(p.lu, AinB->end);

	p.l = BonA->buffer;
	nmg_toss_loops(p.lu, BonA->end);

	p.l = BoutA->buffer;
	nmg_toss_loops(p.lu, BoutA->end);

	p.l = BinA->buffer;
	for (i=0 ; i < BinA->end ; ++i) {
		lu = p.lu[i];
		NMG_CK_LOOPUSE(lu);
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE(fu);
		if (nmg_tbl(&faces, TBL_LOC, &fu->magic) < 0) {
			/* Move fu from shell B to A, flip normal */
			nmg_mv_fu_between_shells( sA, sB, fu );
			nmg_reverse_face( fu );
		}
	}


	if (rt_g.NMG_debug & DEBUG_SUBTRACT) {
		FILE *fd, *fopen();
		struct nmg_ptbl junk;

		(void)nmg_tbl(&junk, TBL_INIT, (long *)NULL);

		if ((fd=fopen("sub.pl", "w")) == (FILE *)NULL) {
			(void)perror("sub.pl");
			exit(-1);
		}
    		rt_log("plotting sub.pl\n");

		for (i=0 ; i < AoutB->end ; ++i) {
			p.l = &	AoutB->buffer[i];
			nmg_pl_lu(fd, *p.lu, &junk, 200, 200, 200);
		}

		for (i=0 ; i < BinA->end ; ++i) {
			p.l = &	BinA->buffer[i];
			nmg_pl_lu(fd, *p.lu, &junk, 200, 200, 200);
		}


		(void)nmg_tbl(&junk, TBL_FREE, (long *)NULL);
		(void)fclose(fd);
	}


	if (sB->fu_p) {
		rt_log("Why does shell B still have faces?\n");
	} else if (!sB->lu_p && !sB->eu_p && !sB->vu_p) {
		nmg_ks(sB);
	}


	(void)nmg_tbl(&faces, TBL_FREE, (long *)NULL);
}
/*	A D D I T I O N
 *
 *	add (union) two shells together
 *
 */
void addition(sA, AinB, AonB, AoutB, sB, BinA, BonA, BoutA)
struct shell *sA, *sB;
struct nmg_ptbl *AinB, *AonB, *AoutB, *BinA, *BonA, *BoutA;
{
	struct nmg_ptbl faces;
	struct loopuse *lu;
	struct faceuse *fu;
	union {
		struct loopuse **lu;
		long **l;
	} p;
	int i;


	(void)nmg_tbl(&faces, TBL_INIT, (long *)NULL);

	/* first we toss out the unwanted faces from both shells */
	p.l = AinB->buffer;
	nmg_toss_loops(p.lu, AinB->end);

	p.l = BinA->buffer;
	nmg_toss_loops(p.lu, BinA->end);

	/* Here we handle the delicat issue of combining faces of A and B
	 * which are "ON" each other
	 *
	 * Unimlpemented
	 */


	/* combine faces of B with A */
	p.l = BoutA->buffer;
	for (i=0 ; i < BoutA->end ; ++i) {
		lu = p.lu[i];
		NMG_CK_LOOPUSE(lu);
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE(fu);
		if (nmg_tbl(&faces, TBL_LOC, &fu->magic) < 0) {
			/* Move fu from shell B to A */
			nmg_mv_fu_between_shells( sA, sB, fu );
		}
	}

	if (sB->fu_p) {
		rt_log("Why does shell B still have faces?\n");
	} else if (!sB->lu_p && !sB->eu_p && !sB->vu_p) {
		nmg_ks(sB);
	}

	(void)nmg_tbl(&faces, TBL_FREE, (long *)NULL);
}

/*	I N T E R S E C T I O N
 *
 *	resulting shell is all faces of sA in sB and all faces of sB in sA
 */
void intersection(sA, AinB, AonB, AoutB, sB, BinA, BonA, BoutA)
struct shell *sA, *sB;
struct nmg_ptbl *AinB, *AonB, *AoutB, *BinA, *BonA, *BoutA;
{
	struct nmg_ptbl faces;
	struct loopuse *lu;
	struct faceuse *fu;
	union {
		struct loopuse **lu;
		long **l;
	} p;
	int i;


	(void)nmg_tbl(&faces, TBL_INIT, (long *)NULL);

	/* first we toss out the unwanted faces from both shells */
	p.l = AoutB->buffer;
	nmg_toss_loops(p.lu, AoutB->end);

	p.l = BoutA->buffer;
	nmg_toss_loops(p.lu, BoutA->end);

	/* we need to handle the "ON" faces here
	 *
	 * unimplemented
	 */

	/* now we combine the faces of B with A */
	p.l = BinA->buffer;
	for (i=0 ; i < BinA->end ; ++i) {
		lu = p.lu[i];
		NMG_CK_LOOPUSE(lu);
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE(fu);
		if (nmg_tbl(&faces, TBL_LOC, &fu->magic) < 0) {
			/* Move fu from shell B to A */
			nmg_mv_fu_between_shells( sA, sB, fu );
		}
	}

	if (sB->fu_p) {
		rt_log("Why does shell B still have faces?\n");
	} else if (!sB->lu_p && !sB->eu_p && !sB->vu_p) {
		nmg_ks(sB);
	}

	(void)nmg_tbl(&faces, TBL_FREE, (long *)NULL);
}
