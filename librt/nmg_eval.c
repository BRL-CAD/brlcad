/*			N M G _ E V A L . C
 *
 *	Evaluate boolean operations on NMG objects
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
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

/*
 *			N M G _ M V _ L U _ B E T W E E N _ S H E L L S
 */
void
nmg_mv_lu_between_shells( dest, src, lu )
struct shell		*dest;
register struct shell	*src;
register struct loopuse	*lu;
{
	register struct loopuse	*lumate;

	NMG_CK_LOOPUSE(lu);
	lumate = lu->lumate_p;
	NMG_CK_LOOPUSE(lumate);

	if( lu->up.s_p != src )  {
		rt_log("nmg_mv_lu_between_shells(dest=x%x, src=x%x, lu=x%x), lu->up.s_p=x%x isn't source shell\n",
			dest, src, lu, lu->up.s_p );
		rt_bomb("lu->up.s_p isn't source shell\n");
	}
	if( lumate->up.s_p != src )  {
		rt_log("nmg_mv_lu_between_shells(dest=x%x, src=x%x, lu=x%x), lumate->up.s_p=x%x isn't source shell\n",
			dest, src, lu, lumate->up.s_p );
		rt_bomb("lumate->up.s_p isn't source shell\n");
	}

	/* Remove lu from src shell */
	src->lu_p = lu;
	DLLRM( src->lu_p, lu );
	if( src->lu_p == lu )  {
		/* This was the last lu in the list */
		rt_log("nmg_mv_lu_between_shells(dest=x%x, src=x%x, lu=x%x), lumate=x%x not in src shell\n",
			dest, src, lu, lumate );
		rt_bomb("src shell emptied before finding lumate\n");
	}

	/* Remove lumate from src shell */
	src->lu_p = lumate;
	DLLRM(src->lu_p, lumate);
	if (src->lu_p == lumate) {
		/* This was the last lu in the list, tidy up */
		src->lu_p = (struct loopuse *)NULL;
	}

	/* Add lu and lumate to dest shell */
	DLLINS(dest->lu_p, lu);
	lu->up.s_p = dest;
	DLLINS(dest->lu_p, lumate);
	lumate->up.s_p = dest;

}

/*
 *			N M G _ M V _ E U _ B E T W E E N _ S H E L L S
 */
void
nmg_mv_eu_between_shells( dest, src, eu )
struct shell		*dest;
register struct shell	*src;
register struct edgeuse	*eu;
{
	register struct edgeuse	*eumate;

	NMG_CK_EDGEUSE(eu);
	eumate = eu->eumate_p;
	NMG_CK_EDGEUSE(eumate);

	if (eu->up.s_p != src) {
		rt_log("nmg_mv_eu_between_shells(dest=x%x, src=x%x, eu=x%x), eu->up.s_p=x%x isnt src shell\n",
			dest, src, eu, eu->up.s_p );
		rt_bomb("eu->up.s_p isnt source shell\n");
	}
	if (eumate->up.s_p != src) {
		rt_log("nmg_mv_eu_between_shells(dest=x%x, src=x%x, eu=x%x), eumate->up.s_p=x%x isn't src shell\n",
			dest, src, eu, eumate->up.s_p );
		rt_bomb("eumate->up.s_p isnt source shell\n");
	}

	/* Remove eu from src shell */
	src->eu_p = eu;
	DLLRM(src->eu_p, eu);
	if (src->eu_p == eu) {
		/* This was the last eu in the list, bad news */
		rt_log("nmg_mv_eu_between_shells(dest=x%x, src=x%x, eu=x%x), eumate=x%x not in src shell\n",
			dest, src, eu, eumate );
		rt_bomb("src shell emptied before finding eumate\n");
	}

	/* Remove eumate from src shell */
	src->eu_p = eumate;
	DLLRM(src->eu_p, eumate);
	if (src->eu_p == eumate) {
		/* This was the last eu in the list, tidy up */
		src->eu_p = (struct edgeuse *)NULL;
	}

	/* Add eu and eumate to dest shell */
	DLLINS(dest->eu_p, eu);
	eu->up.s_p = dest;
	DLLINS(dest->eu_p, eumate);
	eumate->up.s_p = dest;
}

/*
 *			N M G _ M V _ V U _ B E T W E E N _ S H E L L S
 *
 *  If this shell had a single vertexuse in it, move it to the other
 *  shell, but "promote" it to a vertex-with-self-loop along the way.
 */
void
nmg_mv_vu_between_shells( dest, src, vu )
struct shell		*dest;
register struct shell	*src;
register struct vertexuse	*vu;
{

	NMG_CK_VERTEXUSE( vu );
	NMG_CK_VERTEX( vu->v_p );

	(void)nmg_mlv( &(dest->magic), vu->v_p, OT_SAME );
	nmg_kvu( vu );
}

#define BACTION_KILL			1
#define BACTION_RETAIN			2
#define BACTION_RETAIN_AND_FLIP		3

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

#if 0
/* Old way */

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
#else

/* XXX begin new way */

/*
 * Actions are listed:
 *	(Aon)	onAinB, onAonBshared, onAonBanti-shared, onAoutB,
 *	(Bon)	inAonB, onAonBshared, onAonBanti-shared, outAonB
 */
static int		subtraction_actions[8] = {
	BACTION_KILL,
	BACTION_KILL,		/* shared */
	BACTION_RETAIN,		/* anti-shared */
	BACTION_RETAIN,

	BACTION_RETAIN_AND_FLIP,
	BACTION_KILL,
	BACTION_KILL,
	BACTION_KILL
};

static int		union_actions[8] = {
	BACTION_KILL,
	BACTION_RETAIN,		/* shared */
	BACTION_KILL,		/* anti-shared */
	BACTION_RETAIN,

	BACTION_KILL,
	BACTION_KILL,
	BACTION_KILL,
	BACTION_RETAIN
};

static int		intersect_actions[8] = {
	BACTION_RETAIN,
	BACTION_RETAIN,		/* shared */
	BACTION_RETAIN,		/* anti-shared ==> non-manifold result */
	BACTION_KILL,

	BACTION_RETAIN,
	BACTION_KILL,
	BACTION_KILL,
	BACTION_KILL
};

struct nmg_bool_state  {
	struct shell	*bs_dest;
	struct shell	*bs_src;
	int		bs_isA;		/* true if A, else doing B */
	struct nmg_ptbl	*bs_classtab;
	int		*bs_actions;
};

#define NMG_CLASS_AinB		0
#define NMG_CLASS_AonBshared	1
#define NMG_CLASS_AonBanti	2
#define NMG_CLASS_AoutB		3
#define NMG_CLASS_BinA		4
#define NMG_CLASS_BonAshared	5
#define NMG_CLASS_BonAanti	6
#define NMG_CLASS_BoutA		7


/*
 *			N M G _ E V A L U A T E _ B O O L E A N
 *
 *  Evaluate a boolean operation on the two shells "A" and "B",
 *  of the form "answer = A op B".
 *  As input, each element (loop-in-face, wire loop, wire edge, vertex)
 *  in both A and B has been classified as being
 *  "in", "on", or "out" of the other shell.
 *  Using these classifications, operate on the input shells.
 *  At the end, shell A contains the resultant object, and
 *  shell B is destroyed.
 *
 */
void
nmg_evaluate_boolean( sA, sB, op, class_table )
struct shell	*sA;
struct shell	*sB;
int		op;
struct nmg_ptbl class_table[];
{
	struct loopuse	*lu;
	struct faceuse	*fu;
	int		*actions;
	int		i;
	struct nmg_bool_state	bool_state;

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

	bool_state.bs_dest = sA;
	bool_state.bs_src = sB;
	bool_state.bs_classtab = class_table;
	bool_state.bs_actions = actions;

	bool_state.bs_isA = 1;
	nmg_eval_do( sA, &bool_state );

	bool_state.bs_isA = 0;
	nmg_eval_do( sB, &bool_state );

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
}

/*
 *  Make a life-and-death decision on every element of a shell.
 *  Descend the "great chain of being" from the face to loop to edge
 *  to vertex, saving or demoting along the way.
 */
nmg_eval_do( s, bs )
register struct shell	*s;
struct nmg_bool_state *bs;
{
	struct faceuse	*fu;
	struct faceuse	*fu_end;
	struct loopuse	*lu;
	struct loopuse	*lu_end;
	struct edgeuse	*eu;
	struct edgeuse	*eu_end;
	struct vertexuse *vu;
	struct vertex	*v;

	NMG_CK_SHELL(s);

	/*
	 *  For each face in the shell, process all the loops in the face,
	 *  and then handle the face and all loops as a unit.
	 */
	fu = fu_end = s->fu_p;
	do  {
		/* Consider this face */
		if( !fu )  break;
		NMG_CK_FACEUSE(fu);
		NMG_CK_FACE(fu->f_p);
		lu = lu_end = fu->lu_p;
		do {
			if( !lu )  break;
			NMG_CK_LOOPUSE(lu);
			eu = lu->down.eu_p;
			NMG_CK_EDGEUSE(eu);
			if( nmg_action( (genptr_t)lu->l_p, bs ) == BACTION_KILL )  {
				/* Kill by demoting loop to edges */
				register struct loopuse	*nextlu = lu->next;
				if( lu->lumate_p == nextlu )  rt_log("lumate?\n");
				NMG_CK_FACEUSE( lu->up.fu_p );
				NMG_CK_FACEUSE( nextlu->up.fu_p );

				NMG_CK_LOOPUSE(nextlu);
				(void)nmg_demote_lu( lu );
				NMG_CK_LOOPUSE(nextlu);
				lu = nextlu;
				continue;
			}
			/* Retain loop.  Any action will be done to the
			 * faceuse as a whole, not just to this loop.
			 */
			lu = lu->next;
		}while (lu != lu_end);

		/*
		 *  Here, faceuse will have 0 or more loopuses still in it.
		 *  Decide the fate of the face;  if the face dies,
		 *  then any remaining loops, edges, etc, will die too.
		 */
		if( ((lu = fu->lu_p) == (struct loopuse *)0) ||
		    lu->next == fu->lu_p )  {
			/* faceuse is empty, it dies */
			register struct faceuse	*nextfu = fu->next;
		    	NMG_CK_FACEUSE(nextfu);
		    	if( fu->fumate_p == nextfu ) nextfu = nextfu->next;
		    	NMG_CK_FACEUSE(nextfu);
			nmg_kfu( fu );
		    	NMG_CK_FACEUSE(nextfu);
			fu = nextfu;
		    	continue;
		}

#if 0
		/* XXX decide how */
		nmg_reverse_face( fu );
#endif
		if( s != bs->bs_dest )  {
			nmg_mv_fu_between_shells( bs->bs_dest, s, fu );
		}
		fu = fu->next;
	} while (fu != fu_end);

	/*
	 *  For each loop in the shell, process.
	 *  Each loop is either a wire-loop, or a vertex-with-self-loop.
	 *  Only consider wire loops here.
	 */
	lu = lu_end = s->lu_p;
	do  {
		if( !lu )  break;
		NMG_CK_LOOPUSE(lu);
		if( *(lu->down.magic_p) == NMG_VERTEXUSE_MAGIC )  {
			/* ignore vertex-with-self-loop */
			lu = lu->next;
			continue;
		}
		NMG_CK_LOOP( lu->l_p );
		switch( nmg_action( (genptr_t)lu->l_p, bs ) )  {
		case BACTION_KILL:
			/* Demote the loopuse into wire edges */
			{
				register struct loopuse	*nextlu = lu->next;
				NMG_CK_LOOPUSE(nextlu);
				if( lu->lumate_p == nextlu )  nextlu=nextlu->next;
				NMG_CK_LOOPUSE(nextlu);
				(void)nmg_demote_lu( lu );
				NMG_CK_LOOPUSE(nextlu);
				lu = nextlu;
				continue;
			}
		case BACTION_RETAIN:
		case BACTION_RETAIN_AND_FLIP:
			if( s != bs->bs_dest )  {
				nmg_mv_lu_between_shells( bs->bs_dest, s, lu );
			}
			break;
		}
		lu = lu->next;
	}  while( lu != lu_end );

	/*
	 *  For each wire-edge in the shell, ...
	 */
	eu = eu_end = s->eu_p;
	do  {
		/* Consider this edge */
		if( !eu )  break;
		NMG_CK_EDGE( eu->e_p );
		switch( nmg_action( (genptr_t)eu->e_p, bs ) )  {
		case BACTION_KILL:
			/* Demote the degeuse into vertices */
			{
				register struct edgeuse	*nexteu = eu->next;
				NMG_CK_EDGEUSE(nexteu);
				nmg_demote_eu( eu );
				NMG_CK_EDGEUSE(nexteu);
				eu = nexteu;
				continue;
			}
		case BACTION_RETAIN:
		case BACTION_RETAIN_AND_FLIP:
			if( s != bs->bs_dest )  {
				nmg_mv_eu_between_shells( bs->bs_dest, s, eu );
			}
			break;
		}
		eu = eu->next;
	} while( eu != eu_end );

	/*
	 *  For each lone vertex-with-self-loop, process.
	 *  Note that these are intermixed in the loop list.
	 *  Each loop is either a wire-loop, or a vertex-with-self-loop.
	 *  Only consider cases of vertex-with-self-loop here.
	 *
	 *  This case has to be handled separately, because a wire-loop
	 *  may be demoted to a set of wire-edges above, some of which
	 *  may be retained.  The non-retained wire-edges may in turn
	 *  be demoted into vertex-with-self-loop objects above,
	 *  which will be processed here.
	 */
	lu = lu_end = s->lu_p;
	do  {
		if( !lu )  break;
		NMG_CK_LOOPUSE(lu);
		if( *(lu->down.magic_p) != NMG_VERTEXUSE_MAGIC )  {
			/* ignore any remaining wire-loops */
			lu = lu->next;
			continue;
		}
		vu = lu->down.vu_p;
		NMG_CK_VERTEXUSE( vu );
		NMG_CK_VERTEX( vu->v_p );
		switch( nmg_action( (genptr_t)vu->v_p, bs ) )  {
		case BACTION_KILL:
			/* Simply eliminate the loopuse */
			{
				register struct loopuse	*nextlu = lu->next;
				NMG_CK_LOOPUSE(nextlu);
				if( lu->lumate_p == nextlu )  nextlu=nextlu->next;
				NMG_CK_LOOPUSE(nextlu);
				nmg_klu( lu );
				NMG_CK_LOOPUSE(nextlu);
				lu = nextlu;
				continue;
			}
		case BACTION_RETAIN:
		case BACTION_RETAIN_AND_FLIP:
			if( s != bs->bs_dest )  {
				nmg_mv_lu_between_shells( bs->bs_dest, s, lu );
			}
			break;
		}
		lu = lu->next;
	}  while( lu != lu_end );

	/*
	 * Final case:  shell of a single vertexuse
	 */
	if( vu = s->vu_p )  {
		NMG_CK_VERTEXUSE( vu );
		NMG_CK_VERTEX( vu->v_p );
		switch( nmg_action( (genptr_t)vu->v_p, bs ) )  {
		case BACTION_KILL:
			nmg_kvu( vu );
			s->vu_p = (struct vertexuse *)0;	/* sanity */
			break;
		case BACTION_RETAIN:
		case BACTION_RETAIN_AND_FLIP:
			if( s != bs->bs_dest )  {
				nmg_mv_vu_between_shells( bs->bs_dest, s, vu );
				s->vu_p = (struct vertexuse *)0; /* sanity */
			}
			break;
		}
	}
}

/*
 *  Given a pointer to some NMG data structure,
 *  search the 4 classification lists to determine it's classification.
 *  (XXX In the future, this should be done with 1 tagged list
 *   XXX which is sorted, for binary search).
 *  Then, return the action code for an item of that classification.
 */
int
nmg_action( ptr, bs )
genptr_t			ptr;
register struct nmg_bool_state	*bs;
{
	if( bs->bs_isA )  {
		if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_AinB], TBL_LOC, ptr ) >= 0 )
			return( bs->bs_actions[NMG_CLASS_AinB] );
		if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_AonBshared], TBL_LOC, ptr ) >= 0 )
			return( bs->bs_actions[NMG_CLASS_AonBshared] );
		if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_AonBanti], TBL_LOC, ptr ) >= 0 )
			return( bs->bs_actions[NMG_CLASS_AonBanti] );
		if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_AoutB], TBL_LOC, ptr ) >= 0 )
			return( bs->bs_actions[NMG_CLASS_AoutB] );
		rt_log("nmg_action(ptr=x%x) %s has no A classification, retaining\n",
			ptr, nmg_identify_magic( *((long *)ptr) ) );
		return( BACTION_RETAIN );
	}

	/* is B */
	if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_BinA], TBL_LOC, ptr ) >= 0 )
		return( bs->bs_actions[NMG_CLASS_BinA] );
	if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_BonAshared], TBL_LOC, ptr ) >= 0 )
		return( bs->bs_actions[NMG_CLASS_BonAshared] );
	if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_BonAanti], TBL_LOC, ptr ) >= 0 )
		return( bs->bs_actions[NMG_CLASS_BonAanti] );
	if( nmg_tbl( &bs->bs_classtab[NMG_CLASS_BoutA], TBL_LOC, ptr ) >= 0 )
		return( bs->bs_actions[NMG_CLASS_BoutA] );
	rt_log("nmg_action(ptr=x%x) %s has no B classification, retaining\n",
		ptr, nmg_identify_magic( *((long *)ptr) ) );
	return( BACTION_RETAIN );
}

#endif
