/*                        M A T _ D B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file mat_db.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./vecmath.h"
#include "./mat_db.h"
#include "./screen.h"

static Mat_Db_Entry	mat_db_table[MAX_MAT_DB];
static int		mat_db_size = 0;
Mat_Db_Entry		mat_dfl_entry =
				{
				0,		/* Material id.		*/
				4,		/* Shininess.		*/
				0.6,		/* Specular weight.	*/
				0.4,		/* Diffuse weight.	*/
				0.0,		/* Reflectivity.	*/
				0.0,		/* Transmission.	*/
				1.0,		/* Refractive index.	*/
				{255, 255, 255},/* Diffuse RGB values.	*/
				MF_USED,	/* Mode flag.		*/
				"(default)"	/* Material name.	*/
				};
Mat_Db_Entry		mat_nul_entry =
				{
				0,		/* Material id.		*/
				0,		/* Shininess.		*/
				0.0,		/* Specular weight.	*/
				0.0,		/* Diffuse weight.	*/
				0.0,		/* Reflectivity.	*/
				0.0,		/* Transmission.	*/
				0.0,		/* Refractive index.	*/
				{0, 0, 0},	/* Diffuse RGB values.	*/
				MF_NULL,	/* Mode flag.		*/
				"(null)"	/* Material name.	*/
				};
static int	get_Mat_Entry(register Mat_Db_Entry *entry, FILE *fp), put_Mat_Entry(register Mat_Db_Entry *entry, register FILE *fp);

/*	m a t _ R d _ D b ( )
	Open material database and read entries into table,
	return number of entries successfully read.
 */
int
mat_Rd_Db(char *file)
{	register Mat_Db_Entry	*entry;
		register FILE		*fp;
	if( (fp = fopen( file, "r" )) == NULL )
		return	0;
	/* Mark all entries as NULL.					*/
	for( entry = mat_db_table; entry < &mat_db_table[MAX_MAT_DB]; entry++ )
		entry->mode_flag = MF_NULL;
	mat_db_size = 0;
	for(	entry = mat_db_table;
		entry < &mat_db_table[MAX_MAT_DB]
	     && get_Mat_Entry( entry, fp );
		++entry
		)
		mat_db_size++;
	(void) fclose( fp );
	return	mat_db_size;
	}

/*	m a t _ P r i n t _ D b ( )
	Print material database entry.
 */
int
mat_Print_Db(int material_id)
{	register Mat_Db_Entry	*entry;
		register int		stop;
		register int		success = 0;
		int			lines =	(PROMPT_LINE-TOP_SCROLL_WIN);
	if( material_id >= MAX_MAT_DB )
		{
		bu_log( "Material data base only has %d entries.\n",
			MAX_MAT_DB
			);
		bu_log( "If this is not enough, notify the support staff.\n" );
		return	success;
		}
	else
	if( material_id < 0 )
		{
		stop = MAX_MAT_DB - 1;
		material_id = 0;
		}
	else
		stop = material_id;
	for( ; material_id <= stop; material_id++, lines-- )
		{
		entry = &mat_db_table[material_id];
		if( entry->mode_flag == MF_NULL )
			continue;
		success = 1;
		if( lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "\n" );
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "MATERIAL [%d] %s\n",
				material_id,
				entry->name[0] == '\0' ? "(untitled)" : entry->name
				);
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "        shininess\t\t(%d)\n", entry->shine );
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "        specular weight\t\t(%g)\n", entry->wgt_specular );
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "        diffuse weight\t\t(%g)\n", entry->wgt_diffuse );
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "        transparency\t\t(%g)\n", entry->transparency );
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "        reflectivity\t\t(%g)\n", entry->reflectivity );
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		prnt_Scroll( "        refractive index\t(%g)\n", entry->refrac_index );
		if( --lines <= 0 && ! do_More( &lines ) )
			break;
		if( strncmp( TEX_KEYWORD, entry->name, TEX_KEYLEN ) != 0 )
			prnt_Scroll( "        diffuse color\t\t(%d %d %d)\n",
					entry->df_rgb[0],
					entry->df_rgb[1],
					entry->df_rgb[2]
					);
		else
			prnt_Scroll( "        texture map : width=%d height=%d\n",
					entry->df_rgb[0] << 3,
					entry->df_rgb[1] << 3
					);
		}
	return	success;
	}

/*	m a t _ S a v e _ D b ( )
	Write ASCII material database from table.
	Return 1 for success, 0 for failure.
 */
int
mat_Save_Db(char *file)
{	register Mat_Db_Entry	*entry;
		register FILE		*fp;
	if( (fp = fopen( file, "w" )) == NULL )
		return	0;
	setbuf( fp, malloc( BUFSIZ ) );
	for(	entry = mat_db_table;
		entry < &mat_db_table[mat_db_size]
	     && put_Mat_Entry( entry, fp );
		++entry
		)
		;
	(void) fclose( fp );
	if( entry != &mat_db_table[mat_db_size] )
		return	0;
	return	1;
	}


/*	m a t _ E d i t _ D b _ E n t r y ( )
	Create or overwrite entry in material table.
 */
int
mat_Edit_Db_Entry(int id)
{	register Mat_Db_Entry	*entry;
		char			input_buf[MAX_LN];
		char			prompt[MAX_LN];
		int			red, grn, blu;
	if( id < 0 )
		return	-1;
	if( id < MAX_MAT_DB )
		{
		entry = &mat_db_table[id];
		entry->id = id;
		}
	else
		{
		bu_log( "Material table full, MAX_DB_ENTRY too small.\n" );
		return	0;
		}
	(void) sprintf( prompt, "material name ? (%s) ", entry->name );
	if( get_Input( input_buf, MAX_LN, prompt ) != NULL )
		(void) strncpy( entry->name, input_buf, MAX_MAT_NM );
	(void) sprintf( prompt, "shine ? [1 to n](%d) ", entry->shine );
	if( get_Input( input_buf, MAX_LN, prompt ) != NULL )
		(void) sscanf( input_buf, "%d", &entry->shine );
	(void) sprintf( prompt, "specular weighting ? [0.0 to 1.0](%g) ",
			entry->wgt_specular );
	if( get_Input( input_buf, MAX_LN, prompt  ) != NULL )
		(void) sscanf( input_buf, "%lf", &entry->wgt_specular );
	(void) sprintf( prompt, "diffuse weighting ? [0.0 to 1.0](%g) ",
			entry->wgt_diffuse );
	if( get_Input( input_buf, MAX_LN, prompt ) != NULL )
		(void) sscanf( input_buf, "%lf", &entry->wgt_diffuse );
	(void) sprintf( prompt, "transparency ? [0.0 to 1.0](%g) ",
			entry->transparency );
	if( get_Input( input_buf, MAX_LN, prompt ) != NULL )
		(void) sscanf( input_buf, "%lf", &entry->transparency );
	(void) sprintf( prompt, "reflectivity ? [0.0 to 1.0](%g) ",
			entry->reflectivity );
	if( get_Input( input_buf, MAX_LN, prompt ) != NULL )
		(void) sscanf( input_buf, "%lf", &entry->reflectivity );
	(void) sprintf( prompt, "refractive index ? [0.9 to 5.0](%g) ",
			entry->refrac_index );
	if( get_Input( input_buf, MAX_LN, prompt ) != NULL )
		(void) sscanf( input_buf, "%lf", &entry->refrac_index );

	if( strncmp( TEX_KEYWORD, entry->name, TEX_KEYLEN ) != 0 )
		{
		(void) sprintf( prompt, "diffuse RGB values ? [0 to 255](%d %d %d) ",
				entry->df_rgb[RED],
				entry->df_rgb[GRN],
				entry->df_rgb[BLU]
				);
		if(	get_Input( input_buf, MAX_LN, prompt ) != NULL
		     &&	sscanf( input_buf, "%d %d %d", &red, &grn, &blu ) == 3
			)
			{
			entry->df_rgb[0] = red;
			entry->df_rgb[1] = grn;
			entry->df_rgb[2] = blu;
			}
		}
	else
		{
		(void) sprintf( prompt, "texture : width and height? [0 to 1024](%d %d) ",
				entry->df_rgb[0]<<3,
				entry->df_rgb[1]<<3
				);
		if(	get_Input( input_buf, MAX_LN, prompt ) != NULL
	 	    &&	sscanf( input_buf, "%d %d", &red, &grn ) == 2
			)
			{
			entry->df_rgb[0] = red >> 3;
			entry->df_rgb[1] = grn >> 3;
			}
		}
	entry->mode_flag = MF_USED;
	mat_db_size = Max( mat_db_size, id+1 );
	return	1;
	}

/*	m a t _ G e t _ D b _ E n t r y ( )
	Return pointer to entry indexed by id or NULL.
 */
Mat_Db_Entry *
mat_Get_Db_Entry(int id)
{
	if( id < 0 )
		return	MAT_DB_NULL;
	if( id < mat_db_size )
		return	&mat_db_table[id];
	else
		return	MAT_DB_NULL;
	}

static int
get_Mat_Entry(register Mat_Db_Entry *entry, FILE *fp)
{	register char	*ptr;
		int		items;
		int		red, grn, blu, mode;
	if( fgets( entry->name, MAX_MAT_NM, fp ) == NULL )
		return	0;
	ptr = &entry->name[strlen(entry->name) - 1];
	if( *ptr == '\n' )
		/* Read entire line.					*/
		*ptr = '\0';
	else	/* Skip rest of line.					*/
		while( getc( fp ) != '\n' )
			;
	if( (items = fscanf( fp, "%d %d", &entry->id, &entry->shine )) != 2 )
		{
		(void) fprintf( stderr, "Could not read integers (%d read)!\n", items );
		return	0;
		}
	if(	fscanf(	fp,
			"%lf %lf %lf %lf %lf",
			&entry->wgt_specular,
			&entry->wgt_diffuse,
			&entry->transparency,
			&entry->reflectivity,
			&entry->refrac_index
			) != 5
		)
		{
		(void) fprintf( stderr, "Could not read floats!\n" );
		return	0;
		}
	if( fscanf( fp, "%d %d %d", &red, &grn, &blu ) != 3 )
		{
		(void) fprintf( stderr, "Could not read chars!\n" );
		return	0;
		}
	entry->df_rgb[0] = red;
	entry->df_rgb[1] = grn;
	entry->df_rgb[2] = blu;
	if( fscanf( fp, "%d", &mode ) != 1 )
		{
		(void) fprintf( stderr,
				"get_Mat_Entry(): Could not read mode_flag!\n"
				);
		return	0;
		}
	entry->mode_flag = mode;
	while( getc( fp ) != '\n' )
		; /* Gobble rest of line.				*/
	return	1;
	}

static int
put_Mat_Entry(register Mat_Db_Entry *entry, register FILE *fp)
{
	if( entry->mode_flag == MF_NULL )
		entry = &mat_nul_entry;
	(void) fprintf( fp, "%s\n", entry->name );
	(void) fprintf( fp, "%d\n%d\n", entry->id, entry->shine );
	(void) fprintf(	fp,
			"%f\n%f\n%f\n%f\n%f\n",
			entry->wgt_specular,
			entry->wgt_diffuse,
			entry->transparency,
			entry->reflectivity,
			entry->refrac_index
			);
	(void) fprintf(	fp,
			"%u %u %u\n",
			(unsigned) entry->df_rgb[0],
			(unsigned) entry->df_rgb[1],
			(unsigned) entry->df_rgb[2]
			);
	(void) fprintf( fp, "%u\n", (unsigned) entry->mode_flag );
	return	1;
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
