/*                         M A G I C . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @addtogroup magic */
/** @{ */
/** @file magic.h
 *
 * Global registry of recognized magic numbers.
 *
 * This file is part of LIBBU even though it provides magic numbers
 * for structures in other libraries.
 *
 * The defines should be considered PRIVATE (even though they are not)
 * and should NEVER be referenced by value.
 */
/** @file magic.c
 *
 * Routines involved with handling "magic numbers" used to identify
 * various in-memory data structures.  Magic numbers provide a means
 * to perform run-time sanity checks for memory corruption and
 * uninitialized data.
 *
 * The one ugly thing about this implementation is that every BRL-CAD
 * structure needs to have its magic number registered here and in
 * the header.
 *
 */
/** @file badmagic.c
 *
 * Routines involved with handling "magic numbers" used to identify
 * various in-memory data structures.
 *
 */

#ifndef __MAGIC_H__
#define __MAGIC_H__

#include "common.h"

#include "tcl.h"

#include "bu.h"


/* libbu */

#define BU_AVS_MAGIC			0x41765321 /**< AvS! */
#define BU_BITV_MAGIC			0x62697476 /**< bitv */
#define BU_COLOR_MAGIC			0x6275636c
#define BU_EXTERNAL_MAGIC		0x768dbbd0
#define BU_HASH_ENTRY_MAGIC		0x48454E54 /**< HENT */
#define BU_HASH_RECORD_MAGIC		0x68617368 /**< hash */
#define BU_HASH_TBL_MAGIC		0x48415348 /**< HASH */
#define BU_HIST_MAGIC			0x48697374 /**< Hist */
#define BU_HOOK_LIST_MAGIC		0x90d5dead /**< Nietzsche? */
#define BU_IMAGE_FILE_MAGIC		0x6269666d /**< bifm */
#define BU_LIST_HEAD_MAGIC		0x01016580
#define BU_MAPPED_FILE_MAGIC		0x4d617066 /**< Mapf */
#define BU_MRO_MAGIC			0x4D524F4F /**< MROO */
#define BU_PTBL_MAGIC			0x7074626c /**< ptbl */
#define BU_RB_LIST_MAGIC		0x72626c73
#define BU_RB_NODE_MAGIC		0x72626e6f
#define BU_RB_PKG_MAGIC			0x7262706b
#define BU_RB_TREE_MAGIC		0x72627472
#define BU_VLB_MAGIC                    0x5F564C42 /**< _VLB */
#define BU_VLS_MAGIC			0x89333bbb

/* libbn */

#define BN_GAUSS_MAGIC			512256128
#define BN_POLY_MAGIC			0x506f4c79 /**< PoLy */
#define BN_TABDATA_MAGIC		0x53736d70
#define BN_TABLE_MAGIC			0x53706374
#define BN_TOL_MAGIC			0x98c734bb
#define BN_UNIF_MAGIC			12481632
#define BN_VLBLOCK_MAGIC		0x981bd112
#define BN_VLIST_MAGIC			0x98237474

/* primitive internals */

#define RT_ARBN_INTERNAL_MAGIC		0x18236461
#define RT_ARB_INTERNAL_MAGIC		0x9befd010
#define RT_ARS_INTERNAL_MAGIC		0x77ddbbe3
#define RT_BINUNIF_INTERNAL_MAGIC	0x42696e55 /**< BinU */
#define RT_BOT_INTERNAL_MAGIC		0x626F7472 /**< botr */
#define RT_BREP_INTERNAL_MAGIC		0x42524550 /**< BREP */
#define RT_CLINE_INTERNAL_MAGIC		0x43767378 /**< CLIN */
#define RT_DSP_INTERNAL_MAGIC		0x00000de6
#define RT_EBM_INTERNAL_MAGIC		0xf901b231
#define RT_EHY_INTERNAL_MAGIC		0xaaccee91
#define RT_ELL_INTERNAL_MAGIC		0x93bb23ff
#define RT_EPA_INTERNAL_MAGIC		0xaaccee90
#define RT_ETO_INTERNAL_MAGIC		0xaaccee92
#define RT_EXTRUDE_INTERNAL_MAGIC	0x65787472 /**< extr */
#define RT_GRIP_INTERNAL_MAGIC		0x31196205
#define RT_HALF_INTERNAL_MAGIC		0xaa87bbdd
#define RT_HF_INTERNAL_MAGIC		0x4846494d
#define RT_HYP_INTERNAL_MAGIC		0x68797065 /**< hype */
#define RT_METABALL_INTERNAL_MAGIC      0x62616c6c /**< ball */
#define RT_NURB_INTERNAL_MAGIC		0x002b2bdd
#define RT_PART_INTERNAL_MAGIC		0xaaccee87
#define RT_PG_INTERNAL_MAGIC		0x9bfed887
#define RT_PIPE_INTERNAL_MAGIC		0x7dd7bb3e
#define RT_REVOLVE_INTERNAL_MAGIC	0x7265766C /**< revl */
#define RT_RHC_INTERNAL_MAGIC		0xaaccee89
#define RT_RPC_INTERNAL_MAGIC		0xaaccee88
#define RT_SKETCH_INTERNAL_MAGIC	0x736b6574 /**< sket */
#define RT_SUBMODEL_INTERNAL_MAGIC	0x7375626d /**< subm */
#define RT_SUPERELL_INTERNAL_MAGIC      0xff93bb23
#define RT_TGC_INTERNAL_MAGIC		0xaabbdd87
#define RT_TOR_INTERNAL_MAGIC		0x9bffed87
#define RT_VOL_INTERNAL_MAGIC		0x987ba1d0
#define RT_PNTS_INTERNAL_MAGIC          0x706e7473 /**< pnts */

/* n-manifold geometry */

#define NMG_EDGEUSE2_MAGIC		0x91919191 /**< used in eu->l2.magic */
#define NMG_EDGEUSE_MAGIC		0x90909090
#define NMG_EDGE_G_CNURB_MAGIC		0x636e7262 /**< was RT_CNURB_MAGIC */
#define NMG_EDGE_G_LSEG_MAGIC		0x6c696768
#define NMG_EDGE_MAGIC			0x33333333
#define NMG_FACEUSE_MAGIC		0x56565656
#define NMG_FACE_G_PLANE_MAGIC		0x726b6e65
#define NMG_FACE_G_SNURB_MAGIC		0x736e7262 /**< was RT_SNURB_MAGIC */
#define NMG_FACE_MAGIC			0x45454545
#define NMG_INTER_STRUCT_MAGIC		0x99912120
#define NMG_KNOT_VECTOR_MAGIC		0x6b6e6f74 /**< was RT_KNOT_VECTOR_MAGIC */
#define NMG_LOOPUSE_MAGIC		0x78787878
#define NMG_LOOP_G_MAGIC		0x6420224c
#define NMG_LOOP_MAGIC			0x67676767
#define NMG_MODEL_MAGIC 		0x12121212
#define NMG_RADIAL_MAGIC		0x52614421 /**< RaD! */
#define NMG_RAY_DATA_MAGIC 		0x01651771
#define NMG_REGION_A_MAGIC		0x696e6720
#define NMG_REGION_MAGIC		0x23232323
#define NMG_RT_HIT_MAGIC 		0x48697400 /**< Hit  */
#define NMG_RT_HIT_SUB_MAGIC		0x48696d00 /**< Him  */
#define NMG_RT_MISS_MAGIC		0x4d697300 /**< Mis  */
#define NMG_SHELL_A_MAGIC		0x65207761
#define NMG_SHELL_MAGIC 		0x71077345 /**< shell oil */
#define NMG_VERTEXUSE_A_CNURB_MAGIC	0x20416e64
#define NMG_VERTEXUSE_A_PLANE_MAGIC	0x69676874
#define NMG_VERTEXUSE_MAGIC		0x12341234
#define NMG_VERTEX_G_MAGIC		0x72737707
#define NMG_VERTEX_MAGIC		0x00123123

/* raytrace */

#define RT_ANP_MAGIC			0x41507270 /**< APrp */
#define RT_AP_MAGIC			0x4170706c /**< Appl */
#define RT_CNURB_MAGIC			0x636e7262
#define RT_COMB_MAGIC			0x436f6d49 /**< ComI */
#define RT_CONSTRAINT_MAGIC		0x7063696d /**< pcim */
#define RT_CTS_MAGIC			0x98989123
#define RT_DBTR_MAGIC			0x64627472 /**< dbtr */
#define RT_DBTS_MAGIC			0x64627473 /**< dbts */
#define RT_DB_INTERNAL_MAGIC		0x0dbbd867
#define RT_DIR_MAGIC			0x05551212 /**< Directory assistance */
#define RT_FUNCTAB_MAGIC		0x46754e63 /**< FuNc */
#define RT_HIT_MAGIC			0x20686974 /**< hit  */
#define RT_HTBL_MAGIC			0x6874626c /**< htbl */
#define RT_PIECELIST_MAGIC		0x70636c73 /**< pcls */
#define RT_PIECESTATE_MAGIC		0x70637374 /**< pcst */
#define RT_RAY_MAGIC			0x78726179 /**< xray */
#define RT_REGION_MAGIC			0xdffb8001
#define RT_SEG_MAGIC			0x98bcdef1
#define RT_SNURB_MAGIC			0x736e7262
#define RT_SOLTAB2_MAGIC		0x92bfcde2 /**< l2.magic */
#define RT_SOLTAB_MAGIC			0x92bfcde0 /**< l.magic */
#define RT_TESS_TOL_MAGIC		0xb9090dab
#define RT_TREE_MAGIC			0x91191191
#define RT_WDB_MAGIC		       	0x5f576462

/* misc */

#define ANIMATE_MAGIC			0x414e4963 /**< 1095649635 */
#define CURVE_BEZIER_MAGIC		0x62657a69 /**< bezi */
#define CURVE_CARC_MAGIC		0x63617263 /**< carc */
#define CURVE_LSEG_MAGIC		0x6c736567 /**< lseg */
#define CURVE_NURB_MAGIC		0x6e757262 /**< nurb */
#define DB5_RAW_INTERNAL_MAGIC		0x64357269 /**< d5ri */
#define DBI_MAGIC			0x57204381
#define DB_FULL_PATH_MAGIC		0x64626670
#define FB_MAGIC			0xfbfb00fb
#define LIGHT_MAGIC			0xdbddbdb7
#define MF_MAGIC			0x55968058
#define PIXEL_EXT_MAGIC 		0x50787400 /**< Pxt  */
#define PL_MAGIC        		0x0beef00d
#define PT_HD_MAGIC			0x87687680
#define PT_MAGIC			0x87687681
#define RESOURCE_MAGIC			0x83651835
#define RTI_MAGIC			0x99101658
#define SPM_MAGIC			0x41278678
#define VERT_TREE_MAGIC			0x56455254 /**< VERT */
#define WDB_METABALLPT_MAGIC		0x6D627074 /**< mbpt */
#define WDB_PIPESEG_MAGIC		0x9723ffef
#define WMEMBER_MAGIC			0x43128912


/**
 * B U _ C K M A G
 * B U _ C K M A G _ T C L
 *
 * Macros to check and validate a structure pointer, given that the
 * first entry in the structure is a magic number.
 */
#ifdef NO_BOMBING_MACROS
#  define BU_CKMAG(_ptr, _magic, _str)
#  define BU_CKMAG_TCL(_interp, _ptr, _magic, _str)
#else
#  define BU_CKMAG(_ptr, _magic, _str) { \
	uintptr_t _ptrval = (uintptr_t)(_ptr); \
	if (UNLIKELY((_ptrval == 0) || (_ptrval & (sizeof(_ptrval)-1)) || *((unsigned long *)(_ptr)) != (unsigned long)(_magic))) { \
		bu_badmagic((unsigned long *)(_ptr), (unsigned long)_magic, _str, __FILE__, __LINE__); \
	} \
}
#  define BU_CKMAG_TCL(_interp, _ptr, _magic, _str) { \
	uintptr_t _ptrval = (uintptr_t)(_ptr); \
	if (UNLIKELY((_ptrval == 0) || (_ptrval & (sizeof(_ptrval)-1)) || *((unsigned long *)(_ptr)) != (_magic))) { \
		bu_badmagic_tcl((_interp), (unsigned long *)(_ptr), (unsigned long)_magic, _str, __FILE__, __LINE__); \
		return TCL_ERROR; \
	} \
}
#endif


/**
 * b u _ b a d m a g i c
 *
 *  Support routine for BU_CKMAG macro.
 */
BU_EXPORT BU_EXTERN(void bu_badmagic, (const unsigned long *ptr, unsigned long magic, const char *str, const char *file, int line));

/**
 * b u _ b a d m a g i c _ t c l
 *
 * Bad magic checking for Tcl routines.  The presence of Tcl_Interp as
 * an arg prevents giving arg list.
 *
 * Support routine for BU_CKMAG_TCL macro. As used by BU_CKMAG_TCL,
 * this routine is not called unless there is trouble with the
 * pointer. When called, an appropriate message is added to interp
 * indicating the problem.
 *
 * @param interp	- tcl interpreter where result is stored
 * @param ptr	- pointer to a data structure
 * @param magic	- the correct/desired magic number
 * @param str	- usually indicates the data structure name
 * @param file	- file where this routine was called
 * @param line	- line number in the above file
 *
 * @return
 * void
 */
BU_EXPORT BU_EXTERN(void bu_badmagic_tcl, (Tcl_Interp *interp, const unsigned long *ptr, unsigned long magic, const char *str, const char *file, int line));


/**
 * b u _ i d e n t i f y _ m a g i c
 *
 * Given a number which has been found in the magic number field of a
 * structure (which is typically the first entry), determine what kind
 * of structure this magic number pertains to.  This is called by the
 * macro BU_CK_MAGIC() to provide a "hint" as to what sort of pointer
 * error might have been made.
 */
BU_EXPORT BU_EXTERN(const char *bu_identify_magic, (unsigned long magic));


#endif /* __MAGIC_H__ */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
