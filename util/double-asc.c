/*
 *  			D O U B L E - A S C . C
 *  
 *  		Take a stream of IEEE doubles and make them ASCII
 *  
 *  Author -
 *	Paul Tanenbaum
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1996 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <fcntl.h>
#include "machine.h"
#include "externs.h"			/* For getopt */
#include "fb.h"

static char	*file_name;
static int	infd;

static int	fileinput = 0;		/* Input from file (vs. pipe)? */
static int	autosize = 0;		/* Try to autosize input? */

static int	file_width = 512;	/* default input width */
static int	file_height = 512;	/* default input height */

static int	make_cells = 0;		/* Insert cell coords in output? */
static int	d_per_l = 1;		/* doubles per line of output */

void print_usage (void)
{
#define OPT_STRING	"achs:n:w#:?"

    bu_log("Usage: 'double-asc %s [file.d]'\n",
	"[-{ah}c] [-s squaresize] [-w width] [-n height] [-# depth]");
}

get_args(argc, argv)

int		argc;
register char	*argv[];

{
    register int ch;

    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
    {
	switch (ch)
	{
	    case 'a':
		autosize = 1;
		break;
	    case 'c':
		make_cells = 1;
		break;
	    case 'h':
		/* high-res */
		file_height = file_width = 1024;
		autosize = 0;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(optarg);
		autosize = 0;
		break;
	    case 'n':
		file_height = atoi(optarg);
		autosize = 0;
		break;
	    case 'w':
		file_width = atoi(optarg);
		autosize = 0;
		break;
	    case '#':
		d_per_l = atoi(optarg);
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
		return(0);
	}
    }

    /*
     *	Establish the input stream
     */
    switch (argc - optind)
    {
	case 0:
	    file_name = "stdin";
	    infd = 0;
	    break;
	case 1:
	    file_name = argv[optind++];
	    if ((infd = open(file_name, O_RDONLY)) == -1)
	    {
		bu_log ("Cannot open file '%s'\n", file_name);
		exit (1);
	    }
	    fileinput = 1;
	    break;
	default:
	    print_usage();
	    exit (1);
    }

    if (argc > ++optind)
    {
	print_usage();
	exit (1);
    }

    return(1);		/* OK */
}

main (argc, argv)

int	argc;
char	*argv[];

{
    char	*buffer, *bp;
    double	*value;
    int		bufsiz;		/* buffer size (in bytes) */
    int		l_per_b;	/* buffer size (in output lines) */
    int		line_nm;	/* number of current line */
    int		num;		/* number of bytes read */
    int		i;
    int		row, col;	/* coords within input stream */

    if (!get_args( argc, argv))
    {
	print_usage();
	exit (1);
    }

    /* autosize input? */
    if (fileinput && autosize)
    {
	int	w, h;

	if (fb_common_file_size(&w, &h, file_name, d_per_l * 8))
	{
	    file_width = w;
	    file_height = h;
	}
	else
	    bu_log("double-asc: unable to autosize\n");
    }
    bu_log("OK, file is %d wide and %d high\n", file_width, file_height);

    /*
     *	Choose an input-buffer size as close as possible to 64 kbytes,
     *	while still an integral multiple of the size of an output line.
     */
    l_per_b = ((1 << 16) / (d_per_l * 8));
    bufsiz = l_per_b * (d_per_l * 8);

    buffer = (char *) bu_malloc(bufsiz, "char buffer");
    value = (double *) bu_malloc(d_per_l * 8, "doubles");
    col = row = 0;
    while ((num = read(infd, buffer, bufsiz)) > 0)
    {
	bp = buffer;
	l_per_b = num / (d_per_l * 8);
	for (line_nm = 0; line_nm < l_per_b; ++line_nm)
	{
	    if (make_cells)
		printf("%d %d ", col, row);
	    ntohd(value, bp, d_per_l);
	    bp += d_per_l * 8;
	    for (i = 0; i < d_per_l; ++i)
		printf((i == 0) ? "%g" : " %g", value[i]);
	    printf("\n");
	    if (++col % file_width == 0)
	    {
		col = 0;
		++row;
	    }
	}
    }
    if (num < 0)
    {
	bu_log("double-asc: %s\n", strerror(errno));
	exit (1);
    }
}
