/*                       R L E _ A R G S . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2012 United States Government as represented by
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
/** @file util/rle_args.c
 */

#include "./rle_args.h"

#include "bu.h"
#include "bio.h"
/*
 * G E T _ A R G S
 */
int
get_args(int argc, char **argv, rle_hdr *outrle, FILE** infp, char** infile, int **background, size_t* file_width, size_t* file_height)
{
    int c;
    (void)file_height;

    while ((c = bu_getopt(argc, argv, "hs:w:n:C:")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		*file_height = *file_width = 1024;
		break;
	    case 's':
		/* square file size */
		*file_height = *file_width = atoi(bu_optarg);
		break;
	    case 'w':
		*file_width = atoi(bu_optarg);
		break;
	    case 'n':
		*file_height = atoi(bu_optarg);
		break;
	    case 'C':
		{
		    char *cp = bu_optarg;
		    int *conp = *background;

		    /* premature null => atoi gives zeros */
		    for (c=0; c < 3; c++) {
			*conp++ = atoi(cp);
			while (*cp && *cp++ != '/')
			    ;
		    }
		}
		break;
	    default:
	    case '?':
		return 0;
	}
    }
    if (argv[bu_optind] != NULL) {
	if ((*infp = fopen((*infile=argv[bu_optind]), "r")) == NULL) {
	    perror(*infile);
	    return 0;
	}
	bu_optind++;
    } else {
	*infile = "-";
    }
    if (argv[bu_optind] != NULL) {
	if (bu_file_exists(argv[bu_optind], NULL)) {
	    (void) fprintf(stderr,
			   "\"%s\" already exists.\n",
			   argv[bu_optind]);
	    bu_exit(1, NULL);
	}
	if ((outfp = fopen(argv[bu_optind], "w")) == NULL) {
	    perror(argv[bu_optind]);
	    return 0;
	}
    }
    if (argc > ++bu_optind)
	(void) fprintf(stderr, "pix-rle: Excess arguments ignored\n");

    if (isatty(fileno(*infp)) || isatty(fileno(outfp)))
	return 0;
    return 1;
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
