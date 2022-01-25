/*                        I O . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup nmg_io */
/** @{ */
/** @file nmg/io.h */

#ifndef NMG_IO_H
#define NMG_IO_H

#include "common.h"

#include "vmath.h"
#include "bu/parse.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define V4_NAMESIZE 16
struct nmg_rec {
    char        N_id;                   /* DBID_NMG */
    char        N_version;              /* Version indicator */
    char        N_name[V4_NAMESIZE];
    char        N_pad2[2];              /* neatness */
    unsigned char       N_count[4];     /* # additional granules */
    unsigned char       N_structs[26*4];/* # of structs needed */
}; /* struct nmg_rec */

/* The following is a "stand-in" for the librt v4 record union - we need only
 * the nmg form of that union, so rather than pull in the full librt header
 * (which breaks separation and defines a lot of things we don't otherwise
 * need) we define just the part used for NMG I/O as an nmg_record union using
 * the same nmg_rec struct used by db4.h.
 *
 * Note that there is an additional wrinkle here - because the v4 export
 * routine uses the size of the librt union record, the v4 version of export
 * must know the caller's sizeof() value for union record in order to do the
 * math correctly.  We're deliberately NOT defining a union identical to that
 * in librt because we don't have knowledge of all the librt types, but that
 * also means there is zero guarantee the sizeof() calculations on the two
 * unions will match.  Since this logic was written with the librt sizeof()
 * result in mind, we need to use that size, and NOT the size of this cut-down
 * form of the union, when doing that calculation.
 */
union nmg_record {
    struct nmg_rec nmg;
};

#define NMG_CK_DISKMAGIC(_cp, _magic)	\
    if (bu_ntohl(*(uint32_t*)_cp, 0, UINT_MAX - 1) != _magic) { \
	bu_log("NMG_CK_DISKMAGIC: magic mis-match, got x%x, s/b x%x, file %s, line %d\n", \
	       bu_ntohl(*(uint32_t*)_cp, 0, UINT_MAX - 1), _magic, __FILE__, __LINE__); \
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
    disk_index_t la_p;
};


#define DISK_LOOP_A_MAGIC 0x4e6c5f67	/* Nl_g */
struct disk_loop_a {
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
#define NMG_KIND_LOOP_A            11
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


/* The last parameter is either sizeof(union record) (for v4 .g files) or
 * 0 (for v5 and later.) */
NMG_EXPORT extern struct model *
nmg_import(const struct bu_external *ep, const mat_t mat, int record_sizeof);

NMG_EXPORT extern int
nmg_export(struct bu_external *ep, struct model *m, double local2mm, int ver);

__END_DECLS

#endif  /* NMG_MODEL_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
