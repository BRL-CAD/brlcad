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
 *  
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
 * All rights reserved.  */
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

#define MISS_DIST	MAX_FASTF
#define MISS_ID		-1

static unsigned char *edges[MAX_PSW];
static unsigned char *scanline[MAX_PSW];
static unsigned char *blendline[MAX_PSW];

int   		nEdges = 0;
int   		nPixels = 0;
fastf_t		max_dist; /* min. distance for drawing pits/mountains */
fastf_t		maxangle; /* Value of the cosine of the angle between
			   * surface normals that triggers shading 
			   */

typedef int color[3];
color	foreground = { 255, 255, 255};
color	background = { 0, 0, 1};

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
int     detect_attributes = 0; /* unsupported yet */

RGBpixel bg_color;

/*
 * Overlay Mode
 *
 * If set, and the fbio points to a readable framebuffer, only
 * edge pixels are splatted. This allows rtedge to overlay edges
 * directly rather than having to use pixmerge.
 */
int    overlay = 0;

/*
 * Blend Mode
 * 
 * If set, and the fbio points to a readable framebuffer, the edge
 * pixels are blended (using some HSV manipulations) with the 
 * original framebuffer pixels. The intent is to produce an effect
 * similar to the "bugs" on TV networks.
 *
 * Doesn't work worth beans!
 */
int    blend = 0;

/*
 * Region Colors Mode
 * 
 * If set, the color of edge pixels is set to the region colors.
 * If the edge is determined because of a change from one region 
 * to another, the color selected is the one from the region with
 * the lowest hit distance.
 */
int    region_colors = 0;

/*
 * Interlay Mode
 *
 * This is really cool! Interlay allows the user to specify a second
 * set of objects (from the same .g) that can be used to separate fore-
 * ground from background.
 */
struct bu_vls interlay_objects;
int interlay = 0;
struct rt_i *inter_rtip = NULL;
struct application **inter_apps;
static int inter_hit (struct application *, struct partition *, struct seg *);
static int inter_miss (struct application *);

/*
 * Prototypes for the viewedge edge detection functions
 */
static int is_edge(struct application *, struct cell *, struct cell *,
	struct cell *below);
static int rayhit (struct application *, struct partition *, struct seg *);
static int rayhit2 (struct application *, struct partition *, struct seg *);
static int raymiss (struct application *);
static int raymiss2 (struct application *);
static int handle_main_ray(struct application *, struct partition *, 
			   struct seg *);
static int diffpixel (RGBpixel a, RGBpixel b);
static void choose_color (RGBpixel col, struct cell *me,
			  struct cell *left, struct cell *below);

#define COSTOL 0.91    /* normals differ if dot product < COSTOL */
#define OBLTOL 0.1     /* high obliquity if cosine of angle < OBLTOL ! */
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
  {"%d", 3, "background", byteoffset(background), BU_STRUCTPARSE_FUNC_NULL},
  {"%d", 3, "bg", byteoffset(background), BU_STRUCTPARSE_FUNC_NULL},	
  {"%d", 1, "overlay", byteoffset(overlay), BU_STRUCTPARSE_FUNC_NULL},
  {"%d", 1, "ov", byteoffset(overlay), BU_STRUCTPARSE_FUNC_NULL},
  {"%d", 1, "blend", byteoffset(blend), BU_STRUCTPARSE_FUNC_NULL},
  {"%d", 1, "bl", byteoffset(blend), BU_STRUCTPARSE_FUNC_NULL},
  {"%d", 1, "region_color", byteoffset(region_colors), 
   BU_STRUCTPARSE_FUNC_NULL},
  {"%d", 1, "rc", byteoffset(region_colors), BU_STRUCTPARSE_FUNC_NULL},
  {"%S", 1, "interlay", byteoffset(interlay_objects), 
   BU_STRUCTPARSE_FUNC_NULL},
  {"%S", 1, "in", byteoffset(interlay_objects), 
   BU_STRUCTPARSE_FUNC_NULL},
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
 -R                 Do not report overlaps\n\
 -c                 Auxillary commands (see man page)\n";
 

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

  /*
   * Does the user want interlay?
   * 
   * If so, load and prep.
   */
  if (bu_vls_strlen(&interlay_objects) != 0) {
    /*    char idbuf[512]; */
    struct db_i *dbip;
    int nObjs;
    char **objs;
    int i;

    bu_log ("rtedge: loading occlusion geometry from %s.\n", file);

    if (Tcl_SplitList (NULL, bu_vls_addr (&interlay_objects), &nObjs, 
		       &objs) == TCL_ERROR) {
      bu_bomb ("rtedge: could not parse interlay objects list.\n");
    }


    for (i=0; i<nObjs; ++i) {
      bu_log ("object %d = %s\n", i, objs[i]);
    }
    

    if( (dbip = db_open( file, "r" )) == DBI_NULL ) {
      bu_bomb ("rtedge: could not open database.\n");
    }
    RT_CK_DBI(dbip);

    
    inter_rtip = rt_new_rti( dbip );		/* clones dbip */
    db_close(dbip);				/* releases original dbip */

    /*
     *if ((inter_rtip = rt_dirbuild(file, idbuf, sizeof(idbuf))) == RTI_NULL) {
     *bu_log ("rtedge: dirbuild failed for auxillary file %s.\n",
     *ap->a_rt_i->rti_dbip->dbi_filename);
     *bu_bomb ("rtedge: goodbye!\n");
     *}
    */

    bu_log ("rtedge: occlusion rt_dirbuild done.\n");

    for (i=0; i<nObjs; ++i) {
      if (rt_gettree (inter_rtip, objs[i]) < 0) {
	bu_log ("rtedge: gettree failed for %s\n", objs[i]);
      }
    }

    bu_log ("rtedge: occlusion rt_gettrees done.\n");

    rt_prep (inter_rtip);

    bu_log ("rtedge: occlustion prep done.\n");

    /*
     * Create a set of application structures for the interlay
     * geometry. Need one per cpu, the upper half does the per-
     * thread allocation in worker, but that's off limits.
     */
    inter_apps = bu_calloc (npsw, sizeof(struct application *), 
			    "interlay application structure array");
    for (i=0; i<npsw; ++i) {
      inter_apps[i] = bu_calloc (1, sizeof(struct application), 
				 "inter application structure");

      inter_apps[i]->a_rt_i = inter_rtip;
      inter_apps[i]->a_onehit = 1;
      inter_apps[i]->a_hit = inter_hit;
      inter_apps[i]->a_miss = inter_miss;

    }    		       
    interlay = 1;
    bu_log ("rtedge: will perform occlusion testing.\n");
  }
 
  if( minus_o ) {
    /*
     * Output is to a file stream.
     * Do not open a framebuffer, do not allow parallel
     * processing since we can't seek to the rows.
     */
    rt_g.rtg_parallel = 0;
    bu_log ("view_init: deactivating parallelism due to minus_o.\n");
    /*
     * The overlay and blend cannot be used in -o mode.
     * Note that the overlay directive takes precendence, they
     * can't be used together.
     */
    overlay = 0;
    blend = 0;
    bu_log ("view_init: deactivating overlay and blending due to minus_o.\n");
    return(0);
  }
  else if (overlay) {      
    bu_log ("view_init: will perform simple overlay.\n");
  }
  else if (blend) {
    bu_log ("view_init: will perform blending.\n");
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
   * Create a edge flag buffer for each processor.
   */
  for ( i = 0; i < npsw; ++i ) {
    if (edges[i] == NULL) {
      edges[i] = (unsigned char *) bu_calloc( 1, per_processor_chunk, 
					      "edges buffer" );
    }	
  }

  /*
   * Use three bytes per pixel.
   */
  pixsize = 3;
  
  /*
   * Create a scanline buffer for each processor.
   */
  for ( i = 0; i < npsw; ++i ) {
    if (scanline[i] == NULL) {
      scanline[i] = (unsigned char *) bu_calloc( per_processor_chunk, 
						 pixsize, "scanline buffer" );
    }	
  }
  
  /*
   * Set the hit distance difference necessary to trigger an edge.
   * This algorythm was stolen from lgt, I may make it settable later.
   */
  max_dist = (cell_width*ARCTAN_87)+2;
  
  /*
   * Determine if the framebuffer is readable.
   */
  if (overlay || blend) {
    
    if (fb_read(fbp,0,0,bg_color,1) < 0) {
      bu_log ("rt_edge: specified framebuffer is not readable, cannot merge.\n");
      overlay = 0;
      blend = 0;
    }
    
    /*
     * If blending is desired, create scanline buffers to hold
     * the read-in lines from the framebuffer.
     */
    if (blend) {
      for (i = 0; i < npsw; ++i) {
	if (blendline[i] == NULL) {
	  blendline[i] = (unsigned char *) bu_calloc( per_processor_chunk, 
						      pixsize, 
						      "blend buffer" );
	}	
      }
    }
  }

  return;
}

/* end of each pixel */
void view_pixel( struct application *ap ) { }


/*
 * view_eol - action performed at the end of each scanline
 *
 */
void
view_eol( struct application *ap )
{
  int cpu = ap->a_resource->re_cpu;
  int i;

  if (overlay) {
    /*
     * Overlay mode. Check if the pixel is an edge.
     * If so, write it to the framebuffer.
     */
    for (i = 0; i < per_processor_chunk; ++i) {
      if (edges[cpu][i]) {
	/*
	 * Write this pixel
	 */
	bu_semaphore_acquire (BU_SEM_SYSCALL);
	fb_write(fbp, i, ap->a_y, &scanline[cpu][i*3], 1);	  
	bu_semaphore_release (BU_SEM_SYSCALL);
      }
    }
  }
  else if (blend) {
    /*
     * Blend mode.
     *
     * Read a line from the existing framebuffer,
     * convert to HSV, manipulate, and put the results
     * in the scanline as RGB.
     */
    int replace_down = 0; /* flag that specifies if the pixel in the
			   * scanline below must be replaced.
			   */
    RGBpixel rgb;
    fastf_t hsv[3];
    
    bu_semaphore_acquire (BU_SEM_SYSCALL);
    if (fb_read(fbp,0,ap->a_y,blendline[cpu],per_processor_chunk) < 0) {
      bu_bomb ("rtedge: error reading from framebuffer.\n");
    }
    bu_semaphore_release (BU_SEM_SYSCALL);
    
    for (i=0; i<per_processor_chunk; ++i) {
      /*
       * Is this pixel an edge?
       */
      if (edges[cpu][i]) {

	/*
	 * The pixel is an edge, retrieve the appropriate
	 * pixel from the line buffer and convert it to HSV.
	 */
	rgb[RED] = blendline[cpu][i*3+RED];
	rgb[GRN] = blendline[cpu][i*3+GRN];
	rgb[BLU] = blendline[cpu][i*3+BLU];

	/*
	 * Is the pixel in the blendline array the
	 * background color? If so, look left and down
	 * to determine which pixel is the "source" of the
	 * edge. Unless, of course, we are on the bottom 
	 * scanline or the leftmost column (x=y=0)
	 */
	if (i != 0 && ap->a_y != 0 && !diffpixel (rgb,bg_color)) {
	  RGBpixel left;
	  RGBpixel down;
	  
	  left[RED] = blendline[cpu][(i-1)*3+RED];
	  left[GRN] = blendline[cpu][(i-1)*3+GRN];
	  left[BLU] = blendline[cpu][(i-1)*3+BLU];

	  bu_semaphore_acquire (BU_SEM_SYSCALL);
	  fb_read (fbp, i, ap->a_y - 1, down, 1);	  
	  bu_semaphore_release (BU_SEM_SYSCALL);

	  if (diffpixel (left, bg_color)) {
	    /* 
	     * Use this one.
	     */
	    rgb[RED] = left[RED];
	    rgb[GRN] = left[GRN];
	    rgb[BLU] = left[BLU];
	  }
	  else if (diffpixel (down, bg_color)) {
	    /*
	     * Use the pixel from the scanline below
	     */
	    replace_down = 1;

	    rgb[RED] = down[RED];
	    rgb[GRN] = down[GRN];
	    rgb[BLU] = down[BLU];
	  }
	}
	/*
	 * Convert to HSV
	 */
	bu_rgb_to_hsv (rgb, hsv);
      
	/*
	 * Now perform the manipulations.
	 */
	hsv[VAL] *= 3.0;
	hsv[SAT] /= 3.0;

	if (hsv[VAL] > 1.0) {
	  fastf_t d = hsv[VAL] - 1.0;
	  
	  hsv[VAL] = 1.0;
	  hsv[SAT] -= d;
	  hsv[SAT] = hsv[SAT] >= 0.0 ? hsv[SAT] : 0.0;
	}

	/*
	 * Convert back to RGB.
	 */
	bu_hsv_to_rgb(hsv,rgb);
	
	if (replace_down) {
	  /* 
	   * Write this pixel immediately, do not put it into 
	   * the blendline since it corresponds to the wrong
	   * scanline.
	   */
	  bu_semaphore_acquire (BU_SEM_SYSCALL);
	  fb_write (fbp, i, ap->a_y, rgb, 1);
	  bu_semaphore_release (BU_SEM_SYSCALL);

	  replace_down = 0;
	} 
	else {
	  /* 
	   * Put this pixel back into the blendline array.
	   * We'll push it to the buffer when the entire
	   * scanline has been processed.
	   */
	  blendline[cpu][i*3+RED] = rgb[RED];
	  blendline[cpu][i*3+GRN] = rgb[GRN];
	  blendline[cpu][i*3+BLU] = rgb[BLU];
	}
      } /* end "if this pixel is an edge" */
    } /* end pixel loop */

    /* 
     * Write the blendline to the framebuffer.
     */
    bu_semaphore_acquire (BU_SEM_SYSCALL);
    fb_write (fbp, 0, ap->a_y, blendline[cpu], per_processor_chunk);	  
    bu_semaphore_release (BU_SEM_SYSCALL);
  } /* end blend */
  else if( fbp != FBIO_NULL ) {
    /*
     * Simple whole scanline write to a framebuffer.
     */
    bu_semaphore_acquire (BU_SEM_SYSCALL);
    fb_write( fbp, 0, ap->a_y, scanline[cpu], per_processor_chunk );
    bu_semaphore_release (BU_SEM_SYSCALL);
  }
  else if( outfp != NULL ) {
    /*
     * Write to a file.
     */
    bu_semaphore_acquire (BU_SEM_SYSCALL);
    fwrite( scanline[cpu], pixsize, per_processor_chunk, outfp );
    bu_semaphore_release (BU_SEM_SYSCALL);
  }
  else {
    bu_log ("rtedge: strange, no end of line actions taken.\n");
  } 
  
  return;

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
  int could_be = 0;
  int cpu = ap->a_resource->re_cpu;	


  if( here->c_id == -1 && left->c_id == -1 && below->c_id == -1) {
    /*
     * All misses - catches condtions that would be bad later.
     */
    return 0;
  }
  
  if (detect_ids) {
    if (here->c_id != -1 &&
	(here->c_id != left->c_id || here->c_id != below->c_id)) {
      could_be = 1;
    }
  }
  
  if (detect_regions) {
    if (here->c_region != 0 &&
	(here->c_region != left->c_region
	 || here->c_region != below->c_region)) {
      could_be = 1;
    }
  }
  
  if (detect_distance) {
    if (Abs(here->c_dist - left->c_dist) > max_dist ||
	Abs(here->c_dist - below->c_dist) > max_dist) {
      could_be = 1;
    }
  }

  if (detect_normals) {
    if ((VDOT(here->c_normal, left->c_normal) < COSTOL) ||
	(VDOT(here->c_normal, below->c_normal)< COSTOL)) {
      could_be = 1;	
    }
  }
  
  if (could_be && interlay) {
    /*
     * Test the hit distance on the second geometry.
     * If the second geometry is closer, do not
     * color pixel
     */

    //    configure ray/ap;
    inter_apps[cpu]->a_resource = ap->a_resource;
    VMOVE (inter_apps[cpu]->a_ray.r_dir, ap->a_ray.r_dir);
    //    shoot;
    rt_shootray (inter_apps[cpu]);
    //    compare;
    if (inter_apps[cpu]->a_dist <= here->c_dist) {
      /* 
       * The second geometry is close than the edge, therefore it
       * is 'foreground'. Do not draw the edge.
       */
      could_be = 0; 
    }    
  }

  return could_be;
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
  
  static struct cell            saved[MAX_PSW];

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
    me.c_region = 0;
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
    me.c_region = pp->pt_regionp;
    VMOVE(me.c_rdir, ap->a_ray.r_dir);
    VJOIN1(me.c_hit, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir );
    RT_HIT_NORMAL(me.c_normal, hitp,
		  pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);	       
  }
  
  /*
   * Now, fire a ray for both the cell below and if necessary, the
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
  
  if (ap->a_x == 0) {
    /*
     * For the first pixel in a scanline, we have to shoot to the left.
     * For each pixel afterword, we save the current cell info to be used
     * as the left side cell info for the following pixel
     */
    VSUB2(a2.a_ray.r_pt, ap->a_ray.r_pt, dx_model); /* left */
    VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
    a2.a_uptr = (genptr_t)&left;
    rt_shootray(&a2);
  }
  else {
    left.c_ishit    = saved[cpu].c_ishit;
    left.c_id = saved[cpu].c_id;
    left.c_dist = saved[cpu].c_dist;
    left.c_region = saved[cpu].c_region;
    VMOVE (left.c_rdir, saved[cpu].c_rdir);
    VMOVE (left.c_hit, saved[cpu].c_hit);
    VMOVE (left.c_normal, saved[cpu].c_normal);
  }
  
  /*
   * Finally, compare the values. If they differ, record this
   * point as lying on an edge.
   */
  if (is_edge (ap, &me, &left, &below)) {
    RGBpixel col;
    
    /*
     * Don't test.
     */
    edges[cpu][ap->a_x] = 1;
    
    choose_color (col, &me, &left, &below);
    
    scanline[cpu][ap->a_x*3+RED] = col[RED];
    scanline[cpu][ap->a_x*3+GRN] = col[GRN];
    scanline[cpu][ap->a_x*3+BLU] = col[BLU];
    edge = 1;
  } else {
    edges[cpu][ap->a_x] = 0;

    scanline[cpu][ap->a_x*3+RED] = background[RED];
    scanline[cpu][ap->a_x*3+GRN] = background[GRN];
    scanline[cpu][ap->a_x*3+BLU] = background[BLU];	  

    edge = 0;
  }

  /*
   * Save the cell info for the next pixel.
   */
  saved[cpu].c_ishit = me.c_ishit;
  saved[cpu].c_id = me.c_id;
  saved[cpu].c_dist = me.c_dist;
  saved[cpu].c_region = me.c_region;
  VMOVE (saved[cpu].c_rdir, me.c_rdir);
  VMOVE (saved[cpu].c_hit, me.c_hit);
  VMOVE (saved[cpu].c_normal, me.c_normal);

  return edge;
}

void application_init () { 
  bu_vls_init(&interlay_objects);
  RT_G_DEBUG |= DEBUG_DB;
}


int diffpixel (RGBpixel a, RGBpixel b)
{
  if (a[RED] != b[RED]) return 1;
  if (a[GRN] != b[GRN]) return 1;
  if (a[BLU] != b[BLU]) return 1;
  return 0;
}

/*
 *
 *
 */
void choose_color (RGBpixel col, struct cell *me,
		  struct cell *left, struct cell *below)
{
  col[RED] = foreground[RED];
  col[GRN] = foreground[GRN];
  col[BLU] = foreground[BLU];

  if (region_colors) {

    struct cell *use_this = me;

    /*
     * Determine the cell with the smallest hit distance.
     */

    use_this = (me->c_dist < left->c_dist) ? me : left ;
    use_this = (use_this->c_dist < below->c_dist) ? use_this : below ;

    if (use_this == (struct cell *)NULL) {
      bu_bomb ("Error: use_this is NULL.\n");
    }

    col[RED] = 255 * use_this->c_region->reg_mater.ma_color[RED];
    col[GRN] = 255 * use_this->c_region->reg_mater.ma_color[GRN];
    col[BLU] = 255 * use_this->c_region->reg_mater.ma_color[BLU];

  }
  return;
}

static int inter_hit (struct application *ap, struct partition *pt, 
		      struct seg *segp)
{
  struct hit		*hitp = pt->pt_forw->pt_inhit;
  
  ap->a_dist = hitp->hit_dist;
  return(1);		
}

static int inter_miss (struct application *ap)
{
  ap->a_dist = MAX_FASTF;
  return(1);		
}

