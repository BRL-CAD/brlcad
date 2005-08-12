/*                      D B 5 _ S C A N . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup db5 */
/*@{*/
/** @file db5_scan.c
 *  Scan a v5 database, sending each object off to a handler.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"

#include "./debug.h"

/**
 *			D B 5 _ S C A N
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int
db5_scan(
	struct db_i	*dbip,
	void		(*handler)(struct db_i *,
				const struct db5_raw_internal *,
				long addr, genptr_t client_data ),
	genptr_t	client_data )
{
	unsigned char	header[8];
	struct db5_raw_internal	raw;
	int			got;
	long			nrec;
	long			addr;

	RT_CK_DBI(dbip);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_scan( x%x, x%x )\n", dbip, handler);

	raw.magic = DB5_RAW_INTERNAL_MAGIC;
	nrec = 0L;

	/* Fast-path when file is already memory-mapped */
	if( dbip->dbi_mf )  {
		const unsigned char	*cp = (const unsigned char *)dbip->dbi_inmem;
		long	eof;

		BU_CK_MAPPED_FILE(dbip->dbi_mf);
		eof = dbip->dbi_mf->buflen;

		if( db5_header_is_valid( cp ) == 0 )  {
			bu_log("db5_scan ERROR:  %s is lacking a proper BRL-CAD v5 database header\n", dbip->dbi_filename);
		    	goto fatal;
		}
		cp += sizeof(header);
		addr = sizeof(header);
		while( addr < eof )  {
			if( (cp = db5_get_raw_internal_ptr( &raw, cp )) == NULL )  {
				goto fatal;
			}
			(*handler)(dbip, &raw, addr, client_data);
			nrec++;
			addr += raw.object_length;
		}
		dbip->dbi_eof = addr;
		BU_ASSERT_LONG( dbip->dbi_eof, ==, dbip->dbi_mf->buflen );
	}  else  {
		/* In a totally portable way, read the database with stdio */
		rewind( dbip->dbi_fp );
		if( fread( header, sizeof header, 1, dbip->dbi_fp ) != 1  ||
		    db5_header_is_valid( header ) == 0 )  {
			bu_log("db5_scan ERROR:  %s is lacking a proper BRL-CAD v5 database header\n", dbip->dbi_filename);
		    	goto fatal;
		}
		for(;;)  {
			addr = ftell( dbip->dbi_fp );
			if( (got = db5_get_raw_internal_fp( &raw, dbip->dbi_fp )) < 0 )  {
				if( got == -1 )  break;		/* EOF */
				goto fatal;
			}
			(*handler)(dbip, &raw, addr, client_data);
			nrec++;
			if(raw.buf)  {
				bu_free(raw.buf, "raw v5 object");
				raw.buf = NULL;
			}
		}
		dbip->dbi_eof = ftell( dbip->dbi_fp );
		rewind( dbip->dbi_fp );
	}

	dbip->dbi_nrec = nrec;		/* # obj in db, not inc. header */
	return 0;			/* success */

fatal:
	dbip->dbi_read_only = 1;	/* Writing would corrupt it worse */
	return -1;			/* fatal error */
}

struct directory *
db_diradd5(
	   struct db_i				*dbip,
	   const char				*name,
	   long					laddr,
	   unsigned char			major_type,
	   unsigned char 			minor_type,
	   unsigned char			name_hidden,
	   long					object_length,
	   struct bu_attribute_value_set	*avs)
{
	struct directory **headp;
	register struct directory *dp;
	struct bu_vls local;

	RT_CK_DBI( dbip );

	bu_vls_init(&local);
	bu_vls_strcpy(&local, name);
	if (db_dircheck(dbip, &local, 0, &headp) < 0) {
		bu_vls_free(&local);
		return DIR_NULL;
	}

	/* Duplicates the guts of db_diradd() */
	RT_GET_DIRECTORY( dp, &rt_uniresource );
	RT_CK_DIR(dp);
	BU_LIST_INIT( &dp->d_use_hd );
	RT_DIR_SET_NAMEP(dp, bu_vls_addr( &local ));	/* sets d_namep */
	bu_vls_free( &local );
	dp->d_un.ptr = NULL;
	dp->d_un.file_offset = laddr;
	dp->d_major_type = major_type;
	dp->d_minor_type = minor_type;
	switch( major_type )  {
	case DB5_MAJORTYPE_BRLCAD:
		if( minor_type == ID_COMBINATION )  {

			dp->d_flags = DIR_COMB;
			if( !avs || avs->count == 0 )  break;
			/*
			 *  check for the "region=" attribute.
			 */
			if( bu_avs_get( avs, "region" ) != NULL )
				dp->d_flags = DIR_COMB|DIR_REGION;
		} else {
			dp->d_flags = DIR_SOLID;
		}
		break;
	case DB5_MAJORTYPE_BINARY_EXPM:
	case DB5_MAJORTYPE_BINARY_UNIF:
	case DB5_MAJORTYPE_BINARY_MIME:
		/* XXX Do we want to define extra flags for this? */
		dp->d_flags = DIR_NON_GEOM;
		break;
	case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		dp->d_flags = 0;
	}
	if( name_hidden )
		dp->d_flags |= DIR_HIDDEN;
	dp->d_len = object_length;		/* in bytes */
	BU_LIST_INIT( &dp->d_use_hd );
	dp->d_animate = NULL;
	dp->d_nref = 0;
	dp->d_uses = 0;
	dp->d_forw = *headp;
	*headp = dp;

	return( dp );
}

struct directory *
db5_diradd(struct db_i			*dbip,
	   const struct db5_raw_internal *rip,
	   long				laddr,
	   genptr_t			client_data )
{
	struct directory **headp;
	register struct directory *dp;
	struct bu_vls local;

	RT_CK_DBI( dbip );

	bu_vls_init(&local);
	bu_vls_strcpy(&local, rip->name.ext_buf);
	if (db_dircheck(dbip, &local, 0, &headp) < 0) {
		bu_vls_free(&local);
		return DIR_NULL;
	}

	/* Duplicates the guts of db_diradd() */
	RT_GET_DIRECTORY( dp, &rt_uniresource );
	RT_CK_DIR(dp);
	BU_LIST_INIT( &dp->d_use_hd );
	RT_DIR_SET_NAMEP(dp, bu_vls_addr(&local));	/* sets d_namep */
	bu_vls_free(&local);
	dp->d_un.file_offset = laddr;
	dp->d_major_type = rip->major_type;
	dp->d_minor_type = rip->minor_type;
	switch( rip->major_type )  {
	case DB5_MAJORTYPE_BRLCAD:
		if( rip->minor_type == ID_COMBINATION )  {
			struct bu_attribute_value_set	avs;

			bu_avs_init_empty(&avs);

			dp->d_flags = DIR_COMB;
			if( rip->attributes.ext_nbytes == 0 )  break;
			/*
			 *  Crack open the attributes to
			 *  check for the "region=" attribute.
			 */
			if( db5_import_attributes( &avs, &rip->attributes ) < 0 )  {
				bu_log("db5_diradd_handler: Bad attributes on combination '%s'\n",
					rip->name);
				break;
			}
			if( bu_avs_get( &avs, "region" ) != NULL )
				dp->d_flags = DIR_COMB|DIR_REGION;
			bu_avs_free( &avs );
		} else {
			dp->d_flags = DIR_SOLID;
		}
		break;
	case DB5_MAJORTYPE_BINARY_EXPM:
	case DB5_MAJORTYPE_BINARY_UNIF:
	case DB5_MAJORTYPE_BINARY_MIME:
		/* XXX Do we want to define extra flags for this? */
		dp->d_flags = DIR_NON_GEOM;
		break;
	case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		dp->d_flags = 0;
	}
	if( rip->h_name_hidden )
		dp->d_flags |= DIR_HIDDEN;
	dp->d_len = rip->object_length;		/* in bytes */
	BU_LIST_INIT( &dp->d_use_hd );
	dp->d_animate = NULL;
	dp->d_nref = 0;
	dp->d_uses = 0;
	dp->d_forw = *headp;
	*headp = dp;

	return( dp );
}

/**
 *			D B 5 _ D I R A D D _ H A N D L E R
 *
 * In support of db5_scan, add a named entry to the directory.
 */
HIDDEN void
db5_diradd_handler(
	struct db_i		*dbip,
	const struct db5_raw_internal *rip,
	long			laddr,
	genptr_t		client_data )	/* unused client_data from db5_scan() */
{
	RT_CK_DBI(dbip);

	if( rip->h_dli == DB5HDR_HFLAGS_DLI_HEADER_OBJECT )  return;
	if( rip->h_dli == DB5HDR_HFLAGS_DLI_FREE_STORAGE )  {
		/* Record available free storage */
		rt_memfree( &(dbip->dbi_freep), rip->object_length, laddr );
		return;
	}
	
	/* If somehow it doesn't have a name, ignore it */
	if( rip->name.ext_buf == NULL )  return;

	if(RT_G_DEBUG&DEBUG_DB)  {
		bu_log("db5_diradd_handler(dbip=x%x, name='%s', addr=x%x, len=%d)\n",
			dbip, rip->name, laddr, rip->object_length );
	}

	db5_diradd( dbip, rip, laddr, client_data );

	return;
}

/**
 *			D B _ D I R B U I L D
 *
 *  A generic routine to determine the type of the database,
 *  (v4 or v5)
 *  and to invoke the appropriate db_scan()-like routine to
 *  build the in-memory directory.
 *
 *  It is the caller's responsibility to close the database in case of error.
 *
 *  Called from rt_dirbuild(), and g_submodel.c
 *
 *  Returns -
 *	0	OK
 *	-1	failure
 */
int
db_dirbuild( struct db_i *dbip )
{
	unsigned char	header[8];

	RT_CK_DBI(dbip);

	/* First, determine what version database this is */
	rewind(dbip->dbi_fp);
	if( fread( header, sizeof header, 1, dbip->dbi_fp ) != 1  )  {
		bu_log("db_dirbuild(%s) ERROR, file too short to be BRL-CAD database\n",
			dbip->dbi_filename);
		return -1;
	}

	if( db5_header_is_valid( header ) )  {
		struct directory	*dp;
		struct bu_external	ext;
		struct db5_raw_internal	raw;
		struct bu_attribute_value_set	avs;
		const char		*cp;

		bu_avs_init_empty(&avs);

		/* File is v5 format */
#if 0
		bu_log("NOTICE:  %s is BRL-CAD v5 format.\n", dbip->dbi_filename);
#endif
		dbip->dbi_version = 5;
		if( db5_scan( dbip, db5_diradd_handler, NULL ) < 0 )  {
			bu_log("db_dirbuild(%s): db5_scan() failed\n",
				dbip->dbi_filename);
			return -1;
		}

		/* Need to retrieve _GLOBAL object and obtain title and units */
		if( (dp = db_lookup( dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_NOISY )) == DIR_NULL )  {
			bu_log("db_dirbuild(%s): improper database, no %s object\n",
				dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME );
			dbip->dbi_title = bu_strdup(DB5_GLOBAL_OBJECT_NAME);
			/* Missing _GLOBAL object so create it and set default title and units */
			db5_update_ident(dbip, "Untitled BRL-CAD Database",1.0);
			return 0;	/* not a fatal error, user may have deleted it */
		}
		BU_INIT_EXTERNAL(&ext);
		if( db_get_external( &ext, dp, dbip ) < 0 ||
		    db5_get_raw_internal_ptr( &raw, ext.ext_buf ) == NULL )  {
			bu_log("db_dirbuild(%s): improper database, unable to read %s object\n",
				dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME );
			return -1;
		}
		if( raw.major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY )  {
			bu_log("db_dirbuild(%s): improper database, %s exists but is not an attribute-only object\n",
				dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME );
			dbip->dbi_title = bu_strdup(DB5_GLOBAL_OBJECT_NAME);
			return 0;	/* not a fatal error, need to let user proceed to fix it */
		}
		if( db5_import_attributes( &avs, &raw.attributes ) < 0 )  {
			bu_log("db_dirbuild(%s): improper database, corrupted attribute-only %s object\n",
				dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME );
		    	bu_free_external(&ext);
			return -1;	/* this is fatal */
		}
		BU_CK_AVS( &avs );

		/* Parse out the attributes */
		if( (cp = bu_avs_get( &avs, "title" )) != NULL )  {
			dbip->dbi_title = bu_strdup( cp );
		} else {
			dbip->dbi_title = bu_strdup( "Untitled BRL-CAD database" );
		}
		if( (cp = bu_avs_get( &avs, "units" )) != NULL )  {
			double	dd;
			if( sscanf( cp, "%lf", &dd ) != 1 ||
			    NEAR_ZERO( dd, VUNITIZE_TOL ) )  {
			    	bu_log("db_dirbuild(%s): improper database, %s object attribute 'units'=%s is invalid\n",
					dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME,
				    	cp );
			    	/* Not fatal, just stick with default value from db_open() */
			} else {
				dbip->dbi_local2base = dd;
				dbip->dbi_base2local = 1/dd;
			}
		}
		if( (cp = bu_avs_get( &avs, "regionid_colortable")) != NULL )  {
			/* Import the region-id coloring table */
			db5_import_color_table( (char *)cp);
		}
		bu_avs_free( &avs );
		bu_free_external(&ext);	/* not until after done with avs! */
		return 0;
	}

	/* Make a very simple check for a v4 database */
	if( header[0] == 'I' )  {
		dbip->dbi_version = 4;
		if( db_scan( dbip, (int (*)())db_diradd, 1, NULL ) < 0 )  {
			dbip->dbi_version = 0;
		    	return -1;
		}
		return 0;		/* ok */
	}

	bu_log("db_dirbuild(%s) ERROR, file is not in BRL-CAD geometry database format\n",
		dbip->dbi_filename);
	return -1;
}

int
db_get_version(struct db_i *dbip)
{
	unsigned char	header[8];

	rewind(dbip->dbi_fp);
	if( fread( header, sizeof header, 1, dbip->dbi_fp ) != 1  )  {
		bu_log("db_get_version ERROR, file (%s) too short to be BRL-CAD database\n",
			dbip->dbi_filename);
		return -1;
	}

	if( db5_header_is_valid( header ) )
		return( 5 );
	else if( header[0] == 'I' )
		return( 4 );
	else
		return( -1 );
	
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
