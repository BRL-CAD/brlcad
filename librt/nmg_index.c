/*
 *			N M G _ I N D E X . C
 *
 *  Handle counting and re-indexing of NMG data structures.
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
static char RCSnmg_index[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "nmg.h"
#include "raytrace.h"


extern void	nmg_m_reindex();

struct nmg_counter {
	long	max_structs;
	long	regions;
	long	shells;
	long	faces;
	long	face_loops;
	long	face_edges;
	long	face_lone_verts;
	long	wire_loops;
	long	wire_loop_edges;
	long	wire_edges;
	long	lone_verts;
};

#define NMG_HIGH_BIT	0x80000000

/*
 *			N M G _ M _ R E I N D E X
 *
 *  Reassign index numbers to all the data structures in a model.
 *  The model structure will get index 0, all others will be sequentially
 *  assigned after that.
 */
void
nmg_m_reindex( m )
struct model	*m;
{
	struct nmgregion	*r;
	struct shell		*s;
	struct faceuse		*fu;
	struct loopuse		*lu;
	register struct edgeuse		*eu;
	register struct vertexuse	*vu;
	register int		newindex;

	NMG_CK_MODEL(m);
	if( m->index != 0 )  rt_log("nmg_m_reindex() m->index=%d\n", m->index);

	/* First pass:  just set the high bit on all index words */
	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		r->index |= NMG_HIGH_BIT;
		for( NMG_LIST( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			s->index |= NMG_HIGH_BIT;
			/* Faces in shell */
			for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				fu->index |= NMG_HIGH_BIT;
				fu->f_p->index |= NMG_HIGH_BIT;
				/* Loops in face */
				for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					lu->index |= NMG_HIGH_BIT;
					lu->l_p->index |= NMG_HIGH_BIT;
					if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
						/* Loop of Lone vertex */
						vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
						NMG_CK_VERTEXUSE(vu);
						vu->index |= NMG_HIGH_BIT;
						vu->v_p->index |= NMG_HIGH_BIT;
						continue;
					}
					for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
						NMG_CK_EDGEUSE(eu);
						eu->index |= NMG_HIGH_BIT;
						eu->e_p->index |= NMG_HIGH_BIT;
						vu = eu->vu_p;
						NMG_CK_VERTEXUSE(vu);
						vu->index |= NMG_HIGH_BIT;
						vu->v_p->index |= NMG_HIGH_BIT;
					}
				}
			}
			/* Wire loops in shell */
			for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				lu->index |= NMG_HIGH_BIT;
				lu->l_p->index |= NMG_HIGH_BIT;
				if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
					/* Wire loop of Lone vertex */
					vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
					NMG_CK_VERTEXUSE(vu);
					vu->index |= NMG_HIGH_BIT;
					vu->v_p->index |= NMG_HIGH_BIT;
					continue;
				}
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					eu->index |= NMG_HIGH_BIT;
					eu->e_p->index |= NMG_HIGH_BIT;
					vu = eu->vu_p;
					NMG_CK_VERTEXUSE(vu);
					vu->index |= NMG_HIGH_BIT;
					vu->v_p->index |= NMG_HIGH_BIT;
				}
			}
			/* Wire edges in shell */
			for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
				NMG_CK_EDGEUSE(eu);
				eu->index |= NMG_HIGH_BIT;
				eu->e_p->index |= NMG_HIGH_BIT;
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE(vu);
				vu->index |= NMG_HIGH_BIT;
				vu->v_p->index |= NMG_HIGH_BIT;
			}
			/* Lone vertex in shell */
			if( vu = s->vu_p )  {
				NMG_CK_VERTEXUSE(vu);
				vu->index |= NMG_HIGH_BIT;
				vu->v_p->index |= NMG_HIGH_BIT;
			}
		}
	}

#define	NMG_ASSIGN_NEW_INDEX(_p)	\
	{ if( ((_p)->index & NMG_HIGH_BIT) != 0 ) \
		(_p)->index = newindex++; }

	/* Second pass:  assign new index number */
	newindex = 1;	/* model remains index 0 */
	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		NMG_ASSIGN_NEW_INDEX(r);
		for( NMG_LIST( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			NMG_ASSIGN_NEW_INDEX(s);
			/* Faces in shell */
			for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				NMG_ASSIGN_NEW_INDEX(fu);
				NMG_ASSIGN_NEW_INDEX(fu->f_p);
				/* Loops in face */
				for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					NMG_ASSIGN_NEW_INDEX(lu);
					NMG_ASSIGN_NEW_INDEX(lu->l_p);
					if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
						/* Loop of Lone vertex */
						vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
						NMG_CK_VERTEXUSE(vu);
						NMG_ASSIGN_NEW_INDEX(vu);
						NMG_ASSIGN_NEW_INDEX(vu->v_p);
						continue;
					}
					for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
						NMG_CK_EDGEUSE(eu);
						NMG_ASSIGN_NEW_INDEX(eu);
						NMG_ASSIGN_NEW_INDEX(eu->e_p);
						vu = eu->vu_p;
						NMG_CK_VERTEXUSE(vu);
						NMG_ASSIGN_NEW_INDEX(vu);
						NMG_ASSIGN_NEW_INDEX(vu->v_p);
					}
				}
			}
			/* Wire loops in shell */
			for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				NMG_ASSIGN_NEW_INDEX(lu);
				NMG_ASSIGN_NEW_INDEX(lu->l_p);
				if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
					/* Wire loop of Lone vertex */
					vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
					NMG_CK_VERTEXUSE(vu);
					NMG_ASSIGN_NEW_INDEX(vu);
					NMG_ASSIGN_NEW_INDEX(vu->v_p);
					continue;
				}
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					NMG_ASSIGN_NEW_INDEX(eu);
					NMG_ASSIGN_NEW_INDEX(eu->e_p);
					vu = eu->vu_p;
					NMG_CK_VERTEXUSE(vu);
					NMG_ASSIGN_NEW_INDEX(vu);
					NMG_ASSIGN_NEW_INDEX(vu->v_p);
				}
			}
			/* Wire edges in shell */
			for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
				NMG_CK_EDGEUSE(eu);
				NMG_ASSIGN_NEW_INDEX(eu);
				NMG_ASSIGN_NEW_INDEX(eu->e_p);
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE(vu);
				NMG_ASSIGN_NEW_INDEX(vu);
				NMG_ASSIGN_NEW_INDEX(vu->v_p);
			}
			/* Lone vertex in shell */
			if( vu = s->vu_p )  {
				NMG_CK_VERTEXUSE(vu);
				NMG_ASSIGN_NEW_INDEX(vu);
				NMG_ASSIGN_NEW_INDEX(vu->v_p);
			}
		}
	}
#if 0
rt_log("nmg_m_reindex() oldmax=%d, newmax=%d\n", m->maxindex, newindex );
#endif
	m->maxindex = newindex;
}

/*****/

nmg_pr_count( ctr, str )
struct nmg_counter	*ctr;
char			*str;
{
	rt_log("nmg_pr_count(%s)\n", str);
	rt_log("\t%6d max_structs\n", ctr->max_structs);
	rt_log("\t%6d regions\n", ctr->regions);
	rt_log("\t%6d shells\n", ctr->shells);
	rt_log("\t%6d faces\n", ctr->faces);
	rt_log("\t%6d face_loops\n", ctr->face_loops);
	rt_log("\t%6d face_edges\n", ctr->face_edges);
	rt_log("\t%6d face_lone_verts\n", ctr->face_lone_verts);
	rt_log("\t%6d wire_loops\n", ctr->wire_loops);
	rt_log("\t%6d wire_loop_edges\n", ctr->wire_loop_edges);
	rt_log("\t%6d wire_edges\n", ctr->wire_edges);
	rt_log("\t%6d lone_verts\n", ctr->lone_verts);
}

nmg_m_count( ctr, m )
struct model		*m;
struct nmg_counter	*ctr;
{
	struct nmgregion	*r;
	struct shell		*s;
	struct faceuse		*fu;
	struct loopuse		*lu;
	struct edgeuse		*eu;

	NMG_CK_MODEL(m);
	ctr->max_structs = m->maxindex;
	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		ctr->regions++;
		for( NMG_LIST( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			ctr->shells++;
			/* Faces in shell */
			for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				ctr->faces++;
				for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
						ctr->face_lone_verts++;
						continue;
					}
					ctr->face_loops++;
					for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
						NMG_CK_EDGEUSE(eu);
						ctr->face_edges++;
					}
				}
			}
			/* Wire loops in shell */
			for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
					ctr->lone_verts++;
					continue;
				}
				ctr->wire_loops++;
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					ctr->wire_loop_edges++;
				}
			}
			/* Wire edges in shell */
			for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
				NMG_CK_EDGEUSE(eu);
				ctr->wire_edges++;
			}
		}
	}
}
