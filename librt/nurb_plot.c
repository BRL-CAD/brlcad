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
struct snurb * srf;
{
	int i,j;
	fastf_t * m_ptr = srf->ctl_points;
	int evp = RT_NURB_EXTRACT_COORDS( srf->pt_type);

	for( i = 0; i < srf->s_size[0]; i++)
	{
		for( j = 0; j < srf->s_size[1]; j++)
		{
			if( j == 0)
			{
				pd_3move( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);
			} else
				pd_3cont( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);
			
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
				pd_3move( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);
			else
				pd_3cont( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);

			m_ptr += stride;
		}
	}
}
