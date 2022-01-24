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
