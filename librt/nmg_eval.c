/*			N M G _ E V A L . C
 *
 *	evaluate the boolean operations on a set of faces in two shells
 */

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"


/*	S U B T R A C T I O N
 *
 *	reshuffle the faces of two shells to perform a subtraction of
 *	one shell from the other.  Shell "sB" disappears and shell "sA"
 *	contains:
 *		the faces that were in sA outside of sB
 *		the faces that were in sB inside of sA (normals flipped)
 */
static void subtraction(sA, AinB, AonB, AoutB, sB, BinA, BonA, BoutA)
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
	vectp_t v;

	(void)nmg_tbl(&faces, TBL_INIT, (long *)NULL);

	/* first we toss out the unwanted faces from both shells */
	p.l = AinB->buffer;
	toss_loops(p.lu, AinB->end);

	p.l = BonA->buffer;
	toss_loops(p.lu, BonA->end);

	p.l = BoutA->buffer;
	toss_loops(p.lu, BoutA->end);

	p.l = BinA->buffer;
	for (i=0 ; i < BinA->end ; ++i) {
		lu = p.lu[i];
		NMG_CK_LOOPUSE(lu);
		fu = lu->up.fu_p;
		NMG_CK_FACEUSE(fu);
		if (nmg_tbl(&faces, TBL_LOC, &fu->magic) < 0) {
			/* move faceuse to new shell */
			if (fu->s_p != sB) {
				rt_bomb("I'm NMG confused\n");
			}

			sB->fu_p = fu;
			DLLRM(sB->fu_p, fu);
			if (sB->fu_p == fu) {
				rt_bomb("Hey, my NMG face mate isn't in this shell\n");
			}
			DLLINS(sA->fu_p, fu);
			fu->s_p = sA;

			/* move the fumate to new shell */
			fu = fu->fumate_p;
			NMG_CK_FACEUSE(fu);
			if (fu->s_p != sB) {
				rt_bomb("NMG shell ran out of faces!\n");
			}

			sB->fu_p = fu;
			DLLRM(sB->fu_p, fu);
			if (sB->fu_p == fu) {
				sB->fu_p = (struct faceuse *)NULL;
			}
			DLLINS(sA->fu_p, fu);
			fu->s_p = sA;


			/* reverse face normal vector */
			NMG_CK_FACE(fu->f_p);
			NMG_CK_FACE_G(fu->f_p->fg_p);
			v = fu->f_p->fg_p->N;
			VREVERSE(v, v);
			v[H] *= -1.0;

			/* switch which face is "outside" face */
			if (fu->orientation == OT_SAME)
				if (fu->fumate_p->orientation != OT_OPPOSITE)
					rt_bomb("NMG fumate has bad orientation\n");
				else {
					fu->orientation = OT_OPPOSITE;
					fu->fumate_p->orientation = OT_SAME;
				}
			else if (fu->orientation == OT_OPPOSITE)
				if (fu->fumate_p->orientation != OT_SAME)
					rt_bomb("NMG fumate has bad orientation\n");
				else {
					fu->orientation = OT_SAME;
					fu->fumate_p->orientation = OT_OPPOSITE;
				}
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
static void addition(sA, AinB, AonB, AoutB, sB, BinA, BonA, BoutA)
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
	toss_loops(p.lu, AinB->end);

	p.l = BinA->buffer;
	toss_loops(p.lu, BinA->end);

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
			/* move faceuse to new shell */
			if (fu->s_p != sB) {
				rt_bomb("I'm NMG confused\n");
			}

			sB->fu_p = fu;
			DLLRM(sB->fu_p, fu);
			if (sB->fu_p == fu) {
				rt_bomb("Hey, my NMG face mate isn't in this shell\n");
			}
			DLLINS(sA->fu_p, fu);
			fu->s_p = sA;

			/* move the fumate to new shell */
			fu = fu->fumate_p;
			NMG_CK_FACEUSE(fu);
			if (fu->s_p != sB) {
				rt_bomb("I'm NMG confused\n");
			}

			sB->fu_p = fu;
			DLLRM(sB->fu_p, fu);
			if (sB->fu_p == fu) {
				sB->fu_p = (struct faceuse *)NULL;
			}
			DLLINS(sA->fu_p, fu);
			fu->s_p = sA;
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
static void intersection(sA, AinB, AonB, AoutB, sB, BinA, BonA, BoutA)
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
	toss_loops(p.lu, AoutB->end);

	p.l = BoutA->buffer;
	toss_loops(p.lu, BoutA->end);

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
			/* move faceuse to new shell */
			if (fu->s_p != sB) {
				rt_bomb("I'm NMG confused\n");
			}

			sB->fu_p = fu;
			DLLRM(sB->fu_p, fu);
			if (sB->fu_p == fu) {
				rt_bomb("Hey, my NMG face mate isn't in this shell\n");
			}
			DLLINS(sA->fu_p, fu);
			fu->s_p = sA;

			/* move the fumate to new shell */
			fu = fu->fumate_p;
			NMG_CK_FACEUSE(fu);
			if (fu->s_p != sB) {
				rt_bomb("I'm NMG confused\n");
			}

			sB->fu_p = fu;
			DLLRM(sB->fu_p, fu);
			if (sB->fu_p == fu) {
				sB->fu_p = (struct faceuse *)NULL;
			}
			DLLINS(sA->fu_p, fu);
			fu->s_p = sA;
		}
	}

	if (sB->fu_p) {
		rt_log("Why does shell B still have faces?\n");
	} else if (!sB->lu_p && !sB->eu_p && !sB->vu_p) {
		nmg_ks(sB);
	}

	(void)nmg_tbl(&faces, TBL_FREE, (long *)NULL);
}
