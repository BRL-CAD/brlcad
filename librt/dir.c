/*
 *			D I R . C
 *
 * Ray Tracing program, GED database directory manager.
 *
 *  Functions -
 *	rt_dirbuild	Read GED database, build directory
 *	rt_dir_lookup	Look up name in directory
 *	rt_dir_add	Add entry to directory
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSdir[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ D I R B U I L D
 *
 * This routine reads through the 3d object file and
 * builds a directory of the object names, to allow rapid
 * named access to objects.
 *
 *  Note that some multi-record database items include length fields.
 *  These length fields are not used here.
 *  Instead, the sizes of multi-record items are determined by
 *  reading ahead and computing the actual size.
 *  This prevents difficulties arising from external "adjustment" of
 *  the number of records without corresponding adjustment of the length fields.
 *
 * Returns -
 *	(struct rt_i *)	Success
 *	RTI_NULL	Fatal Error
 */
struct rt_i *
rt_dirbuild(filename, buf, len)
char *filename;
char *buf;
int len;
{
	register struct rt_i	*rtip;
	static union record	record;		/* Initial record, holds name */
	static union record	rec2;		/* additional record(s) */
	register long	addr;			/* start of current rec */
	register int	nrec;			/* # total records */
	register long	here;			/* intermediate positions */
	register int	j;

	/*
	 *  Allocate and initialize information for this
	 *  instance of an RT model.
	 */
	GETSTRUCT( rtip, rt_i );
	bzero( (char *)rtip, sizeof(struct rt_i) );
	if( (rtip->fp = fopen(filename, "r")) == NULL )  {
		perror(filename);
		goto bad;
	}
	rtip->needprep = 1;
	rtip->file = rt_strdup( filename );

	VSETALL( rtip->mdl_min,  INFINITY );
	VSETALL( rtip->mdl_max, -INFINITY );
	VSETALL( rtip->rti_inf_box.bn.bn_min, -0.1 );
	VSETALL( rtip->rti_inf_box.bn.bn_max,  0.1 );
	rtip->rti_inf_box.bn.bn_type = CUT_BOXNODE;

	buf[0] = '\0';

	/* In a portable way, read the header (even if not rewound) */
	rewind( rtip->fp );
	if( fread( (char *)&record, sizeof record, 1, rtip->fp ) != 1  ||
	    record.u_id != ID_IDENT )  {
		rt_log("\nWARNING:  File is lacking a proper MGED database header\n");
		rt_log("This database should be converted before further use.\n\n");
	}
	rewind( rtip->fp );

	here = addr = -1;
	while(1)  {
#ifdef DB_MEM
		addr++;		/* really, nrec;  ranges 0..n */
#else
		if( (addr = ftell(rtip->fp)) == EOF )
			rt_log("rt_dirbuild:  ftell() failure\n");
#endif DB_MEM
		if( fread( (char *)&record, sizeof record, 1, rtip->fp ) != 1
		    || feof(rtip->fp) )
			break;

		if(rt_g.debug&DEBUG_DB)rt_log("db x%x %c (0%o)\n",
			addr, record.u_id, record.u_id );

		switch( record.u_id )  {
		case ID_IDENT:
			if( strcmp( record.i.i_version, ID_VERSION) != 0 )  {
				rt_log("WARNING: File is Version %s, Program is version %s\n",
					record.i.i_version, ID_VERSION );
			}
			if( buf[0] == '\0' )
				strncpy( buf, record.i.i_title, len );
			continue;
		case ID_FREE:
			continue;
		case ID_ARS_A:
			nrec = 1;
			while(1) {
				here = ftell( rtip->fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, rtip->fp ) != 1 )
					break;
				if( rec2.u_id != ID_ARS_B )  {
					fseek( rtip->fp, here, 0 );
					break;
				}
				nrec++;
			}
			rt_dir_add( rtip, record.a.a_name, addr, nrec,
				DIR_SOLID );
			continue;
		case ID_ARS_B:
			rt_log("ERROR: Unattached ARS 'B' record\n");
			continue;
		case ID_SOLID:
			rt_dir_add( rtip, record.s.s_name, addr, 1,
				DIR_SOLID );
			continue;
		case ID_MATERIAL:
			rt_color_addrec( &record, addr );
			continue;
		case ID_P_HEAD:
			nrec = 1;
			while(1) {
				here = ftell( rtip->fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, rtip->fp ) != 1 )
					break;
				if( rec2.u_id != ID_P_DATA )  {
					fseek( rtip->fp, here, 0 );
					break;
				}
				nrec++;
			}
			rt_dir_add( rtip, record.p.p_name, addr, nrec,
				DIR_SOLID );
			continue;
		case ID_P_DATA:
			rt_log("ERROR: Unattached P_DATA record\n");
			continue;
		case ID_BSOLID:
			nrec = 1;
			while(1) {
				/* Find and skip subsequent BSURFs */
				here = ftell( rtip->fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, rtip->fp ) != 1 )
					break;
				if( rec2.u_id != ID_BSURF )  {
					fseek( rtip->fp, here, 0 );
					break;
				}

				/* Just skip over knots and control mesh */
				j = (rec2.d.d_nknots + rec2.d.d_nctls);
				while( j-- > 0 )
					fread( (char *)&rec2, sizeof(rec2), 1, rtip->fp );
				nrec += j+1;
			}
			rt_dir_add( rtip, record.B.B_name, addr, nrec,
				DIR_SOLID );
			continue;
		case ID_BSURF:
			rt_log("ERROR: Unattached B-spline surface record\n");

			/* Just skip over knots and control mesh */
			j = (record.d.d_nknots + record.d.d_nctls);
			while( j-- > 0 )
				fread( (char *)&rec2, sizeof(rec2), 1, rtip->fp );
			continue;
		case ID_MEMB:
			rt_log("ERROR: Unattached combination MEMBER record\n");
			continue;
		case ID_COMB:
			nrec = 1;
			while(1) {
				here = ftell( rtip->fp );
				if( fread( (char *)&rec2, sizeof(rec2),
				    1, rtip->fp ) != 1 )
					break;
				if( rec2.u_id != ID_MEMB )  {
					fseek( rtip->fp, here, 0 );
					break;
				}
				nrec++;
			}
			rt_dir_add( rtip, record.c.c_name, addr, nrec,
				record.c.c_flags == 'R' ?
					DIR_COMB|DIR_REGION : DIR_COMB );
			continue;
		default:
			rt_log("rt_dirbuild:  unknown record %c (0%o), addr=x%x\n",
				record.u_id, record.u_id, addr );
			/* skip this record */
			continue;
		}
	}
	rewind( rtip->fp );

#ifdef DB_MEM
	/*
	 * Obtain in-core copy of database, rather than doing lots of
	 * random-access reads.  Here, "addr" is really "nrecords".
	 */
	if( (rtip->rti_db = (union record *)rt_malloc(
	    addr*sizeof(union record), "in-core database"))
	    == (union record *)0 )
	    	rt_bomb("in-core database malloc failure");
	rewind(rtip->fp);
	if( fread( (char *)rtip->rti_db, sizeof(union record), addr,
	    rtip->fp) != addr )  {
	    	rt_log("rt_dirbuild:  problem reading db on 2nd pass\n");
	    	goto bad;
	}
#endif DB_MEM

	rtip->rti_magic = RTI_MAGIC;
	return( rtip );			/* OK */
bad:
	rt_free( (char *)rtip, "rt_i");
    	return( RTI_NULL );		/* FAIL */
}

/*
 *			R T _ D I R _ L O O K U P
 *
 * This routine takes a name, and looks it up in the
 * directory table.  If the name is present, a pointer to
 * the directory struct element is returned, otherwise
 * NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only
 * the return code indicates failure.
 */
struct directory *
rt_dir_lookup( rtip, str, noisy )
struct rt_i	*rtip;
register char	*str;
{
	register struct directory *dp;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_dir_lookup:  bad rtip\n");

	for( dp = rtip->rti_DirHead; dp != DIR_NULL; dp=dp->d_forw )  {
		if(
			str[0] == dp->d_namep[0]  &&	/* speed */
			str[1] == dp->d_namep[1]  &&	/* speed */
			strcmp( str, dp->d_namep ) == 0
		)
			return(dp);
	}

	if( noisy )
		rt_log("rt_dir_lookup:  could not find '%s'\n", str );
	return( DIR_NULL );
}

/*
 *			R T _ D I R _ A D D
 *
 * Add an entry to the directory
 */
struct directory *
rt_dir_add( rtip, name, laddr, len, flags )
struct rt_i	*rtip;
register char	*name;
long		laddr;
int		len;
int		flags;
{
	register struct directory *dp;
	char local[NAMESIZE+2];

	(void)strncpy( local, name, NAMESIZE );	/* Trim the name */
	local[NAMESIZE] = '\0';			/* Ensure null termination */

	if(rt_g.debug&DEBUG_DB)
		rt_log("rt_dir_add(x%x,%s,x%x,%d.,x%x)\n", rtip, local, laddr, len, flags);

	GETSTRUCT( dp, directory );
	dp->d_namep = rt_strdup( local );
	dp->d_addr = laddr;
	dp->d_flags = flags;
	dp->d_len = len;
	dp->d_forw = rtip->rti_DirHead;
	rtip->rti_DirHead = dp;
	return( dp );
}

/*
 *			R T _ P R _ D I R
 */
void
rt_pr_dir( rtip )
register struct rt_i *rtip;
{
	register struct directory *dp;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_pr_dir:  bad rtip\n");

	rt_log("Dump of directory for %s, rtip=x%x\n", rtip->file, rtip);
	for( dp = rtip->rti_DirHead; dp != DIR_NULL; dp=dp->d_forw )  {
		rt_log("%.8x disk=%.8x anim=%.8x %s\n", dp, dp->d_addr,
			dp->d_animate, dp->d_namep);
	}
}
