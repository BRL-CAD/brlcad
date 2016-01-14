/**
 * Store an NMG model as a separate .g file, for later examination.
 * Don't free the model, as the caller may still have uses for it.
 *
 * NON-PARALLEL because of rt_uniresource.
 */
void
nmg_stash_model_to_file(const char *filename, const struct model *m, const char *title)
{
    struct rt_wdb *fp;
    struct rt_db_internal intern;
    struct bu_external ext;
    int ret;
    int flags;
    char *name="error.s";

    bu_log("nmg_stash_model_to_file('%s', %p, %s)\n", filename, (void *)m, title);

    NMG_CK_MODEL(m);
    nmg_vmodel(m);

    if ((fp = wdb_fopen(filename)) == NULL) {
	perror(filename);
	return;
    }

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)m;

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
nmg_to_arb(const struct model *m, struct rt_arb_internal *arb_int)
{
    struct nmgregion *r;
    struct shell *s;
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

    NMG_CK_MODEL(m);

    r = BU_LIST_FIRST(nmgregion, &m->r_hd);

    /* must be a single region */
    if (BU_LIST_NEXT_NOT_HEAD(&r->l, &m->r_hd))
	return 0;

    s = BU_LIST_FIRST(shell, &r->s_hd);
    NMG_CK_SHELL(s);

    /* must be a single shell */
    if (BU_LIST_NEXT_NOT_HEAD(&s->l, &r->s_hd))
	return 0;

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
    const struct model *m,
    struct rt_tgc_internal *tgc_int,
    const struct bn_tol *tol)
{
    struct nmgregion *r;
    struct shell *s;
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

    NMG_CK_MODEL(m);

    BN_CK_TOL(tol);

    r = BU_LIST_FIRST(nmgregion, &m->r_hd);

    /* must be a single region */
    if (BU_LIST_NEXT_NOT_HEAD(&r->l, &m->r_hd))
	return 0;

    s = BU_LIST_FIRST(shell, &r->s_hd);
    NMG_CK_SHELL(s);

    /* must be a single shell */
    if (BU_LIST_NEXT_NOT_HEAD(&s->l, &r->s_hd))
	return 0;

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
nmg_to_poly(const struct model *m, struct rt_pg_internal *poly_int, const struct bn_tol *tol)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct model *dup_m;
    struct nmgregion *dup_r;
    struct shell *dup_s;
    int max_count;
    int count_npts;
    int face_count=0;

    NMG_CK_MODEL(m);

    BN_CK_TOL(tol);

    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    if (nmg_check_closed_shell(s, tol))
		return 0;
	}
    }

    dup_m = nmg_mm();
    dup_r = nmg_mrsv(dup_m);
    dup_s = BU_LIST_FIRST(shell, &dup_r->s_hd);

    for (BU_LIST_FOR (r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	    for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
		if (fu->orientation != OT_SAME)
		    continue;
		(void)nmg_dup_face(fu, dup_s);
	    }
	}
    }

    for (BU_LIST_FOR (dup_r, nmgregion, &dup_m->r_hd)) {
	for (BU_LIST_FOR (dup_s, shell, &dup_r->s_hd)) {
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
	}
    }
    poly_int->npoly = face_count;
    poly_int->poly = (struct rt_pg_face_internal *)bu_calloc(face_count,
							     sizeof(struct rt_pg_face_internal), "nmg_to_poly: poly");

    face_count = 0;
    for (BU_LIST_FOR (dup_r, nmgregion, &dup_m->r_hd)) {
	for (BU_LIST_FOR (dup_s, shell, &dup_r->s_hd)) {
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
	}
    }

    poly_int->magic = RT_PG_INTERNAL_MAGIC;
    nmg_km(dup_m);

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
    nmg_vertex_tabulate(&nmg_vertices, &s->l.magic);

    /* and a list of all the faces */
    nmg_face_tabulate(&nmg_faces, &s->l.magic);

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


/* XXX move to ??? Everything from here on down needs to go into another module */


/**
 * Called from db_walk_tree() each time a tree leaf is encountered.
 * The primitive solid, in external format, is provided in 'ep', and
 * the type of that solid (e.g. ID_ELL) is in 'id'.  The full tree
 * state including the accumulated transformation matrix and the
 * current tolerancing is in 'tsp', and the full path from root to
 * leaf is in 'pathp'.
 *
 * Import the solid, tessellate it into an NMG, stash a pointer to the
 * tessellation in a new tree structure (union), and return a pointer
 * to that.
 *
 * Usually given as an argument to, and called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
nmg_booltree_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *UNUSED(client_data))
{
    struct model *m;
    struct nmgregion *r1 = (struct nmgregion *)NULL;
    union tree *curtree;
    struct directory *dp;

    if (!tsp || !pathp || !ip)
	return TREE_NULL;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (!ip->idb_meth || !ip->idb_meth->ft_tessellate) {
	bu_log("ERROR(%s): tessellation support not available\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_RESOURCE(tsp->ts_resp);

    m = nmg_mm();

    if (ip->idb_meth->ft_tessellate(&r1, m, ip, tsp->ts_ttol, tsp->ts_tol) < 0) {
	bu_log("ERROR(%s): tessellation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_REGION(r1);
    if (nmg_debug & DEBUG_VERIFY) {
	nmg_vshell(&r1->s_hd, r1);
    }

    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = r1;

    if (RT_G_DEBUG&DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
}


/**
 * Called from db_walk_tree() each time a tree leaf is encountered.
 * The primitive solid, in external format, is provided in 'ep', and
 * the type of that solid (e.g. ID_ELL) is in 'id'.  The full tree
 * state including the accumulated transformation matrix and the
 * current tolerancing is in 'tsp', and the full path from root to
 * leaf is in 'pathp'.
 *
 * Import the solid, convert it into an NMG using t-NURBS, stash a
 * pointer in a new tree structure (union), and return a pointer to
 * that.
 *
 * Usually given as an argument to, and called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
nmg_booltree_leaf_tnurb(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *UNUSED(client_data))
{
    struct nmgregion *r1;
    union tree *curtree;
    struct directory *dp;

    NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(tsp->ts_resp);

    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (ip->idb_meth->ft_tnurb(
	    &r1, *tsp->ts_m, ip, tsp->ts_tol) < 0) {
	bu_log("nmg_booltree_leaf_tnurb(%s): CSG to t-NURB conversation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_REGION(r1);
    if (nmg_debug & DEBUG_VERIFY) {
	nmg_vshell(&r1->s_hd, r1);
    }

    RT_GET_TREE(curtree, tsp->ts_resp);
    curtree->tr_op = OP_NMG_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = r1;

    if (RT_G_DEBUG&DEBUG_TREEWALK)
	bu_log("nmg_booltree_leaf_tnurb(%s) OK\n", dp->d_namep);

    return curtree;
}


/* quell the output of nmg_booltree_evaluate() to bu_log. */
int nmg_bool_eval_silent=0;

/**
 * Given a tree of leaf nodes tessellated earlier by
 * nmg_booltree_leaf_tess(), use recursion to do a depth-first
 * traversal of the tree, evaluating each pair of boolean operations
 * and reducing that result to a single nmgregion.
 *
 * Usually called from a do_region_end() handler from db_walk_tree().
 * For an example of several, see mged/dodraw.c.
 *
 * Returns an OP_NMG_TESS union tree node, which will contain the
 * resulting region and its name, as a dynamic string.  The caller is
 * responsible for releasing the string, and the node, by calling
 * db_free_tree() on the node.
 *
 * It is *essential* that the caller call nmg_model_fuse() before
 * calling this subroutine.
 *
 * Returns NULL if there is no geometry to return.
 *
 * Typical calls will be of this form:
 * (void)nmg_model_fuse(m, tol);
 * curtree = nmg_booltree_evaluate(curtree, tol);
 */
union tree *
nmg_booltree_evaluate(register union tree *tp, const struct bn_tol *tol, struct resource *resp)
{
    union tree *tl;
    union tree *tr;
    struct nmgregion *reg;
    int op = NMG_BOOL_ADD;      /* default value */
    const char *op_str = " u "; /* default value */
    size_t rem;
    char *name;

    RT_CK_TREE(tp);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return TREE_NULL;
	case OP_NMG_TESS:
	    /* Hit a tree leaf */
	    if (nmg_debug & DEBUG_VERIFY) {
		nmg_vshell(&tp->tr_d.td_r->s_hd, tp->tr_d.td_r);
	    }
	    return tp;
	case OP_UNION:
	    op = NMG_BOOL_ADD;
	    op_str = " u ";
	    break;
	case OP_INTERSECT:
	    op = NMG_BOOL_ISECT;
	    op_str = " + ";
	    break;
	case OP_SUBTRACT:
	    op = NMG_BOOL_SUB;
	    op_str = " - ";
	    break;
	default:
	    bu_bomb("nmg_booltree_evaluate(): bad op\n");
    }

    /* Handle a boolean operation node.  First get its leaves. */
    tl = nmg_booltree_evaluate(tp->tr_b.tb_left, tol, resp);
    tr = nmg_booltree_evaluate(tp->tr_b.tb_right, tol, resp);

    if (tl) {
	RT_CK_TREE(tl);
	if (tl != tp->tr_b.tb_left) {
	    bu_bomb("nmg_booltree_evaluate(): tl != tp->tr_b.tb_left\n");
	}
    }
    if (tr) {
	RT_CK_TREE(tr);
	if (tr != tp->tr_b.tb_right) {
	    bu_bomb("nmg_booltree_evaluate(): tr != tp->tr_b.tb_right\n");
	}
    }

    if (!tl && !tr) {
	/* left-r == null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	db_free_tree(tp->tr_b.tb_right, resp);
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

    if (tl && !tr) {
	/* left-r != null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_right, resp);
	if (op == NMG_BOOL_ISECT) {
	    /* OP_INTERSECT '+' */
	    RT_CK_TREE(tp);
	    db_free_tree(tl, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;
	} else {
	    /* copy everything from tl to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tl->tr_op;
	    tp->tr_b.tb_regionp = tl->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tl->tr_b.tb_left;
	    tp->tr_b.tb_right = tl->tr_b.tb_right;

	    /* null data from tl so only to free this node */
	    tl->tr_b.tb_regionp = (struct region *)NULL;
	    tl->tr_b.tb_left = TREE_NULL;
	    tl->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tl, resp);
	    return tp;
	}
    }

    if (!tl && tr) {
	/* left-r == null && right-r != null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	if (op == NMG_BOOL_ADD) {
	    /* OP_UNION 'u' */
	    /* copy everything from tr to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tr->tr_op;
	    tp->tr_b.tb_regionp = tr->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tr->tr_b.tb_left;
	    tp->tr_b.tb_right = tr->tr_b.tb_right;

	    /* null data from tr so only to free this node */
	    tr->tr_b.tb_regionp = (struct region *)NULL;
	    tr->tr_b.tb_left = TREE_NULL;
	    tr->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tr, resp);
	    return tp;

	} else if ((op == NMG_BOOL_SUB) || (op == NMG_BOOL_ISECT)) {
	    /* for sub and intersect, if left-hand-side is null, result is null */
	    RT_CK_TREE(tp);
	    db_free_tree(tr, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;

	} else {
	    bu_bomb("nmg_booltree_evaluate(): error, unknown operation\n");
	}
    }

    if (tl->tr_op != OP_NMG_TESS) {
	bu_bomb("nmg_booltree_evaluate(): bad left tree\n");
    }
    if (tr->tr_op != OP_NMG_TESS) {
	bu_bomb("nmg_booltree_evaluate(): bad right tree\n");
    }

    if (!nmg_bool_eval_silent) {
	bu_log(" {%s}%s{%s}\n", tl->tr_d.td_name, op_str, tr->tr_d.td_name);
    }

    NMG_CK_REGION(tr->tr_d.td_r);
    NMG_CK_REGION(tl->tr_d.td_r);

    if (nmg_ck_closed_region(tr->tr_d.td_r, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (r)\n");
    }
    if (nmg_ck_closed_region(tl->tr_d.td_r, tol)) {
	bu_bomb("nmg_booltree_evaluate(): ERROR, non-closed shell (l)\n");
    }

    nmg_r_radial_check(tr->tr_d.td_r, tol);
    nmg_r_radial_check(tl->tr_d.td_r, tol);

    if (nmg_debug & DEBUG_BOOL) {
	bu_log("Before model fuse\nShell A:\n");
	nmg_pr_s_briefly(BU_LIST_FIRST(shell, &tl->tr_d.td_r->s_hd), "");
	bu_log("Shell B:\n");
	nmg_pr_s_briefly(BU_LIST_FIRST(shell, &tr->tr_d.td_r->s_hd), "");
    }

    /* move operands into the same model */
    if (tr->tr_d.td_r->m_p != tl->tr_d.td_r->m_p) {
	nmg_merge_models(tl->tr_d.td_r->m_p, tr->tr_d.td_r->m_p);
    }

    /* input r1 and r2 are destroyed, output is new region */
    reg = nmg_do_bool(tl->tr_d.td_r, tr->tr_d.td_r, op, tol);

    /* build string of result name */
    rem = strlen(tl->tr_d.td_name) + 3 + strlen(tr->tr_d.td_name) + 2 + 1;
    name = (char *)bu_calloc(rem, sizeof(char), "nmg_booltree_evaluate name");
    snprintf(name, rem, "(%s%s%s)", tl->tr_d.td_name, op_str, tr->tr_d.td_name);

    /* clean up child tree nodes */
    tl->tr_d.td_r = (struct nmgregion *)NULL;
    tr->tr_d.td_r = (struct nmgregion *)NULL;
    db_free_tree(tl, resp);
    db_free_tree(tr, resp);


    if (reg) {
	/* convert argument binary node into a result node */
	NMG_CK_REGION(reg);
	nmg_r_radial_check(reg, tol);
	tp->tr_op = OP_NMG_TESS;
	tp->tr_d.td_r = reg;
	tp->tr_d.td_name = name;

	if (nmg_debug & DEBUG_VERIFY) {
	    nmg_vshell(&reg->s_hd, reg);
	}
	return tp;

    } else {
	/* resulting region was null */
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

}


/**
 * This is the main application interface to the NMG Boolean
 * Evaluator.
 *
 * This routine has the opportunity to do "once only" operations
 * before and after the boolean tree is walked.
 *
 * Returns -
 * 0 Boolean went OK.  Result region is in tp->tr_d.td_r
 * !0 Boolean produced null result.
 *
 * The caller is responsible for freeing 'tp' in either case,
 * typically with db_free_tree(tp);
 */
int
nmg_boolean(union tree *tp, struct model *m, const struct bn_tol *tol, struct resource *resp)
{
    union tree *result;
    int ret;

    RT_CK_TREE(tp);
    NMG_CK_MODEL(m);
    BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    if (nmg_debug & (DEBUG_BOOL|DEBUG_BASIC)) {
	bu_log("\n\nnmg_boolean(tp=%p, m=%p) START\n",
	       (void *)tp, (void *)m);
    }

    /* The nmg_model_fuse function was removed from this point in the
     * boolean process since not all geometry that is to be processed is
     * always within the single 'm' nmg model structure passed into this
     * function. In some cases the geometry resides in multiple nmg model
     * structures within the 'tp' tree that is passed into this function.
     * Running nmg_model_fuse is still required but is done later, i.e.
     * within the nmg_booltree_evaluate function just before the nmg_do_bool
     * function is called which is when the geometry, in which the boolean
     * to be performed, is always in a single nmg model structure.
     */

    /*
     * Evaluate the nodes of the boolean tree one at a time, until
     * only a single region remains.
     */
    result = nmg_booltree_evaluate(tp, tol, resp);

    if (result == TREE_NULL) {
	bu_log("nmg_boolean(): result of nmg_booltree_evaluate() is NULL\n");
	rt_pr_tree(tp, 0);
	ret = 1;
	goto out;
    }

    if (result != tp) {
	bu_bomb("nmg_boolean(): result of nmg_booltree_evaluate() isn't tp\n");
    }

    RT_CK_TREE(result);

    if (tp->tr_op != OP_NMG_TESS) {
	bu_log("nmg_boolean(): result of nmg_booltree_evaluate() op != OP_NMG_TESS\n");
	rt_pr_tree(tp, 0);
	ret = 1;
	goto out;
    }

    if (tp->tr_d.td_r == (struct nmgregion *)NULL) {
	/* Pointers are all OK, but boolean produced null set */
	ret = 1;
	goto out;
    }

    /* move result into correct model */
    nmg_merge_models(m, tp->tr_d.td_r->m_p);
    ret = 0;

out:
    if (nmg_debug & (DEBUG_BOOL|DEBUG_BASIC)) {
	bu_log("nmg_boolean(tp=%p, m=%p) END, ret=%d\n\n",
	       (void *)tp, (void *)m, ret);
    }

    return ret;
}


/** END BOOLEAN **/


/**
 * Construct a nice ray for is->pt, is->dir
 * which contains the line of intersection, is->on_eg.
 *
 * See the comment in nmg_isect_two_generics_faces() for details
 * on the constraints on this ray, and the algorithm.
 *
 * XXX Danger?
 * The ray -vs- RPP check is being done in 3D.
 * It really ought to be done in 2D, to ensure that
 * long edge lines on nearly axis-aligned faces don't
 * get discarded prematurely!
 * XXX Can't just comment out the code, I think the selection
 * XXX of is->pt is significant:
 * 1) All intersections are at positive distances on the ray,
 * 2) dir cross N will point "left".
 *
 * Returns -
 * 0 OK
 * 1 ray misses fu2 bounding box
 */
int
nmg_isect_construct_nice_ray(struct nmg_inter_struct *is, struct faceuse *fu2)
{
    struct xray line;
    vect_t invdir;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu2);

    VMOVE(line.r_pt, is->on_eg->e_pt);			/* 3D line */
    VMOVE(line.r_dir, is->on_eg->e_dir);
    VUNITIZE(line.r_dir);
    VINVDIR(invdir, line.r_dir);

    /* nmg_loop_g() makes sure there are no 0-thickness faces */
    if (!rt_in_rpp(&line, invdir, fu2->f_p->min_pt, fu2->f_p->max_pt)) {
	/* The edge ray missed the face RPP, nothing to do. */
	if (nmg_debug & DEBUG_POLYSECT) {
	    VPRINT("r_pt ", line.r_pt);
	    VPRINT("r_dir", line.r_dir);
	    VPRINT("fu2 min", fu2->f_p->min_pt);
	    VPRINT("fu2 max", fu2->f_p->max_pt);
	    bu_log("r_min=%g, r_max=%g\n", line.r_min, line.r_max);
	    bu_log("nmg_isect_construct_nice_ray() edge ray missed face bounding RPP, ret=1\n");
	}
	return 1;	/* Missed */
    }
    if (nmg_debug & DEBUG_POLYSECT) {
	VPRINT("fu2 min", fu2->f_p->min_pt);
	VPRINT("fu2 max", fu2->f_p->max_pt);
	bu_log("r_min=%g, r_max=%g\n", line.r_min, line.r_max);
    }
    /* Start point will lie at min or max dist, outside of face RPP */
    VJOIN1(is->pt, line.r_pt, line.r_min, line.r_dir);
    if (line.r_min > line.r_max) {
	/* Direction is heading the wrong way, flip it */
	VREVERSE(is->dir, line.r_dir);
	if (nmg_debug & DEBUG_POLYSECT)
	    bu_log("flipping dir\n");
    } else {
	VMOVE(is->dir, line.r_dir);
    }
    if (nmg_debug & DEBUG_POLYSECT) {
	VPRINT("r_pt ", line.r_pt);
	VPRINT("r_dir", line.r_dir);
	VPRINT("->pt ", is->pt);
	VPRINT("->dir", is->dir);
	bu_log("nmg_isect_construct_nice_ray() ret=0\n");
    }
    return 0;
}


/**
 * This is intended as a general user interface routine.
 * Given the Cartesian coordinates for a point in space,
 * return the classification for that point with respect to a shell.
 *
 * The algorithm used is to fire a ray from the point, and count
 * the number of times it crosses a face.
 *
 * The flag "in_or_out_only" specifies that the point is known to not
 * be on the shell, therefore only returns of NMG_CLASS_AinB or
 * NMG_CLASS_AoutB are acceptable.
 *
 * The point is "A", and the face is "B".
 *
 * Returns -
 * NMG_CLASS_AinB pt is INSIDE the volume of the shell.
 * NMG_CLASS_AonBshared pt is ON the shell boundary.
 * NMG_CLASS_AoutB pt is OUTSIDE the volume of the shell.
 */
int
nmg_class_pt_s(const fastf_t *pt, const struct shell *s, const int in_or_out_only, const struct bn_tol *tol)
{
    const struct faceuse *fu;
    struct model *m;
    long *faces_seen = NULL;
    vect_t region_diagonal;
    fastf_t region_diameter;
    int nmg_class = 0;
    vect_t projection_dir = VINIT_ZERO;
    int tries = 0;
    struct xray rp;
    fastf_t model_bb_max_width;
    point_t m_min_pt, m_max_pt; /* nmg model min and max points */

    m = s->r_p->m_p;
    NMG_CK_MODEL(m);
    NMG_CK_SHELL(s);
    BN_CK_TOL(tol);

    if (nmg_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_class_pt_s(): pt=(%g, %g, %g), s=%p\n",
	       V3ARGS(pt), (void *)s);
    }
    if (V3PT_OUT_RPP_TOL(pt, s->sa_p->min_pt, s->sa_p->max_pt, tol->dist)) {
	if (nmg_debug & DEBUG_CLASSIFY) {
	    bu_log("nmg_class_pt_s(): OUT, point not in RPP\n");
	}
	return NMG_CLASS_AoutB;
    }

    if (!in_or_out_only) {
	faces_seen = (long *)bu_calloc(m->maxindex, sizeof(long), "nmg_class_pt_s faces_seen[]");
	/*
	 * First pass:  Try hard to see if point is ON a face.
	 */
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    plane_t n;

	    /* If this face processed before, skip on */
	    if (NMG_INDEX_TEST(faces_seen, fu->f_p)) {
		continue;
	    }
	    /* Only consider the outward pointing faceuses */
	    if (fu->orientation != OT_SAME) {
		continue;
	    }

	    /* See if this point lies on this face */
	    NMG_GET_FU_PLANE(n, fu);
	    if ((fabs(DIST_PT_PLANE(pt, n))) < tol->dist) {
		/* Point lies on this plane, it may be possible to
		 * short circuit everything.
		 */
		nmg_class = nmg_class_pt_fu_except(pt, fu, (struct loopuse *)0,
						   (void (*)(struct edgeuse *, point_t, const char *))NULL,
						   (void (*)(struct vertexuse *, point_t, const char *))NULL,
						   (const char *)NULL, 0, 0, tol);
		if (nmg_class == NMG_CLASS_AonBshared) {
		    bu_bomb("nmg_class_pt_s(): function nmg_class_pt_fu_except returned AonBshared when it can only return AonBanti\n");
		}
		if (nmg_class == NMG_CLASS_AonBshared) {
		    /* Point is ON face, therefore it must be
		     * ON the shell also.
		     */
		    nmg_class = NMG_CLASS_AonBshared;
		    goto out;
		}
		if (nmg_class == NMG_CLASS_AonBanti) {
		    /* Point is ON face, therefore it must be
		     * ON the shell also.
		     */
		    nmg_class = NMG_CLASS_AonBanti;
		    goto out;
		}

		if (nmg_class == NMG_CLASS_AinB) {
		    /* Point is IN face, therefor it must be
		     * ON the shell also.
		     */
		    nmg_class = NMG_CLASS_AonBanti;
		    goto out;
		}
		/* Point is OUTside face, it's undecided. */
	    }

	    /* Mark this face as having been processed */
	    NMG_INDEX_SET(faces_seen, fu->f_p);
	}
    }

    /* If we got here, the point isn't ON any of the faces.
     * Time to do the Jordan Curve Theorem.  We fire an arbitrary
     * ray and count the number of crossings (in the positive direction)
     * If that number is even, we're outside the shell, otherwise we're
     * inside the shell.
     */
    NMG_CK_REGION_A(s->r_p->ra_p);
    VSUB2(region_diagonal, s->r_p->ra_p->max_pt, s->r_p->ra_p->min_pt);
    region_diameter = MAGNITUDE(region_diagonal);

    nmg_model_bb(m_min_pt, m_max_pt, m);
    model_bb_max_width = bn_dist_pt3_pt3(m_min_pt, m_max_pt);

    /* Choose an unlikely direction */
    tries = 0;
retry:
    if (tries < MAX_DIR_TRYS) {
	projection_dir[X] = nmg_good_dirs[tries][X];
	projection_dir[Y] = nmg_good_dirs[tries][Y];
	projection_dir[Z] = nmg_good_dirs[tries][Z];
    }

    if (++tries >= MAX_DIR_TRYS) {
	goto out; /* only nmg_good_dirs to try */
    }

    VUNITIZE(projection_dir);

    if (nmg_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_class_pt_s(): Pt=(%g, %g, %g) dir=(%g, %g, %g), reg_diam=%g\n",
	       V3ARGS(pt), V3ARGS(projection_dir), region_diameter);
    }

    VMOVE(rp.r_pt, pt);

    /* give the ray a length which is at least the max
     * length of the nmg model bounding box.
     */
    VSCALE(rp.r_dir, projection_dir, model_bb_max_width * 1.25);

    /* get NMG ray-tracer to tell us if start point is inside or outside */
    nmg_class = nmg_class_ray_vs_shell(&rp, s, in_or_out_only, tol);

    if (nmg_class == NMG_CLASS_AonBshared) {
	bu_bomb("nmg_class_pt_s(): function nmg_class_ray_vs_shell returned AonBshared when it can only return AonBanti\n");
    }
    if (nmg_class == NMG_CLASS_Unknown) {
	goto retry;
    }

out:
    if (!in_or_out_only) {
	bu_free((char *)faces_seen, "nmg_class_pt_s faces_seen[]");
    }

    if (nmg_debug & DEBUG_CLASSIFY) {
	bu_log("nmg_class_pt_s(): returning %s, s=%p, tries=%d\n",
	       nmg_class_name(nmg_class), (void *)s, tries);
    }

    return nmg_class;
}
