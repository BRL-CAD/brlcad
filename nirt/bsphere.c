/*	BSPHERE.C	*/
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include "conf.h"

#include	<stdio.h>
#include	<math.h>

#include	"machine.h"
#include	"vmath.h"
#include	"raytrace.h"
#include	"./nirt.h"
#include	"./usrfmt.h"

fastf_t	bsphere_diameter;

void set_diameter(struct rt_i *rtip)
{
    vect_t	diag;

    VSUB2(diag, rtip -> mdl_max, rtip -> mdl_min);
    bsphere_diameter = MAGNITUDE(diag);
}
