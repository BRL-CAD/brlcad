/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
	Originally extracted from SCCS archive:
		SCCS id:	@(#) lgt_db.c	2.2
		Modified: 	1/30/87 at 17:22:29	G S M
		Retrieved: 	2/4/87 at 08:53:39
		SCCS archive:	/vld/moss/src/lgt/s.lgt_db.c
*/

#include <stdio.h>
#include <std.h>
#include <machine.h>
#include <vmath.h>
#include <raytrace.h>
#include <vecmath.h>
#include <lgt.h>
#include "./extern.h"
_LOCAL_ int	get_Lgt_Entry(), put_Lgt_Entry();

/*	l g t _ P r i n t _ D b ( )
	Print light source database entry.
	'prnt_func' must be a varargs function.
 */
lgt_Print_Db( id, prnt_func )
int		id;
void		(*prnt_func)();
	{	register Lgt_Source	*entry;
	if( id < 0 || id >= lgt_db_size )
		return	-1;
	entry = &lgts[id];
	(*prnt_func)( "\n" );
	(*prnt_func)( "LIGHT SOURCE [%d] %s\n", id, entry->name );
	(*prnt_func)( "\tmanual overide\t(%s)\n", entry->over ? "ON" : "OFF" );
	if( entry->stp == SOLTAB_NULL || entry->over )
		{
		(*prnt_func)( "\tazimuth\t\t(%g)\n", entry->azim*DEGRAD );
		(*prnt_func)( "\televation\t(%g)\n", entry->elev*DEGRAD );
		
		}
	(*prnt_func)( "\tdistance\t(%g)\n", entry->dist );
	V_Print( "\tlocation", entry->loc, prnt_Scroll );
	(*prnt_func)( "\tgaussian beam\t(%s)\n", entry->beam ? "ON" : "OFF" );
	if( entry->beam )
		(*prnt_func)( "\tbeam radius\t(%g)\n", entry->radius );
	(*prnt_func)( "\tintensity\t(%g)\n", entry->energy );
	(*prnt_func)( "\tcolor\t\t(%d %d %d)\n",
			entry->rgb[0], entry->rgb[1], entry->rgb[2]
			);
	return	1;
	}

/*	l g t _ R e a d _ D b ( )
	Open light source database and read entries into table,
	return number of entries successfully read.
 */
lgt_Read_Db( file )
char	*file;
	{	register Lgt_Source	*entry;
		register FILE		*fp;
	if( (fp = fopen( file, "r" )) == NULL )
		return	0;
	lgt_db_size = 0;
	for(	entry = lgts;
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
lgt_Save_Db( file )
char	*file;
	{
	register Lgt_Source	*entry;
	register FILE		*fp;

	if( (fp = fopen( file, "w" )) == NULL )
		return	0;
	setbuf( fp, malloc( BUFSIZ ) );
	for(	entry = lgts;
		entry < &lgts[lgt_db_size]
	     && put_Lgt_Entry( entry, fp );
		++entry
		)
		;
	(void) fclose( fp );
	if( entry != &lgts[lgt_db_size] )
		return	0;
	return	1;
	}

/*	l g t _ E d i t _ D b _ E n t r y ( )
	Create or overwrite entry in light source table.
 */
lgt_Edit_Db_Entry( id )
int	id;
	{	register Lgt_Source	*entry;
		static char		input_buf[BUFSIZ];
		int			red, grn, blu;
	if( id < 0 || id >= MAX_LGTS )
		return	-1;
	lgt_db_size = Max( lgt_db_size, id+1 );
	entry = &lgts[id];
	if( get_Input( input_buf, BUFSIZ, "light source name : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
		(void) strncpy( entry->name, input_buf, MAX_LGT_NM-1 );
	if( get_Input( input_buf, BUFSIZ, "manual overide : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
		(void) sscanf( input_buf, "%d", &entry->over );
	if( entry->over || entry->stp == SOLTAB_NULL )
		{
		if( get_Input( input_buf, BUFSIZ, "azimuth : " ) == NULL )
			return	0;
		if( input_buf[0] != '\0' )
			{
#ifdef sgi
			(void) sscanf( input_buf, "%f", &entry->azim );
#else
			(void) sscanf( input_buf, "%lf", &entry->azim );
#endif
			entry->azim /= DEGRAD;
			}
		if( get_Input( input_buf, BUFSIZ, "elevation : " ) == NULL )
			return	0;
		if( input_buf[0] != '\0' )
			{
#ifdef sgi
			(void) sscanf( input_buf, "%f", &entry->elev );
#else
			(void) sscanf( input_buf, "%lf", &entry->elev );
#endif
			entry->elev /= DEGRAD;
			}
		if( get_Input( input_buf, BUFSIZ, "distance : " ) == NULL )
			return	0;
		if( input_buf[0] != '\0' )
#ifdef sgi
			(void) sscanf( input_buf, "%f", &entry->dist );
#else
			(void) sscanf( input_buf, "%lf", &entry->dist );
#endif
		}
	if( get_Input( input_buf, BUFSIZ, "gaussian beam : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
		(void) sscanf( input_buf, "%d", &entry->beam );
	if( entry->beam )
		{
		if( get_Input( input_buf, BUFSIZ, "radius of beam : " ) == NULL )
			return	0;
		if( input_buf[0] != '\0' )
#ifdef sgi
			(void) sscanf( input_buf, "%f", &entry->radius );
#else
			(void) sscanf( input_buf, "%lf", &entry->radius );
#endif
		}
	if( get_Input( input_buf, BUFSIZ, "intensity : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
#ifdef sgi
		(void) sscanf( input_buf, "%f", &entry->energy );
#else
		(void) sscanf( input_buf, "%lf", &entry->energy );
#endif
	if( get_Input( input_buf, BUFSIZ, "color : " ) == NULL )
		return	0;
	if(	input_buf[0] != '\0'
	     &&	sscanf( input_buf, "%d %d %d", &red, &grn, &blu ) == 3
		)
		{
		entry->rgb[0] = red;
		entry->rgb[1] = grn;
		entry->rgb[2] = blu;
		}
	return	1;
	}

/*	l g t _ G e t _ D b _ E n t r y ( )
	Return pointer to entry indexed by id or NULL.
 */
Lgt_Source *
lgt_Get_Db_Entry( lgt_id )
int	lgt_id;
	{
	if( lgt_id >= 0 && lgt_id < lgt_db_size )
		return	&lgts[lgt_id];
	else
		return	LGT_NULL;
	}

_LOCAL_ int
get_Lgt_Entry( entry, fp )
register Lgt_Source	*entry;
FILE			*fp;
	{	register char	*ptr;
		int		red, grn, blu;
	if( fgets( entry->name, MAX_LGT_NM, fp ) == NULL )
		return	0;
	ptr = &entry->name[strlen(entry->name) - 1];
	if( *ptr == '\n' )
		/* Read entire line.					*/
		*ptr = '\0';
	else	/* Skip rest of line.					*/
		while( getc( fp ) != '\n' )
			;
	if( fscanf( fp, "%d %d", &entry->beam, &entry->over ) != 2 )
		return	0;
	if( fscanf( fp, "%d %d %d", &red, &grn, &blu ) != 3 )
		return	0;
	entry->rgb[0] = red;
	entry->rgb[1] = grn;
	entry->rgb[2] = blu;
	if(	fscanf(	fp,
#ifdef sgi
			"%f %f %f %f %f",
#else
			"%lf %lf %lf %lf %lf",
#endif
			&entry->azim,
			&entry->elev,
			&entry->dist,
			&entry->energy,
			&entry->radius
			) != 5
		)
		return	0;
	while( getc( fp ) != '\n' )
		; /* Gobble rest of line.				*/
	return	1;
	}

_LOCAL_ int
put_Lgt_Entry( entry, fp )
register Lgt_Source	*entry;
FILE			*fp;
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
