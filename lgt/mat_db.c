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
		SCCS id:	@(#) mat_db.c	1.10
		Last edit: 	2/4/87 at 12:18:02
		Retrieved: 	2/4/87 at 12:18:20
		SCCS archive:	/vld/moss/src/libmatdb/s.mat_db.c
*/

#include <stdio.h>
#include <std.h>
#include "./mat_db.h"
static FILE		*mat_db_fp;
static Mat_Db_Entry	mat_db_table[MAX_MAT_DB];
static int		mat_db_size = 0;
Mat_Db_Entry		mat_dfl_entry =
				{
				0,		/* Material id.		*/
				4,		/* Shininess.		*/
				0.6,		/* Specular weight.	*/
				0.3,		/* Diffuse weight.	*/
				0.0,		/* Reflectivity.	*/
				0.0,		/* Transmission.	*/
				1.0,		/* Refractive index.	*/
				255, 255, 255,	/* Diffuse RGB values.	*/
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
				0, 0, 0,	/* Diffuse RGB values.	*/
				MF_NULL,	/* Mode flag.		*/
				"(null)"	/* Material name.	*/
				};
LOCAL int	get_Entry(), put_Entry();
LOCAL int	mat_W_Open();

/*	m a t _ O p e n _ D b ( )
	Open file, return file pointer or NULL.
 */
FILE *
mat_Open_Db( mat_db_file )
char	*mat_db_file;
	{	register	Mat_Db_Entry	*entry;
	if( (mat_db_fp = fopen( mat_db_file, "r+" )) == NULL )
		return	mat_db_fp; /* NULL */
	/* Mark all entries as NULL.					*/
	for( entry = mat_db_table; entry < &mat_db_table[MAX_MAT_DB]; entry++ )
		entry->mode_flag = MF_NULL;
	return	mat_db_fp;
	}

/*	m a t _ C l o s e _ D b ( )
	Close material database input file.  Return success or failure.
 */
mat_Close_Db()
	{
	return	fclose( mat_db_fp );
	}

/*	m a t _ P r i n t _ D b ( )
	Print material database entry.
	'prnt_func' must be a varargs function.
 */
mat_Print_Db( material_id, prnt_func )
int		material_id;
void		(*prnt_func)();
	{	register Mat_Db_Entry	*entry;
		
	if( material_id < 0 )
		return	-1;
	if( material_id < MAX_MAT_DB )
		entry = &mat_db_table[material_id];
	else 
		return	-1;
	if( entry->mode_flag == MF_NULL )
		entry = &mat_nul_entry;
	(*prnt_func)( "\n" );
	(*prnt_func)( "MATERIAL [%d] %s\n",
			material_id,
			entry->name[0] == '\0' ? "(untitled)" : entry->name
			);
	(*prnt_func)( "\tshininess\t\t(%d)\n", entry->shine );
	(*prnt_func)( "\tspecular weight\t\t(%g)\n", entry->wgt_specular );
	(*prnt_func)( "\tdiffuse weight\t\t(%g)\n", entry->wgt_diffuse );
	(*prnt_func)( "\ttransparency\t\t(%g)\n", entry->transparency );
	(*prnt_func)( "\treflectivity\t\t(%g)\n", entry->reflectivity );
	(*prnt_func)( "\trefractive index\t(%g)\n", entry->refrac_index );
	(*prnt_func)( "\tdiffuse color\t\t(%d %d %d)\n",
			entry->df_rgb[0],
			entry->df_rgb[1],
			entry->df_rgb[2]
			);
	return	1;
	}

/*	m a t _ A s c _ R e a d _ D b ( )
	Read ASCII material database into table, return number of entries.
 */
mat_Asc_Read_Db()
	{	Mat_Db_Entry	*entry;
	mat_db_size = 0;
	for(	entry = mat_db_table;
		entry < &mat_db_table[MAX_MAT_DB]
	     && get_Entry( entry );
		++entry
		)
		mat_db_size++;
	return	mat_db_size;
	}

/*	m a t _ B i n _ R e a d _ D b ( )
	Read binary material database into table, return number of entries.
 */
mat_Bin_Read_Db()
	{	Mat_Db_Entry	*entry;
	mat_db_size = 0;
	for(	entry = mat_db_table;
		entry < &mat_db_table[MAX_MAT_DB]
	     && fread( (char *) entry, sizeof(Mat_Db_Entry), 1, mat_db_fp ) == 1;
		++entry
		)
		mat_db_size++;
	return	mat_db_size;
	}

/*	m a t _ A s c _ S a v e _ D b ( )
	Write ASCII material database from table.  If 'file' is NULL,
	overwrite input file, otherwise open or create 'file'.  Return success
	or failure.
 */
mat_Asc_Save_Db( file )
char	*file;
	{	register Mat_Db_Entry	*entry;
	if( ! mat_W_Open( file ) )
		return	0;
	for(	entry = mat_db_table;
		entry < &mat_db_table[mat_db_size]
	     && put_Entry( entry );
		++entry
		)
		;
	if( entry != &mat_db_table[mat_db_size] )
		return	0;
	(void) fflush( mat_db_fp );
	return	1;
	}

/*	m a t _ B i n _ S a v e _ D b ( )
	Write binary material database from table.  If 'file' is NULL,
	overwrite input file, otherwise open or create 'file'.  Return success
	or failure.
 */
mat_Bin_Save_Db( file )
char	*file;
	{	register Mat_Db_Entry	*entry;
	if( ! mat_W_Open( file ) )
		return	0;
	for(	entry = mat_db_table;
		entry < &mat_db_table[mat_db_size]
	     && fwrite( (char *) entry, sizeof(Mat_Db_Entry), 1, mat_db_fp ) == 1;
		++entry
		)
		;
	if( entry != &mat_db_table[mat_db_size] )
		return	0;
	(void) fflush( mat_db_fp );
	return	1;
	}

/*	m a t _ P u t _ D b _ E n t r y ( )
	Create or overwrite entry in material table.
 */
mat_Put_Db_Entry( material_id )
int	material_id;
	{	register Mat_Db_Entry	*entry;
		static char		input_buf[BUFSIZ];
		int			red, grn, blu;
	if( material_id < 0 )
		return	-1;
	if( material_id < MAX_MAT_DB )
		{
		entry = &mat_db_table[material_id];
		entry->id = material_id;
		}
	else
		{
		(void) fprintf( stderr,
			"Material table full, MAX_DB_ENTRY too small.\n"
				);
		return	0;
		}
	if( get_Input( input_buf, BUFSIZ, "material name : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
		(void) strncpy( entry->name, input_buf, MAX_MAT_NM );
	if( get_Input( input_buf, BUFSIZ, "shine : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
		(void) sscanf( input_buf, "%d", &entry->shine );
	if( get_Input( input_buf, BUFSIZ, "specular weighting [0.0 .. 1.0] : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
#ifdef sgi
		(void) sscanf( input_buf, "%f", &entry->wgt_specular );
#else
		(void) sscanf( input_buf, "%lf", &entry->wgt_specular );
#endif
	if( get_Input( input_buf, BUFSIZ, "diffuse weighting [0.0 .. 1.0] : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
#ifdef sgi
		(void) sscanf( input_buf, "%f", &entry->wgt_diffuse );
#else
		(void) sscanf( input_buf, "%lf", &entry->wgt_diffuse );
#endif
	if( get_Input( input_buf, BUFSIZ, "transparency [0.0 .. 1.0] : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
#ifdef sgi
		(void) sscanf( input_buf, "%f", &entry->transparency );
#else	
		(void) sscanf( input_buf, "%lf", &entry->transparency );
#endif
	if( get_Input( input_buf, BUFSIZ, "reflectivity [0.0 .. 1.0] : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
#ifdef sgi
		(void) sscanf( input_buf, "%f", &entry->reflectivity );
#else
		(void) sscanf( input_buf, "%lf", &entry->reflectivity );
#endif
	if( get_Input( input_buf, BUFSIZ, "refractive index [0.9 .. 5.0] : " ) == NULL )
		return	0;
	if( input_buf[0] != '\0' )
#ifdef sgi
		(void) sscanf( input_buf, "%f", &entry->refrac_index );
#else
		(void) sscanf( input_buf, "%lf", &entry->refrac_index );
#endif
	if( get_Input( input_buf, BUFSIZ, "diffuse RGB values [0 .. 255] : " ) == NULL )
		return	0;
	if(	input_buf[0] != '\0'
	     &&	sscanf( input_buf, "%d %d %d", &red, &grn, &blu ) == 3
		)
		{
		entry->df_rgb[0] = red;
		entry->df_rgb[1] = grn;
		entry->df_rgb[2] = blu;
		}
	entry->mode_flag = MF_USED;
	mat_db_size = Max( mat_db_size, material_id+1 );
	return	1;
	}

/*	m a t _ G e t _ D b _ E n t r y ( )
	Return pointer to entry indexed by id or NULL.
 */
Mat_Db_Entry *
mat_Get_Db_Entry( material_id )
int	material_id;
	{
	if( material_id < 0 )
		return	MAT_DB_NULL;
	if( material_id < mat_db_size )
		return	&mat_db_table[material_id];
	else
		return	MAT_DB_NULL;
	}

LOCAL
get_Entry( entry )
register Mat_Db_Entry	*entry;
	{	register char	*ptr;
		int		items;
		int		red, grn, blu, mode;
	if( fgets( entry->name, MAX_MAT_NM, mat_db_fp ) == NULL )
		return	0;
	ptr = &entry->name[strlen(entry->name) - 1];
	if( *ptr == '\n' )
		/* Read entire line.					*/
		*ptr = '\0';
	else	/* Skip rest of line.					*/
		while( getc( mat_db_fp ) != '\n' )
			;
	if( (items = fscanf( mat_db_fp, "%d %d", &entry->id, &entry->shine )) != 2 )
		{
		(void) fprintf( stderr, "Could not read integers (%d read)!\n", items );
		return	0;
		}
	if(	fscanf(	mat_db_fp,
#ifdef sgi
			"%f %f %f %f %f",
#else
			"%lf %lf %lf %lf %lf",
#endif
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
	if( fscanf( mat_db_fp, "%d %d %d", &red, &grn, &blu ) != 3 )
		{
		(void) fprintf( stderr, "Could not read chars!\n" );
		return	0;
		}
	entry->df_rgb[0] = red;
	entry->df_rgb[1] = grn;
	entry->df_rgb[2] = blu;
	if( fscanf( mat_db_fp, "%d", &mode ) != 1 )
		{
		(void) fprintf( stderr,
				"get_Entry(): Could not read mode_flag!\n"
				);
		return	0;
		}
	entry->mode_flag = mode;
	while( getc( mat_db_fp ) != '\n' )
		; /* Gobble rest of line.				*/
	return	1;
	}

LOCAL
put_Entry( entry )
register Mat_Db_Entry	*entry;
	{
	if( entry->mode_flag == MF_NULL )
		entry = &mat_nul_entry;
	(void) fprintf( mat_db_fp, "%s\n", entry->name );
	(void) fprintf( mat_db_fp, "%d\n%d\n", entry->id, entry->shine );
	(void) fprintf(	mat_db_fp,
			"%f\n%f\n%f\n%f\n%f\n",
			entry->wgt_specular,
			entry->wgt_diffuse,
			entry->transparency,
			entry->reflectivity,
			entry->refrac_index
			);
	(void) fprintf(	mat_db_fp,
			"%u %u %u\n",
			(unsigned) entry->df_rgb[0],
			(unsigned) entry->df_rgb[1],
			(unsigned) entry->df_rgb[2]
			);
	(void) fprintf( mat_db_fp, "%u\n", (unsigned) entry->mode_flag );
	return	1;
	}

LOCAL
mat_W_Open( file )
char	*file;
	{
	if( file != NULL )
		{	register FILE	*fp;
		if( (fp = fopen( file, "w+" )) == NULL )
			return	0;
		else
			{
			mat_db_fp = fp;
			setbuf( mat_db_fp, malloc( BUFSIZ ) );
			}
		}
	/* else default to last database accessed.			*/
	if( mat_db_fp == NULL )
		return	0;
	return	fseek( mat_db_fp, 0L, 0 ) == 0;
	}
