/*                            D S P . C
 * BRL-CAD
 *
 * Copyright (C) 1994-2005 United States Government as represented by
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
/** @file dsp.c
 *
 */

#include "common.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"

int
mk_dsp(struct rt_wdb *fp, const char *name, const char *file, int xdim, int ydim, const matp_t mat)
          	      		/* name of file containing elevation data */
   		     		/* X dimension of file (w cells) */
   		     		/* Y dimension of file (n cells) */
            	    		/* convert solid coords to model space */
{
	struct rt_dsp_internal *dsp;

	BU_GETSTRUCT( dsp, rt_dsp_internal );
	dsp->magic = RT_DSP_INTERNAL_MAGIC;
	bu_vls_init( &dsp->dsp_name );
	bu_vls_strcpy( &dsp->dsp_name, "file:");
	bu_vls_strcat( &dsp->dsp_name, file);

	dsp->dsp_xcnt = xdim;
	dsp->dsp_ycnt = ydim;
	MAT_COPY( dsp->dsp_stom, mat );

	return wdb_export( fp, name, (genptr_t)dsp, ID_DSP, mk_conv2mm );
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
