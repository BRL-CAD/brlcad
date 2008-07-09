/*                       V I E W M L T . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file viewmlt.c
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

#include "ext.h"
#include "rtprivate.h"
#include "scanline.h"


int use_air = 0;
int ibackground[3] = {0};
int inonbackground[3] = {0};
static short int pwidth;			/* Width of each pixel (in bytes) */
static struct scanline* scanline;   /* From scanline.c */
struct mfuncs *mfHead = MF_NULL;	/* Head of list of shaders */

extern FBIO* fbp;

const char title[] = "Metropolis Light Transport renderer";
const char usage[] = "\
Usage:  rtmlt [options] model.g objects... > file.pix\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees\n\
 -e Elev	Elevation in degrees\n\
 -M		Read matrix, cmds on stdin\n\
 -o file.pix	Output file name\n\
 -x #		Set librt debug flags\n\
";

int	rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int	raymiss(register struct application *ap);

/* From scanline.c */
void free_scanlines(int, struct scanline*);
struct scanline* alloc_scanlines(int);

struct bu_structparse view_parse[] = {
    "",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL
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
};

/*
 *  			V I E W _ I N I T
 *
 *  Called by main() at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
view_init(register struct application *ap, char *file, char *obj, int minus_o)
{
    ap->a_ray.r_dir[0] = 1;
    ap->a_ray.r_dir[1] = 0;
    ap->a_ray.r_dir[2] = 1;
    VUNITIZE(ap->a_ray.r_dir);


    return 1;		/* no framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  The beginning of a frame.
 *  Called by do_frame() just before raytracing starts.
 */
void
view_2init(struct application *ap)
{
    int i;
    /* Initialization of the mlt application structure */
    struct mlt_app* mlt_application;
    mlt_application = (struct mlt_app*) bu_malloc(sizeof(struct mlt_app), "mlt application");
    mlt_application->lightSources = (struct point_list*) NULL;
    mlt_application->paths = (struct path_list*) NULL;

    /* Setting application callback functions and
     * linkage to the mlt application
     */
    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 1;
    ap->a_uptr = (genptr_t) mlt_application;

    /* Allocation of the scanline array */
    if (scanline)
        free_scanlines(height, scanline);
    scanline = alloc_scanlines(height);

    /* Setting the pixel size */
    pwidth = 3 + ((rpt_dist) ? (8) : (0));

    /* BUFMODE_SCANLINE
     * This will be expanded to handle multiple BUFMODES,
     * as in view.c */

    if (sub_grid_mode)  {
	    for (i = sub_ymin; i <= sub_ymax; i++)
		    scanline[i].sl_left = sub_xmax-sub_xmin+1;
	}
    else {
		for (i = 0; i < height; i++)
		    scanline[i].sl_left = width;
    }


    /* Setting Lights if the user did not specify */
    if (BU_LIST_IS_EMPTY(&(LightHead.l)) ||
       (BU_LIST_UNINITIALIZED(&(LightHead.l)))) {
		if (R_DEBUG&RDEBUG_SHOWERR) bu_log("No explicit light\n");
		light_maker(1, view2model);
	}

    ap->a_rt_i->rti_nlights = light_init(ap);
}

/*
 *			V I E W _ P I X E L
 *
 *  Called by worker() after the end of proccessing for each pixel.
 *//*
void
view_pixel(register struct application* ap)
{
    int r, g, b, npix, do_eol;
    struct scanline* slp;
    char* pixelp;
    width = pwidth = 512;

    if (ap->a_user) {
        r = g = b = 1;
        VSETALL(ap->a_color, -1e-20); 
    } else {
        r = g = b = 200;
    }

    slp = &(scanline[ap->a_y]);
	if (!(slp->sl_buf))  {
		slp->sl_buf = bu_calloc(width, pwidth, "sl_buf scanline buffer");
	}
	pixelp = slp->sl_buf + (ap->a_x * pwidth);
	*pixelp++ = r;
	*pixelp++ = g;
	*pixelp++ = b;

    if (--(slp->sl_left) <= 0)
        do_eol = 1;

    if (!do_eol) return;

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

    bu_free(scanline[ap->a_y].sl_buf, "sl_buf scanline buffer");
    scanline[ap->a_y].sl_buf = (char *) 0;
    bu_log(".");

}*/


void
view_pixel(register struct application *ap) 
{
    register int	r, g, b, do_eol = 0;
    register char	*pixelp;
    register struct scanline	*slp;
    width = 512;
    pwidth = 3;    

    if (ap->a_user == 0) {
        r = 255;
        g = 255;
        b = 255;
        VSETALL(ap->a_color, -1e-20); 
    }
    else {
        r = 0;
        b = 0;
        g = 0;
    }

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

    if (!do_eol) return;

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
    bu_free(scanline[ap->a_y].sl_buf, "sl_buf scanline buffer");
    scanline[ap->a_y].sl_buf = (char *) 0;
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
    struct path_list *p_path, *p_temp;
    struct point_list *temp;
    /* p_mlt = (struct mlt_app*) ap->a_uptr;
    p_path = p_mlt->paths;
    
    
    /* Freeing path list */
    /*while (BU_LIST_WHILE(p_temp, path_list, &(p_path->l))) {
        while (BU_LIST_WHILE(temp, point_list, &(p_path->pt_list->l))) {
	        BU_LIST_DEQUEUE(&(temp->l));
	        bu_free(temp, "free point_list entry");
        }
        bu_free(p_path->pt_list, "free point_list head");

        BU_LIST_DEQUEUE(&(p_temp->l));
        bu_free(p_temp, "free path_list entry");
    }
    bu_free(p_path, "free path_list head");

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
    register struct mlt_app* p_mlt;
    register struct path_list* p_path;
    /*bu_log("hit: 0x%x\n", ap->a_resource);*/

    /* This will be used by view_pixel() to verify if the ray hit or missed */
    ap->a_user = 1;

    /* The application uses a generic pointer (genptr_t)
     * to point to a struct mlt_app
     */
    
    /*
    p_mlt = (struct mlt_app*) ap->a_uptr;
    p_path = p_mlt->paths;

    /* This will be used find the hit point: *//*
    VJOIN1(segp->seg_in.hit_point, ap->a_ray.r_pt,
        segp->seg_in.hit_dist, ap->a_ray.r_dir);

    /* Once found, it will be stored in p_mlt->path_list. */
    /* This block verifies if the path list already exists.
     * If not, it allocates memory and initializes a new point_list
     * and adds the hit point found to that list.
     *
     * If it exists, this block verifies if the point list also
     * exists and allocates memory accordingly.
     */

    /*

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

        BU_GETSTRUCT(p_path->pt_list, point_list);
        BU_LIST_INIT(&(p_path->pt_list->l));
        VMOVE(p_path->pt_list->pt_cell, segp->seg_in.hit_point);
    }

    /* Use a BRDF function to set the new ap->a_ray->r_dir;
     * r_pt will be the same hitpoint found before;
     * and call rt_shootray(ap)
     */
    /*VMOVE(ap->a_ray.r_pt, segp->seg_in.hit_point);*/

    return 1;	/* report hit to main routine */
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
 *			V I E W _ E O L
 *
 *  Called by worker() at the end of each line.  Deprecated.
 *  Any end-of-line processing should be done in view_pixel().
 */
void
view_eol(register struct application *ap) {}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
