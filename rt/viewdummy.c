/* dummy view module */

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./rdebug.h"

extern struct resource resource[];

int	use_air = 0;		/* Handling of air in librt */
int	using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
struct structparse view_parse[] = {
	(char *)0, 0, (char *)0,	0,	FUNC_NULL
};

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

int	dummyhit();
int	dummymiss();

/*
 *  			V I E W _ I N I T
 *
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
{
	ap->a_hit = dummyhit;
	ap->a_miss = dummymiss;
	ap->a_onehit = 1;

	return(0);		/* no framebuffer needed */
}

/* beginning of a frame */
void
view_2init( ap )
struct application *ap;
{
}

/* end of each pixel */
void	view_pixel(ap)
register struct application *ap;
{
}

/* end of each line */
void	view_eol(ap)
register struct application *ap;
{
}

/* end of a frame */
void	view_end() {}

/* Associated with "clean" command, before new tree is loaded  */
void	view_cleanup() {}

int
dummyhit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	rt_log("hit: 0x%x\n", ap->a_resource);

	return(1);	/* report hit to main routine */
}

int
dummymiss( ap )
register struct application *ap;
{
	rt_log("miss: 0x%x\n", ap->a_resource);

	return(0);
}
