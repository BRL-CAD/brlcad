/*              	S O L V E R _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2008 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "pc_constraint.h"
#include "pc.h"

int
main(int argc, char **argv)
{
	int i,ret;
	struct cn_node cnn;
	struct cn_edge cne;
	
	cnn.solved_status=0;
	cnn.n_relations=1;
	cnn.edge[0] = (struct cn_edge *) &cne;
	cne.solved_status=0;
	cne.n_params=1;

	fprintf(stdout,"%d\n",cnn.edge[0]->n_params);

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
