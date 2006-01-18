/*                     N U R B _ P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup nurb */

/*@{*/
/** @file nurb_plot.c
 *	Utilities for spline debuging.
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
 */
/*@}*/

#include "common.h"



#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"
#include "plot3.h"

/*
 *			R T _ N U R B _ P L O T _ S N U R B
 */
void
rt_nurb_plot_snurb(FILE *fp, const struct face_g_snurb *srf)
{
	int i,j;
	const fastf_t * m_ptr = srf->ctl_points;
	int evp = RT_NURB_EXTRACT_COORDS( srf->pt_type);
	int rat = RT_NURB_IS_PT_RATIONAL( srf->pt_type);
	point_t pt;

	NMG_CK_SNURB(srf);

	for( i = 0; i < srf->s_size[0]; i++)
	{
		for( j = 0; j < srf->s_size[1]; j++)
		{
                       if ( rat )
                        {
                                pt[0] = m_ptr[0]/ m_ptr[3];
                                pt[1] = m_ptr[1]/ m_ptr[3];
                                pt[2] = m_ptr[2]/ m_ptr[3];
                        } else
                        {
                                pt[0] = m_ptr[0];
                                pt[1] = m_ptr[1];
                                pt[2] = m_ptr[2];

                        }

			if( j == 0)
			{
				pdv_3move( fp, pt );
			} else
				pdv_3cont( fp, pt );

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
                        if ( rat )
                        {
                                pt[0] = m_ptr[0]/ m_ptr[3];
                                pt[1] = m_ptr[1]/ m_ptr[3];
                                pt[2] = m_ptr[2]/ m_ptr[3];
                        } else
                        {
                                pt[0] = m_ptr[0];
                                pt[1] = m_ptr[1];
                                pt[2] = m_ptr[2];

                        }


			if( i == 0)
				pdv_3move( fp, pt );
			else
				pdv_3cont( fp, pt );

			m_ptr += stride;
		}
	}
}

/*
 *			R T _ N U R B _ P L O T _ C N U R B
 */
void
rt_nurb_plot_cnurb(FILE *fp, const struct edge_g_cnurb *crv)
{
	register int	i, k;
	const fastf_t * m_ptr = crv->ctl_points;
	int evp = RT_NURB_EXTRACT_COORDS( crv->pt_type);
	int rat = RT_NURB_IS_PT_RATIONAL( crv->pt_type);
	point_t ptr;

	for( i = 0; i < crv->c_size; i++)  {
		if( rat )
		{
			for(k = 0; k < evp; k++)
				ptr[k] = m_ptr[k] / m_ptr[evp-1];

		} else
		{
			for(k = 0; k < evp; k++)
				ptr[k] = m_ptr[k];

		}
		if( i == 0 )
			pdv_3move( fp, ptr );
		else
			pdv_3cont( fp, ptr );
		m_ptr += evp;
	}
}

/* Old routines included for backwards compat.  Don't use in new code. */
void rt_nurb_setfile(int n)
{
	pl_color(stdout, n * 25 % 255, n * 50 % 255, n * 75 %255);
}

void
rt_nurb_closefile(void)
{
}

void rt_nurb_s_plot(const struct face_g_snurb *srf)
{
	rt_nurb_plot_snurb( stdout, srf );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
