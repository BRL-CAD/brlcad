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

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include "machine.h"
#include "vmath.h"
#include "./nurb.h"

static FILE * plotfile;

void rt_nurb_setfile(n)
int n;
{
	pl_color(stdout, n * 25 % 255, n * 50 % 255, n * 75 %255);
}

rt_nurb_closefile()
{

}

void rt_nurb_s_plot( srf)
struct snurb * srf;
{

	rt_nurb_m_plot( srf->mesh);
}

rt_nurb_m_plot( m )
struct s_mesh * m;
{
	int i,j,k;
	fastf_t * m_ptr = m->ctl_points;
	int evp = EXTRACT_COORDS( m->pt_type);

	for( i = 0; i < m->s_size[0]; i++)
	{
		for( j = 0; j < m->s_size[1]; j++)
		{
			if( j == 0)
			{
				pd_3move( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);
			} else
				pd_3cont( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);
			
			m_ptr += evp;
		}
	}

	for( j = 0; j < m->s_size[1]; j++)
	{
		int stride;
		stride = m->s_size[1] * evp;
		m_ptr = &m->ctl_points[j * evp];
		for( i = 0; i < m->s_size[0]; i++)
		{
			if( i == 0)
				pd_3move( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);
			else
				pd_3cont( stdout, m_ptr[0], m_ptr[1], m_ptr[2]);

			m_ptr += stride;
		}
	}
}
