/*                         P D B - G . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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

#include "common.h"

#include "bu.h"


void
read_pdb(char *fileName)
{
    if (!bu_file_exists(fileName, 0)) {
	fprintf(stderr, "pdb file: %s, does not exist\n", fileName);
	return;
    }

    bu_log("pdb file: %s\n", fileName);

    return;
}


void
mk_protein()
{
    return;
}


void
write_g(char *fileName)
{
    if (!bu_file_exists(fileName, 0)) {
	fprintf(stderr, "g file: %s, does not exist\n", fileName);
	return;
    }

    bu_log("g file: %s\n", fileName);

    return;
}


/* format of command: pdb-g pdbfile gfile */
int
main(int argc, char *argv[])
{
    if (argc < 3) {
	fprintf(stderr, "No pdb filename or g filename given.\n");
	return 1;
    }

    read_pdb(argv[1]);

    mk_protein();

    write_g(argv[2]);

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
