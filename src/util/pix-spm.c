/*                       P I X - S P M . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file pix-spm.c
 *
 *  Turn a pix file into sphere map data.
 *
 *  Phil Dykstra
 *  12 Aug 1986
 */
#include "common.h"



#include <stdio.h>
#include <stdlib.h>

#include "machine.h"
#include "fb.h"
#include "spm.h"

static	char *Usage = "usage: pix-spm file.pix size > file.spm\n";

int
main(int argc, char **argv)
{
	int	size;
	spm_map_t *mp;

	if( argc != 3 ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	size = atoi( argv[2] );
	mp = spm_init( size, sizeof(RGBpixel) );
	spm_px_load( mp, argv[1], size, size );
	spm_save( mp, "-" );

	return 0;
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
