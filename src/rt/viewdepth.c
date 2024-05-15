/*                     V I E W D E P T H . C
 * BRL-CAD
 *
 * Copyright (c) 2022-2024 United States Government as represented by
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
/** @file rt/viewdepth.c
 *
 *  Ray Tracing program RTdepth bottom half.
 *
 *  This module produces pseudo depth images of models by adding
 *  up ray path lengths.  By default it will scale these lengths
 *  by the max model RPP length and write a grey scale BW file.
 *  The alternate lighting model outputs "doubles" containing the
 *  actual lengths for post processing.
 *
 *  A better version of this would know something about material
 *  densities.
 *
 */

#include "common.h"

#include <stdio.h>

#include "bu/parallel.h"
#include "bu/mime.h"
#include "bu/path.h"
#include "vmath.h"
#include "icv.h"
#include "raytrace.h"
#include "dm.h"

#include "./rtuif.h"
#include "./ext.h"


extern	struct fb *fbp;
extern	FILE	*outfp;
extern	fastf_t	viewsize;

unsigned char *scanbuf;
static int pixsize = 0;		/* bytes per pixel in scanbuf */

static int depthhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
static int depthmiss(register struct application *ap);

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

const char title[] = "RT Depth";

/*
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(struct application *UNUSED(ap), char *UNUSED(file), char *UNUSED(obj), int minus_o, int minus_F)
{
	/*
	 * We need to work to get the output pixels and scanlines
	 * in order before we can run in parallel.  Something like
	 * view.c does in its dynamic buffering mode.
	 *
	 * XXX this hack-around causes a need for more careful
	 * semaphore acquisition since it may block
	 */
	if (RTG.rtg_parallel)
	{
		RTG.rtg_parallel = 0;
		bu_log("rtdepth: Can't do parallel yet, using one CPU\n");
	}

	pixsize = 3; /* use frame buffer size with icv */
	scanbuf = (unsigned char *)bu_malloc(width * pixsize, "scanline buffer");

	if (minus_F || (!minus_o && !minus_F))
	{
		/* open a framebuffer? */
		return 1;
	}

	/* no framebuffer */
	return 0;
}

/* beginning of a frame */
void
view_2init(struct application *ap, char *UNUSED(framename))
{
	ap->a_hit = depthhit;
	ap->a_miss = depthmiss;
	ap->a_onehit = 0;
}

/* end of each pixel */
void
view_pixel(struct application *UNUSED(ap))
{
}

/* end of each line */
void
view_eol(struct application *ap)
{
	size_t i;

	if (bif != NULL)
	{
		/* TODO : Add double type data to maintain resolution */
		icv_writeline(bif, ap->a_y, scanbuf, ICV_DATA_UCHAR);
	}
	else if (outfp != NULL)
	{
		if (RTG.rtg_parallel)
		{
			bu_semaphore_acquire(BU_SEM_SYSCALL);
		}
		i = fwrite(scanbuf, pixsize, width, outfp);
		if (i < width)
		{
			perror("fwrite");
		}
		if (RTG.rtg_parallel)
		{
			bu_semaphore_release(BU_SEM_SYSCALL);
		}
	}
	if (fbp != FB_NULL)
	{
		if (RTG.rtg_parallel)
		{
			bu_semaphore_acquire(BU_SEM_SYSCALL);
		}
		fb_write(fbp, 0, ap->a_y, scanbuf, width);
		if (RTG.rtg_parallel)
		{
			bu_semaphore_release(BU_SEM_SYSCALL);
		}
	}

	if (bif == NULL && fbp == FB_NULL && outfp == NULL)
		bu_log("rtdepth: strange, no end of line actions taken.\n");
}

void	view_setup(struct rt_i *UNUSED(rtip)) {}
/* end of a frame, called after rt_clean() */
void	view_cleanup(struct rt_i *UNUSED(rtip)) {}

/* end of each frame */
void
view_end(struct application *UNUSED(ap))
{

}

static int
depthhit(register struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
	register struct partition *pp;
	register struct hit *hitp;
	fastf_t fvalue;
	unsigned char value;
	double dist;

	for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw)
		if (pp->pt_outhit->hit_dist >= 0.0)
			break;
	if (pp == PartHeadp)
	{
		bu_log("depthhit:  no hit out front?\n");
		return 0;
	}

	if (OPTICAL_DEBUG & OPTICAL_DEBUG_HITS)
	{
		rt_pr_partition(ap->a_rt_i, pp);
	}

	hitp = pp->pt_inhit;
	if (hitp->hit_dist >= INFINITY)
	{
		bu_log("depthhit:  entry beyond infinity\n");
		return 1;
	}
	/* Check to see if eye is "inside" the solid */
	if (hitp->hit_dist < 0.0)
	{
		/* XXX */
		bu_log("depthhit:  Eye inside solid (%g)\n", hitp->hit_dist);
		for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw)
			rt_pr_partition(ap->a_rt_i, pp);
		return 0;
	}

	dist = pp->pt_inhit->hit_dist;

	fvalue = 1.0 - dist / viewsize;
	if (fvalue > 1.0)
		fvalue = 1.0;
	else if (fvalue <= 0.0)
		fvalue = 0.0;
	value = 1.0 + 254.99 * fvalue;
	bu_semaphore_acquire(RT_SEM_RESULTS);
	scanbuf[ap->a_x * 3 + RED] = value;
	scanbuf[ap->a_x * 3 + GRN] = value;
	scanbuf[ap->a_x * 3 + BLU] = value;
	bu_semaphore_release(RT_SEM_RESULTS);

	return 1; /* report hit to main routine */
}

static int
depthmiss(register struct application *ap)
{
	bu_semaphore_acquire(RT_SEM_RESULTS);
	if (pixsize == 1)
	{
		scanbuf[ap->a_x] = 0;
	}
	else
	{
		scanbuf[ap->a_x * 3 + RED] = 0;
		scanbuf[ap->a_x * 3 + GRN] = 0;
		scanbuf[ap->a_x * 3 + BLU] = 0;
	}
	bu_semaphore_release(RT_SEM_RESULTS);

	return 0; /* report miss to main routine */
}

void
application_init (void)
{
    option("", "-o file.bw", "Output black & white image filename", 0);
    option("Raytrace", "-i", "Enable incremental (progressive-style) rendering", 1);
    option("Raytrace", "-t", "Render from top to bottom (default: from bottom up)", 1);

    option(NULL, "-C", "Disabled, not implemented", -2);
    option(NULL, "-W", "Disabled, non implemented", -2);
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
