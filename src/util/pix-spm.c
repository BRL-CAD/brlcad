/*                       P I X - S P M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file util/pix-spm.c
 *
 * Turn a pix file into sphere map data.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "spm.h"
#include "fb.h"
#include "bu.h"


int
main(int argc, char **argv)
{
    int size;
    bn_spm_map_t *mp;

    if (argc != 3) {
	bu_exit(1, "usage: pix-spm file.pix size > file.spm\n");
    }

    size = atoi(argv[2]);
    mp = bn_spm_init(size, sizeof(RGBpixel));
    bn_spm_pix_load(mp, argv[1], size, size);
    bn_spm_save(mp, "-");

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
