/*                     C S G B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"
#include "bu.h"
#include "opennurbs.h"
#include "rtgeom.h"
#include "wdb.h"


#define OBJ_BREP
/* without OBJ_BREP, this entire procedural example is disabled */
#ifdef OBJ_BREP

#include "bio.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "vmath.h"		/* BRL-CAD Vector macros */
#include "wdb.h"

#ifdef __cplusplus
}
#endif


void
write_out(struct rt_wdb* fp, struct rt_db_internal *ip, const char *name, struct bn_tol *tol)
{
    ON_Brep* brep = NULL;

    std::string bname = name;
    bname += ".brep";

    if (!fp || !ip || !name)
	return;

    if (fp->dbip->dbi_fp) {
	rt_fwrite_internal(fp->dbip->dbi_fp, name, ip, 1.0);
    }
    brep = ON_Brep::New();
    ip->idb_meth->ft_brep(&brep, ip, tol);
    mk_brep(fp, bname.c_str(), brep);
    // delete brep; 
}


int
main(int argc, char** argv)
{
    struct rt_wdb* outfp;
    struct rt_db_internal tmp_internal;
    ON_TextLog error_log;

    RT_DB_INTERNAL_INIT(&tmp_internal);

    bn_tol tol;
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = SMALL_FASTF;
    tol.para = 1.0 - tol.perp;

    if (argc > 1)
	bu_log("Usage: %s\n", argv[0]);

    ON::Begin();

    outfp = wdb_fopen("csgbrep.g");
    const char* id_name = "CSG B-Rep Examples";
    mk_id(outfp, id_name);

    bu_log("ARB4\n");
    struct rt_arb_internal arb4;
    arb4.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb4.pt[0], 1000, -1000, -1000);
    VSET(arb4.pt[1], 1000, 1000, -1000);
    VSET(arb4.pt[2], 1000, 1000, 1000);
    VSET(arb4.pt[3], 1000, 1000, 1000);
    VSET(arb4.pt[4], -1000, 1000, -1000);
    VSET(arb4.pt[5], -1000, 1000, -1000);
    VSET(arb4.pt[6], -1000, 1000, -1000);
    VSET(arb4.pt[7], -1000, 1000, -1000);
    tmp_internal.idb_ptr = (genptr_t)&arb4;
    tmp_internal.idb_meth = &rt_functab[ID_ARB8];
    write_out(outfp, &tmp_internal, "arb4", &tol);
 
    bu_log("ARB5\n");
    struct rt_arb_internal arb5;
    arb5.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb5.pt[0], 1000, -1000, -1000);
    VSET(arb5.pt[1], 1000, 1000, -1000);
    VSET(arb5.pt[2], 1000, 1000, 1000);
    VSET(arb5.pt[3], 1000, -1000, 1000);
    VSET(arb5.pt[4], -1000, 0, 0);
    VSET(arb5.pt[5], -1000, 0, 0);
    VSET(arb5.pt[6], -1000, 0, 0);
    VSET(arb5.pt[7], -1000, 0, 0);
    tmp_internal.idb_ptr = (genptr_t)&arb5;
    tmp_internal.idb_meth = &rt_functab[ID_ARB8];
    write_out(outfp, &tmp_internal, "arb5", &tol);
 
    bu_log("ARB6\n");
    struct rt_arb_internal arb6;
    arb6.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb6.pt[0], 1000, -1000, -1000);
    VSET(arb6.pt[1], 1000, 1000, -1000);
    VSET(arb6.pt[2], 1000, 1000, 1000);
    VSET(arb6.pt[3], 1000, -1000, 1000);
    VSET(arb6.pt[4], -1000, 0, -1000);
    VSET(arb6.pt[5], -1000, 0, -1000);
    VSET(arb6.pt[6], -1000, 0, 1000);
    VSET(arb6.pt[7], -1000, 0, 1000);
    tmp_internal.idb_ptr = (genptr_t)&arb6;
    tmp_internal.idb_meth = &rt_functab[ID_ARB8];
    write_out(outfp, &tmp_internal, "arb6", &tol);
 
    bu_log("ARB7\n");
    struct rt_arb_internal arb7;
    arb7.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb7.pt[0], 1000, -1000, -500);
    VSET(arb7.pt[1], 1000, 1000, -500);
    VSET(arb7.pt[2], 1000, 1000, 1500);
    VSET(arb7.pt[3], 1000, -1000, 500);
    VSET(arb7.pt[4], -1000, -1000, -500);
    VSET(arb7.pt[5], -1000, 1000, -500);
    VSET(arb7.pt[6], -1000, 1000, 500);
    VSET(arb7.pt[7], -1000, -1000, -500);
    tmp_internal.idb_ptr = (genptr_t)&arb7;
    tmp_internal.idb_meth = &rt_functab[ID_ARB8];
    write_out(outfp, &tmp_internal, "arb7", &tol);
 
    bu_log("ARB8\n");
    struct rt_arb_internal arb8;
    arb8.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb8.pt[0], 1015, -1000, -995);
    VSET(arb8.pt[1], 1015, 1000, -995);
    VSET(arb8.pt[2], 1015, 1000, 1005);
    VSET(arb8.pt[3], 1015, -1000, 1005);
    VSET(arb8.pt[4], -985, -1000, -995);
    VSET(arb8.pt[5], -985, 1000, -995);
    VSET(arb8.pt[6], -985, 1000, 1005);
    VSET(arb8.pt[7], -985, -1000, 1005);
    tmp_internal.idb_ptr = (genptr_t)&arb8;
    tmp_internal.idb_meth = &rt_functab[ID_ARB8];
    write_out(outfp, &tmp_internal, "arb8", &tol);
    
    bu_log("ARBN\n");
    struct rt_arbn_internal arbn;
    arbn.magic = RT_ARBN_INTERNAL_MAGIC;
    arbn.neqn = 8;
    arbn.eqn = (plane_t *)bu_calloc(arbn.neqn, sizeof(plane_t), "arbn plane eqns");
    VSET(arbn.eqn[0], 1, 0, 0);
    arbn.eqn[0][3] = 1000;
    VSET(arbn.eqn[1], -1, 0, 0);
    arbn.eqn[1][3] = 1000;
    VSET(arbn.eqn[2], 0, 1, 0);
    arbn.eqn[2][3] = 1000;
    VSET(arbn.eqn[3], 0, -1, 0);
    arbn.eqn[3][3] = 1000;
    VSET(arbn.eqn[4], 0, 0, 1);
    arbn.eqn[4][3] = 1000;
    VSET(arbn.eqn[5], 0, 0, -1);
    arbn.eqn[5][3] = 1000;
    VSET(arbn.eqn[6], 0.57735, 0.57735, 0.57735);
    arbn.eqn[6][3] = 1000;
    VSET(arbn.eqn[7], -0.57735, -0.57735, -0.57735);
    arbn.eqn[7][3] = 1000;
    tmp_internal.idb_ptr = (genptr_t)&arbn;
    tmp_internal.idb_meth = &rt_functab[ID_ARBN];
    write_out(outfp, &tmp_internal, "arbn", &tol);
    bu_free(arbn.eqn, "free arbn eqn");
  
    // This routine does explicitly what is done
    // by the previous ARB8 brep call internally.
    // Ideally a more general NMG will be created
    // and fed to rt_nmg_brep rather than using an
    // ARB8 as a starting point.
    bu_log("NMG\n");
    struct rt_arb_internal arbnmg8;
    arbnmg8.magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arbnmg8.pt[0], 0,0,0);
    VSET(arbnmg8.pt[1], 0,2000,0);
    VSET(arbnmg8.pt[2], 0,2000,2000);
    VSET(arbnmg8.pt[3], 0,0,2000);
    VSET(arbnmg8.pt[4], -2000,0, 0);
    VSET(arbnmg8.pt[5], -2000,2000,0);
    VSET(arbnmg8.pt[6], -2000,2000,2000);
    VSET(arbnmg8.pt[7], -2000,0,2000);
    tmp_internal.idb_ptr = (genptr_t)&arbnmg8;

    // Now, need nmg form of the arb
    struct model *m = nmg_mm();
    struct nmgregion *r;
    struct rt_tess_tol ttol;
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;
    tmp_internal.idb_ptr = (genptr_t)&arbnmg8;
    tmp_internal.idb_meth = &rt_functab[ID_ARB8];
    tmp_internal.idb_meth->ft_tessellate(&r, m, &tmp_internal, &ttol, &tol);

    tmp_internal.idb_ptr = m;
    tmp_internal.idb_meth = &rt_functab[ID_NMG];
    write_out(outfp, &tmp_internal, "nmg", &tol);
    FREE_MODEL(m);
    
    bu_log("SPH\n");
    struct rt_ell_internal sph;
    sph.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(sph.v, 0.0, 0.0, 0.0);
    VSET(sph.a, 5.0, 0.0, 0.0);
    VSET(sph.b, 0.0, 5.0, 0.0);
    VSET(sph.c, 0.0, 0.0, 5.0);
    tmp_internal.idb_ptr = (genptr_t)&sph;
    tmp_internal.idb_meth = &rt_functab[ID_SPH];
    write_out(outfp, &tmp_internal, "sph", &tol);

    bu_log("ELL\n");
    struct rt_ell_internal ell;
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, 0.0, 0.0, 0.0);
    VSET(ell.a, 5.0, 0.0, 0.0);
    VSET(ell.b, 0.0, 3.0, 0.0);
    VSET(ell.c, 0.0, 0.0, 1.0);
    tmp_internal.idb_ptr = (genptr_t)&ell;
    tmp_internal.idb_meth = &rt_functab[ID_ELL];
    write_out(outfp, &tmp_internal, "ell", &tol);
    
    bu_log("RHC\n");
    struct rt_rhc_internal rhc;
    rhc.rhc_magic = RT_RHC_INTERNAL_MAGIC;
    VSET(rhc.rhc_V, 0.0, 0.0, 0.0);
    VSET(rhc.rhc_H, 0.0, 2000.0, 0.0);
    VSET(rhc.rhc_B, 0.0, 0.0, 2000.0);
    rhc.rhc_r = 1000.0;
    rhc.rhc_c = 400.0;
    tmp_internal.idb_ptr = (genptr_t)&rhc;
    tmp_internal.idb_meth = &rt_functab[ID_RHC];
    write_out(outfp, &tmp_internal, "rhc", &tol);

    bu_log("RPC\n");
    struct rt_rpc_internal rpc;
    rpc.rpc_magic = RT_RPC_INTERNAL_MAGIC;
    VSET(rpc.rpc_V, 24.0, -1218.0, -1000.0);
    VSET(rpc.rpc_H, 60.0, 2000.0, -100.0);
    VCROSS(rpc.rpc_B, rpc.rpc_V, rpc.rpc_H);
    VUNITIZE(rpc.rpc_B);
    VSCALE(rpc.rpc_B, rpc.rpc_B, 2000.0);
    rpc.rpc_r = 1000.0;
    tmp_internal.idb_ptr = (genptr_t)&rpc;
    tmp_internal.idb_meth = &rt_functab[ID_RPC];
    write_out(outfp, &tmp_internal, "rpc", &tol);

    bu_log("EPA\n");
    struct rt_epa_internal epa;
    epa.epa_magic = RT_EPA_INTERNAL_MAGIC;
    VSET(epa.epa_V, 0.0, 0.0, 0.0);
    VSET(epa.epa_H, 0.0, 0.0, 2000.0);
    VSET(epa.epa_Au, 1.0, 0.0, 0.0);
    epa.epa_r1 = 1000.0;
    epa.epa_r2 = 500.0;
    tmp_internal.idb_ptr = (genptr_t)&epa;
    tmp_internal.idb_meth = &rt_functab[ID_EPA];
    write_out(outfp, &tmp_internal, "epa", &tol);

    bu_log("EHY\n");
    struct rt_ehy_internal ehy;
    ehy.ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VSET(ehy.ehy_V, 0, 0, 0);
    VSET(ehy.ehy_H, 0, 0, 2000);
    VSET(ehy.ehy_Au, 1, 0, 0);
    ehy.ehy_r1 = 1000;
    ehy.ehy_r2 = 500;
    ehy.ehy_c = 400;
    tmp_internal.idb_ptr = (genptr_t)&ehy;
    tmp_internal.idb_meth = &rt_functab[ID_EHY];
    write_out(outfp, &tmp_internal, "ehy", &tol);

    bu_log("HYP\n");
    struct rt_hyp_internal hyp;
    hyp.hyp_magic = RT_HYP_INTERNAL_MAGIC;
    VSET(hyp.hyp_Vi, 0, 0, 0);
    VSET(hyp.hyp_Hi, 0, 0, 200);
    VSET(hyp.hyp_A, 100, 0, 0);
    hyp.hyp_b = 50;
    hyp.hyp_bnr = 0.5;
    tmp_internal.idb_ptr = (genptr_t)&hyp;
    tmp_internal.idb_meth = &rt_functab[ID_HYP];
    write_out(outfp, &tmp_internal, "hyp", &tol);

    bu_log("TGC\n");
    struct rt_tgc_internal tgc;
    tgc.magic = RT_TGC_INTERNAL_MAGIC;
    VSET(tgc.v, 0, 0, -1000);
    VSET(tgc.h, 0, 0, 2000);
    VSET(tgc.a, 500, 0, 0);
    VSET(tgc.b, 0, 250, 0);
    VSET(tgc.c, 250, 0, 0);
    VSET(tgc.d, 0, 500, 0);
    tmp_internal.idb_ptr = (genptr_t)&tgc;
    tmp_internal.idb_meth = &rt_functab[ID_TGC];
    write_out(outfp, &tmp_internal, "tgc", &tol);
   
    bu_log("TOR\n");
    struct rt_tor_internal tor;
    tor.magic = RT_TOR_INTERNAL_MAGIC;
    VSET(tor.v, 0.0, 0.0, 0.0);
    VSET(tor.h, 0.0, 0.0, 1.0);
    tor.r_a = 5.0;
    tor.r_h = 2.0;
    tmp_internal.idb_ptr = (genptr_t)&tor;
    tmp_internal.idb_meth = &rt_functab[ID_TOR];
    write_out(outfp, &tmp_internal, "tor", &tol);
    
    bu_log("ETO\n");
    struct rt_eto_internal eto;
    eto.eto_magic = RT_ETO_INTERNAL_MAGIC;
    VSET(eto.eto_V, 0.0, 0.0, 0.0);
    VSET(eto.eto_N, 0.0, 0.0, 1.0);
    VSET(eto.eto_C, 200.0, 0.0, 200.0);
    eto.eto_r = 800;
    eto.eto_rd = 100;
    tmp_internal.idb_ptr = (genptr_t)&eto;
    tmp_internal.idb_meth = &rt_functab[ID_ETO];
    write_out(outfp, &tmp_internal, "eto", &tol);

    bu_log("PIPE\n");
    struct wdb_pipept pipe1[] = {
     {
         {(long)WDB_PIPESEG_MAGIC, 0, 0},
	 {0, 1000, 0},
	 50, 100, 100
     },
     {
	 {(long)WDB_PIPESEG_MAGIC, 0, 0},
	 {4000, 5000, 0},
	 50,100,2000
     },
     {
	 {(long)WDB_PIPESEG_MAGIC, 0, 0},
	 {4000, 9000, 0},
	 50,100,1500
     },
     {
	 {(long)WDB_PIPESEG_MAGIC, 0, 0},
	 {9000, 9000, 0},
	 50,100,100
     }
    };
    int pipe1_npts = sizeof(pipe1)/sizeof(struct wdb_pipept);
    
    struct rt_pipe_internal pipe;
    BU_LIST_INIT(&pipe.pipe_segs_head);
    pipe.pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    
    for (int i=0; i<pipe1_npts; i++) {
	BU_LIST_INSERT(&pipe.pipe_segs_head, &pipe1[i].l);
    }
    tmp_internal.idb_ptr = (genptr_t)&pipe;
    tmp_internal.idb_meth = &rt_functab[ID_PIPE];
    write_out(outfp, &tmp_internal, "pipe", &tol);
    
    bu_log("SKETCH\n");
    struct rt_sketch_internal skt;
    struct bezier_seg *bsg;
    struct line_seg *lsg;
    struct carc_seg *csg;
    point2d_t verts[] = {
        { 250, 0 },     // 0 
	{ 500, 0 },     // 1 
	{ 500, 500 },   // 2 
	{ 0, 500 },     // 3 
	{ 0, 250 },     // 4 
	{ 250, 250 },   // 5 
	{ 125, 125 },   // 6 
	{ 0, 125 },     // 7 
	{ 125, 0 },     // 8 
	{ 200, 200 }    // 9 
    };
    size_t cnti;
    skt.magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(skt.V, 10.0, 20.0, 30.0);
    VSET(skt.u_vec, 1.0, 0.0, 0.0);
    VSET(skt.v_vec, 0.0, 1.0, 0.0);
    skt.vert_count = 10;
    skt.verts = (point2d_t *)bu_calloc(skt.vert_count, sizeof(point2d_t), "verts");
    for (cnti=0; cnti < skt.vert_count; cnti++) {
	V2MOVE(skt.verts[cnti], verts[cnti]);
    }
    skt.skt_curve.seg_count = 6;
    skt.skt_curve.reverse = (int *)bu_calloc(skt.skt_curve.seg_count, sizeof(int), "sketch: reverse");
    skt.skt_curve.segments = (genptr_t *)bu_calloc(skt.skt_curve.seg_count, sizeof(genptr_t), "segs");

    BU_GETSTRUCT(bsg, bezier_seg);
    bsg->magic = CURVE_BEZIER_MAGIC;
    bsg->degree = 4;
    bsg->ctl_points = (int *)bu_calloc(bsg->degree+1, sizeof(int), "sketch: bsg->ctl_points");
    bsg->ctl_points[0] = 4;
    bsg->ctl_points[1] = 7;
    bsg->ctl_points[2] = 9;
    bsg->ctl_points[3] = 8;
    bsg->ctl_points[4] = 0;
    skt.skt_curve.segments[0] = (genptr_t)bsg;

    BU_GETSTRUCT(lsg, line_seg);
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 0;
    lsg->end = 1;
    skt.skt_curve.segments[1] = (genptr_t)lsg;
    
    BU_GETSTRUCT(lsg, line_seg);
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 1;
    lsg->end = 2;
    skt.skt_curve.segments[2] = (genptr_t)lsg;
    
    BU_GETSTRUCT(lsg, line_seg);
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 2;
    lsg->end = 3;
    skt.skt_curve.segments[3] = (genptr_t)lsg;
    
    BU_GETSTRUCT(lsg, line_seg);
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = 3;
    lsg->end = 4;
    skt.skt_curve.segments[4] = (genptr_t)lsg;
    
    BU_GETSTRUCT(csg, carc_seg);
    csg->magic = CURVE_CARC_MAGIC;
    csg->radius = -1.0;
    csg->start = 6;
    csg->end = 5;
    skt.skt_curve.segments[5] = (genptr_t)csg;
    
    tmp_internal.idb_ptr = (genptr_t)&skt;
    tmp_internal.idb_meth = &rt_functab[ID_SKETCH];
    write_out(outfp, &tmp_internal, "sketch", &tol);

    /* !!! clean up calloc memory  */

    /* !!! wot? */
    mk_sketch(outfp, "sketch", &skt);

    bu_log("EXTRUDE\n");
    // extrude will need its own sketch
    struct rt_sketch_internal eskt;
    struct bezier_seg *ebsg;
    struct line_seg *elsg;
    struct carc_seg *ecsg;
    point2d_t everts[] = {
        { 250, 0 },     // 0 
	{ 500, 0 },     // 1 
	{ 500, 500 },   // 2 
	{ 0, 500 },     // 3 
	{ 0, 250 },     // 4 
	{ 250, 250 },   // 5 
	{ 125, 125 },   // 6 
	{ 0, 125 },     // 7 
	{ 125, 0 },     // 8 
	{ 200, 200 }    // 9 
    };
    eskt.magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(eskt.V, 10.0, 20.0, 30.0);
    VSET(eskt.u_vec, 1.0, 0.0, 0.0);
    VSET(eskt.v_vec, 0.0, 1.0, 0.0);
    eskt.vert_count = 10;
    eskt.verts = (point2d_t *)bu_calloc(eskt.vert_count, sizeof(point2d_t), "verts");
    for (cnti=0; cnti < eskt.vert_count; cnti++) {
	V2MOVE(eskt.verts[cnti], everts[cnti]);
    }
    eskt.skt_curve.seg_count = 6;
    eskt.skt_curve.reverse = (int *)bu_calloc(eskt.skt_curve.seg_count, sizeof(int), "sketch: reverse");
    eskt.skt_curve.segments = (genptr_t *)bu_calloc(eskt.skt_curve.seg_count, sizeof(genptr_t), "segs");

    BU_GETSTRUCT(ebsg, bezier_seg);
    ebsg->magic = CURVE_BEZIER_MAGIC;
    ebsg->degree = 4;
    ebsg->ctl_points = (int *)bu_calloc(bsg->degree+1, sizeof(int), "sketch: bsg->ctl_points");
    ebsg->ctl_points[0] = 4;
    ebsg->ctl_points[1] = 7;
    ebsg->ctl_points[2] = 9;
    ebsg->ctl_points[3] = 8;
    ebsg->ctl_points[4] = 0;
    eskt.skt_curve.segments[0] = (genptr_t)ebsg;
    
    BU_GETSTRUCT(elsg, line_seg);
    elsg->magic = CURVE_LSEG_MAGIC;
    elsg->start = 0;
    elsg->end = 1;
    eskt.skt_curve.segments[1] = (genptr_t)elsg;
    
    BU_GETSTRUCT(elsg, line_seg);
    elsg->magic = CURVE_LSEG_MAGIC;
    elsg->start = 1;
    elsg->end = 2;
    eskt.skt_curve.segments[2] = (genptr_t)elsg;
    
    BU_GETSTRUCT(elsg, line_seg);
    elsg->magic = CURVE_LSEG_MAGIC;
    elsg->start = 2;
    elsg->end = 3;
    eskt.skt_curve.segments[3] = (genptr_t)elsg;
    
    BU_GETSTRUCT(elsg, line_seg);
    elsg->magic = CURVE_LSEG_MAGIC;
    elsg->start = 3;
    elsg->end = 4;
    eskt.skt_curve.segments[4] = (genptr_t)elsg;
    
    BU_GETSTRUCT(ecsg, carc_seg);
    ecsg->magic = CURVE_CARC_MAGIC;
    ecsg->radius = -1.0;
    ecsg->start = 6;
    ecsg->end = 5;
    eskt.skt_curve.segments[5] = (genptr_t)ecsg;

    // now to the actual extrusion 
    struct rt_extrude_internal extrude;
    extrude.magic = RT_EXTRUDE_INTERNAL_MAGIC;
    VSET(extrude.V, 0.0, 0.0, 0.0);
    VSET(extrude.h, 0.0, 0.0, 1000.0);
    VSET(extrude.u_vec, 1.0, 0.0, 0.0);
    VSET(extrude.v_vec, 0.0, 1.0, 0.0);
    const char* esketch_name = "esketch.brep";
    extrude.sketch_name = bu_strdup(esketch_name);
    extrude.skt = &eskt;
    tmp_internal.idb_ptr = (genptr_t)&extrude;
    tmp_internal.idb_meth = &rt_functab[ID_EXTRUDE];
    write_out(outfp, &tmp_internal, "extrude", &tol);
 
    bu_log("REVOLVE\n");
    // revolve will need its own sketch
    struct rt_sketch_internal rskt;
    struct bezier_seg *rbsg;
    struct line_seg *rlsg;
    struct carc_seg *rcsg;
    point2d_t rverts[] = {
        { 250, 0 },     // 0 
	{ 500, 0 },     // 1 
	{ 500, 500 },   // 2 
	{ 0, 500 },     // 3 
	{ 0, 250 },     // 4 
	{ 250, 250 },   // 5 
	{ 125, 125 },   // 6 
	{ 0, 125 },     // 7 
	{ 125, 0 },     // 8 
	{ 200, 200 }    // 9 
    };

    rskt.magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(rskt.V, 10.0, 20.0, 30.0);
    VSET(rskt.u_vec, 1.0, 0.0, 0.0);
    VSET(rskt.v_vec, 0.0, 1.0, 0.0);
    rskt.vert_count = 10;
    rskt.verts = (point2d_t *)bu_calloc(rskt.vert_count, sizeof(point2d_t), "verts");
    for (cnti=0; cnti < rskt.vert_count; cnti++) {
	V2MOVE(rskt.verts[cnti], rverts[cnti]);
    }
    rskt.skt_curve.seg_count = 6;
    rskt.skt_curve.reverse = (int *)bu_calloc(rskt.skt_curve.seg_count, sizeof(int), "sketch: reverse");
    rskt.skt_curve.segments = (genptr_t *)bu_calloc(rskt.skt_curve.seg_count, sizeof(genptr_t), "segs");

    BU_GETSTRUCT(rbsg, bezier_seg);
    rbsg->magic = CURVE_BEZIER_MAGIC;
    rbsg->degree = 4;
    rbsg->ctl_points = (int *)bu_calloc(rbsg->degree+1, sizeof(int), "sketch: bsg->ctl_points");
    rbsg->ctl_points[0] = 4;
    rbsg->ctl_points[1] = 7;
    rbsg->ctl_points[2] = 9;
    rbsg->ctl_points[3] = 8;
    rbsg->ctl_points[4] = 0;
    rskt.skt_curve.segments[0] = (genptr_t)rbsg;
    
    BU_GETSTRUCT(rlsg, line_seg);
    rlsg->magic = CURVE_LSEG_MAGIC;
    rlsg->start = 0;
    rlsg->end = 1;    
    rskt.skt_curve.segments[1] = (genptr_t)rlsg;
    
    BU_GETSTRUCT(rlsg, line_seg);
    rlsg->magic = CURVE_LSEG_MAGIC;
    rlsg->start = 1;
    rlsg->end = 2;    
    rskt.skt_curve.segments[2] = (genptr_t)rlsg;
    
    BU_GETSTRUCT(rlsg, line_seg);
    rlsg->magic = CURVE_LSEG_MAGIC;
    rlsg->start = 2;
    rlsg->end = 3;
    rskt.skt_curve.segments[3] = (genptr_t)rlsg;
    
    BU_GETSTRUCT(rlsg, line_seg);
    rlsg->magic = CURVE_LSEG_MAGIC;
    rlsg->start = 3;
    rlsg->end = 4;    
    rskt.skt_curve.segments[4] = (genptr_t)rlsg;
    
    BU_GETSTRUCT(rcsg, carc_seg);
    rcsg->magic = CURVE_CARC_MAGIC;
    rcsg->radius = -1.0;
    rcsg->start = 6;
    rcsg->end = 5;    
    rskt.skt_curve.segments[5] = (genptr_t)rcsg;

    // now to the actual revolve 
    struct rt_revolve_internal revolve;
    revolve.magic = RT_REVOLVE_INTERNAL_MAGIC;
    VSET(revolve.v3d, -2000.0, 0.0, 0.0);
    VSET(revolve.axis3d, 0.0, 1.0, 0.0);
    revolve.sk = &rskt;
    tmp_internal.idb_ptr = (genptr_t)&revolve;
    tmp_internal.idb_meth = &rt_functab[ID_REVOLVE];
    write_out(outfp, &tmp_internal, "revolve", &tol);
 
    bu_log("DSP\n");
    struct rt_dsp_internal dsp;
    dsp.magic = RT_DSP_INTERNAL_MAGIC;
    bu_vls_init(&(dsp.dsp_name));
    bu_vls_printf(&(dsp.dsp_name), "data.dsp");
    dsp.dsp_xcnt = 256;
    dsp.dsp_ycnt = 256;
    dsp.dsp_smooth = 1;
    dsp.dsp_cuttype = 'a';
    dsp.dsp_datasrc = RT_DSP_SRC_FILE;
    MAT_IDN(dsp.dsp_mtos);
    MAT_IDN(dsp.dsp_stom);
    tmp_internal.idb_ptr = (genptr_t)&dsp;
    tmp_internal.idb_meth = &rt_functab[ID_DSP];
    write_out(outfp, &tmp_internal, "dsp", &tol);
    bu_vls_free(&dsp.dsp_name);

    /* clean up */
    wdb_close(outfp);

    ON::End();

    return 0;
}

#else /* !OBJ_BREP */

int
main(int argc, char *argv[])
{
    bu_log("ERROR: Boundary Representation object support is not available with\n"
	   "       this compilation of BRL-CAD.\n");
    return 1;
}

#endif /* OBJ_BREP */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
