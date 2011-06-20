/*                      V I E W F R A C . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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
/** @file rt/viewfrac.c
 *
 *  An experimental volume fractions code.
 *
 */

#include "common.h"

#include <stdio.h>

#include "vmath.h"
#include "raytrace.h"

#include "./rtuif.h"
#include "./ext.h"


double	cell_depth;
int	depth;


/*
 *  Viewing module specific "set" variables.
 *  Any additional variables that the user may be allowed to change
 *  should be listed here.
 *  They can then be accessed via a "set" command, either from
 *  the command line, or from within an animation script.
 */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


const char title[] = "RT Volume Fractions";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects... >file.frac\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees\n");
    bu_log(" -e Elev	Elevation in degrees\n");
    bu_log(" -M		Read matrix, cmds on stdin\n");
    bu_log(" -o file	Output file name, else stdout\n");
    bu_log(" -x #		Set librt debug flags\n");
}


int	rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int	raymiss(register struct application *ap);


/*
 *  			V I E W _ I N I T
 *
 *  Called by main() at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(struct application *UNUSED(ap), char *UNUSED(file), char *UNUSED(obj), int minus_o, int UNUSED(minus_F))
{
    /* Handling of air in librt */
    use_air = 1;

    if ( !minus_o )
	outfp = stdout;

    output_is_binary = 0;		/* output is printable ascii */

    if ( npsw > 1 )  {
	bu_log("rtfrac:  dropping down to 1 CPU\n");
	npsw = 1;
    }

    return 0;		/* no framebuffer needed */
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
    extern double	cell_width;
    extern size_t width;

    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 1;

    cell_depth = cell_width;
    depth = width;
    bu_log("cell_depth=%g, # depth cells=%d\n", cell_depth, depth);
}

/*
 *			V I E W _ P I X E L
 *
 *  Called by worker() after the end of proccessing for each pixel.
 */
void
view_pixel(register struct application *UNUSED(ap))
{
}

/*
 *			V I E W _ E O L
 *
 *  Called by worker() at the end of each line.  Depricated.
 *  Any end-of-line processing should be done in view_pixel().
 */
void
view_eol(struct application *UNUSED(ap))
{
}

/*
 *			V I E W _ E N D
 *
 *  Called in do_frame() at the end of a frame,
 *  just after raytracing completes.
 */
void
view_end(struct application *UNUSED(ap))
{
}

/*
 *			V I E W _ S E T U P
 *
 *  Called by do_prep(), just before rt_prep() is called, in do.c
 *  This allows the lighting model to get set up for this frame,
 *  e.g., generate lights, associate materials routines, etc.
 */
void
view_setup(struct rt_i *UNUSED(rtip))
{
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called by "clean" command, just before rt_clean() is called, in do.c
 */
void
view_cleanup(struct rt_i *UNUSED(rtip))
{
}

/*
 *			R A Y H I T
 *
 *  Called via a_hit linkage from rt_shootray() when ray hits.
 */
int
rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp;
    int		d;		/* current depth cell # */
    fastf_t		cur_depth;
    fastf_t		frac;

    d = 0;
    /* examine each partition until we get back to the head */
    for ( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
	if ( pp->pt_outhit->hit_dist <= 0 )  continue;
	cur_depth = pp->pt_inhit->hit_dist;
	while ( cur_depth > (d+1) * cell_depth )  d++;

	/* Partition starts in this cell */
    more:
	if ( pp->pt_outhit->hit_dist <= (d+1) * cell_depth )  {
	    /* Partition ends in this cell */
	    frac = (pp->pt_outhit->hit_dist -
		    cur_depth) / cell_depth;
	    fprintf(outfp, "%d %d %d %d %8f\n",
		    ap->a_x, ap->a_y, d,
		    pp->pt_regionp->reg_regionid, frac );
	    continue;
	}
	/* Partition starts in this cell, ends in some later cell */
	frac = ( (d+1) * cell_depth - cur_depth ) / cell_depth;
	fprintf(outfp, "%d %d %d %d %8f\n",
		ap->a_x, ap->a_y, d,
		pp->pt_regionp->reg_regionid, frac );
	d++;
	goto more;
    }

    return 1;	/* report hit to main routine */
}

/*
 *			R A Y M I S S
 *
 *  Called via a_miss linkage from rt_shootray() when ray misses.
 */
int
raymiss(register struct application *UNUSED(ap))
{
    return 0;
}

void application_init (void) {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
