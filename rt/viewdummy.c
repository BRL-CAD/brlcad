/*
 *			V I E W D U M M Y . C
 *
 *  A dummy or example "view" module.
 *  This code may be used as a prototype for building a new application
 *  to the RT User-Interface (RTUIF) specification.
 *
 *  For more details, see:
 *  S. Muuss, ``Rapid Creation of Analysis Codes Using BRL-CAD:
 *  The RT View-Module Interface'', in the Proceedings of the
 *  BRL-CAD Symposium 1991.
 *
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static char RCSview[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#include "./rdebug.h"

/*
 *  If this variable is set to zero, then "air" solids in the model
 *  will be entirely discarded.
 *  If this variable is set non-zero, then "air" solids will be
 *  retained, and can be expected to show up in the partition lists.
 */
int	use_air = 0;		/* Handling of air in librt */

/*
 *  Viewing module specific "set" variables.
 *  Any additional variables that the user may be allowed to change
 *  should be listed here.
 *  They can then be accessed via a "set" command, either from
 *  the command line, or from within an animation script.
 */
struct bu_structparse view_parse[] = {
	"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL
};

/*
 *  The usage message for this application.
 */
char usage[] = "\
Usage:  rtdummy [options] model.g objects... >file.rad\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -a Az		Azimuth in degrees\n\
 -e Elev	Elevation in degrees\n\
 -M		Read matrix, cmds on stdin\n\
 -o file.rad	Output file name, else stdout\n\
 -x #		Set librt debug flags\n\
";

int	rayhit();
int	raymiss();

/*
 *  			V I E W _ I N I T
 *
 *  Called by main() at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
view_init( ap, file, obj, minus_o )
register struct application *ap;
char	*file;
char	*obj;
int	minus_o;
{
	return(0);		/* no framebuffer needed */
}

/*
 *			V I E W _ 2 I N I T
 *
 *  The beginning of a frame.
 *  Called by do_frame() just before raytracing starts.
 */
void
view_2init( ap )
struct application *ap;
{
	ap->a_hit = rayhit;
	ap->a_miss = raymiss;
	ap->a_onehit = 1;
}

/*
 *			V I E W _ P I X E L
 *
 *  Called by worker() after the end of proccessing for each pixel.
 */
void
view_pixel( ap )
register struct application *ap;
{
}

/*
 *			V I E W _ E O L
 *
 *  Called by worker() at the end of each line.  Depricated.
 *  Any end-of-line processing should be done in view_pixel().
 */
void
view_eol( ap )
register struct application *ap;
{
}

/*
 *			V I E W _ E N D
 *
 *  Called in do_frame() at the end of a frame,
 *  just after raytracing completes.
 */
void
view_end( ap )
register struct application	*ap;
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
view_setup( rtip )
struct rt_i	*rtip;
{
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called by "clean" command, just before rt_clean() is called, in do.c
 */
void
view_cleanup( rtip )
struct rt_i	*rtip;
{
}

/*
 *			R A Y H I T
 *
 *  Called via a_hit linkage from rt_shootray() when ray hits.
 */
int
rayhit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	bu_log("hit: 0x%x\n", ap->a_resource);

	return(1);	/* report hit to main routine */
}

/*
 *			R A Y M I S S
 *
 *  Called via a_miss linkage from rt_shootray() when ray misses.
 */
int
raymiss( ap )
register struct application *ap;
{
	bu_log("miss: 0x%x\n", ap->a_resource);

	return(0);
}

void application_init () {}
