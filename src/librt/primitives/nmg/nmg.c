/*                           N M G . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2014 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/nmg/nmg.c
 *
 * Intersect a ray with an NMG solid.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bin.h"

#include "bu/cv.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"


/* rt_nmg_internal is just "model", from nmg.h */

#define NMG_SPEC_START_MAGIC 6014061
#define NMG_SPEC_END_MAGIC 7013061

/* This is the solid information specific to an nmg solid */
struct nmg_specific {
    uint32_t nmg_smagic;	/* STRUCT START magic number */
    struct shell *nmg_shell;
    char *manifolds;		/* structure 1-3manifold table */
    vect_t nmg_invdir;
    uint32_t nmg_emagic;	/* STRUCT END magic number */
};


struct tmp_v {
    point_t pt;
    struct vertex *v;
};


/**
 * Calculate the bounding box for an N-Manifold Geometry
 */
int
rt_nmg_bbox(struct rt_db_internal *ip, point_t *min, point_t * max, const struct bn_tol *UNUSED(tol)) {
    struct shell *s;

    RT_CK_DB_INTERNAL(ip);
    s = (struct shell *)ip->idb_ptr;
    NMG_CK_SHELL(s);

    nmg_shell_bb(*min, *max, s);
    return 0;
}


/**
 * Given a pointer to a ged database record, and a transformation
 * matrix, determine if this is a valid nmg, and if so, precompute
 * various terms of the formula.
 *
 * returns 0 if nmg is ok and !0 on error in description
 *
 * implicit return - a struct nmg_specific is created, and its
 * address is stored in stp->st_specific for use by nmg_shot().
 */
int
rt_nmg_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct shell *s;
    struct nmg_specific *nmg_s;
    vect_t work;

    RT_CK_DB_INTERNAL(ip);
    s = (struct shell *)ip->idb_ptr;
    NMG_CK_SHELL(s);

    if (stp->st_meth->ft_bbox(ip, &(stp->st_min), &(stp->st_max), &(rtip->rti_tol))) return 1;

    VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
    VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);
    stp->st_aradius = stp->st_bradius = MAGNITUDE(work);

    BU_GET(nmg_s, struct nmg_specific);
    stp->st_specific = (void *)nmg_s;
    nmg_s->nmg_shell = s;
    ip->idb_ptr = (void *)NULL;
    nmg_s->nmg_smagic = NMG_SPEC_START_MAGIC;
    nmg_s->nmg_emagic = NMG_SPEC_END_MAGIC;

    /* build table indicating the manifold level of each sub-element
     * of NMG solid
     */
    nmg_s->manifolds = nmg_manifolds(s);

    return 0;
}


void
rt_nmg_print(const struct soltab *stp)
{
    struct shell *s = (struct shell *)stp->st_specific;

    NMG_CK_SHELL(s);
    nmg_pr_s(s);
}


/**
 * Intersect a ray with a nmg.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_nmg_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)

/* info about the ray */

/* intersection w/ ray */
{
    struct ray_data rd;
    int status;
    struct nmg_specific *nmg =
	(struct nmg_specific *)stp->st_specific;

    if (nmg_debug & DEBUG_NMGRT) {
	bu_log("rt_nmg_shot()\n\t");
	rt_pr_tol(&ap->a_rt_i->rti_tol);
    }

    /* check validity of nmg specific structure */
    if (nmg->nmg_smagic != NMG_SPEC_START_MAGIC)
	bu_bomb("start of NMG st_specific structure corrupted\n");

    if (nmg->nmg_emagic != NMG_SPEC_END_MAGIC)
	bu_bomb("end of NMG st_specific structure corrupted\n");

    /* Compute the inverse of the direction cosines */
    if (!ZERO(rp->r_dir[X])) {
	nmg->nmg_invdir[X]=1.0/rp->r_dir[X];
    } else {
	nmg->nmg_invdir[X] = INFINITY;
	rp->r_dir[X] = 0.0;
    }
    if (!ZERO(rp->r_dir[Y])) {
	nmg->nmg_invdir[Y]=1.0/rp->r_dir[Y];
    } else {
	nmg->nmg_invdir[Y] = INFINITY;
	rp->r_dir[Y] = 0.0;
    }
    if (!ZERO(rp->r_dir[Z])) {
	nmg->nmg_invdir[Z]=1.0/rp->r_dir[Z];
    } else {
	nmg->nmg_invdir[Z] = INFINITY;
	rp->r_dir[Z] = 0.0;
    }

    /* build the NMG per-ray data structure */
    rd.rd_s = nmg->nmg_shell;
    rd.manifolds = nmg->manifolds;
    VMOVE(rd.rd_invdir, nmg->nmg_invdir);
    rd.rp = rp;
    rd.tol = &ap->a_rt_i->rti_tol;
    rd.ap = ap;
    rd.stp = stp;
    rd.seghead = seghead;
    rd.classifying_ray = 0;

    /* create a table to keep track of which elements have been
     * processed before and which haven't.  Elements in this table
     * will either be (NULL) if item not previously processed or a
     * hitmiss ptr if item was previously processed
     */
    rd.hitmiss = (struct hitmiss **)bu_calloc(rd.rd_s->maxindex,
					      sizeof(struct hitmiss *), "nmg geom hit list");

    /* initialize the lists of things that have been hit/missed */
    BU_LIST_INIT(&rd.rd_hit);
    BU_LIST_INIT(&rd.rd_miss);
    rd.magic = NMG_RAY_DATA_MAGIC;

    /* intersect the ray with the geometry (sets surfno) */
    nmg_isect_ray_shell(&rd);

    /* build the segment lists */
    status = nmg_ray_segs(&rd);

    /* free the hitmiss table */
    bu_free((char *)rd.hitmiss, "free nmg geom hit list");

    return status;
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_nmg_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    if (!hitp || !rp)
	return;

    if (stp) RT_CK_SOLTAB(stp);
    RT_CK_RAY(rp);
    RT_CK_HIT(hitp);

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
}


/**
 * Return the curvature of the nmg.
 */
void
rt_nmg_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (!cvp || !hitp)
	return;

    RT_CK_HIT(hitp);
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * For a hit on the surface of an nmg, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_nmg_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp) return;
}


void
rt_nmg_free(struct soltab *stp)
{
    struct nmg_specific *nmg =
	(struct nmg_specific *)stp->st_specific;

    nmg_ks(nmg->nmg_shell);
    BU_PUT(nmg, struct nmg_specific);
    stp->st_specific = NULL; /* sanity */
}


int
rt_nmg_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct shell *s;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    s = (struct shell *)ip->idb_ptr;
    NMG_CK_SHELL(s);

    nmg_s_to_vlist(vhead, s, 0);

    return 0;
}


/**
 * XXX This routine "destroys" the internal nmg solid.  This means
 * that once you tessellate an NMG solid, your in-memory copy becomes
 * invalid, and you can't do anything else with it until you get a new
 * copy from disk.
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to shell that holds this tessellation.
 */
int
rt_nmg_tess(struct shell **s, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    *s = (struct shell *)ip->idb_ptr;
    ip->idb_ptr = ((void *)0);

    return 0;
}


#define NMG_CK_DISKMAGIC(_cp, _magic)	\
    if (ntohl(*(uint32_t*)_cp) != _magic) { \
	bu_log("NMG_CK_DISKMAGIC: magic mis-match, got x%x, s/b x%x, file %s, line %d\n", \
	       ntohl(*(uint32_t*)_cp), _magic, __FILE__, __LINE__); \
	bu_bomb("bad magic\n"); \
    }


/* ----------------------------------------------------------------------
 *
 * Definitions for the binary, machine-independent format of the NMG
 * data structures.
 *
 * There are two special values that may be assigned to an
 * disk_index_t to signal special processing when the structure is
 * re-import4ed.
 */
#define DISK_INDEX_NULL 0
#define DISK_INDEX_LISTHEAD -1

#define DISK_MODEL_VERSION 1	/* V0 was Release 4.0 */

typedef unsigned char disk_index_t[4]; /* uint32_t buffer */
struct disk_rt_list {
    disk_index_t forw;
    disk_index_t back;
};


#define DISK_MODEL_MAGIC 0x4e6d6f64	/* Nmod */
struct disk_model {
    unsigned char magic[4];
    unsigned char version[4];	/* unused */
    struct disk_rt_list r_hd;
};


#define DISK_REGION_MAGIC 0x4e726567	/* Nreg */
struct disk_nmgregion {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t m_p;
    disk_index_t ra_p;
    struct disk_rt_list s_hd;
};


#define DISK_REGION_A_MAGIC 0x4e725f61	/* Nr_a */
struct disk_nmgregion_a {
    unsigned char magic[4];
    unsigned char min_pt[3*8];
    unsigned char max_pt[3*8];
};


#define DISK_SHELL_MAGIC 0x4e73686c	/* Nshl */
struct disk_shell {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t r_p;
    disk_index_t sa_p;
    struct disk_rt_list fu_hd;
    struct disk_rt_list lu_hd;
    struct disk_rt_list eu_hd;
    disk_index_t vu_p;
};


#define DISK_SHELL_A_MAGIC 0x4e735f61	/* Ns_a */
struct disk_shell_a {
    unsigned char magic[4];
    unsigned char min_pt[3*8];
    unsigned char max_pt[3*8];
};


#define DISK_FACE_MAGIC 0x4e666163	/* Nfac */
struct disk_face {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t fu_p;
    disk_index_t g;
    unsigned char flip[4];
};


#define DISK_FACE_G_PLANE_MAGIC 0x4e666770	/* Nfgp */
struct disk_face_g_plane {
    unsigned char magic[4];
    struct disk_rt_list f_hd;
    unsigned char N[4*8];
};


#define DISK_FACE_G_SNURB_MAGIC 0x4e666773	/* Nfgs */
struct disk_face_g_snurb {
    unsigned char magic[4];
    struct disk_rt_list f_hd;
    unsigned char u_order[4];
    unsigned char v_order[4];
    unsigned char u_size[4];	/* u.k_size */
    unsigned char v_size[4];	/* v.k_size */
    disk_index_t u_knots;	/* u.knots subscript */
    disk_index_t v_knots;	/* v.knots subscript */
    unsigned char us_size[4];
    unsigned char vs_size[4];
    unsigned char pt_type[4];
    disk_index_t ctl_points;	/* subscript */
};


#define DISK_FACEUSE_MAGIC 0x4e667520	/* Nfu */
struct disk_faceuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t s_p;
    disk_index_t fumate_p;
    unsigned char orientation[4];
    disk_index_t f_p;
    disk_index_t fua_p;
    struct disk_rt_list lu_hd;
};


#define DISK_LOOP_MAGIC 0x4e6c6f70	/* Nlop */
struct disk_loop {
    unsigned char magic[4];
    disk_index_t lu_p;
    disk_index_t lg_p;
};


#define DISK_LOOP_G_MAGIC 0x4e6c5f67	/* Nl_g */
struct disk_loop_g {
    unsigned char magic[4];
    unsigned char min_pt[3*8];
    unsigned char max_pt[3*8];
};


#define DISK_LOOPUSE_MAGIC 0x4e6c7520	/* Nlu */
struct disk_loopuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t up;
    disk_index_t lumate_p;
    unsigned char orientation[4];
    disk_index_t l_p;
    disk_index_t lua_p;
    struct disk_rt_list down_hd;
};


#define DISK_EDGE_MAGIC 0x4e656467	/* Nedg */
struct disk_edge {
    unsigned char magic[4];
    disk_index_t eu_p;
    unsigned char is_real[4];
};


#define DISK_EDGE_G_LSEG_MAGIC 0x4e65676c	/* Negl */
struct disk_edge_g_lseg {
    unsigned char magic[4];
    struct disk_rt_list eu_hd2;
    unsigned char e_pt[3*8];
    unsigned char e_dir[3*8];
};


#define DISK_EDGE_G_CNURB_MAGIC 0x4e656763	/* Negc */
struct disk_edge_g_cnurb {
    unsigned char magic[4];
    struct disk_rt_list eu_hd2;
    unsigned char order[4];
    unsigned char k_size[4];	/* k.k_size */
    disk_index_t knots;		/* knot.knots subscript */
    unsigned char c_size[4];
    unsigned char pt_type[4];
    disk_index_t ctl_points;	/* subscript */
};


#define DISK_EDGEUSE_MAGIC 0x4e657520	/* Neu */
struct disk_edgeuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    struct disk_rt_list l2;
    disk_index_t up;
    disk_index_t eumate_p;
    disk_index_t radial_p;
    disk_index_t e_p;
    disk_index_t eua_p;
    unsigned char orientation[4];
    disk_index_t vu_p;
    disk_index_t g;
};


#define DISK_VERTEX_MAGIC 0x4e767274	/* Nvrt */
struct disk_vertex {
    unsigned char magic[4];
    struct disk_rt_list vu_hd;
    disk_index_t vg_p;
};


#define DISK_VERTEX_G_MAGIC 0x4e765f67	/* Nv_g */
struct disk_vertex_g {
    unsigned char magic[4];
    unsigned char coord[3*8];
};


#define DISK_VERTEXUSE_MAGIC 0x4e767520	/* Nvu */
struct disk_vertexuse {
    unsigned char magic[4];
    struct disk_rt_list l;
    disk_index_t up;
    disk_index_t v_p;
    disk_index_t a;
};


#define DISK_VERTEXUSE_A_PLANE_MAGIC 0x4e767561	/* Nvua */
struct disk_vertexuse_a_plane {
    unsigned char magic[4];
    unsigned char N[3*8];
};


#define DISK_VERTEXUSE_A_CNURB_MAGIC 0x4e766163	/* Nvac */
struct disk_vertexuse_a_cnurb {
    unsigned char magic[4];
    unsigned char param[3*8];
};


#define DISK_DOUBLE_ARRAY_MAGIC 0x4e666172		/* Narr */
struct disk_double_array {
    unsigned char magic[4];
    unsigned char ndouble[4];	/* # of doubles to follow */
    unsigned char vals[1*8];	/* actually [ndouble*8] */
};


/* ---------------------------------------------------------------------- */
/* All these arrays and defines have to use the same implicit index
 * values. FIXME: this should probably be an enum.
 */
#define NMG_KIND_MODEL              0
#define NMG_KIND_NMGREGION          1
#define NMG_KIND_NMGREGION_A        2
#define NMG_KIND_SHELL              3
#define NMG_KIND_SHELL_A            4
#define NMG_KIND_FACEUSE            5
#define NMG_KIND_FACE               6
#define NMG_KIND_FACE_G_PLANE       7
#define NMG_KIND_FACE_G_SNURB       8
#define NMG_KIND_LOOPUSE            9
#define NMG_KIND_LOOP              10
#define NMG_KIND_LOOP_G            11
#define NMG_KIND_EDGEUSE           12
#define NMG_KIND_EDGE              13
#define NMG_KIND_EDGE_G_LSEG       14
#define NMG_KIND_EDGE_G_CNURB      15
#define NMG_KIND_VERTEXUSE         16
#define NMG_KIND_VERTEXUSE_A_PLANE 17
#define NMG_KIND_VERTEXUSE_A_CNURB 18
#define NMG_KIND_VERTEX            19
#define NMG_KIND_VERTEX_G          20
/* 21 through 24 are unassigned, and reserved for future use */

/** special, variable sized */
#define NMG_KIND_DOUBLE_ARRAY      25

/* number of kinds.  This number must have some extra space, for
 * upwards compatibility.
 */
#define NMG_N_KINDS                26


const int rt_nmg_disk_sizes[NMG_N_KINDS] = {
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
    sizeof(struct disk_loop_g),
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
const char rt_nmg_kind_names[NMG_N_KINDS+2][18] = {
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
    "loop_g",
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
int
rt_nmg_magic_to_kind(uint32_t magic)
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
	case NMG_LOOP_G_MAGIC:
	    return NMG_KIND_LOOP_G;
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
    bu_bomb("rt_nmg_magic_to_kind: bad magic");
    return -1;
}


/* ---------------------------------------------------------------------- */

struct nmg_exp_counts {
    long new_subscript;
    long per_struct_index;
    int kind;
    long first_fastf_relpos;	/* for snurb and cnurb. */
    long byte_offset;		/* for snurb and cnurb. */
};


/* XXX These are horribly non-PARALLEL, and they *must* be PARALLEL ! */
static unsigned char *rt_nmg_fastf_p;
static unsigned int rt_nmg_cur_fastf_subscript;


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
rt_nmg_export4_fastf(const fastf_t *fp, int count, int pt_type, double scale)


/* If zero, means literal array of values */

{
    int i;
    unsigned char *cp;

    /* always write doubles to disk */
    double *scanp;

    if (pt_type)
	count *= RT_NURB_EXTRACT_COORDS(pt_type);

    cp = rt_nmg_fastf_p;
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
	bu_free(scanp, "rt_nmg_export4_fastf");
    }
    cp += (4+4) + count * 8;
    rt_nmg_fastf_p = cp;
    return rt_nmg_cur_fastf_subscript++;
}


fastf_t *
rt_nmg_import4_fastf(const unsigned char *base, struct nmg_exp_counts *ecnt, long int subscript, const matp_t mat, int len, int pt_type)
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
    if (ntohl(*(uint32_t*)cp) != DISK_DOUBLE_ARRAY_MAGIC) {
	bu_log("magic mis-match, got x%x, s/b x%x, file %s, line %d\n",
	       ntohl(*(uint32_t*)cp), DISK_DOUBLE_ARRAY_MAGIC, __FILE__, __LINE__);
	bu_log("subscript=%ld, byte_offset=%ld\n",
	       subscript, ecnt[subscript].byte_offset);
	bu_bomb("rt_nmg_import4_fastf() bad magic\n");
    }

    if (pt_type)
	len *= RT_NURB_EXTRACT_COORDS(pt_type);

    count = ntohl(*(uint32_t*)(cp + 4));
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


/**
 * Depends on ecnt[0].byte_offset having been set to maxindex.
 *
 * There are some special values for the disk index returned here:
 * >0 normal structure index.
 * 0 substitute a null pointer when imported.
 * -1 substitute pointer to within-struct list head when imported.
 */
HIDDEN int
reindex(void *p, struct nmg_exp_counts *ecnt)
{
    long idx;
    long ret=0;	/* zero is NOT the default value, this is just to satisfy cray compilers */

    /* If null pointer, return new subscript of zero */
    if (p == 0) {
	ret = 0;
	idx = 0;	/* sanity */
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
	    /* ret == 0 on suppressed loop_g ptrs, etc. */
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
	if (_f == DISK_INDEX_NULL) bu_log("Warning rt_nmg_edisk: reindex forw to null?\n"); \
	*(uint32_t *)((oo)->elem.forw) = htonl(_f);			\
	*(uint32_t *)((oo)->elem.back) = htonl(reindex((void *)((ii)->elem.back), ecnt)); }
#define PUTMAGIC(_magic) *(uint32_t *)d->magic = htonl(_magic)


/**
 * Export a given structure from memory to disk format
 *
 * Scale geometry by 'local2mm'
 */
void
rt_nmg_edisk(void *op, void *ip, struct nmg_exp_counts *ecnt, int idx, double local2mm)
/* base of disk array */
/* ptr to in-memory structure */


{
    int oindex;		/* index in op */

    oindex = ecnt[idx].per_struct_index;
    switch (ecnt[idx].kind) {
	case NMG_KIND_SHELL: {
	    struct shell *s = (struct shell *)ip;
	    struct disk_shell *d;
	    d = &((struct disk_shell *)op)[oindex];
	    NMG_CK_SHELL(s);
	    PUTMAGIC(DISK_SHELL_MAGIC);
	    INDEX(d, s, sa_p);
	    INDEXL(d, s, fu_hd);
	    INDEXL(d, s, lu_hd);
	    INDEXL(d, s, eu_hd);
	    INDEX(d, s, vu_p);
	    	
	    *(uint32_t *)((d)->l.forw) = htonl(-1);
	    *(uint32_t *)((d)->l.back) = htonl(-1);
	    *(uint32_t *)(d)->r_p = htonl(2);
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
		rt_nmg_export4_fastf(fg->u.knots,
				     fg->u.k_size, 0, 1.0));
	    *(uint32_t *)d->v_knots = htonl(
		rt_nmg_export4_fastf(fg->v.knots,
				     fg->v.k_size, 0, 1.0));
	    *(uint32_t *)d->us_size = htonl(fg->s_size[0]);
	    *(uint32_t *)d->vs_size = htonl(fg->s_size[1]);
	    *(uint32_t *)d->pt_type = htonl(fg->pt_type);
	    /* scale XYZ ctl_points by local2mm */
	    *(uint32_t *)d->ctl_points = htonl(
		rt_nmg_export4_fastf(fg->ctl_points,
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
	    INDEX(d, loop, lg_p);
	}
	    return;
	case NMG_KIND_LOOP_G: {
	    struct loop_g *lg = (struct loop_g *)ip;
	    struct disk_loop_g *d;

	    /* must be double for import and export */
	    double min[ELEMENTS_PER_POINT];
	    double max[ELEMENTS_PER_POINT];

	    d = &((struct disk_loop_g *)op)[oindex];
	    NMG_CK_LOOP_G(lg);
	    PUTMAGIC(DISK_LOOP_G_MAGIC);

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
	    *(uint32_t *)d->knots = htonl(
		rt_nmg_export4_fastf(eg->k.knots,
				     eg->k.k_size, 0, 1.0));
	    *(uint32_t *)d->c_size = htonl(eg->c_size);
	    *(uint32_t *)d->pt_type = htonl(eg->pt_type);
	    /*
	     * The curve's control points are in parameter space
	     * for cnurbs on snurbs, and in XYZ for cnurbs on planar faces.
	     * UV values do NOT get transformed, XYZ values do!
	     */
	    *(uint32_t *)d->ctl_points = htonl(
		rt_nmg_export4_fastf(eg->ctl_points,
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
    bu_log("rt_nmg_edisk kind=%d unknown\n", ecnt[idx].kind);
}
#undef INDEX
#undef INDEXL

/*
 * For symmetry with export, use same macro names and arg ordering,
 * but here take from "o" (outboard) variable and put in "i" (internal).
 *
 * NOTE that the "< 0" test here is a comparison with DISK_INDEX_LISTHEAD.
 */
#define INDEX(o, i, ty, elem)	(i)->elem = (struct ty *)ptrs[ntohl(*(uint32_t*)((o)->elem))]
#define INDEXL_HD(oo, ii, elem, hd) { \
	int sub; \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.forw))) < 0) \
	    (ii)->elem.forw = &(hd); \
	else (ii)->elem.forw = (struct bu_list *)ptrs[sub]; \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.back))) < 0) \
	    (ii)->elem.back = &(hd); \
	else (ii)->elem.back = (struct bu_list *)ptrs[sub]; }

/* For use with the edgeuse l2 / edge_g eu2_hd secondary list */
/* The subscripts will point to the edgeuse, not the edgeuse's l2 rt_list */
#define INDEXL_HD2(oo, ii, elem, hd) { \
	int sub; \
	struct edgeuse *eu2; \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.forw))) < 0) { \
	    (ii)->elem.forw = &(hd); \
	} else { \
	    eu2 = (struct edgeuse *)ptrs[sub]; \
	    NMG_CK_EDGEUSE(eu2); \
	    (ii)->elem.forw = &eu2->l2; \
	} \
	if ((sub = ntohl(*(uint32_t*)((oo)->elem.back))) < 0) { \
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
int
rt_nmg_idisk(void *op, void *ip, struct nmg_exp_counts *ecnt, int idx, uint32_t **ptrs, const fastf_t *mat, const unsigned char *basep)
/* ptr to in-memory structure */
/* base of disk array */


/* base of whole import record */
{
    int iindex;		/* index in ip */

    iindex = 0;
    switch (ecnt[idx].kind) {
	case NMG_KIND_MODEL:
	case NMG_KIND_NMGREGION:
	case NMG_KIND_NMGREGION_A:
	    return 0;
	case NMG_KIND_SHELL: {
	    struct shell *s = (struct shell *)op;
	    struct disk_shell *d;
	    d = &((struct disk_shell *)ip)[iindex];
	    NMG_CK_SHELL(s);
	    NMG_CK_DISKMAGIC(d->magic, DISK_SHELL_MAGIC);
	    INDEX(d, s, shell_a, sa_p);
	    INDEXL_HD(d, s, fu_hd, s->fu_hd);
	    INDEXL_HD(d, s, lu_hd, s->lu_hd);
	    INDEXL_HD(d, s, eu_hd, s->eu_hd);
	    INDEX(d, s, vertexuse, vu_p);
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
	    bn_rotate_bbox(sa->min_pt, sa->max_pt, mat, min, max);
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
	    fu->orientation = ntohl(*(uint32_t*)(d->orientation));
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
	    g_index = ntohl(*(uint32_t*)(d->g));
	    f->g.magic_p = (uint32_t *)ptrs[g_index];
	    f->flip = ntohl(*(uint32_t*)(d->flip));
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
	    bn_rotate_plane(fg->N, mat, plane);
	}
	    return 0;
	case NMG_KIND_FACE_G_SNURB: {
	    struct face_g_snurb *fg = (struct face_g_snurb *)op;
	    struct disk_face_g_snurb *d;
	    const matp_t matrix = (const matp_t)mat;
	    d = &((struct disk_face_g_snurb *)ip)[iindex];
	    NMG_CK_FACE_G_SNURB(fg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_FACE_G_SNURB_MAGIC);
	    INDEXL_HD(d, fg, f_hd, fg->f_hd);
	    fg->order[0] = ntohl(*(uint32_t*)(d->u_order));
	    fg->order[1] = ntohl(*(uint32_t*)(d->v_order));
	    fg->u.k_size = ntohl(*(uint32_t*)(d->u_size));
	    fg->u.knots = rt_nmg_import4_fastf(basep,
					       ecnt,
					       ntohl(*(uint32_t*)(d->u_knots)),
					       (const matp_t)NULL,
					       fg->u.k_size,
					       0);
	    fg->v.k_size = ntohl(*(uint32_t*)(d->v_size));
	    fg->v.knots = rt_nmg_import4_fastf(basep,
					       ecnt,
					       ntohl(*(uint32_t*)(d->v_knots)),
					       (const matp_t)NULL,
					       fg->v.k_size,
					       0);
	    fg->s_size[0] = ntohl(*(uint32_t*)(d->us_size));
	    fg->s_size[1] = ntohl(*(uint32_t*)(d->vs_size));
	    fg->pt_type = ntohl(*(uint32_t*)(d->pt_type));
	    /* Transform ctl_points by 'mat' */
	    fg->ctl_points = rt_nmg_import4_fastf(basep,
						  ecnt,
						  ntohl(*(uint32_t*)(d->ctl_points)),
						  matrix,
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
	    up_index = ntohl(*(uint32_t*)(d->up));
	    lu->up.magic_p = ptrs[up_index];
	    INDEX(d, lu, loopuse, lumate_p);
	    lu->orientation = ntohl(*(uint32_t*)(d->orientation));
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
	    INDEX(d, loop, loop_g, lg_p);
	    NMG_CK_LOOPUSE(loop->lu_p);
	}
	    return 0;
	case NMG_KIND_LOOP_G: {
	    struct loop_g *lg = (struct loop_g *)op;
	    struct disk_loop_g *d;
	    point_t min;
	    point_t max;

	    /* must be double for import and export */
	    double scanmin[ELEMENTS_PER_POINT];
	    double scanmax[ELEMENTS_PER_POINT];

	    d = &((struct disk_loop_g *)ip)[iindex];
	    NMG_CK_LOOP_G(lg);
	    NMG_CK_DISKMAGIC(d->magic, DISK_LOOP_G_MAGIC);
	    bu_cv_ntohd((unsigned char *)scanmin, d->min_pt, ELEMENTS_PER_POINT);
	    VMOVE(min, scanmin); /* convert double to fastf_t */
	    bu_cv_ntohd((unsigned char *)scanmax, d->max_pt, ELEMENTS_PER_POINT);
	    VMOVE(max, scanmax); /* convert double to fastf_t */
	    bn_rotate_bbox(lg->min_pt, lg->max_pt, mat, min, max);
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
	    up_index = ntohl(*(uint32_t*)(d->up));
	    eu->up.magic_p = ptrs[up_index];
	    INDEX(d, eu, edgeuse, eumate_p);
	    INDEX(d, eu, edgeuse, radial_p);
	    INDEX(d, eu, edge, e_p);
	    eu->orientation = ntohl(*(uint32_t*)(d->orientation));
	    INDEX(d, eu, vertexuse, vu_p);
	    up_kind = ecnt[up_index].kind;
	    if (up_kind == NMG_KIND_LOOPUSE) {
		INDEXL_HD(d, eu, l, eu->up.lu_p->down_hd);
	    } else if (up_kind == NMG_KIND_SHELL) {
		INDEXL_HD(d, eu, l, eu->up.s_p->eu_hd);
	    } else bu_log("bad edgeuse up, index=%d, kind=%d\n", up_index, up_kind);
	    eu->g.magic_p = ptrs[ntohl(*(uint32_t*)(d->g))];
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
	    e->is_real = ntohl(*(uint32_t*)(d->is_real));
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
	    eg->order = ntohl(*(uint32_t*)(d->order));

	    /* If order is zero, so is everything else */
	    if (eg->order == 0) return 0;

	    eg->k.k_size = ntohl(*(uint32_t*)(d->k_size));
	    eg->k.knots = rt_nmg_import4_fastf(basep,
					       ecnt,
					       ntohl(*(uint32_t*)(d->knots)),
					       (const matp_t)NULL,
					       eg->k.k_size,
					       0);
	    eg->c_size = ntohl(*(uint32_t*)(d->c_size));
	    eg->pt_type = ntohl(*(uint32_t*)(d->pt_type));
	    /*
	     * The curve's control points are in parameter space.
	     * They do NOT get transformed!
	     */
	    if (RT_NURB_EXTRACT_PT_TYPE(eg->pt_type) == RT_NURB_PT_UV) {
		/* UV coords on snurb surface don't get xformed */
		eg->ctl_points = rt_nmg_import4_fastf(basep,
						      ecnt,
						      ntohl(*(uint32_t*)(d->ctl_points)),
						      (const matp_t)NULL,
						      eg->c_size,
						      eg->pt_type);
	    } else {
		const matp_t matrix = (const matp_t)mat;

		/* XYZ coords on planar face DO get xformed */
		eg->ctl_points = rt_nmg_import4_fastf(basep,
						      ecnt,
						      ntohl(*(uint32_t*)(d->ctl_points)),
						      matrix,
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
	    vu->up.magic_p = ptrs[ntohl(*(uint32_t*)(d->up))];
	    INDEX(d, vu, vertex, v_p);
	    vu->a.magic_p = ptrs[ntohl(*(uint32_t*)(d->a))];
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


/**
 * Allocate storage for all the in-memory NMG structures, in
 * preparation for the importation operation, using the GET_xxx()
 * macros, so that m->maxindex, etc., are all appropriately handled.
 */
HIDDEN struct shell *
rt_nmg_ialloc(uint32_t **ptrs, struct nmg_exp_counts *ecnt, int *kind_counts)
{
    struct shell *s = (struct shell *)0;
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
		case NMG_KIND_NMGREGION:
		case NMG_KIND_NMGREGION_A:
		    ptrs[subscript] = (uint32_t *)0;
		    break;
		case NMG_KIND_SHELL: {
		    s = nmg_ms();
		    s->maxindex++;
		    s->magic = NMG_SHELL_MAGIC;
		    BU_LIST_INIT(&s->fu_hd);
		    BU_LIST_INIT(&s->lu_hd);
		    BU_LIST_INIT(&s->eu_hd);
		    ptrs[subscript] = (uint32_t *)s;
		}
		    break;
		case NMG_KIND_SHELL_A: {
		    struct shell_a *sa;
		    GET_SHELL_A(sa, s);
		    sa->magic = NMG_SHELL_A_MAGIC;
		    ptrs[subscript] = (uint32_t *)sa;
		}
		    break;
		case NMG_KIND_FACEUSE: {
		    struct faceuse *fu;
		    GET_FACEUSE(fu, s);
		    fu->l.magic = NMG_FACEUSE_MAGIC;
		    BU_LIST_INIT(&fu->lu_hd);
		    ptrs[subscript] = (uint32_t *)fu;
		}
		    break;
		case NMG_KIND_FACE: {
		    struct face *f;
		    GET_FACE(f, s);
		    f->l.magic = NMG_FACE_MAGIC;
		    ptrs[subscript] = (uint32_t *)f;
		}
		    break;
		case NMG_KIND_FACE_G_PLANE: {
		    struct face_g_plane *fg;
		    GET_FACE_G_PLANE(fg, s);
		    fg->magic = NMG_FACE_G_PLANE_MAGIC;
		    BU_LIST_INIT(&fg->f_hd);
		    ptrs[subscript] = (uint32_t *)fg;
		}
		    break;
		case NMG_KIND_FACE_G_SNURB: {
		    struct face_g_snurb *fg;
		    GET_FACE_G_SNURB(fg, s);
		    fg->l.magic = NMG_FACE_G_SNURB_MAGIC;
		    BU_LIST_INIT(&fg->f_hd);
		    ptrs[subscript] = (uint32_t *)fg;
		}
		    break;
		case NMG_KIND_LOOPUSE: {
		    struct loopuse *lu;
		    GET_LOOPUSE(lu, s);
		    lu->l.magic = NMG_LOOPUSE_MAGIC;
		    BU_LIST_INIT(&lu->down_hd);
		    ptrs[subscript] = (uint32_t *)lu;
		}
		    break;
		case NMG_KIND_LOOP: {
		    struct loop *l;
		    GET_LOOP(l, s);
		    l->magic = NMG_LOOP_MAGIC;
		    ptrs[subscript] = (uint32_t *)l;
		}
		    break;
		case NMG_KIND_LOOP_G: {
		    struct loop_g *lg;
		    GET_LOOP_G(lg, s);
		    lg->magic = NMG_LOOP_G_MAGIC;
		    ptrs[subscript] = (uint32_t *)lg;
		}
		    break;
		case NMG_KIND_EDGEUSE: {
		    struct edgeuse *eu;
		    GET_EDGEUSE(eu, s);
		    eu->l.magic = NMG_EDGEUSE_MAGIC;
		    eu->l2.magic = NMG_EDGEUSE2_MAGIC;
		    ptrs[subscript] = (uint32_t *)eu;
		}
		    break;
		case NMG_KIND_EDGE: {
		    struct edge *e;
		    GET_EDGE(e, s);
		    e->magic = NMG_EDGE_MAGIC;
		    ptrs[subscript] = (uint32_t *)e;
		}
		    break;
		case NMG_KIND_EDGE_G_LSEG: {
		    struct edge_g_lseg *eg;
		    GET_EDGE_G_LSEG(eg, s);
		    eg->l.magic = NMG_EDGE_G_LSEG_MAGIC;
		    BU_LIST_INIT(&eg->eu_hd2);
		    ptrs[subscript] = (uint32_t *)eg;
		}
		    break;
		case NMG_KIND_EDGE_G_CNURB: {
		    struct edge_g_cnurb *eg;
		    GET_EDGE_G_CNURB(eg, s);
		    eg->l.magic = NMG_EDGE_G_CNURB_MAGIC;
		    BU_LIST_INIT(&eg->eu_hd2);
		    ptrs[subscript] = (uint32_t *)eg;
		}
		    break;
		case NMG_KIND_VERTEXUSE: {
		    struct vertexuse *vu;
		    GET_VERTEXUSE(vu, s);
		    vu->l.magic = NMG_VERTEXUSE_MAGIC;
		    ptrs[subscript] = (uint32_t *)vu;
		}
		    break;
		case NMG_KIND_VERTEXUSE_A_PLANE: {
		    struct vertexuse_a_plane *vua;
		    GET_VERTEXUSE_A_PLANE(vua, s);
		    vua->magic = NMG_VERTEXUSE_A_PLANE_MAGIC;
		    ptrs[subscript] = (uint32_t *)vua;
		}
		    break;
		case NMG_KIND_VERTEXUSE_A_CNURB: {
		    struct vertexuse_a_cnurb *vua;
		    GET_VERTEXUSE_A_CNURB(vua, s);
		    vua->magic = NMG_VERTEXUSE_A_CNURB_MAGIC;
		    ptrs[subscript] = (uint32_t *)vua;
		}
		    break;
		case NMG_KIND_VERTEX: {
		    struct vertex *v;
		    GET_VERTEX(v, s);
		    v->magic = NMG_VERTEX_MAGIC;
		    BU_LIST_INIT(&v->vu_hd);
		    ptrs[subscript] = (uint32_t *)v;
		}
		    break;
		case NMG_KIND_VERTEX_G: {
		    struct vertex_g *vg;
		    GET_VERTEX_G(vg, s);
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

    return s;
}


/**
 * Find the locations of all the variable-sized fastf_t arrays in the
 * input record.  Record that position as a byte offset from the very
 * front of the input record in ecnt[], indexed by subscript number.
 *
 * No storage is allocated here, that will be done by
 * rt_nmg_import4_fastf() on the fly.  A separate call to bu_malloc()
 * will be used, so that nmg_keg(), etc., can kill each array as
 * appropriate.
 */
void
rt_nmg_i2alloc(struct nmg_exp_counts *ecnt, unsigned char *cp, int *kind_counts)
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
	offset += rt_nmg_disk_sizes[kind] * kind_counts[kind];
	subscript += kind_counts[kind];
    }

    /* Should have found the first one now */
    NMG_CK_DISKMAGIC(cp + offset, DISK_DOUBLE_ARRAY_MAGIC);
    for (i=0; i < nkind; i++) {
	int ndouble;
	NMG_CK_DISKMAGIC(cp + offset, DISK_DOUBLE_ARRAY_MAGIC);
	ndouble = ntohl(*(uint32_t*)(cp + offset + 4));
	ecnt[subscript].kind = NMG_KIND_DOUBLE_ARRAY;
	/* Stored byte offset is from beginning of disk record */
	ecnt[subscript].byte_offset = offset;
	offset += (4+4) + 8*ndouble;
	subscript++;
    }
}


/**
 * Import an NMG from the database format to the internal format.
 * Apply modeling transformations as well.
 *
 * Special subscripts are used in the disk file:
 * -1 indicates a pointer to the rt_list structure which
 * heads a linked list, and is not the first struct element.
 * 0 indicates that a null pointer should be used.
 */
int
rt_nmg_import4_internal(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, int rebound, const struct bn_tol *tol)
{
    struct shell *s;
    union record *rp;
    int kind_counts[NMG_N_KINDS];
    unsigned char *cp;
    uint32_t **real_ptrs;
    uint32_t **ptrs;
    struct nmg_exp_counts *ecnt;
    int i;
    int maxindex;
    int kind;
    static uint32_t bad_magic = 0x999;

    BU_CK_EXTERNAL(ep);
    BN_CK_TOL(tol);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_NMG) {
	bu_log("rt_nmg_import4: defective record\n");
	return -1;
    }

    /*
     * Check for proper version.
     * In the future, this will be the backwards-compatibility hook.
     */
    if (rp->nmg.N_version != DISK_MODEL_VERSION) {
	bu_log("rt_nmg_import4:  expected NMG '.g' format version %d, got version %d, aborting.\n",
	       DISK_MODEL_VERSION,
	       rp->nmg.N_version);
	return -1;
    }

    /* Obtain counts of each kind of structure */
    maxindex = 1;
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	kind_counts[kind] = ntohl(*(uint32_t*)(rp->nmg.N_structs+4*kind));
	maxindex += kind_counts[kind];
    }

    /* Collect overall new subscripts, and structure-specific indices */
    ecnt = (struct nmg_exp_counts *)bu_calloc(maxindex+3,
					      sizeof(struct nmg_exp_counts), "ecnt[]");
    real_ptrs = (uint32_t **)bu_calloc(maxindex+3,
				       sizeof(uint32_t *), "ptrs[]");
    /* So that indexing [-1] gives an appropriately bogus magic # */
    ptrs = real_ptrs+1;
    ptrs[-1] = &bad_magic;		/* [-1] gives bad magic */
    ptrs[0] = NULL;	/* [0] gives NULL */
    ptrs[maxindex] = &bad_magic;	/* [maxindex] gives bad magic */
    ptrs[maxindex+1] = &bad_magic;	/* [maxindex+1] gives bad magic */

    /* Allocate storage for all the NMG structs, in ptrs[] */
    s = rt_nmg_ialloc(ptrs, ecnt, kind_counts);

    /* Locate the variably sized fastf_t arrays.  ecnt[] has room. */
    cp = (unsigned char *)(rp+1);	/* start at first granule in */
    rt_nmg_i2alloc(ecnt, cp, kind_counts);

    /* Import each structure, in turn */
    for (i=1; i < maxindex; i++) {
	/* If we made it to the last kind, stop.  Nothing follows */
	if (ecnt[i].kind == NMG_KIND_DOUBLE_ARRAY) break;
	if (rt_nmg_idisk((void *)(ptrs[i]), (void *)cp,
			 ecnt, i, ptrs, mat, (unsigned char *)(rp+1)) < 0)
	    return -1;	/* FAIL */
	cp += rt_nmg_disk_sizes[ecnt[i].kind];
    }

    if (rebound) {
	/* Recompute all bounding boxes in model */
	nmg_rebound(s, tol);
    } else {
	/*
	 * Need to recompute bounding boxes for the faces here.
	 * Other bounding boxes will exist and be intact if NMG
	 * exporter wrote the _a structures.
	 */
	for (i=1; i < maxindex; i++) {
	    if (ecnt[i].kind != NMG_KIND_FACE) continue;
	    nmg_face_bb((struct face *)ptrs[i], tol);
	}
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_NMG;
    ip->idb_meth = &OBJ[ID_NMG];
    ip->idb_ptr = (void *)s;

    bu_free((char *)ecnt, "ecnt[]");
    bu_free((char *)real_ptrs, "ptrs[]");

    return 0;			/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 *
 * When the "compact" flag is set, bounding boxes from (at present)
 * nmgregion_a
 * shell_a
 * loop_g
 * are not converted for storage in the database.
 * They should be re-generated at import time.
 *
 * If the "compact" flag is not set, then the NMG model is saved,
 * verbatim.
 *
 * The overall layout of the on-disk NMG is like this:
 *
 *	+---------------------------+
 *	|  NMG header granule       |
 *	|    solid name             |
 *	|    # additional granules  |
 *	|    format version         |
 *	|    kind_count[] array     |
 *	+---------------------------+
 *	|                           |
 *	|                           |
 *	~     N_count granules      ~
 *	~              :            ~
 *	|              :            |
 *	|                           |
 *	+---------------------------+
 *
 * In the additional granules, all structures of "kind" 0 (model) go
 * first, followed by all structures of kind 1 (nmgregion), etc.  As
 * each structure is output, it is assigned a subscript number,
 * starting with #1 for the model structure.  All pointers are
 * converted to the matching subscript numbers.  An on-disk subscript
 * of zero indicates a corresponding NULL pointer in memory.  All
 * integers are converted to network (Big-Endian) byte order.  All
 * floating point values are stored in network (Big-Endian IEEE)
 * format.
 */
int
rt_nmg_export4_internal(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, int compact)
{
    struct shell *s;
    union record *rp;
    struct nmg_struct_counts cntbuf;
    uint32_t **ptrs;
    struct nmg_exp_counts *ecnt;
    int i;
    int subscript;
    int kind_counts[NMG_N_KINDS];
    void *disk_arrays[NMG_N_KINDS];
    int additional_grans;
    int tot_size;
    int kind;
    char *cp;
    int double_count;
    int fastf_byte_count;

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_NMG) return -1;
    s = (struct shell *)ip->idb_ptr;
    NMG_CK_SHELL(s);

    /* As a by-product, this fills in the ptrs[] array! */
    memset((char *)&cntbuf, 0, sizeof(cntbuf));
    ptrs = nmg_s_struct_count(&cntbuf, s);

    /* Collect overall new subscripts, and structure-specific indices */
    ecnt = (struct nmg_exp_counts *)bu_calloc(s->maxindex+4,
					      sizeof(struct nmg_exp_counts), "ecnt[]");
    for (i = 0; i < NMG_N_KINDS; i++)
	kind_counts[i] = 0;
    subscript = 1;		/* must be larger than DISK_INDEX_NULL */
    double_count = 0;
    fastf_byte_count = 0;
    for (i=0; i < s->maxindex + 3; i++) {
	if (ptrs[i] == NULL) {
	    ecnt[i].kind = -1;
	    continue;
	}

	switch (i) {
	    case 0: kind = rt_nmg_magic_to_kind(NMG_MODEL_MAGIC); break;
	    case 1: kind = rt_nmg_magic_to_kind(NMG_REGION_MAGIC); break;
	    case 2: kind = rt_nmg_magic_to_kind(NMG_REGION_A_MAGIC); break;
	    default: kind = rt_nmg_magic_to_kind(*(ptrs[i]));
	}

	ecnt[i].per_struct_index = kind_counts[kind]++;
	ecnt[i].kind = kind;
	/* Handle the variable sized kinds */
	switch (kind) {
	    case NMG_KIND_FACE_G_SNURB: {
		struct face_g_snurb *fg;
		int ndouble;
		fg = (struct face_g_snurb *)ptrs[i];
		ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
		kind_counts[NMG_KIND_DOUBLE_ARRAY] += 3;
		ndouble =  fg->u.k_size +
		    fg->v.k_size +
		    fg->s_size[0] * fg->s_size[1] *
		    RT_NURB_EXTRACT_COORDS(fg->pt_type);
		double_count += ndouble;
		ecnt[i].byte_offset = fastf_byte_count;
		fastf_byte_count += 3*(4+4) + 8*ndouble;
	    }
		break;
	    case NMG_KIND_EDGE_G_CNURB: {
		struct edge_g_cnurb *eg;
		int ndouble;
		eg = (struct edge_g_cnurb *)ptrs[i];
		ecnt[i].first_fastf_relpos = kind_counts[NMG_KIND_DOUBLE_ARRAY];
		/* If order is zero, no knots or ctl_points */
		if (eg->order == 0) break;
		kind_counts[NMG_KIND_DOUBLE_ARRAY] += 2;
		ndouble = eg->k.k_size + eg->c_size *
		    RT_NURB_EXTRACT_COORDS(eg->pt_type);
		double_count += ndouble;
		ecnt[i].byte_offset = fastf_byte_count;
		fastf_byte_count += 2*(4+4) + 8*ndouble;
	    }
		break;
	}
    }
    if (compact) {
	kind_counts[NMG_KIND_NMGREGION_A] = 0;
	kind_counts[NMG_KIND_SHELL_A] = 0;
	kind_counts[NMG_KIND_LOOP_G] = 0;
    }

    /* Assign new subscripts to ascending guys of same kind */
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	if (compact && (kind == NMG_KIND_NMGREGION_A ||
			kind == NMG_KIND_SHELL_A ||
			kind == NMG_KIND_LOOP_G)) {
	    /*
	     * Don't assign any new subscripts for them.
	     * Instead, use DISK_INDEX_NULL, yielding null ptrs.
	     */
	    for (i=0; i < s->maxindex + 3; i++) {
		if (ptrs[i] == NULL) continue;
		if (ecnt[i].kind != kind) continue;
		ecnt[i].new_subscript = DISK_INDEX_NULL;
	    }
	    continue;
	}
	for (i=0; i < s->maxindex + 3; i++) {
	    if (ptrs[i] == NULL) continue;
	    if (ecnt[i].kind != kind) continue;
	    ecnt[i].new_subscript = subscript++;
	}
    }
    /* Tack on the variable sized fastf_t arrays at the end */
    rt_nmg_cur_fastf_subscript = subscript;
    subscript += kind_counts[NMG_KIND_DOUBLE_ARRAY];

    /* Sanity checking */
    for (i=0; i < s->maxindex + 3; i++) {
	if (ptrs[i] == NULL) continue;
	if (nmg_index_of_struct(ptrs[i]) != i) {
	    bu_log("***ERROR, ptrs[%d]->index = %d\n",
		   i, nmg_index_of_struct((uint32_t *)ptrs[i]));
	}
	if (rt_nmg_magic_to_kind(*ptrs[i]) != ecnt[i].kind) {
	    bu_log("@@@ERROR, ptrs[%d] kind(%d) != %d\n",
		   i, rt_nmg_magic_to_kind(*ptrs[i]),
		   ecnt[i].kind);
	}
    }

    tot_size = 0;
    for (i = 0; i < NMG_N_KINDS; i++) {
	if (kind_counts[i] <= 0) {
	    disk_arrays[i] = ((void *)0);
	    continue;
	}
	tot_size += kind_counts[i] * rt_nmg_disk_sizes[i];
    }
    /* Account for variable sized double arrays, at the end */
    tot_size += kind_counts[NMG_KIND_DOUBLE_ARRAY] * (4+4) +
	double_count * 8;

    ecnt[0].byte_offset = subscript; /* implicit arg to reindex() */

    additional_grans = (tot_size + sizeof(union record)-1) / sizeof(union record);
    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = (1 + additional_grans) * sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "nmg external");
    rp = (union record *)ep->ext_buf;
    rp->nmg.N_id = DBID_NMG;
    rp->nmg.N_version = DISK_MODEL_VERSION;
    *(uint32_t *)rp->nmg.N_count = htonl((uint32_t)additional_grans);

    /* Record counts of each kind of structure */
    for (kind = 0; kind < NMG_N_KINDS; kind++) {
	*(uint32_t *)(rp->nmg.N_structs+4*kind) = htonl(kind_counts[kind]);
    }

    cp = (char *)(rp+1);	/* advance one granule */
    for (i=0; i < NMG_N_KINDS; i++) {
	disk_arrays[i] = (void *)cp;
	cp += kind_counts[i] * rt_nmg_disk_sizes[i];
    }
    /* disk_arrays[NMG_KIND_DOUBLE_ARRAY] is set properly because it is last */
    rt_nmg_fastf_p = (unsigned char *)disk_arrays[NMG_KIND_DOUBLE_ARRAY];

    /* Convert all the structures to their disk versions */
    for (i = s->maxindex + 2; i >= 0; i--) {
	if (ptrs[i] == NULL) continue;
	kind = ecnt[i].kind;
	if (kind_counts[kind] <= 0) continue;
	rt_nmg_edisk((void *)(disk_arrays[kind]),
		     (void *)(ptrs[i]), ecnt, i, local2mm);
    }

    bu_free((void *)ptrs, "ptrs[]");
    bu_free((void *)ecnt, "ecnt[]");

    return 0;
}


/**
 * Import an NMG from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_nmg_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct shell *s;
    union record *rp;
    struct bn_tol tol;

    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_NMG) {
	bu_log("rt_nmg_import4: defective record\n");
	return -1;
    }

    /* XXX The bounding box routines need a tolerance.
     * XXX This is sheer guesswork here.
     * As long as this NMG is going to be turned into vlist, or
     * handed off to the boolean evaluator, any non-zero numbers are fine.
     */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    if (rt_nmg_import4_internal(ip, ep, mat, 1, &tol) < 0)
	return -1;

    s = (struct shell *)ip->idb_ptr;
    NMG_CK_SHELL(s);

    if (RT_G_DEBUG || nmg_debug)
	nmg_vsshell(s);

    return 0;			/* OK */
}


int
rt_nmg_import5(struct rt_db_internal *ip,
	       struct bu_external *ep,
	       const mat_t mat,
	       const struct db_i *dbip)
{
    struct shell *s;
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

    if (dbip) RT_CK_DBI(dbip);

    BU_CK_EXTERNAL(ep);
    dp = (unsigned char *)ep->ext_buf;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    {
	int version;
	version = ntohl(*(uint32_t*)dp);
	dp+= SIZEOF_NETWORK_LONG;
	if (version != DISK_MODEL_VERSION) {
	    bu_log("rt_nmg_import4: expected NMG '.g' format version %d, got %d, aborting nmg solid import\n",
		   DISK_MODEL_VERSION, version);
	    return -1;
	}
    }
    maxindex = 1;
    for (kind =0; kind < NMG_N_KINDS; kind++) {
	kind_counts[kind] = ntohl(*(uint32_t*)dp);
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

    s = rt_nmg_ialloc(ptrs, ecnt, kind_counts);

    rt_nmg_i2alloc(ecnt, dp, kind_counts);

    /* Now import each structure, in turn */
    for (i=1; i < maxindex; i++) {
	/* We know that the DOUBLE_ARRAY is the last thing to process */
	if (ecnt[i].kind == NMG_KIND_DOUBLE_ARRAY) break;
	if (rt_nmg_idisk((void *)(ptrs[i]), (void *)dp, ecnt,
			 i, ptrs, mat, (unsigned char *)startdata) < 0) {
	    return -1;
	}
	dp += rt_nmg_disk_sizes[ecnt[i].kind];
    }

    /* Face min_pt and max_pt are not stored, so this is mandatory. */
    nmg_rebound(s, &tol);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_NMG;
    ip->idb_meth = &OBJ[ ID_NMG ];

    ip->idb_ptr = (void *)s;
    NMG_CK_SHELL(s);
    bu_free((char *)ecnt, "ecnt[]");
    bu_free((char *)real_ptrs, "ptrs[]");

    if (RT_G_DEBUG || nmg_debug) {
	nmg_vsshell(s);
    }
    return 0;		/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_nmg_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct shell *s;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_NMG) return -1;
    s = (struct shell *)ip->idb_ptr;
    NMG_CK_SHELL(s);

    /* To ensure that a decent database is written, verify source first */
    nmg_vsshell(s);

    /* The "compact" flag is used to save space in the database */
    return rt_nmg_export4_internal(ep, ip, local2mm, 1);
}


int
rt_nmg_export5(
    struct bu_external *ep,
    const struct rt_db_internal *ip,
    double local2mm,
    const struct db_i *dbip)
{
    struct shell *s;
    unsigned char *dp;
    uint32_t **ptrs;
    struct nmg_struct_counts cntbuf;
    struct nmg_exp_counts *ecnt;
    int kind_counts[NMG_N_KINDS];
    void *disk_arrays[NMG_N_KINDS];
    int tot_size;
    int kind;
    int double_count;
    int i;
    int subscript, fastf_byte_count;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_NMG) return -1;
    s = (struct shell *)ip->idb_ptr;
    NMG_CK_SHELL(s);

    nmg_s_reindex(s, 0);
    memset((char *)&cntbuf, 0, sizeof(cntbuf));
    ptrs = nmg_s_struct_count(&cntbuf, s);

    ecnt = (struct nmg_exp_counts *)bu_calloc(s->maxindex+1,
					      sizeof(struct nmg_exp_counts), "ecnt[]");
    for (i=0; i<NMG_N_KINDS; i++) {
	kind_counts[i] = 0;
    }
    subscript = 3;
    double_count = 0;
    fastf_byte_count = 0;
    for (i=0; i< s->maxindex; i++) {
	if (ptrs[i] == NULL) {
	    ecnt[i].kind = -1;
	    continue;
	}

	kind = rt_nmg_magic_to_kind(*(ptrs[i]));
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
    kind_counts[NMG_KIND_MODEL] = 1;
    kind_counts[NMG_KIND_NMGREGION] = 1;
    kind_counts[NMG_KIND_NMGREGION_A] = 0;
    kind_counts[NMG_KIND_SHELL_A] = 0;
    kind_counts[NMG_KIND_LOOP_G] = 0;

    /* Assign new subscripts to ascending struts of the same kind */
    for (kind=0; kind < NMG_N_KINDS; kind++) {
	/* Compacting */
	if (kind == NMG_KIND_NMGREGION_A ||
	    kind == NMG_KIND_SHELL_A ||
	    kind == NMG_KIND_LOOP_G) {
	    for (i=0; i<s->maxindex; i++) {
		if (ptrs[i] == NULL) continue;
		if (ecnt[i].kind != kind) continue;
		ecnt[i].new_subscript = DISK_INDEX_NULL;
	    }
	    continue;
	}

	for (i=0; i< s->maxindex;i++) {
	    if (ptrs[i] == NULL) continue;
	    if (ecnt[i].kind != kind) continue;
	    ecnt[i].new_subscript = subscript++;
	}
    }
    /* Tack on the variable sized fastf_t arrays at the end */
    rt_nmg_cur_fastf_subscript = subscript;
    subscript += kind_counts[NMG_KIND_DOUBLE_ARRAY];

    /* Now do some checking to make sure the world is not totally mad */
    for (i=0; i<s->maxindex; i++) {
	if (ptrs[i] == NULL) continue;

	if (nmg_index_of_struct(ptrs[i]) != i) {
	    bu_log("***ERROR, ptrs[%d]->index = %d\n",
		   i, nmg_index_of_struct(ptrs[i]));
	}
	if (rt_nmg_magic_to_kind(*ptrs[i]) != ecnt[i].kind) {
	    bu_log("***ERROR, ptrs[%d] kind(%d) != %d\n",
		   i, rt_nmg_magic_to_kind(*ptrs[i]),
		   ecnt[i].kind);
	}

    }

    tot_size = 0;
    for (i=0; i< NMG_N_KINDS; i++) {
	if (kind_counts[i] <= 0) {
	    disk_arrays[i] = ((void *)0);
	    continue;
	}
	tot_size += kind_counts[i] * rt_nmg_disk_sizes[i];
    }

    /* Account for variable sized double arrays, at the end */
    tot_size += kind_counts[NMG_KIND_DOUBLE_ARRAY] * (4+4) +
	double_count*8;

    ecnt[0].byte_offset = subscript; /* implicit arg to reindex() */
    tot_size += SIZEOF_NETWORK_LONG*(NMG_N_KINDS + 1); /* one for magic */
    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = tot_size;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "nmg external5");
    dp = ep->ext_buf;
    *(uint32_t *)dp = htonl(DISK_MODEL_VERSION);
    dp+=SIZEOF_NETWORK_LONG;

    for (kind=0; kind <NMG_N_KINDS; kind++) {
	*(uint32_t *)dp = htonl(kind_counts[kind]);
	dp+=SIZEOF_NETWORK_LONG;
    }
    for (i=0; i< NMG_N_KINDS; i++) {
	disk_arrays[i] = dp;
	dp += kind_counts[i] * rt_nmg_disk_sizes[i];
    }
    rt_nmg_fastf_p = (unsigned char*)disk_arrays[NMG_KIND_DOUBLE_ARRAY];

    for (i = s->maxindex-1;i >=0; i--) {
	if (ptrs[i] == NULL) continue;
	kind = ecnt[i].kind;
	if (kind_counts[kind] <= 0) continue;
	rt_nmg_edisk((void *)(disk_arrays[kind]),
		     (void *)(ptrs[i]), ecnt, i, local2mm);
    }

    {
	struct disk_model *d = &((struct disk_model *)disk_arrays[NMG_KIND_MODEL])[0];
	PUTMAGIC(DISK_MODEL_MAGIC);
	*(uint32_t *)((d)->r_hd.forw) = htonl(2);
	*(uint32_t *)((d)->r_hd.back) = htonl(2);
    }

    {
	struct disk_nmgregion *d = &((struct disk_nmgregion *)disk_arrays[NMG_KIND_NMGREGION])[0];
	PUTMAGIC(DISK_REGION_MAGIC);
	*(uint32_t *)((d)->l.forw) = htonl(-1);
	*(uint32_t *)((d)->l.back) = htonl(-1);
	*(uint32_t *)(d)->m_p = htonl(1);
	*(uint32_t *)(d)->ra_p = htonl(0);
	*(uint32_t *)((d)->s_hd.forw) = htonl(3);
	*(uint32_t *)((d)->s_hd.back) = htonl(3);
    }

    bu_free((char *)ptrs, "ptrs[]");
    bu_free((char *)ecnt, "ecnt[]");
    return 0;		/* OK */
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_nmg_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    struct shell *s =
	(struct shell *)ip->idb_ptr;

    NMG_CK_SHELL(s);
    bu_vls_printf(str, "n-Manifold Geometry solid (NMG) maxindex=%ld\n",
		  (long)s->maxindex);

    if (!verbose) return 0;

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_nmg_ifree(struct rt_db_internal *ip)
{
    struct shell *s;

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_ptr) {
	s = (struct shell *)ip->idb_ptr;
	NMG_CK_SHELL(s);
	nmg_ks(s);
    }

    ip->idb_ptr = ((void *)0);	/* sanity */
}


int
rt_nmg_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct shell *s=(struct shell *)intern->idb_ptr;
    struct bu_ptbl verts;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertexuse *vu;
    struct vertex *v;
    struct vertex_g *vg;
    size_t i;

    NMG_CK_SHELL(s);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "nmg");
	bu_ptbl_init(&verts, 256, "nmg verts");
	nmg_vertex_tabulate(&verts, &s->magic);

	/* first list all the vertices */
	bu_vls_strcat(logstr, " V {");
	for (i=0; i<BU_PTBL_LEN(&verts); i++) {
	    v = (struct vertex *) BU_PTBL_GET(&verts, i);
	    NMG_CK_VERTEX(v);
	    vg = v->vg_p;
	    if (!vg) {
		bu_vls_printf(logstr, "Vertex has no geometry\n");
		bu_ptbl_free(&verts);
		return BRLCAD_ERROR;
	    }
	    bu_vls_printf(logstr, " { %.25g %.25g %.25g }", V3ARGS(vg->coord));
	}
	bu_vls_strcat(logstr, " }");

	/* all the faces */
	if (BU_LIST_NON_EMPTY(&s->fu_hd)) {
	    for (BU_LIST_FOR_BACKWARDS(fu, faceuse, &s->fu_hd)) {
		if (fu->orientation != OT_SAME)
		    continue;

		bu_vls_strcat(logstr, " F {");

		/* all the loops in this face */
		for (BU_LIST_FOR_BACKWARDS(lu, loopuse, &fu->lu_hd)) {

		    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
			bu_vls_printf(logstr, " %d",
				      bu_ptbl_locate(&verts, (long *)vu->v_p));
		    } else {
			bu_vls_strcat(logstr, " {");
			for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
			    vu = eu->vu_p;
			    bu_vls_printf(logstr, " %d",
					  bu_ptbl_locate(&verts, (long *)vu->v_p));
			}
			/* end of this loop */
			bu_vls_strcat(logstr, " }");
		    }
		}

		/* end of this face */
		bu_vls_strcat(logstr, " }");
	    }
	}
	bu_ptbl_free(&verts);
    } else if (BU_STR_EQUAL(attr, "V")) {
	/* list of vertices */

	bu_ptbl_init(&verts, 256, "nmg verts");
	nmg_vertex_tabulate(&verts, &s->magic);
	for (i=0; i<BU_PTBL_LEN(&verts); i++) {
	    v = (struct vertex *) BU_PTBL_GET(&verts, i);
	    NMG_CK_VERTEX(v);
	    vg = v->vg_p;
	    if (!vg) {
		bu_vls_printf(logstr, "Vertex has no geometry\n");
		bu_ptbl_free(&verts);
		return BRLCAD_ERROR;
	    }
	    bu_vls_printf(logstr, " { %.25g %.25g %.25g }", V3ARGS(vg->coord));
	}
	bu_ptbl_free(&verts);
    } else {
	bu_vls_printf(logstr, "Unrecognized parameter\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
rt_nmg_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct shell *s=NULL;
    struct faceuse *fu=NULL;
    Tcl_Obj *obj, **obj_array;
    int len;
    int num_verts = 0;
    int num_loops = 0;
    int *loop;
    int loop_len;
    int i, j;
    struct tmp_v *verts;
    fastf_t *tmp;
    struct bn_tol tol;

    RT_CK_DB_INTERNAL(intern);
    s = (struct shell *)intern->idb_ptr;
    NMG_CK_SHELL(s);

    verts = (struct tmp_v *)NULL;
    for (i=0; i<argc; i += 2) {
	if (BU_STR_EQUAL(argv[i], "V")) {
	    obj = Tcl_NewStringObj(argv[i+1], -1);
	    if (Tcl_ListObjGetElements(brlcad_interp, obj, &num_verts,
				       &obj_array) != TCL_OK) {
		bu_vls_printf(logstr,
			      "ERROR: failed to parse vertex list\n");
		Tcl_DecrRefCount(obj);
		return BRLCAD_ERROR;
	    }
	    verts = (struct tmp_v *)bu_calloc(num_verts,
					      sizeof(struct tmp_v),
					      "verts");
	    for (j=0; j<num_verts; j++) {
		len = 3;
		tmp = &verts[j].pt[0];
		if (tcl_obj_to_fastf_array(brlcad_interp, obj_array[j],
					   &tmp, &len) != 3) {
		    bu_vls_printf(logstr,
				  "ERROR: incorrect number of coordinates for vertex\n");
		    return BRLCAD_ERROR;
		}
	    }

	}
    }

    while (argc >= 2) {
	struct vertex ***face_verts;

	if (BU_STR_EQUAL(argv[0], "V")) {
	    /* vertex list handled above */
	    goto cont;
	} else if (BU_STR_EQUAL(argv[0], "F")) {
	    if (!verts) {
		bu_vls_printf(logstr,
			      "ERROR: cannot set faces without vertices\n");
		return BRLCAD_ERROR;
	    }

	    obj = Tcl_NewStringObj(argv[1], -1);
	    if (Tcl_ListObjGetElements(brlcad_interp, obj, &num_loops,
				       &obj_array) != TCL_OK) {
		bu_vls_printf(logstr,
			      "ERROR: failed to parse face list\n");
		Tcl_DecrRefCount(obj);
		return BRLCAD_ERROR;
	    }
	    for (i=0, fu=NULL; i<num_loops; i++) {
		struct vertex **loop_verts;
		/* struct faceuse fu is initialized in earlier scope */

		loop_len = 0;
		(void)tcl_obj_to_int_array(brlcad_interp, obj_array[i],
					   &loop, &loop_len);
		if (!loop_len) {
		    bu_vls_printf(logstr,
				  "ERROR: unable to parse face list\n");
		    return BRLCAD_ERROR;
		}
		if (i) {
		    loop_verts = (struct vertex **)bu_calloc(
			loop_len,
			sizeof(struct vertex *),
			"loop_verts");
		    for (i=0; i<loop_len; i++) {
			loop_verts[i] = verts[loop[i]].v;
		    }
		    fu = nmg_add_loop_to_face(s, fu,
					      loop_verts, loop_len,
					      OT_OPPOSITE);
		    for (i=0; i<loop_len; i++) {
			verts[loop[i]].v = loop_verts[i];
		    }
		} else {
		    face_verts = (struct vertex ***)bu_calloc(
			loop_len,
			sizeof(struct vertex **),
			"face_verts");
		    for (j=0; j<loop_len; j++) {
			face_verts[j] = &verts[loop[j]].v;
		    }
		    fu = nmg_cmface(s, face_verts, loop_len);
		    bu_free((char *)face_verts, "face_verts");
		}
	    }
	} else {
	    bu_vls_printf(logstr,
			  "ERROR: Unrecognized parameter, must be V or F\n");
	    return BRLCAD_ERROR;
	}
    cont:
	argc -= 2;
	argv += 2;
    }

    /* assign geometry for entire vertex list (if we have one) */
    for (i=0; i<num_verts; i++) {
	if (verts[i].v)
	    nmg_vertex_gv(verts[i].v, verts[i].pt);
    }

    /* assign face geometry */
    if (s) {
	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	    if (fu->orientation != OT_SAME)
		continue;
	    nmg_calc_face_g(fu);
	}
    }

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    nmg_rebound(s, &tol);

    return BRLCAD_OK;
}


void
rt_nmg_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct shell *s;

    s = nmg_ms();
    intern->idb_ptr = (void *)s;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_NMG;
    intern->idb_meth = ftp;
}


int
rt_nmg_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/* contains information used to analyze a polygonal face */
struct poly_face
{
    char label[5];
    size_t npts;
    point_t *pts;
    plane_t plane_eqn;
    fastf_t area;
    fastf_t vol_pyramid;
    point_t cent_pyramid;
    point_t cent;
};


static void
rt_nmg_faces_area(struct poly_face* faces, struct shell* s)
{
    struct bu_ptbl nmg_faces;
    unsigned int num_faces, i;
    size_t *npts;
    point_t **tmp_pts;
    plane_t *eqs;
    nmg_face_tabulate(&nmg_faces, &s->magic);
    num_faces = BU_PTBL_LEN(&nmg_faces);
    tmp_pts = (point_t **)bu_calloc(num_faces, sizeof(point_t *), "rt_nmg_faces_area: tmp_pts");
    npts = (size_t *)bu_calloc(num_faces, sizeof(size_t), "rt_nmg_faces_area: npts");
    eqs = (plane_t *)bu_calloc(num_faces, sizeof(plane_t), "rt_nmg_faces_area: eqs");

    for (i = 0; i < num_faces; i++) {
	struct face *f;
	f = (struct face *)BU_PTBL_GET(&nmg_faces, i);
	HMOVE(faces[i].plane_eqn, f->g.plane_p->N);
	VUNITIZE(faces[i].plane_eqn);
	tmp_pts[i] = faces[i].pts;
	HMOVE(eqs[i], faces[i].plane_eqn);
    }
    bn_polygon_mk_pts_planes(npts, tmp_pts, num_faces, (const plane_t *)eqs);
    for (i = 0; i < num_faces; i++) {
	faces[i].npts = npts[i];
	bn_polygon_sort_ccw(faces[i].npts, faces[i].pts, faces[i].plane_eqn);
	bn_polygon_area(&faces[i].area, faces[i].npts, (const point_t *)faces[i].pts);
    }
    bu_free((char *)tmp_pts, "rt_nmg_faces_area: tmp_pts");
    bu_free((char *)npts, "rt_nmg_faces_area: npts");
    bu_free((char *)eqs, "rt_nmg_faces_area: eqs");
}


void
rt_nmg_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    struct shell *s;
    struct bu_ptbl nmg_faces;
    unsigned int num_faces, i;
    struct poly_face *faces;

    s = (struct shell *)ip->idb_ptr;

    /*get faces of this shell*/
    nmg_face_tabulate(&nmg_faces, &s->magic);
    num_faces = BU_PTBL_LEN(&nmg_faces);
    faces = (struct poly_face *)bu_calloc(num_faces, sizeof(struct poly_face), "rt_nmg_surf_area: faces");

    for (i = 0; i < num_faces; i++) {
	/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
	faces[i].pts = (point_t *)bu_calloc(num_faces - 1, sizeof(point_t), "rt_nmg_surf_area: pts");
    }

    rt_nmg_faces_area(faces, s);

    for (i = 0; i < num_faces; i++) {
	*area += faces[i].area;
    }

    for (i = 0; i < num_faces; i++) {
	bu_free((char *)faces[i].pts, "rt_nmg_surf_area: pts");
    }

    bu_free((char *)faces, "rt_nmg_surf_area: faces");
}


void
rt_nmg_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    struct shell* s;
    struct poly_face *faces;
    struct bu_ptbl nmg_faces;
    fastf_t volume = 0.0;
    point_t arbit_point = VINIT_ZERO;
    size_t num_faces, i;

    *cent[0] = 0.0;
    *cent[1] = 0.0;
    *cent[2] = 0.0;
    s = (struct shell *)ip->idb_ptr;

    /*get faces*/
    nmg_face_tabulate(&nmg_faces, &s->magic);
    num_faces = BU_PTBL_LEN(&nmg_faces);
    faces = (struct poly_face *)bu_calloc(num_faces, sizeof(struct poly_face), "rt_nmg_centroid: faces");

    for (i = 0; i < num_faces; i++) {
	/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
	faces[i].pts = (point_t *)bu_calloc(num_faces - 1, sizeof(point_t), "rt_nmg_centroid: pts");
    }
    rt_nmg_faces_area(faces, s);
    for (i = 0; i < num_faces; i++) {
	bn_polygon_centroid(&faces[i].cent, faces[i].npts, (const point_t *) faces[i].pts);
	VADD2(arbit_point, arbit_point, faces[i].cent);
    }
    VSCALE(arbit_point, arbit_point, (1/num_faces));

    for (i = 0; i < num_faces; i++) {
	vect_t tmp = VINIT_ZERO;

	/* calculate volume */
	volume = 0.0;
	VSCALE(tmp, faces[i].plane_eqn, faces[i].area);
	faces[i].vol_pyramid = (VDOT(faces[i].pts[0], tmp)/3);
	volume += faces[i].vol_pyramid;
	/*Vector from arbit_point to centroid of face, results in h of pyramid */
	VSUB2(faces[i].cent_pyramid, faces[i].cent, arbit_point);
	/*centroid of pyramid is 1/4 up from the bottom */
	VSCALE(faces[i].cent_pyramid, faces[i].cent_pyramid, 0.75f);
	/* now cent_pyramid is back in the polyhedron */
	VADD2(faces[i].cent_pyramid, faces[i].cent_pyramid, arbit_point);
	/* weight centroid of pyramid by pyramid's volume */
	VSCALE(faces[i].cent_pyramid, faces[i].cent_pyramid, faces[i].vol_pyramid);
	/* add cent_pyramid to the centroid of the polyhedron */
	VADD2(*cent, *cent, faces[i].cent_pyramid);
    }
    /* reverse the weighting */
    VSCALE(*cent, *cent, (1/volume));
    for (i = 0; i < num_faces; i++) {
	bu_free((char *)faces[i].pts, "rt_nmg_centroid: pts");
    }
    bu_free((char *)faces, "rt_nmg_centroid: faces");
}


void
rt_nmg_volume(fastf_t *volume, const struct rt_db_internal *ip)
{
    struct shell *s;
    struct bu_ptbl nmg_faces;
    unsigned int num_faces, i;
    struct poly_face *faces;

    s = (struct shell *)ip->idb_ptr;

    /*get faces of this shell*/
    nmg_face_tabulate(&nmg_faces, &s->magic);
    num_faces = BU_PTBL_LEN(&nmg_faces);
    faces = (struct poly_face *)bu_calloc(num_faces, sizeof(struct poly_face), "rt_nmg_volume: faces");

    for (i = 0; i < num_faces; i++) {
	/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
	faces[i].pts = (point_t *)bu_calloc(num_faces - 1, sizeof(point_t), "rt_nmg_volume: pts");
    }

    rt_nmg_faces_area(faces, s);

    for (i = 0; i < num_faces; i++) {
	vect_t tmp = VINIT_ZERO;

	/* calculate volume of pyramid*/
	VSCALE(tmp, faces[i].plane_eqn, faces[i].area);
	*volume = (VDOT(faces[i].pts[0], tmp)/3);
    }

    for (i = 0; i < num_faces; i++) {
	bu_free((char *)faces[i].pts, "rt_nmg_volume: pts");
    }

    bu_free((char *)faces, "rt_nmg_volume: faces");
}

/**
 * Store an NMG model as a separate .g file, for later examination.
 * Don't free the model, as the caller may still have uses for it.
 *
 * NON-PARALLEL because of rt_uniresource.
 */
void
nmg_stash_shell_to_file(const char *filename, const struct shell *s, const char *title)
{
    struct rt_wdb *fp;
    struct rt_db_internal intern;
    struct bu_external ext;
    int ret;
    int flags;
    char *name="error.s";

    bu_log("nmg_stash_shell_to_file('%s', %p, %s)\n", filename, (void *)s, title);

    NMG_CK_SHELL(s);
    nmg_vsshell(s);

    if ((fp = wdb_fopen(filename)) == NULL) {
	perror(filename);
	return;
    }

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)s;

    if (db_version(fp->dbip) < 5) {
	BU_EXTERNAL_INIT(&ext);
	ret = intern.idb_meth->ft_export4(&ext, &intern, 1.0, fp->dbip, &rt_uniresource);
	if (ret < 0) {
	    bu_log("rt_db_put_internal(%s):  solid export failure\n",
		   name);
	    ret = -1;
	    goto out;
	}
	db_wrap_v4_external(&ext, name);
    } else {
	if (rt_db_cvt_to_external5(&ext, name, &intern, 1.0, fp->dbip, &rt_uniresource, intern.idb_major_type) < 0) {
	    bu_log("wdb_export4(%s): solid export failure\n",
		   name);
	    ret = -2;
	    goto out;
	}
    }
    BU_CK_EXTERNAL(&ext);

    flags = db_flags_internal(&intern);
    ret = wdb_export_external(fp, &ext, name, flags, intern.idb_type);
out:
    bu_free_external(&ext);
    wdb_close(fp);

    bu_log("nmg_stash_model_to_file(): wrote error.s to '%s'\n",
	   filename);
}


/**
 * Number of bytes used for each value of DB5HDR_WIDTHCODE_*
 */
const int db5_enc_len[4] = {
    1,
    2,
    4,
    8
};

/**
 * this array depends on the values of the definitions of the
 * DB5_MINORTYPE_BINU_* in db5.h
 */
const char *binu_types[] = {
    NULL,
    NULL,
    "binary(float)",
    "binary(double)",
    "binary(u_8bit_int)",
    "binary(u_16bit_int)",
    "binary(u_32bit_int)",
    "binary(u_64bit_int)",
    NULL,
    NULL,
    NULL,
    NULL,
    "binary(8bit_int)",
    "binary(16bit_int)",
    "binary(32bit_int)",
    "binary(64bit_int)"
};

HIDDEN int
Shell_is_arb(const struct shell *s, struct bu_ptbl *tab)
{
    struct faceuse *fu;
    struct face *f;
    int four_verts=0;
    int three_verts=0;
    int face_count=0;
    int loop_count;

    NMG_CK_SHELL(s);

    nmg_vertex_tabulate(tab, &s->magic);

    if (BU_PTBL_END(tab) > 8 || BU_PTBL_END(tab) < 4)
	goto not_arb;

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	struct loopuse *lu;
	vect_t fu_norm;

	NMG_CK_FACEUSE(fu);

	if (fu->orientation != OT_SAME)
	    continue;

	f = fu->f_p;
	NMG_CK_FACE(f);

	if (*f->g.magic_p != NMG_FACE_G_PLANE_MAGIC)
	    goto not_arb;

	NMG_GET_FU_NORMAL(fu_norm, fu);

	loop_count = 0;
	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;

	    NMG_CK_LOOPUSE(lu);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		goto not_arb;

	    loop_count++;

	    /* face must be a single loop */
	    if (loop_count > 1)
		goto not_arb;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		struct edgeuse *eu_radial;
		struct faceuse *fu_radial;
		struct face *f_radial;
		vect_t norm_radial;
		vect_t eu_dir;
		vect_t cross;
		fastf_t dot;

		NMG_CK_EDGEUSE(eu);

		eu_radial = nmg_next_radial_eu(eu, s, 0);

		/* Can't have any dangling faces */
		if (eu_radial == eu || eu_radial == eu->eumate_p)
		    goto not_arb;

		fu_radial = nmg_find_fu_of_eu(eu_radial);
		NMG_CK_FACEUSE(fu_radial);

		if (fu_radial->orientation != OT_SAME)
		    fu_radial = fu_radial->fumate_p;

		f_radial = fu_radial->f_p;
		NMG_CK_FACE(f_radial);

		/* faces must be planar */
		if (*f_radial->g.magic_p != NMG_FACE_G_PLANE_MAGIC)
		    goto not_arb;


		/* Make sure shell is convex by checking that edgeuses
		 * run in direction fu_norm X norm_radial
		 */
		NMG_GET_FU_NORMAL(norm_radial, fu_radial);

		dot = VDOT(norm_radial, fu_norm);

		if (!NEAR_EQUAL(dot, 1.0, 0.00001)) {

		    VCROSS(cross, fu_norm, norm_radial);

		    if (eu->orientation == OT_NONE) {
			VSUB2(eu_dir, eu->vu_p->v_p->vg_p->coord, eu->eumate_p->vu_p->v_p->vg_p->coord);
			if (eu->orientation != OT_SAME) {
			    VREVERSE(eu_dir, eu_dir);
			}
		    } else {
			VMOVE(eu_dir, eu->g.lseg_p->e_dir);
		    }

		    if (eu->orientation == OT_SAME || eu->orientation == OT_NONE) {
			if (VDOT(cross, eu_dir) < 0.0)
			    goto not_arb;
		    } else {
			if (VDOT(cross, eu_dir) > 0.0)
			    goto not_arb;
		    }
		}
	    }
	}
    }

    /* count face types */
    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	struct loopuse *lu;
	int vert_count=0;

	if (fu->orientation != OT_SAME)
	    continue;

	face_count++;
	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		vert_count++;
	}

	if (vert_count == 3)
	    three_verts++;
	else if (vert_count == 4)
	    four_verts++;
    }

    /* must have only three and four vertices in each face */
    if (face_count != three_verts + four_verts)
	goto not_arb;

    /* which type of arb is this?? */
    switch (BU_PTBL_END(tab)) {
	case 4:		/* each face must have 3 vertices */
	    if (three_verts != 4 || four_verts != 0)
		goto not_arb;
	    break;
	case 5:		/* one face with 4 verts, four with 3 verts */
	    if (four_verts != 1 || three_verts != 4)
		goto not_arb;
	    break;
	case 6:		/* three faces with 4 verts, two with 3 verts */
	    if (three_verts != 2 || four_verts != 3)
		goto not_arb;
	    break;
	case 7:		/* four faces with 4 verts, two with 3 verts */
	    if (four_verts != 4 || three_verts != 2)
		goto not_arb;
	    break;
	case 8:		/* each face must have 4 vertices */
	    if (four_verts != 6 || three_verts != 0)
		goto not_arb;
	    break;
    }

    return BU_PTBL_END(tab);


not_arb:
    bu_ptbl_free(tab);
    return 0;
}

/**
 * Converts an NMG to an ARB, if possible.
 *
 * NMG must have been coplanar face merged and simplified
 *
 * Returns:
 * 	1 - Equivalent ARB was constructed
 * 	0 - Cannot construct an equivalent ARB
 *
 * The newly constructed arb is in "arb_int"
 */
int
nmg_to_arb(const struct shell *s, struct rt_arb_internal *arb_int)
{
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct vertex *v;
    struct edgeuse *eu_start;
    struct faceuse *fu1;
    struct bu_ptbl tab = BU_PTBL_INIT_ZERO;
    int face_verts;
    int i, j;
    int found;
    int ret_val = 0;

    NMG_CK_SHELL(s);

    switch (Shell_is_arb(s, &tab)) {
	case 0:
	    ret_val = 0;
	    break;
	case 4:
	    v = (struct vertex *)BU_PTBL_GET(&tab, 0);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[0], v->vg_p->coord);
	    v = (struct vertex *)BU_PTBL_GET(&tab, 1);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[1], v->vg_p->coord);
	    v = (struct vertex *)BU_PTBL_GET(&tab, 2);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[2], v->vg_p->coord);
	    VMOVE(arb_int->pt[3], v->vg_p->coord);
	    v = (struct vertex *)BU_PTBL_GET(&tab, 3);
	    NMG_CK_VERTEX(v);
	    VMOVE(arb_int->pt[4], v->vg_p->coord);
	    VMOVE(arb_int->pt[5], v->vg_p->coord);
	    VMOVE(arb_int->pt[6], v->vg_p->coord);
	    VMOVE(arb_int->pt[7], v->vg_p->coord);

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 5:
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    face_verts = 0;
	    while (face_verts != 4) {
		face_verts = 0;
		fu = BU_LIST_PNEXT_CIRC(faceuse, &fu->l);
		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		    face_verts++;
	    }
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    j = 0;
	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->eumate_p;
	    for (i=0; i<4; i++) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 6:
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    face_verts = 0;
	    while (face_verts != 3) {
		face_verts = 0;
		fu = BU_LIST_PNEXT_CIRC(faceuse, &fu->l);
		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		    face_verts++;
	    }
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);

	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    eu = eu_start;
	    VMOVE(arb_int->pt[1], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[0], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[4], eu->vu_p->v_p->vg_p->coord);
	    VMOVE(arb_int->pt[5], eu->vu_p->v_p->vg_p->coord);

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->radial_p->eumate_p;
	    VMOVE(arb_int->pt[2], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[3], eu->vu_p->v_p->vg_p->coord);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    VMOVE(arb_int->pt[6], eu->vu_p->v_p->vg_p->coord);
	    VMOVE(arb_int->pt[7], eu->vu_p->v_p->vg_p->coord);

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 7:
	    found = 0;
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    while (!found) {
		int verts4=0, verts3=0;

		lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		    struct loopuse *lu1;
		    struct edgeuse *eu1;
		    int vert_count=0;

		    fu1 = nmg_find_fu_of_eu(eu->radial_p);
		    lu1 = BU_LIST_FIRST(loopuse, &fu1->lu_hd);
		    for (BU_LIST_FOR (eu1, edgeuse, &lu1->down_hd))
			vert_count++;

		    if (vert_count == 4)
			verts4++;
		    else if (vert_count == 3)
			verts3++;
		}

		if (verts4 == 2 && verts3 == 2)
		    found = 1;
	    }
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    j = 0;
	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->radial_p->eumate_p;
	    fu1 = nmg_find_fu_of_eu(eu);
	    if (nmg_faces_are_radial(fu, fu1)) {
		eu = eu_start->radial_p;
		eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
		eu = eu->radial_p->eumate_p;
	    }
	    for (i=0; i<4; i++) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
		eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    }

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	case 8:
	    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		fu = fu->fumate_p;
	    NMG_CK_FACEUSE(fu);

	    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	    j = 0;
	    eu_start = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
	    }

	    eu = eu_start->radial_p;
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    eu = eu->radial_p->eumate_p;
	    for (i=0; i<4; i++) {
		VMOVE(arb_int->pt[j], eu->vu_p->v_p->vg_p->coord);
		j++;
		eu = BU_LIST_PNEXT_CIRC(edgeuse, &eu->l);
	    }

	    bu_ptbl_free(&tab);
	    ret_val = 1;
	    break;
	default:
	    bu_bomb("Shell_is_arb screwed up");
	    break;
    }
    if (ret_val)
	arb_int->magic = RT_ARB_INTERNAL_MAGIC;
    return ret_val;
}


/**
 * Converts an NMG to a TGC, if possible.
 *
 *
 * NMG must have been coplanar face merged and simplified
 *
 * Returns:
 * 	1 - Equivalent TGC was constructed
 * 	0 - Cannot construct an equivalent TGC
 *
 * The newly constructed tgc is in "tgc_int"
 *
 * Currently only supports RCC, and creates circumscribed RCC
 */
int
nmg_to_tgc(
    const struct shell *s,
    struct rt_tgc_internal *tgc_int,
    const struct bn_tol *tol)
{
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct faceuse *fu_base=(struct faceuse *)NULL;
    struct faceuse *fu_top=(struct faceuse *)NULL;
    int three_vert_faces=0;
    int four_vert_faces=0;
    int many_vert_faces=0;
    int base_vert_count=0;
    int top_vert_count=0;
    point_t sum = VINIT_ZERO;
    int vert_count=0;
    fastf_t one_over_vert_count;
    point_t base_center;
    fastf_t min_base_r_sq;
    fastf_t max_base_r_sq;
    fastf_t sum_base_r_sq;
    fastf_t ave_base_r_sq;
    fastf_t base_r;
    point_t top_center;
    fastf_t min_top_r_sq;
    fastf_t max_top_r_sq;
    fastf_t sum_top_r_sq;
    fastf_t ave_top_r_sq;
    fastf_t top_r;
    plane_t top_pl = HINIT_ZERO;
    plane_t base_pl = HINIT_ZERO;
    vect_t plv_1, plv_2;

    NMG_CK_SHELL(s);

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	int lu_count=0;

	NMG_CK_FACEUSE(fu);
	if (fu->orientation != OT_SAME)
	    continue;

	vert_count = 0;

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {

	    NMG_CK_LOOPUSE(lu);

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		return 0;

	    lu_count++;
	    if (lu_count > 1)
		return 0;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		vert_count++;
	}

	if (vert_count < 3)
	    return 0;

	if (vert_count == 4)
	    four_vert_faces++;
	else if (vert_count == 3)
	    three_vert_faces++;
	else {
	    many_vert_faces++;
	    if (many_vert_faces > 2)
		return 0;

	    if (many_vert_faces == 1) {
		fu_base = fu;
		base_vert_count = vert_count;
		NMG_GET_FU_PLANE(base_pl, fu_base);
	    } else if (many_vert_faces == 2) {
		fu_top = fu;
		top_vert_count = vert_count;
		NMG_GET_FU_PLANE(top_pl, fu_top);
	    }
	}
    }
    /* if there are any three vertex faces,
     * there must be an even number of them
     */
    if (three_vert_faces%2)
	return 0;

    /* base and top must have same number of vertices */
    if (base_vert_count != top_vert_count)
	return 0;

    /* Must have correct number of side faces */
    if ((base_vert_count != four_vert_faces) &&
	(base_vert_count*2 != three_vert_faces))
	return 0;

    if (!NEAR_EQUAL(VDOT(top_pl, base_pl), -1.0, tol->perp))
	return 0;

    /* This looks like a good candidate,
     * Calculate center of base and top faces
     */

    if (fu_base) {
	vert_count = 0;
	VSETALL(sum, 0.0);
	lu = BU_LIST_FIRST(loopuse, &fu_base->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;

	    NMG_CK_EDGEUSE(eu);

	    NMG_CK_VERTEXUSE(eu->vu_p);
	    NMG_CK_VERTEX(eu->vu_p->v_p);
	    vg = eu->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg);

	    VADD2(sum, sum, vg->coord);
	    vert_count++;
	}

	one_over_vert_count = 1.0/(fastf_t)vert_count;
	VSCALE(base_center, sum, one_over_vert_count);

	/* Calculate Average Radius */
	min_base_r_sq = MAX_FASTF;
	max_base_r_sq = (-min_base_r_sq);
	sum_base_r_sq = 0.0;
	lu = BU_LIST_FIRST(loopuse, &fu_base->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;
	    vect_t rad_vect;
	    fastf_t r_sq;

	    vg = eu->vu_p->v_p->vg_p;

	    VSUB2(rad_vect, vg->coord, base_center);
	    r_sq = MAGSQ(rad_vect);
	    if (r_sq > max_base_r_sq)
		max_base_r_sq = r_sq;
	    if (r_sq < min_base_r_sq)
		min_base_r_sq = r_sq;

	    sum_base_r_sq += r_sq;
	}

	ave_base_r_sq = sum_base_r_sq * one_over_vert_count;

	base_r = sqrt(max_base_r_sq);

	if (!NEAR_ZERO((max_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001) ||
	    !NEAR_ZERO((min_base_r_sq - ave_base_r_sq)/ave_base_r_sq, 0.001))
	    return 0;

	VSETALL(sum, 0.0);
	lu = BU_LIST_FIRST(loopuse, &fu_top->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;

	    NMG_CK_EDGEUSE(eu);

	    NMG_CK_VERTEXUSE(eu->vu_p);
	    NMG_CK_VERTEX(eu->vu_p->v_p);
	    vg = eu->vu_p->v_p->vg_p;
	    NMG_CK_VERTEX_G(vg);

	    VADD2(sum, sum, vg->coord);
	}

	VSCALE(top_center, sum, one_over_vert_count);

	/* Calculate Average Radius */
	min_top_r_sq = MAX_FASTF;
	max_top_r_sq = (-min_top_r_sq);
	sum_top_r_sq = 0.0;
	lu = BU_LIST_FIRST(loopuse, &fu_top->lu_hd);
	for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
	    struct vertex_g *vg;
	    vect_t rad_vect;
	    fastf_t r_sq;

	    vg = eu->vu_p->v_p->vg_p;

	    VSUB2(rad_vect, vg->coord, top_center);
	    r_sq = MAGSQ(rad_vect);
	    if (r_sq > max_top_r_sq)
		max_top_r_sq = r_sq;
	    if (r_sq < min_top_r_sq)
		min_top_r_sq = r_sq;

	    sum_top_r_sq += r_sq;
	}

	ave_top_r_sq = sum_top_r_sq * one_over_vert_count;
	top_r = sqrt(max_top_r_sq);

	if (!NEAR_ZERO((max_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001) ||
	    !NEAR_ZERO((min_top_r_sq - ave_top_r_sq)/ave_top_r_sq, 0.001))
	    return 0;


	VMOVE(tgc_int->v, base_center);
	VSUB2(tgc_int->h, top_center, base_center);

	bn_vec_perp(plv_1, top_pl);
	VCROSS(plv_2, top_pl, plv_1);
	VUNITIZE(plv_1);
	VUNITIZE(plv_2);
	VSCALE(tgc_int->a, plv_1, base_r);
	VSCALE(tgc_int->b, plv_2, base_r);
	VSCALE(tgc_int->c, plv_1, top_r);
	VSCALE(tgc_int->d, plv_2, top_r);

	tgc_int->magic = RT_TGC_INTERNAL_MAGIC;

    }

    return 1;
}

/**
 * XXX This routine is deprecated in favor of BoTs
 */
int
nmg_to_poly(const struct shell *s, struct rt_pg_internal *poly_int, const struct bn_tol *tol)
{
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct shell *dup_s;
    int max_count;
    int count_npts;
    int face_count=0;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (nmg_check_closed_shell(s, tol))
	return 0;

    dup_s = nmg_ms();

    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	if (fu->orientation != OT_SAME)
	    continue;
	(void)nmg_dup_face(fu, dup_s);
    }

    for (BU_LIST_FOR (fu, faceuse, &dup_s->fu_hd)) {
	NMG_CK_FACEUSE(fu);

	/* only do OT_SAME faces */
	if (fu->orientation != OT_SAME)
	    continue;

	/* count vertices in loops */
	max_count = 0;
	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    NMG_CK_LOOPUSE(lu);
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    if (lu->orientation != OT_SAME) {
		/* triangulate holes */
		max_count = 6;
		break;
	    }

	    count_npts = 0;
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		count_npts++;

	    if (count_npts > 5) {
		max_count = count_npts;
		break;
	    }
	    if (!nmg_lu_is_convex(lu, tol)) {
		/* triangulate non-convex faces */
		max_count = 6;
		break;
	    }
	}

	/* if any loop has more than 5 vertices, triangulate the face */
	if (max_count > 5) {
	    if (nmg_debug & DEBUG_BASIC)
		bu_log("nmg_to_poly: triangulating fu %p\n", (void *)fu);
	    nmg_triangulate_fu(fu, tol);
	}

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    NMG_CK_LOOPUSE(lu);
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    face_count++;
	}
    }

    poly_int->npoly = face_count;
    poly_int->poly = (struct rt_pg_face_internal *)bu_calloc(face_count,
							     sizeof(struct rt_pg_face_internal), "nmg_to_poly: poly");

    face_count = 0;

    for (BU_LIST_FOR (fu, faceuse, &dup_s->fu_hd)) {
	vect_t norm;

	NMG_CK_FACEUSE(fu);

	/* only do OT_SAME faces */
	if (fu->orientation != OT_SAME)
	    continue;

	NMG_GET_FU_NORMAL(norm, fu);

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    int pt_no=0;

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    /* count vertices in loop */
	    count_npts = 0;
	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		count_npts++;

	    poly_int->poly[face_count].npts = count_npts;
	    poly_int->poly[face_count].verts = (fastf_t *) bu_calloc(3*count_npts, sizeof(fastf_t), "nmg_to_poly: verts");
	    poly_int->poly[face_count].norms = (fastf_t *) bu_calloc(3*count_npts, sizeof(fastf_t), "nmg_to_poly: norms");

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		struct vertex_g *vg;

		vg = eu->vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);

		VMOVE(&(poly_int->poly[face_count].verts[pt_no*3]), vg->coord);
		VMOVE(&(poly_int->poly[face_count].norms[pt_no*3]), norm);

		pt_no++;
	    }
	    face_count++;
	}
    }

    poly_int->magic = RT_PG_INTERNAL_MAGIC;
    nmg_ks(dup_s);

    return 1;
}

/**
 * Convert an NMG to a BOT solid
 */
struct rt_bot_internal *
nmg_bot(struct shell *s, const struct bn_tol *tol)
{
    struct rt_bot_internal *bot;
    struct bu_ptbl nmg_vertices;
    struct bu_ptbl nmg_faces;
    size_t i, face_no;
    struct vertex *v;

    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    /* first convert the NMG to triangles */
    (void)nmg_triangulate_shell(s, tol);

    /* make a list of all the vertices */
    nmg_vertex_tabulate(&nmg_vertices, &s->magic);

    /* and a list of all the faces */
    nmg_face_tabulate(&nmg_faces, &s->magic);

    /* now build the BOT */
    BU_ALLOC(bot, struct rt_bot_internal);

    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;

    bot->num_vertices = BU_PTBL_LEN(&nmg_vertices);
    bot->num_faces = 0;

    /* count the number of triangles */
    for (i=0; i<BU_PTBL_LEN(&nmg_faces); i++) {
	struct face *f;
	struct faceuse *fu;
	struct loopuse *lu;

	f = (struct face *)BU_PTBL_GET(&nmg_faces, i);
	NMG_CK_FACE(f);

	fu = f->fu_p;

	if (fu->orientation != OT_SAME) {
	    fu = fu->fumate_p;
	    if (fu->orientation != OT_SAME) {
		bu_log("nmg_bot(): Face has no OT_SAME use!\n");
		bu_free((char *)bot->vertices, "BOT vertices");
		bu_free((char *)bot->faces, "BOT faces");
		bu_free((char *)bot, "BOT");
		return (struct rt_bot_internal *)NULL;
	    }
	}

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;
	    bot->num_faces++;
	}
    }

    bot->faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "BOT faces");
    bot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "BOT vertices");

    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    /* fill in the vertices */
    for (i=0; i<BU_PTBL_LEN(&nmg_vertices); i++) {
	struct vertex_g *vg;

	v = (struct vertex *)BU_PTBL_GET(&nmg_vertices, i);
	NMG_CK_VERTEX(v);

	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);

	VMOVE(&bot->vertices[i*3], vg->coord);
    }

    /* fill in the faces */
    face_no = 0;
    for (i=0; i<BU_PTBL_LEN(&nmg_faces); i++) {
	struct face *f;
	struct faceuse *fu;
	struct loopuse *lu;

	f = (struct face *)BU_PTBL_GET(&nmg_faces, i);
	NMG_CK_FACE(f);

	fu = f->fu_p;

	if (fu->orientation != OT_SAME) {
	    fu = fu->fumate_p;
	    if (fu->orientation != OT_SAME) {
		bu_log("nmg_bot(): Face has no OT_SAME use!\n");
		bu_free((char *)bot->vertices, "BOT vertices");
		bu_free((char *)bot->faces, "BOT faces");
		bu_free((char *)bot, "BOT");
		return (struct rt_bot_internal *)NULL;
	    }
	}

	for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
	    struct edgeuse *eu;
	    size_t vertex_no=0;

	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		continue;

	    for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		if (vertex_no > 2) {
		    bu_log("nmg_bot(): Face has not been triangulated!\n");
		    bu_free((char *)bot->vertices, "BOT vertices");
		    bu_free((char *)bot->faces, "BOT faces");
		    bu_free((char *)bot, "BOT");
		    return (struct rt_bot_internal *)NULL;
		}

		v = eu->vu_p->v_p;
		NMG_CK_VERTEX(v);


		bot->faces[ face_no*3 + vertex_no ] = bu_ptbl_locate(&nmg_vertices, (long *)v);

		vertex_no++;
	    }

	    face_no++;
	}
    }

    bu_ptbl_free(&nmg_vertices);
    bu_ptbl_free(&nmg_faces);

    return bot;
}

struct rt_g RTG = RT_G_INIT_ZERO;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
