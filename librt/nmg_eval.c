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

#define BACTION_KILL			1
#define BACTION_RETAIN			2
#define BACTION_RETAIN_AND_FLIP		3

/* Actions are listed: onAinB, onAonB, onAoutB, inAonB, onAonB, outAonB */
static int		subtraction_actions[6] = {
	BACTION_KILL,
	BACTION_RETAIN,		/* _IF_OPPOSITE */
	BACTION_RETAIN,
	BACTION_RETAIN_AND_FLIP,
	BACTION_KILL,
	BACTION_KILL
};

static int		union_actions[6] = {
	BACTION_KILL,
	BACTION_RETAIN,		/* _IF_SAME */
	BACTION_RETAIN,
	BACTION_KILL,
	BACTION_KILL,
	BACTION_RETAIN
};

static int		intersect_actions[6] = {
	BACTION_RETAIN,
	BACTION_RETAIN,		/* If opposite, ==> non-manifold */
	BACTION_KILL,
	BACTION_RETAIN,
	BACTION_KILL,
	BACTION_KILL
};

/*
 *			N M G _ A C T _ O N _ L O O P
 *
 *  Perform the given action on all the loops listed in the loopuse array.
 */
void
nmg_act_on_loop( ltbl, action, dest, src )
struct nmg_ptbl	*ltbl;
int		action;
struct shell	*dest;
struct shell	*src;
{
	register struct loopuse	**lup;
	register struct loopuse	*lu;
	register struct faceuse	*fu;
	register int		i;

	NMG_CK_SHELL( dest );
	NMG_CK_SHELL( src );

	switch( action )  {
	default:
		rt_bomb("nmg_act_on_loop: bad action\n");
		/* NOTREACHED */

	case BACTION_KILL:
		i = ltbl->end-1;
		lup = &((struct loopuse **)(ltbl->buffer))[i];
		for( ; i >= 0; i--, lup-- ) {
			lu = *lup;
			*lup = 0;			/* sanity */
			NMG_CK_LOOPUSE(lu);
			fu = lu->up.fu_p;
			NMG_CK_FACEUSE(fu);
			/* Kill this loop use */
			nmg_klu( lu );
			/* If no loops remain in this face use, kill face use */
			if( !fu->lu_p )  nmg_kfu( fu );
		}
		break;

	case BACTION_RETAIN:
		i = ltbl->end-1;
		lup = &((struct loopuse **)(ltbl->buffer))[i];
		for( ; i >= 0; i--, lup-- ) {
			lu = *lup;
			*lup = 0;			/* sanity */
			NMG_CK_LOOPUSE(lu);
			fu = lu->up.fu_p;
			NMG_CK_FACEUSE(fu);
			if( fu->s_p == dest )  continue;
			nmg_mv_fu_between_shells( dest, src, fu );
		}
		break;

	case BACTION_RETAIN_AND_FLIP:
		i = ltbl->end-1;
		lup = &((struct loopuse **)(ltbl->buffer))[i];
		for( ; i >= 0; i--, lup-- ) {
			lu = *lup;
			*lup = 0;			/* sanity */
			NMG_CK_LOOPUSE(lu);
			fu = lu->up.fu_p;
			NMG_CK_FACEUSE(fu);
			nmg_reverse_face( fu );		/* flip */
			if( fu->s_p == dest )  continue;
			nmg_mv_fu_between_shells( dest, src, fu );
		}
		break;
	}
}

/*
 *			N M G _ E V A L U A T E _ B O O L E A N
 *
 *  Evaluate a boolean operation on the two shells "A" and "B",
 *  of the form "answer = A op B".
 *  As input, each loop in both A and B has been classified as being
 *  "in", "on", or "out" of the other shell.
 *  Using these classifications, operate on the faces.
 *  At the end, shell A contains the resultant object, and
 *  shell B is destroyed.
 *
 *  XXX need to ensure each face is treated only once, maybe.
 *  XXX really need onAonBsame and onAonBopposite lists, not AonB & BonA.
 */
void
nmg_evaluate_boolean( sA, sB, op, AinB, AonB, AoutB, BinA, BonA, BoutA)
struct shell	*sA;
struct shell	*sB;
int		op;
struct nmg_ptbl *AinB, *AonB, *AoutB, *BinA, *BonA, *BoutA;
{
	struct nmg_ptbl	*classified_shell_loops[6];
	struct nmg_ptbl faces;
	struct loopuse	*lu;
	struct faceuse	*fu;
	int		*actions;
	int		i;

	switch( op )  {
	case NMG_BOOL_SUB:
		actions = subtraction_actions;
		break;
	case NMG_BOOL_ADD:
		actions = union_actions;
		break;
	case NMG_BOOL_ISECT:
		actions = intersect_actions;
		break;
	default:
		rt_log("ERROR nmg_evaluate_boolean() op=%d.\n", op);
		rt_bomb("bad boolean\n");
	}

	/* XXX for the future, ensure each face is treated only once */
	(void)nmg_tbl(&faces, TBL_INIT, (long *)NULL);

	classified_shell_loops[0] = AinB;
	classified_shell_loops[1] = AonB;
	classified_shell_loops[2] = AoutB;
	classified_shell_loops[3] = BinA;
	classified_shell_loops[4] = BonA;
	classified_shell_loops[5] = BoutA;

	for( i=0; i<6; i++ )  {
		nmg_act_on_loop( classified_shell_loops[i],
			actions[i], sA, sB );
	}

	/* Plot the result */
	if (rt_g.NMG_debug & DEBUG_SUBTRACT) {
		FILE *fp, *fopen();

		if ((fp=fopen("bool_ans.pl", "w")) == (FILE *)NULL) {
			(void)perror("bool_ans.pl");
			exit(-1);
		}
    		rt_log("plotting bool_ans.pl\n");
		nmg_pl_s( fp, sA );
		(void)fclose(fp);
	}


	if (sB->fu_p) {
		rt_log("WARNING:  nmg_evaluate_boolean():  shell B still has faces!\n");
	}
	if (sB->lu_p || sB->eu_p || sB->vu_p) {
		rt_log("WARNING:  nmg_evaluate_boolean():  shell B still has loops/edges/verts!\n");
	}
	/* Regardless of what is in it, kill shell B */
	nmg_ks( sB );

	(void)nmg_tbl(&faces, TBL_FREE, (long *)NULL);
}
