/*      READ_MAT.C      */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/ 
#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#if USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./nirt.h"
#include "./usrfmt.h"

#define			RMAT_SAW_EYE	0x01
#define			RMAT_SAW_ORI	0x02
#define			RMAT_SAW_VR	0x02

extern outval			ValTab[];

/*	               R E A D _ M A T ( )
 *
 */

void read_mat ()
{
    char	*buf;
    int		status = 0x0;
    mat_t	m;
    point_t	p;
    quat_t	q;

    while ((buf = rt_read_cmd(stdin)) != (char *) 0)
	if (strncmp(buf, "eye_pt", 6) == 0)
	{
	    if (sscanf(buf + 6, "%lf%lf%lf", &p[X], &p[Y], &p[Z]) != 3)
	    {
		(void) fputs("nirt: read_mat(): Failed to read eye_pt\n",
		    stderr);
		exit (1);
	    }
	    target(X) = p[X];
	    target(Y) = p[Y];
	    target(Z) = p[Z];
	    status |= RMAT_SAW_EYE;
	}
	else if (strncmp(buf, "orientation", 11) == 0)
	{
	    if (sscanf(buf + 11,
		"%lf%lf%lf%lf",
		&q[0], &q[1], &q[2], &q[3]) != 4)
	    {
		(void) fputs("nirt: read_mat(): Failed to read orientation\n",
		    stderr);
		exit (1);
	    }
	    quat_quat2mat(m,q);
	    azimuth() = atan2(-m[0],m[1]) / deg2rad;
	    elevation() = atan2(m[10],m[6]) / deg2rad;
	    status |= RMAT_SAW_ORI;
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
		(void) fputs("nirt: read_mat(): Failed to read viewrot\n",
		    stderr);
		exit (1);
	    }
	    azimuth() = atan2(-m[0],m[1]) / deg2rad;
	    elevation() = atan2(m[10],m[6]) / deg2rad;
	    status |= RMAT_SAW_VR;
	}

    if ((status & RMAT_SAW_EYE) == 0)
    {
	(void) fputs("nirt: read_mat(): Was given no eye_pt\n", stderr);
	exit (1);
    }
    if ((status & (RMAT_SAW_ORI | RMAT_SAW_VR)) == 0)
    {
	(void) fputs("nirt: read_mat(): Was given no orientation or viewrot\n",
		stderr);
	exit (1);
    }
    ae2dir();
    targ2grid();
    shoot("", 0);
}
