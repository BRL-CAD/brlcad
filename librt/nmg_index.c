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

#define NMG_HIGH_BIT	0x80000000

#define NMG_MARK_INDEX(_p)	((_p)->index |= NMG_HIGH_BIT)

#define	NMG_ASSIGN_NEW_INDEX(_p)	\
	{ if( ((_p)->index & NMG_HIGH_BIT) != 0 ) \
		(_p)->index = newindex++; }

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
	struct face		*f;
	struct loopuse		*lu;
	struct loop		*l;
	register struct edgeuse		*eu;
	struct edge			*e;
	register struct vertexuse	*vu;
	struct vertex			*v;
	register int		newindex;

	/*
	 *  First pass:  just set the high bit on all index words
	 */
	NMG_CK_MODEL(m);
	if( m->index != 0 )  rt_log("nmg_m_reindex() m->index=%d\n", m->index);

	NMG_MARK_INDEX(m);
	if( m->ma_p )  NMG_MARK_INDEX(m->ma_p);
	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		NMG_MARK_INDEX(r);
		if( r->ra_p )  {
			NMG_CK_REGION_A(r->ra_p);
			NMG_MARK_INDEX(r->ra_p);
		}
		for( NMG_LIST( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			NMG_MARK_INDEX(s);
			if( s->sa_p )  {
				NMG_CK_SHELL_A(s->sa_p);
				NMG_MARK_INDEX(s->sa_p);
			}
			/* Faces in shell */
			for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				NMG_MARK_INDEX(fu);
				if( fu->fua_p )  {
					NMG_CK_FACEUSE_A(fu->fua_p);
					NMG_MARK_INDEX(fu->fua_p);
				}
				f = fu->f_p;
				NMG_CK_FACE(f);
				NMG_MARK_INDEX(f);
				if( f->fg_p )  {
					NMG_CK_FACE_G(f->fg_p);
					NMG_MARK_INDEX(f->fg_p);
				}
				/* Loops in face */
				for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					NMG_MARK_INDEX(lu);
					if( lu->lua_p )  {
						NMG_CK_LOOPUSE_A(lu->lua_p);
						NMG_MARK_INDEX(lu->lua_p);
					}
					l = lu->l_p;
					NMG_CK_LOOP(l);
					NMG_MARK_INDEX(l);
					if( l->lg_p )  {
						NMG_CK_LOOP_G(l->lg_p);
						NMG_MARK_INDEX(l->lg_p);
					}
					if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
						/* Loop of Lone vertex */
						vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
						NMG_CK_VERTEXUSE(vu);
						NMG_MARK_INDEX(vu);
						if(vu->vua_p)  {
							NMG_CK_VERTEXUSE_A(vu->vua_p);
							NMG_MARK_INDEX(vu->vua_p);
						}
						v = vu->v_p;
						NMG_CK_VERTEX(v);
						NMG_MARK_INDEX(v);
						if(v->vg_p)  {
							NMG_CK_VERTEX_G(v->vg_p);
							NMG_MARK_INDEX(v->vg_p);
						}
						continue;
					}
					for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
						NMG_CK_EDGEUSE(eu);
						NMG_MARK_INDEX(eu);
						if(eu->eua_p)  {
							NMG_CK_EDGEUSE_A(eu->eua_p);
							NMG_MARK_INDEX(eu->eua_p);
						}
						e = eu->e_p;
						NMG_CK_EDGE(e);
						NMG_MARK_INDEX(e);
						if(e->eg_p)  {
							NMG_CK_EDGE_G(e->eg_p);
							NMG_MARK_INDEX(e->eg_p);
						}
						vu = eu->vu_p;
						NMG_CK_VERTEXUSE(vu);
						NMG_MARK_INDEX(vu);
						v = vu->v_p;
						NMG_CK_VERTEX(v);
						NMG_MARK_INDEX(v);
						if(v->vg_p)  {
							NMG_CK_VERTEX_G(v->vg_p);
							NMG_MARK_INDEX(v->vg_p);
						}
					}
				}
			}
			/* Wire loops in shell */
			for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				NMG_MARK_INDEX(lu);
				if( lu->lua_p )  {
					NMG_CK_LOOPUSE_A(lu->lua_p);
					NMG_MARK_INDEX(lu->lua_p);
				}
				l = lu->l_p;
				NMG_CK_LOOP(l);
				NMG_MARK_INDEX(l);
				if( l->lg_p )  {
					NMG_CK_LOOP_G(l->lg_p);
					NMG_MARK_INDEX(l->lg_p);
				}
				if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
					/* Wire loop of Lone vertex */
					vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
					NMG_CK_VERTEXUSE(vu);
					NMG_MARK_INDEX(vu);
					if(vu->vua_p)  {
						NMG_CK_VERTEXUSE_A(vu->vua_p);
						NMG_MARK_INDEX(vu->vua_p);
					}
					v = vu->v_p;
					NMG_CK_VERTEX(v);
					NMG_MARK_INDEX(v);
					if(v->vg_p)  {
						NMG_CK_VERTEX_G(v->vg_p);
						NMG_MARK_INDEX(v->vg_p);
					}
					continue;
				}
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					NMG_MARK_INDEX(eu);
					if(eu->eua_p)  {
						NMG_CK_EDGEUSE_A(eu->eua_p);
						NMG_MARK_INDEX(eu->eua_p);
					}
					e = eu->e_p;
					NMG_CK_EDGE(e);
					NMG_MARK_INDEX(e);
					if(e->eg_p)  {
						NMG_CK_EDGE_G(e->eg_p);
						NMG_MARK_INDEX(e->eg_p);
					}
					vu = eu->vu_p;
					NMG_CK_VERTEXUSE(vu);
					NMG_MARK_INDEX(vu);
					v = vu->v_p;
					NMG_CK_VERTEX(v);
					NMG_MARK_INDEX(v);
					if(v->vg_p)  {
						NMG_CK_VERTEX_G(v->vg_p);
						NMG_MARK_INDEX(v->vg_p);
					}
				}
			}
			/* Wire edges in shell */
			for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
				NMG_CK_EDGEUSE(eu);
				NMG_MARK_INDEX(eu);
				if(eu->eua_p)  {
					NMG_CK_EDGEUSE_A(eu->eua_p);
					NMG_MARK_INDEX(eu->eua_p);
				}
				e = eu->e_p;
				NMG_CK_EDGE(e);
				NMG_MARK_INDEX(e);
				if(e->eg_p)  {
					NMG_CK_EDGE_G(e->eg_p);
					NMG_MARK_INDEX(e->eg_p);
				}
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE(vu);
				NMG_MARK_INDEX(vu);
				v = vu->v_p;
				NMG_CK_VERTEX(v);
				NMG_MARK_INDEX(v);
				if(v->vg_p)  {
					NMG_CK_VERTEX_G(v->vg_p);
					NMG_MARK_INDEX(v->vg_p);
				}
			}
			/* Lone vertex in shell */
			if( vu = s->vu_p )  {
				NMG_CK_VERTEXUSE(vu);
				NMG_MARK_INDEX(vu);
				v = vu->v_p;
				NMG_CK_VERTEX(v);
				NMG_MARK_INDEX(v);
				if(v->vg_p)  {
					NMG_CK_VERTEX_G(v->vg_p);
					NMG_MARK_INDEX(v->vg_p);
				}
			}
		}
	}

	/*
	 *  Second pass:  assign new index number
	 */
	newindex = 0;	/* model remains index 0 */

	NMG_ASSIGN_NEW_INDEX(m);
	if( m->ma_p )  NMG_ASSIGN_NEW_INDEX(m->ma_p);
	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		NMG_ASSIGN_NEW_INDEX(r);
		if( r->ra_p )  NMG_ASSIGN_NEW_INDEX(r->ra_p);
		for( NMG_LIST( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			NMG_ASSIGN_NEW_INDEX(s);
			if( s->sa_p )  NMG_ASSIGN_NEW_INDEX(s->sa_p);
			/* Faces in shell */
			for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				NMG_ASSIGN_NEW_INDEX(fu);
				if( fu->fua_p )  NMG_ASSIGN_NEW_INDEX(fu->fua_p);
				f = fu->f_p;
				NMG_CK_FACE(f);
				NMG_ASSIGN_NEW_INDEX(f);
				if( f->fg_p )  NMG_ASSIGN_NEW_INDEX(f->fg_p);
				/* Loops in face */
				for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					NMG_ASSIGN_NEW_INDEX(lu);
					if( lu->lua_p )  NMG_ASSIGN_NEW_INDEX(lu->lua_p);
					l = lu->l_p;
					NMG_CK_LOOP(l);
					NMG_ASSIGN_NEW_INDEX(l);
					if( l->lg_p )  NMG_ASSIGN_NEW_INDEX(l->lg_p);
					if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
						/* Loop of Lone vertex */
						vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
						NMG_CK_VERTEXUSE(vu);
						NMG_ASSIGN_NEW_INDEX(vu);
						if(vu->vua_p)  NMG_ASSIGN_NEW_INDEX(vu->vua_p);
						v = vu->v_p;
						NMG_CK_VERTEX(v);
						NMG_ASSIGN_NEW_INDEX(v);
						if(v->vg_p) NMG_ASSIGN_NEW_INDEX(v->vg_p);
						continue;
					}
					for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
						NMG_CK_EDGEUSE(eu);
						NMG_ASSIGN_NEW_INDEX(eu);
						if(eu->eua_p) NMG_ASSIGN_NEW_INDEX(eu->eua_p);
						e = eu->e_p;
						NMG_CK_EDGE(e);
						NMG_ASSIGN_NEW_INDEX(e);
						if(e->eg_p) NMG_ASSIGN_NEW_INDEX(e->eg_p);
						vu = eu->vu_p;
						NMG_CK_VERTEXUSE(vu);
						NMG_ASSIGN_NEW_INDEX(vu);
						v = vu->v_p;
						NMG_CK_VERTEX(v);
						NMG_ASSIGN_NEW_INDEX(v);
						if(v->vg_p) NMG_ASSIGN_NEW_INDEX(v->vg_p);
					}
				}
			}
			/* Wire loops in shell */
			for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				NMG_ASSIGN_NEW_INDEX(lu);
				if( lu->lua_p )  NMG_ASSIGN_NEW_INDEX(lu->lua_p);
				l = lu->l_p;
				NMG_CK_LOOP(l);
				NMG_ASSIGN_NEW_INDEX(l);
				if( l->lg_p )  NMG_ASSIGN_NEW_INDEX(l->lg_p);
				if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
					/* Wire loop of Lone vertex */
					vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
					NMG_CK_VERTEXUSE(vu);
					NMG_ASSIGN_NEW_INDEX(vu);
					if(vu->vua_p)  NMG_ASSIGN_NEW_INDEX(vu->vua_p);
					v = vu->v_p;
					NMG_CK_VERTEX(v);
					NMG_ASSIGN_NEW_INDEX(v);
					if(v->vg_p) NMG_ASSIGN_NEW_INDEX(v->vg_p);
					continue;
				}
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					NMG_ASSIGN_NEW_INDEX(eu);
					if(eu->eua_p) NMG_ASSIGN_NEW_INDEX(eu->eua_p);
					e = eu->e_p;
					NMG_CK_EDGE(e);
					NMG_ASSIGN_NEW_INDEX(e);
					if(e->eg_p) NMG_ASSIGN_NEW_INDEX(e->eg_p);
					vu = eu->vu_p;
					NMG_CK_VERTEXUSE(vu);
					NMG_ASSIGN_NEW_INDEX(vu);
					v = vu->v_p;
					NMG_CK_VERTEX(v);
					NMG_ASSIGN_NEW_INDEX(v);
					if(v->vg_p) NMG_ASSIGN_NEW_INDEX(v->vg_p);
				}
			}
			/* Wire edges in shell */
			for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
				NMG_CK_EDGEUSE(eu);
				NMG_ASSIGN_NEW_INDEX(eu);
				if(eu->eua_p) NMG_ASSIGN_NEW_INDEX(eu->eua_p);
				e = eu->e_p;
				NMG_CK_EDGE(e);
				NMG_ASSIGN_NEW_INDEX(e);
				if(e->eg_p) NMG_ASSIGN_NEW_INDEX(e->eg_p);
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE(vu);
				NMG_ASSIGN_NEW_INDEX(vu);
				v = vu->v_p;
				NMG_CK_VERTEX(v);
				NMG_ASSIGN_NEW_INDEX(v);
				if(v->vg_p) NMG_ASSIGN_NEW_INDEX(v->vg_p);
			}
			/* Lone vertex in shell */
			if( vu = s->vu_p )  {
				NMG_CK_VERTEXUSE(vu);
				NMG_ASSIGN_NEW_INDEX(vu);
				v = vu->v_p;
				NMG_CK_VERTEX(v);
				NMG_ASSIGN_NEW_INDEX(v);
				if(v->vg_p) NMG_ASSIGN_NEW_INDEX(v->vg_p);
			}
		}
	}

#if 1
rt_log("nmg_m_reindex() oldmax=%d, newmax=%d\n", m->maxindex, newindex );
#endif
	m->maxindex = newindex;
}

/*****/

struct nmg_counter {
	/* Actual structure counts */
	long	model;
	long	model_a;
	long	region;
	long	region_a;
	long	shell;
	long	shell_a;
	long	face;
	long	face_g;
	long	faceuse;
	long	faceuse_a;
	long	loopuse;
	long	loopuse_a;
	long	loop;
	long	loop_g;
	long	edgeuse;
	long	edgeuse_a;
	long	edge;
	long	edge_g;
	long	vertexuse;
	long	vertexuse_a;
	long	vertex;
	long	vertex_g;
	/* Abstractions */
	long	max_structs;
	long	face_loops;
	long	face_edges;
	long	face_lone_verts;
	long	wire_loops;
	long	wire_loop_edges;
	long	wire_edges;
	long	wire_lone_verts;
	long	shells_of_lone_vert;
};

nmg_pr_count( ctr, str )
struct nmg_counter	*ctr;
char			*str;
{
	rt_log("nmg_pr_count(%s)\n", str);
	rt_log(" Actual structure counts:\n");
	rt_log("\t%6d model\n", ctr->model);
	rt_log("\t%6d model_a\n", ctr->model_a);
	rt_log("\t%6d region\n", ctr->region);
	rt_log("\t%6d region_a\n", ctr->region_a);
	rt_log("\t%6d shell\n", ctr->shell);
	rt_log("\t%6d shell_a\n", ctr->shell_a);
	rt_log("\t%6d face\n", ctr->face);
	rt_log("\t%6d face_g\n", ctr->face_g);
	rt_log("\t%6d faceuse\n", ctr->faceuse);
	rt_log("\t%6d faceuse_a\n", ctr->faceuse_a);
	rt_log("\t%6d loopuse\n", ctr->loopuse);
	rt_log("\t%6d loopuse_a\n", ctr->loopuse_a);
	rt_log("\t%6d loop\n", ctr->loop);
	rt_log("\t%6d loop_g\n", ctr->loop_g);
	rt_log("\t%6d edgeuse\n", ctr->edgeuse);
	rt_log("\t%6d edgeuse_a\n", ctr->edgeuse_a);
	rt_log("\t%6d edge\n", ctr->edge);
	rt_log("\t%6d edge_g\n", ctr->edge_g);
	rt_log("\t%6d vertexuse\n", ctr->vertexuse);
	rt_log("\t%6d vertexuse_a\n", ctr->vertexuse_a);
	rt_log("\t%6d vertex\n", ctr->vertex);
	rt_log("\t%6d vertex_g\n", ctr->vertex_g);
	rt_log(" Abstractions:\n");
	rt_log("\t%6d max_structs\n", ctr->max_structs);
	rt_log("\t%6d face_loops\n", ctr->face_loops);
	rt_log("\t%6d face_edges\n", ctr->face_edges);
	rt_log("\t%6d face_lone_verts\n", ctr->face_lone_verts);
	rt_log("\t%6d wire_loops\n", ctr->wire_loops);
	rt_log("\t%6d wire_loop_edges\n", ctr->wire_loop_edges);
	rt_log("\t%6d wire_edges\n", ctr->wire_edges);
	rt_log("\t%6d wire_lone_verts\n", ctr->wire_lone_verts);
	rt_log("\t%6d shells_of_lone_vert\n", ctr->shells_of_lone_vert);
}

nmg_m_count( ctr, m )
struct model		*m;
struct nmg_counter	*ctr;
{
	struct nmgregion	*r;
	struct shell		*s;
	struct faceuse		*fu;
	struct face		*f;
	struct loopuse		*lu;
	struct loop		*l;
	struct edgeuse		*eu;
	struct edge		*e;
	struct vertexuse	*vu;
	struct vertex		*v;
	long			*flags;

#define NMG_UNIQ_INDEX(_p,_type)	\
	if( flags[(_p)->index] == 0 )  { \
		flags[(_p)->index] = 1; \
		ctr->_type++; \
	}

	flags = (long *)rt_calloc( m->maxindex, sizeof(long), "nmg_m_count flags[]");

	NMG_CK_MODEL(m);
	NMG_UNIQ_INDEX(m, model);
	if(m->ma_p)  {
		NMG_CK_MODEL_A(m->ma_p);
		NMG_UNIQ_INDEX(m->ma_p, model_a);
	}
	ctr->max_structs = m->maxindex;
	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		NMG_UNIQ_INDEX(r, region);
		if(r->ra_p)  {
			NMG_CK_REGION_A(r->ra_p);
			NMG_UNIQ_INDEX(r->ra_p, region_a);
		}
		for( NMG_LIST( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);
			NMG_UNIQ_INDEX(s, shell);
			if(s->sa_p)  {
				NMG_CK_SHELL_A(s->sa_p);
				NMG_UNIQ_INDEX(s->sa_p, shell_a);
			}
			/* Faces in shell */
			for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				NMG_UNIQ_INDEX(fu, faceuse);
				if( fu->fua_p )  {
					NMG_CK_FACEUSE_A(fu->fua_p);
					NMG_UNIQ_INDEX(fu->fua_p, faceuse_a);
				}
				f = fu->f_p;
				NMG_CK_FACE(f);
				NMG_UNIQ_INDEX(f, face);
				if( f->fg_p )  {
					NMG_CK_FACE_G(f->fg_p);
					NMG_UNIQ_INDEX(f->fg_p, face_g);
				}
				/* Loops in face */
				for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					NMG_UNIQ_INDEX(lu, loopuse);
					if( lu->lua_p )  {
						NMG_CK_LOOPUSE_A(lu->lua_p);
						NMG_UNIQ_INDEX(lu->lua_p, loopuse_a);
					}
					l = lu->l_p;
					NMG_CK_LOOP(l);
					NMG_UNIQ_INDEX(l, loop);
					if( l->lg_p )  {
						NMG_CK_LOOP_G(l->lg_p);
						NMG_UNIQ_INDEX(l->lg_p, loop_g);
					}
					if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
						/* Loop of Lone vertex */
						ctr->face_lone_verts++;
						NMG_CK_VERTEXUSE(vu);
						NMG_UNIQ_INDEX(vu, vertexuse);
						if(vu->vua_p)  {
							NMG_CK_VERTEXUSE_A(vu->vua_p);
							NMG_UNIQ_INDEX(vu->vua_p, vertexuse_a);
						}
						v = vu->v_p;
						NMG_CK_VERTEX(v);
						NMG_UNIQ_INDEX(v, vertex);
						if(v->vg_p)  {
							NMG_CK_VERTEX_G(v->vg_p);
							NMG_UNIQ_INDEX(v->vg_p, vertex_g);
						}
						continue;
					}
					ctr->face_loops++;
					for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
						ctr->face_edges++;
						NMG_CK_EDGEUSE(eu);
						NMG_UNIQ_INDEX(eu, edgeuse);
						if(eu->eua_p)  {
							NMG_CK_EDGEUSE_A(eu->eua_p);
							NMG_UNIQ_INDEX(eu->eua_p, edgeuse_a);
						}
						e = eu->e_p;
						NMG_CK_EDGE(e);
						NMG_UNIQ_INDEX(e, edge);
						if(e->eg_p)  {
							NMG_CK_EDGE_G(e->eg_p);
							NMG_UNIQ_INDEX(e->eg_p, edge_g);
						}
						vu = eu->vu_p;
						NMG_CK_VERTEXUSE(vu);
						NMG_UNIQ_INDEX(vu, vertexuse);
						v = vu->v_p;
						NMG_CK_VERTEX(v);
						NMG_UNIQ_INDEX(v, vertex);
						if(v->vg_p)  {
							NMG_CK_VERTEX_G(v->vg_p);
							NMG_UNIQ_INDEX(v->vg_p, vertex_g);
						}
					}
				}
			}
			/* Wire loops in shell */
			for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				NMG_UNIQ_INDEX(lu, loopuse);
				if( lu->lua_p )  {
					NMG_CK_LOOPUSE_A(lu->lua_p);
					NMG_UNIQ_INDEX(lu->lua_p, loopuse_a);
				}
				l = lu->l_p;
				NMG_CK_LOOP(l);
				NMG_UNIQ_INDEX(l, loop);
				if( l->lg_p )  {
					NMG_CK_LOOP_G(l->lg_p);
					NMG_UNIQ_INDEX(l->lg_p, loop_g);
				}
				if( NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
					ctr->wire_lone_verts++;
					/* Wire loop of Lone vertex */
					vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
					NMG_CK_VERTEXUSE(vu);
					NMG_UNIQ_INDEX(vu, vertexuse);
					if(vu->vua_p)  {
						NMG_CK_VERTEXUSE_A(vu->vua_p);
						NMG_UNIQ_INDEX(vu->vua_p, vertexuse_a);
					}
					v = vu->v_p;
					NMG_CK_VERTEX(v);
					NMG_UNIQ_INDEX(v, vertex);
					if(v->vg_p)  {
						NMG_CK_VERTEX_G(v->vg_p);
						NMG_UNIQ_INDEX(v->vg_p, vertex_g);
					}
					continue;
				}
				ctr->wire_loops++;
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					NMG_CK_EDGEUSE(eu);
					NMG_UNIQ_INDEX(eu, edgeuse);
					if(eu->eua_p)  {
						NMG_CK_EDGEUSE_A(eu->eua_p);
						NMG_UNIQ_INDEX(eu->eua_p, edgeuse_a);
					}
					e = eu->e_p;
					NMG_CK_EDGE(e);
					NMG_UNIQ_INDEX(e, edge);
					if(e->eg_p)  {
						NMG_CK_EDGE_G(e->eg_p);
						NMG_UNIQ_INDEX(e->eg_p, edge_g);
					}
					vu = eu->vu_p;
					NMG_CK_VERTEXUSE(vu);
					NMG_UNIQ_INDEX(vu, vertexuse);
					v = vu->v_p;
					NMG_CK_VERTEX(v);
					NMG_UNIQ_INDEX(v, vertex);
					if(v->vg_p)  {
						NMG_CK_VERTEX_G(v->vg_p);
						NMG_UNIQ_INDEX(v->vg_p, vertex_g);
					}
					ctr->wire_loop_edges++;
				}
			}
			/* Wire edges in shell */
			for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
				NMG_CK_EDGEUSE(eu);
				ctr->wire_edges++;
				NMG_UNIQ_INDEX(eu, edgeuse);
				if(eu->eua_p)  {
					NMG_CK_EDGEUSE_A(eu->eua_p);
					NMG_UNIQ_INDEX(eu->eua_p, edgeuse_a);
				}
				e = eu->e_p;
				NMG_CK_EDGE(e);
				NMG_UNIQ_INDEX(e, edge);
				if(e->eg_p)  {
					NMG_CK_EDGE_G(e->eg_p);
					NMG_UNIQ_INDEX(e->eg_p, edge_g);
				}
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE(vu);
				NMG_UNIQ_INDEX(vu, vertexuse);
				v = vu->v_p;
				NMG_CK_VERTEX(v);
				NMG_UNIQ_INDEX(v, vertex);
				if(v->vg_p)  {
					NMG_CK_VERTEX_G(v->vg_p);
					NMG_UNIQ_INDEX(v->vg_p, vertex_g);
				}
			}
			/* Lone vertex in shell */
			if( vu = s->vu_p )  {
				ctr->shells_of_lone_vert++;
				NMG_CK_VERTEXUSE(vu);
				NMG_UNIQ_INDEX(vu, vertexuse);
				v = vu->v_p;
				NMG_CK_VERTEX(v);
				NMG_UNIQ_INDEX(v, vertex);
				if(v->vg_p)  {
					NMG_CK_VERTEX_G(v->vg_p);
					NMG_UNIQ_INDEX(v->vg_p, vertex_g);
				}
			}
		}
	}
	rt_free( (char *)flags, "flags[]" );
}
