/*                      A S C _ V 4 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file asc_v4.cpp
 *
 * Brief description
 *
 */

#include "common.h"
#include "vmath.h"

#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include "bu/units.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "wdb.h"
#include "gcv/api.h"
#include "gcv/util.h"

/* maximum input line buffer size */
#define BUFSIZE (16*1024)
#define SIZE (128*1024*1024)
#define TYPE_LEN 200
#define NAME_LEN 200


char *ibuf = NULL;
/* GED database record */
static union record irecord;
FILE *iifp = NULL;
struct rt_wdb *iofp = NULL;

/* Record input buffer */
char NAME[NAME_LEN + 2] = {0};

static int ars_ncurves = 0;
static int ars_ptspercurve = 0;
static int ars_curve = 0;
static int ars_pt = 0;
static char *ars_name = NULL;
static fastf_t **ars_curves = NULL;

static int linecnt = 0;

char *
nxt_spc(char *cp)
{
    while (*cp != ' ' && *cp != '\t' && *cp !='\0') {
	cp++;
    }
    if (*cp != '\0') {
	cp++;
    }
    return cp;
}


int
incr_ars_pnt(void)
{
    int ret=0;

    ars_pt++;
    if (ars_pt >= ars_ptspercurve) {
	ars_curve++;
	ars_pt = 0;
	ret = 1;
    }

    if (ars_curve >= ars_ncurves)
	return 2;

    return ret;
}


/**
 * This routine removes newline and carriage return characters from
 * the ibuffer and substitutes in NULL.
 */
void
zap_nl(void)
{
    char *bp;

    bp = &ibuf[0];

    while (*bp != '\0') {
	if ((*bp == '\n') || (*bp == '\r')) {
	    *bp = '\0';
	}
	bp++;
    }
}


/**
 * Input format is:
 *	s type name args...\n
 *
 * Individual processing is needed for each 'type' of solid, to hand
 * it off to the appropriate LIBWDB routine.
 */
void
strsolbld(void)
{
    const struct rt_functab *ftp;
    const char delim[]     = " ";
    const char end_delim[] = "\n";
    char *type    = NULL;
    char *name    = NULL;
    char *args    = NULL;
#if defined(HAVE_WORKING_STRTOK_R_FUNCTION)
    char *saveptr = NULL;
#endif
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *ibuf2 = (char *)bu_malloc(sizeof(char) * BUFSIZE, "strsolbld temporary ibuffer");
    char *ibufp = ibuf2;

    memcpy(ibuf2, ibuf, sizeof(char) * BUFSIZE);

#if defined(HAVE_WORKING_STRTOK_R_FUNCTION)
    /* this function is reentrant */
    (void)strtok_r(ibuf2, delim, &saveptr);  /* skip stringsolid_id */
    type = strtok_r(NULL, delim, &saveptr);
    name = strtok_r(NULL, delim, &saveptr);
    args = strtok_r(NULL, end_delim, &saveptr);
#else
    (void)strtok(ibuf2, delim);		/* skip stringsolid_id */
    type = strtok(NULL, delim);
    name = strtok(NULL, delim);
    args = strtok(NULL, end_delim);
#endif

    if (BU_STR_EQUAL(type, "dsp")) {
	struct rt_dsp_internal *dsp;

	BU_ALLOC(dsp, struct rt_dsp_internal);
	bu_vls_init(&dsp->dsp_name);
	bu_vls_strcpy(&str, args);
	if (bu_struct_parse(&str, OBJ[ID_DSP].ft_parsetab, (char *)dsp, NULL) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    ftp = rt_get_functab_by_label("dsp");
	    if (ftp && ftp->ft_ifree)
		ftp->ft_ifree((struct rt_db_internal *)dsp);
	    goto out;
	}
	dsp->magic = RT_DSP_INTERNAL_MAGIC;
	if (wdb_export(iofp, name, (void *)dsp, ID_DSP, mk_conv2mm) < 0) {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'dsp' has already been freed by wdb_export() */
    } else if (BU_STR_EQUAL(type, "ebm")) {
	struct rt_ebm_internal *ebm;

	BU_ALLOC(ebm, struct rt_ebm_internal);

	MAT_IDN(ebm->mat);

	bu_vls_strcpy(&str, args);
	if (bu_struct_parse(&str, OBJ[ID_EBM].ft_parsetab, (char *)ebm, NULL) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    ftp = rt_get_functab_by_label("ebm");
	    if (ftp && ftp->ft_ifree)
		ftp->ft_ifree((struct rt_db_internal *)ebm);
	    return;
	}
	ebm->magic = RT_EBM_INTERNAL_MAGIC;
	if (wdb_export(iofp, name, (void *)ebm, ID_EBM, mk_conv2mm) < 0) {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'ebm' has already been freed by wdb_export() */
    } else if (BU_STR_EQUAL(type, "vol")) {
	struct rt_vol_internal *vol;

	BU_ALLOC(vol, struct rt_vol_internal);
	MAT_IDN(vol->mat);

	bu_vls_strcpy(&str, args);
	if (bu_struct_parse(&str, OBJ[ID_VOL].ft_parsetab, (char *)vol, NULL) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    ftp = rt_get_functab_by_label("vol");
	    if (ftp && ftp->ft_ifree)
		ftp->ft_ifree((struct rt_db_internal *)vol);
	    return;
	}
	vol->magic = RT_VOL_INTERNAL_MAGIC;
	if (wdb_export(iofp, name, (void *)vol, ID_VOL, mk_conv2mm) < 0) {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'vol' has already been freed by wdb_export() */
    } else {
	bu_log("strsolbld(%s): unable to convert '%s' type solid, skipping\n",
	       name, type);
    }

out:
    bu_free(ibufp, "strsolbld temporary ibuffer");
    bu_vls_free(&str);
}


#define LSEG 'L'
#define CARC 'A'
#define NURB 'N'

void
sktbld(void)
{
    char *cp, *ptr;
    size_t i, j;
    unsigned long vert_count, seg_count;
    float fV[3], fu[3], fv[3];
    point_t V;
    vect_t u, v;
    point2d_t *verts;
    char name[NAME_LEN+1];
    struct rt_sketch_internal *skt;
    struct rt_curve *crv;
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct nurb_seg *nsg;

    cp = ibuf;

    cp++;
    cp++;

    sscanf(cp, "%200s %f %f %f %f %f %f %f %f %f %lu %lu", /* NAME_LEN */
	   name,
	   &fV[0], &fV[1], &fV[2],
	   &fu[0], &fu[1], &fu[2],
	   &fv[0], &fv[1], &fv[2],
	   &vert_count, &seg_count);

    VMOVE(V, fV);
    VMOVE(u, fu);
    VMOVE(v, fv);

    if (bu_fgets(ibuf, BUFSIZE, iifp) == (char *)0)
	bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);

    verts = (point2d_t *)bu_calloc(vert_count, sizeof(point2d_t), "verts");
    ptr = strtok(ibuf, " ");
    if (!ptr)
	bu_exit(1, "ERROR: no vertices for sketch (%s)\n", name);
    for (i=0; i<vert_count; i++) {
	verts[i][0] = atof(ptr);
	ptr = strtok((char *)NULL, " ");
	if (!ptr)
	    bu_exit(1, "ERROR: not enough vertices for sketch (%s)\n", name);
	verts[i][1] = atof(ptr);
	ptr = strtok((char *)NULL, " ");
	if (!ptr && i < vert_count-1)
	    bu_exit(1, "ERROR: not enough vertices for sketch (%s)\n", name);
    }

    BU_ALLOC(skt, struct rt_sketch_internal);
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VMOVE(skt->V, V);
    VMOVE(skt->u_vec, u);
    VMOVE(skt->v_vec, v);
    skt->vert_count = vert_count;
    skt->verts = verts;
    crv = &skt->curve;
    crv->count = seg_count;

    crv->segment = (void **)bu_calloc(crv->count, sizeof(void *), "segments");
    crv->reverse = (int *)bu_calloc(crv->count, sizeof(int), "reverse");
    for (j=0; j<crv->count; j++) {
	double radius;
	int k;

	if (bu_fgets(ibuf, BUFSIZE, iifp) == (char *)0)
	    bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);

	cp = ibuf + 2;
	switch (*cp) {
	    case LSEG:
		BU_ALLOC(lsg, struct line_seg);
		sscanf(cp+1, "%d %d %d", &crv->reverse[j], &lsg->start, &lsg->end);
		lsg->magic = CURVE_LSEG_MAGIC;
		crv->segment[j] = lsg;
		break;
	    case CARC:
		BU_ALLOC(csg, struct carc_seg);
		sscanf(cp+1, "%d %d %d %lf %d %d", &crv->reverse[j], &csg->start, &csg->end,
		       &radius, &csg->center_is_left, &csg->orientation);
		csg->radius = radius;
		csg->magic = CURVE_CARC_MAGIC;
		crv->segment[j] = csg;
		break;
	    case NURB:
		BU_ALLOC(nsg, struct nurb_seg);
		sscanf(cp+1, "%d %d %d %d %d", &crv->reverse[j], &nsg->order, &nsg->pt_type,
		       &nsg->k.k_size, &nsg->c_size);
		nsg->k.knots = (fastf_t *)bu_calloc(nsg->k.k_size, sizeof(fastf_t), "knots");
		nsg->ctl_points = (int *)bu_calloc(nsg->c_size, sizeof(int), "control points");
		if (bu_fgets(ibuf, BUFSIZE, iifp) == (char *)0)
		    bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);
		cp = ibuf + 3;
		ptr = strtok(cp, " ");
		if (!ptr)
		    bu_exit(1, "ERROR: not enough knots for nurb segment in sketch (%s)\n", name);
		for (k=0; k<nsg->k.k_size; k++) {
		    nsg->k.knots[k] = atof(ptr);
		    ptr = strtok((char *)NULL, " ");
		    if (!ptr && k<nsg->k.k_size-1)
			bu_exit(1, "ERROR: not enough knots for nurb segment in sketch (%s)\n", name);
		}
		if (bu_fgets(ibuf, BUFSIZE, iifp) == (char *)0)
		    bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);
		cp = ibuf + 3;
		ptr = strtok(cp, " ");
		if (!ptr)
		    bu_exit(1, "ERROR: not enough control points for nurb segment in sketch (%s)\n", name);
		for (k=0; k<nsg->c_size; k++) {
		    nsg->ctl_points[k] = atoi(ptr);
		    ptr = strtok((char *)NULL, " ");
		    if (!ptr && k<nsg->c_size-1)
			bu_exit(1, "ERROR: not enough control points for nurb segment in sketch (%s)\n", name);
		}
		nsg->magic = CURVE_NURB_MAGIC;
		crv->segment[j] = nsg;
		break;
	    default:
		bu_exit(1, "Unrecognized segment type (%c) in sketch (%s)\n", *cp, name);
	}

    }

    (void)mk_sketch(iofp, name,  skt);
}


void
extrbld(void)
{
    char *cp;
    char name[NAME_LEN+1];
    char sketch_name[NAME_LEN+1];
    int keypoint;
    float fV[3];
    float fh[3];
    float fu_vec[3], fv_vec[3];
    point_t V;
    vect_t h, u_vec, v_vec;

    cp = ibuf;

    cp++;

    cp++;
    sscanf(cp, "%200s %200s %d %f %f %f  %f %f %f %f %f %f %f %f %f", /* NAME_LEN */
	   name, sketch_name, &keypoint, &fV[0], &fV[1], &fV[2], &fh[0], &fh[1], &fh[2],
	   &fu_vec[0], &fu_vec[1], &fu_vec[2], &fv_vec[0], &fv_vec[1], &fv_vec[2]);

    VMOVE(V, fV);
    VMOVE(h, fh);
    VMOVE(u_vec, fu_vec);
    VMOVE(v_vec, fv_vec);
    (void)mk_extrusion(iofp, name, sketch_name, V, h, u_vec, v_vec, keypoint);
}


/**
 * For the time being, what we read in from the ascii form is a hex
 * dump of the on-disk form of NMG.  This is the same between v4 and
 * v5.  Reassemble it in v5 binary form here, then import it, then
 * re-export it.  This extra step is necessary because we don't know
 * what version database the output it, LIBWDB is only interested in
 * writing in-memory versions.
 */
void
nmgbld(void)
{
    char *cp;
    int version;
    char *name;
    long granules;
    long struct_count[26];
    struct bu_external ext;
    struct rt_db_internal intern;
    int j;

    /* First, process the header line */
    strtok(ibuf, " ");
    /* This is nmg_id, unused here. */
    cp = strtok(NULL, " ");
    version = atoi(cp);
    cp = strtok(NULL, " ");
    name = bu_strdup(cp);
    cp = strtok(NULL, " ");
    granules = atol(cp);

    /* Allocate storage for external v5 form of the body */
    BU_EXTERNAL_INIT(&ext);
    ext.ext_nbytes = SIZEOF_NETWORK_LONG + 26*SIZEOF_NETWORK_LONG + 128 * granules;
    ext.ext_buf = (uint8_t *)bu_malloc(ext.ext_nbytes, "nmg ext_buf");
    *(uint32_t *)ext.ext_buf = htonl(version);
    BU_ASSERT(version == 1);	/* DISK_MODEL_VERSION */

    /* Get next line of input with the 26 counts on it */
    if (bu_fgets(ibuf, BUFSIZE, iifp) == (char *)0)
	bu_exit(-1, "Unexpected EOF while reading NMG %s data, line 2\n", name);

    /* Second, process counts for each kind of structure */
    cp = strtok(ibuf, " ");
    for (j=0; j<26; j++) {
	struct_count[j] = atol(cp);
	*(uint32_t *)(ext.ext_buf + SIZEOF_NETWORK_LONG*(j+1)) = htonl(struct_count[j]);
	cp = strtok((char *)NULL, " ");
    }

    /* Remaining lines have 32 bytes per line, in hex */
    /* There are 4 lines to make up one granule */
    cp = ((char *)ext.ext_buf) + (26+1)*SIZEOF_NETWORK_LONG;
    for (j=0; j < granules * 4; j++) {
	int k;
	unsigned int cp_i;

	if (bu_fgets(ibuf, BUFSIZE, iifp) == (char *)0)
	    bu_exit(-1, "Unexpected EOF while reading NMG %s data, hex line %d\n", name, j);

	for (k=0; k<32; k++) {
	    sscanf(&ibuf[k*2], "%2x", &cp_i);
	    *cp++ = cp_i;
	}
    }

    /* Next, import this disk record into memory */
    RT_DB_INTERNAL_INIT(&intern);
    if (OBJ[ID_NMG].ft_import5(&intern, &ext, bn_mat_identity, iofp->dbip, &rt_uniresource) < 0)
	bu_exit(-1, "ft_import5 failed on NMG %s\n", name);
    bu_free_external(&ext);

    /* Now we should have a good NMG in memory */
    nmg_vmodel((struct model *)intern.idb_ptr);

    /* Finally, squirt it back out through LIBWDB */
    mk_nmg(iofp, name, (struct model *)intern.idb_ptr);
    /* mk_nmg() frees the intern.idp_ptr pointer */
    RT_DB_INTERNAL_INIT(&intern);

    bu_free(name, "name");
}


/**
 * This routine parses a solid record and determines which libwdb
 * routine to call to replicate this solid.  Simple primitives are
 * expected.
 */
void
solbld(void)
{
    char *cp;
    char *np;
    int i;

    char s_type;		/* id for the type of primitive */
    fastf_t val[24];		/* array of values/parameters for solid */
    point_t center;		/* center; used by many solids */
    point_t pnts[9];		/* array of points for the arbs */
    point_t norm;
    vect_t a, b, c, d, n;	/* various vectors required */
    vect_t height;		/* height vector for tgc */
    vect_t breadth;		/* breadth vector for rpc */
    double dd, rad1, rad2;

    cp = ibuf;
    cp++;			/* ident */
    cp = nxt_spc(cp);		/* skip the space */
    s_type = atoi(cp);

    cp = nxt_spc(cp);

    np = NAME;
    while (*cp != ' ') {
	*np++ = *cp++;
    }
    *np = '\0';

    cp = nxt_spc(cp);
    /* Comgeom solid type */

    for (i = 0; i < 24; i++) {
	cp = nxt_spc(cp);
	val[i] = atof(cp);
    }

    /* Switch on the record type to make the solids. */

    switch (s_type) {

	case GRP:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(n, val[3], val[4], val[5]);
	    (void)mk_grip(iofp, NAME, center, n, val[6]);
	    break;

	case TOR:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(n, val[3], val[4], val[5]);
	    rad1 = MAGNITUDE(&val[6]);
	    rad2 = MAGNITUDE(n);
	    VUNITIZE(n);

	    /* Prevent illegal torii from floating point fuzz */
	    V_MIN(rad2, rad1);

	    mk_tor(iofp, NAME, center, n, rad1, rad2);
	    break;

	case GENTGC:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    VSET(b, val[9], val[10], val[11]);
	    VSET(c, val[12], val[13], val[14]);
	    VSET(d, val[15], val[16], val[17]);

	    mk_tgc(iofp, NAME, center, height, a, b, c, d);
	    break;

	case GENELL:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(a, val[3], val[4], val[5]);
	    VSET(b, val[6], val[7], val[8]);
	    VSET(c, val[9], val[10], val[11]);

	    mk_ell(iofp, NAME, center, a, b, c);
	    break;

	case GENARB8:
	    VSET(pnts[0], val[0], val[1], val[2]);
	    VSET(pnts[1], val[3], val[4], val[5]);
	    VSET(pnts[2], val[6], val[7], val[8]);
	    VSET(pnts[3], val[9], val[10], val[11]);
	    VSET(pnts[4], val[12], val[13], val[14]);
	    VSET(pnts[5], val[15], val[16], val[17]);
	    VSET(pnts[6], val[18], val[19], val[20]);
	    VSET(pnts[7], val[21], val[22], val[23]);

	    /* Convert from vector notation to absolute points */
	    for (i=1; i<8; i++) {
		VADD2(pnts[i], pnts[i], pnts[0]);
	    }

	    mk_arb8(iofp, NAME, &pnts[0][X]);
	    break;

	case HALFSPACE:
	    VSET(norm, val[0], val[1], val[2]);
	    dd = val[3];

	    mk_half(iofp, NAME, norm, dd);
	    break;

	case RPC:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(breadth, val[6], val[7], val[8]);
	    dd = val[9];

	    mk_rpc(iofp, NAME, center, height, breadth, dd);
	    break;

	case RHC:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(breadth, val[6], val[7], val[8]);
	    rad1 = val[9];
	    dd = val[10];

	    mk_rhc(iofp, NAME, center, height, breadth, rad1, dd);
	    break;

	case EPA:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    VUNITIZE(a);
	    rad1 = val[9];
	    rad2 = val[10];

	    mk_epa(iofp, NAME, center, height, a, rad1, rad2);
	    break;

	case EHY:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    VUNITIZE(a);
	    rad1 = val[9];
	    rad2 = val[10];
	    dd = val[11];

	    mk_ehy(iofp, NAME, center, height, a, rad1, rad2, dd);
	    break;

	case HYP:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    rad1 = val[9];
	    rad2 = val[10];

	    mk_hyp(iofp, NAME, center, height, a, rad1, rad2);
	    break;

	case ETO:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(norm, val[3], val[4], val[5]);
	    VSET(c, val[6], val[7], val[8]);
	    rad1 = val[9];
	    rad2 = val[10];

	    mk_eto(iofp, NAME, center, norm, c, rad1, rad2);
	    break;

	default:
	    bu_log("asc2g: bad solid %s s_type= %d, skipping\n",
		   NAME, s_type);
    }

}


/**
 * This routine invokes libwdb to build a member of a combination.
 * Called only from combbld()
 */
void
membbld(struct bu_list *headp)
{
    char *cp;
    char *np;
    int i;
    char relation;	/* boolean operation */
    char inst_name[NAME_LEN+2];
    struct wmember *memb;

    cp = ibuf;
    cp++;			/* ident */
    cp = nxt_spc(cp);		/* skip the space */

    relation = *cp++;
    cp = nxt_spc(cp);

    np = inst_name;
    while (*cp != ' ') {
	*np++ = *cp++;
    }
    *np = '\0';

    cp = nxt_spc(cp);

    memb = mk_addmember(inst_name, headp, NULL, relation);

    for (i = 0; i < 16; i++) {
	memb->wm_mat[i] = atof(cp);
	cp = nxt_spc(cp);
    }
}


/**
 * This routine builds combinations.  It does so by processing the "C"
 * combination input line, (which may be followed by optional material
 * properties lines), and it then slurps up any following "M" member
 * lines, building up a linked list of all members.  Reading continues
 * until a non-"M" record is encountered.
 *
 * Returns -
 * 0 OK
 * 1 OK, another record exists in global input line ibuffer.
 */
int
combbld(void)
{
    struct bu_list head;
    char *cp = NULL;
    char *np = NULL;
    /* indicators for optional fields */
    int temp_nflag = 0;
    int temp_pflag = 0;

    char override = 0;
    char reg_flags;	/* region flag */
    int is_reg;
    short regionid;
    short aircode;
    short material;	/* GIFT material code */
    short los;		/* LOS estimate */
    unsigned char rgb[3];		/* Red, green, blue values */
    char matname[32];	/* String of material name */
    char matparm[60];	/* String of material parameters */
    char inherit;	/* Inheritance property */

    /* Set all flags initially. */
    BU_LIST_INIT(&head);

    cp = ibuf;
    cp++;			/* ID_COMB */
    cp = nxt_spc(cp);		/* skip the space */

    reg_flags = *cp++;		/* Y, N, or new P, F */
    cp = nxt_spc(cp);

    np = NAME;
    while (*cp != ' ') {
	*np++ = *cp++;
    }
    *np = '\0';

    cp = nxt_spc(cp);

    regionid = (short)atoi(cp);
    cp = nxt_spc(cp);
    aircode = (short)atoi(cp);
    cp = nxt_spc(cp);
    /* DEPRECATED: number of members expected */
    cp = nxt_spc(cp);
    /* DEPRECATED: Comgeom reference number */
    cp = nxt_spc(cp);
    material = (short)atoi(cp);
    cp = nxt_spc(cp);
    los = (short)atoi(cp);
    cp = nxt_spc(cp);
    override = (char)atoi(cp);
    cp = nxt_spc(cp);

    rgb[0] = (unsigned char)atoi(cp);
    cp = nxt_spc(cp);
    rgb[1] = (unsigned char)atoi(cp);
    cp = nxt_spc(cp);
    rgb[2] = (unsigned char)atoi(cp);
    cp = nxt_spc(cp);

    temp_nflag = atoi(cp);
    cp = nxt_spc(cp);
    temp_pflag = atoi(cp);

    cp = nxt_spc(cp);
    inherit = atoi(cp);

    /* To support FASTGEN, different kinds of regions now exist. */
    switch (reg_flags) {
	case 'Y':
	case 'R':
	    is_reg = DBV4_REGION;
	    break;
	case 'P':
	    is_reg = DBV4_REGION_FASTGEN_PLATE;
	    break;
	case 'V':
	    is_reg = DBV4_REGION_FASTGEN_VOLUME;
	    break;
	case 'N':
	default:
	    is_reg = 0;
    }

    if (temp_nflag) {
	bu_fgets(ibuf, BUFSIZE, iifp);
	zap_nl();
	memset(matname, 0, sizeof(matname));
	bu_strlcpy(matname, ibuf, sizeof(matname));
    }
    if (temp_pflag) {
	bu_fgets(ibuf, BUFSIZE, iifp);
	zap_nl();
	memset(matparm, 0, sizeof(matparm));
	bu_strlcpy(matparm, ibuf, sizeof(matparm));
    }

    for (;;) {
	ibuf[0] = '\0';
	if (bu_fgets(ibuf, BUFSIZE, iifp) == (char *)0)
	    break;

	if (ibuf[0] != ID_MEMB) break;

	/* Process (and accumulate) the members */
	membbld(&head);
    }

    /* Spit them out, all at once.  Use GIFT semantics. */
    if (mk_comb(iofp, NAME, &head, is_reg,
		temp_nflag ? matname : (char *)0,
		temp_pflag ? matparm : (char *)0,
		override ? (unsigned char *)rgb : (unsigned char *)0,
		regionid, aircode, material, los, inherit, 0, 1) < 0) {
	bu_exit(1, "asc2g: mk_lrcomb fail\n");
    }

    if (ibuf[0] == '\0') return 0;
    return 1;
}


/**
 * This routine builds ARS's.
 */
void
arsabld(void)
{
    char *cp;
    char *np;
    int i;

    if (ars_name)
	bu_free((char *)ars_name, "ars_name");
    cp = ibuf;
    cp = nxt_spc(cp);
    cp = nxt_spc(cp);

    np = cp;
    while (*(++cp) != ' ');
    *cp++ = '\0';
    ars_name = bu_strdup(np);
    ars_ncurves = (short)atoi(cp);
    cp = nxt_spc(cp);
    ars_ptspercurve = (short)atoi(cp);

    ars_curves = (fastf_t **)bu_calloc((ars_ncurves+1), sizeof(fastf_t *), "ars_curves");
    for (i=0; i<ars_ncurves; i++) {
	ars_curves[i] = (fastf_t *)bu_calloc(ars_ptspercurve + 1,
					     sizeof(fastf_t) * ELEMENTS_PER_VECT, "ars_curve");
    }

    ars_pt = 0;
    ars_curve = 0;
}


/**
 * This is the second half of the ARS-building.  It builds the ARS B
 * record.
 */
void
arsbbld(void)
{
    char *cp;
    int i;
    int incr_ret;

    cp = ibuf;
    cp = nxt_spc(cp);		/* skip the space */
    cp = nxt_spc(cp);
    cp = nxt_spc(cp);
    for (i = 0; i < 8; i++) {
	cp = nxt_spc(cp);
	ars_curves[ars_curve][ars_pt*3] = atof(cp);
	cp = nxt_spc(cp);
	ars_curves[ars_curve][ars_pt*3 + 1] = atof(cp);
	cp = nxt_spc(cp);
	ars_curves[ars_curve][ars_pt*3 + 2] = atof(cp);
	if (ars_curve > 0 || ars_pt > 0)
	    VADD2(&ars_curves[ars_curve][ars_pt*3], &ars_curves[ars_curve][ars_pt*3], &ars_curves[0][0]);

	incr_ret = incr_ars_pnt();
	if (incr_ret == 2) {
	    /* finished, write out the ARS solid */
	    if (mk_ars(iofp, ars_name, ars_ncurves, ars_ptspercurve, ars_curves)) {
		bu_exit(1, "Failed trying to make ARS (%s)\n", ars_name);
	    }
	    return;
	} else if (incr_ret == 1) {
	    /* end of curve, ignore remainder of record */
	    return;
	}
    }
}


/**
 * This routine makes an ident record.  It calls libwdb to do this.
 */
void
identbld(void)
{
    char *cp;
    char *np;
    char units;		/* units code number */
    char version[6] = {0};
    char title[255] = {0};
    char unit_str[8] = {0};
    double local2mm;

    bu_strlcpy(unit_str, "none", sizeof(unit_str));

    cp = ibuf;
    cp++;			/* ident */
    cp = nxt_spc(cp);		/* skip the space */

    units = (char)atoi(cp);
    cp = nxt_spc(cp);

    /* Note that there is no provision for handing libwdb the version.
     * However, this is automatically provided when needed.
     */

    np = version;
    while (*cp != '\n' && *cp != '\r' && *cp != '\0') {
	*np++ = *cp++;
    }
    *np = '\0';

    if (!BU_STR_EQUAL(version, ID_VERSION)) {
	bu_log("WARNING:  input file version (%s) is not %s\n",
	       version, ID_VERSION);
    }

    (void)bu_fgets(ibuf, BUFSIZE, iifp);
    zap_nl();
    bu_strlcpy(title, ibuf, sizeof(title));

    /* FIXME: Should use db_conversions() for this */
    switch (units) {
	case ID_NO_UNIT:
	    bu_strlcpy(unit_str, "mm", sizeof(unit_str));
	    break;
	case ID_MM_UNIT:
	    bu_strlcpy(unit_str, "mm", sizeof(unit_str));
	    break;
	case ID_UM_UNIT:
	    bu_strlcpy(unit_str, "um", sizeof(unit_str));
	    break;
	case ID_CM_UNIT:
	    bu_strlcpy(unit_str, "cm", sizeof(unit_str));
	    break;
	case ID_M_UNIT:
	    bu_strlcpy(unit_str, "m", sizeof(unit_str));
	    break;
	case ID_KM_UNIT:
	    bu_strlcpy(unit_str, "km", sizeof(unit_str));
	    break;
	case ID_IN_UNIT:

	    bu_strlcpy(unit_str, "in", sizeof(unit_str));
	    break;
	case ID_FT_UNIT:
	    bu_strlcpy(unit_str, "ft", sizeof(unit_str));
	    break;
	case ID_YD_UNIT:
	    bu_strlcpy(unit_str, "yard", sizeof(unit_str));
	    break;
	case ID_MI_UNIT:
	    bu_strlcpy(unit_str, "mile", sizeof(unit_str));
	    break;
	default:
	    fprintf(stderr, "asc2g: unknown v4 units code = %d, defaulting to millimeters\n", units);
	    bu_strlcpy(unit_str, "mm", sizeof(unit_str));
    }
    local2mm = bu_units_conversion(unit_str);
    if (local2mm <= 0) {
	fprintf(stderr, "asc2g: unable to convert v4 units string '%s', got local2mm=%g\n",
		unit_str, local2mm);
	bu_exit(3, NULL);
    }

    if (mk_id_editunits(iofp, title, local2mm) < 0)
	bu_exit(2, "asc2g: unable to write database ID\n");
}


/**
 * Collect up all the information for a POLY-solid.  These are handled
 * as BoT solids in v5, but we still have to read the data in the old
 * format, and then convert it.
 *
 * The poly header line is followed by an unknown number of poly data
 * lines.
 */
void
polyhbld(void)
{
    char *cp;
    char *name;
    b_off_t startpos;
    size_t nlines;
    struct rt_pg_internal *pg;
    struct rt_db_internal intern;
    struct bn_tol tol;

    (void)strtok(ibuf, " ");	/* skip the ident character */
    cp = strtok(NULL, " \n");
    name = bu_strdup(cp);

    /* Count up the number of poly data lines which follow */
    startpos = bu_ftell(iifp);
    for (nlines = 0;; nlines++) {
	if (bu_fgets(ibuf, BUFSIZE, iifp) == NULL) break;
	if (ibuf[0] != ID_P_DATA) break;	/* 'Q' */
    }
    BU_ASSERT(nlines > 0);

    /* Allocate storage for the faces */
    BU_ALLOC(pg, struct rt_pg_internal);
    pg->magic = RT_PG_INTERNAL_MAGIC;
    pg->npoly = nlines;
    pg->poly = (struct rt_pg_face_internal *)bu_calloc(pg->npoly,
						       sizeof(struct rt_pg_face_internal), "poly[]");
    pg->max_npts = 0;

    /* Return to first 'Q' record */
    bu_fseek(iifp, startpos, 0);

    for (nlines = 0; nlines < pg->npoly; nlines++) {
	struct rt_pg_face_internal *fp = &pg->poly[nlines];
	int i;

	if (bu_fgets(ibuf, BUFSIZE, iifp) == NULL) break;
	if (ibuf[0] != ID_P_DATA) bu_exit(1, "mis-count of Q records?\n");

	/* Input always has 5 points, even if all aren't significant */
	fp->verts = (fastf_t *)bu_malloc(5*3*sizeof(fastf_t), "verts[]");
	fp->norms = (fastf_t *)bu_malloc(5*3*sizeof(fastf_t), "norms[]");

	cp = ibuf;
	cp++;				/* ident */
	cp = nxt_spc(cp);		/* skip the space */

	fp->npts = (char)atoi(cp);
	CLAMP(pg->max_npts, fp->npts, pg->npoly * 3);

	for (i = 0; i < 5*3; i++) {
	    cp = nxt_spc(cp);
	    fp->verts[i] = atof(cp);
	}

	for (i = 0; i < 5*3; i++) {
	    cp = nxt_spc(cp);
	    fp->norms[i] = atof(cp);
	}
    }

    /* Convert the polysolid to a BoT */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_POLY;
    intern.idb_meth = &OBJ[ID_POLY];
    intern.idb_ptr = pg;

    /* this tolerance structure is only used for converting polysolids
     * to BOT's use zero distance to avoid losing any polysolid facets
     */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    if (rt_pg_to_bot(&intern, &tol, &rt_uniresource) < 0)
	bu_exit(1, "Failed to convert [%s] polysolid object to triangle mesh\n", name);
    /* The polysolid is freed by the converter */

    /* Since we already have an internal form, this is much simpler
     * than calling mk_bot().
     */
    if (wdb_put_internal(iofp, name, &intern, mk_conv2mm) < 0)
	bu_exit(1, "Failed to create [%s] triangle mesh representation from polysolid object\n", name);
    /* BoT internal has been freed */
}


/**
 * Add information to the region-id based coloring table.
 */
void
materbld(void)
{
    char *cp;
    int low, hi;
    int r, g, b;

    cp = ibuf;
    cp++;			/* skip ID_MATERIAL */
    cp = nxt_spc(cp);		/* skip the space */

    cp = nxt_spc(cp);
    low = (short)atoi(cp);
    cp = nxt_spc(cp);
    hi = (short)atoi(cp);
    cp = nxt_spc(cp);
    r = (unsigned char)atoi(cp);
    cp = nxt_spc(cp);
    g = (unsigned char)atoi(cp);
    cp = nxt_spc(cp);
    b = (unsigned char)atoi(cp);

    /* Put it on a linked list for output later */
    rt_color_addrec(low, hi, r, g, b, -1L);
}


void
clinebld(void)
{
    char my_name[NAME_LEN];
    fastf_t thickness;
    fastf_t radius;
    point_t V;
    vect_t height;
    char *cp;
    char *np;

    cp = ibuf;
    cp++;
    cp = nxt_spc(cp);

    np = my_name;
    while (*cp != ' ' && *cp != '\n' && *cp != '\r') {
	*np++ = *cp++;
    }
    *np = '\0';

    cp = nxt_spc(cp);
    V[0] = atof(cp);
    cp = nxt_spc(cp);
    V[1] = atof(cp);
    cp = nxt_spc(cp);
    V[2] = atof(cp);
    cp = nxt_spc(cp);
    height[0] = atof(cp);
    cp = nxt_spc(cp);
    height[1] = atof(cp);
    cp = nxt_spc(cp);
    height[2] = atof(cp);
    cp = nxt_spc(cp);
    radius = atof(cp);
    cp = nxt_spc(cp);
    thickness = atof(cp);
    mk_cline(iofp, my_name, V, height, radius, thickness);
}


void
botbld(void)
{
    char my_name[NAME_LEN];
    char type;
    int mode, orientation, error_mode;
    unsigned long int num_vertices, num_faces;
    unsigned long int i, j;
    double a[3];
    fastf_t *vertices;
    fastf_t *thick=NULL;
    int *faces;
    struct bu_bitv *facemode=NULL;

    sscanf(ibuf, "%c %200s %d %d %d %lu %lu", &type, my_name, &mode, &orientation, /* NAME_LEN */
	   &error_mode, &num_vertices, &num_faces);

    /* get vertices */
    vertices = (fastf_t *)bu_calloc(num_vertices * 3, sizeof(fastf_t), "botbld: vertices");
    for (i=0; i<num_vertices; i++) {
	bu_fgets(ibuf, BUFSIZE, iifp);
	sscanf(ibuf, "%lu: %le %le %le", &j, &a[0], &a[1], &a[2]);
	if (i != j) {
	    bu_log("Vertices out of order in solid %s (expecting %lu, found %lu)\n",
		   my_name, i, j);
	    bu_free((char *)vertices, "botbld: vertices");
	    bu_log("Skipping this solid!\n");
	    while (ibuf[0] == '\t')
		bu_fgets(ibuf, BUFSIZE, iifp);
	    return;
	}
	VMOVE(&vertices[i*3], a);
    }

    /* get faces (and possibly thicknesses */
    faces = (int *)bu_calloc(num_faces * 3, sizeof(int), "botbld: faces");
    if (mode == RT_BOT_PLATE)
	thick = (fastf_t *)bu_calloc(num_faces, sizeof(fastf_t), "botbld thick");
    for (i=0; i<num_faces; i++) {
	bu_fgets(ibuf, BUFSIZE, iifp);
	if (mode == RT_BOT_PLATE)
	    sscanf(ibuf, "%lu: %d %d %d %le", &j, &faces[i*3], &faces[i*3+1], &faces[i*3+2], &a[0]);
	else
	    sscanf(ibuf, "%lu: %d %d %d", &j, &faces[i*3], &faces[i*3+1], &faces[i*3+2]);

	if (i != j) {
	    bu_log("Faces out of order in solid %s (expecting %lu, found %lu)\n",
		   my_name, i, j);
	    bu_free((char *)vertices, "botbld: vertices");
	    bu_free((char *)faces, "botbld: faces");
	    if (mode == RT_BOT_PLATE)
		bu_free((char *)thick, "botbld thick");
	    bu_log("Skipping this solid!\n");
	    while (ibuf[0] == '\t')
		bu_fgets(ibuf, BUFSIZE, iifp);
	    return;
	}

	if (mode == RT_BOT_PLATE)
	    thick[i] = a[0];
    }

    if (mode == RT_BOT_PLATE) {
	/* get bit vector */
	bu_fgets(ibuf, BUFSIZE, iifp);
	facemode = bu_hex_to_bitv(&ibuf[1]);
    }

    mk_bot(iofp, my_name, mode, orientation, 0, num_vertices, num_faces,
	   vertices, faces, thick, facemode);

    bu_free((char *)vertices, "botbld: vertices");
    bu_free((char *)faces, "botbld: faces");
    if (mode == RT_BOT_PLATE) {
	bu_free((char *)thick, "botbld thick");
	bu_free((char *)facemode, "botbld facemode");
    }
}


/**
 * This routine reads pipe data from standard in, constructs a
 * doubly linked list of pipe points, and sends this list to
 * mk_pipe().
 */
void
pipebld(void)
{

    char name[NAME_LEN];
    char *cp;
    char *np;
    struct wdb_pipe_pnt *sp;
    struct bu_list head;

    /* Process the first ibuffer */

    cp = ibuf;
    cp++;			/* ident, not used later */
    cp = nxt_spc(cp);		/* skip spaces */

    np = name;
    while (*cp != '\n' && *cp != '\r' && *cp != '\0') {
	*np++ = *cp++;
    }
    *np = '\0';			/* null terminate the string */


    /* Read data lines and process */

    BU_LIST_INIT(&head);
    bu_fgets(ibuf, BUFSIZE, iifp);
    while (bu_strncmp (ibuf, "END_PIPE", 8)) {
	double id, od, x, y, z, bendradius;

	BU_ALLOC(sp, struct wdb_pipe_pnt);

	sscanf(ibuf, "%le %le %le %le %le %le",
	       &id, &od,
	       &bendradius, &x, &y, &z);

	sp->l.magic = WDB_PIPESEG_MAGIC;

	sp->pp_id = id;
	sp->pp_od = od;
	sp->pp_bendradius = bendradius;
	VSET(sp->pp_coord, x, y, z);

	BU_LIST_INSERT(&head, &sp->l);
	bu_fgets(ibuf, BUFSIZE, iifp);
    }

    mk_pipe(iofp, name, &head);
    mk_pipe_free(&head);
}


/**
 * This routine reads particle data from standard in, and constructs
 * the parameters required by mk_particle.
 */
void
particlebld(void)
{

    char name[NAME_LEN];
    char ident;
    point_t vertex;
    vect_t height;
    double vrad;
    double hrad;
    double scanvertex[3];
    double scanheight[3];


    /* Read all the information out of the existing ibuffer.  Note that
     * particles fit into one granule.
     */

    sscanf(ibuf, "%c %200s %le %le %le %le %le %le %le %le", /* NAME_LEN */
	   &ident, name,
	   &scanvertex[0],
	   &scanvertex[1],
	   &scanvertex[2],
	   &scanheight[0],
	   &scanheight[1],
	   &scanheight[2],
	   &vrad, &hrad);
    /* convert double to fastf_t */
    VMOVE(vertex, scanvertex);
    VMOVE(height, scanheight);

    mk_particle(iofp, name, vertex, height, vrad, hrad);
}


/**
 * This routine reads arbn data from standard in and sends it to
 * mk_arbn().
 */
void
arbnbld(void)
{

    char name[NAME_LEN] = {0};
    char type[TYPE_LEN] = {0};
    int i;
    int neqn;     /* number of eqn expected */
    plane_t *eqn; /* pointer to plane equations for faces */
    char *cp;
    char *np;

    /* Process the first ibuffer */

    cp = ibuf;
    cp++;				/* ident */
    cp = nxt_spc(cp);			/* skip spaces */

    np = name;
    while (*cp != ' ') {
	*np++ = *cp++;
    }
    *np = '\0';				/* null terminate the string */

    cp = nxt_spc(cp);

    neqn = atoi(cp);			/* find number of eqns */

    /* Check to make sure plane equations actually came in. */
    if (neqn <= 0) {
	bu_log("asc2g: warning: %d equations counted for arbn %s\n", neqn, name);
    }

    /* Malloc space for the in-coming plane equations */
    eqn = (plane_t *)bu_malloc(neqn * sizeof(plane_t), "eqn");

    /* Now, read the plane equations and put in appropriate place */

    for (i = 0; i < neqn; i++) {
	double scan[4];

	bu_fgets(ibuf, BUFSIZE, iifp);
	sscanf(ibuf, "%200s %le %le %le %le", type, /* TYPE_LEN */
	       &scan[0], &scan[1], &scan[2], &scan[3]);
	/* convert double to fastf_t */
	HMOVE(eqn[i], scan);
    }

    mk_arbn(iofp, name, neqn, (const plane_t *)eqn);

    bu_free(eqn, "eqn");
}


/**
 * This routine checks the last character in the string to see if it matches the
 * specified character. Used by gettclblock() to check for an escaped return.
 *
 */
int
endswith(char *line, char ch)
{
    if (*(line+strlen(line)-1) == ch) {
	return 1;
    }
    return 0;
}
/**
 * This routine counts the number of open braces and is used to determine whether a Tcl
 * command is complete.
 *
 */
int
bracecnt(char *line)
{
    char *start;
    int cnt = 0;

    start = line;
    while (*start != '\0') {
	if (*start == '{') {
	    cnt++;
	} else if (*start == '}') {
	    cnt--;
	}
	start++;
    }
    return cnt;
}
/**
 * This routine reads the next block of Tcl commands. This block is expected to be a Tcl
 * command script and will be fed to an interpreter using Tcl_Eval(). Any escaped returns
 * or open braces are parsed through and concatenated ensuring Tcl commands are complete.
 *
 * SIZE is used as the approximate blocking size allowing to grow past this to close the
 * command line.
 */
int
gettclblock(struct bu_vls *line, FILE *fp)
{
    int ret = 0;
    struct bu_vls tmp = BU_VLS_INIT_ZERO;

    if ((ret=bu_vls_gets(line, fp)) >= 0) {
	int bcnt = 0;
	int escapedcr = 0;

	linecnt++;
	escapedcr = endswith(bu_vls_addr(line), '\\');
	bcnt = bracecnt(bu_vls_addr(line));
	while ((ret >= 0) && ((bu_vls_strlen(line) < SIZE) || (escapedcr) || (bcnt != 0))) {
	    linecnt++;
	    if (escapedcr) {
		bu_vls_trunc(line, bu_vls_strlen(line)-1);
	    }
	    if ((ret=bu_vls_gets(&tmp, fp)) > 0) {
		escapedcr = endswith(bu_vls_addr(&tmp), '\\');
		bcnt = bcnt + bracecnt(bu_vls_addr(&tmp));
		bu_vls_putc(line, '\n');
		bu_vls_strcat(line, bu_vls_addr(&tmp));
		bu_vls_trunc(&tmp, 0);
	    } else {
		escapedcr = 0;
	    }
	}
	ret = (int)bu_vls_strlen(line);
    }
    bu_vls_free(&tmp);

    return ret;
}

/*******************************************************************/
/*                     Main asc V4 read entry point                */
/*******************************************************************/
int
asc_read_v4(
	struct gcv_context *UNUSED(c),
       	const struct gcv_opts *UNUSED(o),
	std::ifstream &fs
	)
{
    std::string sline;
    bu_log("Reading v4...\n");
    while (std::getline(fs, sline)) {
	std::cout << sline << "\n";
    }

    /* allocate our input buffer */
    ibuf = (char *)bu_calloc(sizeof(char), BUFSIZE, "input buffer");

    /* Read ASCII input file, each record on a line */
    while ((bu_fgets(ibuf, BUFSIZE, iifp)) != (char *)0) {

    after_read:
	/* Clear the output record -- vital! */
	(void)memset((char *)&irecord, 0, sizeof(record));

	/* Check record type */
	switch (ibuf[0]) {
	    case ID_SOLID:
		solbld();
		continue;

	    case ID_COMB:
		if (combbld() > 0) goto after_read;
		continue;

	    case ID_MEMB:
		bu_log("Warning: unattached Member record, ignored\n");
		continue;

	    case ID_ARS_A:
		arsabld();
		continue;

	    case ID_ARS_B:
		arsbbld();
		continue;

	    case ID_P_HEAD:
		polyhbld();
		continue;

	    case ID_P_DATA:
		bu_log("Unattached POLY-solid P_DATA (Q) record, skipping\n");
		continue;

	    case ID_IDENT:
		identbld();
		continue;

	    case ID_MATERIAL:
		materbld();
		continue;

	    case ID_BSOLID:
		bu_log("WARNING: conversion support for old B-spline solid geometry is not supported.  Contact devs@brlcad.org if you need this feature.");
		continue;

	    case ID_BSURF:
		bu_log("WARNING: conversion support for old B-spline surface geometry is not supported.  contact devs@brlcad.org if you need this feature.");
		continue;

	    case DBID_PIPE:
		pipebld();
		continue;

	    case DBID_STRSOL:
		strsolbld();
		continue;

	    case DBID_NMG:
		nmgbld();
		continue;

	    case DBID_PARTICLE:
		particlebld();
		continue;

	    case DBID_ARBN:
		arbnbld();
		continue;

	    case DBID_CLINE:
		clinebld();
		continue;

	    case DBID_BOT:
		botbld();
		continue;

	    case DBID_EXTR:
		extrbld();
		continue;

	    case DBID_SKETCH:
		sktbld();
		continue;

	    case '#':
		continue;

	    default:
		bu_log("asc2g: bad record type '%c' (0%o), skipping\n", ibuf[0], ibuf[0]);
		bu_log("%s\n", ibuf);
		continue;
	}
    }

    /* Now, at the end of the database, dump out the entire
     * region-id-based color table.
     */
    mk_write_color_table(iofp);

    /* close up shop */
    bu_free(ibuf, "input buffer");
    ibuf = NULL; /* sanity */
    fclose(iifp); iifp = NULL;
    wdb_close(iofp); iofp = NULL;

    return 1;
}

static int     combdump(void);
static void    idendump(void), polyhead_asc(void), polydata_asc(void);
static void    soldump(void), extrdump(void), sketchdump(void);
static void    membdump(union record *rp), arsadump(void), arsbdump(void);
static void    materdump(void), bspldump(void), bsurfdump(void);
static void    pipe_dump(void), particle_dump(void), dump_pipe_segs(char *, struct bu_list *);
static void    arbn_dump(void), cline_dump(void), bot_dump(void);
static void    nmg_dump(void);
static void    strsol_dump(void);
static char *  strchop(char *str, size_t len);

const mat_t id_mat = MAT_INIT_IDN; /* identity matrix for pipes */
#define CH(x)	strchop(x, sizeof(x))
union record	record;		/* GED database record */
FILE	*ifp;
FILE	*ofp;

/*
 *  Take a database name and null-terminate it,
 *  converting unprintable characters to something printable.
 *  Here we deal with names not being null-terminated.
 */
static char *
encode_name(char *str)
{
    static char buf[NAMESIZE+1];
    char *ip = str;
    char *op = buf;
    int warn = 0;

    while (op < &buf[NAMESIZE]) {
	if (*ip == '\0')  break;
	if (isprint((int)*ip) && !isspace((int)*ip)) {
	    *op++ = *ip++;
	}  else  {
	    *op++ = '@';
	    ip++;
	    warn = 1;
	}
    }
    *op = '\0';
    if (warn) {
	fprintf(stderr,
		      "g2asc: Illegal char in object name, converted to '%s'\n",
		      buf);
    }
    if (op == buf) {
	/* Null input name */
	fprintf(stderr,
		      "g2asc:  NULL object name converted to -=NULL=-\n");
	
	// TODO
	// return "-=NULL=-";
	return NULL;
    }
    return buf;
}


/*
 *  Take "ngran" granules, and put them in memory.
 *  The first granule comes from the global extern "record",
 *  the remainder are read from ifp.
 */
static void
get_ext(struct bu_external *ep, size_t ngran)
{
    size_t count;

    BU_EXTERNAL_INIT(ep);

    ep->ext_nbytes = ngran * sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "get_ext ext_buf");

    /* Copy the freebie (first) record into the array of records.  */
    memcpy((char *)ep->ext_buf, (char *)&record, sizeof(union record));
    if (ngran <= 1)  return;

    count = fread(((char *)ep->ext_buf)+sizeof(union record),
		   sizeof(union record), ngran-1, ifp);
    if (count != (size_t)ngran-1) {
	fprintf(stderr,
		"g2asc: get_ext:  wanted to read %lu granules, got %lu\n",
		(unsigned long)ngran-1, (unsigned long)count);
	bu_exit(1, NULL);
    }
}

static void
nmg_dump(void)
{
    union record rec;
    long struct_count[26];
    size_t i, granules;
    size_t j, k;

    /* just in case someone changes the record size */
    if (sizeof(union record)%32)
    {
	fprintf(stderr, "g2asc: nmg_dump cannot work with records not multiple of 32\n");
	bu_exit(-1, NULL);
    }

    /* get number of granules needed for this NMG */
    granules = ntohl(*(uint32_t *)record.nmg.N_count);

    /* get the array of structure counts */
    for (j = 0; j < 26; j++)
	struct_count[j] = ntohl(*(uint32_t *)&record.nmg.N_structs[j*4]);

    /* output some header info */
    fprintf(ofp,  "%c %d %.16s %lu\n",
		  record.nmg.N_id,	/* N */
		  record.nmg.N_version,	/* NMG version */
		  record.nmg.N_name,	/* solid name */
		  (unsigned long)granules);		/* number of additional granules */

    /* output the structure counts */
    for (j = 0; j < 26; j++)
	fprintf(ofp,  " %ld", struct_count[j]);
    (void)fputc('\n', ofp);

    /* dump the reminder in hex format */
    for (i = 0; i < granules; i++) {
	char *cp;
	/* Read the record */
	if (!fread((char *)&rec, sizeof record, 1, ifp)) {
	    fprintf(stderr, "Error reading nmg granules\n");
	    bu_exit(-1, NULL);
	}
	cp = (char *)&rec;

	/* 32 bytes per line */
	for (k = 0; k < sizeof(union record)/32; k++) {
	    for (j = 0; j < 32; j++)
		fprintf(ofp,  "%02x", (0xff & (*cp++)));	 /* two hex digits per byte */
	    fputc('\n', ofp);
	}
    }
}

static void
strsol_dump(void)	/* print out strsol solid info */
{
    union record rec[DB_SS_NGRAN];
    char *cp;

    /* get all the strsol granules */
    rec[0] = record;	/* struct copy the current record */

    /* read the rest from ifp */
    if (!fread((char *)&rec[1], sizeof record, DB_SS_NGRAN-1, ifp))
    {
	fprintf(stderr, "Error reading strsol granules\n");
	bu_exit(-1, NULL);
    }

    /* make sure that at least the last byte is null */
    cp = (char *)&rec[DB_SS_NGRAN-1];
    cp += (sizeof(union record) - 1);
    *cp = '\0';

    fprintf(ofp,  "%c %.16s %.16s %s\n",
		  rec[0].ss.ss_id,	/* s */
		  rec[0].ss.ss_keyword,	/* "ebm", "vol", or ??? */
		  rec[0].ss.ss_name,	/* solid name */
		  rec[0].ss.ss_args);	/* everything else */

}

static void
idendump(void)	/* Print out Ident record information */
{
    fprintf(ofp,  "%c %d %.6s\n",
		  record.i.i_id,			/* I */
		  record.i.i_units,		/* units */
		  CH(record.i.i_version)		/* version */
	);
    fprintf(ofp,  "%.72s\n",
		  CH(record.i.i_title)	/* title or description */
	);

    /* Print a warning message on stderr if versions differ */
    if (!BU_STR_EQUAL(record.i.i_version, ID_VERSION)) {
	fprintf(stderr,
		      "g2asc: File is version (%s), Program is version (%s)\n",
		      record.i.i_version, ID_VERSION);
    }
}

static void
polyhead_asc(void)	/* Print out Polyhead record information */
{
    fprintf(ofp, "%c ", record.p.p_id);		/* P */
    fprintf(ofp, "%.16s", encode_name(record.p.p_name));	/* unique name */
    fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

static void
polydata_asc(void)	/* Print out Polydata record information */
{
    int i, j;

    fprintf(ofp, "%c ", record.q.q_id);		/* Q */
    fprintf(ofp, "%d", record.q.q_count);		/* # of vertices <= 5 */
    for (i = 0; i < 5; i++) {
	/* [5][3] vertices */
	for (j = 0; j < 3; j++) {
	    fprintf(ofp, " %.12e", record.q.q_verts[i][j]);
	}
    }
    for (i = 0; i < 5; i++) {
	/* [5][3] normals */
	for (j = 0; j < 3; j++) {
	    fprintf(ofp, " %.12e", record.q.q_norms[i][j]);
	}
    }
    fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

static void
soldump(void)	/* Print out Solid record information */
{
    int i;

    fprintf(ofp, "%c ", record.s.s_id);	/* S */
    fprintf(ofp, "%d ", record.s.s_type);	/* GED primitive type */
    fprintf(ofp, "%.16s ", encode_name(record.s.s_name));	/* unique name */
    fprintf(ofp, "%d", record.s.s_cgtype);/* COMGEOM solid type */
    for (i = 0; i < 24; i++)
	fprintf(ofp, " %.12e", record.s.s_values[i]); /* parameters */
    fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

static void
cline_dump(void)
{
    size_t ngranules;	/* number of granules, total */
    char *name;
    struct rt_cline_internal *cli;
    struct bu_external ext;
    struct rt_db_internal intern;

    name = record.cli.cli_name;

    ngranules = 1;
    get_ext(&ext, ngranules);

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ((OBJ[ID_CLINE].ft_import4(&intern, &ext, id_mat, DBI_NULL, &rt_uniresource)) != 0) {
	fprintf(stderr, "g2asc: cline import failure\n");
	bu_exit(-1, NULL);
    }

    cli = (struct rt_cline_internal *)intern.idb_ptr;
    RT_CLINE_CK_MAGIC(cli);

    fprintf(ofp, "%c ", DBID_CLINE);	/* c */
    fprintf(ofp, "%.16s ", name);	/* unique name */
    fprintf(ofp, "%26.20e %26.20e %26.20e ", V3ARGS(cli->v));
    fprintf(ofp, "%26.20e %26.20e %26.20e ", V3ARGS(cli->h));
    fprintf(ofp, "%26.20e %26.20e", cli->radius, cli->thickness);
    fprintf(ofp, "\n");			/* Terminate w/ a newline */

    rt_db_free_internal(&intern);
    bu_free_external(&ext);
}

static void
bot_dump(void)
{
    size_t ngranules;
    char *name;
    struct rt_bot_internal *bot;
    struct bu_external ext;
    struct rt_db_internal intern;
    size_t i;

    name = record.bot.bot_name;
    ngranules = ntohl(*(uint32_t *)record.bot.bot_nrec) + 1;
    get_ext(&ext, ngranules);

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ((OBJ[ID_BOT].ft_import4(&intern, &ext, id_mat, DBI_NULL, &rt_uniresource)) != 0) {
	fprintf(stderr, "g2asc: bot import failure\n");
	bu_exit(-1, NULL);
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    fprintf(ofp, "%c ", DBID_BOT);	/* t */
    fprintf(ofp, "%.16s ", name);	/* unique name */
    fprintf(ofp, "%d ", bot->mode);
    fprintf(ofp, "%d ", bot->orientation);
    fprintf(ofp, "%d ", 0);	/* was error_mode */
    fprintf(ofp, "%lu ", (unsigned long)bot->num_vertices);
    fprintf(ofp, "%lu", (unsigned long)bot->num_faces);
    fprintf(ofp, "\n");

    for (i = 0; i < bot->num_vertices; i++)
	fprintf(ofp,  "	%lu: %26.20e %26.20e %26.20e\n", (unsigned long)i, V3ARGS(&bot->vertices[i*3]));
    if (bot->mode == RT_BOT_PLATE) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	for (i = 0; i < bot->num_faces; i++)
	    fprintf(ofp,  "	%lu: %d %d %d %26.20e\n", (unsigned long)i, V3ARGS(&bot->faces[i*3]), bot->thickness[i]);
	bu_bitv_to_hex(&vls, bot->face_mode);
	fprintf(ofp,  "	%s\n", bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else {
	for (i = 0; i < bot->num_faces; i++)
	    fprintf(ofp,  "	%lu: %d %d %d\n", (unsigned long)i, V3ARGS(&bot->faces[i*3]));
    }

    rt_db_free_internal(&intern);
    bu_free_external(&ext);
}

static void
pipe_dump(void)	/* Print out Pipe record information */
{

    size_t ngranules;	/* number of granules, total */
    char *name;
    struct rt_pipe_internal *pipeip;		/* want a struct for the head, not a ptr. */
    struct bu_external ext;
    struct rt_db_internal intern;

    ngranules = ntohl(*(uint32_t *)record.pwr.pwr_count) + 1;
    name = record.pwr.pwr_name;

    get_ext(&ext, ngranules);

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ((OBJ[ID_PIPE].ft_import4(&intern, &ext, id_mat, NULL, &rt_uniresource)) != 0) {
	fprintf(stderr, "g2asc: pipe import failure\n");
	bu_exit(-1, NULL);
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    RT_PIPE_CK_MAGIC(pipeip);

    /* send the doubly linked list off to dump_pipe_segs(), which
     * will print all the information.
     */

    dump_pipe_segs(name, &pipeip->pipe_segs_head);

    rt_db_free_internal(&intern);
    bu_free_external(&ext);
}

static void
dump_pipe_segs(char *name, struct bu_list *headp)
{

    struct wdb_pipe_pnt *sp;

    fprintf(ofp, "%c %.16s\n", DBID_PIPE, name);

    /* print parameters for each point: one point per line */

    for (BU_LIST_FOR(sp, wdb_pipe_pnt, headp)) {
	fprintf(ofp,  "%26.20e %26.20e %26.20e %26.20e %26.20e %26.20e\n",
		sp->pp_id, sp->pp_od, sp->pp_bendradius, V3ARGS(sp->pp_coord));
    }
    fprintf(ofp,  "END_PIPE %s\n", name);
}

/*
 * Print out Particle record information.
 * Note that particles fit into one granule only.
 */
static void
particle_dump(void)
{
    struct rt_part_internal 	*part;	/* head for the structure */
    struct bu_external	ext;
    struct rt_db_internal	intern;

    get_ext(&ext, 1);

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ((OBJ[ID_PARTICLE].ft_import4(&intern, &ext, id_mat, NULL, &rt_uniresource)) != 0) {
	fprintf(stderr, "g2asc: particle import failure\n");
	bu_exit(-1, NULL);
    }

    part = (struct rt_part_internal *)intern.idb_ptr;
    RT_PART_CK_MAGIC(part);

    /* Particle type is picked up on here merely to ensure receiving
     * valid data.  The type is not used any further.
     */

    switch (part->part_type) {
	case RT_PARTICLE_TYPE_SPHERE:
	    break;
	case RT_PARTICLE_TYPE_CYLINDER:
	    break;
	case RT_PARTICLE_TYPE_CONE:
	    break;
	default:
	    fprintf(stderr, "g2asc: no particle type %d\n", part->part_type);
	    bu_exit(-1, NULL);
    }

    fprintf(ofp, "%c %.16s %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e %26.20e\n",
	    record.part.p_id, record.part.p_name,
	    part->part_V[X],
	    part->part_V[Y],
	    part->part_V[Z],
	    part->part_H[X],
	    part->part_H[Y],
	    part->part_H[Z],
	    part->part_vrad, part->part_hrad);
}


/*
 *  Print out arbn information.
 *
 */
static void
arbn_dump(void)
{
    size_t ngranules;	/* number of granules to be read */
    size_t i;		/* a counter */
    char *name;
    struct rt_arbn_internal *arbn;
    struct bu_external ext;
    struct rt_db_internal intern;

    ngranules = ntohl(*(uint32_t *)record.n.n_grans) + 1;
    name = record.n.n_name;

    get_ext(&ext, ngranules);

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ((OBJ[ID_ARBN].ft_import4(&intern, &ext, id_mat, NULL, &rt_uniresource)) != 0) {
	fprintf(stderr, "g2asc: arbn import failure\n");
	bu_exit(-1, NULL);
    }

    arbn = (struct rt_arbn_internal *)intern.idb_ptr;
    RT_ARBN_CK_MAGIC(arbn);

    fprintf(ofp, "%c %.16s %lu\n", 'n', name, (unsigned long)arbn->neqn);
    for (i = 0; i < arbn->neqn; i++) {
	fprintf(ofp, "n %26.20e %20.26e %26.20e %26.20e\n",
		arbn->eqn[i][X], arbn->eqn[i][Y],
		arbn->eqn[i][Z], arbn->eqn[i][3]);
    }

    rt_db_free_internal(&intern);
    bu_free_external(&ext);
}


/*
 *  Note that for compatibility with programs such as FRED that
 *  (inappropriately) read .asc files, the member count has to be
 *  recalculated here.
 *
 *  Returns -
 *	0	converted OK
 *	1	converted OK, left next record in global "record" for reuse.
 */
static int
combdump(void)	/* Print out Combination record information */
{
    int	m1, m2;		/* material property flags */
    struct bu_list	head;
    struct mchain {
	struct bu_list	l;
	union record	r;
    };
    struct mchain	*mp;
    struct mchain	*ret_mp = (struct mchain *)0;
    int		mcount;

    /*
     *  Gobble up all subsequent member records, so that
     *  an accurate count of them can be output.
     */
    BU_LIST_INIT(&head);
    mcount = 0;
    while (1) {
	BU_GET(mp, struct mchain);
	if (fread((char *)&mp->r, sizeof(mp->r), 1, ifp) != 1
	     || feof(ifp))
	    break;
	if (mp->r.u_id != ID_MEMB) {
	    ret_mp = mp;	/* Handle it later */
	    break;
	}
	BU_LIST_INSERT(&head, &(mp->l));
	mcount++;
    }

    /*
     *  Output the combination
     */
    fprintf(ofp, "%c ", record.c.c_id);		/* C */
    switch (record.c.c_flags) {
	case DBV4_REGION:
	    fprintf(ofp, "Y ");			/* Y if `R' */
	    break;
	case DBV4_NON_REGION_NULL:
	case DBV4_NON_REGION:
	    fprintf(ofp, "N ");			/* N if ` ' or '\0' */
	    break;
	case DBV4_REGION_FASTGEN_PLATE:
	    fprintf(ofp, "P ");
	    break;
	case DBV4_REGION_FASTGEN_VOLUME:
	    fprintf(ofp, "V ");
	    break;
    }
    fprintf(ofp, "%.16s ", encode_name(record.c.c_name));	/* unique name */
    fprintf(ofp, "%d ", record.c.c_regionid);	/* region ID code */
    fprintf(ofp, "%d ", record.c.c_aircode);	/* air space code */

    /* DEPRECATED: # of members */
    fprintf(ofp, "%d ", mcount);

    /* DEPRECATED: COMGEOM region # */
    fprintf(ofp, "%d ", 0 /* was record.c.c_num */);

    fprintf(ofp, "%d ", record.c.c_material);	/* material code */
    fprintf(ofp, "%d ", record.c.c_los);		/* equiv. LOS est. */
    fprintf(ofp, "%d %d %d %d ",
		  record.c.c_override ? 1 : 0,
		  record.c.c_rgb[0],
		  record.c.c_rgb[1],
		  record.c.c_rgb[2]);
    m1 = m2 = 0;
    if (isprint((int)record.c.c_matname[0])) {
	m1 = 1;
	if (record.c.c_matparm[0])
	    m2 = 1;
    }
    fprintf(ofp, "%d %d", m1, m2);
    switch (record.c.c_inherit) {
	case DB_INH_HIGHER:
	    fprintf(ofp, " %d", DB_INH_HIGHER);
	    break;
	default:
	case DB_INH_LOWER:
	    fprintf(ofp, " %d", DB_INH_LOWER);
	    break;
    }
    fprintf(ofp, "\n");			/* Terminate w/ a newline */

    if (m1)
	fprintf(ofp, "%.32s\n", CH(record.c.c_matname));
    if (m2)
	fprintf(ofp, "%.60s\n", CH(record.c.c_matparm));

    /*
     *  Output the member records now
     */
    while (BU_LIST_WHILE(mp, mchain, &head)) {
	membdump(&mp->r);
	BU_LIST_DEQUEUE(&mp->l);
	BU_PUT(mp, struct mchain);
    }

    if (ret_mp) {
	memcpy((char *)&record, (char *)&ret_mp->r, sizeof(record));
	BU_PUT(ret_mp, struct mchain);
	return 1;
    }
    return 0;
}

/*
 *  Print out Member record information.
 *  Intended to be called by combdump only.
 */
static void
membdump(union record *rp)
{
    int i;

    fprintf(ofp, "%c ", rp->M.m_id);		/* M */
    fprintf(ofp, "%c ", rp->M.m_relation);	/* Boolean oper. */
    fprintf(ofp, "%.16s ", encode_name(rp->M.m_instname));	/* referred-to obj. */
    for (i = 0; i < 16; i++)			/* homogeneous transform matrix */
	fprintf(ofp, "%.12e ", rp->M.m_mat[i]);
    fprintf(ofp, "%d", 0);			/* was COMGEOM solid # */
    fprintf(ofp, "\n");				/* Terminate w/ nl */
}

static void
arsadump(void)	/* Print out ARS record information */
{
    int i;
    int length;	/* Keep track of number of ARS B records */

    fprintf(ofp, "%c ", record.a.a_id);	/* A */
    fprintf(ofp, "%d ", record.a.a_type);	/* primitive type */
    fprintf(ofp, "%.16s ", encode_name(record.a.a_name));	/* unique name */
    fprintf(ofp, "%d ", record.a.a_m);	/* # of curves */
    fprintf(ofp, "%d ", record.a.a_n);	/* # of points per curve */
    fprintf(ofp, "%d ", record.a.a_curlen);/* # of granules per curve */
    fprintf(ofp, "%d ", record.a.a_totlen);/* # of granules for ARS */
    fprintf(ofp, "%.12e ", record.a.a_xmax);	/* max x coordinate */
    fprintf(ofp, "%.12e ", record.a.a_xmin);	/* min x coordinate */
    fprintf(ofp, "%.12e ", record.a.a_ymax);	/* max y coordinate */
    fprintf(ofp, "%.12e ", record.a.a_ymin);	/* min y coordinate */
    fprintf(ofp, "%.12e ", record.a.a_zmax);	/* max z coordinate */
    fprintf(ofp, "%.12e", record.a.a_zmin);	/* min z coordinate */
    fprintf(ofp, "\n");			/* Terminate w/ a newline */

    length = (int)record.a.a_totlen;	/* Get # of ARS B records */

    for (i = 0; i < length; i++) {
	arsbdump();
    }
}

static void
arsbdump(void)	/* Print out ARS B record information */
{
    int i;
    size_t ret;

    /* Read in a member record for processing */
    ret = fread((char *)&record, sizeof record, 1, ifp);
    if (ret != 1)
	perror("fread");
    fprintf(ofp, "%c ", record.b.b_id);		/* B */
    fprintf(ofp, "%d ", record.b.b_type);		/* primitive type */
    fprintf(ofp, "%d ", record.b.b_n);		/* current curve # */
    fprintf(ofp, "%d", record.b.b_ngranule);	/* current granule */
    for (i = 0; i < 24; i++) {
	/* [8*3] vectors */
	fprintf(ofp, " %.12e", record.b.b_values[i]);
    }
    fprintf(ofp, "\n");			/* Terminate w/ a newline */
}

static void
materdump(void)	/* Print out material description record information */
{
    fprintf(ofp,  "%c %d %d %d %d %d %d\n",
		  record.md.md_id,			/* m */
		  record.md.md_flags,			/* UNUSED */
		  record.md.md_low,	/* low end of region IDs affected */
		  record.md.md_hi,	/* high end of region IDs affected */
		  record.md.md_r,
		  record.md.md_g,		/* color of regions: 0..255 */
		  record.md.md_b);
}

static void
bspldump(void)	/* Print out B-spline solid description record information */
{
    fprintf(ofp,  "%c %.16s %d\n",
		  record.B.B_id,		/* b */
		  encode_name(record.B.B_name),	/* unique name */
		  record.B.B_nsurf);	/* # of surfaces in this solid */
}

static void
bsurfdump(void)	/* Print d-spline surface description record information */
{
    size_t i;
    float *vp;
    size_t nbytes, count;
    float *fp;

    fprintf(ofp,  "%c %d %d %d %d %d %d %d %d %d\n",
		  record.d.d_id,		/* D */
		  record.d.d_order[0],	/* order of u and v directions */
		  record.d.d_order[1],	/* order of u and v directions */
		  record.d.d_kv_size[0],	/* knot vector size (u and v) */
		  record.d.d_kv_size[1],	/* knot vector size (u and v) */
		  record.d.d_ctl_size[0],	/* control mesh size (u and v) */
		  record.d.d_ctl_size[1],	/* control mesh size (u and v) */
		  record.d.d_geom_type,	/* geom type 3 or 4 */
		  record.d.d_nknots,	/* # granules of knots */
		  record.d.d_nctls);	/* # granules of ctls */
    /*
     * The b_surf_head record is followed by
     * d_nknots granules of knot vectors (first u, then v),
     * and then by d_nctls granules of control mesh information.
     * Note that neither of these have an ID field!
     *
     * B-spline surface record, followed by
     *	d_kv_size[0] floats,
     *	d_kv_size[1] floats,
     *	padded to d_nknots granules, followed by
     *	ctl_size[0]*ctl_size[1]*geom_type floats,
     *	padded to d_nctls granules.
     *
     * IMPORTANT NOTE: granule == sizeof(union record)
     */

    /* Malloc and clear memory for the KNOT DATA and read it */
    nbytes = record.d.d_nknots * sizeof(union record);
    vp = (float *)bu_calloc((unsigned int)nbytes, 1, "KNOT DATA");
    fp = vp;
    count = fread((char *)fp, 1, nbytes, ifp);
    if (count != nbytes) {
	fprintf(stderr, "g2asc: spline knot read failure\n");
	bu_exit(1, NULL);
    }
    /* Print the knot vector information */
    count = record.d.d_kv_size[0] + record.d.d_kv_size[1];
    for (i = 0; i < count; i++) {
	fprintf(ofp, "%.12e\n", *vp++);
    }
    /* Free the knot data memory */
    (void)bu_free((char *)fp, "KNOT DATA");

    /* Malloc and clear memory for the CONTROL MESH data and read it */
    nbytes = record.d.d_nctls * sizeof(union record);
    vp = (float *)bu_calloc((unsigned int)nbytes, 1, "CONTROL MESH");
    fp = vp;
    count = fread((char *)fp, 1, nbytes, ifp);
    if (count != nbytes) {
	fprintf(stderr, "g2asc: control mesh read failure\n");
	bu_exit(1, NULL);
    }
    /* Print the control mesh information */
    count = record.d.d_ctl_size[0] * record.d.d_ctl_size[1] *
	record.d.d_geom_type;
    for (i = 0; i < count; i++) {
	fprintf(ofp, "%.12e\n", *vp++);
    }
    /* Free the control mesh memory */
    (void)bu_free((char *)fp, "CONTROL MESH");
}

/*
 *  Take a string and a length, and null terminate,
 *  converting unprintable characters to something printable.
 */
static char *
strchop(char *str, size_t len)
{
    static char buf[10000] = {0};
    char *ip = str;
    char *op = buf;
    int warn = 0;
    char *ep;

    CLAMP(len, 1, sizeof(buf)-2);

    ep = &buf[len-1];		/* Leave room for null */
    while (op < ep) {
	if (*ip == '\0')  break;
	if ((int)isprint((int)*ip) || isspace((int)*ip)) {
	    *op++ = *ip++;
	}  else  {
	    *op++ = '@';
	    ip++;
	    warn = 1;
	}
    }
    *op = '\0';
    if (warn) {
	fprintf(stderr,
		      "g2asc: Illegal char in string, converted to '%s'\n",
		      buf);
    }
    if (op == buf) {
	/* Null input name */
	fprintf(stderr,
		      "g2asc:  NULL string converted to -=STRING=-\n");
	// TODO
	//return "-=STRING=-";
	return NULL;
    }
    return buf;
}

static void
extrdump(void)
{
    struct rt_extrude_internal	*extr;
    int				ngranules;
    char				*myname;
    struct bu_external		ext;
    struct rt_db_internal		intern;

    myname = record.extr.ex_name;
    ngranules = ntohl(*(uint32_t *)record.extr.ex_count) + 1;
    get_ext(&ext, ngranules);

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ((OBJ[ID_EXTRUDE].ft_import4(&intern, &ext, id_mat, DBI_NULL, &rt_uniresource)) != 0) {
	fprintf(stderr, "g2asc: extrusion import failure\n");
	bu_exit(-1, NULL);
    }

    extr = (struct rt_extrude_internal *)intern.idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extr);

    fprintf(ofp, "%c ", DBID_EXTR);	/* e */
    fprintf(ofp, "%.16s ", encode_name(myname));	/* unique name */
    fprintf(ofp, "%.16s ", encode_name(extr->sketch_name));
    fprintf(ofp, "%d ", extr->keypoint);
    fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS(extr->V));
    fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS(extr->h));
    fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS(extr->u_vec));
    fprintf(ofp, "%.12e %.12e %.12e\n", V3ARGS(extr->v_vec));
}

static void
sketchdump(void)
{
    struct rt_sketch_internal *skt;
    size_t ngranules;
    char *myname;
    struct bu_external ext;
    struct rt_db_internal intern;
    size_t i, j;
    struct rt_curve *crv;

    myname = record.skt.skt_name;
    ngranules = ntohl(*(uint32_t *)record.skt.skt_count) + 1;
    get_ext(&ext, ngranules);

    /* Hand off to librt's import() routine */
    RT_DB_INTERNAL_INIT(&intern);
    if ((OBJ[ID_SKETCH].ft_import4(&intern, &ext, id_mat, DBI_NULL, &rt_uniresource)) != 0) {
	fprintf(stderr, "g2asc: sketch import failure\n");
	bu_exit(-1, NULL);
    }

    skt = (struct rt_sketch_internal *)intern.idb_ptr;
    RT_SKETCH_CK_MAGIC(skt);
    crv = &skt->curve;
    fprintf(ofp, "%c ", DBID_SKETCH); /* d */
    fprintf(ofp, "%.16s ", encode_name(myname));  /* unique name */
    fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS(skt->V));
    fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS(skt->u_vec));
    fprintf(ofp, "%.12e %.12e %.12e ", V3ARGS(skt->v_vec));
    fprintf(ofp, "%lu %lu\n", (unsigned long)skt->vert_count, (unsigned long)crv->count);
    for (i = 0; i < skt->vert_count; i++)
	fprintf(ofp, " %.12e %.12e", V2ARGS(skt->verts[i]));
    fprintf(ofp, "\n");

    for (j = 0; j < crv->count; j++) {
	long *lng;
	struct line_seg *lsg;
	struct carc_seg *csg;
	struct nurb_seg *nsg;
	int k;

	lng = (long *)crv->segment[j];
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		fprintf(ofp, "  L %d %d %d\n", crv->reverse[j], lsg->start, lsg->end);
		break;
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		fprintf(ofp, "  A %d %d %d %.12e %d %d\n", crv->reverse[j], csg->start, csg->end,
			      csg->radius, csg->center_is_left, csg->orientation);
		break;
	    case CURVE_NURB_MAGIC:
		nsg = (struct nurb_seg *)lng;
		fprintf(ofp, "  N %d %d %d %d %d\n   ", crv->reverse[j], nsg->order, nsg->pt_type,
			      nsg->k.k_size, nsg->c_size);
		for (k = 0; k < nsg->k.k_size; k++)
		    fprintf(ofp, " %.12e", nsg->k.knots[k]);
		fprintf(ofp, "\n   ");
		for (k = 0; k < nsg->c_size; k++)
		    fprintf(ofp, " %d", nsg->ctl_points[k]);
		fprintf(ofp, "\n");
		break;
	}
    }
}

int
asc_write_v4(
	    struct gcv_context *UNUSED(c),
	    const struct gcv_opts *UNUSED(o),
	    const char *dest_path
	    )
{
    if (!dest_path) return 0;

top:
do {
	/* A v4 record is already in the input buffer */
	/* Check record type and skip deleted records */
	switch (record.u_id) {
	    case ID_FREE:
		continue;
	    case ID_SOLID:
		soldump();
		continue;
	    case ID_COMB:
		if (combdump() > 0)
		    goto top;
		continue;
	    case ID_MEMB:
		fprintf(stderr, "g2asc: stray MEMB record, skipped\n");
		continue;
	    case ID_ARS_A:
		arsadump();
		continue;
	    case ID_P_HEAD:
		polyhead_asc();
		continue;
	    case ID_P_DATA:
		polydata_asc();
		continue;
	    case ID_IDENT:
		idendump();
		continue;
	    case ID_MATERIAL:
		materdump();
		continue;
	    case DBID_PIPE:
		pipe_dump();
		continue;
	    case DBID_STRSOL:
		strsol_dump();
		continue;
	    case DBID_NMG:
		nmg_dump();
		continue;
	    case DBID_PARTICLE:
		particle_dump();
		continue;
	    case DBID_ARBN:
		arbn_dump();
		continue;
	    case DBID_CLINE:
		cline_dump();
		continue;
	    case DBID_BOT:
		bot_dump();
		continue;
	    case ID_BSOLID:
		bspldump();
		continue;
	    case ID_BSURF:
		bsurfdump();
		continue;
	    case DBID_SKETCH:
		sketchdump();
		continue;
	    case DBID_EXTR:
		extrdump();
		continue;
	    default:
		fprintf(stderr,
			      "g2asc: unable to convert record type '%c' (0%o), skipping\n",
			      record.u_id, record.u_id);
		continue;
	}
    }  while (fread((char *)&record, sizeof record, 1, ifp) == 1  &&
	       !feof(ifp));

    return 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
