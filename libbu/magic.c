/*
 *			M A G I C . C
 *
 *  Routines involved with handling "magic numbers" used to identify
 *  various in-memory data structures.
 *
 *  The one ugly thing about this strategy is that every BRL-CAD
 *  library needs to have it's magic numbers registered here.
 *  XXX What is needed is an extension mechanism.
 *  It is a shame that C does not provide a wextern (weak extern) declaration.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSmagic[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "msr.h"
#include "wdb.h"
#include "spm.h"
#include "redblack.h"
#include "../libredblack/rb_internals.h"
#include "bu.h"
#include "bn.h"

/*
 *			B U _ I D E N T I F Y _ M A G I C
 *
 *  Given a number which has been found in the magic number field of
 *  a structure (which is typically the first entry),
 *  determine what kind of structure this magic number pertains to.
 *  This is called by the macro BU_CK_MAGIC() to provide a "hint"
 *  as to what sort of pointer error might have been made.
 */
CONST char *
bu_identify_magic( magic )
register long	magic;
{
	switch(magic)  {
	case 0:
		return "NULL";
	default:
		return "Unknown_Magic";
	/*
	 *  bu.h
	 */
	case BU_VLS_MAGIC:
		return "bu_vls";
	case BU_PTBL_MAGIC:
		return "bu_ptbl";
	case BU_BITV_MAGIC:
		return "bu_bitv";
	case BU_LIST_HEAD_MAGIC:
		return "bu_list";
	case BU_HIST_MAGIC:
		return "bu_hist";
	case BU_MAPPED_FILE_MAGIC:
		return "bu_mapped_file";
	case BU_EXTERNAL_MAGIC:
		return "bu_external";

	/*
	 *  bn.h
	 */
	case BN_POLY_MAGIC:
		return "bn_poly";

	/*
	 *  nmg.h:  NMG magic numbers
	 */
	case NMG_MODEL_MAGIC:
		return("model");
	case NMG_REGION_MAGIC:
		return("region");
	case NMG_REGION_A_MAGIC:
		return("region_a");
	case NMG_SHELL_MAGIC:
		return("shell");
	case NMG_SHELL_A_MAGIC:
		return("shell_a");
	case NMG_FACE_MAGIC:
		return("face");
	case NMG_FACE_G_PLANE_MAGIC:
		return("face_g_plane");
	case NMG_FACE_G_SNURB_MAGIC:
		return("face_g_snurb");
	case NMG_FACEUSE_MAGIC:
		return("faceuse");
	case NMG_LOOP_MAGIC:
		return("loop");
	case NMG_LOOP_G_MAGIC:
		return("loop_g");
	case NMG_LOOPUSE_MAGIC:
		return("loopuse");
	case NMG_EDGE_MAGIC:
		return("edge");
	case NMG_EDGE_G_LSEG_MAGIC:
		return("edge_g_lseg");
	case NMG_EDGE_G_CNURB_MAGIC:
		return("edge_g_cnurb");
	case NMG_KNOT_VECTOR_MAGIC:
		return("knot_vector");
	case NMG_EDGEUSE_MAGIC:
		return("edgeuse");
	case NMG_EDGEUSE2_MAGIC:
		return("edgeuse2 [midway into edgeuse]");
	case NMG_VERTEX_MAGIC:
		return("vertex");
	case NMG_VERTEX_G_MAGIC:
		return("vertex_g");
	case NMG_VERTEXUSE_MAGIC:
		return("vertexuse");
	case NMG_VERTEXUSE_A_PLANE_MAGIC:
		return("vertexuse_a_plane");
	case NMG_VERTEXUSE_A_CNURB_MAGIC:
		return("vertexuse_a_cnurb");
	/*
	 *  raytrace.h
	 */
	case RT_TOL_MAGIC:
		return("rt_tol");
	case RT_TESS_TOL_MAGIC:
		return("rt_tess_tol");
	case RT_DB_INTERNAL_MAGIC:
		return("rt_db_internal");
	case RT_SEG_MAGIC:
		return("rt seg");
	case RT_SOLTAB_MAGIC:
		return("rt soltab");
	case RT_REGION_MAGIC:
		return("rt region");
	case PT_MAGIC:
		return("rt partition");
	case PT_HD_MAGIC:
		return "rt partition list head";
	case DBI_MAGIC:
		return("rt db_i");
	case RT_DIR_MAGIC:
		return "(librt)directory";
	case DB_FULL_PATH_MAGIC:
		return "db_full_path";
	case RT_CTS_MAGIC:
		return "combined_tree_state";
	case RT_TREE_MAGIC:
		return "rt union tree";
	case ANIMATE_MAGIC:
		return("rt animate");
	case RESOURCE_MAGIC:
		return("rt resource");
	case RTI_MAGIC:
		return("rt_i");
	case RT_VLIST_MAGIC:
		return("rt_vlist");
	case RT_VLBLOCK_MAGIC:
		return("rt_vlblock");
	/*
	 *  rtgeom.h
	 */
	case RT_TOR_INTERNAL_MAGIC:
		return("rt_tor_internal");
	case RT_TGC_INTERNAL_MAGIC:
		return("rt_tgc_internal");
	case RT_ELL_INTERNAL_MAGIC:
		return("rt_ell_internal");
	case RT_ARB_INTERNAL_MAGIC:
		return("rt_arb_internal");
	case RT_ARS_INTERNAL_MAGIC:
		return("rt_ars_internal");
	case RT_HALF_INTERNAL_MAGIC:
		return("rt_half_internal");
	case RT_PG_INTERNAL_MAGIC:
		return("rt_pg_internal");
	case RT_EBM_INTERNAL_MAGIC:
		return("rt_ebm_internal");
	case RT_VOL_INTERNAL_MAGIC:
		return("rt_vol_internal");
	case RT_ARBN_INTERNAL_MAGIC:
		return("rt_arbn_internal");
	case RT_PIPE_INTERNAL_MAGIC:
		return("rt_pipe_internal");
	case RT_PART_INTERNAL_MAGIC:
		return("rt_part_internal");
	/*
	 * msr.h
	 */
	case MSR_UNIF_MAGIC:
		return("msr_unif_magic");
	case MSR_GAUSS_MAGIC:
		return("msr_gauss_magic");
	/*
	 * wdb.h
	 */
	case WMEMBER_MAGIC:
		return("wdb.h wmember_magic");
	case WDB_PIPESEG_MAGIC:
		return("wdb.h wdb_pipeseg_magic");

	/*
	 * fb.h -- not a good idea to include for real.
	 */
	case 0xfbfb00fb /* FB_MAGIC */:
		return("fb.h fb_magic");

	/*
	 *  spm.h
	 */
	case SPM_MAGIC:
		return "spm.h spm_map_t";

	/*
	 *  redblack.h
	 */
	case RB_TREE_MAGIC:
		return "red-black tree";

	/*
	 *  ../libredblack/rb_internals.h
	 */
	case RB_NODE_MAGIC:
		return "red-black node";
	case RB_PKG_MAGIC:
		return "red-black package";

	}
}
