/*      READ_MAT.C      */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/ 
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./nirt.h"
#include "./usrfmt.h"

extern outval			ValTab[];

/*	               R E A D _ M A T ( )
 *
 */

void read_mat ()
{
    char	*buf;
    int		i;		/* for diagnostics only */
    mat_t	m;
    point_t	p;

    while ((buf = rt_read_cmd(stdin)) != (char *) 0)
	if (strncmp(buf, "eye_pt", 6) == 0)
	{
	    if (sscanf(buf + 6, "%lf%lf%lf", &p[X], &p[Y], &p[Z]) != 3)
	    {
		fputs("nirt: read_mat(): Failed to read eye_pt", stderr);
	    }
	    target(X) = p[X];
	    target(Y) = p[Y];
	    target(Z) = p[Z];
	    targ2grid();
	}
	else if (strncmp(buf, "viewrot", 7) == 0)
	{
	    if (sscanf(buf + 7,
		"%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf",
		&m[0], &m[1], &m[2], &m[3], 
		&m[4], &m[5], &m[6], &m[7], 
		&m[8], &m[9], &m[10], &m[11], 
		&m[12], &m[13], &m[14], &m[15]) != 16)
	    {
		fputs("nirt: read_mat(): Failed to read viewrot", stderr);
	    }
	    azimuth() = atan2(-m[0],m[1]) / deg2rad;
	    elevation() = atan2(m[10],m[6]) / deg2rad;
	    ae2dir();
	}

    shoot("", 0);
}
