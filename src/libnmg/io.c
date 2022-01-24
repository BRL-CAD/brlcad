/*                            I O . C
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file io.c
 *
 * Serialization/de-serialization routines for NMG data structures
 *
 */

#include "common.h"
#include <string.h>
#include "bnetwork.h"
#include "bu/cv.h"
#include "bu/malloc.h"
#include "bn/tol.h"
#include "nmg.h"

struct nmg_exp_counts {
    long new_subscript;
    long per_struct_index;
    int kind;
    long first_fastf_relpos;    /* for snurb and cnurb. */
    long byte_offset;           /* for snurb and cnurb. */
};

const int nmg_disk_sizes[NMG_N_KINDS] = {
    sizeof(struct disk_model),		/* 0 */
    sizeof(struct disk_nmgregion),
    sizeof(struct disk_nmgregion_a),
    sizeof(struct disk_shell),
    sizeof(struct disk_shell_a),
    sizeof(struct disk_faceuse),
    sizeof(struct disk_face),
    sizeof(struct disk_face_g_plane),
    sizeof(struct disk_face_g_snurb),
    sizeof(struct disk_loopuse),
    sizeof(struct disk_loop),		/* 10 */
    sizeof(struct disk_loop_a),
    sizeof(struct disk_edgeuse),
    sizeof(struct disk_edge),
    sizeof(struct disk_edge_g_lseg),
    sizeof(struct disk_edge_g_cnurb),
    sizeof(struct disk_vertexuse),
    sizeof(struct disk_vertexuse_a_plane),
    sizeof(struct disk_vertexuse_a_cnurb),
    sizeof(struct disk_vertex),
    sizeof(struct disk_vertex_g),		/* 20 */
    0,
    0,
    0,
    0,
    0  /* disk_double_array, MUST BE ZERO */	/* 25: MUST BE ZERO */
};
const char nmg_kind_names[NMG_N_KINDS+2][18] = {
    "model",				/* 0 */
    "nmgregion",
    "nmgregion_a",
    "shell",
    "shell_a",
    "faceuse",
    "face",
    "face_g_plane",
    "face_g_snurb",
    "loopuse",
    "loop",					/* 10 */
    "loop_a",
    "edgeuse",
    "edge",
    "edge_g_lseg",
    "edge_g_cnurb",
    "vertexuse",
    "vertexuse_a_plane",
    "vertexuse_a_cnurb",
    "vertex",
    "vertex_g",				/* 20 */
    "k21",
    "k22",
    "k23",
    "k24",
    "double_array",				/* 25 */
    "k26-OFF_END",
    "k27-OFF_END"
};


/**
 * Given the magic number for an NMG structure, return the
 * manifest constant which identifies that structure kind.
 */
static int
nmg_magic_to_kind(uint32_t magic)
{
    switch (magic) {
	case NMG_MODEL_MAGIC:
	    return NMG_KIND_MODEL;
	case NMG_REGION_MAGIC:
	    return NMG_KIND_NMGREGION;
	case NMG_REGION_A_MAGIC:
	    return NMG_KIND_NMGREGION_A;
	case NMG_SHELL_MAGIC:
	    return NMG_KIND_SHELL;
	case NMG_SHELL_A_MAGIC:
	    return NMG_KIND_SHELL_A;
	case NMG_FACEUSE_MAGIC:
	    return NMG_KIND_FACEUSE;
	case NMG_FACE_MAGIC:
	    return NMG_KIND_FACE;
	case NMG_FACE_G_PLANE_MAGIC:
	    return NMG_KIND_FACE_G_PLANE;
	case NMG_FACE_G_SNURB_MAGIC:
	    return NMG_KIND_FACE_G_SNURB;
	case NMG_LOOPUSE_MAGIC:
	    return NMG_KIND_LOOPUSE;
	case NMG_LOOP_A_MAGIC:
	    return NMG_KIND_LOOP_A;
	case NMG_LOOP_MAGIC:
	    return NMG_KIND_LOOP;
	case NMG_EDGEUSE_MAGIC:
	    return NMG_KIND_EDGEUSE;
	case NMG_EDGE_MAGIC:
	    return NMG_KIND_EDGE;
	case NMG_EDGE_G_LSEG_MAGIC:
	    return NMG_KIND_EDGE_G_LSEG;
	case NMG_EDGE_G_CNURB_MAGIC:
	    return NMG_KIND_EDGE_G_CNURB;
	case NMG_VERTEXUSE_MAGIC:
	    return NMG_KIND_VERTEXUSE;
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
	    return NMG_KIND_VERTEXUSE_A_PLANE;
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
	    return NMG_KIND_VERTEXUSE_A_CNURB;
	case NMG_VERTEX_MAGIC:
	    return NMG_KIND_VERTEX;
	case NMG_VERTEX_G_MAGIC:
	    return NMG_KIND_VERTEX_G;
    }
    /* default */
    bu_log("magic = x%x\n", magic);
    bu_bomb("nmg_magic_to_kind: bad magic");
    return -1;
}


/* XXX These are horribly non-PARALLEL, and they *must* be PARALLEL ! */
static unsigned char *nmg_fastf_p;
static unsigned int nmg_cur_fastf_subscript;

/**
 * Format a variable sized array of fastf_t's into external format
 * (IEEE big endian double precision) with a 2 element header.
 *
 *		+-----------+
 *		|  magic    |
 *		+-----------+
 *		|  count    |
 *		+-----------+
 *		|           |
 *		~  doubles  ~
 *		~    :      ~
 *		|           |
 *		+-----------+
 *
 * Increments the pointer to the next free byte in the external array,
 * and increments the subscript number of the next free array.
 *
 * Note that this subscript number is consistent with the rest of the
 * NMG external subscript numbering, so that the first
 * disk_double_array subscript will be one larger than the largest
 * disk_vertex_g subscript, and in the external record the array of
 * fastf_t arrays will follow the array of disk_vertex_g structures.
 *
 * Returns subscript number of this array, in the external form.
 */
int
nmg_export4_fastf(const fastf_t *fp, int count, int pt_type, double scale)
/* If zero, means literal array of values */
{
    int i;
    unsigned char *cp;

    /* always write doubles to disk */
    double *scanp;

    if (pt_type)
	count *= RT_NURB_EXTRACT_COORDS(pt_type);

    cp = nmg_fastf_p;
    *(uint32_t *)&cp[0] = htonl(DISK_DOUBLE_ARRAY_MAGIC);
    *(uint32_t *)&cp[4] = htonl(count);
    if (pt_type == 0 || ZERO(scale - 1.0)) {
	scanp = (double *)bu_malloc(count * sizeof(double), "scanp");
	/* convert fastf_t to double */
	for (i=0; i<count; i++) {
	    scanp[i] = fp[i];
	}
	bu_cv_htond(cp + (4+4), (unsigned char *)scanp, count);
	bu_free(scanp, "scanp");
    } else {
	/* Need to scale data by 'scale' ! */
	scanp = (double *)bu_malloc(count*sizeof(double), "scanp");
	if (RT_NURB_IS_PT_RATIONAL(pt_type)) {
	    /* Don't scale the homogeneous (rational) coord */
	    int nelem;	/* # elements per tuple */

	    nelem = RT_NURB_EXTRACT_COORDS(pt_type);
	    for (i = 0; i < count; i += nelem) {
		VSCALEN(&scanp[i], &fp[i], scale, nelem-1);
		scanp[i+nelem-1] = fp[i+nelem-1];
	    }
	} else {
	    /* Scale everything as one long array */
	    VSCALEN(scanp, fp, scale, count);
	}
	bu_cv_htond(cp + (4+4), (unsigned char *)scanp, count);
	bu_free(scanp, "nmg_export4_fastf");
    }
    cp += (4+4) + count * 8;
    nmg_fastf_p = cp;
    return nmg_cur_fastf_subscript++;
}

/**
 * Depends on ecnt[0].byte_offset having been set to maxindex.
 *
 * There are some special values for the disk index returned here:
 * >0 normal structure index.
 * 0 substitute a null pointer when imported.
 * -1 substitute pointer to within-struct list head when imported.
 */
static int
reindex(void *p, struct nmg_exp_counts *ecnt)
{
    long idx = 0;
    long ret=0;	/* zero is NOT the default value, this is just to satisfy cray compilers */

    /* If null pointer, return new subscript of zero */
    if (p == 0) {
	ret = 0;
    } else {
	idx = nmg_index_of_struct((uint32_t *)(p));
	if (idx == -1) {
	    ret = DISK_INDEX_LISTHEAD; /* FLAG:  special list head */
	} else if (idx < -1) {
	    bu_bomb("reindex(): unable to obtain struct index\n");
	} else {
	    ret = ecnt[idx].new_subscript;
	    if (ecnt[idx].kind < 0) {
		bu_log("reindex(p=%p), p->index=%ld, ret=%ld, kind=%d\n", p, idx, ret, ecnt[idx].kind);
		bu_bomb("reindex() This index not found in ecnt[]\n");
	    }
	    /* ret == 0 on suppressed loop_a ptrs, etc. */
	    if (ret < 0 || ret > ecnt[0].byte_offset) {
		bu_log("reindex(p=%p) %s, p->index=%ld, ret=%ld, maxindex=%ld\n",
		       p,
		       bu_identify_magic(*(uint32_t *)p),
		       idx, ret, ecnt[0].byte_offset);
		bu_bomb("reindex() subscript out of range\n");
	    }
	}
    }
/*bu_log("reindex(p=x%x), p->index=%d, ret=%d\n", p, idx, ret);*/
    return ret;
}



/* forw may never be null;  back may be null for loopuse (sigh) */
#define INDEX(o, i, elem) *(uint32_t *)(o)->elem = htonl(reindex((void *)((i)->elem), ecnt))
#define INDEXL(oo, ii, elem) {						\
	uint32_t _f = reindex((void *)((ii)->elem.forw), ecnt);	\
	if (_f == DISK_INDEX_NULL) \
	    bu_log("INTERNAL WARNING: nmg_edisk() reindex forw to null?\n"); \
	*(uint32_t *)((oo)->elem.forw) = htonl(_f);			\
	*(uint32_t *)((oo)->elem.back) = htonl(reindex((void *)((ii)->elem.back), ecnt)); }
#define PUTMAGIC(_magic) *(uint32_t *)d->magic = htonl(_magic)


/**
 * Export a given structure from memory to disk format
 *
 * Scale geometry by 'local2mm'
 */
static void
nmg_edisk(void *op, void *ip, struct nmg_exp_counts *ecnt, int idx, double local2mm)
/* base of disk array */
/* ptr to in-memory structure */
{
    int oindex;		/* index in op */

    oindex = ecnt[idx].per_struct_index;
    switch (ecnt[idx].kind) {
	case NMG_KIND_MODEL: {
	    struct model *m = (struct model *)ip;
	    struct disk_model *d;
	    d = &((struct disk_model *)op)[oindex];
	    NMG_CK_MODEL(m);
	    PUTMAGIC(DISK_MODEL_MAGIC);
	    *(uint32_t *)d->version = htonl(0);
	    INDEXL(d, m, r_hd);
	}
	    return;
	case NMG_KIND_NMGREGION: {
	    struct nmgregion *r = (struct nmgregion *)ip;
	    struct disk_nmgregion *d;
	    d = &((struct disk_nmgregion *)op)[oindex];
	    NMG_CK_REGION(r);
	    PUTMAGIC(DISK_REGION_MAGIC);
	    INDEXL(d, r, l);
	    INDEX(d, r, m_p);
	    INDEX(d, r, ra_p);
	    INDEXL(d, r, s_hd);
	}
	    return;
	case NMG_KIND_NMGREGION_A: {
	    struct nmgregion_a *r = (struct nmgregion_a *)ip;
	    struct disk_nmgregion_a *d;

	    /* must be double for import and export */
	    double min[ELEMENTS_PER_POINT];
	    double max[ELEMENTS_PER_POINT];

	    d = &((struct disk_nmgregion_a *)op)[oindex];
	    NMG_CK_REGION_A(r);
	    PUTMAGIC(DISK_REGION_A_MAGIC);
	    VSCALE(min, r->min_pt, local2mm);
	    VSCALE(max, r->max_pt, local2mm);
	    bu_cv_htond(d->min_pt, (unsigned char *)min, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->max_pt, (unsigned char *)max, ELEMENTS_PER_POINT);
	}
	    return;
	case NMG_KIND_SHELL: {
	    struct shell *s = (struct shell *)ip;
	    struct disk_shell *d;
	    d = &((struct disk_shell *)op)[oindex];
	    NMG_CK_SHELL(s);
	    PUTMAGIC(DISK_SHELL_MAGIC);
	    INDEXL(d, s, l);
	    INDEX(d, s, r_p);
	    INDEX(d, s, sa_p);
	    INDEXL(d, s, fu_hd);
	    INDEXL(d, s, lu_hd);
	    INDEXL(d, s, eu_hd);
	    INDEX(d, s, vu_p);
	}
	    return;
	case NMG_KIND_SHELL_A: {
	    struct shell_a *sa = (struct shell_a *)ip;
	    struct disk_shell_a *d;

	    /* must be double for import and export */
	    double min[ELEMENTS_PER_POINT];
	    double max[ELEMENTS_PER_POINT];

	    d = &((struct disk_shell_a *)op)[oindex];
	    NMG_CK_SHELL_A(sa);
	    PUTMAGIC(DISK_SHELL_A_MAGIC);
	    VSCALE(min, sa->min_pt, local2mm);
	    VSCALE(max, sa->max_pt, local2mm);
	    bu_cv_htond(d->min_pt, (unsigned char *)min, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->max_pt, (unsigned char *)max, ELEMENTS_PER_POINT);
	}
	    return;
	case NMG_KIND_FACEUSE: {
	    struct faceuse *fu = (struct faceuse *)ip;
	    struct disk_faceuse *d;
	    d = &((struct disk_faceuse *)op)[oindex];
	    NMG_CK_FACEUSE(fu);
	    NMG_CK_FACEUSE(fu->fumate_p);
	    NMG_CK_FACE(fu->f_p);
	    if (fu->f_p != fu->fumate_p->f_p) bu_log("faceuse export, differing faces\n");
	    PUTMAGIC(DISK_FACEUSE_MAGIC);
	    INDEXL(d, fu, l);
	    INDEX(d, fu, s_p);
	    INDEX(d, fu, fumate_p);
	    *(uint32_t *)d->orientation = htonl(fu->orientation);
	    INDEX(d, fu, f_p);
	    INDEXL(d, fu, lu_hd);
	}
	    return;
	case NMG_KIND_FACE: {
	    struct face *f = (struct face *)ip;
	    struct disk_face *d;
	    d = &((struct disk_face *)op)[oindex];
	    NMG_CK_FACE(f);
	    PUTMAGIC(DISK_FACE_MAGIC);
	    INDEXL(d, f, l);	/* face is member of fg list */
	    INDEX(d, f, fu_p);
	    *(uint32_t *)d->g = htonl(reindex((void *)(f->g.magic_p), ecnt));
	    *(uint32_t *)d->flip = htonl(f->flip);
	}
	    return;
	case NMG_KIND_FACE_G_PLANE: {
	    struct face_g_plane *fg = (struct face_g_plane *)ip;
	    struct disk_face_g_plane *d;

	    /* must be double for import and export */
	    double plane[ELEMENTS_PER_PLANE];

	    d = &((struct disk_face_g_plane *)op)[oindex];
	    NMG_CK_FACE_G_PLANE(fg);
	    PUTMAGIC(DISK_FACE_G_PLANE_MAGIC);
	    INDEXL(d, fg, f_hd);

	    /* convert fastf_t to double */
	    VMOVE(plane, fg->N);
	    plane[W] = fg->N[W] * local2mm;

	    bu_cv_htond(d->N, (unsigned char *)plane, ELEMENTS_PER_PLANE);
	}
	    return;
	case NMG_KIND_FACE_G_SNURB: {
	    struct face_g_snurb *fg = (struct face_g_snurb *)ip;
	    struct disk_face_g_snurb *d;

	    d = &((struct disk_face_g_snurb *)op)[oindex];
	    NMG_CK_FACE_G_SNURB(fg);
	    PUTMAGIC(DISK_FACE_G_SNURB_MAGIC);
	    INDEXL(d, fg, f_hd);
	    *(uint32_t *)d->u_order = htonl(fg->order[0]);
	    *(uint32_t *)d->v_order = htonl(fg->order[1]);
	    *(uint32_t *)d->u_size = htonl(fg->u.k_size);
	    *(uint32_t *)d->v_size = htonl(fg->v.k_size);
	    *(uint32_t *)d->u_knots = htonl(
		nmg_export4_fastf(fg->u.knots, fg->u.k_size, 0, 1.0));
	    *(uint32_t *)d->v_knots = htonl(
		nmg_export4_fastf(fg->v.knots, fg->v.k_size, 0, 1.0));
	    *(uint32_t *)d->us_size = htonl(fg->s_size[0]);
	    *(uint32_t *)d->vs_size = htonl(fg->s_size[1]);
	    *(uint32_t *)d->pt_type = htonl(fg->pt_type);
	    /* scale XYZ ctl_points by local2mm */
	    *(uint32_t *)d->ctl_points = htonl(
		nmg_export4_fastf(fg->ctl_points,
				     fg->s_size[0] * fg->s_size[1],
				     fg->pt_type,
				     local2mm));
	}
	    return;
	case NMG_KIND_LOOPUSE: {
	    struct loopuse *lu = (struct loopuse *)ip;
	    struct disk_loopuse *d;
	    d = &((struct disk_loopuse *)op)[oindex];
	    NMG_CK_LOOPUSE(lu);
	    PUTMAGIC(DISK_LOOPUSE_MAGIC);
	    INDEXL(d, lu, l);
	    *(uint32_t *)d->up = htonl(reindex((void *)(lu->up.magic_p), ecnt));
	    INDEX(d, lu, lumate_p);
	    *(uint32_t *)d->orientation = htonl(lu->orientation);
	    INDEX(d, lu, l_p);
	    INDEXL(d, lu, down_hd);
	}
	    return;
	case NMG_KIND_LOOP: {
	    struct loop *loop = (struct loop *)ip;
	    struct disk_loop *d;
	    d = &((struct disk_loop *)op)[oindex];
	    NMG_CK_LOOP(loop);
	    PUTMAGIC(DISK_LOOP_MAGIC);
	    INDEX(d, loop, lu_p);
	    INDEX(d, loop, la_p);
	}
	    return;
	case NMG_KIND_LOOP_A: {
	    struct loop_a *lg = (struct loop_a *)ip;
	    struct disk_loop_a *d;

	    /* must be double for import and export */
	    double min[ELEMENTS_PER_POINT];
	    double max[ELEMENTS_PER_POINT];

	    d = &((struct disk_loop_a *)op)[oindex];
	    NMG_CK_LOOP_A(lg);
	    PUTMAGIC(DISK_LOOP_A_MAGIC);

	    VSCALE(min, lg->min_pt, local2mm);
	    VSCALE(max, lg->max_pt, local2mm);

	    bu_cv_htond(d->min_pt, (unsigned char *)min, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->max_pt, (unsigned char *)max, ELEMENTS_PER_POINT);
	}
	    return;
	case NMG_KIND_EDGEUSE: {
	    struct edgeuse *eu = (struct edgeuse *)ip;
	    struct disk_edgeuse *d;
	    d = &((struct disk_edgeuse *)op)[oindex];
	    NMG_CK_EDGEUSE(eu);
	    PUTMAGIC(DISK_EDGEUSE_MAGIC);
	    INDEXL(d, eu, l);
	    /* NOTE The pointers in l2 point at other l2's.
	     * nmg_index_of_struct() will point 'em back
	     * at the top of the edgeuse.  Beware on import.
	     */
	    INDEXL(d, eu, l2);
	    *(uint32_t *)d->up = htonl(reindex((void *)(eu->up.magic_p), ecnt));
	    INDEX(d, eu, eumate_p);
	    INDEX(d, eu, radial_p);
	    INDEX(d, eu, e_p);
	    *(uint32_t *)d->orientation = htonl(eu->orientation);
	    INDEX(d, eu, vu_p);
	    *(uint32_t *)d->g = htonl(reindex((void *)(eu->g.magic_p), ecnt));
	}
	    return;
	case NMG_KIND_EDGE: {
	    struct edge *e = (struct edge *)ip;
	    struct disk_edge *d;
	    d = &((struct disk_edge *)op)[oindex];
	    NMG_CK_EDGE(e);
	    PUTMAGIC(DISK_EDGE_MAGIC);
	    *(uint32_t *)d->is_real = htonl(e->is_real);
	    INDEX(d, e, eu_p);
	}
	    return;
	case NMG_KIND_EDGE_G_LSEG: {
	    struct edge_g_lseg *eg = (struct edge_g_lseg *)ip;
	    struct disk_edge_g_lseg *d;

	    /* must be double for import and export */
	    double pt[ELEMENTS_PER_POINT];
	    double dir[ELEMENTS_PER_VECT];

	    d = &((struct disk_edge_g_lseg *)op)[oindex];
	    NMG_CK_EDGE_G_LSEG(eg);
	    PUTMAGIC(DISK_EDGE_G_LSEG_MAGIC);
	    INDEXL(d, eg, eu_hd2);

	    /* convert fastf_t to double */
	    VSCALE(pt, eg->e_pt, local2mm);
	    VMOVE(dir, eg->e_dir);

	    bu_cv_htond(d->e_pt, (unsigned char *)pt, ELEMENTS_PER_POINT);
	    bu_cv_htond(d->e_dir, (unsigned char *)dir, ELEMENTS_PER_VECT);
	}
	    return;
	case NMG_KIND_EDGE_G_CNURB: {
	    struct edge_g_cnurb *eg = (struct edge_g_cnurb *)ip;
	    struct disk_edge_g_cnurb *d;
	    d = &((struct disk_edge_g_cnurb *)op)[oindex];
	    NMG_CK_EDGE_G_CNURB(eg);
	    PUTMAGIC(DISK_EDGE_G_CNURB_MAGIC);
	    INDEXL(d, eg, eu_hd2);
	    *(uint32_t *)d->order = htonl(eg->order);

	    /* If order is zero, everything else is NULL */
	    if (eg->order == 0) return;

	    *(uint32_t *)d->k_size = htonl(eg->k.k_size);
	    *(uint32_t *)d->knots = htonl(nmg_export4_fastf(eg->k.knots, eg->k.k_size, 0, 1.0));
	    *(uint32_t *)d->c_size = htonl(eg->c_size);
	    *(uint32_t *)d->pt_type = htonl(eg->pt_type);
	    /*
	     * The curve's control points are in parameter space
	     * for cnurbs on snurbs, and in XYZ for cnurbs on planar faces.
	     * UV values do NOT get transformed, XYZ values do!
	     */
	    *(uint32_t *)d->ctl_points = htonl(
		nmg_export4_fastf(eg->ctl_points,
				     eg->c_size,
				     eg->pt_type,
				     RT_NURB_EXTRACT_PT_TYPE(eg->pt_type) == RT_NURB_PT_UV ?
				     1.0 : local2mm));
	}
	    return;
	case NMG_KIND_VERTEXUSE: {
	    struct vertexuse *vu = (struct vertexuse *)ip;
	    struct disk_vertexuse *d;
	    d = &((struct disk_vertexuse *)op)[oindex];
	    NMG_CK_VERTEXUSE(vu);
	    PUTMAGIC(DISK_VERTEXUSE_MAGIC);
	    INDEXL(d, vu, l);
	    *(uint32_t *)d->up = htonl(reindex((void *)(vu->up.magic_p), ecnt));
	    INDEX(d, vu, v_p);
	    if (vu->a.magic_p)NMG_CK_VERTEXUSE_A_EITHER(vu->a.magic_p);
	    *(uint32_t *)d->a = htonl(reindex((void *)(vu->a.magic_p), ecnt));
	}
	    return;
	case NMG_KIND_VERTEXUSE_A_PLANE: {
	    struct vertexuse_a_plane *vua = (struct vertexuse_a_plane *)ip;
	    struct disk_vertexuse_a_plane *d;

	    /* must be double for import and export */
	    double normal[ELEMENTS_PER_VECT];

	    d = &((struct disk_vertexuse_a_plane *)op)[oindex];
	    NMG_CK_VERTEXUSE_A_PLANE(vua);
	    PUTMAGIC(DISK_VERTEXUSE_A_PLANE_MAGIC);

	    /* Normal vectors don't scale */
	    /* This is not a plane equation here */
	    VMOVE(normal, vua->N); /* convert fastf_t to double */
	    bu_cv_htond(d->N, (unsigned char *)normal, ELEMENTS_PER_VECT);
	}
	    return;
	case NMG_KIND_VERTEXUSE_A_CNURB: {
	    struct vertexuse_a_cnurb *vua = (struct vertexuse_a_cnurb *)ip;
	    struct disk_vertexuse_a_cnurb *d;

	    /* must be double for import and export */
	    double param[3];

	    d = &((struct disk_vertexuse_a_cnurb *)op)[oindex];
	    NMG_CK_VERTEXUSE_A_CNURB(vua);
	    PUTMAGIC(DISK_VERTEXUSE_A_CNURB_MAGIC);

	    /* (u, v) parameters on curves don't scale */
	    VMOVE(param, vua->param); /* convert fastf_t to double */

	    bu_cv_htond(d->param, (unsigned char *)param, 3);
	}
	    return;
	case NMG_KIND_VERTEX: {
	    struct vertex *v = (struct vertex *)ip;
	    struct disk_vertex *d;
	    d = &((struct disk_vertex *)op)[oindex];
	    NMG_CK_VERTEX(v);
	    PUTMAGIC(DISK_VERTEX_MAGIC);
	    INDEXL(d, v, vu_hd);
	    INDEX(d, v, vg_p);
	}
	    return;
	case NMG_KIND_VERTEX_G: {
	    struct vertex_g *vg = (struct vertex_g *)ip;
	    struct disk_vertex_g *d;

	    /* must be double for import and export */
	    double pt[ELEMENTS_PER_POINT];

	    d = &((struct disk_vertex_g *)op)[oindex];
	    NMG_CK_VERTEX_G(vg);
	    PUTMAGIC(DISK_VERTEX_G_MAGIC);
	    VSCALE(pt, vg->coord, local2mm);

	    bu_cv_htond(d->coord, (unsigned char *)pt, ELEMENTS_PER_POINT);
	}
	    return;
    }
    bu_log("nmg_edisk kind=%d unknown\n", ecnt[idx].kind);
}
#undef INDEX
#undef INDEXL

/**
 * Allocate storage for all the in-memory NMG structures, in
 * preparation for the importation operation, using the GET_xxx()
 * macros, so that m->maxindex, etc., are all appropriately handled.
 */
static struct model *
nmg_ialloc(uint32_t **ptrs, struct nmg_exp_counts *ecnt, int *kind_counts)
{
    struct model *m = (struct model *)0;
    int subscript;
    int kind;
    int j;

    subscript = 1;
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	if (kind == NMG_KIND_DOUBLE_ARRAY) continue;
	for (j = 0; j < kind_counts[kind]; j++) {
	    ecnt[subscript].kind = kind;
	    ecnt[subscript].per_struct_index = 0; /* unused on import */
	    switch (kind) {
		case NMG_KIND_MODEL:
		    if (m) bu_bomb("multiple models?");
		    m = nmg_mm();
		    /* Keep disk indices & new indices equal... */
		    m->maxindex++;
		    ptrs[subscript] = (uint32_t *)m;
		    break;
		case NMG_KIND_NMGREGION: {
		    struct nmgregion *r;
		    GET_REGION(r, m);
		    r->l.magic = NMG_REGION_MAGIC;
		    BU_LIST_INIT(&r->s_hd);
		    ptrs[subscript] = (uint32_t *)r;
		}
		    break;
		case NMG_KIND_NMGREGION_A: {
		    struct nmgregion_a *ra;
		    GET_REGION_A(ra, m);
		    ra->magic = NMG_REGION_A_MAGIC;
		    ptrs[subscript] = (uint32_t *)ra;
		}
		    break;
		case NMG_KIND_SHELL: {
		    struct shell *s;
		    GET_SHELL(s, m);
		    s->l.magic = NMG_SHELL_MAGIC;
		    BU_LIST_INIT(&s->fu_hd);
		    BU_LIST_INIT(&s->lu_hd);
		    BU_LIST_INIT(&s->eu_hd);
		    ptrs[subscript] = (uint32_t *)s;
		}
		    break;
		case NMG_KIND_SHELL_A: {
		    struct shell_a *sa;
		    GET_SHELL_A(sa, m);
		    sa->magic = NMG_SHELL_A_MAGIC;
		    ptrs[subscript] = (uint32_t *)sa;
		}
		    break;
		case NMG_KIND_FACEUSE: {
		    struct faceuse *fu;
		    GET_FACEUSE(fu, m);
		    fu->l.magic = NMG_FACEUSE_MAGIC;
		    BU_LIST_INIT(&fu->lu_hd);
		    ptrs[subscript] = (uint32_t *)fu;
		}
		    break;
		case NMG_KIND_FACE: {
		    struct face *f;
		    GET_FACE(f, m);
		    f->l.magic = NMG_FACE_MAGIC;
		    ptrs[subscript] = (uint32_t *)f;
		}
		    break;
		case NMG_KIND_FACE_G_PLANE: {
		    struct face_g_plane *fg;
		    GET_FACE_G_PLANE(fg, m);
		    fg->magic = NMG_FACE_G_PLANE_MAGIC;
		    BU_LIST_INIT(&fg->f_hd);
		    ptrs[subscript] = (uint32_t *)fg;
		}
		    break;
		case NMG_KIND_FACE_G_SNURB: {
		    struct face_g_snurb *fg;
		    GET_FACE_G_SNURB(fg, m);
		    fg->l.magic = NMG_FACE_G_SNURB_MAGIC;
		    BU_LIST_INIT(&fg->f_hd);
		    ptrs[subscript] = (uint32_t *)fg;
		}
		    break;
		case NMG_KIND_LOOPUSE: {
		    struct loopuse *lu;
		    GET_LOOPUSE(lu, m);
		    lu->l.magic = NMG_LOOPUSE_MAGIC;
		    BU_LIST_INIT(&lu->down_hd);
		    ptrs[subscript] = (uint32_t *)lu;
		}
		    break;
		case NMG_KIND_LOOP: {
		    struct loop *l;
		    GET_LOOP(l, m);
		    l->magic = NMG_LOOP_MAGIC;
		    ptrs[subscript] = (uint32_t *)l;
		}
		    break;
		case NMG_KIND_LOOP_A: {
		    struct loop_a *lg;
		    GET_LOOP_A(lg, m);
		    lg->magic = NMG_LOOP_A_MAGIC;
		    ptrs[subscript] = (uint32_t *)lg;
		}
		    break;
		case NMG_KIND_EDGEUSE: {
		    struct edgeuse *eu;
		    GET_EDGEUSE(eu, m);
		    eu->l.magic = NMG_EDGEUSE_MAGIC;
		    eu->l2.magic = NMG_EDGEUSE2_MAGIC;
		    ptrs[subscript] = (uint32_t *)eu;
		}
		    break;
		case NMG_KIND_EDGE: {
		    struct edge *e;
		    GET_EDGE(e, m);
		    e->magic = NMG_EDGE_MAGIC;
		    ptrs[subscript] = (uint32_t *)e;
		}
		    break;
		case NMG_KIND_EDGE_G_LSEG: {
		    struct edge_g_lseg *eg;
		    GET_EDGE_G_LSEG(eg, m);
		    eg->l.magic = NMG_EDGE_G_LSEG_MAGIC;
		    BU_LIST_INIT(&eg->eu_hd2);
		    ptrs[subscript] = (uint32_t *)eg;
		}
		    break;
		case NMG_KIND_EDGE_G_CNURB: {
		    struct edge_g_cnurb *eg;
		    GET_EDGE_G_CNURB(eg, m);
		    eg->l.magic = NMG_EDGE_G_CNURB_MAGIC;
		    BU_LIST_INIT(&eg->eu_hd2);
		    ptrs[subscript] = (uint32_t *)eg;
		}
		    break;
		case NMG_KIND_VERTEXUSE: {
		    struct vertexuse *vu;
		    GET_VERTEXUSE(vu, m);
		    vu->l.magic = NMG_VERTEXUSE_MAGIC;
		    ptrs[subscript] = (uint32_t *)vu;
		}
		    break;
		case NMG_KIND_VERTEXUSE_A_PLANE: {
		    struct vertexuse_a_plane *vua;
		    GET_VERTEXUSE_A_PLANE(vua, m);
		    vua->magic = NMG_VERTEXUSE_A_PLANE_MAGIC;
		    ptrs[subscript] = (uint32_t *)vua;
		}
		    break;
		case NMG_KIND_VERTEXUSE_A_CNURB: {
		    struct vertexuse_a_cnurb *vua;
		    GET_VERTEXUSE_A_CNURB(vua, m);
		    vua->magic = NMG_VERTEXUSE_A_CNURB_MAGIC;
		    ptrs[subscript] = (uint32_t *)vua;
		}
		    break;
		case NMG_KIND_VERTEX: {
		    struct vertex *v;
		    GET_VERTEX(v, m);
		    v->magic = NMG_VERTEX_MAGIC;
		    BU_LIST_INIT(&v->vu_hd);
		    ptrs[subscript] = (uint32_t *)v;
		}
		    break;
		case NMG_KIND_VERTEX_G: {
		    struct vertex_g *vg;
		    GET_VERTEX_G(vg, m);
		    vg->magic = NMG_VERTEX_G_MAGIC;
		    ptrs[subscript] = (uint32_t *)vg;
		}
		    break;
		default:
		    bu_log("bad kind = %d\n", kind);
		    ptrs[subscript] = (uint32_t *)0;
		    break;
	    }

	    /* new_subscript unused on import except for printf()s */
	    ecnt[subscript].new_subscript = nmg_index_of_struct(ptrs[subscript]);
	    subscript++;
	}
    }
    return m;
}

/**
 * Find the locations of all the variable-sized fastf_t arrays in the
 * input record.  Record that position as a byte offset from the very
 * front of the input record in ecnt[], indexed by subscript number.
 *
 * No storage is allocated here, that will be done by
 * nmg_import4_fastf() on the fly.  A separate call to bu_malloc()
 * will be used, so that nmg_keg(), etc., can kill each array as
 * appropriate.
 */
static void
nmg_i2alloc(struct nmg_exp_counts *ecnt, unsigned char *cp, int *kind_counts)
{
    int kind;
    int nkind;
    int subscript;
    int offset;
    int i;

    nkind = kind_counts[NMG_KIND_DOUBLE_ARRAY];
    if (nkind <= 0) return;

    /* First, find the beginning of the fastf_t arrays */
    subscript = 1;
    offset = 0;
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	if (kind == NMG_KIND_DOUBLE_ARRAY) continue;
	offset += nmg_disk_sizes[kind] * kind_counts[kind];
	subscript += kind_counts[kind];
    }

    /* Should have found the first one now */
    NMG_CK_DISKMAGIC(cp + offset, DISK_DOUBLE_ARRAY_MAGIC);
    for (i=0; i < nkind; i++) {
	int ndouble;
	NMG_CK_DISKMAGIC(cp + offset, DISK_DOUBLE_ARRAY_MAGIC);
	ndouble = bu_ntohl(*(uint32_t*)(cp + offset + 4), 0, UINT_MAX - 1);
	ecnt[subscript].kind = NMG_KIND_DOUBLE_ARRAY;
	/* Stored byte offset is from beginning of disk record */
	ecnt[subscript].byte_offset = offset;
	offset += (4+4) + 8*ndouble;
	subscript++;
    }
}

static fastf_t *
nmg_import4_fastf(const unsigned char *base, struct nmg_exp_counts *ecnt, long int subscript, const matp_t mat, int len, int pt_type)
{
    const unsigned char *cp;

    int i;
    int count;
    fastf_t *ret;

    /* must be double for import and export */
    double *tmp;
    double *scanp;

    if (ecnt[subscript].byte_offset <= 0 || ecnt[subscript].kind != NMG_KIND_DOUBLE_ARRAY) {
	bu_log("subscript=%ld, byte_offset=%ld, kind=%d (expected %d)\n",
	       subscript, ecnt[subscript].byte_offset,
	       ecnt[subscript].kind, NMG_KIND_DOUBLE_ARRAY);
	bu_bomb("rt_nmg_import4_fastf() bad ecnt table\n");
    }


    cp = base + ecnt[subscript].byte_offset;
    if (bu_ntohl(*(uint32_t*)cp, 0, UINT_MAX - 1) != DISK_DOUBLE_ARRAY_MAGIC) {
	bu_log("magic mis-match, got x%x, s/b x%x, file %s, line %d\n",
	       bu_ntohl(*(uint32_t*)cp, 0, UINT_MAX - 1), DISK_DOUBLE_ARRAY_MAGIC, __FILE__, __LINE__);
	bu_log("subscript=%ld, byte_offset=%ld\n",
	       subscript, ecnt[subscript].byte_offset);
	bu_bomb("rt_nmg_import4_fastf() bad magic\n");
    }

    if (pt_type)
	len *= RT_NURB_EXTRACT_COORDS(pt_type);

    count = bu_ntohl(*(uint32_t*)(cp + 4), 0, UINT_MAX - 1);
    if (count != len || count < 0) {
	bu_log("rt_nmg_import4_fastf() subscript=%ld, expected len=%d, got=%d\n",
	       subscript, len, count);
	bu_bomb("rt_nmg_import4_fastf()\n");
    }
    ret = (fastf_t *)bu_malloc(count * sizeof(fastf_t), "rt_nmg_import4_fastf[]");
    if (!mat) {
	scanp = (double *)bu_malloc(count * sizeof(double), "scanp");
	bu_cv_ntohd((unsigned char *)scanp, cp + (4+4), count);
	/* read as double, return as fastf_t */
	for (i=0; i<count; i++) {
	    ret[i] = scanp[i];
	}
	bu_free(scanp, "scanp");
	return ret;
    }

    /*
     * An amazing amount of work: transform all points by 4x4 mat.
     * Need to know width of data points, may be 3, or 4-tuples.
     * The vector times matrix calculation can't be done in place.
     */
    tmp = (double *)bu_malloc(count * sizeof(double), "rt_nmg_import4_fastf tmp[]");
    bu_cv_ntohd((unsigned char *)tmp, cp + (4+4), count);

    switch (RT_NURB_EXTRACT_COORDS(pt_type)) {
	case 3:
	    if (RT_NURB_IS_PT_RATIONAL(pt_type)) bu_bomb("rt_nmg_import4_fastf() Rational 3-tuple?\n");
	    for (count -= 3; count >= 0; count -= 3) {
		MAT4X3PNT(&ret[count], mat, &tmp[count]);
	    }
	    break;
	case 4:
	    if (!RT_NURB_IS_PT_RATIONAL(pt_type)) bu_bomb("rt_nmg_import4_fastf() non-rational 4-tuple?\n");
	    for (count -= 4; count >= 0; count -= 4) {
		MAT4X4PNT(&ret[count], mat, &tmp[count]);
	    }
	    break;
	default:
	    bu_bomb("rt_nmg_import4_fastf() unsupported # of coords in ctl_point\n");
    }

    bu_free(tmp, "rt_nmg_import4_fastf tmp[]");

    return ret;
}

/*
 * For symmetry with export, use same macro names and arg ordering,
 * but here take from "o" (outboard) variable and put in "i" (internal).
 *
 * NOTE that the "< 0" test here is a comparison with DISK_INDEX_LISTHEAD.
 */
#define INDEX(o, i, ty, elem)	(i)->elem = (struct ty *)ptrs[bu_ntohl(*(uint32_t*)((o)->elem), 0, UINT_MAX - 1)]
#define INDEXL_HD(oo, ii, elem, hd) { \
	int sub; \
	if ((sub = bu_ntohl(*(uint32_t*)((oo)->elem.forw), 0, UINT_MAX - 1)) < 0) \
	    (ii)->elem.forw = &(hd); \
	else (ii)->elem.forw = (struct bu_list *)ptrs[sub]; \
	if ((sub = bu_ntohl(*(uint32_t*)((oo)->elem.back), 0, UINT_MAX - 1)) < 0) \
	    (ii)->elem.back = &(hd); \
	else (ii)->elem.back = (struct bu_list *)ptrs[sub]; }

/* For use with the edgeuse l2 / edge_g eu2_hd secondary list */
/* The subscripts will point to the edgeuse, not the edgeuse's l2 rt_list */
#define INDEXL_HD2(oo, ii, elem, hd) { \
	int sub; \
	struct edgeuse *eu2; \
	if ((sub = bu_ntohl(*(uint32_t*)((oo)->elem.forw), 0, UINT_MAX - 1)) < 0) { \
	    (ii)->elem.forw = &(hd); \
	} else { \
	    eu2 = (struct edgeuse *)ptrs[sub]; \
	    NMG_CK_EDGEUSE(eu2); \
	    (ii)->elem.forw = &eu2->l2; \
	} \
	if ((sub = bu_ntohl(*(uint32_t*)((oo)->elem.back), 0, UINT_MAX - 1)) < 0) { \
	    (ii)->elem.back = &(hd); \
	} else { \
	    eu2 = (struct edgeuse *)ptrs[sub]; \
	    NMG_CK_EDGEUSE(eu2); \
	    (ii)->elem.back = &eu2->l2; \
	} }

/**
 * Import a given structure from disk to memory format.
 *
 * Transform geometry by given matrix.
 */
static int
nmg_idisk(void *op, void *ip, struct nmg_exp_counts *ecnt, int idx, uint32_t **ptrs, const fastf_t *mat, const unsigned char *basep)
/* ptr to in-memory structure */
/* base of disk array */
/* base of whole import record */
{
    int iindex;		/* index in ip */

    iindex = 0;
    switch (ecnt[idx].kind) {
	case NMG_KIND_MODEL: {
	    struct model *m = (struct model *)op;
	    struct disk_model *d;
	    d = &((struct disk_model *)ip)[iindex];
	    NMG_CK_MODEL(m);
	    NMG_CK_DISKMAGIC(d->magic, DISK_MODEL_MAGIC);
	    INDEXL_HD(d, m, r_hd, m->r_hd);
	}
	    return 0;
	case NMG_KIND_NMGREGION: {
	    struct nmgregion *r = (struct nmgregion *)op;
	    struct disk_nmgregion *d;
	    d = &((struct disk_nmgregion *)ip)[iindex];
	    NMG_CK_REGION(r);
	    NMG_CK_DISKMAGIC(d->magic, DISK_REGION_MAGIC);
	    INDEX(d, r, model, m_p);
	    INDEX(d, r, nmgregion_a, ra_p);
	    INDEXL_HD(d, r, s_hd, r->s_hd);
	    INDEXL_HD(d, r, l, r->m_p->r_hd); /* do after m_p */
	    NMG_CK_MODEL(r->m_p);
	}
	    return 0;
	case NMG_KIND_NMGREGION_A: {
	    struct nmgregion_a *r = (struct nmgregion_a *)op;
	    struct disk_nmgregion_a *d;
	    point_t min;
	    point_t max;

	    /* must be double for import and export */
	    double scanmin[ELEMENTS_PER_POINT];
	    double scanmax[ELEMENTS_PER_POINT];

	    d = &((struct disk_nmgregion_a *)ip)[iindex];
	    NMG_CK_REGION_A(r);
	    NMG_CK_DISKMAGIC(d->magic, DISK_REGION_A_MAGIC);
	    bu_cv_ntohd((unsigned char *)scanmin, d->min_pt, ELEMENTS_PER_POINT);
	    VMOVE(min, scanmin); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scanmax, d->max_pt, ELEMENTS_PER_POINT);
	    VMOVE(max, scanmax); /* convert double to fastf_t */
	    bg_rotate_bbox(r->min_pt, r->max_pt, mat, min, max);
	}
	    return 0;
	case NMG_KIND_SHELL: {
	    struct shell *s = (struct shell *)op;
	    struct disk_shell *d;
	    d = &((struct disk_shell *)ip)[iindex];
	    NMG_CK_SHELL(s);
	    NMG_CK_DISKMAGIC(d->magic, DISK_SHELL_MAGIC);
	    INDEX(d, s, nmgregion, r_p);
	    INDEX(d, s, shell_a, sa_p);
	    INDEXL_HD(d, s, fu_hd, s->fu_hd);
	    INDEXL_HD(d, s, lu_hd, s->lu_hd);
	    INDEXL_HD(d, s, eu_hd, s->eu_hd);
	    INDEX(d, s, vertexuse, vu_p);
	    NMG_CK_REGION(s->r_p);
	    INDEXL_HD(d, s, l, s->r_p->s_hd); /* after s->r_p */
	}
	    return 0;
	case NMG_KIND_SHELL_A: {
	    struct shell_a *sa = (struct shell_a *)op;
	    struct disk_shell_a *d;
	    point_t min;
	    point_t max;

	    /* must be double for import and export */
	    double scanmin[ELEMENTS_PER_POINT];
	    double scanmax[ELEMENTS_PER_POINT];

	    d = &((struct disk_shell_a *)ip)[iindex];
	    NMG_CK_SHELL_A(sa);
	    NMG_CK_DISKMAGIC(d->magic, DISK_SHELL_A_MAGIC);
	    bu_cv_ntohd((unsigned char *)scanmin, d->min_pt, ELEMENTS_PER_POINT);
	    VMOVE(min, scanmin); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scanmax, d->max_pt, ELEMENTS_PER_POINT);
	    VMOVE(max, scanmax); /* convert double to fastf_t */
	    bg_rotate_bbox(sa->min_pt, sa->max_pt, mat, min, max);
	}
	    return 0;
	case NMG_KIND_FACEUSE: {
	    struct faceuse *fu = (struct faceuse *)op;
	    struct disk_faceuse *d;
	    d = &((struct disk_faceuse *)ip)[iindex];
	    NMG_CK_FACEUSE(fu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACEUSE_MAGIC);
	    INDEX(d, fu, shell, s_p);
	    INDEX(d, fu, faceuse, fumate_p);
	    fu->orientation = bu_ntohl(*(uint32_t*)(d->orientation), 0, UINT_MAX - 1);
	    INDEX(d, fu, face, f_p);
	    INDEXL_HD(d, fu, lu_hd, fu->lu_hd);
	    INDEXL_HD(d, fu, l, fu->s_p->fu_hd); /* after fu->s_p */
	    NMG_CK_FACE(fu->f_p);
	    NMG_CK_FACEUSE(fu->fumate_p);
	}
	    return 0;
	case NMG_KIND_FACE: {
	    struct face *f = (struct face *)op;
	    struct disk_face *d;
	    int g_index;

	    d = &((struct disk_face *)ip)[iindex];
	    NMG_CK_FACE(f);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACE_MAGIC);
	    INDEX(d, f, faceuse, fu_p);
	    g_index = bu_ntohl(*(uint32_t*)(d->g), 0, UINT_MAX - 1);
	    f->g.magic_p = (uint32_t *)ptrs[g_index];
	    f->flip = bu_ntohl(*(uint32_t*)(d->flip), 0, UINT_MAX - 1);
	    /* Enroll this face on fg's list of users */
	    NMG_CK_FACE_G_EITHER(f->g.magic_p);
	    INDEXL_HD(d, f, l, f->g.plane_p->f_hd); /* after fu->fg_p set */
	    NMG_CK_FACEUSE(f->fu_p);
	}
	    return 0;
	case NMG_KIND_FACE_G_PLANE: {
	    struct face_g_plane *fg = (struct face_g_plane *)op;
	    struct disk_face_g_plane *d;
	    plane_t plane;

	    /* must be double for import and export */
	    double scan[ELEMENTS_PER_PLANE];

	    d = &((struct disk_face_g_plane *)ip)[iindex];
	    NMG_CK_FACE_G_PLANE(fg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACE_G_PLANE_MAGIC);
	    INDEXL_HD(d, fg, f_hd, fg->f_hd);
	    bu_cv_ntohd((unsigned char *)scan, d->N, ELEMENTS_PER_PLANE);
	    HMOVE(plane, scan); /* convert double to fastf_t */
	    bg_rotate_plane(fg->N, mat, plane);
	}
	    return 0;
	case NMG_KIND_FACE_G_SNURB: {
	    struct face_g_snurb *fg = (struct face_g_snurb *)op;
	    struct disk_face_g_snurb *d;
	    d = &((struct disk_face_g_snurb *)ip)[iindex];
	    NMG_CK_FACE_G_SNURB(fg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACE_G_SNURB_MAGIC);
	    INDEXL_HD(d, fg, f_hd, fg->f_hd);
	    fg->order[0] = bu_ntohl(*(uint32_t*)(d->u_order), 0, UINT_MAX - 1);
	    fg->order[1] = bu_ntohl(*(uint32_t*)(d->v_order), 0, UINT_MAX - 1);
	    fg->u.k_size = bu_ntohl(*(uint32_t*)(d->u_size), 0, UINT_MAX - 1);
	    fg->u.knots = nmg_import4_fastf(basep,
					       ecnt,
					       bu_ntohl(*(uint32_t*)(d->u_knots), 0, UINT_MAX - 1),
					       NULL,
					       fg->u.k_size,
					       0);
	    fg->v.k_size = bu_ntohl(*(uint32_t*)(d->v_size), 0, UINT_MAX - 1);
	    fg->v.knots = nmg_import4_fastf(basep,
					       ecnt,
					       bu_ntohl(*(uint32_t*)(d->v_knots), 0, UINT_MAX - 1),
					       NULL,
					       fg->v.k_size,
					       0);
	    fg->s_size[0] = bu_ntohl(*(uint32_t*)(d->us_size), 0, UINT_MAX - 1);
	    fg->s_size[1] = bu_ntohl(*(uint32_t*)(d->vs_size), 0, UINT_MAX - 1);
	    fg->pt_type = bu_ntohl(*(uint32_t*)(d->pt_type), 0, UINT_MAX - 1);
	    /* Transform ctl_points by 'mat' */
	    fg->ctl_points = nmg_import4_fastf(basep,
						  ecnt,
						  bu_ntohl(*(uint32_t*)(d->ctl_points), 0, UINT_MAX - 1),
						  (matp_t)mat,
						  fg->s_size[0] * fg->s_size[1],
						  fg->pt_type);
	}
	    return 0;
	case NMG_KIND_LOOPUSE: {
	    struct loopuse *lu = (struct loopuse *)op;
	    struct disk_loopuse *d;
	    int up_index;
	    int up_kind;

	    d = &((struct disk_loopuse *)ip)[iindex];
	    NMG_CK_LOOPUSE(lu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_LOOPUSE_MAGIC);
	    up_index = bu_ntohl(*(uint32_t*)(d->up), 0, UINT_MAX - 1);
	    lu->up.magic_p = ptrs[up_index];
	    INDEX(d, lu, loopuse, lumate_p);
	    lu->orientation = bu_ntohl(*(uint32_t*)(d->orientation), 0, UINT_MAX - 1);
	    INDEX(d, lu, loop, l_p);
	    up_kind = ecnt[up_index].kind;
	    if (up_kind == NMG_KIND_FACEUSE) {
		INDEXL_HD(d, lu, l, lu->up.fu_p->lu_hd);
	    } else if (up_kind == NMG_KIND_SHELL) {
		INDEXL_HD(d, lu, l, lu->up.s_p->lu_hd);
	    } else bu_log("bad loopuse up, index=%d, kind=%d\n", up_index, up_kind);
	    INDEXL_HD(d, lu, down_hd, lu->down_hd);
	    if (lu->down_hd.forw == BU_LIST_NULL)
		bu_bomb("rt_nmg_idisk: null loopuse down_hd.forw\n");
	    NMG_CK_LOOP(lu->l_p);
	}
	    return 0;
	case NMG_KIND_LOOP: {
	    struct loop *loop = (struct loop *)op;
	    struct disk_loop *d;
	    d = &((struct disk_loop *)ip)[iindex];
	    NMG_CK_LOOP(loop);
	    NMG_CK_DISKMAGIC(d->magic, DISK_LOOP_MAGIC);
	    INDEX(d, loop, loopuse, lu_p);
	    INDEX(d, loop, loop_a, la_p);
	    NMG_CK_LOOPUSE(loop->lu_p);
	}
	    return 0;
	case NMG_KIND_LOOP_A: {
	    struct loop_a *lg = (struct loop_a *)op;
	    struct disk_loop_a *d;
	    point_t min;
	    point_t max;

	    /* must be double for import and export */
	    double scanmin[ELEMENTS_PER_POINT];
	    double scanmax[ELEMENTS_PER_POINT];

	    d = &((struct disk_loop_a *)ip)[iindex];
	    NMG_CK_LOOP_A(lg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_LOOP_A_MAGIC);
	    bu_cv_ntohd((unsigned char *)scanmin, d->min_pt, ELEMENTS_PER_POINT);
	    VMOVE(min, scanmin); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scanmax, d->max_pt, ELEMENTS_PER_POINT);
	    VMOVE(max, scanmax); /* convert double to fastf_t */
	    bg_rotate_bbox(lg->min_pt, lg->max_pt, mat, min, max);
	}
	    return 0;
	case NMG_KIND_EDGEUSE: {
	    struct edgeuse *eu = (struct edgeuse *)op;
	    struct disk_edgeuse *d;
	    int up_index;
	    int up_kind;

	    d = &((struct disk_edgeuse *)ip)[iindex];
	    NMG_CK_EDGEUSE(eu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGEUSE_MAGIC);
	    up_index = bu_ntohl(*(uint32_t*)(d->up), 0, UINT_MAX - 1);
	    eu->up.magic_p = ptrs[up_index];
	    INDEX(d, eu, edgeuse, eumate_p);
	    INDEX(d, eu, edgeuse, radial_p);
	    INDEX(d, eu, edge, e_p);
	    eu->orientation = bu_ntohl(*(uint32_t*)(d->orientation), 0, UINT_MAX - 1);
	    INDEX(d, eu, vertexuse, vu_p);
	    up_kind = ecnt[up_index].kind;
	    if (up_kind == NMG_KIND_LOOPUSE) {
		INDEXL_HD(d, eu, l, eu->up.lu_p->down_hd);
	    } else if (up_kind == NMG_KIND_SHELL) {
		INDEXL_HD(d, eu, l, eu->up.s_p->eu_hd);
	    } else bu_log("bad edgeuse up, index=%d, kind=%d\n", up_index, up_kind);
	    eu->g.magic_p = ptrs[bu_ntohl(*(uint32_t*)(d->g), 0, UINT_MAX - 1)];
	    NMG_CK_EDGE(eu->e_p);
	    NMG_CK_EDGEUSE(eu->eumate_p);
	    NMG_CK_EDGEUSE(eu->radial_p);
	    NMG_CK_VERTEXUSE(eu->vu_p);
	    if (eu->g.magic_p != NULL) {
		NMG_CK_EDGE_G_EITHER(eu->g.magic_p);

		/* Note that l2 subscripts will be for edgeuse, not l2 */
		/* g.lseg_p->eu_hd2 is a pun for g.cnurb_p->eu_hd2 also */
		INDEXL_HD2(d, eu, l2, eu->g.lseg_p->eu_hd2);
	    } else {
		eu->l2.forw = &eu->l2;
		eu->l2.back = &eu->l2;
	    }
	}
	    return 0;
	case NMG_KIND_EDGE: {
	    struct edge *e = (struct edge *)op;
	    struct disk_edge *d;
	    d = &((struct disk_edge *)ip)[iindex];
	    NMG_CK_EDGE(e);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGE_MAGIC);
	    e->is_real = bu_ntohl(*(uint32_t*)(d->is_real), 0, UINT_MAX - 1);
	    INDEX(d, e, edgeuse, eu_p);
	    NMG_CK_EDGEUSE(e->eu_p);
	}
	    return 0;
	case NMG_KIND_EDGE_G_LSEG: {
	    struct edge_g_lseg *eg = (struct edge_g_lseg *)op;
	    struct disk_edge_g_lseg *d;
	    point_t pt;
	    vect_t dir;

	    /* must be double for import and export */
	    double scanpt[ELEMENTS_PER_POINT];
	    double scandir[ELEMENTS_PER_VECT];

	    d = &((struct disk_edge_g_lseg *)ip)[iindex];
	    NMG_CK_EDGE_G_LSEG(eg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGE_G_LSEG_MAGIC);
	    /* Forw subscript points to edgeuse, not edgeuse2 */
	    INDEXL_HD2(d, eg, eu_hd2, eg->eu_hd2);
	    bu_cv_ntohd((unsigned char *)scanpt, d->e_pt, ELEMENTS_PER_POINT);
	    VMOVE(pt, scanpt); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scandir, d->e_dir, ELEMENTS_PER_VECT);
	    VMOVE(dir, scandir); /* convert double to fastf_t */
	    MAT4X3PNT(eg->e_pt, mat, pt);
	    MAT4X3VEC(eg->e_dir, mat, dir);
	}
	    return 0;
	case NMG_KIND_EDGE_G_CNURB: {
	    struct edge_g_cnurb *eg = (struct edge_g_cnurb *)op;
	    struct disk_edge_g_cnurb *d;
	    d = &((struct disk_edge_g_cnurb *)ip)[iindex];
	    NMG_CK_EDGE_G_CNURB(eg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_EDGE_G_CNURB_MAGIC);
	    INDEXL_HD2(d, eg, eu_hd2, eg->eu_hd2);
	    eg->order = bu_ntohl(*(uint32_t*)(d->order), 0, UINT_MAX - 1);

	    /* If order is zero, so is everything else */
	    if (eg->order == 0) return 0;

	    eg->k.k_size = bu_ntohl(*(uint32_t*)(d->k_size), 0, UINT_MAX - 1);
	    eg->k.knots = nmg_import4_fastf(basep,
					       ecnt,
					       bu_ntohl(*(uint32_t*)(d->knots), 0, UINT_MAX - 1),
					       NULL,
					       eg->k.k_size,
					       0);
	    eg->c_size = bu_ntohl(*(uint32_t*)(d->c_size), 0, UINT_MAX - 1);
	    eg->pt_type = bu_ntohl(*(uint32_t*)(d->pt_type), 0, UINT_MAX - 1);
	    /*
	     * The curve's control points are in parameter space.
	     * They do NOT get transformed!
	     */
	    if (RT_NURB_EXTRACT_PT_TYPE(eg->pt_type) == RT_NURB_PT_UV) {
		/* UV coords on snurb surface don't get xformed */
		eg->ctl_points = nmg_import4_fastf(basep,
						      ecnt,
						      bu_ntohl(*(uint32_t*)(d->ctl_points), 0, UINT_MAX - 1),
						      NULL,
						      eg->c_size,
						      eg->pt_type);
	    } else {
		/* XYZ coords on planar face DO get xformed */
		eg->ctl_points = nmg_import4_fastf(basep,
						      ecnt,
						      bu_ntohl(*(uint32_t*)(d->ctl_points), 0, UINT_MAX - 1),
						      (matp_t)mat,
						      eg->c_size,
						      eg->pt_type);
	    }
	}
	    return 0;
	case NMG_KIND_VERTEXUSE: {
	    struct vertexuse *vu = (struct vertexuse *)op;
	    struct disk_vertexuse *d;
	    d = &((struct disk_vertexuse *)ip)[iindex];
	    NMG_CK_VERTEXUSE(vu);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEXUSE_MAGIC);
	    vu->up.magic_p = ptrs[bu_ntohl(*(uint32_t*)(d->up), 0, UINT_MAX - 1)];
	    INDEX(d, vu, vertex, v_p);
	    vu->a.magic_p = ptrs[bu_ntohl(*(uint32_t*)(d->a), 0, UINT_MAX - 1)];
	    NMG_CK_VERTEX(vu->v_p);
	    if (vu->a.magic_p)NMG_CK_VERTEXUSE_A_EITHER(vu->a.magic_p);
	    INDEXL_HD(d, vu, l, vu->v_p->vu_hd);
	}
	    return 0;
	case NMG_KIND_VERTEXUSE_A_PLANE: {
	    struct vertexuse_a_plane *vua = (struct vertexuse_a_plane *)op;
	    struct disk_vertexuse_a_plane *d;
	    /* must be double for import and export */
	    double norm[ELEMENTS_PER_VECT];

	    d = &((struct disk_vertexuse_a_plane *)ip)[iindex];
	    NMG_CK_VERTEXUSE_A_PLANE(vua);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEXUSE_A_PLANE_MAGIC);
	    bu_cv_ntohd((unsigned char *)norm, d->N, ELEMENTS_PER_VECT);
	    MAT4X3VEC(vua->N, mat, norm);
	}
	    return 0;
	case NMG_KIND_VERTEXUSE_A_CNURB: {
	    struct vertexuse_a_cnurb *vua = (struct vertexuse_a_cnurb *)op;
	    struct disk_vertexuse_a_cnurb *d;

	    /* must be double for import and export */
	    double scan[3];

	    d = &((struct disk_vertexuse_a_cnurb *)ip)[iindex];
	    NMG_CK_VERTEXUSE_A_CNURB(vua);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEXUSE_A_CNURB_MAGIC);
	    /* These parameters are invariant w.r.t. 'mat' */
	    bu_cv_ntohd((unsigned char *)scan, d->param, 3);
	    VMOVE(vua->param, scan); /* convert double to fastf_t */
	}
	    return 0;
	case NMG_KIND_VERTEX: {
	    struct vertex *v = (struct vertex *)op;
	    struct disk_vertex *d;
	    d = &((struct disk_vertex *)ip)[iindex];
	    NMG_CK_VERTEX(v);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEX_MAGIC);
	    INDEXL_HD(d, v, vu_hd, v->vu_hd);
	    INDEX(d, v, vertex_g, vg_p);
	}
	    return 0;
	case NMG_KIND_VERTEX_G: {
	    struct vertex_g *vg = (struct vertex_g *)op;
	    struct disk_vertex_g *d;
	    /* must be double for import and export */
	    double pt[ELEMENTS_PER_POINT];

	    d = &((struct disk_vertex_g *)ip)[iindex];
	    NMG_CK_VERTEX_G(vg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_VERTEX_G_MAGIC);
	    bu_cv_ntohd((unsigned char *)pt, d->coord, ELEMENTS_PER_POINT);
	    MAT4X3PNT(vg->coord, mat, pt);
	}
	    return 0;
    }
    bu_log("rt_nmg_idisk kind=%d unknown\n", ecnt[idx].kind);
    return -1;
}
#undef INDEX
#undef INDEXL


struct model *
nmg_import(struct bu_external *ep, const mat_t mat, int ver)
{
    if (!ep || !(ver == 4 || ver == 5))
	return NULL;

    if (ver == 4)
	return NULL;

    struct model *m;
    struct bn_tol tol;
    int maxindex;
    int kind;
    int kind_counts[NMG_N_KINDS];
    unsigned char *dp;		/* data pointer */
    void *startdata;	/* data pointer */
    uint32_t **real_ptrs;
    uint32_t **ptrs;
    struct nmg_exp_counts *ecnt;
    int i;
    static uint32_t bad_magic = 0x999;

    BU_CK_EXTERNAL(ep);
    dp = (unsigned char *)ep->ext_buf;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    {
	int version;
	version = bu_ntohl(*(uint32_t*)dp, 0, UINT_MAX - 1);
	dp+= SIZEOF_NETWORK_LONG;
	if (version != DISK_MODEL_VERSION) {
	    bu_log("nmg_import: expected NMG '.g' format version %d, got %d, aborting nmg solid import\n",
		   DISK_MODEL_VERSION, version);
	    return NULL;
	}
    }
    maxindex = 1;
    for (kind =0; kind < NMG_N_KINDS; kind++) {
	kind_counts[kind] = bu_ntohl(*(uint32_t*)dp, 0, UINT_MAX - 1);
	dp+= SIZEOF_NETWORK_LONG;
	maxindex += kind_counts[kind];
    }

    startdata = dp;

    /* Collect overall new subscripts, and structure-specific indices */
    ecnt = (struct nmg_exp_counts *) bu_calloc(maxindex+3,
					       sizeof(struct nmg_exp_counts), "ecnt[]");
    real_ptrs = (uint32_t **)bu_calloc(maxindex+3, sizeof(uint32_t *), "ptrs[]");
    /* some safety checking.  Indexing by, -1, 0, n+1, N+2 give interesting results */
    ptrs = real_ptrs+1;
    ptrs[-1] = &bad_magic;
    ptrs[0] = NULL;
    ptrs[maxindex] = &bad_magic;
    ptrs[maxindex+1] = &bad_magic;

    m = nmg_ialloc(ptrs, ecnt, kind_counts);

    nmg_i2alloc(ecnt, dp, kind_counts);

    /* Now import each structure, in turn */
    for (i=1; i < maxindex; i++) {
	/* We know that the DOUBLE_ARRAY is the last thing to process */
	if (ecnt[i].kind == NMG_KIND_DOUBLE_ARRAY) break;
	if (nmg_idisk((void *)(ptrs[i]), (void *)dp, ecnt,
			 i, ptrs, mat, (unsigned char *)startdata) < 0) {
	    return NULL;
	}
	dp += nmg_disk_sizes[ecnt[i].kind];
    }

    /* Face min_pt and max_pt are not stored, so this is mandatory. */
    nmg_rebound(m, &tol);

    NMG_CK_MODEL(m);
    bu_free((char *)ecnt, "ecnt[]");
    bu_free((char *)real_ptrs, "ptrs[]");

    if (nmg_debug) {
	nmg_vmodel(m);
    }
    return 0;		/* OK */
}


struct bu_external *
nmg_export(struct model *m, double local2mm, int ver)
{
    if (!m || local2mm < 0)
	return NULL;
    if (ver != 4 && ver != 5)
	return NULL;

    if (ver == 4)
	// TODO - old export
	return NULL;

    struct nmg_struct_counts cntbuf;
    struct nmg_exp_counts *ecnt;
    uint32_t **ptrs;
    int kind_counts[NMG_N_KINDS];
    void *disk_arrays[NMG_N_KINDS];
    int tot_size;
    int kind;
    int double_count;
    int subscript, fastf_byte_count;

    NMG_CK_MODEL(m);

    memset((char *)&cntbuf, 0, sizeof(cntbuf));
    ptrs = nmg_m_struct_count(&cntbuf, m);

    ecnt = (struct nmg_exp_counts *)bu_calloc(m->maxindex+1,
	    sizeof(struct nmg_exp_counts), "ecnt[]");

    for (int i=0; i<NMG_N_KINDS; i++) {
        kind_counts[i] = 0;
    }
    subscript = 1;
    double_count = 0;
    fastf_byte_count = 0;
    for (int i=0; i< m->maxindex; i++) {
        if (ptrs[i] == NULL) {
            ecnt[i].kind = -1;
            continue;
        }

	kind = nmg_magic_to_kind(*(ptrs[i]));
	ecnt[i].per_struct_index = kind_counts[kind]++;
	ecnt[i].kind = kind;

	/*
	 * SNURB and CNURBS are variable sized and as such need
	 * special handling
	 */
	if (kind == NMG_KIND_FACE_G_SNURB) {
	    struct face_g_snurb *fg;
	    int ndouble;
	    fg = (struct face_g_snurb *)ptrs[i];
	    ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
	    kind_counts[NMG_KIND_DOUBLE_ARRAY] += 3;
	    ndouble = fg->u.k_size +
		fg->v.k_size +
		fg->s_size[0] * fg->s_size[1] *
		RT_NURB_EXTRACT_COORDS(fg->pt_type);
	    double_count += ndouble;
	    ecnt[i].byte_offset = fastf_byte_count;
	    fastf_byte_count += 3*(4*4) + 89*ndouble;
	} else if (kind == NMG_KIND_EDGE_G_CNURB) {
	    struct edge_g_cnurb *eg;
	    int ndouble;
	    eg = (struct edge_g_cnurb *)ptrs[i];
	    ecnt[i].first_fastf_relpos =
		kind_counts[NMG_KIND_DOUBLE_ARRAY];
	    if (eg->order != 0) {
		kind_counts[NMG_KIND_DOUBLE_ARRAY] += 2;
		ndouble = eg->k.k_size +eg->c_size *
		    RT_NURB_EXTRACT_COORDS(eg->pt_type);
		double_count += ndouble;
		ecnt[i].byte_offset = fastf_byte_count;
		fastf_byte_count += 2*(4+4) + 8*ndouble;
	    }
	}
    }
    /* Compacting wanted */
    kind_counts[NMG_KIND_NMGREGION_A] = 0;
    kind_counts[NMG_KIND_SHELL_A] = 0;
    kind_counts[NMG_KIND_LOOP_A] = 0;

    /* Assign new subscripts to ascending struts of the same kind */
    for (kind=0; kind < NMG_N_KINDS; kind++) {
	/* Compacting */
	if (kind == NMG_KIND_NMGREGION_A ||
	    kind == NMG_KIND_SHELL_A ||
	    kind == NMG_KIND_LOOP_A) {
	    for (int i=0; i<m->maxindex; i++) {
		if (ptrs[i] == NULL) continue;
		if (ecnt[i].kind != kind) continue;
		ecnt[i].new_subscript = DISK_INDEX_NULL;
	    }
	    continue;
	}

	for (int i=0; i< m->maxindex;i++) {
	    if (ptrs[i] == NULL) continue;
	    if (ecnt[i].kind != kind) continue;
	    ecnt[i].new_subscript = subscript++;
	}
    }
    /* Tack on the variable sized fastf_t arrays at the end */
    subscript += kind_counts[NMG_KIND_DOUBLE_ARRAY];

    /* Now do some checking to make sure the world is not totally mad */
    for (int i=0; i<m->maxindex; i++) {
	if (ptrs[i] == NULL) continue;

	if (nmg_index_of_struct(ptrs[i]) != i) {
	    bu_log("***ERROR, ptrs[%d]->index = %d\n",
		   i, nmg_index_of_struct(ptrs[i]));
	}
	if (nmg_magic_to_kind(*ptrs[i]) != ecnt[i].kind) {
	    bu_log("***ERROR, ptrs[%d] kind(%d) != %d\n",
		   i, nmg_magic_to_kind(*ptrs[i]),
		   ecnt[i].kind);
	}

    }

    tot_size = 0;
    for (int i=0; i< NMG_N_KINDS; i++) {
	if (kind_counts[i] <= 0) {
	    disk_arrays[i] = ((void *)0);
	    continue;
	}
	tot_size += kind_counts[i] * nmg_disk_sizes[i];
    }

    /* Account for variable sized double arrays, at the end */
    tot_size += kind_counts[NMG_KIND_DOUBLE_ARRAY] * (4+4) +
	double_count*8;

    ecnt[0].byte_offset = subscript; /* implicit arg to reindex() */
    tot_size += SIZEOF_NETWORK_LONG*(NMG_N_KINDS + 1); /* one for magic */

    struct bu_external *ep;
    unsigned char *dp;
    BU_GET(ep, struct bu_external);
    BU_EXTERNAL_INIT(ep);
    ep->ext_nbytes = tot_size;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "nmg external5");
    dp = ep->ext_buf;
    *(uint32_t *)dp = htonl(DISK_MODEL_VERSION);
    dp+=SIZEOF_NETWORK_LONG;

    for (kind=0; kind <NMG_N_KINDS; kind++) {
	*(uint32_t *)dp = htonl(kind_counts[kind]);
	dp+=SIZEOF_NETWORK_LONG;
    }
    for (int i=0; i< NMG_N_KINDS; i++) {
	disk_arrays[i] = dp;
	dp += kind_counts[i] * nmg_disk_sizes[i];
    }
    nmg_fastf_p = (unsigned char*)disk_arrays[NMG_KIND_DOUBLE_ARRAY];

    for (int i = m->maxindex-1;i >=0; i--) {
	if (ptrs[i] == NULL) continue;
	kind = ecnt[i].kind;
	if (kind_counts[kind] <= 0) continue;
	nmg_edisk((void *)(disk_arrays[kind]),
		     (void *)(ptrs[i]), ecnt, i, local2mm);
    }

    bu_free((char *)ptrs, "ptrs[]");
    bu_free((char *)ecnt, "ecnt[]");

    return ep;
}



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
