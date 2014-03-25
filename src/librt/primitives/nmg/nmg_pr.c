/*                        N M G _ P R . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nmg */
/** @{ */
/** @file primitives/nmg/nmg_pr.c
 *
 * Contains routines to print or describe NMG data structures.
 * These routines are always available (not conditionally compiled)
 * so that NMG programmers can always format and print
 * their data structures.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"


static char nmg_pr_padstr[128];


/**
 * Convert orientation code to string.
 */
char *
nmg_orientation(int orientation)
{
    switch (orientation) {
	case OT_SAME:
	    return "OT_SAME";
	case OT_OPPOSITE:
	    return "OT_OPPOSITE";
	case OT_NONE:
	    return "OT_NONE";
	case OT_UNSPEC:
	    return "OT_UNSPEC";
	case OT_BOOLPLACE:
	    return "OT_BOOLPLACE";
    }
    return "OT_IS_BOGUS!!";
}


/**
 * Print the orientation in a nice, English form
 */
void
nmg_pr_orient(int orientation, const char *h)
{
    switch (orientation) {
	case OT_SAME : bu_log("%s%8s orientation\n", h, "SAME"); break;
	case OT_OPPOSITE : bu_log("%s%8s orientation\n", h, "OPPOSITE"); break;
	case OT_NONE : bu_log("%s%8s orientation\n", h, "NONE"); break;
	case OT_UNSPEC : bu_log("%s%8s orientation\n", h, "UNSPEC"); break;
	case OT_BOOLPLACE : bu_log("%s%8s orientation\n", h, "BOOLPLACE"); break;
	default : bu_log("%s%8s orientation\n", h, "BOGUS!!!"); break;
    }
}


void
nmg_pr_m(const struct model *m)
{
    const struct nmgregion *r;

    bu_log("MODEL %p\n", (void *)m);
    if (!m || m->magic != NMG_MODEL_MAGIC) {
	bu_log("bad model magic\n");
	return;
    }
    bu_log("%ld maxindex\n", m->maxindex);

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	nmg_pr_r(r, (char *)NULL);
    }
}


/**
 * NOTE:  All the nmg_pr_*() routines take an "h" (header string) pointer.
 * This can be an arbitrary caller-provided string, as long as it is kept
 * short.  The string will be copied over into nmg_pr_padstr[], and
 * "h" will be changed to point there, so spaces can be added to the end.
 */
#define MKPAD(_h) { \
	if (!_h) { _h = nmg_pr_padstr; nmg_pr_padstr[0] = '\0'; } \
	else if ((_h) < nmg_pr_padstr || (_h) >= nmg_pr_padstr+sizeof(nmg_pr_padstr)) { \
		bu_strlcpy(nmg_pr_padstr, (_h), sizeof(nmg_pr_padstr)/2); \
		_h = nmg_pr_padstr; \
	} else { if (strlen(_h) < sizeof(nmg_pr_padstr)-4) bu_strlcat(_h, "   ", sizeof(nmg_pr_padstr)); } }

#define Return { h[strlen(h)-3] = '\0'; return; }

void
nmg_pr_r(const struct nmgregion *r, char *h)
{
    struct shell *s;

    bu_log("REGION %p\n", (void *)r);

    MKPAD(h);

    if (!r || r->l.magic != NMG_REGION_MAGIC) {
	bu_log("bad region magic\n");
	Return;
    }

    bu_log("%p m_p\n", (void *)r->m_p);
    bu_log("%p l.forw\n", (void *)r->l.forw);
    bu_log("%p l.back\n", (void *)r->l.back);
    bu_log("%p ra_p\n", (void *)r->ra_p);

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	nmg_pr_s(s, h);
    }
    Return;
}


void
nmg_pr_sa(const struct shell_a *sa, char *h)
{
    MKPAD(h);

    bu_log("%sSHELL_A %p\n", h, (void *)sa);
    if (!sa || sa->magic != NMG_SHELL_A_MAGIC) {
	bu_log("bad shell_a magic\n");
	Return;
    }

    bu_log("%s%f %f %f Min\n", h, sa->min_pt[X], sa->min_pt[Y],
	   sa->min_pt[Z]);
    bu_log("%s%f %f %f Max\n", h, sa->max_pt[X], sa->max_pt[Y],
	   sa->max_pt[Z]);

    Return;
}


void
nmg_pr_lg(const struct loop_g *lg, char *h)
{
    MKPAD(h);
    NMG_CK_LOOP_G(lg);

    bu_log("%sLOOP_G %p\n", h, (void *)lg);
    bu_log("%s%f %f %f Min\n", h, lg->min_pt[X], lg->min_pt[Y],
	   lg->min_pt[Z]);
    bu_log("%s%f %f %f Max\n", h, lg->max_pt[X], lg->max_pt[Y],
	   lg->max_pt[Z]);

    Return;
}


void
nmg_pr_fg(const uint32_t *magic, char *h)
{
    const struct face_g_snurb *fgs;
    int ncoords;
    int i, j, k, l;

    MKPAD(h);

    switch (*magic) {
	case NMG_FACE_G_PLANE_MAGIC:
	    bu_log("%sFACE_G_PLANE %p\n", h, (void *)magic);

	    bu_log("%s%fX + %fY + %fZ = %f\n", h,
		   V4ARGS(((struct face_g_plane *)magic)->N));
	    break;
	case NMG_FACE_G_SNURB_MAGIC:
	    fgs = (struct face_g_snurb *)magic;
	    bu_log("%sFACE_G_SNURB %p\n", h, (void *)magic);
	    bu_log("%s  order (%d, %d)\n", h, fgs->order[0], fgs->order[1]);
	    bu_log("%s  U knots: size=%d", h, fgs->u.k_size);
	    for (i=0; i<fgs->u.k_size; i++)
		bu_log(" %f", fgs->u.knots[i]);
	    bu_log("\n%s  V knots: size=%d", h, fgs->v.k_size);
	    for (i=0; i<fgs->v.k_size; i++)
		bu_log(" %f", fgs->v.knots[i]);
	    bu_log("\n%s  Mesh size = (%d X %d), pt_type = %d\n", h, fgs->s_size[0], fgs->s_size[1], fgs->pt_type);
	    ncoords = RT_NURB_EXTRACT_COORDS(fgs->pt_type);
	    l = 0;
	    for (i=0; i<fgs->s_size[0]; i++) {
		bu_log("%s  ", h);
		for (j=0; j<fgs->s_size[1]; j++) {
		    bu_log(" (%s ", h);
		    for (k=0; k<ncoords; k++)
			bu_log("%f ", fgs->ctl_points[l+k]);
		    bu_log(")");
		    l += ncoords;
		}
		bu_log("\n");
	    }
	    break;
	default:
	    bu_bomb("nmg_pr_fg() bad magic\n");
    }

    Return;
}


void
nmg_pr_s(const struct shell *s, char *h)
{
    const struct faceuse *fu;
    const struct loopuse *lu;
    const struct edgeuse *eu;

    MKPAD(h);

    bu_log("%sSHELL %p\n", h, (void *)s);
    if (!s || s->l.magic != NMG_SHELL_MAGIC) {
	bu_log("bad shell magic\n");
	Return;
    }

    bu_log("%s%p r_p\n", h, (void *)s->r_p);
    bu_log("%s%p l.forw\n", h, (void *)s->l.forw);
    bu_log("%s%p l.back\n", h, (void *)s->l.back);
    bu_log("%s%p sa_p\n", h, (void *)s->sa_p);
    if (s->sa_p)
	nmg_pr_sa(s->sa_p, h);

    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	nmg_pr_fu(fu, h);
    }

    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	nmg_pr_lu(lu, h);
    }

    for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
	nmg_pr_eu(eu, h);
    }
    if (s->vu_p)
	nmg_pr_vu(s->vu_p, h);

    Return;
}


void
nmg_pr_s_briefly(const struct shell *s, char *h)
{
    const struct faceuse *fu;
    const struct loopuse *lu;
    const struct edgeuse *eu;

    MKPAD(h);

    bu_log("%sSHELL %p\n", h, (void *)s);
    if (!s || s->l.magic != NMG_SHELL_MAGIC) {
	bu_log("bad shell magic\n");
	Return;
    }

    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	nmg_pr_fu_briefly(fu, h);
    }

    for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	nmg_pr_lu_briefly(lu, h);
    }

    for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
	nmg_pr_eu_briefly(eu, h);
    }
    if (s->vu_p)
	nmg_pr_vu_briefly(s->vu_p, h);

    Return;
}


void
nmg_pr_f(const struct face *f, char *h)
{
    MKPAD(h);
    NMG_CK_FACE(f);

    bu_log("%sFACE %p\n", h, (void *)f);
    bu_log("%s%p fu_p\n", h, (void *)f->fu_p);
    bu_log("%s%p g.magic_p\n", h, (void *)f->g.magic_p);

    bu_log("%s%f %f %f Min\n", h, f->min_pt[X], f->min_pt[Y],
	   f->min_pt[Z]);
    bu_log("%s%f %f %f Max\n", h, f->max_pt[X], f->max_pt[Y],
	   f->max_pt[Z]);

    bu_log("%s%d Flip\n", h, f->flip);

    if (f->g.plane_p)
	nmg_pr_fg(f->g.magic_p, h);

    Return;
}


void
nmg_pr_fu(const struct faceuse *fu, char *h)
{
    const struct loopuse *lu;

    MKPAD(h);
    NMG_CK_FACEUSE(fu);

    bu_log("%sFACEUSE %p\n", h, (void *)fu);

    if (!fu || fu->l.magic != NMG_FACEUSE_MAGIC) {
	bu_log("bad faceuse magic\n");
	Return;
    }

    bu_log("%s%p s_p\n", h, (void *)fu->s_p);
    bu_log("%s%p l.forw\n", h, (void *)fu->l.forw);
    bu_log("%s%p l.back\n", h, (void *)fu->l.back);
    bu_log("%s%p fumate_p\n", h, (void *)fu->fumate_p);
    nmg_pr_orient(fu->orientation, h);

    bu_log("%s%p f_p\n", h, (void *)fu->f_p);
    if (fu->f_p)
	nmg_pr_f(fu->f_p, h);

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	nmg_pr_lu(lu, h);
    }
    Return;
}


void
nmg_pr_fu_briefly(const struct faceuse *fu, char *h)
{
    const struct loopuse *lu;
    struct face *f;

    MKPAD(h);
    NMG_CK_FACEUSE(fu);
    f = fu->f_p;

    if (f->g.magic_p && *f->g.magic_p == NMG_FACE_G_PLANE_MAGIC) {
	bu_log("%sFACEUSE %p (%s) f_p=%p fg=%p flip=%d pl= %f %f %f %f\n",
	       h,
	       (void *)fu, nmg_orientation(fu->orientation),
	       (void *)fu->f_p, (void *)f->g.magic_p, fu->f_p->flip, V4ARGS(f->g.plane_p->N));
    } else
	bu_log("%sFACEUSE %p (%s) f_p=%p\n",
	       h, (void *)fu, nmg_orientation(fu->orientation), (void *)fu->f_p);

    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	nmg_pr_lu_briefly(lu, h);
    }
    Return;
}


void
nmg_pr_l(const struct loop *l, char *h)
{
    MKPAD(h);
    NMG_CK_LOOP(l);

    bu_log("%sLOOP %p\n", h, (void *)l);
    if (!l || l->magic != NMG_LOOP_MAGIC) {
	bu_log("bad loop magic\n");
	Return;
    }
    bu_log("%s%p lu_p\n", h, (void *)l->lu_p);
    bu_log("%s%p lg_p\n", h, (void *)l->lg_p);
    if (l->lg_p)
	nmg_pr_lg(l->lg_p, h);

    Return;
}


void
nmg_pr_lu(const struct loopuse *lu, char *h)
{
    const struct edgeuse *eu;
    const struct vertexuse *vu;
    uint32_t magic1;

    MKPAD(h);
    NMG_CK_LOOPUSE(lu);

    bu_log("%sLOOPUSE %p\n", h, (void *)lu);

    switch (*lu->up.magic_p) {
	case NMG_SHELL_MAGIC	: bu_log("%s%p up.s_p\n", h, (void *)lu->up.s_p);
	    break;
	case NMG_FACEUSE_MAGIC	: bu_log("%s%p up.fu_p\n", h, (void *)lu->up.fu_p);
	    break;
	default			: bu_log("Bad loopuse parent magic\n");
	    Return;
    }

    bu_log("%s%p l.forw\n", h, (void *)lu->l.forw);
    bu_log("%s%p l.back\n", h, (void *)lu->l.back);
    bu_log("%s%p lumate_p\n", h, (void *)lu->lumate_p);
    nmg_pr_orient(lu->orientation, h);
    bu_log("%s%p l_p\n", h, (void *)lu->l_p);
    if (lu->l_p)
	nmg_pr_l(lu->l_p, h);


    bu_log("%s%8x down_hd.magic\n", h, lu->down_hd.magic);
    bu_log("%s%p down_hd.forw\n", h, (void *)lu->down_hd.forw);
    bu_log("%s%p down_hd.back\n", h, (void *)lu->down_hd.back);

    magic1 = BU_LIST_FIRST_MAGIC(&lu->down_hd);
    if (magic1 == NMG_VERTEXUSE_MAGIC) {
	vu = BU_LIST_PNEXT(vertexuse, &lu->down_hd);
	bu_log("%s%p down_hd->forw (vu)\n", h, (void *)vu);
	nmg_pr_vu(vu, h);
    } else if (magic1 == NMG_EDGEUSE_MAGIC) {
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    nmg_pr_eu(eu, h);
	}
    } else
	bu_log("bad loopuse child magic\n");

    Return;
}


void
nmg_pr_lu_briefly(const struct loopuse *lu, char *h)
{
    const struct edgeuse *eu;
    const struct vertexuse *vu;
    uint32_t magic1;

    MKPAD(h);
    NMG_CK_LOOPUSE(lu);

    bu_log("%sLOOPUSE %p, lumate_p=%p (%s) \n",
	   h, (void *)lu, (void *)lu->lumate_p, nmg_orientation(lu->orientation));

    magic1 = BU_LIST_FIRST_MAGIC(&lu->down_hd);
    if (magic1 == NMG_VERTEXUSE_MAGIC) {
	vu = BU_LIST_PNEXT(vertexuse, &lu->down_hd);
	bu_log("%s%p down_hd->forw (vu)\n", h, (void *)vu);
	nmg_pr_vu_briefly(vu, h);
    } else if (magic1 == NMG_EDGEUSE_MAGIC) {
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    nmg_pr_eu_briefly(eu, h);
	}
    } else
	bu_log("bad loopuse child magic\n");

    Return;
}


/**
 * Expects a pointer to the magic number of an edge geometry structure
 * either edge_g_lseg or edge_g_cnurb structures.
 */
void
nmg_pr_eg(const uint32_t *eg_magic_p, char *h)
{
    MKPAD(h);
    NMG_CK_EDGE_G_EITHER(eg_magic_p);

    switch (*eg_magic_p) {
	case NMG_EDGE_G_LSEG_MAGIC: {
	    struct edge_g_lseg *eg_l=(struct edge_g_lseg *)eg_magic_p;

	    bu_log("%sEDGE_G_LSEG %p pt:(%f %f %f)\n",
		   h, (void *)eg_l, V3ARGS(eg_l->e_pt));
	    bu_log("%s       eu uses=%d  dir:(%f %f %f)\n",
		   h, bu_list_len(&eg_l->eu_hd2), V3ARGS(eg_l->e_dir));
	    break;
	}
	case NMG_EDGE_G_CNURB_MAGIC: {
	    struct edge_g_cnurb *eg_c=(struct edge_g_cnurb *)eg_magic_p;
	    int i, j, l;
	    int ncoords;

	    bu_log("%sEDGE_G_CNURB %p\n", h, (void *)eg_c);
	    bu_log("%s  order=%d, %d ctl pts, pt_type=%d\n",
		   h, eg_c->order, eg_c->c_size, eg_c->pt_type);
	    bu_log("%s  knot vector (len=%d):", h, eg_c->k.k_size);
	    for (i = 0; i < eg_c->k.k_size; i++)
		bu_log(" %f", eg_c->k.knots[i]);
	    bu_log("\n");
	    ncoords = RT_NURB_EXTRACT_COORDS(eg_c->pt_type);
	    l = 0;
	    bu_log("%s  control points: ", h);
	    for (i = 0; i < eg_c->c_size; i++) {
		bu_log("(");
		for (j = 0; j < ncoords; j++)
		    bu_log("%f ", eg_c->ctl_points[l+j]);
		bu_log(")");
		l += ncoords;
	    }
	    bu_log("\n");
	    break;
	}
    }

    Return;
}


void
nmg_pr_e(const struct edge *e, char *h)
{
    MKPAD(h);
    NMG_CK_EDGE(e);

    bu_log("%sEDGE %p\n", h, (void *)e);
    if (!e || e->magic != NMG_EDGE_MAGIC) {
	bu_log("bad edge magic\n");
	Return;
    }
    bu_log("%s%p eu_p\n", h, (void *)e->eu_p);

    Return;
}


void
nmg_pr_eu(const struct edgeuse *eu, char *h)
{
    MKPAD(h);
    NMG_CK_EDGEUSE(eu);

    bu_log("%sEDGEUSE %p\n", h, (void *)eu);

    switch (*eu->up.magic_p) {
	case NMG_SHELL_MAGIC	: bu_log("%s%p up.s_p\n", h, (void *)eu->up.s_p);
	    break;
	case NMG_LOOPUSE_MAGIC	: bu_log("%s%p up.lu_p\n", h, (void *)eu->up.lu_p);
	    break;
	default			: bu_log("bad edgeuse parent magic\n");
	    Return;
    }
    bu_log("%s%p l.forw\n", h, (void *)eu->l.forw);
    bu_log("%s%p l.back\n", h, (void *)eu->l.back);
    bu_log("%s%p eumate_p\n", h, (void *)eu->eumate_p);
    bu_log("%s%p radial_p\n", h, (void *)eu->radial_p);
    nmg_pr_orient(eu->orientation, h);
    bu_log("%s%p e_p\n", h, (void *)eu->e_p);
    bu_log("%s%p vu_p\n", h, (void *)eu->vu_p);
    bu_log("%s%p g.magic_p\n", h, (void *)eu->g.magic_p);
    nmg_pr_e(eu->e_p, h);
    nmg_pr_vu(eu->vu_p, h);

    if (eu->g.magic_p)
	nmg_pr_eg(eu->g.magic_p, h);

    Return;
}


void
nmg_pr_eu_briefly(const struct edgeuse *eu, char *h)
{
    MKPAD(h);
    NMG_CK_EDGEUSE(eu);

    bu_log("%sEDGEUSE %p, g=%p, e_p=%p\n",
	   h, (void *)eu, (void *)eu->g.magic_p, (void *)eu->e_p);
    nmg_pr_vu_briefly(eu->vu_p, h);

    Return;
}


void
nmg_pr_eu_endpoints(const struct edgeuse *eu, char *h)
{
    struct vertex_g *vg1, *vg2;

    MKPAD(h);
    NMG_CK_EDGEUSE(eu);

    vg1 = eu->vu_p->v_p->vg_p;
    vg2 = eu->eumate_p->vu_p->v_p->vg_p;
    NMG_CK_VERTEX_G(vg1);
    NMG_CK_VERTEX_G(vg2);

    bu_log("%sEDGEUSE %p\n%s  (%g, %g, %g) -- (%g, %g, %g)\n",
	   h, (void *)eu, h,
	   V3ARGS(vg1->coord),
	   V3ARGS(vg2->coord));

    Return;
}


void
nmg_pr_vg(const struct vertex_g *vg, char *h)
{
    MKPAD(h);
    NMG_CK_VERTEX_G(vg);

    if (!vg || vg->magic != NMG_VERTEX_G_MAGIC) {
	bu_log("%sVERTEX_G %p\n", h, (void *)vg);
	bu_log("bad vertex_g magic\n");
	Return;
    }
    bu_log("%sVERTEX_G %p %f %f %f = XYZ coord\n",
	   h, (void *)vg, V3ARGS(vg->coord));
    Return;
}


void
nmg_pr_v(const struct vertex *v, char *h)
{
    MKPAD(h);
    NMG_CK_VERTEX(v);

    bu_log("%sVERTEX %p\n", h, (void *)v);
    if (!v || v->magic != NMG_VERTEX_MAGIC) {
	bu_log("bad vertex magic\n");
	Return;
    }
    /* vu_hd ? */
    bu_log("%s   vu_hd %p\n", h, (void *)&v->vu_hd);
    bu_log("%s%p vu_hd.forw\n", h, (void *)v->vu_hd.forw);
    bu_log("%s%p vu_hd.back\n", h, (void *)v->vu_hd.back);


    bu_log("%s%p vg_p\n", h, (void *)v->vg_p);
    if (v->vg_p)
	nmg_pr_vg(v->vg_p, h);

    Return;
}


void
nmg_pr_vu(const struct vertexuse *vu, char *h)
{
    MKPAD(h);
    NMG_CK_VERTEXUSE(vu);

    bu_log("%sVERTEXUSE %p\n", h, (void *)vu);
    if (!vu || vu->l.magic != NMG_VERTEXUSE_MAGIC) {
	bu_log("bad vertexuse magic\n");
	Return;
    }

    switch (*vu->up.magic_p) {
	case NMG_SHELL_MAGIC	: bu_log("%s%p up.s_p\n", h, (void *)vu->up.s_p); break;
	case NMG_LOOPUSE_MAGIC	: bu_log("%s%p up.lu_p\n", h, (void *)vu->up.lu_p); break;
	case NMG_EDGEUSE_MAGIC	: bu_log("%s%p up.eu_p\n", h, (void *)vu->up.eu_p); break;
	default			: bu_log("bad vertexuse parent magic\n");
	    Return;
    }
    bu_log("%s%p l.forw\n", h, (void *)vu->l.forw);
    bu_log("%s%p l.back\n", h, (void *)vu->l.back);
    if (vu->a.magic_p) switch (*vu->a.magic_p) {
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
	    bu_log("%s%p a.plane_p\n", h, (void *)vu->a.plane_p);
	    bu_log("%s normal: %f %f %f\n", h, V3ARGS(vu->a.plane_p->N));
	    break;
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
	    bu_log("%s%p a.cnurb_p\n", h, (void *)vu->a.cnurb_p);
	    break;
    }
    bu_log("%s%p v_p\n", h, (void *)vu->v_p);
    nmg_pr_v(vu->v_p, h);
    if (vu->a.magic_p) nmg_pr_vua(vu->a.magic_p, h);

    Return;
}


void
nmg_pr_vu_briefly(const struct vertexuse *vu, char *h)
{
    const struct vertex_g *vg;

    MKPAD(h);
    NMG_CK_VERTEXUSE(vu);
    NMG_CK_VERTEX(vu->v_p);

    vg = vu->v_p->vg_p;
    if (vg) {
	NMG_CK_VERTEX_G(vg);
	bu_log("%sVERTEXUSE %p, v=%p, %f %f %f\n", h,
	       (void *)vu, (void *)vu->v_p,
	       V3ARGS(vg->coord));
    } else {
	bu_log("%sVERTEXUSE %p, v=%p\n", h, (void *)vu, (void *)vu->v_p);
    }

    Return;
}


void
nmg_pr_vua(const uint32_t *magic_p, char *h)
{
    MKPAD(h);

    bu_log("%sVERTEXUSE_A %p\n", h, (void *)magic_p);
    if (!magic_p) {
	bu_log("bad vertexuse_a magic\n");
	Return;
    }

    switch (*magic_p) {
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
	    bu_log("%s N=(%g, %g, %g)\n", h,
		   V3ARGS(((struct vertexuse_a_plane *)magic_p)->N));
	    break;
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
	    bu_log("%s param=(%g, %g, %g)\n", h,
		   V3ARGS(((struct vertexuse_a_cnurb *)magic_p)->param));
	    break;
    }

    Return;
}


void
nmg_euprint(const char *str, const struct edgeuse *eu)
{
    const fastf_t *eup;
    const fastf_t *matep;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_VERTEXUSE(eu->vu_p);
    NMG_CK_VERTEX(eu->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

    NMG_CK_EDGEUSE(eu->eumate_p);
    NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
    NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
    NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

    eup = eu->vu_p->v_p->vg_p->coord;
    matep = eu->eumate_p->vu_p->v_p->vg_p->coord;

    bu_log("%s (%g, %g, %g -> %g, %g, %g)\n", str, eup[0], eup[1], eup[2],
	   matep[0], matep[1], matep[2]);
}
/**
 * Print an bu_ptbl array for inspection.
 */
void
nmg_pr_ptbl(const char *title, const struct bu_ptbl *tbl, int verbose)
{
    register long **lp;

    BU_CK_PTBL(tbl);
    bu_log("%s: bu_ptbl array with %ld entries\n",
	   title, tbl->end);

    if (!verbose) return;

    for (lp = (long **)BU_PTBL_BASEADDR(tbl);
	 lp <= (long **)BU_PTBL_LASTADDR(tbl); lp++
	) {
	if (*lp == 0) {
	    bu_log("  %p NULL entry\n", (void *)*lp);
	    continue;
	}
	switch (**lp) {
	    default:
		bu_log("  %p %s\n", (void *)*lp, bu_identify_magic(**lp));
		break;
	    case NMG_EDGEUSE_MAGIC:
		bu_log("  %p edgeuse vu=%p, far vu=%p\n",
		       (void *)*lp,
		       (void *)((struct edgeuse *)*lp)->vu_p,
		       (void *)BU_LIST_PNEXT_CIRC(edgeuse, *lp)->vu_p);
		break;
	    case NMG_VERTEXUSE_MAGIC:
		bu_log("  %p vertexuse v=%p\n",
		       (void *)*lp,
		       (void *)((struct vertexuse *)*lp)->v_p);
		break;
	}
    }
}


/**
 * Print a ptbl array as a vertex list.
 */
void
nmg_pr_ptbl_vert_list(const char *str, const struct bu_ptbl *tbl, const fastf_t *mag)
{
    int i;
    const struct vertexuse **vup;
    const struct vertexuse *vu;
    const struct vertex *v;
    const struct vertex_g *vg;

    bu_log("nmg_pr_ptbl_vert_list(%s):\n", str);

    vup = (const struct vertexuse **)tbl->buffer;
    for (i = 0; i < tbl->end; ++i) {
	vu = vup[i];
	if (vu->l.magic != NMG_VERTEXUSE_MAGIC) {
	    bu_log("\tWARNING: vertexuse #%d has bad MAGIC (%u)\n",
		   i, vu->l.magic);
	    continue;
	}
	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);
	bu_log("%d\t%g, %g, %g\t", i, V3ARGS(vg->coord));
	if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC) {
	    bu_log("EDGEUSE");
	} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	    bu_log("LOOPUSE");
	    if ((struct vertexuse *)vu->up.lu_p->down_hd.forw != vu) {
		bu_log("ERROR vertexuse's parent disowns us!\n");
		if (((struct vertexuse *)(vu->up.lu_p->lumate_p->down_hd.forw))->l.magic == NMG_VERTEXUSE_MAGIC)
		    bu_bomb("lumate has vertexuse\n");
		bu_bomb("lumate has garbage\n");
	    }
	} else {
	    bu_log("vu up UNKNOWN");
	}
	bu_log("\tv=%p, vu=%p, dist=%g\n", (void *)v, (void *)vu, mag[i]);
    }
}


/**
 * Common formatting code for edgeuses and edgeuse mates.
 * Does not mind wire edges.
 */
void
nmg_pr_one_eu_vecs(const struct edgeuse *eu, const fastf_t *xvec, const fastf_t *yvec,
		   const fastf_t *zvec, const struct bn_tol *UNUSED(tol))
{
    const struct loopuse *lu;
    const struct faceuse *fu;
    const struct face *f;
    const struct shell *s;
    char *lu_orient;
    char *fu_orient;

    NMG_CK_EDGEUSE(eu);
    lu = (struct loopuse *)NULL;
    lu_orient = "W";
    fu = (struct faceuse *)NULL;
    fu_orient = "W";
    f = (struct face *)NULL;
    if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	lu = eu->up.lu_p;
	NMG_CK_LOOPUSE(lu);
	/* +3 is to skip the "OT_" prefix */
	lu_orient = nmg_orientation(lu->orientation)+3;
	if (*lu->up.magic_p == NMG_FACEUSE_MAGIC) {
	    fu = lu->up.fu_p;
	    NMG_CK_FACEUSE(fu);
	    fu_orient = nmg_orientation(fu->orientation)+3;
	    f = fu->f_p;
	    s = fu->s_p;
	} else {
	    s = lu->up.s_p;
	}
    } else {
	s = eu->up.s_p;
    }
    NMG_CK_SHELL(s);
    if (f && *f->g.magic_p == NMG_FACE_G_SNURB_MAGIC) {
	bu_log(" %p, lu=%p=%1.1s, f=%p, fu=%p=%1.1s, s=%p SNURB\n",
	       (void *)eu,
	       (void *)lu, lu_orient,
	       (void *)f,
	       (void *)fu, fu_orient,
	       (void *)s);
    } else {
	bu_log(" %p, lu=%p=%1.1s, f=%p, fu=%p=%1.1s, s=%p %g deg\n",
	       (void *)eu,
	       (void *)lu, lu_orient,
	       (void *)f,
	       (void *)fu, fu_orient,
	       (void *)s,
	       nmg_measure_fu_angle(eu, xvec, yvec, zvec) * RAD2DEG);
    }
}


void
nmg_pr_fu_around_eu_vecs(const struct edgeuse *eu, const fastf_t *xvec, const fastf_t *yvec, const fastf_t *zvec, const struct bn_tol *tol)
{
    const struct edgeuse *eu1;

    NMG_CK_EDGEUSE(eu);
    BN_CK_TOL(tol);
    bu_log("nmg_pr_fu_around_eu_vecs(eu=%p) e=%p\n", (void *)eu, (void *)eu->e_p);

    /* To go correct way around, start with arg's mate,
     * so that arg, then radial, will follow.
     */
    eu = eu->eumate_p;

    eu1 = eu;
    do {
	/* First, the edgeuse mate */
	nmg_pr_one_eu_vecs(eu1, xvec, yvec, zvec, tol);

	/* Second, the edgeuse itself (mate's mate) */
	eu1 = eu1->eumate_p;
	nmg_pr_one_eu_vecs(eu1, xvec, yvec, zvec, tol);

	/* Now back around to the radial edgeuse */
	eu1 = eu1->radial_p;
    } while (eu1 != eu);
}


/**
 * A debugging routine to print all the faceuses around a given edge,
 * starting with the given edgeuse.
 * The normal of the first face is considered to be "0 degrees",
 * and the rest are measured from there.
 */
void
nmg_pr_fu_around_eu(const struct edgeuse *eu, const struct bn_tol *tol)
{
    vect_t xvec, yvec, zvec;

    NMG_CK_EDGEUSE(eu);
    BN_CK_TOL(tol);
    bu_log("nmg_pr_fu_around_eu(%p)\n", (void *)eu);

    if (eu->vu_p->v_p == eu->eumate_p->vu_p->v_p || *eu->g.magic_p == NMG_EDGE_G_CNURB_MAGIC) {
	VSET(xvec, 1, 0, 0);
	VSET(yvec, 0, 1, 0);
	VSET(zvec, 0, 0, 1);
    } else {
	/* Erect coordinate system around eu */
	nmg_eu_2vecs_perp(xvec, yvec, zvec, eu, tol);
    }

    nmg_pr_fu_around_eu_vecs(eu, xvec, yvec, zvec, tol);
}


/**
 * Plot all the loopuses around an edgeuse.
 * Don't bother drawing the loopuse mates.
 */
void
nmg_pl_lu_around_eu(const struct edgeuse *eu)
{
    FILE *fp;
    const struct edgeuse *eu1;
    const struct loopuse *lu1;
    long *b;
    static int num;
    char buf[128];

    NMG_CK_EDGEUSE(eu);

    sprintf(buf, "eu_vicinity%d.plot3", num++);
    fp = fopen(buf, "wb");
    if (fp == NULL) {
	perror(buf);
	return;
    }

    b = (long *)bu_calloc(nmg_find_model((uint32_t *)eu)->maxindex, sizeof(long),
			  "nmg_pl_lu_around_eu flag[]");

    /* To go correct way around, start with arg's mate,
     * so that arg, then radial, will follow.
     */
    eu = eu->eumate_p;

    eu1 = eu;
    do {
	/* First, the edgeuse mate */
	/* Second, the edgeuse itself (mate's mate) */
	eu1 = eu1->eumate_p;

	if (*eu1->up.magic_p == NMG_LOOPUSE_MAGIC) {
	    lu1 = eu1->up.lu_p;
	    nmg_pl_lu(fp, lu1, b, 80, 100, 170);
	}

	/* Now back around to the radial edgeuse */
	eu1 = eu1->radial_p;
    } while (eu1 != eu);

    bu_free((char *)b, "nmg_pl_lu_around_eu flag[]");
    fclose(fp);
    bu_log("Wrote %s\n", buf);
}


/**
 * For either kind of face geometry, print the list of all faces & faceuses
 * that share this geometry.
 */
void
nmg_pr_fus_in_fg(const uint32_t *fg_magic)
{
    struct face *f;

    NMG_CK_FACE_G_EITHER(fg_magic);
    bu_log("nmg_pr_fus_in_fg(%p):\n", (void *)fg_magic);
    for (BU_LIST_FOR(f, face, &(((struct face_g_plane *)fg_magic)->f_hd))) {
	NMG_CK_FACE(f);
	NMG_CK_FACEUSE(f->fu_p);
	bu_log(" f=%p, fu=%p, fumate=%p\n",
	       (void *)f, (void *)f->fu_p, (void *)f->fu_p->fumate_p);
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
