/*
 *			V I E W E D G E
 *
 *  Ray Tracing program RTEDGE bottom half.
 *
 *  This module utilizes the RT library to interrogate a MGED
 *  model and produce a pixfile or framebuffer image of the
 *  hidden line 'edges' of the geometry. An edge exists whenever
 *  there is a change in region ID, or a significant change in
 *  obliquity or line-of-sight distance.
 *
 *  XXX - Get parallel processing working.
 *  XXX - Add support for detecting changes in specified attributes.
 *
 *  Author -
 *	Ronald A. Bowers
 *
 *  Source -
 *	
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2001 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSviewedge[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "fb.h"
#include "./ext.h"

#define RTEDGE_DEBUG 1

int	use_air = 0;			/* Handling of air in librt */
int	using_mlib = 0;			/* Material routines NOT used */

extern 	FBIO	*fbp;			/* Framebuffer handle */
extern	fastf_t	viewsize;
extern	int	lightmodel;
extern	int	width, height;
extern int	per_processor_chunk;

static	int pixsize = 0;		/* bytes per pixel in scanline */

struct cell {
        int		c_ishit;
	struct region *	c_region;
	fastf_t		c_dist;		/* distance from emanation plane to in_hit */
	int		c_id;		/* region_id of component hit */
	point_t		c_hit;		/* 3-space hit point of ray */
	vect_t		c_normal;	/* surface normal at the hit point */
	vect_t		c_rdir;		/* ray direction, permits perspective */
};

#define MISS_DIST	-1
#define MISS_ID		-1

static unsigned char *scanline[MAX_PSW];
int   		nEdges = 0;
int   		nPixels = 0;
fastf_t		max_dist;	/* min. distance for drawing pits/mountains */
fastf_t		maxangle;	/* value of the cosine of the angle bet. surface normals that triggers shading */

typedef int color[3];
color	foreground = { 255, 255, 255};
color	background = { 0, 0, 0};

/*
 * Flags that set which edges are detected.
 * detect_ids means detect boundaries region id codes.
 * detect_regions -> detect region boundaries.
 * detect_distance -> detect noticable differences in hit distance.
 * detect_normals -> detect rapid change in surface normals
 */
int	detect_ids = 1;
int	detect_regions = 0;
int	detect_distance = 1;
int	detect_normals = 1;

/*
 * Prototypes for the viewedge edge detection functions
 */
static int is_edge(struct application *, struct cell *, struct cell *,
	struct cell *below);
static int rayhit (struct application *, struct partition *, struct seg *);
static int rayhit2 (struct application *, struct partition *, struct seg *);
static int raymiss (struct application *);
static int raymiss2 (struct application *);
static int handle_main_ray(struct application *, struct partition *, struct seg *);

#define COSTOL          0.91    /* normals differ if dot product < COSTOL */
#define OBLTOL          0.1     /* high obliquity if cosine of angle < OBLTOL ! */
#define is_Odd(_a)      ((_a)&01)
#define ARCTAN_87       19.08

#ifndef Abs
#define Abs( x )        ((x) < 0 ? -(x) : (x))                  /* UNSAFE */
#endif

/*
 * From do.c
 */
#if CRAY
#	define byteoffset(_i)	(((int)&(_i)))	/* actually a word offset */
#else
#  if IRIX > 5 && _MIPS_SIM != _MIPS_SIM_ABI32
#	define byteoffset(_i)	((size_t)__INTADDR__(&(_i)))
#  else
#    if sgi || __convexc__ || ultrix || _HPUX_SOURCE
	/* "Lazy" way.  Works on reasonable machines with byte addressing */
#	define byteoffset(_i)	((int)((char *)&(_i)))
#    else
	/* "Conservative" way of finding # bytes as diff of 2 char ptrs */
#	define byteoffset(_i)	((int)(((char *)&(_i))-((char *)0)))
#    endif
#  endif
#endif
/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"%d", 1, "detect_regions", byteoffset(detect_regions), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "dr", byteoffset(detect_regions), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "detect_distance", byteoffset(detect_distance), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "dd", byteoffset(detect_distance), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "detect_normals", byteoffset(detect_normals), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "dn", byteoffset(detect_normals), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "detect_ids", byteoffset(detect_ids), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "di", byteoffset(detect_ids), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 3, "foreground", byteoffset(foreground), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 3, "fg", byteoffset(foreground), BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 3, "background", byteoffset(background),	BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 3, "bg", byteoffset(background),	BU_STRUCTPARSE_FUNC_NULL},	
	{"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL }
};

char usage[] = "\
Usage:  rtedge [options] model.g objects... >file.pix\n\
Options:\n\
 -s #               Grid size in pixels, (default 512)\n\
 -w # -n #          Grid size width and height in pixels\n\
 -V #               View (pixel) aspect ratio (width/height)\n\
 -a #               Azimuth in deg\n\
 -e #               Elevation in deg\n\
 -M                 Read matrix+cmds on stdin\n\
 -N #               NMG debug flags\n\
 -o model.pix       Output file, .pix format (default=fb)\n\
 -x #               librt debug flags\n\
 -X #               rt debug flags\n\
 -p #               Perspsective, degrees side to side\n\
 -P #               Set number of processors\n\
 -T #/#             Tolerance: distance/angular\n\
 -r                 Report overlaps\n\
 -R                 Do not report overlaps\n";

/*
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init( struct application *ap, char *file, char *obj, int minus_o )
{
    /*
     *  Allocate a scanline for each processor.
     */
    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 1;

    if( minus_o ) {
	/*
	 * Output is to a file stream.
	 * Do not open a framebuffer, do not allow parallel
	 * processing since we can't seek to the rows.
	 */
	rt_g.rtg_parallel = 0;
	bu_log ("view_init: deactivating parallelism due to minus_o.\n");
	return(0);
    }

    return(1);		/* we need a framebuffer */
}

/* beginning of a frame */
void
view_2init( struct application *ap )
{
    int i;

    /*
     * Per_processor_chuck specifies the number of pixels rendered
     * per each pass of a worker. By making this value equal to the
     * width of the image, each worker will render one scanline at
     * a time.
     */
    per_processor_chunk = width;

    /*
     * Use three bytes per pixel.
     */
    pixsize = 3;

    /*
     * Create a buffer for each scanline.
     */
    for ( i = 0; i < npsw; ++i ) {
	if (scanline[i] == NULL) {
	    scanline[i] = (unsigned char *)
		bu_malloc( per_processor_chunk*pixsize, "scanline buffer" );
	}	
    }

    /*
     * Set the hit distance difference necessary to trigger an edge.
     * This algorythm stolen from lgt, I may make it settable later.
     */
    max_dist = (cell_width*ARCTAN_87)+2;

}

/* end of each pixel */
void view_pixel( struct application *ap ) { }

/* end of each line */
void
view_eol( struct application *ap )
{
    int		cpu = ap->a_resource->re_cpu;

    bu_semaphore_acquire( BU_SEM_SYSCALL );
	if( outfp != NULL ) {
	    fwrite( scanline[cpu], pixsize, per_processor_chunk, outfp );
	} else if( fbp != FBIO_NULL ) {
	    fb_write( fbp, 0, ap->a_y, scanline[cpu], per_processor_chunk );
	}
    bu_semaphore_release( BU_SEM_SYSCALL );
}

void view_setup() { }

/*
 * end of a frame, called after rt_clean()
 */
void view_cleanup() { }

/*
 * end of each frame
 */
void view_end() { }

/*
 *			R A Y H I T
 */
int rayhit (struct application *ap, register struct partition *pt,
	struct seg *segp )
{
    if ( handle_main_ray(ap, pt, segp)) {
	ap->a_user = 1;
    } else {
	ap->a_user = 0;
    }
    return 1;
}

/*
 *			R A Y M I S S
 */
int raymiss( struct application *ap )
{
    if ( handle_main_ray(ap, NULL, NULL)) {
	ap->a_user = 1;
    } else {
	ap->a_user = 0;
    }
    return 0;
}

/*
 *			R A Y H I T 2
 */
int rayhit2 (struct application *ap, register struct partition *pt,
	struct seg *segp )
{
    struct partition	*pp = pt->pt_forw;
    struct hit		*hitp = pt->pt_forw->pt_inhit;
    struct cell 	*c = (struct cell *)ap->a_uptr;

    c->c_ishit 		= 1;
    c->c_region 	= pp->pt_regionp;
    c->c_id 		= pp->pt_regionp->reg_regionid;
    VMOVE(c->c_rdir, ap->a_ray.r_dir);
    VJOIN1(c->c_hit, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir );
    RT_HIT_NORMAL(c->c_normal, hitp,
	pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);
    c->c_dist = hitp->hit_dist;

    return(1);		
}

/*
 *			R A Y M I S S 2
 */
int raymiss2( register struct application *ap )
{
    struct cell *c = (struct cell *)ap->a_uptr;

    c->c_ishit    	= 0;
    c->c_region   	= 0;
    c->c_dist     	= MISS_DIST;
    c->c_id	    	= MISS_ID;	
    VSETALL(c->c_hit, MISS_DIST);
    VSETALL(c->c_normal, 0);
    VMOVE(c->c_rdir, ap->a_ray.r_dir);

    return(0);
}

int is_edge(struct application *ap, struct cell *here,
    struct cell *left, struct cell *below)
{

    if( here->c_id == -1 && left->c_id == -1 && below->c_id == -1) {
	/*
	 * All misses - catches condtions that would be bad later.
	 */
	return 0;
    }

    if (detect_ids) {
        if (here->c_id != -1 &&
            (here->c_id != left->c_id || here->c_id != below->c_id)) {
	    return 1;
        }
    }

    if (detect_regions) {
        if (here->c_region != 0 &&
            (here->c_region != left->c_region
            || here->c_region != below->c_region)) {
	    return 1;
        }
    }

    if (detect_distance) {
        if (Abs(here->c_dist - left->c_dist) > max_dist ||
	    Abs(here->c_dist - below->c_dist) > max_dist) {
	    return 1;
        }
    }

    if (detect_normals) {
    	if ((VDOT(here->c_normal, left->c_normal) < COSTOL) ||
	    (VDOT(here->c_normal, below->c_normal)< COSTOL)) {
	     return 1;	
        }
    }

    return 0;
}


/*
 *			H A N D L E _ M A I N _ R A Y
 *
 */

int
handle_main_ray( struct application *ap, register struct partition *PartHeadp,
	struct seg *segp )
{
	register struct partition *pp;
	register struct hit	*hitp;		/* which hit */
	
	LOCAL struct application	a2;
	LOCAL struct cell		me;
	LOCAL struct cell		below;
	LOCAL struct cell		left;
	LOCAL int			edge;
	LOCAL int			cpu;	

	memset(&a2, 0, sizeof(struct application));
	memset(&me, 0, sizeof(struct cell));
	memset(&below, 0, sizeof(struct cell));
	memset(&left, 0, sizeof(struct cell));
	
	cpu = ap->a_resource->re_cpu;	
	
	if (PartHeadp == NULL) {
	    /* The main shotline missed.
	     * pack the application struct
	     */
	    me.c_ishit    = 0;
	    me.c_dist   = MISS_DIST;
    	    me.c_id	    = MISS_ID;	
            VSETALL(me.c_hit, MISS_DIST);
            VSETALL(me.c_normal, 0);
            VMOVE(me.c_rdir, ap->a_ray.r_dir);
	} else {
	     pp = PartHeadp->pt_forw;
	     hitp = pp->pt_inhit;	
            /*
	     * Stuff the information for this cell.
	     */
	    me.c_ishit    = 0;
	    me.c_id = pp->pt_regionp->reg_regionid;
	    me.c_dist = hitp->hit_dist;
	    VMOVE(me.c_rdir, ap->a_ray.r_dir);
	    VJOIN1(me.c_hit, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir );
	    RT_HIT_NORMAL(me.c_normal, hitp,
	        pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);	    		
	}

	/*
	 * Now, fire a ray for both the cell below and the
	 * cell to the left.
	 */
	a2.a_hit = rayhit2;
	a2.a_miss = raymiss2;
	a2.a_onehit = 1;
	a2.a_rt_i = ap->a_rt_i;
	a2.a_resource = ap->a_resource;
	
	VSUB2(a2.a_ray.r_pt, ap->a_ray.r_pt, dy_model); /* below */
	VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
	a2.a_uptr = (genptr_t)&below;
        rt_shootray(&a2);
	
	VSUB2(a2.a_ray.r_pt, ap->a_ray.r_pt, dx_model); /* left */
	VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
	a2.a_uptr = (genptr_t)&left;
        rt_shootray(&a2);
	
	/*
	 * Finally, compare the values. If they differ, record this
	 * point as lying on an edge.
	 */
	if (is_edge (ap, &me, &left, &below)) {
	    bu_semaphore_acquire (RT_SEM_RESULTS);	
	    	scanline[cpu][ap->a_x*3+RED] = foreground[RED];
		scanline[cpu][ap->a_x*3+GRN] = foreground[GRN];
		scanline[cpu][ap->a_x*3+BLU] = foreground[BLU];
	    bu_semaphore_release (RT_SEM_RESULTS);
	    edge = 1;
	} else {
	    bu_semaphore_acquire (RT_SEM_RESULTS);
	       	scanline[cpu][ap->a_x*3+RED] = background[RED];
		scanline[cpu][ap->a_x*3+GRN] = background[GRN];
		scanline[cpu][ap->a_x*3+BLU] = background[BLU];
	    bu_semaphore_release (RT_SEM_RESULTS);
	    edge = 0;
	}
	return edge;
}

void application_init () { }
