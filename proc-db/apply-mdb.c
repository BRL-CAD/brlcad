/*
 *			A P P L Y - M D B . C
 *
 *  A quick hack to convert LGT-style region-id based material descriptions
 *  into RT-style material command strings.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "db.h"
#include "../lgt/mat_db.h"

union record rec;

char buf[256];

main(argc, argv)
char **argv;
{
	register Mat_Db_Entry *mp;
	register char *cp;

	if( argc != 2 )  {
		fprintf(stderr, "Usage:  apply-mdb file.mdb < a.g > b.g\n");
		exit(1);
	}
	mat_Open_Db(argv[1]);
	mat_Asc_Read_Db();

	while(
	    fread( (char *)&rec, sizeof(rec), 1, stdin ) == 1  &&
	    !feof(stdin)
	)  {
		if( rec.u_id != ID_COMB || rec.c.c_flags != DBV4_NON_REGION )  {
			fwrite( (char *)&rec, sizeof(rec), 1, stdout );
			continue;
		}
		mp = mat_Get_Db_Entry(rec.c.c_material);
		fprintf(stderr,"%s mater=%d, %s\n",
			rec.c.c_name,
			rec.c.c_material, mp->name);
		rec.c.c_rgb[0] = mp->df_rgb[0];
		rec.c.c_rgb[1] = mp->df_rgb[1];
		rec.c.c_rgb[2] = mp->df_rgb[2];
		rec.c.c_override = 1;
		sprintf(rec.c.c_matname, "plastic");
		cp = buf;
#define END(c)	while( *cp )  cp++
		if( mp->shine != 10 )  {
			sprintf(cp,"sh=%d ", mp->shine );
			END(cp);
		}
		if( mp->wgt_specular != 0.7 )  {
			sprintf(cp,"sp=%g ", mp->wgt_specular);
			END(cp);
		}
		if( mp->wgt_diffuse != 0.3 )  {
			sprintf(cp, "di=%g ", mp->wgt_diffuse );
			END(cp);
		}
		if( mp->transparency > 0.0 )  {
			sprintf(cp,"tr=%g ", mp->transparency );
			END(cp);
		}
		if( mp->reflectivity > 0.0 )  {
			sprintf(cp,"re=%g ", mp->reflectivity );
			END(cp);
		}
		if( mp->refrac_index > 0.0 )  {
			sprintf(cp,"ri=%g ", mp->refrac_index );
			END(cp);
		}
		strncpy( rec.c.c_matparm, buf, 59 );
		fprintf(stderr, "%s\n", buf );
		fwrite( (char *)&rec, sizeof(rec), 1, stdout );
	}
}

#include "../lgt/mat_db.c"
