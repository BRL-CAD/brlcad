/* 	N U R B _ P L O T . C
 *
 *  Function -
 *	Utilities for spline debuging
 *
 *  Author -
 *	Paul Randal Stay
 *
 *
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

void rt_nurb_setfile(n)
int n;
{
	pl_color(stdout, n * 25 % 255, n * 50 % 255, n * 75 %255);
}

rt_nurb_closefile()
{

}

void rt_nurb_s_plot( srf )
CONST struct snurb * srf;
{
	rt_nurb_plot_snurb( stdout, srf );
}

void
rt_nurb_plot_snurb( fp, srf )
FILE	*fp;
CONST struct snurb	*srf;
{
	int i,j;
	CONST fastf_t * m_ptr = srf->ctl_points;
	int evp = RT_NURB_EXTRACT_COORDS( srf->pt_type);

	NMG_CK_SNURB(srf);

	for( i = 0; i < srf->s_size[0]; i++)
	{
		for( j = 0; j < srf->s_size[1]; j++)
		{
			if( j == 0)
			{
				pdv_3move( fp, m_ptr );
			} else
				pdv_3cont( fp, m_ptr );

			m_ptr += evp;
		}
	}

	for( j = 0; j < srf->s_size[1]; j++)
	{
		int stride;
		stride = srf->s_size[1] * evp;
		m_ptr = &srf->ctl_points[j * evp];
		for( i = 0; i < srf->s_size[0]; i++)
		{
			if( i == 0)
				pdv_3move( fp, m_ptr );
			else
				pdv_3cont( fp, m_ptr );

			m_ptr += stride;
		}
	}
}

void
rt_nurb_plot_cnurb( fp, crv )
FILE	*fp;
CONST struct cnurb	*crv;
{
	register int	i;
	CONST fastf_t * m_ptr = crv->ctl_points;
	int evp = RT_NURB_EXTRACT_COORDS( crv->pt_type);

	for( i = 0; i < crv->c_size; i++)  {
		if( i == 0 )
			pdv_3move( fp, m_ptr );
		else
			pdv_3cont( fp, m_ptr );
		m_ptr += evp;
	}
}
