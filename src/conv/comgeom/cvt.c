/*                           C V T . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
 *
 */
/** @file comgeom/cvt.c
 *
 *	This is the mainline for converting COM-GEOM
 * cards to a GED style database.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "bu.h"


/* defined in region.c */
extern void group_init(void);
extern int getregion(void);
extern void region_register(int reg_num, int id, int air, int mat, int los);
extern void group_write(void);

/* defined in read.c */
extern int get_line(char *cp, int buflen, char *title);

/* defined in solid.c */
extern void trim_trail_spaces(char *cp);
extern int getsolid(void);

struct wmember	*wmp;	/* array indexed by region number */

int	version = 5;	/* v4 or v5 ? */
int	verbose = 0;	/* verbose = print a message on every item read */

char name_it[16];	/* stores argv if it exists and appends it
			   to each name generated.*/

int	cur_col = 0;

FILE		*infp;
struct rt_wdb	*outfp;		/* Output file descriptor */

int	sol_total, sol_work;	/* total num solids, num solids processed */
int	reg_total;

extern void	getid(void);

void		col_pr(char *str);

static char usage[] = "\
Usage: comgeom-g [options] input_file output_file\n\
Options:\n\
	-v input_vers#		default is 5 (cg5)\n\
	-d debug_lvl\n\
	-s name_suffix\n\
";

int
get_args(int argc, char **argv)
{
    int	c;
    char		*file_name;

    while ( (c = bu_getopt( argc, argv, "d:v:s:" )) != -1 )  {
	switch ( c )  {
	    case 'd':
		verbose = atoi(bu_optarg);
		break;
	    case 's':
		bu_strlcpy( name_it, bu_optarg, sizeof(name_it) );
		break;
	    case 'v':
		version = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if ( bu_optind+2 > argc )
	return 0;		/* FAIL */

    /* Input File */
    if ( bu_optind >= argc )  {
	return 0;		/* FAIL */
    } else {
	file_name = argv[bu_optind++];
	if ( (infp = fopen(file_name, "rb")) == NULL )  {
	    perror(file_name);
	    return 0;
	}
    }

    /* Output File */
    if ( bu_optind >= argc )  {
	return 0;		/* FAIL */
    } else {
	file_name = argv[bu_optind++];
	if ( (outfp = wdb_fopen(file_name)) == NULL )  {
	    perror(file_name);
	    return 0;
	}
    }

    if ( argc > ++bu_optind )
	(void)fprintf( stderr, "comgeom-g: excess argument(s) ignored\n" );

    return 1;		/* OK */
}


/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
    int i;
    char	ctitle[132];
    char	*title;
    char	units[16];

    if ( !get_args( argc, argv ) )  {
	(void)fputs(usage, stderr);
	return 1;
    }

    if ( version != 1 && version != 4 && version != 5 )  {
	fprintf(stderr, "version %d not supported\n", version );
	(void)fputs(usage, stderr);
	return 1;
    }

    printf("Reading version %d COMGEOM file\n", version );

    if ( verbose )  {
	printf("COMGEOM input file must have this format:\n");
	switch (version)  {
	    case 1:
		printf("     1.  title card\n");
		printf("     2.  solid table\n");
		printf("     3.  END\n");
		printf("     4.  region table\n");
		printf("     5.  END\n");
		break;
	    case 4:
		printf("     1.  units & title card\n");
		printf("     2.  solid & region count card\n");
		printf("     3.  solid table\n");
		printf("     4.  region table\n");
		printf("     5.  -1\n");
		printf("     6.  blank\n");
		printf("     7.  region ident table\n\n");
		break;
	    case 5:
		printf("     1.  units & title card\n");
		printf("     2.  solid & region count card\n");
		printf("     3.  solid table\n");
		printf("     4.  region table\n");
		printf("     5.  -1\n");
		printf("     6.  region ident table\n\n");
		break;
	}
    }

    group_init();

    /*
     *  Read title card
     */
    if ( get_line( ctitle, sizeof(ctitle), "title card" ) == EOF ) {
	printf("Empty input file:  no title record\n");
	return 10;
    }

    title = NULL;
    switch ( version )  {
	case 1:
	    title = ctitle;
	    bu_strlcpy( units, "in", sizeof(units) );
	    break;
	case 4:
	case 5:
	    /* First 2 chars are units */
	    units[0] = ctitle[0];
	    units[1] = ctitle[1];
	    units[2] = '\0';
	    title = ctitle+3;
	    break;
    }

    /* Drop leading blanks in title */
    while ( isspace( *title ) )  title++;
    trim_trail_spaces( title );
    trim_trail_spaces( units );

    /* Convert units to lower case */
    {
	char	*cp = units;
	while ( *cp )  {
	    if ( isupper(*cp) )
		*cp = tolower(*cp);
	    cp++;
	}
    }

    printf("Title: %s\n", title);
    printf("Units: %s\n", units);

    /* Before converting any geometry, establish the units conversion
     * factor which all mk_* routines will apply.
     */
    if ( mk_conversion( units ) < 0 )  {
	printf("WARNING:  unknown units '%s', using inches\n", units);
	bu_strlcpy( units, "in", sizeof(units) );
	(void)mk_conversion( units );
    }

    /* Output the MGED database header */
    if ( mk_id_units( outfp, title, units ) < 0 )  {
	printf("Unable to write database ID, units='%s'\n", units);
	return 1;
    }

    /*
     *  Read control card, if present
     */
    sol_total = reg_total = 0;
    switch ( version )  {
	case 1:
	    sol_total = reg_total = 9999;	/* Reads until 'END' rec */
	    break;

	case 4:
	    if ( get_line( ctitle, sizeof(ctitle), "control card" ) == EOF ) {
		printf("No control card .... STOP\n");
		return 10;
	    }
	    sscanf( ctitle, "%20d%10d", &sol_total, &reg_total );
	    break;
	case 5:
	    if ( get_line( ctitle, sizeof(ctitle), "control card" ) == EOF ) {
		printf("No control card .... STOP\n");
		return 10;
	    }
	    sscanf( ctitle, "%5d%5d", &sol_total, &reg_total );
	    break;
    }

    if (verbose) printf("Expecting %d solids, %d regions\n", sol_total, reg_total);


    /*
     *  SOLID TABLE
     */
    if (verbose) printf("Primitive table\n");
    sol_work = 0;
    while ( sol_work < sol_total ) {
	i = getsolid();
	if ( i < 0 )  {
	    printf("error converting primitive %d\n", sol_work);
	    /* Should we abort here? */
	    continue;
	}
	if ( i > 0 ) {
	    printf("\nprocessed %d of %d solids\n\n", sol_work, sol_total);
	    if ( sol_work < sol_total && version > 1 )  {
		printf("some solids are missing, aborting\n");
		return 1;
	    }
	    break;		/* done */
	}
    }

    /* REGION TABLE */

    if (verbose)printf("\nRegion table\n");

    i = sizeof(struct wmember) * (reg_total+2);
    wmp = (struct wmember *)bu_calloc(reg_total+2, sizeof( struct wmember ), "wmp");

    for ( i=reg_total+1; i>=0; i-- )  {
	BU_LIST_INIT( &wmp[i].l );
    }

    cur_col = 0;
    if ( getregion() < 0 ) {
	return 10;
    }

    if ( version == 1 )  {
	for ( i=1; i < reg_total; i++ )  {
	    region_register( i, 0, 0, 0, 0 );
	}
    } else {
	if ( version == 4 )  {
	    char	dummy[88];
	    /* read the blank card (line) */
	    (void)get_line( dummy, sizeof(dummy), "blank card" );
	}

	if (verbose) printf("\nRegion ident table\n");
	getid();
    }

    if (verbose) printf("\nGroups\n");
    cur_col = 0;
    group_write();
    if (verbose) printf("\n");

    return 0;
}

/*
 *			C O L _ P R
 */
void
col_pr(char *str)
{
    printf("%s", str);
    cur_col += strlen(str);
    while ( cur_col < 78 && ((cur_col%10) > 0) )  {
	putchar(' ');
	cur_col++;
    }
    if ( cur_col >= 78 )  {
	printf("\n");
	cur_col = 0;
    }
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
