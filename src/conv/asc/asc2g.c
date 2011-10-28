/*                         A S C 2 G . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
 *
 */
/** @file asc2g.c
 *
 * This program generates a GED database from an ASCII GED data file.
 *
 * Usage:  asc2g file.asc file.g
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"
#include "bin.h"

#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "ged.h"
#include "wdb.h"
#include "mater.h"
#include "tclcad.h"


/* maximum input line buffer size */
#define BUFSIZE (16*1024)
#define SIZE (128*1024*1024)
#define TYPE_LEN 200
#define NAME_LEN 200


/* GED database record */
static union record record;

/* Record input buffer */
char *buf = NULL;
char NAME[NAME_LEN + 2] = {0};

FILE *ifp = NULL;
struct rt_wdb *ofp = NULL;
static int ars_ncurves = 0;
static int ars_ptspercurve = 0;
static int ars_curve = 0;
static int ars_pt = 0;
static char *ars_name = NULL;
static fastf_t **ars_curves = NULL;
static char *slave_name = "safe_interp";
static char *db_name = "_db";

static int linecnt = 0;
static char usage[] = "\
Usage: asc2g file.asc file.g\n\
 Convert an ASCII BRL-CAD database to binary form\n\
";

char *aliases[] = {
    "attr",
    "color",
    "put",
    "title",
    "units",
    "find",
    "dbfind",
    "rm",
    (char *)0
};


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
ngran(int nfloat)
{
    int gran;
    /* Round up */
    gran = nfloat + ((sizeof(union record)-1) / sizeof(float));
    gran = (gran * sizeof(float)) / sizeof(union record);
    return gran;
}


int
incr_ars_pt(void)
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
 * Z A P _ N L
 *
 * This routine removes newline and carriage return characters from
 * the buffer and substitutes in NULL.
 */
void
zap_nl(void)
{
    char *bp;

    bp = &buf[0];

    while (*bp != '\0') {
	if ((*bp == '\n') || (*bp == '\r')) {
	    *bp = '\0';
	}
	bp++;
    }
}


/**
 * S T R S O L B L D
 *
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
    char	*type = NULL;
    char	*name = NULL;
    char	*args = NULL;
    struct bu_vls	str;
    char *buf2 = (char *)bu_malloc(sizeof(char) * BUFSIZE, "strsolbld temporary buffer");
    char *bufp = buf2;

    memcpy(buf2, buf, sizeof(char) * BUFSIZE);

    bu_vls_init(&str);

#if defined (HAVE_STRSEP)
    (void)strsep(&buf2, " ");		/* skip stringsolid_id */
    type = strsep(&buf2, " ");
    name = strsep(&buf2, " ");
    args = strsep(&buf2, "\n");
#else
    (void)strtok(buf, " ");		/* skip stringsolid_id */
    type = strtok(NULL, " ");
    name = strtok(NULL, " ");
    args = strtok(NULL, "\n");
#endif

    if (BU_STR_EQUAL(type, "dsp")) {
	struct rt_dsp_internal *dsp;

	BU_GETSTRUCT(dsp, rt_dsp_internal);
	bu_vls_init(&dsp->dsp_name);
	bu_vls_strcpy(&str, args);
	if (bu_struct_parse(&str, rt_functab[ID_DSP].ft_parsetab, (char *)dsp) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    ftp = rt_get_functab_by_label("dsp");
	    if (ftp && ftp->ft_ifree)
		ftp->ft_ifree((struct rt_db_internal *)dsp);
	    goto out;
	}
	dsp->magic = RT_DSP_INTERNAL_MAGIC;
	if (wdb_export(ofp, name, (genptr_t)dsp, ID_DSP, mk_conv2mm) < 0) {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'dsp' has already been freed by wdb_export() */
    } else if (BU_STR_EQUAL(type, "ebm")) {
	struct rt_ebm_internal *ebm;

	BU_GETSTRUCT(ebm, rt_ebm_internal);

	MAT_IDN(ebm->mat);

	bu_vls_strcpy(&str, args);
	if (bu_struct_parse(&str, rt_functab[ID_EBM].ft_parsetab, (char *)ebm) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    ftp = rt_get_functab_by_label("ebm");
	    if (ftp && ftp->ft_ifree)
		ftp->ft_ifree((struct rt_db_internal *)ebm);
	    return;
	}
	ebm->magic = RT_EBM_INTERNAL_MAGIC;
	if (wdb_export(ofp, name, (genptr_t)ebm, ID_EBM, mk_conv2mm) < 0) {
	    bu_log("strsolbld(%s): Unable to export %s solid, args='%s'\n",
		   name, type, args);
	    goto out;
	}
	/* 'ebm' has already been freed by wdb_export() */
    } else if (BU_STR_EQUAL(type, "vol")) {
	struct rt_vol_internal *vol;

	BU_GETSTRUCT(vol, rt_vol_internal);
	MAT_IDN(vol->mat);

	bu_vls_strcpy(&str, args);
	if (bu_struct_parse(&str, rt_functab[ID_VOL].ft_parsetab, (char *)vol) < 0) {
	    bu_log("strsolbld(%s): Unable to parse %s solid's args of '%s'\n",
		   name, type, args);
	    ftp = rt_get_functab_by_label("vol");
	    if (ftp && ftp->ft_ifree)
		ftp->ft_ifree((struct rt_db_internal *)vol);
	    return;
	}
	vol->magic = RT_VOL_INTERNAL_MAGIC;
	if (wdb_export(ofp, name, (genptr_t)vol, ID_VOL, mk_conv2mm) < 0) {
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
    bu_free(bufp, "strsolbld temporary buffer");
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

    cp = buf;

    cp++;
    cp++;

    (void)sscanf(cp, "%200s %f %f %f %f %f %f %f %f %f %lu %lu", /* NAME_LEN */
		 name,
		 &fV[0], &fV[1], &fV[2],
		 &fu[0], &fu[1], &fu[2],
		 &fv[0], &fv[1], &fv[2],
		 &vert_count, &seg_count);

    VMOVE(V, fV);
    VMOVE(u, fu);
    VMOVE(v, fv);

    verts = (point2d_t *)bu_calloc(vert_count, sizeof(point2d_t), "verts");

    if (bu_fgets(buf, BUFSIZE, ifp) == (char *)0)
	bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);

    verts = (point2d_t *)bu_calloc(vert_count, sizeof(point2d_t), "verts");
    cp = buf;
    ptr = strtok(buf, " ");
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

    skt = (struct rt_sketch_internal *)bu_calloc(1, sizeof(struct rt_sketch_internal), "sketch");
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VMOVE(skt->V, V);
    VMOVE(skt->u_vec, u);
    VMOVE(skt->v_vec, v);
    skt->vert_count = vert_count;
    skt->verts = verts;
    crv = &skt->curve;
    crv->count = seg_count;

    crv->segment = (genptr_t *)bu_calloc(crv->count, sizeof(genptr_t), "segments");
    crv->reverse = (int *)bu_calloc(crv->count, sizeof(int), "reverse");
    for (j=0; j<crv->count; j++) {
	double radius;
	int k;

	if (bu_fgets(buf, BUFSIZE, ifp) == (char *)0)
	    bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);

	cp = buf + 2;
	switch (*cp) {
	    case LSEG:
		lsg = (struct line_seg *)bu_malloc(sizeof(struct line_seg), "line segment");
		sscanf(cp+1, "%d %d %d", &crv->reverse[j], &lsg->start, &lsg->end);
		lsg->magic = CURVE_LSEG_MAGIC;
		crv->segment[j] = lsg;
		break;
	    case CARC:
		csg = (struct carc_seg *)bu_malloc(sizeof(struct carc_seg), "arc segment");
		sscanf(cp+1, "%d %d %d %lf %d %d", &crv->reverse[j], &csg->start, &csg->end,
		       &radius, &csg->center_is_left, &csg->orientation);
		csg->radius = radius;
		csg->magic = CURVE_CARC_MAGIC;
		crv->segment[j] = csg;
		break;
	    case NURB:
		nsg = (struct nurb_seg *)bu_malloc(sizeof(struct nurb_seg), "nurb segment");
		sscanf(cp+1, "%d %d %d %d %d", &crv->reverse[j], &nsg->order, &nsg->pt_type,
		       &nsg->k.k_size, &nsg->c_size);
		nsg->k.knots = (fastf_t *)bu_calloc(nsg->k.k_size, sizeof(fastf_t), "knots");
		nsg->ctl_points = (int *)bu_calloc(nsg->c_size, sizeof(int), "control points");
		if (bu_fgets(buf, BUFSIZE, ifp) == (char *)0)
		    bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);
		cp = buf + 3;
		ptr = strtok(cp, " ");
		if (!ptr)
		    bu_exit(1, "ERROR: not enough knots for nurb segment in sketch (%s)\n", name);
		for (k=0; k<nsg->k.k_size; k++) {
		    nsg->k.knots[k] = atof(ptr);
		    ptr = strtok((char *)NULL, " ");
		    if (!ptr && k<nsg->k.k_size-1)
			bu_exit(1, "ERROR: not enough knots for nurb segment in sketch (%s)\n", name);
		}
		if (bu_fgets(buf, BUFSIZE, ifp) == (char *)0)
		    bu_exit(-1, "Unexpected EOF while reading sketch (%s) data\n", name);
		cp = buf + 3;
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

    (void)mk_sketch(ofp, name,  skt);
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

    cp = buf;

    cp++;

    cp++;
    (void)sscanf(cp, "%200s %200s %d %f %f %f  %f %f %f %f %f %f %f %f %f", /* NAME_LEN */
		 name, sketch_name, &keypoint, &fV[0], &fV[1], &fV[2], &fh[0], &fh[1], &fh[2],
		 &fu_vec[0], &fu_vec[1], &fu_vec[2], &fv_vec[0], &fv_vec[1], &fv_vec[2]);

    VMOVE(V, fV);
    VMOVE(h, fh);
    VMOVE(u_vec, fu_vec);
    VMOVE(v_vec, fv_vec);
    (void)mk_extrusion(ofp, name, sketch_name, V, h, u_vec, v_vec, keypoint);
}

/**
 * N M G B L D
 *
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
    int	version;
    char	*name;
    long	granules;
    long	struct_count[26];
    struct bu_external	ext;
    struct rt_db_internal	intern;
    int	j;

    /* First, process the header line */
    cp = strtok(buf, " ");
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
    ext.ext_buf = bu_malloc(ext.ext_nbytes, "nmg ext_buf");
    *(uint32_t *)ext.ext_buf = htonl(version);
    BU_ASSERT_LONG(version, ==, 1);	/* DISK_MODEL_VERSION */

    /* Get next line of input with the 26 counts on it */
    if (bu_fgets(buf, BUFSIZE, ifp) == (char *)0)
	bu_exit(-1, "Unexpected EOF while reading NMG %s data, line 2\n", name);

    /* Second, process counts for each kind of structure */
    cp = strtok(buf, " ");
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

	if (bu_fgets(buf, BUFSIZE, ifp) == (char *)0)
	    bu_exit(-1, "Unexpected EOF while reading NMG %s data, hex line %d\n", name, j);

	for (k=0; k<32; k++) {
	    sscanf(&buf[k*2], "%2x", &cp_i);
	    *cp++ = cp_i;
	}
    }

    /* Next, import this disk record into memory */
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_functab[ID_NMG].ft_import5(&intern, &ext, bn_mat_identity, ofp->dbip, &rt_uniresource) < 0)
	bu_exit(-1, "ft_import5 failed on NMG %s\n", name);
    bu_free_external(&ext);

    /* Now we should have a good NMG in memory */
    nmg_vmodel((struct model *)intern.idb_ptr);

    /* Finally, squirt it back out through LIBWDB */
    mk_nmg(ofp, name, (struct model *)intern.idb_ptr);
    /* mk_nmg() frees the intern.idp_ptr pointer */
    RT_DB_INTERNAL_INIT(&intern);

    bu_free(name, "name");
}


/**
 * S O L B L D
 *
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

    char	s_type;		/* id for the type of primitive */
    fastf_t	val[24];	/* array of values/parameters for solid */
    point_t	center;		/* center; used by many solids */
    point_t pnts[9];		/* array of points for the arbs */
    point_t	norm;
    vect_t	a, b, c, d, n;	/* various vectors required */
    vect_t	height;		/* height vector for tgc */
    vect_t	breadth;	/* breadth vector for rpc */
    double	dd, rad1, rad2;

    cp = buf;
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
	    (void)mk_grip(ofp, NAME, center, n, val[6]);
	    break;

	case TOR:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(n, val[3], val[4], val[5]);
	    rad1 = MAGNITUDE(&val[6]);
	    rad2 = MAGNITUDE(n);
	    VUNITIZE(n);

	    /* Prevent illegal torii from floating point fuzz */
	    if (rad2 > rad1)  rad2 = rad1;

	    mk_tor(ofp, NAME, center, n, rad1, rad2);
	    break;

	case GENTGC:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    VSET(b, val[9], val[10], val[11]);
	    VSET(c, val[12], val[13], val[14]);
	    VSET(d, val[15], val[16], val[17]);

	    mk_tgc(ofp, NAME, center, height, a, b, c, d);
	    break;

	case GENELL:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(a, val[3], val[4], val[5]);
	    VSET(b, val[6], val[7], val[8]);
	    VSET(c, val[9], val[10], val[11]);

	    mk_ell(ofp, NAME, center, a, b, c);
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

	    mk_arb8(ofp, NAME, &pnts[0][X]);
	    break;

	case HALFSPACE:
	    VSET(norm, val[0], val[1], val[2]);
	    dd = val[3];

	    mk_half(ofp, NAME, norm, dd);
	    break;

	case RPC:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(breadth, val[6], val[7], val[8]);
	    dd = val[9];

	    mk_rpc(ofp, NAME, center, height, breadth, dd);
	    break;

	case RHC:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(breadth, val[6], val[7], val[8]);
	    rad1 = val[9];
	    dd = val[10];

	    mk_rhc(ofp, NAME, center, height, breadth, rad1, dd);
	    break;

	case EPA:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    VUNITIZE(a);
	    rad1 = val[9];
	    rad2 = val[10];

	    mk_epa(ofp, NAME, center, height, a, rad1, rad2);
	    break;

	case EHY:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    VUNITIZE(a);
	    rad1 = val[9];
	    rad2 = val[10];
	    dd = val[11];

	    mk_ehy(ofp, NAME, center, height, a, rad1, rad2, dd);
	    break;

	case HYP:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(height, val[3], val[4], val[5]);
	    VSET(a, val[6], val[7], val[8]);
	    rad1 = val[9];
	    rad2 = val[10];

	    mk_hyp(ofp, NAME, center, height, a, rad1, rad2);

	case ETO:
	    VSET(center, val[0], val[1], val[2]);
	    VSET(norm, val[3], val[4], val[5]);
	    VSET(c, val[6], val[7], val[8]);
	    rad1 = val[9];
	    rad2 = val[10];

	    mk_eto(ofp, NAME, center, norm, c, rad1, rad2);
	    break;

	default:
	    bu_log("asc2g: bad solid %s s_type= %d, skipping\n",
		   NAME, s_type);
    }

}


/**
 * M E M B B L D
 *
 * This routine invokes libwdb to build a member of a combination.
 * Called only from combbld()
 */
void
membbld(struct bu_list *headp)
{
    char 	*cp;
    char 	*np;
    int 	i;
    char		relation;	/* boolean operation */
    char		inst_name[NAME_LEN+2];
    struct wmember	*memb;

    cp = buf;
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
 * C O M B B L D
 *
 * This routine builds combinations.  It does so by processing the "C"
 * combination input line, (which may be followed by optional material
 * properties lines), and it then slurps up any following "M" member
 * lines, building up a linked list of all members.  Reading continues
 * until a non-"M" record is encountered.
 *
 * Returns -
 *	0	OK
 *	1	OK, another record exists in global input line buffer.
 */
int
combbld(void)
{
    struct bu_list	head;
    char 	*cp;
    char 	*np;
    int 		temp_nflag, temp_pflag;

    char		override;
    char		reg_flags;	/* region flag */
    int		is_reg;
    short		regionid;
    short		aircode;
    short		material;	/* GIFT material code */
    short		los;		/* LOS estimate */
    unsigned char	rgb[3];		/* Red, green, blue values */
    char		matname[32];	/* String of material name */
    char		matparm[60];	/* String of material parameters */
    char		inherit;	/* Inheritance property */

    /* Set all flags initially. */
    BU_LIST_INIT(&head);

    override = 0;
    temp_nflag = temp_pflag = 0;	/* indicators for optional fields */

    cp = buf;
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
    /* DEPRECTED: number of members expected */
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
	bu_fgets(buf, BUFSIZE, ifp);
	zap_nl();
	memset(matname, 0, sizeof(matname));
	bu_strlcpy(matname, buf, sizeof(matname));
    }
    if (temp_pflag) {
	bu_fgets(buf, BUFSIZE, ifp);
	zap_nl();
	memset(matparm, 0, sizeof(matparm));
	bu_strlcpy(matparm, buf, sizeof(matparm));
    }

    for (;;) {
	buf[0] = '\0';
	if (bu_fgets(buf, BUFSIZE, ifp) == (char *)0)
	    break;

	if (buf[0] != ID_MEMB)  break;

	/* Process (and accumulate) the members */
	membbld(&head);
    }

    /* Spit them out, all at once.  Use GIFT semantics. */
    if (mk_comb(ofp, NAME, &head, is_reg,
		temp_nflag ? matname : (char *)0,
		temp_pflag ? matparm : (char *)0,
		override ? (unsigned char *)rgb : (unsigned char *)0,
		regionid, aircode, material, los, inherit, 0, 1) < 0) {
	fprintf(stderr, "asc2g: mk_lrcomb fail\n");
	abort();
    }

    if (buf[0] == '\0')  return 0;
    return 1;
}


/**
 * A R S B L D
 *
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
    cp = buf;
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
 * A R S B L D
 *
 * This is the second half of the ARS-building.  It builds the ARS B
 * record.
 */
void
arsbbld(void)
{
    char *cp;
    int i;
    int incr_ret;

    cp = buf;
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

	incr_ret = incr_ars_pt();
	if (incr_ret == 2) {
	    /* finished, write out the ARS solid */
	    if (mk_ars(ofp, ars_name, ars_ncurves, ars_ptspercurve, ars_curves)) {
		bu_exit(1, "Failed trying to make ARS (%s)\n", ars_name);
	    }
	    return;
	} else if (incr_ret == 1) {
	    /* end of curve, ignore remainder of reocrd */
	    return;
	}
    }
}


/**
 * I D E N T B L D
 *
 * This routine makes an ident record.  It calls libwdb to do this.
 */
void
identbld(void)
{
    char	*cp;
    char	*np;
    char		units;		/* units code number */
    char		version[6] = {0};
    char		title[255] = {0};
    char		unit_str[8] = {0};
    double		local2mm;

    bu_strlcpy(unit_str, "none", sizeof(unit_str));

    cp = buf;
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

    (void)bu_fgets(buf, BUFSIZE, ifp);
    zap_nl();
    bu_strlcpy(title, buf, sizeof(title));

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

    if (mk_id_editunits(ofp, title, local2mm) < 0)
	bu_exit(2, "asc2g: unable to write database ID\n");
}


/**
 * P O L Y H B L D
 *
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
    char	*cp;
    char	*name;
    long	startpos;
    size_t	nlines;
    struct rt_pg_internal	*pg;
    struct rt_db_internal	intern;
    struct bn_tol	tol;

    (void)strtok(buf, " ");	/* skip the ident character */
    cp = strtok(NULL, " \n");
    name = bu_strdup(cp);

    /* Count up the number of poly data lines which follow */
    startpos = ftell(ifp);
    for (nlines = 0;; nlines++) {
	if (bu_fgets(buf, BUFSIZE, ifp) == NULL)  break;
	if (buf[0] != ID_P_DATA)  break;	/* 'Q' */
    }
    BU_ASSERT_LONG(nlines, >, 0);

    /* Allocate storage for the faces */
    BU_GETSTRUCT(pg, rt_pg_internal);
    pg->magic = RT_PG_INTERNAL_MAGIC;
    pg->npoly = nlines;
    pg->poly = (struct rt_pg_face_internal *)bu_calloc(pg->npoly,
						       sizeof(struct rt_pg_face_internal), "poly[]");
    pg->max_npts = 0;

    /* Return to first 'Q' record */
    fseek(ifp, startpos, 0);

    for (nlines = 0; nlines < pg->npoly; nlines++) {
	struct rt_pg_face_internal	*fp = &pg->poly[nlines];
	int	i;

	if (bu_fgets(buf, BUFSIZE, ifp) == NULL)  break;
	if (buf[0] != ID_P_DATA)  bu_exit(1, "mis-count of Q records?\n");

	/* Input always has 5 points, even if all aren't significant */
	fp->verts = (fastf_t *)bu_malloc(5*3*sizeof(fastf_t), "verts[]");
	fp->norms = (fastf_t *)bu_malloc(5*3*sizeof(fastf_t), "norms[]");

	cp = buf;
	cp++;				/* ident */
	cp = nxt_spc(cp);		/* skip the space */

	fp->npts = (char)atoi(cp);
	if (fp->npts > pg->max_npts)  pg->max_npts = fp->npts;

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
    intern.idb_meth = &rt_functab[ID_POLY];
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
    if (wdb_put_internal(ofp, name, &intern, mk_conv2mm) < 0)
	bu_exit(1, "Failed to create [%s] triangle mesh representation from polysolid object\n", name);
    /* BoT internal has been freed */
}


/**
 * M A T E R B L D
 *
 * Add information to the region-id based coloring table.
 */
void
materbld(void)
{
    char *cp;
    int	low, hi;
    int	r, g, b;

    cp = buf;
    cp++;			/* skip ID_MATERIAL */
    cp = nxt_spc(cp);		/* skip the space */

    /* flags = (char)atoi(cp); */
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


/**
 * C L I N E B L D
 *
 */
void
clinebld(void)
{
    char			my_name[NAME_LEN];
    fastf_t			thickness;
    fastf_t			radius;
    point_t			V;
    vect_t			height;
    char		*cp;
    char		*np;

    cp = buf;
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
    mk_cline(ofp, my_name, V, height, radius, thickness);
}


/**
 * B O T B L D
 *
 */
void
botbld(void)
{
    char			my_name[NAME_LEN];
    char			type;
    int				mode, orientation, error_mode;
    unsigned long int		num_vertices, num_faces;
    unsigned long int		i, j;
    double			a[3];
    fastf_t			*vertices;
    fastf_t			*thick=NULL;
    int				*faces;
    struct bu_bitv		*facemode=NULL;

    sscanf(buf, "%c %200s %d %d %d %lu %lu", &type, my_name, &mode, &orientation, /* NAME_LEN */
	   &error_mode, &num_vertices, &num_faces);

    /* get vertices */
    vertices = (fastf_t *)bu_calloc(num_vertices * 3, sizeof(fastf_t), "botbld: vertices");
    for (i=0; i<num_vertices; i++) {
	bu_fgets(buf, BUFSIZE, ifp);
	sscanf(buf, "%lu: %le %le %le", &j, &a[0], &a[1], &a[2]);
	if (i != j) {
	    bu_log("Vertices out of order in solid %s (expecting %d, found %d)\n",
		   my_name, i, j);
	    bu_free((char *)vertices, "botbld: vertices");
	    bu_log("Skipping this solid!\n");
	    while (buf[0] == '\t')
		bu_fgets(buf, BUFSIZE, ifp);
	    return;
	}
	VMOVE(&vertices[i*3], a);
    }

    /* get faces (and possibly thicknesses */
    faces = (int *)bu_calloc(num_faces * 3, sizeof(int), "botbld: faces");
    if (mode == RT_BOT_PLATE)
	thick = (fastf_t *)bu_calloc(num_faces, sizeof(fastf_t), "botbld thick");
    for (i=0; i<num_faces; i++) {
	bu_fgets(buf, BUFSIZE, ifp);
	if (mode == RT_BOT_PLATE)
	    sscanf(buf, "%lu: %d %d %d %le", &j, &faces[i*3], &faces[i*3+1], &faces[i*3+2], &a[0]);
	else
	    sscanf(buf, "%lu: %d %d %d", &j, &faces[i*3], &faces[i*3+1], &faces[i*3+2]);

	if (i != j) {
	    bu_log("Faces out of order in solid %s (expecting %d, found %d)\n",
		   my_name, i, j);
	    bu_free((char *)vertices, "botbld: vertices");
	    bu_free((char *)faces, "botbld: faces");
	    if (mode == RT_BOT_PLATE)
		bu_free((char *)thick, "botbld thick");
	    bu_log("Skipping this solid!\n");
	    while (buf[0] == '\t')
		bu_fgets(buf, BUFSIZE, ifp);
	    return;
	}

	if (mode == RT_BOT_PLATE)
	    thick[i] = a[0];
    }

    if (mode == RT_BOT_PLATE) {
	/* get bit vector */
	bu_fgets(buf, BUFSIZE, ifp);
	facemode = bu_hex_to_bitv(&buf[1]);
    }

    mk_bot(ofp, my_name, mode, orientation, 0, num_vertices, num_faces,
	   vertices, faces, thick, facemode);

    bu_free((char *)vertices, "botbld: vertices");
    bu_free((char *)faces, "botbld: faces");
    if (mode == RT_BOT_PLATE) {
	bu_free((char *)thick, "botbld thick");
	bu_free((char *)facemode, "botbld facemode");
    }
}


/**
 * P I P E B L D
 *
 * This routine reads pipe data from standard in, constructs a
 * doublely linked list of pipe points, and sends this list to
 * mk_pipe().
 */
void
pipebld(void)
{

    char			name[NAME_LEN];
    char		*cp;
    char		*np;
    struct wdb_pipept	*sp;
    struct bu_list		head;

    /* Process the first buffer */

    cp = buf;
    cp++;			/* ident, not used later */
    cp = nxt_spc(cp);		/* skip spaces */

    np = name;
    while (*cp != '\n' && *cp != '\r' && *cp != '\0') {
	*np++ = *cp++;
    }
    *np = '\0';			/* null terminate the string */


    /* Read data lines and process */

    BU_LIST_INIT(&head);
    bu_fgets(buf, BUFSIZE, ifp);
    while (strncmp (buf, "END_PIPE", 8)) {
	double id, od, x, y, z, bendradius;

	sp = (struct wdb_pipept *)bu_malloc(sizeof(struct wdb_pipept), "pipe");

	(void)sscanf(buf, "%le %le %le %le %le %le",
		     &id, &od,
		     &bendradius, &x, &y, &z);

	sp->l.magic = WDB_PIPESEG_MAGIC;

	sp->pp_id = id;
	sp->pp_od = od;
	sp->pp_bendradius = bendradius;
	VSET(sp->pp_coord, x, y, z);

	BU_LIST_INSERT(&head, &sp->l);
	bu_fgets(buf, BUFSIZE, ifp);
    }

    mk_pipe(ofp, name, &head);
    mk_pipe_free(&head);
}


/**
 * P A R T I C L E B L D
 *
 * This routine reads particle data from standard in, and constructs
 * the parameters required by mk_particle.
 */
void
particlebld(void)
{

    char		name[NAME_LEN];
    char		ident;
    point_t		vertex;
    vect_t		height;
    double		vrad;
    double		hrad;


    /* Read all the information out of the existing buffer.  Note that
     * particles fit into one granule.
     */

    (void)sscanf(buf, "%c %200s %le %le %le %le %le %le %le %le", /* NAME_LEN */
		 &ident, name,
		 &vertex[0],
		 &vertex[1],
		 &vertex[2],
		 &height[0],
		 &height[1],
		 &height[2],
		 &vrad, &hrad);

    mk_particle(ofp, name, vertex, height, vrad, hrad);
}


/**
 * A R B N B L D
 *
 * This routine reads arbn data from standard in and sendss it to
 * mk_arbn().
 */
void
arbnbld(void)
{

    char		name[NAME_LEN] = {0};
    char		type[TYPE_LEN] = {0};
    int		i;
    int		neqn;			/* number of eqn expected */
    plane_t		*eqn;		/* pointer to plane equations for faces */
    char	*cp;
    char	*np;

    /* Process the first buffer */

    cp = buf;
    cp++;				/* ident */
    cp = nxt_spc(cp);			/* skip spaces */

    np = name;
    while (*cp != ' ') {
	*np++ = *cp++;
    }
    *np = '\0';				/* null terminate the string */

    cp = nxt_spc(cp);

    neqn = atoi(cp);			/* find number of eqns */
    /*bu_log("neqn = %d\n", neqn);
     */
    /* Check to make sure plane equations actually came in. */
    if (neqn <= 0) {
	bu_log("asc2g: warning: %d equations counted for arbn %s\n", neqn, name);
    }

    /*bu_log("mallocing space for eqns\n");
     */
    /* Malloc space for the in-coming plane equations */
    eqn = (plane_t *)bu_malloc(sizeof(plane_t) * neqn, "eqn");

    /* Now, read the plane equations and put in appropriate place */

    /*bu_log("starting to dump eqns\n");
     */
    for (i = 0; i < neqn; i++) {
	bu_fgets(buf, BUFSIZE, ifp);
	(void)sscanf(buf, "%200s %le %le %le %le", type, /* TYPE_LEN */
		     &eqn[i][X], &eqn[i][Y], &eqn[i][Z], &eqn[i][W]);
    }

    /*bu_log("sending info to mk_arbn\n");
     */
    mk_arbn(ofp, name, neqn, eqn);
}
/**
 * E N D S W I T H
 *
 * This routine checks the last character in the string to see if it matches the
 * specified character. Used by gettclblock() to check for an escaped return.
 *
 */
int
endswith(char *line, char ch)
{
    if ( *(line+strlen(line)-1) == ch ) {
	return 1;
    }
    return 0;
}
/**
 * B R A C E C N T
 *
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
    while(*start != '\0') {
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
 * G E T T C L B L O C K
 *
 * This routine reads the next block of Tcl commands. This block is expected to be a Tcl
 * command script and will be fed to an interpreter using Tcl_Eval(). Any escaped returns
 * or open braces are parsed through and concatenated ensuring Tcl commands are complete.
 * 
 *  SIZE is used as the approximate blocking size allowing to grow past this to close the
 * command line.
 */
int
gettclblock(struct bu_vls *line, FILE *fp)
{
    int ret = 0;
    struct bu_vls tmp;
    int bcnt = 0;
    int escapedcr = 0;

    bu_vls_init(&tmp);

    if ((ret=bu_vls_gets(line, fp)) >= 0) {
        linecnt++;
	escapedcr = endswith(bu_vls_addr(line),'\\');
        bcnt = bracecnt(bu_vls_addr(line));
        while ( (ret >= 0) && ((bu_vls_strlen(line) < SIZE) || (escapedcr) || ( bcnt != 0 )) ) {
	    linecnt++;
	    if (escapedcr) {
		bu_vls_trunc(line, bu_vls_strlen(line)-1);
	    }
            if ((ret=bu_vls_gets(&tmp, fp)) > 0) {
		escapedcr = endswith(bu_vls_addr(&tmp),'\\');
		bcnt = bcnt + bracecnt(bu_vls_addr(&tmp));
		bu_vls_putc(line, '\n');
		bu_vls_strcat(line,bu_vls_addr(&tmp));
		bu_vls_trunc(&tmp,0);
	    } else {
		escapedcr = 0;
	    }
        }
	ret = bu_vls_strlen(line);
    }
    bu_vls_free(&tmp);

    return ret;
}

/**
 * M A I N
 */
int
main(int argc, char *argv[])
{
    struct bu_vls       str_title;
    struct bu_vls       str_put;
    struct bu_vls	line;
    int                 isComment=1;

    bu_debug = BU_DEBUG_COREDUMP;

    if (argc != 3)
	bu_exit(1, "%s", usage);

    Tcl_FindExecutable(argv[0]);

    ifp = fopen(argv[1], "rb");
    if (!ifp)  perror(argv[1]);

    ofp = wdb_fopen(argv[2]);
    if (!ofp)  perror(argv[2]);
    if (ifp == NULL || ofp == NULL) {
	bu_exit(1, "asc2g: can't open files.");
    }

    rt_init_resource(&rt_uniresource, 0, NULL);

    bu_vls_init(&line);
    bu_vls_extend( &line, SIZE);
    bu_vls_init(&str_title);
    bu_vls_strcpy( &str_title, "title");
    bu_vls_init(&str_put);
    bu_vls_strcpy( &str_put, "put ");

    while (isComment) {
        char *str;
        int charIndex;
        int len;
        bu_vls_trunc2(&line, 0);
        if (bu_vls_gets(&line, ifp) < 0) {
            fclose(ifp); ifp = NULL;
            wdb_close(ofp); ofp = NULL;
            bu_exit(1, "Unexpected EOF\n");
        }
        str = bu_vls_addr(&line);
        len = strlen(str);
        for (charIndex=0 ; charIndex<len ; charIndex++) {
            if (str[charIndex] == '#') {
                isComment = 1;
                break;
            } else if (isspace(str[charIndex])) {
                continue;
            } else {
                isComment = 0;
                break;
            }
        }
    }

    /* new style ascii database */
    if (!bu_vls_strncmp( &line, &str_title, 5) || !bu_vls_strncmp( &line, &str_put, 4)) {
	Tcl_Interp     *interp;
	Tcl_Interp     *safe_interp;

	/* this is a Tcl script */

        rewind(ifp);
        bu_vls_trunc( &line, 0);
	BU_LIST_INIT(&rt_g.rtg_headwdb.l);

	interp = Tcl_CreateInterp();
	Go_Init(interp);
	wdb_close(ofp);

	{
	    int ac = 4;
	    const char *av[5];

	    av[0] = "to_open";
	    av[1] = db_name;
	    av[2] = "db";
	    av[3] = argv[2];
	    av[4] = (char *)0;

	    if (to_open_tcl((ClientData)0, interp, ac, av) != TCL_OK) {
		fclose(ifp);
		bu_log("Failed to initialize tclcad_obj!\n");
		Tcl_Exit(1);
	    }
	}

	/* Create the safe interpreter */
	if ((safe_interp = Tcl_CreateSlave(interp, slave_name, 1)) == NULL) {
	    fclose(ifp);
	    bu_log("Failed to create safe interpreter");
	    Tcl_Exit(1);
	}

	/* Create aliases */
	{
	    int	i;
	    int	ac = 1;
	    const char	*av[2];

	    av[1] = (char *)0;
	    for (i = 0; aliases[i] != (char *)0; ++i) {
		av[0] = aliases[i];
		Tcl_CreateAlias(safe_interp, aliases[i], interp, db_name, ac, av);
	    }
	    /* add "dbfind" separately */
	    av[0] = "find";
	    Tcl_CreateAlias(safe_interp, "dbfind", interp, db_name, ac, av);
	}

        while ((gettclblock(&line,ifp)) >= 0)
        {
	    if (Tcl_Eval(safe_interp, (const char *)bu_vls_addr(&line)) != TCL_OK) {
		fclose(ifp);
	        bu_log("Failed to process input file (%s)!\n", argv[1]);
	        bu_log("%s\n", Tcl_GetStringResult(safe_interp));
	        Tcl_Exit(1);
	    }            
            bu_vls_trunc(&line,0);
        }

	/* free up our resources */
        bu_vls_free(&line);
        bu_vls_free(&str_title);
        bu_vls_free(&str_put);

	fclose(ifp);

	Tcl_Exit(0);
    } else {
        bu_vls_free(&line);
        bu_vls_free(&str_title);
        bu_vls_free(&str_put);
	rewind(ifp);
    }

    /* allocate our input buffer */
    buf = (char *)bu_calloc(sizeof(char), BUFSIZE, "input buffer");

    /* Read ASCII input file, each record on a line */
    while ((bu_fgets(buf, BUFSIZE, ifp)) != (char *)0) {

    after_read:
	/* Clear the output record -- vital! */
	(void)memset((char *)&record, 0, sizeof(record));

	/* Check record type */
	switch (buf[0]) {
	    case ID_SOLID:
		solbld();
		continue;

	    case ID_COMB:
		if (combbld() > 0)  goto after_read;
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
		bu_log("asc2g: bad record type '%c' (0%o), skipping\n", buf[0], buf[0]);
		bu_log("%s\n", buf);
		continue;
	}
	memset(buf, 0, sizeof(char) * BUFSIZE);
    }

    /* Now, at the end of the database, dump out the entire
     * region-id-based color table.
     */
    mk_write_color_table(ofp);

    /* close up shop */
    bu_free(buf, "input buffer");
    buf = NULL; /* sanity */
    fclose(ifp); ifp = NULL;
    wdb_close(ofp); ofp = NULL;

    bu_exit(0, "");
    return 0;
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
