/*	DIST_DEF.C	*/
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include	<stdio.h>
#include	<math.h>
#include	<machine.h>
#include	<vmath.h>
#include	<raytrace.h>
#include	"./nirt.h"
#include	"./usrfmt.h"

extern struct rt_i	*rtip;
extern outval		ValTab[];

double dist_default()
{
    vect_t		targ[8];       /* corners of target bounds       */
    vect_t		Gr;              /* corners of grid bounds         */
    int			i;
    static int		first_time = 1;
    static double	g_max;
    double		ca, ce, sa, se;

    if (first_time)
    {
	ca = cos(azimuth() * deg2rad);
	ce = cos(elevation() * deg2rad);
	sa = sin(azimuth() * deg2rad);
	se = sin(elevation() * deg2rad);

	/* determine the outer bounds of the RPP in gridplane coordinates */
	targ[0][X] = targ[3][X] = targ[6][X] = targ[7][X] =
		    rtip -> mdl_min[X];
	targ[1][X] = targ[2][X] = targ[4][X] = targ[5][X] =
		    rtip -> mdl_max[X];
	targ[0][Y] = targ[3][Y] = targ[6][Y] = targ[7][Y] =
		    rtip -> mdl_min[Y];
	targ[1][Y] = targ[2][Y] = targ[4][Y] = targ[5][Y] =
		    rtip -> mdl_max[Y];
	targ[0][Z] = targ[3][Z] = targ[6][Z] = targ[7][Z] =
		    rtip -> mdl_min[Z];
	targ[1][Z] = targ[2][Z] = targ[4][Z] = targ[5][Z] =
		    rtip -> mdl_max[Z];

	g_max = targ[0][X] * ce * ca + targ[0][Y] * ce * sa + targ[0][Z] * se;

	for (i=1; i<8; i++)    /* find the horz and vert max and min */
	    {
	    Gr[DIST] = targ[i][X] * ce * ca
		       + targ[i][Y] * ce * sa
		       + targ[i][Z] * se;
	    g_max = max(g_max, Gr[DIST]); 
	    } 
	first_time = 0;
    }
    return(g_max);
}
