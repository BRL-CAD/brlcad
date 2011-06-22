/*                     C O N V - V G 2 G . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file conv/conv-vg2g.c
 *
 *  Converts .vg files to .g (latest style, with idents).
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"
#include <fcntl.h>

#include "db.h"
#include "bu.h"


void	mat_pr(char *title, float *mp);

union record rec;

char line[256];

int
main(int argc, char **argv)
{
    static int ifd, ofd;
    static int units;
    static int count;
    static int i;
    double factor = 1.0;
    int ret;

    if ( argc != 3 )  {
	printf("Usage: conv-vg2g file.vg file.g\n");
	return 11;
    }
    if ((ifd = open( argv[1], O_RDONLY | O_BINARY)) < 0) {
	perror(argv[1]);
	return 12;
    }
    if ( (ofd = creat(argv[2], 0664)) < 0 )  {
	perror(argv[2]);
	return 13;
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(ofd, O_BINARY);
#endif

    /* check for conversion from version 3 to version 4 */
    i = read(ifd, &rec, sizeof rec);
    if (i < 0) {
	perror("READ ERROR");
	return 14;
    }

    if (rec.u_id == ID_IDENT) {
	/* have an mged type file - check its version */
	if ( BU_STR_EQUAL(rec.i.i_version, ID_VERSION) ) {
	    (void)printf("%s: NO conversion necessary\n", argv[1]);
	    (void)putchar(7);
	    return 0;
	}

	else {
	    /* convert from version 3 to version 4 */
	    (void)printf("convert from ver 3 to ver 4\n");
	    units = ID_IN_UNIT;
	    rec.i.i_version[0] = '\0';
	    bu_strlcpy(rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version));
	}
    }
    else {
	lseek(ifd, (off_t)0L, 0);
	/* have an old vged file to convert */

	/* units are inportant now because:
	 *	The ged data records will be stored in a constant BASE unit
	 *	of MiliMeters (MM).
	 *	At any time the ged user can change his local units.
	 *    	Hence cv must know the original units of the ".vg" file so
	 *	that they can be converted to BASE units.
	 */
	(void)printf("* *  V E R Y    I M P O R T A N T    N O T I C E  * *\n");
	(void)printf("    You must KNOW the units of the %s file\n", argv[1]);
	(void)printf("    If you don't know, DON'T guess....find out\n");
	(void)putchar( 7 );

	rec.i.i_id = ID_IDENT;
	bu_strlcpy( rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version) );
	rec.i.i_units = 100;
	while ( rec.i.i_units < ID_MM_UNIT || rec.i.i_units > ID_FT_UNIT )  {
	    printf("Units: 1=mm, 2=cm, 3=meters, 4=inches, 5=feet\nUnits? ");
	    bu_fgets( line, sizeof(line), stdin );
	    sscanf( line, "%d", &units );
	    rec.i.i_units = units;
	    printf("units=%d\n", rec.i.i_units);
	}
	printf("Title? "); fflush(stdout);
	bu_fgets( line, sizeof(line), stdin );
	line[strlen(line)-1] = '\0';		/* discard \n */
	bu_strlcpy( rec.i.i_title, line, sizeof(rec.i.i_title) );
	printf("Title=%s\n", rec.i.i_title );
    }

    ret = write( ofd, &rec, sizeof rec );
    if (ret < 0)
	perror("write");

    count = 1;

    /* find the units scale factor */
    switch ( units ) {

	case ID_MM_UNIT:
	    factor = 1.0;			/* from MM to MM */
	    break;

	case ID_CM_UNIT:
	    factor = 10.0;			/* from CM to MM */
	    break;

	case ID_M_UNIT:
	    factor = 1000.0;		/* from M to MM */
	    break;

	case ID_IN_UNIT:
	    factor = 25.4;			/* from IN to MM */
	    break;

	case ID_FT_UNIT:
	    factor = 304.8;			/* from FT to MM */
	    break;
	default:
	    printf("eh?\n");
    }

 top:
    while ( read( ifd, &rec, sizeof rec ) == sizeof rec )  {
    after_read:
	switch ( rec.u_id )  {

	    case ID_IDENT:
		printf("Unexpected ID record encountered in input\n");
		printf("Title=%s\n", rec.i.i_title );
		break;
	    case ID_FREE:
		goto top;
	    case ID_SOLID:
		if ( rec.s.s_name[0] == '\0' )
		    goto top;
		for (i=0; i<24; i++)
		    rec.s.s_values[i] *= factor;
		break;
	    case ID_COMB:
		if ( rec.c.c_name[0] == '\0' )  {
		    /* This is an old-style flag for a deleted combination */
		    /* Skip any folowing member records */
		    do  {
			if (read( ifd, &rec, sizeof(rec) ) == -1) {
			    perror("READ ERROR");
			    break;
			}
		    } while ( rec.u_id == ID_MEMB );
		    goto after_read;
		}
		break;
	    case ID_ARS_B:
		for (i=0; i<24; i++)
		    rec.b.b_values[i] *= factor;
		break;
	    case ID_ARS_A:
		if ( rec.a.a_name[0] == '\0' )  {
		    /* Skip deleted junk */
		    lseek( ifd, (off_t)(rec.a.a_totlen) *
			   (long)(sizeof rec), 1 );
		    goto top;
		}
		rec.a.a_xmin *= factor;
		rec.a.a_xmax *= factor;
		rec.a.a_ymin *= factor;
		rec.a.a_ymax *= factor;
		rec.a.a_zmin *= factor;
		rec.a.a_zmax *= factor;
		break;
	    case ID_P_HEAD:
	    case ID_P_DATA:
		break;

	    default:
		printf("Garbage record in database\n");
		return 42;
	    case ID_MEMB:
		/* flip translation to other side */
#define m rec.M.m_mat
		m[3] *= factor;
		m[7] *= factor;
		m[11] *= factor;
		break;
	}
	ret = write( ofd, &rec, sizeof(rec) );
	if (ret < 0)
	    perror("write");

	count++;
    }
    printf("%d database granules written\n", count);
    return 0;
}

void
mat_pr(char *title, float *mp)
{
    int i;

    printf("MATRIX %s:\n  ", title);
    for (i=0; i<16; i++)  {
	printf(" %8.3f", mp[i]);
	if ( (i&3) == 3 ) printf("\n  ");
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
