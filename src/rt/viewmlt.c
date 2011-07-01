/*                       V I E W M L T . C
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
/** @file rt/viewmlt.c
 *
 * Implementation of the Metropolis Light Transport algorithm.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "photonmap.h"
#include "light.h"
#include "raytrace.h"
#include "fb.h"

#include "./scanline.h"
#include "./rtuif.h"
#include "./ext.h"


static int	buf_mode=0;
#define BUFMODE_UNBUF	1		/* No output buffering */
#define BUFMODE_DYNAMIC	2		/* Dynamic output buffering */
#define BUFMODE_INCR	3		/* incr_mode set, dynamic buffering */
#define BUFMODE_RTSRV	4		/* output buffering into scanbuf */
#define BUFMODE_FULLFLOAT 5		/* buffer entire frame as floats */
#define BUFMODE_SCANLINE 6		/* Like _DYNAMIC, one scanline/cpu */

/* Local communication a.la. worker() */
extern int per_processor_chunk;	/* how many pixels to do at once */
extern int cur_pixel;		/* current pixel number, 0..last_pixel */
extern int last_pixel;		/* last pixel number */

int ibackground[3] = {0};
int inonbackground[3] = {0};
static short int pwidth;			/* Width of each pixel (in bytes) */
static struct scanline* scanline;   /* From scanline.c */
struct mfuncs *mfHead = MF_NULL;	/* Head of list of shaders */

extern FBIO* fbp;

const char title[] = "Metropolis Light Transport renderer";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects... > file.pix\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees\n");
    bu_log(" -e Elev	Elevation in degrees\n");
    bu_log(" -M		Read matrix, cmds on stdin\n");
    bu_log(" -o file.pix	Output file name\n");
    bu_log(" -x #		Set librt debug flags\n");
}


int	rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int	secondary_hit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int	raymiss(register struct application *ap);
void reproject_worker(int cpu, genptr_t arg);

/* From scanline.c */
void free_scanlines(int, struct scanline*);
struct scanline* alloc_scanlines(int);

struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

struct point_list {
    struct bu_list l;
    point_t pt_cell;
    int color[3];
};

struct path_list {
    struct bu_list l;
    struct point_list *pt_list;
};


/* This structure will hold information relevant to the algorithm.
 * It will be pointed by ap->a_user 
 */
struct mlt_app {
    struct path_list * paths;       /** @brief Current path */
    point_t eye;        /** @brief Position of the camera */
    struct point_list * lightSources;   /** @brief List of lightsource points */
    struct point_list * nextPoint;  /** @brief Pointer used in rayhit() to alternate 
                                     * between shooting from the camera and from
                                     * the lightsource */
    genptr_t m_uptr;    /** @brief Generic pointer that will be used to hold useful
                         * information that can't be stored in the application structure,
                         * due to the application's pointer already being used to point
                         * to the mlt structure.
                         */
    int m_user;     /** @brief Application specific information */
};

/*
 *  			V I E W _ I N I T
 *
 *  Called by main() at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(struct application *UNUSED(ap), char *UNUSED(file), char *UNUSED(obj), int UNUSED(minus_o), int UNUSED(minus_F))
{
     return 1;		/* framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  The beginning of a frame.
 *  Called by do_frame() just before raytracing starts.
 */
void
view_2init(struct application *ap, char *UNUSED(framename))
{
    int i;
    struct mlt_app* mlt_application;
        
    if (outputfile)
	bif = bu_image_save_open(outputfile, BU_IMAGE_AUTO, width, height, 3);

    /* Initialization of the mlt application structure */
    mlt_application = (struct mlt_app*) bu_malloc(sizeof(struct mlt_app), "mlt application");
    mlt_application->lightSources = (struct point_list*) NULL;
    mlt_application->paths = (struct path_list*) NULL;
    mlt_application->m_uptr = (genptr_t) NULL;
    /*mlt_application->m_user = 0;*/

    /* Setting application callback functions and
     * linkage to the mlt application */
    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 1;
    ap->a_uptr = (genptr_t) mlt_application;

    /* Allocation of the scanline array */
    if (scanline)
        free_scanlines(height, scanline);
    scanline = alloc_scanlines(height);


#ifdef RTSRV
    buf_mode = BUFMODE_RTSRV;		/* multi-pixel buffering */
#else
    if (fullfloat_mode)  {
	    buf_mode = BUFMODE_FULLFLOAT;
    } else if (incr_mode)  {
	    buf_mode = BUFMODE_INCR;
    } else if (width <= 96)  {
	    buf_mode = BUFMODE_UNBUF;
    } else if ((size_t)npsw <= height/4)  {
	    /* Have each CPU do a whole scanline.
	     * Saves lots of semaphore overhead.
	     * For load balancing make sure each CPU has several lines to do.
	     */
	    per_processor_chunk = width;
	    buf_mode = BUFMODE_SCANLINE;
    }  else  {
        buf_mode = BUFMODE_DYNAMIC;
    }
#endif

    switch ( buf_mode )  {
	case BUFMODE_UNBUF:
	    bu_log("Single pixel I/O, unbuffered\n");
	    break;
	case BUFMODE_FULLFLOAT:
	    if ( !curr_float_frame )  {
		bu_log("mallocing curr_float_frame\n");
		curr_float_frame = (struct floatpixel *)bu_malloc(
		    width * height * sizeof(struct floatpixel),
		    "floatpixel frame");
	    }

	    /* Mark entire current frame as "not computed" */
	    {
		register struct floatpixel	*fp;

		for ( fp = &curr_float_frame[width*height-1];
		      fp >= curr_float_frame; fp--
		    ) {
		    fp->ff_frame = -1;
		}
	    }

	    /* Reproject previous frame */
	    if ( prev_float_frame && reproject_mode )  {
		reproj_cur = 0;	/* incremented by reproject_worker */
		reproj_max = width*height;

		cur_pixel = 0;
		last_pixel = width*height-1;
		if ( npsw == 1 )
		    reproject_worker(0, NULL);
		else
		    bu_parallel( reproject_worker, npsw, NULL );
	    } else {
		reproj_cur = reproj_max = 0;
	    }
	    break;
#ifdef RTSRV
	case BUFMODE_RTSRV:
	    scanbuf = bu_malloc( srv_scanlen*pwidth + sizeof(long),
				 "scanbuf [multi-line]" );
	    break;
#endif
	case BUFMODE_INCR:
	{
	    register int j = 1<<incr_level;
	    register int w = 1<<(incr_nlevel-incr_level);

	    bu_log("Incremental resolution %d\n", j);

	    /* Diminish buffer expectations on work-saved lines */
	    for ( i=0; i<j; i++ )  {
		if ( sub_grid_mode )  {
		    /* ???? */
		    if ( (i & 1) == 0 )
			scanline[i*w].sl_left = j/2;
		    else
			scanline[i*w].sl_left = j;
		} else {
		    if ( (i & 1) == 0 )
			scanline[i*w].sl_left = j/2;
		    else
			scanline[i*w].sl_left = j;
		}
	    }
	}
	if ( incr_level > 1 )
	    return;		 /* more res to come */
	break;

	case BUFMODE_SCANLINE:
	    bu_log("Low overhead scanline-per-CPU buffering\n");
	    /* Fall through... */
	case BUFMODE_DYNAMIC:
	    if ((buf_mode == BUFMODE_DYNAMIC) && (rt_verbosity & VERBOSE_OUTPUTFILE)) {
		    bu_log("Dynamic scanline buffering\n");
	    }

	    if (sub_grid_mode)  {
		for (i = sub_ymin; i <= sub_ymax; i++)
		    scanline[i].sl_left = sub_xmax-sub_xmin + 1;
	    } else {
		for (i = 0; (size_t)i < height; i++)
		    scanline[i].sl_left = width;
	    }
    
	    break;
	default:
	    bu_exit(EXIT_FAILURE, "bad buf_mode");
    }


    /* Setting the pixel size */
    pwidth = 3 + ((rpt_dist) ? (8) : (0));

    /* BUFMODE_SCANLINE
     * This will be expanded to handle multiple BUFMODES,
     * as in view.c */
    /*if (sub_grid_mode)  {
      for (i = sub_ymin; i <= sub_ymax; i++)
      scanline[i].sl_left = sub_xmax-sub_xmin+1;
      }
      else {
      for (i = 0; i < height; i++)
      scanline[i].sl_left = width;
    }*/


    /* Setting Lights if the user did not specify */
    if (BU_LIST_IS_EMPTY(&(LightHead.l))
	|| (!BU_LIST_IS_INITIALIZED(&(LightHead.l)))) {
	if (R_DEBUG&RDEBUG_SHOWERR) bu_log("No explicit light\n");
	light_maker(1, view2model);
    }

    ap->a_rt_i->rti_nlights = light_init(ap);
}

/*
 *			V I E W _ P I X E L
 *
 *  Called by worker() after the end of proccessing for each pixel.
 */
void
view_pixel(register struct application *ap) 
{
    register int	r, g, b, do_eol = 0;
    register unsigned char *pixelp;
    register struct scanline *slp;
    unsigned char	dist[8];	/* pixel distance (in IEEE format) */

    if (ap->a_user == 0) {
        r = ibackground[0];
        g = ibackground[1];
        b = ibackground[2];
        VSETALL(ap->a_color, -1e-20); 
    }
    else {
        /* Setting r, g and b according to values found in rayhit()
         * Gamma correction will be implemented here. */
        r = ap->a_color[0]*255. + bn_rand0to1(ap->a_resource->re_randptr);
        g = ap->a_color[1]*255. + bn_rand0to1(ap->a_resource->re_randptr);
        b = ap->a_color[2]*255. + bn_rand0to1(ap->a_resource->re_randptr);

        /* Restricting r, g and b values to 0..255 */
        r = (r > 255) ? (255) : ((r < 0) ? (0) : (r));
        g = (g > 255) ? (255) : ((g < 0) ? (0) : (g));
        b = (b > 255) ? (255) : ((b < 0) ? (0) : (b));

        if (r == ibackground[0] &&
            g == ibackground[1] &&
            b == ibackground[2]) {
            
            r = inonbackground[0];
            g = inonbackground[1];
            b = inonbackground[2];
        }

        /* Make sure that it's never perfect black */
        if ((benchmark == 0) && (r == 0) && (g == 0) && (b == 0)) {
            b = 1;
        }
    }/*

	slp = &scanline[ap->a_y];
	if (slp->sl_buf == ((char*) 0))  {
		slp->sl_buf = bu_calloc(width, pwidth, "sl_buf scanline buffer");
	}
	pixelp = slp->sl_buf + (ap->a_x * pwidth);
	*pixelp++ = r;
	*pixelp++ = g;
	*pixelp++ = b;

    if (--(slp->sl_left) <= 0)
        do_eol = 1;
*/
    switch (buf_mode)  {
	case BUFMODE_FULLFLOAT:
	{
	    /* No output semaphores required for word-width memory writes */
	    register struct floatpixel	*fp;
	    fp = &curr_float_frame[ap->a_y*width + ap->a_x];
	    fp->ff_frame = curframe;
	    fp->ff_color[0] = r;
	    fp->ff_color[1] = g;
	    fp->ff_color[2] = b;
	    fp->ff_x = ap->a_x;
	    fp->ff_y = ap->a_y;
	    if ( ap->a_user == 0 )  {
		fp->ff_dist = -INFINITY;	/* shot missed model */
		fp->ff_frame = -1;		/* Don't cache misses */
		return;
	    }
	    /* XXX a_dist is negative and misleading when eye is in air */
	    fp->ff_dist = (float)ap->a_dist;
	    VJOIN1( fp->ff_hitpt, ap->a_ray.r_pt,
		    ap->a_dist, ap->a_ray.r_dir );
	    fp->ff_regp = (struct region *)ap->a_uptr;
	    RT_CK_REGION(fp->ff_regp);

	    /*
	     *  This pixel was just computed.
	     *  Look at next pixel on scanline,
	     *  and if it is a reprojected old value
	     *  and hit a different region than this pixel,
	     *  then recompute it too.
	     */
	    if ( (size_t)ap->a_x >= width-1 )  return;
	    if ( fp[1].ff_frame <= 0 )  return;	/* not valid, will be recomputed. */
	    if ( fp[1].ff_regp == fp->ff_regp )
		return;				/* OK */
	    /* Next pixel is probably out of date, mark it for re-computing */
	    fp[1].ff_frame = -1;
	    return;
	}

	case BUFMODE_UNBUF:
	{
	    RGBpixel	p;
	    int		npix;

	    p[0] = r;
	    p[1] = g;
	    p[2] = b;

	    if ( outfp != NULL )  {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		if ( fseek( outfp, (ap->a_y*width*pwidth) + (ap->a_x*pwidth), 0 ) != 0 )
		    fprintf(stderr, "fseek error\n");
		if ( fwrite( p, 3, 1, outfp ) != 1 )
		    bu_exit(EXIT_FAILURE, "pixel fwrite error");
		if ( rpt_dist &&
		     ( fwrite( dist, 8, 1, outfp ) != 1 ))
		    bu_exit(EXIT_FAILURE, "pixel fwrite error");
		bu_semaphore_release( BU_SEM_SYSCALL);
	    }

	    if ( fbp != FBIO_NULL )  {
		/* Framebuffer output */
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		npix = fb_write( fbp, ap->a_x, ap->a_y,
				 (unsigned char *)p, 1 );
		bu_semaphore_release( BU_SEM_SYSCALL);
		if ( npix < 1 )  bu_exit(EXIT_FAILURE, "pixel fb_write error");
	    }
	}
	return;

#ifdef RTSRV
	case BUFMODE_RTSRV:
	    /* Multi-pixel buffer */
	    pixelp = scanbuf+ pwidth *
		((ap->a_y*width) + ap->a_x - srv_startpix);
	    bu_semaphore_acquire( RT_SEM_RESULTS );
	    *pixelp++ = r;
	    *pixelp++ = g;
	    *pixelp++ = b;
	    if (rpt_dist)
	    {
		*pixelp++ = dist[0];
		*pixelp++ = dist[1];
		*pixelp++ = dist[2];
		*pixelp++ = dist[3];
		*pixelp++ = dist[4];
		*pixelp++ = dist[5];
		*pixelp++ = dist[6];
		*pixelp++ = dist[7];
	    }
	    bu_semaphore_release( RT_SEM_RESULTS );
	    return;
#endif

	    /*
	     *  Store results into pixel buffer.
	     *  Don't depend on interlocked hardware byte-splice.
	     *  Need to protect scanline[].sl_left when in parallel mode.
	     */

	case BUFMODE_DYNAMIC:
	    slp = &scanline[ap->a_y];
	    bu_semaphore_acquire(RT_SEM_RESULTS);
	    if (slp->sl_buf == (unsigned char *)0)  {
		    slp->sl_buf = bu_calloc(width, pwidth, "sl_buf scanline buffer");
	    }

	    pixelp = slp->sl_buf+(ap->a_x*pwidth);
	    *pixelp++ = r;
	    *pixelp++ = g;
	    *pixelp++ = b;

	    if (rpt_dist) {
		    *pixelp++ = dist[0];
		    *pixelp++ = dist[1];
		    *pixelp++ = dist[2];
		    *pixelp++ = dist[3];
		    *pixelp++ = dist[4];
		    *pixelp++ = dist[5];
		    *pixelp++ = dist[6];
		    *pixelp++ = dist[7];
	    }
	    if (--(slp->sl_left) <= 0)
		    do_eol = 1;

	    bu_semaphore_release(RT_SEM_RESULTS);
	    break;

	    /*
	     *  Only one CPU is working on this scanline,
	     *  no parallel interlock required!  Much faster.
	     */
	case BUFMODE_SCANLINE:
	    slp = &scanline[ap->a_y];
	    if (slp->sl_buf == (unsigned char *) 0)  {
		    slp->sl_buf = bu_calloc(width, pwidth, "sl_buf scanline buffer");
	    }
	    pixelp = slp->sl_buf+(ap->a_x*pwidth);
	    *pixelp++ = r;
	    *pixelp++ = g;
	    *pixelp++ = b;

        if (rpt_dist) {
		    *pixelp++ = dist[0];
		    *pixelp++ = dist[1];
		    *pixelp++ = dist[2];
		    *pixelp++ = dist[3];
		    *pixelp++ = dist[4];
		    *pixelp++ = dist[5];
		    *pixelp++ = dist[6];
		    *pixelp++ = dist[7];
	    }
	    if (--(slp->sl_left) <= 0)
    		do_eol = 1;
	    break;

	case BUFMODE_INCR:
	{
	    register int dx, dy;
	    register int spread;

	    spread = 1<<(incr_nlevel-incr_level);

	    bu_semaphore_acquire( RT_SEM_RESULTS );
	    for ( dy=0; dy<spread; dy++ )  {
		if ( (size_t)ap->a_y+dy >= height )  break;
		slp = &scanline[ap->a_y+dy];
		if ( slp->sl_buf == (unsigned char *)0 )
		    slp->sl_buf = bu_calloc( width+32,
					     pwidth, "sl_buf scanline buffer" );

		pixelp = slp->sl_buf+(ap->a_x*pwidth);
		for ( dx=0; dx<spread; dx++ )  {
		    *pixelp++ = r;
		    *pixelp++ = g;
		    *pixelp++ = b;
		    if (rpt_dist)
		    {
			*pixelp++ = dist[0];
			*pixelp++ = dist[1];
			*pixelp++ = dist[2];
			*pixelp++ = dist[3];
			*pixelp++ = dist[4];
			*pixelp++ = dist[5];
			*pixelp++ = dist[6];
			*pixelp++ = dist[7];
		    }
		}
	    }
	    /* First 3 incremental iterations are boring */
	    if ( incr_level > 3 )  {
		if ( --(scanline[ap->a_y].sl_left) <= 0 )
		    do_eol = 1;
	    }
	    bu_semaphore_release( RT_SEM_RESULTS );
	}
	break;

	default:
	    bu_exit(EXIT_FAILURE, "bad buf_mode");
    }
    if (!do_eol) return;
/*
    if (fbp != FBIO_NULL) {
        int npix;

        bu_semaphore_acquire(BU_SEM_SYSCALL);
        if (sub_grid_mode) {
            npix = fb_write(fbp, sub_xmin, ap->a_y,
          (unsigned char *) scanline[ap->a_y].sl_buf + 3 * sub_xmin,
                            sub_xmax - sub_xmin + 1);
        } else {
            npix = fb_write(fbp, 0, ap->a_y,
          (unsigned char *) scanline[ap->a_y].sl_buf, width);
        }
        bu_semaphore_release(BU_SEM_SYSCALL);

        if (sub_grid_mode) {
            if (npix < sub_xmax - sub_xmin - 1)
                bu_exit(EXIT_FAILURE, "scanline fb_write error");
        } else {
            if (npix < width)
                bu_exit(EXIT_FAILURE, "scanline fb_write error");
        }
    }
    if (outputfile != NULL) {
	bu_semaphore_acquire (BU_SEM_SYSCALL);
	bu_image_save_writeline(bif, ap->a_y, scanline[ap->a_y].sl_buf);
        bu_semaphore_release(BU_SEM_SYSCALL);
    }
    bu_free(scanline[ap->a_y].sl_buf, "sl_buf scanline buffer");
    scanline[ap->a_y].sl_buf = (char *) 0;*/

    switch ( buf_mode )  {
	case BUFMODE_INCR:
	    if (fbp == FBIO_NULL) bu_exit(EXIT_FAILURE, "Incremental rendering with no framebuffer?");
	    {
		register int dy, yy;
		register int spread;
		size_t npix = 0;

		spread = (1<<(incr_nlevel-incr_level))-1;
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		for ( dy=spread; dy >= 0; dy-- )  {
		    yy = ap->a_y + dy;
		    if ( sub_grid_mode )  {
			if ( dy < sub_ymin || dy > sub_ymax )
			    continue;
			npix = fb_write( fbp, sub_xmin, yy,
					 (unsigned char *)scanline[yy].sl_buf+3*sub_xmin,
					 sub_xmax-sub_xmin+1 );
			if ( npix != (size_t)sub_xmax-sub_xmin+1 )  break;
		    } else {
			npix = fb_write( fbp, 0, yy,
					 (unsigned char *)scanline[yy].sl_buf,
					 width );
			if ( npix != width )  break;
		    }
		}
		bu_semaphore_release( BU_SEM_SYSCALL);
		if ( npix != width )  bu_exit(EXIT_FAILURE, "fb_write error (incremental res)");
	    }
	    break;

	case BUFMODE_SCANLINE:
	case BUFMODE_DYNAMIC:/*
	    if ( fbp != FBIO_NULL )  {
		    int		npix;
		    bu_semaphore_acquire( BU_SEM_SYSCALL );
		    if (sub_grid_mode)  {
		        npix = fb_write( fbp, sub_xmin, ap->a_y,
				         (unsigned char *)scanline[ap->a_y].sl_buf+3*sub_xmin,
				         sub_xmax-sub_xmin+1 );
		    } else {
		        npix = fb_write( fbp, 0, ap->a_y,
				         (unsigned char *)scanline[ap->a_y].sl_buf, width );
		    }
		    bu_semaphore_release( BU_SEM_SYSCALL);
		    if ( sub_grid_mode )  {
		        if ( npix < sub_xmax-sub_xmin-1 )  bu_exit(EXIT_FAILURE, "scanline fb_write error");
		    } else {
		        if ( npix < width )  bu_exit(EXIT_FAILURE, "scanline fb_write error");
		    }
	    }
	    if ( outfp != NULL )  {
		int	count;

		bu_semaphore_acquire( BU_SEM_SYSCALL );
		if ( fseek( outfp, ap->a_y*width*pwidth, 0 ) != 0 )
		    fprintf(stderr, "fseek error\n");
		count = fwrite( scanline[ap->a_y].sl_buf,
				sizeof(char), width*pwidth, outfp );
		bu_semaphore_release( BU_SEM_SYSCALL);
		if ( count != width*pwidth )
		    bu_exit(EXIT_FAILURE, "view_pixel:  fwrite failure\n");
	    }
	    bu_free( scanline[ap->a_y].sl_buf, "sl_buf scanline buffer" );
	    scanline[ap->a_y].sl_buf = (char *)0;*/

    if (fbp != FBIO_NULL) {
        size_t npix;

        bu_semaphore_acquire(BU_SEM_SYSCALL);
        if (sub_grid_mode) {
            npix = fb_write(fbp, sub_xmin, ap->a_y,
          (unsigned char *) scanline[ap->a_y].sl_buf + 3 * sub_xmin,
                            sub_xmax - sub_xmin + 1);
        } else {
            npix = fb_write(fbp, 0, ap->a_y,
          (unsigned char *) scanline[ap->a_y].sl_buf, width);
        }
        bu_semaphore_release(BU_SEM_SYSCALL);

        if (sub_grid_mode) {
            if (npix < (size_t)sub_xmax - sub_xmin - 1)
                bu_exit(EXIT_FAILURE, "scanline fb_write error");
        } else {
            if (npix < width)
                bu_exit(EXIT_FAILURE, "scanline fb_write error");
        }
    }
    if (outputfile != NULL) {
	    bu_semaphore_acquire (BU_SEM_SYSCALL);
	    bu_image_save_writeline(bif, ap->a_y, (unsigned char *)scanline[ap->a_y].sl_buf);
            bu_semaphore_release(BU_SEM_SYSCALL);
    }
    bu_free(scanline[ap->a_y].sl_buf, "sl_buf scanline buffer");
    scanline[ap->a_y].sl_buf = (unsigned char *) 0;
    
    }   /* End of case */
}



/*
 *			V I E W _ E N D
 *
 *  Called in do_frame() at the end of a frame,
 *  just after raytracing completes.
 */
void
view_end(register struct application *ap) 
{
    struct mlt_app *p_mlt;
    struct point_list *temp_point;
    struct path_list *p_path, *temp_path;
    
    p_mlt = (struct mlt_app*) ap->a_uptr;
    p_path = p_mlt->paths;
    
    /* save out the file */
    if (bif)
	bu_image_save_close(bif); 
    bif = NULL;

    /* Iterating through the path lists, freeing every point list entry */
    while (BU_LIST_WHILE(temp_path, path_list, &(p_path->l))) {
        while (BU_LIST_WHILE(temp_point, point_list, &(temp_path->pt_list->l))) {
	        BU_LIST_DEQUEUE(&(temp_point->l));
	        bu_free(temp_point, "freeing point list entry");
        }
        bu_free(p_path->pt_list, "freeing point list head");
	    
	    BU_LIST_DEQUEUE(&(temp_path->l));
	    bu_free(temp_path, "free path list entry");
    }
    while (BU_LIST_WHILE(temp_point, point_list, &(p_path->pt_list->l))) {
        BU_LIST_DEQUEUE(&(temp_point->l));
        bu_free(temp_point, "free point list entry");
    }
    bu_free(p_path, "free path list head");

    /* Freeing scanlines */
    if (scanline)
        free_scanlines(height, scanline);
} 

/*
 *			V I E W _ S E T U P
 *
 *  Called by do_prep(), just before rt_prep() is called, in do.c
 *  This allows the lighting model to get set up for this frame,
 *  e.g., generate lights, associate materials routines, etc.
 */
void
view_setup(struct rt_i *rtip)
{
    struct region *regp;

    RT_CHECK_RTI(rtip);
    /*
     *  Initialize the material library for all regions.
     *  As this may result in some regions being dropped,
     *  (eg, light solids that become "implicit" -- non drawn),
     *  this must be done before allowing the library to prep
     *  itself.  This is a slight layering violation;  later it
     *  may be clear how to repackage this operation.
     */
    regp = BU_LIST_FIRST(region, &rtip->HeadRegion);
    while (BU_LIST_NOT_HEAD(regp, &rtip->HeadRegion))  {
	switch (mlib_setup(&mfHead, regp, rtip)) {
	    case -1:
	    default:
		    bu_log("mlib_setup failure on %s\n", regp->reg_name);
		    break;
	    case 0:
		    if (R_DEBUG & RDEBUG_MATERIAL)
		        bu_log("mlib_setup: drop region %s\n", regp->reg_name);
		    {
		    struct region *r = BU_LIST_NEXT(region, &regp->l);
		    /* zap reg_udata? beware of light structs */
		    rt_del_regtree( rtip, regp, &rt_uniresource );
		    regp = r;
		    continue;
		}
	    case 1:
		/* Full success */
		if (R_DEBUG&RDEBUG_MATERIAL &&
		   ((struct mfuncs *)(regp->reg_mfuncs))->mf_print)  {
		   ((struct mfuncs *)(regp->reg_mfuncs))->
			mf_print( regp, regp->reg_udata );
		}
		break;
	    case 2:
		/* Full success, and this region should get dropped later */
		/* Add to list of regions to drop */
		bu_ptbl_ins( &rtip->delete_regs, (long *)regp );
		break;
	}
	regp = BU_LIST_NEXT( region, &regp->l );
    }
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called by "clean" command, just before rt_clean() is called, in do.c
 */
void
view_cleanup(struct rt_i *rtip) 
{
    struct region* regp;
    RT_CHECK_RTI(rtip);

    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	    mlib_free( regp );
    }
    if (env_region.reg_mfuncs)  {
	    bu_free( (char *)env_region.reg_name, "env_region.reg_name" );
	    env_region.reg_name = (char *)0;
	    mlib_free( &env_region );
    }

    light_cleanup();
}

/*
 *			R A Y H I T
 *
 *  Called via a_hit linkage from rt_shootray() when ray hits.
 */
int
rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
    struct mlt_app* p_mlt;
    struct path_list* p_path;
    struct hit *hitp;
    struct partition *pp;
    struct light_specific *lp;
    struct application sub_ap;

    vect_t normal, work0, new_dir;
    fastf_t	diffuse0 = 0;
    fastf_t	cosI0 = 0;
  
    /*bu_log("hit: 0x%x\n", ap->a_resource);*/
    pp = PartHeadp->pt_forw;    

    for (; pp != PartHeadp; pp = pp->pt_forw)
	    if (pp->pt_outhit->hit_dist >= 0.0) break;

    if (pp == PartHeadp) {
        bu_log("rayhit: no hit out front?");
        return 0;
    }

    hitp = pp->pt_inhit;

    /* This will be used find the hit point: */
    RT_HIT_NORMAL(normal, 
        hitp,
        pp->pt_inseg->seg_stp,
        &(ap->a_ray),
        pp->pt_inflip);

    /*
     * Diffuse reflectance from each light source
     */
    /* Light from the "eye" (ray source).  Note sign change */
	lp = BU_LIST_FIRST(light_specific, &(LightHead.l));
	diffuse0 = 0;
	if ((cosI0 = -VDOT(normal, ap->a_ray.r_dir)) >= 0.0)
	    diffuse0 = cosI0 * (1.0 - AmbientIntensity);
	VSCALE(work0, lp->lt_color, diffuse0);

    /* Preparing the direction of the new ray */
    if (BU_LIST_NOT_HEAD((&LightHead), &(LightHead.l))) {
    /*    if (p_mlt->m_user == 0) {
            bu_log("Using light position.. \n");
            p_mlt->m_user = 1;
        }
    */
        VSUB2(new_dir, lp->lt_pos, pp->pt_inhit->hit_point);
    }
    else {
    /*    if (p_mlt->m_user == 0) {
            bu_log("Using default light position.. \n");
            p_mlt->m_user = 1;
        }
    */
        VSET(new_dir, 1.0, 1.0, 1.0);   /* Placeholder value
                                         * What is the vector to the
                                         * Default light?
                                         */
    }
    VUNITIZE(new_dir);

	sub_ap = *ap;	/* struct copy */
	sub_ap.a_level = ap->a_level+1;
    sub_ap.a_hit = secondary_hit;
    sub_ap.a_miss = ap->a_miss;
    
    /* Setting up new ray */
    VMOVE(sub_ap.a_ray.r_pt, segp->seg_in.hit_point);
    VMOVE(sub_ap.a_ray.r_dir, new_dir);

	sub_ap.a_purpose = "Secondary Hit, shadows treatment";
    sub_ap.a_uptr = ap->a_uptr;
        
    (void) rt_shootray(&sub_ap);

    /* Checks whether the secondary ray hit and acts accordingly */
    if (sub_ap.a_user) {
        work0[2] = work0[2] * 2;  /* Just to note the shadowed part */
        VSCALE(ap->a_color, work0, 0.50);   /* 20% of work0 value.
                                             * Placeholder constant
                                             */
    }
    else {
        VMOVE(ap->a_color, work0);
    };    

    /*  The application uses a generic pointer (genptr_t)
     *  to point to a struct mlt_app
     *
     *  In view.c, this generic pointer is used to store visited regions
     *  information. If it proves needed here, mlt_app has a generic pointer
     *  that can handle that.
     */    
    p_mlt = (struct mlt_app*) ap->a_uptr;
    p_path = p_mlt->paths;
    /*  Once found, it will be stored in p_mlt->path_list. */
    /*  This block verifies if the path list already exists.
     *  If not, it allocates memory and initializes a new point_list
     *  and adds the hit point found to that list.
     *
     *  If it exists, this block verifies if the point list also
     *  exists and allocates memory accordingly.
     */

    if (p_path) {
        if (p_path->pt_list) {
            struct point_list* new_point;
            BU_GETSTRUCT(new_point, point_list);
                        
            VMOVE(new_point->pt_cell, segp->seg_in.hit_point);
            BU_LIST_PUSH(&(p_path->pt_list->l), &(new_point->l));
        }
        
        else {
            BU_GETSTRUCT(p_path->pt_list, point_list);
            BU_LIST_INIT(&(p_path->pt_list->l));
            VMOVE(p_path->pt_list->pt_cell, segp->seg_in.hit_point);
        }
    }
    else {
        BU_GETSTRUCT(p_path, path_list);
        BU_LIST_INIT(&(p_path->l));
        p_path->pt_list = (struct point_list *) NULL;

        BU_GETSTRUCT(p_path->pt_list, point_list);
        BU_LIST_INIT(&(p_path->pt_list->l));
        VMOVE(p_path->pt_list->pt_cell, segp->seg_in.hit_point);
    }

    p_mlt->paths = p_path;
    ap->a_uptr = (genptr_t) p_mlt;
    

    /* Use a BRDF function to set the new ap->a_ray->r_dir;
     * r_pt will be the same hitpoint found before;
     * and call rt_shootray(ap)
     */
    /*VMOVE(ap->a_ray.r_pt, segp->seg_in.hit_point);*/

    /* This will be used by view_pixel() to verify if the ray hit or missed */
    ap->a_user = 1;

    return 1;	/* report hit to main routine */
}

/*
 *      M L T _ B U I L D _ P A T H
 *
 *  This is a callback function that will be passed to ap->a_hit.
 *  It will call itself recursively and adding hit points to the
 *  mlt application structure. 
 *
 *  The stopping condition, with path tracing, is determined by the
 *  probability of the light being absorbed.
 *
 *  Also, long paths, with 5 or more points, contribute little to
 *  the final image, so those can be discarded.
 */
int
mlt_build_path(register struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
    struct hit *hitp;
    struct partition *pp;
    struct application new_app;
    struct mlt_app *p_mlt;
    struct path_list* p_path;

    vect_t normal, new_dir;

    /*  The application uses a generic pointer (genptr_t)
     *  to point to a struct mlt_app
     *
     *  In view.c, this generic pointer is used to store visited regions
     *  information. If it proves needed here, mlt_app has a generic pointer
     *  that can handle that.
     */    
    p_mlt = (struct mlt_app*) ap->a_uptr;
    p_path = p_mlt->paths;
    pp = PartHeadp->pt_forw;    

    for (; pp != PartHeadp; pp = pp->pt_forw)
        if (pp->pt_outhit->hit_dist >= 0.0)  break;

    if (pp == PartHeadp) {
        bu_log("rayhit: no hit out front?");
        ap->a_user = 0;
        return 0;
    }

    hitp = pp->pt_inhit;

    /* This is used find the hit point: */
    RT_HIT_NORMAL(normal, 
        hitp,
        pp->pt_inseg->seg_stp,
        &(ap->a_ray),
        pp->pt_inflip);

    VSET(new_dir, 1,0,1);   /* Temporary value */
    
    if (p_mlt->m_user < 5) {    /* Arbitrary stopping condition */

        /*  This block verifies if the path list already exists.
         *  If not, it allocates memory and initializes a new point_list
         *  and adds the hit point found to that list.
         *
         *  If it exists, this block verifies if the point list also
         *  exists and allocates memory accordingly.
         */

        if (p_path) {
            if (p_path->pt_list) {
                struct point_list* new_point;
                BU_GETSTRUCT(new_point, point_list);
                            
                VMOVE(new_point->pt_cell, segp->seg_in.hit_point);
                BU_LIST_PUSH(&(p_path->pt_list->l), &(new_point->l));
            }
            
            else {
                BU_GETSTRUCT(p_path->pt_list, point_list);
                BU_LIST_INIT(&(p_path->pt_list->l));
                VMOVE(p_path->pt_list->pt_cell, segp->seg_in.hit_point);
            }

        }
        else {
            BU_GETSTRUCT(p_path, path_list);
            BU_LIST_INIT(&(p_path->l));
            p_path->pt_list = (struct point_list *) NULL;

            BU_GETSTRUCT(p_path->pt_list, point_list);
            BU_LIST_INIT(&(p_path->pt_list->l));
            VMOVE(p_path->pt_list->pt_cell, segp->seg_in.hit_point);
        }

	    new_app = *ap;	/* struct copy */
	    new_app.a_level = ap->a_level+1;
        new_app.a_hit = mlt_build_path;
        new_app.a_miss = ap->a_miss;
        
        /* Setting up new ray */
        VMOVE(new_app.a_ray.r_pt, segp->seg_in.hit_point);
        VMOVE(new_app.a_ray.r_dir, new_dir);

	    new_app.a_purpose = "Path building";
        new_app.a_uptr = ap->a_uptr;
            
        (void) rt_shootray(&new_app);
    }

    ap->a_user = 1;

    return 1;

}

/*
 *      S E C O N D A R Y _ H I T
 *
 *  Called by rayhit() to handle shadows. The idea is, primarily,
 *  to shoot new rays in the direction of the light source. Then,
 *  this function is associated with the new ray. If this hits,
 *  we have a shadow (because something is obstructing the light's
 *  path). 
 *
 *  This will be changed and will do something else when path
 *  tracing development starts. For now, it should just say when
 *  it hit or not.
 */
int
secondary_hit(register struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    struct hit *hitp;
    struct partition *pp;
  
    /*bu_log("hit: 0x%x\n", ap->a_resource);*/

    pp = PartHeadp->pt_forw;    

    for (; pp != PartHeadp; pp = pp->pt_forw)
        if (pp->pt_outhit->hit_dist >= 0.0)  break;

    if (pp == PartHeadp) {
        bu_log("rayhit: no hit out front?");
        ap->a_user = 0;
        return 0;
    }

    hitp = pp->pt_inhit;

    /* This is used find the hit point: */
    /*RT_HIT_NORMAL(normal, 
        hitp,
        pp->pt_inseg->seg_stp,
        &(ap->a_ray),
        pp->pt_inflip);
    */
    
    ap->a_user = 1;

    return 1;
}

/*
 *			R A Y M I S S
 *
 *  Called via a_miss linkage from rt_shootray() when ray misses.
 */
int
raymiss(register struct application *ap)
{
    /*bu_log("miss: 0x%x\n", ap->a_resource);*/
    ap->a_user = 0;
    return 0;
}

void
application_init(void) {}

/*
 *			R E P R O J E C T _ S P L A T
 *
 *  Called when the reprojected value lies on the current screen.
 *  Write the reprojected value into the screen,
 *  checking *screen* Z values if the new location is already occupied.
 *
 *  May be run in parallel.
 */
int	rt_scr_lim_dist_sq = 100;	/* dist**2 pixels allowed to move */

int
reproject_splat(int ix, int iy, register struct floatpixel *ip, const fastf_t *new_view_pt)
{
    register struct floatpixel	*op;
    int	count = 1;

    /* Reprojection lies on screen, see if dest pixel already occupied */
    op = &curr_float_frame[iy*width + ix];

    /* Don't reproject again if new val is more distant */
    if ( op->ff_frame >= 0 )  {
	point_t o_pt;
	/* Recompute both distances from current eye_pt! */
	/* Inefficient, only need Z component. */
	MAT4X3PNT(o_pt, model2view, op->ff_hitpt);
	if ( o_pt[Z] > new_view_pt[Z] )
	    return 0;	/* previous val closer to eye, leave it be. */
	else
	    count = 0;	/* Already reproj, don't double-count */
    }

    /* re-use old pixel as new pixel */
    *op = *ip;	/* struct copy */

    return count;
}

/*
 *			R E P R O J E C T _ W O R K E R
 */
void
reproject_worker(int UNUSED(cpu), genptr_t UNUSED(arg))
{
    int	pixel_start;
    int	pixelnum;
    register struct floatpixel	*ip;
    int	count = 0;

    /* The more CPUs at work, the bigger the bites we take */
    if ( per_processor_chunk <= 0 )  per_processor_chunk = npsw;

    while (1)  {

	bu_semaphore_acquire( RT_SEM_WORKER );
	pixel_start = cur_pixel;
	cur_pixel += per_processor_chunk;
	bu_semaphore_release( RT_SEM_WORKER );

	for ( pixelnum = pixel_start; pixelnum < pixel_start+per_processor_chunk; pixelnum++ )  {
	    point_t new_view_pt;
	    int ix, iy;

	    if ( pixelnum > last_pixel )
		goto out;

	    ip = &prev_float_frame[pixelnum];

	    if ( ip->ff_frame < 0 )
		continue;	/* Not valid */
	    if ( ip->ff_dist <= -INFINITY )
		continue;	/* was a miss */
	    /* new model2view has been computed before here */
	    MAT4X3PNT( new_view_pt, model2view, ip->ff_hitpt );

	    /* Convert from -1..+1 range to pixel subscript */
	    ix = (new_view_pt[X] + 1) * 0.5 * width;
	    iy = (new_view_pt[Y] + 1) * 0.5 * height;

	    /*  If not in reproject-only mode,
	     *  apply quality-preserving heuristics.
	     */
	    if ( reproject_mode != 2 )  {
		register int dx, dy;
		int	agelim;

		/* Don't reproject if too pixel moved too far on the screen */
		dx = ix - ip->ff_x;
		dy = iy - ip->ff_y;
		if ( dx*dx + dy*dy > rt_scr_lim_dist_sq )
		    continue;	/* moved too far */

				/* Don't reproject for too many frame-times */
				/* See if old pixel is more then N frames old */
				/* Temporal load-spreading: Don't have 'em all die at the same age! */
		agelim = ((iy+ix)&03)+4;
		if ( curframe - ip->ff_frame >= agelim )
		    continue;	/* too old */
	    }

	    /* 4-way splat.  See if reprojects off of screen */
	    if ( ix >= 0 && ix < (int)width && iy >= 0 && iy < (int)height )
		count += reproject_splat( ix, iy, ip, new_view_pt );

	    ix++;
	    if ( ix >= 0 && ix < (int)width && iy >= 0 && iy < (int)height )
		count += reproject_splat( ix, iy, ip, new_view_pt );

	    iy++;
	    if ( ix >= 0 && ix < (int)width && iy >= 0 && iy < (int)height )
		count += reproject_splat( ix, iy, ip, new_view_pt );

	    ix--;
	    if ( ix >= 0 && ix < (int)width && iy >= 0 && iy < (int)height )
		count += reproject_splat( ix, iy, ip, new_view_pt );
	}
    }

    /* Deposit the statistics */
 out:
    bu_semaphore_acquire( RT_SEM_WORKER );
    reproj_cur += count;
    bu_semaphore_release( RT_SEM_WORKER );
}

/*
 *			V I E W _ E O L
 *
 *  Called by worker() at the end of each line.  Deprecated.
 *  Any end-of-line processing should be done in view_pixel().
 */
void
view_eol(struct application *UNUSED(ap)) {}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
