/*                            D O . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2014 United States Government as represented by
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
/** @file rt/do.c
 *
 * Routines that process the various commands, and manage the overall
 * process of running the raytracing process.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bu/getopt.h"
#include "bu/debug.h"
#include "bu/mime.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "icv.h"

#include "./rtuif.h"
#include "./ext.h"

#include "scanline.h"


/***** Variables shared with viewing model *** */
extern FILE *outfp;			/* optional pixel output file */
extern mat_t view2model;
extern mat_t model2view;
/***** end of sharing with viewing model *****/

extern void grid_setup(void);
extern void worker(int cpu, void *arg);

/***** variables shared with opt.c *****/
extern int	orientflag;		/* 1 means orientation has been set */
/***** end variables shared with opt.c *****/

/***** variables shared with rt.c *****/
extern char *string_pix_start;	/* string spec of starting pixel */
extern char *string_pix_end;	/* string spec of ending pixel */
extern int finalframe;		/* frame to halt at */
/***** end variables shared with rt.c *****/

/***** variables shared with viewg3.c *****/
struct bu_vls ray_data_file = BU_VLS_INIT_ZERO;  /* file name for ray data output */
/***** end variables shared with viewg3.c *****/

/***** variables for frame buffer black pixel rendering *****/
unsigned char *pixmap = NULL; /* Pixel Map for rerendering of black pixels */


void def_tree(register struct rt_i *rtip);
void do_ae(double azim, double elev);
void res_pr(void);
void memory_summary(void);

extern struct icv_image *bif;


/**
 * Acquire particulars about a frame, in the old format.  Returns -1
 * if unable to acquire info, 0 if successful.
 */
int
old_frame(FILE *fp)
{
    register int i;
    char number[128];

    /* Visible part is from -1 to +1 in view space */
    if (fscanf(fp, "%128s", number) != 1) return -1;
    viewsize = atof(number);
    if (fscanf(fp, "%128s", number) != 1) return -1;
    eye_model[X] = atof(number);
    if (fscanf(fp, "%128s", number) != 1) return -1;
    eye_model[Y] = atof(number);
    if (fscanf(fp, "%128s", number) != 1) return -1;
    eye_model[Z] = atof(number);
    for (i = 0; i < 16; i++) {
	if (fscanf(fp, "%128s", number) != 1)
	    return -1;
	Viewrotscale[i] = atof(number);
    }
    return 0;		/* OK */
}


/**
 * Determine if input file is old or new format, and if old format,
 * handle process.  Returns 0 if new way, 1 if old way (and all done).
 * Note that the rewind() will fail on ttys, pipes, and sockets
 * (sigh).
 */
int
old_way(FILE *fp)
{
    int c;

    viewsize = -42.0;

    /* Sneak a peek at the first character, and then put it back */
    if ((c = fgetc(fp)) == EOF) {
	/* Claim old way, all (i.e., nothing) done */
	return 1;
    }
    if (ungetc(c, fp) != c)
	bu_exit(EXIT_FAILURE, "do.c:old_way() ungetc failure\n");

    /*
     * Old format files start immediately with a %.9e format, so the
     * very first character should be a digit or '-'.
     */
    if ((c < '0' || c > '9') && c != '-') {
	return 0;		/* Not old way */
    }

    if (old_frame(fp) < 0 || viewsize <= 0.0) {
	rewind(fp);
	return 0;		/* Not old way */
    }
    bu_log("Interpreting command stream in old format\n");

    def_tree(APP.a_rt_i);	/* Load the default trees */

    curframe = 0;
    do {
	if (finalframe >= 0 && curframe > finalframe)
	    return 1;
	if (curframe >= desiredframe)
	    do_frame(curframe);
	curframe++;
    }  while (old_frame(fp) >= 0 && viewsize > 0.0);
    return 1;			/* old way, all done */
}


/**
 * Process "start" command in new format input stream
 */
int cm_start(const int argc, const char **argv)
{
    char *buf = (char *)NULL;
    int frame;

    if (argc < 2)
	return 1;

    frame = atoi(argv[1]);
    if (finalframe >= 0 && frame > finalframe)
	return -1;	/* Indicate EOF -- user declared a halt */
    if (frame >= desiredframe) {
	curframe = frame;
	return 0;
    }

    /* Skip over unwanted frames -- find next frame start */
    while ((buf = rt_read_cmd(stdin)) != (char *)0) {
	register char *cp;

	cp = buf;
	while (*cp && isspace((int)*cp)) cp++;	/* skip spaces */
	if (bu_strncmp(cp, "start", 5) != 0) continue;
	while (*cp && !isspace((int)*cp)) cp++;	/* skip keyword */
	while (*cp && isspace((int)*cp)) cp++;	/* skip spaces */
	frame = atoi(cp);
	bu_free(buf, "rt_read_cmd command buffer (skipping frames)");
	buf = (char *)0;
	if (finalframe >= 0 && frame > finalframe)
	    return -1;			/* "EOF" */
	if (frame >= desiredframe) {
	    curframe = frame;
	    return 0;
	}
    }
    return -1;		/* EOF */
}


int cm_vsize(const int argc, const char **argv)
{
    if (argc < 2)
	return 1;

    viewsize = atof(argv[1]);
    return 0;
}


int cm_eyept(const int argc, const char **argv)
{
    register int i;

    if (argc < 2)
	return 1;

    for (i = 0; i < 3; i++)
	eye_model[i] = atof(argv[i+1]);
    return 0;
}


int cm_lookat_pt(const int argc, const char **argv)
{
    point_t pt;
    vect_t dir;
    int yflip = 0;
    quat_t quat;

    if (argc < 4)
	return -1;
    pt[X] = atof(argv[1]);
    pt[Y] = atof(argv[2]);
    pt[Z] = atof(argv[3]);
    if (argc > 4)
	yflip = atoi(argv[4]);

    /*
     * eye_pt should have been specified before here and different
     * from the lookat point or the lookat point will be from the
     * "front"
     */
    if (VNEAR_EQUAL(pt, eye_model, VDIVIDE_TOL)) {
	VSETALLN(quat, 0.5, 4);
	quat_quat2mat(Viewrotscale, quat); /* front */
    } else {
	VSUB2(dir, pt, eye_model);
	VUNITIZE(dir);
	bn_mat_lookat(Viewrotscale, dir, yflip);
    }

    return 0;
}


int cm_vrot(const int argc, const char **argv)
{
    register int i;

    if (argc < 16)
	return 1;

    for (i = 0; i < 16; i++)
	Viewrotscale[i] = atof(argv[i+1]);
    return 0;
}


int cm_orientation(const int argc, const char **argv)
{
    register int i;
    quat_t quat;

    if (argc < 4)
	return 1;

    for (i = 0; i < 4; i++)
	quat[i] = atof(argv[i+1]);
    quat_quat2mat(Viewrotscale, quat);
    orientflag = 1;
    return 0;
}


int cm_end(const int UNUSED(argc), const char **UNUSED(argv))
{
    struct rt_i *rtip = APP.a_rt_i;

    if (rtip && BU_LIST_IS_EMPTY(&rtip->HeadRegion)) {
	def_tree(rtip);		/* Load the default trees */
    }

    /* If no matrix or az/el specified yet, use params from cmd line */
    if (Viewrotscale[15] <= 0.0)
	do_ae(azimuth, elevation);

    if (do_frame(curframe) < 0) return -1;
    return 0;
}


int cm_tree(const int argc, const char **argv)
{
    register struct rt_i *rtip = APP.a_rt_i;
    struct bu_vls times = BU_VLS_INIT_ZERO;

    if (argc <= 1) {
	def_tree(rtip);		/* Load the default trees */
	return 0;
    }

    rt_prep_timer();
    if (rt_gettrees(rtip, argc-1, &argv[1], npsw) < 0)
	bu_log("rt_gettrees(%s) FAILED\n", argv[0]);
    (void)rt_get_timer(&times, NULL);

    if (rt_verbosity & VERBOSE_STATS)
	bu_log("GETTREE: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    return 0;
}


int cm_multiview(const int UNUSED(argc), const char **UNUSED(argv))
{
    register struct rt_i *rtip = APP.a_rt_i;
    size_t i;
    static int a[] = {
	35,   0,
	0,  90, 135, 180, 225, 270, 315,
	0,  90, 135, 180, 225, 270, 315
    };
    static int e[] = {
	25, 90,
	30, 30, 30, 30, 30, 30, 30,
	60, 60, 60, 60, 60, 60, 60
    };

    if (rtip && BU_LIST_IS_EMPTY(&rtip->HeadRegion)) {
	def_tree(rtip);		/* Load the default trees */
    }
    for (i = 0; i < (sizeof(a)/sizeof(a[0])); i++) {
	do_ae((double)a[i], (double)e[i]);
	(void)do_frame(curframe++);
    }
    return -1;	/* end RT by returning an error */
}


/**
 * Experimental animation code
 *
 * Usage: anim path type args
 */
int cm_anim(const int argc, const char **argv)
{

    if (db_parse_anim(APP.a_rt_i->rti_dbip, argc, argv) < 0) {
	bu_log("cm_anim:  %s %s failed\n", argv[1], argv[2]);
	return -1;		/* BAD */
    }
    return 0;
}


/**
 * Clean out results of last rt_prep(), and start anew.
 */
int cm_clean(const int UNUSED(argc), const char **UNUSED(argv))
{
    /* Allow lighting model clean up (e.g. lights, materials, etc.) */
    view_cleanup(APP.a_rt_i);

    rt_clean(APP.a_rt_i);

    if (R_DEBUG&RDEBUG_RTMEM_END)
	bu_prmem("After cm_clean");
    return 0;
}


/**
 * To be invoked after a "clean" command, to close out the ".g"
 * database.  Intended for memory debugging, to help chase down memory
 * "leaks".  This terminates the program, as there is no longer a
 * database.
 */
int cm_closedb(const int UNUSED(argc), const char **UNUSED(argv))
{
    db_close(APP.a_rt_i->rti_dbip);
    APP.a_rt_i->rti_dbip = DBI_NULL;

    bu_free((void *)APP.a_rt_i, "struct rt_i");
    APP.a_rt_i = RTI_NULL;

    bu_prmem("After _closedb");
    bu_exit(0, NULL);

    return 1;	/* for compiler */
}


/* viewing module specific variables */
extern struct bu_structparse view_parse[];

struct bu_structparse set_parse[] = {
    {"%d",	1, "width",			bu_byteoffset(width),			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "height",			bu_byteoffset(height),			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "save_overlaps",		bu_byteoffset(save_overlaps),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "perspective",		bu_byteoffset(rt_perspective),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "angle",			bu_byteoffset(rt_perspective),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
#if !defined(_WIN32) || defined(__CYGWIN__)
    /* FIXME: these cannot be listed in here because they are LIBRT
     * globals.  due to the way symbols are not imported until a DLL
     * is loaded on Windows, the byteoffset address of the global is
     * not known at compile-time.  they would needed to be added to
     * set_parse() during runtime initialization.
     */
    {"%d",	1, "rt_bot_minpieces",		bu_byteoffset(rt_bot_minpieces),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "rt_bot_tri_per_piece",	bu_byteoffset(rt_bot_tri_per_piece),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "rt_cline_radius",		bu_byteoffset(rt_cline_radius),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
#endif
    {"%V",	1, "ray_data_file",		bu_byteoffset(ray_data_file),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    /* daisy-chain to additional app-specific parameters */
    {"%p",	1, "Application-Specific Parameters", bu_byteoffset(view_parse[0]),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,						BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 * Allow variable values to be set or examined.
 */
int cm_set(const int argc, const char **argv)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (argc <= 1) {
	bu_struct_print("Generic and Application-Specific Parameter Values",
			set_parse, (char *)0);
	return 0;
    }

    bu_vls_from_argv(&str, argc-1, (const char **)argv+1);
    if (bu_struct_parse(&str, set_parse, (char *)0, NULL) < 0) {
	bu_vls_free(&str);
	return -1;
    }
    bu_vls_free(&str);
    return 0;
}


int cm_ae(const int argc, const char **argv)
{
    if (argc < 3)
	return 1;

    azimuth = atof(argv[1]);	/* set elevation and azimuth */
    elevation = atof(argv[2]);
    do_ae(azimuth, elevation);

    return 0;
}


int cm_opt(const int argc, const char **argv)
{
    int old_bu_optind=bu_optind;	/* need to restore this value after calling get_args() */

    if (get_args(argc, argv) <= 0) {
	bu_optind = old_bu_optind;
	return -1;
    }
    bu_optind = old_bu_optind;
    return 0;
}


/**
 * Load default tree list, from command line.
 */
void
def_tree(register struct rt_i *rtip)
{
    struct bu_vls times = BU_VLS_INIT_ZERO;

    RT_CK_RTI(rtip);

    rt_prep_timer();
    if (rt_gettrees(rtip, nobjs, (const char **)objtab, npsw) < 0) {
	bu_log("rt_gettrees(%s) FAILED\n", (objtab && objtab[0]) ? objtab[0] : "ERROR");
    }
    (void)rt_get_timer(&times, NULL);

    if (rt_verbosity & VERBOSE_STATS)
	bu_log("GETTREE: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    memory_summary();
}



/*********************************************************************************/
static point_t lo, hi;

static vect_t size, isize;
static int M[3];

static cl_uint *C;
static cl_uint *L;
static struct soltab **S;
static cl_uint NS;

static int ibackground[3] = {0, 0, 50};         /* integer 0..255 version */
static int inonbackground[3] = {0, 0, 51};	/* integer non-background */
static size_t pwidth = 3;                       /* Width of each pixel (in bytes) */
extern fb *fbp;                                 /* Framebuffer handle */
static struct scanline scanline;


static inline
int simple_index(const int x, const int y, const int z)
{
    return (((M[Y] * z) + y) * M[X]) + x;
}

struct bboxi {
    cl_uint min[3], max[3];
};

#define MY_CLAMP(x, min, max)   ((x < min) ? min : ((x > max) ? max : x))
#define MY_SIGN(x)              ((x < 0) ? -1 : 1)

static void
alloc_grid(const struct boxnode *fromp)
{
    const fastf_t density = 4.0;
    fastf_t V, factor;
    int N;

    ssize_t i, j;
    cl_uint x, y, z;
    struct bboxi *B;

    NS = fromp->bn_len;
    VMOVE(lo, fromp->bn_min);
    VMOVE(hi, fromp->bn_max);

    VSUB2(size, hi, lo);

    V = size[X]*size[Y]*size[Z];
    factor = pow((density * (fastf_t)fromp->bn_len) / V, (1.0/3.0));

    for (i=0; i<3; i++) {
        M[i] = (int)(factor * size[i] + 0.5);
    }

    if (M[X] == 1 && M[Y] == 1 && M[Z] == 1) {
        return;
    }

    VELDIV(size, size, M);

    VSETALL(isize, 1.0);
    VELDIV(isize, isize, size);

    N = M[X] * M[Y] * M[Z];


    /* TODO: do a deep copy here in case it ain't safe. */
    S = fromp->bn_list;

    clt_db_store(NS, S);
    
    printf("%dx%dx%d\n", M[X], M[Y], M[Z]);

    B = (struct bboxi *)bu_calloc(fromp->bn_len, sizeof(struct bboxi), "B");

    for (i=0; i<(ssize_t)fromp->bn_len; ++i) {
        const struct soltab *stp = fromp->bn_list[i];
        point_t min, max;
        struct bboxi boundi;
        cl_uint k;

        VSUB2(min, stp->st_min, lo);
        VELMUL(min, min, isize);

        VSUB2(max, stp->st_max, lo);
        VELMUL(max, max, isize);

        for (k=0; k<3; ++k) {
            boundi.max[k] = MY_CLAMP((int)max[k], 0, M[k]-1);
            boundi.min[k] = MY_CLAMP((int)min[k], 0, M[k]-1);
        }
        B[i] = boundi;
    }



    C = (cl_uint *)bu_calloc(N+1, sizeof(cl_uint), "C");
    
    for (i=0; i<=N; ++i) {
        C[i] = 0;
    }
    
    /* insert objects in the cells that intersect the object's bounding box. */
    for (j=0; j<(ssize_t)fromp->bn_len; ++j) {
        struct bboxi boundi = B[j];

        for (z=boundi.min[2]; z<=boundi.max[2]; ++z) {
            for (y=boundi.min[1]; y<=boundi.max[1]; ++y) {
                for (x=boundi.min[0]; x<=boundi.max[0]; ++x) {
                    cl_uint ii = simple_index(x, y, z);
                    C[ii]++;
                }
            }
        }
    }

    for (i=1; i<=N; ++i) {
        C[i] += C[i-1];
    }

    L = (cl_uint *)bu_calloc(C[N-1], sizeof(cl_uint), "L");

    for (i=(ssize_t)fromp->bn_len-1; i>=0; --i) {
        /* for each cell j overlapped by object i. */
        struct bboxi boundi = B[i];

        for (z=boundi.min[2]; z<=boundi.max[2]; ++z) {
            for (y=boundi.min[1]; y<=boundi.max[1]; ++y) {
                for (x=boundi.min[0]; x<=boundi.max[0]; ++x) {
                    cl_uint jj = simple_index(x, y, z);
                    L[--C[jj]] = i;
                }
            }
        }
    }

    bu_free((struct bboxi *)B, "B");
}
/*
static void
free_grid(void)
{
    bu_free((cl_uint *)C, "C");
    bu_free((cl_uint *)L, "L");
}
*/

void
simple_prep(struct rt_i *rtip)
{
    struct soltab *stp;
    union cutter *finp;	/* holds the finite solids */

    /* Make a list of all solids into one special boxnode, then refine. */
    BU_ALLOC(finp, union cutter);
    finp->cut_type = CUT_BOXNODE;
    VMOVE(finp->bn.bn_min, rtip->mdl_min);
    VMOVE(finp->bn.bn_max, rtip->mdl_max);
    finp->bn.bn_len = 0;
    finp->bn.bn_maxlen = rtip->nsolids+1;
    finp->bn.bn_list = (struct soltab **)bu_calloc(
        finp->bn.bn_maxlen, sizeof(struct soltab *),
        "do_prep: initial list alloc");

    rtip->rti_inf_box.cut_type = CUT_BOXNODE;

    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
        /* Ignore "dead" solids in the list.  (They failed prep) */
        if (stp->st_aradius <= 0) continue;

        /* Infinite and finite solids all get lumped together */
        rt_cut_extend(finp, stp, rtip);

        if (stp->st_aradius >= INFINITY) {
            /* Also add infinite solids to a special BOXNODE */
            rt_cut_extend(&rtip->rti_inf_box, stp, rtip);
        }
    } RT_VISIT_ALL_SOLTABS_END;

    alloc_grid(&finp->bn);
    
    bu_free(finp, "union cutter");
}

int
clt_accelerated(const struct soltab *stp)
{
    switch (stp->st_id) {
	case ID_TOR:
	case ID_TGC:
	case ID_ELL:
	case ID_ARB8:
	case ID_SPH:
	case ID_EHY:
	    return 1;
	default:
	    return 0;
    }
}

int
simple_shootray(struct resource *resp, struct xray *a_ray, struct rt_i *rtip, register struct application *ap, struct partition *pFinalPart, struct seg *pfinished_segs)
{
    struct rt_shootray_status ss;
    struct seg new_segs;	/* from solid intersections */
    struct seg waiting_segs;	/* awaiting rt_boolweave() */
    fastf_t last_bool_start;
    struct bu_bitv *solidbits;	/* bits for all solids shot so far */
    struct bu_bitv *backbits=NULL;	/* bits for all solids using pieces that need to be intersected behind
					   the ray start point */
    struct bu_ptbl *regionbits;	/* table of all involved regions */
    struct partition InitialPart;	/* Head of Initial Partitions */
    fastf_t pending_hit = 0; /* dist of closest odd hit pending */

    ss.ap = ap;
    ss.resp = resp;

    InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
    InitialPart.pt_magic = PT_HD_MAGIC;

    BU_LIST_INIT(&new_segs.l);
    BU_LIST_INIT(&waiting_segs.l);

    if (!BU_LIST_IS_INITIALIZED(&resp->re_parthead)) {
	/*
	 * We've been handed a mostly un-initialized resource struct,
	 * with only a magic number and a cpu number filled in.  Init
	 * it and add it to the table.  This is how
	 * application-provided resource structures are remembered for
	 * later cleanup by the library.
	 */
	rt_init_resource(resp, resp->re_cpu, rtip);
    }

    solidbits = rt_get_solidbitv(rtip->nsolids, resp);

    if (BU_LIST_IS_EMPTY(&resp->re_region_ptbl)) {
	BU_ALLOC(regionbits, struct bu_ptbl);
	bu_ptbl_init(regionbits, 7, "rt_shootray() regionbits ptbl");
    } else {
	regionbits = BU_LIST_FIRST(bu_ptbl, &resp->re_region_ptbl);
	BU_LIST_DEQUEUE(&regionbits->l);
	BU_CK_PTBL(regionbits);
    }

    if (!resp->re_pieces && rtip->rti_nsolids_with_pieces > 0) {
	/* Initialize this processors 'solid pieces' state */
	rt_res_pieces_init(resp, rtip);
    }
    if (UNLIKELY(!BU_LIST_MAGIC_EQUAL(&resp->re_pieces_pending.l, BU_PTBL_MAGIC))) {
	/* only happens first time through */
	bu_ptbl_init(&resp->re_pieces_pending, 100, "re_pieces_pending");
    }
    bu_ptbl_reset(&resp->re_pieces_pending);

    /*
     * Record essential statistics in per-processor data structure.
     */
    resp->re_nshootray++;

    /* Compute the inverse of the direction cosines */
    if (a_ray->r_dir[X] < -SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[X] = -(ss.inv_dir[X]=1.0/a_ray->r_dir[X]);
	ss.rstep[X] = -1;
    } else if (a_ray->r_dir[X] > SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[X] =  (ss.inv_dir[X]=1.0/a_ray->r_dir[X]);
	ss.rstep[X] = 1;
    } else {
	a_ray->r_dir[X] = 0.0;
	ss.abs_inv_dir[X] = ss.inv_dir[X] = INFINITY;
	ss.rstep[X] = 0;
    }
    if (a_ray->r_dir[Y] < -SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Y] = -(ss.inv_dir[Y]=1.0/a_ray->r_dir[Y]);
	ss.rstep[Y] = -1;
    } else if (a_ray->r_dir[Y] > SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Y] =  (ss.inv_dir[Y]=1.0/a_ray->r_dir[Y]);
	ss.rstep[Y] = 1;
    } else {
	a_ray->r_dir[Y] = 0.0;
	ss.abs_inv_dir[Y] = ss.inv_dir[Y] = INFINITY;
	ss.rstep[Y] = 0;
    }
    if (a_ray->r_dir[Z] < -SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Z] = -(ss.inv_dir[Z]=1.0/a_ray->r_dir[Z]);
	ss.rstep[Z] = -1;
    } else if (a_ray->r_dir[Z] > SQRT_SMALL_FASTF) {
	ss.abs_inv_dir[Z] =  (ss.inv_dir[Z]=1.0/a_ray->r_dir[Z]);
	ss.rstep[Z] = 1;
    } else {
	a_ray->r_dir[Z] = 0.0;
	ss.abs_inv_dir[Z] = ss.inv_dir[Z] = INFINITY;
	ss.rstep[Z] = 0;
    }
    VMOVE(ap->a_inv_dir, ss.inv_dir);

    /*
     * If ray does not enter the model RPP, skip on.  If ray ends
     * exactly at the model RPP, trace it.
     */
    if (!rt_in_rpp(a_ray, ss.inv_dir, rtip->mdl_min, rtip->mdl_max)  ||
	a_ray->r_max < 0.0) {
        const union cutter *cutp;
	cutp = &rtip->rti_inf_box;
	if (cutp->bn.bn_len == 0) {
            resp->re_nmiss_model++;
            if (ap->a_miss)
                ap->a_return = ap->a_miss(ap);
            else
                ap->a_return = 0;
            goto out;
        }
    }

    {
        vect_t d; /* ray distance from axis. */
        vect_t ddist; /* ray distance between sub-voxel faces. */
        signed char rd[3]; /* {-1, 1} depends on ray direction. */
        int id[3]; /* current voxel index. */
        int ip[3]; /* array index increment. */
        int i; /* current voxel index into 3D array. */
        point_t p; /* voxel intersection point. */

        ss.newray = *a_ray;
        ss.dist_corr = 0.0;

        VJOIN1(p, ss.newray.r_pt, ss.newray.r_min, ss.newray.r_dir);
        VSUB2(p, p, rtip->mdl_min);

        for (i=0; i<3; i++) {
            d[i] = p[i]*isize[i];

            id[i] = MY_CLAMP((int)d[i], 0, M[i]-1);
            rd[i] = MY_SIGN(ss.newray.r_dir[i]);
            d[i] -= id[i];

            if (rd[i] > 0)  d[i] = 1.0f - d[i];
        }

        VELMUL(ddist, size, ss.abs_inv_dir);
        VELMUL(d, d, ddist);

        ip[X] = rd[X];
        ip[Y] = M[X]*rd[Y];
        ip[Z] = M[Y]*M[X]*rd[Z];

        i = simple_index(V3ARGS(id));

        for (;;) {
            int j;

            if (C[i] < C[i+1] && ss.box_end >= BACKING_DIST) {
                unsigned k;

                for (k = C[i]; k < C[i+1]; ++k) {
                    struct soltab *stp;
                    int ret;
                    
                    stp = S[L[k]];

                    if (BU_BITTEST(solidbits, stp->st_bit)) {
                        resp->re_ndup++;
                        continue;	/* already shot */
                    }

                    /* Shoot a ray */
                    BU_BITSET(solidbits, stp->st_bit);

    		    resp->re_shots++;
                    BU_LIST_INIT(&(new_segs.l));

                    ret = -1;

		    if (clt_accelerated(stp)) {
			struct seg *seghead = &new_segs;
			int ii;
			struct cl_hit hits[4];

			ret = clt_db_solid_shot(sizeof(hits), hits, &ss.newray, L[k]);
			for (ii = 0; ii < ret; ii += 2) {
			    struct seg *segp;
			    RT_GET_SEG(segp, ap->a_resource);
			    segp->seg_stp = stp;
			    segp->seg_in.hit_dist = hits[ii].hit_dist;
			    segp->seg_in.hit_surfno = hits[ii].hit_surfno;
			    segp->seg_out.hit_dist = hits[ii+1].hit_dist;
			    segp->seg_out.hit_surfno = hits[ii+1].hit_surfno;
			    /* Set aside vector for rt_tor_norm() later */
			    VMOVE(segp->seg_in.hit_vpriv, hits[ii].hit_vpriv.s);
			    VMOVE(segp->seg_out.hit_vpriv, hits[ii+1].hit_vpriv.s);
			    BU_LIST_INSERT(&(seghead->l), &(segp->l));
			}
		    } else if (stp->st_meth->ft_shot) {
			ret = stp->st_meth->ft_shot(stp, &ss.newray, ap, &new_segs);
		    }

                    if (ret <= 0) {
                        resp->re_shot_miss++;
                        continue;	/* MISS */
                    }
                    {
                        register struct seg *s2;
                        while (BU_LIST_WHILE(s2, seg, &(new_segs.l))) {
                            BU_LIST_DEQUEUE(&(s2->l));
                            /* Restore to original distance */
                            s2->seg_in.hit_dist += ss.dist_corr;
                            s2->seg_out.hit_dist += ss.dist_corr;
                            s2->seg_in.hit_rayp = s2->seg_out.hit_rayp = a_ray;
                            BU_LIST_INSERT(&(waiting_segs.l), &(s2->l));
                        }
                    }
                    resp->re_shot_hit++;
                }
            }

            /*
             * If a_onehit == 0 and a_ray_length <= 0, then the ray is
             * traced to +infinity.
             *
             * If a_onehit != 0, then it indicates how many hit points
             * (which are greater than the ray start point of 0.0) the
             * application requires, i.e., partitions with inhit >= 0.  (If
             * negative, indicates number of non-air hits needed).  If
             * this box yielded additional segments, immediately weave
             * them into the partition list, and perform final boolean
             * evaluation.  If this results in the required number of
             * final partitions, then cease ray-tracing and hand the
             * partitions over to the application.  All partitions will
             * have valid in and out distances.  a_ray_length is treated
             * similarly to a_onehit.
             */
            if (BU_LIST_NON_EMPTY(&(waiting_segs.l))) {
                if (ap->a_onehit != 0) {
                    int done;

                    /* Weave these segments into partition list */
                    rt_boolweave(pfinished_segs, &waiting_segs, &InitialPart, ap);

                    if (BU_PTBL_LEN(&resp->re_pieces_pending) > 0) {

                        /* Find the lowest pending mindist, that's as far
                         * as boolfinal can progress to.
                         */
                        struct rt_piecestate **psp;
                        for (BU_PTBL_FOR(psp, (struct rt_piecestate **), &resp->re_pieces_pending)) {
                            register fastf_t dist;

                            dist = (*psp)->mindist;
                            if (dist < pending_hit) {
                                pending_hit = dist;
                            }
                        }
                    }

                    /* Evaluate regions up to end of good segs */
                    if (ss.box_end < pending_hit) pending_hit = ss.box_end;
                    done = rt_boolfinal(&InitialPart, pFinalPart, last_bool_start, pending_hit, regionbits, ap, solidbits);
                    last_bool_start = pending_hit;

                    /* See if enough partitions have been acquired */
                    if (done > 0) goto hitit;
                }
            }

            if (ap->a_ray_length > 0.0 &&
                ss.box_end >= ap->a_ray_length &&
                ap->a_ray_length < pending_hit)
                break;

            /* Push ray onwards to next box */
            ss.box_start = ss.box_end;


            j = (d[0]<d[1] && d[0]<d[2]) ? 0 : (d[1]<d[2] ? 1 : 2);

            id[j] += rd[j];

            if ((id[j] < 0) || (id[j] >= M[j]))
                break;

            d[j] += ddist[j];
            i += ip[j];    
        }
    }

    /*
     * Ray has finally left known space -- Weave any remaining
     * segments into the partition list.
     */
    /* Process any pending hits into segs */
    if (BU_PTBL_LEN(&resp->re_pieces_pending) > 0) {
	struct rt_piecestate **psp;
	for (BU_PTBL_FOR(psp, (struct rt_piecestate **), &resp->re_pieces_pending)) {
	    if ((*psp)->htab.end > 0) {
		/* Convert any pending hits into segs */
		/* Distance correction was handled in ft_piece_shot */
		(*psp)->stp->st_meth->ft_piece_hitsegs(*psp, &waiting_segs, ap);
		rt_htbl_reset(&(*psp)->htab);
	    }
	    *psp = NULL;
	}
	bu_ptbl_reset(&resp->re_pieces_pending);
    }

    if (BU_LIST_NON_EMPTY(&(waiting_segs.l))) {
	rt_boolweave(pfinished_segs, &waiting_segs, &InitialPart, ap);
    }

    /* finished_segs chain now has all segments hit by this ray */
    if (BU_LIST_IS_EMPTY(&(pfinished_segs->l))) {
        ap->a_return = 0;
	goto out;
    }

    /*
     * All intersections of the ray with the model have been computed.
     * Evaluate the boolean trees over each partition.
     */
    (void)rt_boolfinal(&InitialPart, pFinalPart, BACKING_DIST, INFINITY, regionbits, ap, solidbits);

    if (pFinalPart->pt_forw == pFinalPart) {
        ap->a_return = 0;
	RT_FREE_PT_LIST(&InitialPart, resp);
	goto out;
    }

    /*
     * Ray/model intersections exist.  Pass the list to the user's
     * a_hit() routine.  Note that only the hit_dist elements of
     * pt_inhit and pt_outhit have been computed yet.  To compute both
     * hit_point and hit_normal, use the
     *
     * RT_HIT_NORMAL(NULL, hitp, stp, rayp, 0);
     *
     * macro.  To compute just hit_point, use
     *
     * VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
     */
hitit:
    /*
     * Before recursing, release storage for unused Initial
     * partitions.  finished_segs can not be released yet, because
     * FinalPart partitions will point to hits in those segments.
     */
    RT_FREE_PT_LIST(&InitialPart, resp);

    /*
     * finished_segs is only used by special hit routines which don't
     * follow the traditional solid modeling paradigm.
     */
    ap->a_return = 1;

    /*
     * Processing of this ray is complete.
     */
out:
    /* Return dynamic resources to their freelists.  */
    BU_CK_BITV(solidbits);
    BU_LIST_APPEND(&resp->re_solid_bitv, &solidbits->l);
    if (backbits) {
	BU_CK_BITV(backbits);
	BU_LIST_APPEND(&resp->re_solid_bitv, &backbits->l);
    }
    BU_CK_PTBL(regionbits);
    BU_LIST_APPEND(&resp->re_region_ptbl, &regionbits->l);

    /* Clean up any pending hits */
    if (BU_PTBL_LEN(&resp->re_pieces_pending) > 0) {
	struct rt_piecestate **psp;
	for (BU_PTBL_FOR(psp, (struct rt_piecestate **), &resp->re_pieces_pending)) {
	    if ((*psp)->htab.end > 0)
		rt_htbl_reset(&(*psp)->htab);
	}
	bu_ptbl_reset(&resp->re_pieces_pending);
    }

    /* Terminate any logging */
    return ap->a_return;
}

/**
 * Arrange to have the pixel output.  a_uptr has region pointer, for
 * reference.
 */
void
simple_view_pixel(int a_x, int a_y, int a_user, point_t a_color)
{
    int r, g, b;
    unsigned char *pixelp;
    struct scanline *slp;

    if (a_user == 0) {
	/* Shot missed the model, don't dither */
	r = ibackground[0];
	g = ibackground[1];
	b = ibackground[2];
	VSETALL(a_color, -1e-20);	/* background flag */
    } else {
        r = a_color[0]*255;
        g = a_color[1]*255;
        b = a_color[2]*255;
	if (r > 255) r = 255;
	else if (r < 0) r = 0;
	if (g > 255) g = 255;
	else if (g < 0) g = 0;
	if (b > 255) b = 255;
	else if (b < 0) b = 0;
	if (r == ibackground[0] && g == ibackground[1] &&  b == ibackground[2]) {
	    r = inonbackground[0];
	    g = inonbackground[1];
	    b = inonbackground[2];
	}

	/* Make sure it's never perfect black */
	if (r==0 && g==0 && b==0)
	    b = 1;
    }

    slp = &scanline;
    if (slp->sl_buf == (unsigned char *)0) {
        slp->sl_buf = (unsigned char *)bu_calloc(width, pwidth, "sl_buf scanline buffer");
        slp->sl_left = width;
    }
    pixelp = slp->sl_buf+(a_x*pwidth);
    *pixelp++ = r;
    *pixelp++ = g;
    *pixelp++ = b;
    if (--(slp->sl_left) <= 0) {
        /* do_eol */
        if (fbp != FB_NULL) {
            size_t npix;
            bu_semaphore_acquire(BU_SEM_SYSCALL);
            npix = fb_write(fbp, 0, a_y, (unsigned char *)slp->sl_buf, width);
            bu_semaphore_release(BU_SEM_SYSCALL);
            if (npix < width)
                bu_exit(EXIT_FAILURE, "scanline fb_write error");
        }
        bu_free(slp->sl_buf, "sl_buf scanline buffer");
        slp->sl_buf = (unsigned char *)0;
    }
}

/**
 * Call the material-specific shading function, after making certain
 * that all shadework fields desired have been provided.
 *
 * Returns -
 * 0 on failure
 * 1 on success
 *
 * But of course, nobody cares what this returns.  Everyone calls us
 * as (void)viewshade()
 */
int
simple_viewshade(struct xray *a_ray, double perp, const struct partition *pp, struct hit *sw_hit, point_t *color)
{
    fastf_t cosine;
    point_t matcolor;		/* Material color */

    /* Default color is white (uncolored) */
    VSETALL(matcolor, 1);

    if (sw_hit->hit_dist < 0.0)
	sw_hit->hit_dist = 0.0;	/* Eye inside solid */

    /* If optional inputs are required, have them computed */
    VJOIN1(sw_hit->hit_point, a_ray->r_pt, sw_hit->hit_dist, a_ray->r_dir);

    /* shade inputs */
    if (sw_hit->hit_dist < 0.0) {
        /* Eye inside solid, orthoview */
        VREVERSE(sw_hit->hit_normal, a_ray->r_dir);
    } else {
        fastf_t f;
        /* Get surface normal for hit point */
        if (pp->pt_inseg->seg_stp->st_meth->ft_norm) {
            pp->pt_inseg->seg_stp->st_meth->ft_norm(sw_hit, pp->pt_inseg->seg_stp, a_ray);
        }
        if (pp->pt_inflip) {
            VREVERSE(sw_hit->hit_normal, sw_hit->hit_normal);
        } else {
            VMOVE(sw_hit->hit_normal, sw_hit->hit_normal);
        }

        /* Check to make sure normals are OK */
        f = VDOT(a_ray->r_dir, sw_hit->hit_normal);
        if (f > 0.0 && !(((f) < 0) ? ((-f)<=perp) : (f <= perp))) {
            /* reverse the normal so it's lit */
            VREVERSE(sw_hit->hit_normal, sw_hit->hit_normal);
        }
    }
    
    /* Invoke the actual shader (diffuse) */
    /* Diffuse reflectance from "Ambient" light source (at eye) */
    if ((cosine = -VDOT(sw_hit->hit_normal, a_ray->r_dir)) > 0.0) {
        if (cosine > 1.00001) {
            cosine = 1;
        }
        cosine *= AmbientIntensity;
        VSCALE(*color, matcolor, cosine);
    } else {
        VSETALL(*color, 0);
    }
    return 1;
}

/**
 * Manage the coloring of whatever it was we just hit.  This can be a
 * recursive procedure.
 */
int
simple_colorview(struct application *ap, struct partition *PartHeadp, point_t *color)
{
    struct partition *pp;
    struct hit *hitp;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw)
	if (pp->pt_outhit->hit_dist >= 0.0) break;

    if (pp == PartHeadp) {
	return 0;
    }

    hitp = pp->pt_inhit;
    ap->a_uptr = (void *)pp->pt_regionp;	/* note which region was shaded */

    /* individual shaders must handle reflection & refraction */
    (void)simple_viewshade(&ap->a_ray, ap->a_rt_i->rti_tol.perp, pp, pp->pt_inhit, color);

    /* XXX This is always negative when eye is inside air solid */
    ap->a_dist = hitp->hit_dist;
    return 1;
}

void
simple_pixel(struct resource *resp, int pixelnum)
{
    struct application a;
    vect_t point;		/* Ref point on eye or view plane */

    struct seg finished_segs;	/* processed by rt_boolweave() */
    struct partition FinalPart;	/* Head of Final Partitions */
    int a_y, a_x;
    int a_user;
    point_t a_color;

    int hit;
    point_t color;

    /* Obtain fresh copy of global application struct */
    a = APP;				/* struct copy */
    a.a_magic = RT_AP_MAGIC;
    a.a_ray.magic = RT_RAY_MAGIC;
    a.a_resource = resp;

    a_y = (int)(pixelnum/width);
    a_x = (int)(pixelnum - (a_y * width));

    /* our starting point, used for non-jitter */
    VJOIN2 (point, viewbase_model, a_x, dx_model, a_y, dy_model);

    /* not tracing the corners of a prism by default */
    a.a_pixelext=NULL;

    /* LOOP BELOW IS UNROLLED ONE SAMPLE SINCE THAT'S THE COMMON CASE.
     *
     * XXX - If you edit the unrolled or non-unrolled section, be sure
     * to edit the other section.
     */
    /* not hypersampling, so just do it */

    VMOVE(a.a_ray.r_pt, point);
    VMOVE(a.a_ray.r_dir, APP.a_ray.r_dir);
    
    a.a_level = 0;		/* recursion level */
    a.a_purpose = "main ray";

    BU_LIST_INIT(&finished_segs.l);

    FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;
    FinalPart.pt_magic = PT_HD_MAGIC;
    a.a_Final_Part_hdp = &FinalPart;
    a.a_finished_segs_hdp = &finished_segs;
    
    hit = simple_shootray(resp, &a.a_ray, a.a_rt_i, &a, &FinalPart, &finished_segs);

    if (hit) {
        /* hit */
        a_user = 1;   		/* Signal view_pixel:  HIT */
        a.a_return = simple_colorview(&a, &FinalPart, &color);
        VMOVE(a_color, color);
    } else {
        /* miss */
        a_user = 0;             	/* Signal view_pixel:  MISS */
        a.a_return = 0;
        VMOVE(a_color, background);	/* In case someone looks */
    }

    RT_FREE_SEG_LIST(&finished_segs, resp);
    RT_FREE_PT_LIST(&FinalPart, resp);
    
    simple_view_pixel(a_x, a_y, a_user, a_color);
    return;
}

void
simple_run(int cur_pixel, int last_pixel)
{
    int per_processor_chunk = 0;	/* how many pixels to do at once */

    int pixelnum;

    npsw = 1;

    /* The more CPUs at work, the bigger the bites we take */
    if (per_processor_chunk <= 0) per_processor_chunk = npsw;

    bu_semaphore_acquire(RT_SEM_WORKER);
    
/*#pragma omp parallel for*/
    for (pixelnum = cur_pixel; pixelnum <= last_pixel; pixelnum++) {
        int cpu;
        struct resource *resp;

        cpu = 0;
        resp = &resource[cpu];
        RT_CK_RESOURCE(resp);

        simple_pixel(resp, pixelnum);
    }

    bu_semaphore_release(RT_SEM_WORKER);

    /* Tally up the statistics */
    {
        size_t cpu;
        for (cpu=0; cpu < npsw; cpu++) {
            if (resource[cpu].re_magic != RESOURCE_MAGIC) {
                bu_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
                continue;
            }
            rt_add_res_stats(APP.a_rt_i, &resource[cpu]);
        }
    }
    return;
}
/*********************************************************************************/



/**
 * This is a separate function primarily as a service to REMRT.
 */
void
do_prep(struct rt_i *rtip)
{
    struct bu_vls times = BU_VLS_INIT_ZERO;

    RT_CHECK_RTI(rtip);
    if (rtip->needprep) {
	/* Allow lighting model to set up (e.g. lights, materials, etc.) */
	view_setup(rtip);

	/* Allow RT library to prepare itself */
	rt_prep_timer();
	rt_prep_parallel(rtip, npsw);

        simple_prep(rtip);

	(void)rt_get_timer(&times, NULL);
	if (rt_verbosity & VERBOSE_STATS)
	    bu_log("PREP: %s\n", bu_vls_addr(&times));
	bu_vls_free(&times);
    }
    memory_summary();
    if (rt_verbosity & VERBOSE_STATS) {
	bu_log("%s: %d nu, %d cut, %d box (%zu empty)\n",
	       rtip->rti_space_partition == RT_PART_NUGRID ?
	       "NUGrid" : "NUBSP",
	       rtip->rti_ncut_by_type[CUT_NUGRIDNODE],
	       rtip->rti_ncut_by_type[CUT_CUTNODE],
	       rtip->rti_ncut_by_type[CUT_BOXNODE],
	       rtip->nempty_cells);
    }
}


/**
 * Do all the actual work to run a frame.
 *
 * Returns -1 on error, 0 if OK.
 */
int
do_frame(int framenumber)
{
    struct bu_vls times = BU_VLS_INIT_ZERO;
    char framename[128] = {0};		/* File name to hold current frame */
    struct rt_i *rtip = APP.a_rt_i;
    double utime = 0.0;			/* CPU time used */
    double nutime = 0.0;		/* CPU time used, normalized by ncpu */
    double wallclock = 0.0;		/* # seconds of wall clock time */
    int npix = 0;			/* # of pixel values to be done */
    vect_t work, temp;
    quat_t quat;

    if (rt_verbosity & VERBOSE_FRAMENUMBER)
	bu_log("\n...................Frame %5d...................\n",
	       framenumber);

    /* Compute model RPP, etc. */
    do_prep(rtip);

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log("Tree: %zu solids in %zu regions\n", rtip->nsolids, rtip->nregions);

    if (Query_one_pixel) {
	query_rdebug = R_DEBUG;
	query_debug = RT_G_DEBUG;
	RTG.debug = rdebug = 0;
    }

    if (rtip->nsolids <= 0)
	bu_exit(3, "ERROR: No primitives remaining.\n");

    if (rtip->nregions <= 0)
	bu_exit(3, "ERROR: No regions remaining.\n");

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log("Model: X(%g, %g), Y(%g, %g), Z(%g, %g)\n",
	       rtip->mdl_min[X], rtip->mdl_max[X],
	       rtip->mdl_min[Y], rtip->mdl_max[Y],
	       rtip->mdl_min[Z], rtip->mdl_max[Z]);

    /*
     * Perform Grid setup.
     * This may alter cell size or width/height.
     */
    grid_setup();
    /* az/el 0, 0 is when screen +Z is model +X */
    VSET(work, 0, 0, 1);
    MAT3X3VEC(temp, view2model, work);
    bn_ae_vec(&azimuth, &elevation, temp);

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log(
	    "View: %g azimuth, %g elevation off of front view\n",
	    azimuth, elevation);
    quat_mat2quat(quat, model2view);
    if (rt_verbosity & VERBOSE_VIEWDETAIL) {
	bu_log("Orientation: %g, %g, %g, %g\n", V4ARGS(quat));
	bu_log("Eye_pos: %g, %g, %g\n", V3ARGS(eye_model));
	bu_log("Size: %gmm\n", viewsize);

	/**
	 * This code shows how the model2view matrix can be
	 * reconstructed using the information from the Orientation,
	 * Eye_pos, and Size messages in the rt log output.
	 @code
	 {
	 mat_t rotscale, xlate, newmat;
	 quat_t newquat;
	 bn_mat_print("model2view", model2view);
	 quat_quat2mat(rotscale, quat);
	 rotscale[15] = 0.5 * viewsize;
	 MAT_IDN(xlate);
	 MAT_DELTAS_VEC_NEG(xlate, eye_model);
	 bn_mat_mul(newmat, rotscale, xlate);
	 bn_mat_print("reconstructed m2v", newmat);
	 quat_mat2quat(newquat, newmat);
	 HPRINT("reconstructed orientation:", newquat);
	 @endcode
	 *
	 */

	bu_log("Grid: (%g, %g) mm, (%zu, %zu) pixels\n",
	       cell_width, cell_height,
	       width, height);
	bu_log("Beam: radius=%g mm, divergence=%g mm/1mm\n",
	       APP.a_rbeam, APP.a_diverge);
    }

    /* Process -b and ??? options now, for this frame */
    if (pix_start == -1) {
	pix_start = 0;
	pix_end = (int)(height * width - 1);
    }
    if (string_pix_start) {
	int xx, yy;
	register char *cp = string_pix_start;

	xx = atoi(cp);
	while (*cp >= '0' && *cp <= '9') cp++;
	while (*cp && (*cp < '0' || *cp > '9')) cp++;
	yy = atoi(cp);
	bu_log("only pixel %d %d\n", xx, yy);
	if (xx * yy >= 0) {
	    pix_start = (int)(yy * width + xx);
	    pix_end = pix_start;
	}
    }
    if (string_pix_end) {
	int xx, yy;
	register char *cp = string_pix_end;

	xx = atoi(cp);
	while (*cp >= '0' && *cp <= '9') cp++;
	while (*cp && (*cp < '0' || *cp > '9')) cp++;
	yy = atoi(cp);
	bu_log("ending pixel %d %d\n", xx, yy);
	if (xx * yy >= 0) {
	    pix_end = (int)(yy * width + xx);
	}
    }

    /* Allocate data for pixel map for rerendering of black pixels */
    if (pixmap == NULL) {
	pixmap = (unsigned char*)bu_calloc(sizeof(RGBpixel), width*height, "pixmap allocate");
    }

    /*
     * If this image is unlikely to be for debugging,
     * be gentle to the machine.
     */
    if (!interactive) {
	if (npix > 512*512)
	    bu_nice_set(14);
	else if (npix > 256*256)
	    bu_nice_set(10);
    }

    /*
     * Determine output file name
     * On UNIX only, check to see if this is a "restart".
     */
    if (outputfile != (char *)0) {
	if (framenumber <= 0) {
	    snprintf(framename, 128, "%s", outputfile);
	} else {
	    snprintf(framename, 128, "%s.%d", outputfile, framenumber);
	}

#ifdef HAVE_SYS_STAT_H
	/*
	 * This code allows the computation of a particular frame to a
	 * disk file to be resumed automatically.  This is worthwhile
	 * crash protection.  This use of stat() and fseek() is
	 * UNIX-specific.
	 *
	 * It is not appropriate for the RT "top part" to assume
	 * anything about the data that the view module will be
	 * storing.  Therefore, it is the responsibility of
	 * view_2init() to also detect that some existing data is in
	 * the file, and take appropriate measures (like reading it
	 * in).
	 *
	 * view_2init() can depend on the file being open for both
	 * reading and writing, but must do its own positioning.
	 */
	{
	    int fd;
	    int ret;
	    struct stat sb;

	    if (bu_file_exists(framename, NULL)) {
		/* File exists, maybe with partial results */
		outfp = NULL;
		fd = open(framename, O_RDWR);
		if (fd < 0) {
		    perror("open");
		} else {
		    outfp = fdopen(fd, "r+");
		    if (!outfp)
			perror("fdopen");
		}

		if (fd < 0 || !outfp) {
		    bu_log("ERROR: Unable to open \"%s\" for reading and writing (check file permissions)\n", framename);

		    if (matflag)
			return 0; /* OK: some undocumented reason */

		    return -1; /* BAD: oops, disappeared */
		}

		/* check if partial result */
		ret = fstat(fd, &sb);
		if (ret >= 0 && sb.st_size > 0 && (size_t)sb.st_size < width*height*sizeof(RGBpixel)) {

		    /* Read existing pix data into the frame buffer */
		    if (sb.st_size > 0) {
			size_t bytes_read = fread(pixmap, 1, (size_t)sb.st_size, outfp);
			if (rt_verbosity & VERBOSE_OUTPUTFILE)
			    bu_log("Reading existing pix data from \"%s\".\n", framename);
			if (bytes_read < (size_t)sb.st_size)
			    return -1;
		    }

		}
	    }
	}
#endif

	/* Ordinary case for creating output file */
	if (outfp == NULL) {
#ifndef RT_TXT_OUTPUT
	    /* FIXME: in the case of rtxray, this is wrong.  it writes
	     * out a bw image so depth should be just 1, not 3.
	     */
	    bif = icv_create(width, height, ICV_COLOR_SPACE_RGB);

	    if (bif == NULL && (outfp = fopen(framename, "w+b")) == NULL) {
		perror(framename);
		if (matflag) return 0;	/* OK */
		return -1;			/* Bad */
	    }
#else
	    outfp = fopen(framename, "w");
	    if (outfp == NULL) {
		perror(framename);
		if (matflag) return 0;	/* OK */
		return -1;			/* Bad */
	    }
#endif
	}

	if (rt_verbosity & VERBOSE_OUTPUTFILE)
	    bu_log("Output file is '%s' %zux%zu pixels\n",
		   framename, width, height);
    }

    /* initialize lighting, may update pix_start */
    view_2init(&APP, framename);

    /* Just while doing the ray-tracing */
    if (R_DEBUG&RDEBUG_RTMEM)
	bu_debug |= (BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_LOG);

    rtip->nshots = 0;
    rtip->nmiss_model = 0;
    rtip->nmiss_tree = 0;
    rtip->nmiss_solid = 0;
    rtip->nmiss = 0;
    rtip->nhits = 0;
    rtip->rti_nrays = 0;

    if (rt_verbosity & (VERBOSE_LIGHTINFO|VERBOSE_STATS))
	bu_log("\n");
    fflush(stdout);
    fflush(stderr);

    /*
     * Compute the image
     * It may prove desirable to do this in chunks
     */
    rt_prep_timer();
#if 0
    if (incr_mode) {
	for (incr_level = 1; incr_level <= incr_nlevel; incr_level++) {
	    if (incr_level > 1)
		view_2init(&APP, framename);

	    do_run(0, (1<<incr_level)*(1<<incr_level)-1);
	}
    }
    else if (full_incr_mode) {
	/* Multiple frame buffer mode */
	for (full_incr_sample = 1; full_incr_sample <= full_incr_nsamples;
	    full_incr_sample++) {
	    if (full_incr_sample > 1) /* first sample was already initialized */
		view_2init(&APP, framename);
	    do_run(pix_start, pix_end);
	}
    }
    else {
	do_run(pix_start, pix_end);

	/* Reset values to full size, for next frame (if any) */
	pix_start = 0;
	pix_end = (int)(height*width - 1);
    }
#endif
    printf("pix_start: %d, pix_end: %d\n", pix_start, pix_end);
    simple_run(pix_start, pix_end);

    /* Reset values to full size, for next frame (if any) */
    pix_start = 0;
    pix_end = (int)(height*width - 1);
    /**/
    
    utime = rt_get_timer(&times, &wallclock);

    /*
     * End of application.  Done outside of timing section.
     * Typically, writes any remaining results out.
     */
    view_end(&APP);

    /* Stop memory debug printing until next frame, leave full checking on */
    if (R_DEBUG&RDEBUG_RTMEM)
	bu_debug &= ~BU_DEBUG_MEM_LOG;

    /* These results need to be normalized.  Otherwise, all we would
     * know is that a given workload takes about the same amount of
     * CPU time, regardless of the number of CPUs.
     */
    if (npsw > 1) {
	size_t avail_cpus;
	size_t ncpus;

	avail_cpus = bu_avail_cpus();
	if (npsw > avail_cpus) {
	    ncpus = avail_cpus;
	} else {
	    ncpus = npsw;
	}
	nutime = utime / ncpus;			/* compensate */
    } else {
	nutime = utime;
    }

    /* prevent a bogus near-zero time to prevent infinite and
     * near-infinite results without relying on IEEE floating point
     * zero comparison.
     */
    if (NEAR_ZERO(nutime, VDIVIDE_TOL)) {
	bu_log("WARNING:  Raytrace timings are likely to be meaningless\n");
	nutime = VDIVIDE_TOL;
    }

    /*
     * All done.  Display run statistics.
     */
    if (rt_verbosity & VERBOSE_STATS)
	bu_log("SHOT: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    memory_summary();
    if (rt_verbosity & VERBOSE_STATS) {
	bu_log("%zu solid/ray intersections: %zu hits + %zu miss\n",
	       rtip->nshots, rtip->nhits, rtip->nmiss);
	bu_log("pruned %.1f%%:  %zu model RPP, %zu dups skipped, %zu solid RPP\n",
	       rtip->nshots > 0 ? ((double)rtip->nhits*100.0)/rtip->nshots : 100.0,
	       rtip->nmiss_model, rtip->ndup, rtip->nmiss_solid);
	bu_log("Frame %2d: %10zu pixels in %9.2f sec = %12.2f pixels/sec\n",
	       framenumber,
	       width*height, nutime, ((double)(width*height))/nutime);
	bu_log("Frame %2d: %10zu rays   in %9.2f sec = %12.2f rays/sec (RTFM)\n",
	       framenumber,
	       rtip->rti_nrays, nutime, ((double)(rtip->rti_nrays))/nutime);
	bu_log("Frame %2d: %10zu rays   in %9.2f sec = %12.2f rays/CPU_sec\n",
	       framenumber,
	       rtip->rti_nrays, utime, ((double)(rtip->rti_nrays))/utime);
	bu_log("Frame %2d: %10zu rays   in %9.2f sec = %12.2f rays/sec (wallclock)\n",
	       framenumber,
	       rtip->rti_nrays,
	       wallclock, ((double)(rtip->rti_nrays))/wallclock);
    }
    if (bif != NULL) {
	icv_write(bif, framename, MIME_IMAGE_AUTO);
	icv_destroy(bif);
	bif = NULL;
    }

    if (outfp != NULL) {
	/* Protect finished product */
	if (outputfile != (char *)0)
	    (void)bu_fchmod(fileno(outfp), 0444);

	(void)fclose(outfp);
	outfp = NULL;
    }

    if (R_DEBUG&RDEBUG_STATS) {
	/* Print additional statistics */
	res_pr();
    }

    bu_log("\n");
    bu_free(pixmap, "pixmap allocate");
    pixmap = (unsigned char *)NULL;
    return 0;		/* OK */
}


/**
 * Compute the rotation specified by the azimuth and elevation
 * parameters.  First, note that these are specified relative to the
 * GIFT "front view", i.e., model (X, Y, Z) is view (Z, X, Y): looking
 * down X axis, Y right, Z up.
 *
 * A positive azimuth represents rotating the *eye* around the
 * Y axis, or, rotating the *model* in -Y.
 * A positive elevation represents rotating the *eye* around the
 * X axis, or, rotating the *model* in -X.
 */
void
do_ae(double azim, double elev)
{
    vect_t temp;
    vect_t diag;
    mat_t toEye;
    struct rt_i *rtip = APP.a_rt_i;

    if (rtip == NULL)
	return;

    if (rtip->nsolids <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no primitives active\n");

    if (rtip->nregions <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no regions active\n");

    if (rtip->mdl_max[X] >= INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }

    /*
     * Enlarge the model RPP just slightly, to avoid nasty effects
     * with a solid's face being exactly on the edge NOTE: This code
     * is duplicated out of librt/tree.c/rt_prep(), and has to appear
     * here to enable the viewsize calculation to match the final RPP.
     */
    rtip->mdl_min[X] = floor(rtip->mdl_min[X]);
    rtip->mdl_min[Y] = floor(rtip->mdl_min[Y]);
    rtip->mdl_min[Z] = floor(rtip->mdl_min[Z]);
    rtip->mdl_max[X] = ceil(rtip->mdl_max[X]);
    rtip->mdl_max[Y] = ceil(rtip->mdl_max[Y]);
    rtip->mdl_max[Z] = ceil(rtip->mdl_max[Z]);

    MAT_IDN(Viewrotscale);
    bn_mat_angles(Viewrotscale, 270.0+elev, 0.0, 270.0-azim);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X]+rtip->mdl_min[X])/2.0);
    toEye[MDY] = -((rtip->mdl_max[Y]+rtip->mdl_min[Y])/2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z]+rtip->mdl_min[Z])/2.0);

    /* Fit a sphere to the model RPP, diameter is viewsize, unless
     * viewsize command used to override.
     */
    if (viewsize <= 0) {
	VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	viewsize = MAGNITUDE(diag);
	if (aspect > 1) {
	    /* don't clip any of the image when autoscaling */
	    viewsize *= aspect;
	}
    }

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (viewsize < 0 || ZERO(viewsize)) {
	viewsize = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(eye_model, view2model, temp);
}


void
res_pr(void)
{
    register struct resource *res;
    register size_t i;

    bu_log("\nResource use summary, by processor:\n");
    res = &resource[0];
    for (i = 0; i < npsw; i++, res++) {
	bu_log("---CPU %d:\n", i);
	if (res->re_magic != RESOURCE_MAGIC) {
	    bu_log("Bad magic number!\n");
	    continue;
	}
	bu_log("seg       len=%10ld get=%10ld free=%10ld\n", res->re_seglen, res->re_segget, res->re_segfree);
	bu_log("partition len=%10ld get=%10ld free=%10ld\n", res->re_partlen, res->re_partget, res->re_partfree);
	bu_log("boolstack len=%10ld\n", res->re_boolslen);
    }
}


/**
 * Command table for RT control script language
 */
struct command_tab rt_cmdtab[] = {
    {"start", "frame number", "start a new frame",
     cm_start,	2, 2},
    {"viewsize", "size in mm", "set view size",
     cm_vsize,	2, 2},
    {"eye_pt", "xyz of eye", "set eye point",
     cm_eyept,	4, 4},
    {"lookat_pt", "x y z [yflip]", "set eye look direction, in X-Y plane",
     cm_lookat_pt,	4, 5},
    {"viewrot", "4x4 matrix", "set view direction from matrix",
     cm_vrot,	17, 17},
    {"orientation", "quaternion", "set view direction from quaternion",
     cm_orientation,	5, 5},
    {"end", 	"", "end of frame setup, begin raytrace",
     cm_end,		1, 1},
    {"multiview", "", "produce stock set of views",
     cm_multiview,	1, 1},
    {"anim", 	"path type args", "specify articulation animation",
     cm_anim,	4, 999},
    {"tree", 	"treetop(s)", "specify alternate list of tree tops",
     cm_tree,	1, 999},
    {"clean", "", "clean articulation from previous frame",
     cm_clean,	1, 1},
    {"_closedb", "", "Close .g database, (for memory debugging)",
     cm_closedb,	1, 1},
    {"set", 	"", "show or set parameters",
     cm_set,		1, 999},
    {"ae", "azim elev", "specify view as azim and elev, in degrees",
     cm_ae,		3, 3},
    {"opt", "-flags", "set flags, like on command line",
     cm_opt,		2, 999},
    {(char *)0, (char *)0, (char *)0,
     0,		0, 0	/* END */}
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
