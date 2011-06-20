/*                        L G T _ D B . C
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
/** @file lgt/lgt_db.c
    Author:		Gary S. Moss
*/

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./screen.h"
static int	get_Lgt_Entry(Lgt_Source *entry, FILE *fp), put_Lgt_Entry(Lgt_Source *entry, FILE *fp);

/*	l g t _ P r i n t _ D b ( )
	Print light source database entry.
*/
int
lgt_Print_Db(int id)
{
    Lgt_Source	*entry;
    int		stop;
    int			lines =	(PROMPT_LINE-TOP_SCROLL_WIN);
    if ( id >= lgt_db_size )
	return	0;
    else
	if ( id < 0 )
	{
	    stop = lgt_db_size - 1;
	    id = 0;
	}
	else
	    stop = id;
    for (; id <= stop; id++, lines-- )
    {
	entry = &lgts[id];
	if ( lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "\n" );
	if ( --lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "LIGHT SOURCE [%d] %s\n", id, entry->name );
	if ( --lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "       manual override\t(%s)\n", entry->over ? "ON" : "OFF" );
	if ( entry->stp == SOLTAB_NULL || entry->over )
	{
	    if ( --lines <= 0 && ! do_More( &lines ) )
		break;
	    prnt_Scroll( "       azimuth\t\t(%g)\n", entry->azim*RAD2DEG );
	    if ( --lines <= 0 && ! do_More( &lines ) )
		break;
	    prnt_Scroll( "       elevation\t(%g)\n", entry->elev*RAD2DEG );
	}
	if ( --lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "       distance\t\t(%g)\n", entry->dist );
	if ( --lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "       location\t\t<%g,%g,%g>\n",
		     entry->loc[X], entry->loc[Y], entry->loc[Z]
	    );
	if ( --lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "       gaussian beam\t(%s)\n", entry->beam ? "ON" : "OFF" );
	if ( entry->beam )
	{
	    if ( --lines <= 0 && ! do_More( &lines ) )
		break;
	    prnt_Scroll( "       beam radius\t(%g)\n", entry->radius );
	}
	if ( --lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "       intensity\t(%g)\n", entry->energy );
	if ( --lines <= 0 && ! do_More( &lines ) )
	    break;
	prnt_Scroll( "       color\t\t(%d %d %d)\n",
		     entry->rgb[0], entry->rgb[1], entry->rgb[2]
	    );
    }
    (void) fflush( stdout );
    return	1;
}

/*	l g t _ R e a d _ D b ( )
	Open light source database and read entries into table,
	return number of entries successfully read.
*/
int
lgt_Rd_Db(char *file)
{
    Lgt_Source	*entry;
    FILE		*fp;
    if ( (fp = fopen( file, "rb" )) == NULL )
	return	0;
    lgt_db_size = 0;
    for (	entry = lgts;
		entry < &lgts[MAX_LGTS]
		    && get_Lgt_Entry( entry, fp );
		++entry
	)
	lgt_db_size++;
    (void) fclose( fp );
    return	lgt_db_size;
}

/*	l g t _ S a v e _ D b ( )
	Write ASCII light source database from table.
	Return 1 for success, 0 for failure.
*/
int
lgt_Save_Db(char *file)
{
    Lgt_Source	*entry;
    FILE		*fp;

    if ( (fp = fopen( file, "wb" )) == NULL )
	return	0;
    setbuf( fp, bu_malloc( BUFSIZ, "buffer" ) );
    for (	entry = lgts;
		entry < &lgts[lgt_db_size]
		    && put_Lgt_Entry( entry, fp );
		++entry
	)
	;
    (void) fclose( fp );
    if ( entry != &lgts[lgt_db_size] )
	return	0;
    return	1;
}

/*	l g t _ E d i t _ D b _ E n t r y ( )
	Create or overwrite entry in light source table.
*/
int
lgt_Edit_Db_Entry(int id)
{
    Lgt_Source	*entry;
    char			input_buf[MAX_LN];
    char			editprompt[MAX_LN];
    int			red, grn, blu;
    if ( id < 0 || id >= MAX_LGTS )
	return	-1;
    V_MAX( lgt_db_size, id+1 );
    entry = &lgts[id];
    (void) snprintf( editprompt, MAX_LN, "light source name ? (%s) ", entry->name );
    if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	bu_strlcpy( entry->name, input_buf, MAX_LGT_NM );
    (void) sprintf( editprompt, "manual override ? [y|n](%c) ",
		    entry->over ? 'y' : 'n' );
    if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	entry->over = input_buf[0] != 'n';
    if ( entry->over || entry->stp == SOLTAB_NULL )
    {
	(void) sprintf( editprompt, "azimuth ? (%g) ",
			entry->azim*RAD2DEG );
	if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	{
	    (void) sscanf( input_buf, "%lf", &entry->azim );
	    entry->azim /= RAD2DEG;
	}
	(void) sprintf( editprompt, "elevation ? (%g) ",
			entry->elev*RAD2DEG );
	if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	{
	    (void) sscanf( input_buf, "%lf", &entry->elev );
	    entry->elev /= RAD2DEG;
	}
	(void) sprintf( editprompt, "distance ? (%g) ", entry->dist );
	if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	    (void) sscanf( input_buf, "%lf", &entry->dist );
    }
    (void) sprintf( editprompt, "gaussian beam ? [y|n](%c) ",
		    entry->beam ? 'y' : 'n' );
    if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	entry->beam = input_buf[0] != 'n';
    if ( entry->beam )
    {
	(void) sprintf( editprompt, "radius of beam ? (%g) ",
			entry->radius );
	if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	    (void) sscanf( input_buf, "%lf", &entry->radius );
    }
    (void) sprintf( editprompt, "intensity ? [0.0 to 1.0](%g) ",
		    entry->energy );
    if ( get_Input( input_buf, MAX_LN, editprompt ) != NULL )
	(void) sscanf( input_buf, "%lf", &entry->energy );
    (void) sprintf( editprompt, "color ? [0 to 255](%d %d %d) ",
		    entry->rgb[RED],
		    entry->rgb[GRN],
		    entry->rgb[BLU]
	);
    if (	get_Input( input_buf, MAX_LN, editprompt ) != NULL
		&&	sscanf( input_buf, "%d %d %d", &red, &grn, &blu ) == 3
	)
    {
	entry->rgb[RED] = red;
	entry->rgb[GRN] = grn;
	entry->rgb[BLU] = blu;
    }
    return	1;
}

static int
get_Lgt_Entry(Lgt_Source *entry, FILE *fp)
{
    char	*ptr;
    int		red, grn, blu;
    if ( bu_fgets( entry->name, MAX_LGT_NM, fp ) == NULL )
	return	0;
    ptr = &entry->name[strlen(entry->name) - 1];
    if ( *ptr == '\n' )
	/* Read entire line.					*/
	*ptr = '\0';
    else	/* Skip rest of line.					*/
	while ( getc( fp ) != '\n' )
	    ;
    if ( fscanf( fp, "%d %d", &entry->beam, &entry->over ) != 2 )
	return	0;
    if ( fscanf( fp, "%d %d %d", &red, &grn, &blu ) != 3 )
	return	0;
    entry->rgb[0] = red;
    entry->rgb[1] = grn;
    entry->rgb[2] = blu;
    if (	fscanf(	fp,
			"%lf %lf %lf %lf %lf",
			&entry->azim,
			&entry->elev,
			&entry->dist,
			&entry->energy,
			&entry->radius
		    ) != 5
	)
	return	0;
    while ( getc( fp ) != '\n' )
	; /* Gobble rest of line.				*/
    return	1;
}

static int
put_Lgt_Entry(Lgt_Source *entry, FILE *fp)
{
    (void) fprintf( fp, "%s\n", entry->name );
    (void) fprintf(	fp,
			"%d\n%d\n%u %u %u\n",
			entry->beam,
			entry->over,
			(unsigned) entry->rgb[0],
			(unsigned) entry->rgb[1],
			(unsigned) entry->rgb[2]
	);
    (void) fprintf( fp,
		    "%f\n%f\n%f\n%f\n%f\n",
		    entry->azim,
		    entry->elev,
		    entry->dist,
		    entry->energy,
		    entry->radius
	);
    return	1;
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
