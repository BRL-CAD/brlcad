/*
 *			V I E W A R E A
 */
#ifndef lint
static const char RCSrayg3[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ext.h"
#include "../librt/debug.h"
#include "plot3.h"
#include "rtprivate.h"

#define	MM2IN	0.03937008		/* mm times MM2IN gives inches */
#define TOL 0.01/MM2IN			/* GIFT has a 0.01 inch tolerance */

extern int	npsw;			/* number of worker PSWs to run */

int		use_air = 1;		/* Handling of air in librt */

extern int 	 rpt_overlap;

extern fastf_t  rt_cline_radius;        /* from g_cline.c */

static long hit_count=0;
static fastf_t cell_area=0.0;


/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL }
};

const char usage[] = "\
Usage:  rtarea [options] model.g objects...\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees	(conflicts with -M)\n\
 -e Elev	Elevation in degrees	(conflicts with -M)\n\
 -M		Read model2view matrix on stdin (conflicts with -a, -e)\n\
 -g #		Grid cell width in millimeters (conflicts with -s)\n\
 -G #		Grid cell height in millimeters (conflicts with -s)\n\
 -J #		Jitter.  Default is off.  Any non-zero number is on\n\
 -U #		Set use_air boolean to # (default=1)\n\
 -c \"set rt_cline_radius=radius\"      Additional radius to be added to CLINE solids\n\
 -x #		Set librt debug flags\n\
";

int	rayhit(), raymiss();

/*
 *  			V I E W _ I N I T
 *
 */

int
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{

	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 1;

	if( !rpt_overlap )
		 ap->a_logoverlap = rt_silent_logoverlap;

	output_is_binary = 0;		/* output is printable ascii */

	return(0);		/* No framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  View_2init is called by do_frame(), which in turn is called by
 *  main() in rt.c.  It writes the view-specific COVART header.
 * 
 */
void
view_2init( ap )
struct application	*ap;
{
	hit_count = 0;

	cell_area = cell_width * cell_height;

	return;
}

/*
 *			R A Y M I S S
 *
 *  Null function -- handle a miss
 *  This function is called by rt_shootray(), which is called by
 *  do_frame().
 */
int
raymiss()
{
	return(0);
}

/*
 *			V I E W _ P I X E L
 *
 *  This routine is called from do_run(), and in this case does nothing.
 */
void
view_pixel()
{
	return;
}

/*
 *			R A Y H I T
 *
 */
int
rayhit( ap, PartHeadp )
struct application *ap;
register struct partition *PartHeadp;
{
	register struct partition *pp = PartHeadp->pt_forw;

	if( pp == PartHeadp )
		return(0);		/* nothing was actually hit?? */

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	hit_count++;
	bu_semaphore_release( BU_SEM_SYSCALL );


	return(0);
}

/*
 *			V I E W _ E O L
 *
 *  View_eol() is called by rt_shootray() in do_run().  In this case,
 *  it does nothing.
 */
void	view_eol()
{
}

/*
 *			V I E W _ E N D
 *
 */
void
view_end()
{
	fastf_t total_area=0.0;


	total_area = cell_area * (fastf_t)hit_count;

	bu_log( "Area = %g square mm (%g square meters)\n", total_area, total_area / 1000000.0 );
}

void view_setup() {}
void view_cleanup() {}
void application_init () {}
