/*
 *			D I R . C
 *
 * Ray Tracing program, GED database directory manager.
 *
 *  Functions -
 *	rt_dirbuild	Read GED database, build directory
 *	rt_dir_lookup	Look up name in directory
 *	rt_dir_add		Add entry to directory
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
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/db.h"
#include "../h/raytrace.h"
#include "debug.h"

/*
 *			R T _ D I R B U I L D
 *
 * This routine reads through the 3d object file and
 * builds a directory of the object names, to allow rapid
 * named access to objects.
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
	static union record	record;
	register long	addr;

	/*
	 *  Allocate and initialize information for this
	 *  instance of an RT model.
	 *
	 *  (Here we will allocate an rt_i struct, someday)
	 */
	bzero( (char *)&rt_i, sizeof(rt_i) );
	if( (rt_i.fp = fopen(filename, "r")) == NULL )  {
		perror(filename);
		return(RTI_NULL);
	}
	rt_i.needprep = 1;
	rt_i.file = rt_strdup( filename );

	/* In case everything is a halfspace, set a minimum space */
	VSETALL( rt_i.mdl_min, -10 );
	VSETALL( rt_i.mdl_max,  10 );
	VMOVE( rt_i.rti_inf_box.bn.bn_min, rt_i.mdl_min );
	VMOVE( rt_i.rti_inf_box.bn.bn_max, rt_i.mdl_max );
	rt_i.rti_inf_box.bn.bn_type = CUT_BOXNODE;

	buf[0] = '\0';
	addr = ftell(rt_i.fp);
	(void)fread( (char *)&record, sizeof record, 1, rt_i.fp );
	if( record.u_id != ID_IDENT )  {
		rt_log("WARNING:  File is not a proper GED database\n");
		rt_log("This database should be converted before further use.\n");
	}
	(void)fseek( rt_i.fp, addr, 0 );
	while(1)  {
		addr = ftell(rt_i.fp);
		if( fread( (char *)&record, sizeof record, 1, rt_i.fp ) != 1
		    || feof(rt_i.fp) )
			break;

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
			rt_dir_add( record.a.a_name, addr );
			continue;
		case ID_ARS_B:
			continue;
		case ID_SOLID:
			rt_dir_add( record.s.s_name, addr );
			continue;
		case ID_MATERIAL:
			rt_color_addrec( &record, addr );
			continue;
		case ID_P_HEAD:
			{
				union record rec;
				register int nrec;

				nrec = 1;
				while(1) {
					register int here;
					here = ftell( rt_i.fp );
					if( fread( (char *)&rec, sizeof(rec), 1,
					    rt_i.fp ) != 1 )
						break;
					if( rec.u_id != ID_P_DATA )  {
						fseek( rt_i.fp, here, 0 );
						break;
					}
					nrec++;
				}
				rt_dir_add( record.p.p_name, addr );
				continue;
			}
		case ID_BSOLID:
			rt_dir_add( record.B.B_name, addr );
			continue;
		case ID_BSURF:
			{
				register int j;
				/* Just skip over knots and control mesh */
				j = (record.d.d_nknots + record.d.d_nctls) *
					sizeof(union record);
				/****** NON-PORTABLE ******/
				fseek( rt_i.fp, j, 1 );
				rt_dir_add( record.p.p_name, addr );
				continue;
			}
		case ID_MEMB:
			continue;
		case ID_COMB:
			rt_dir_add( record.c.c_name, addr );
			continue;
		default:
			rt_log("rt_dirbuild:  unknown record %c (0%o), addr=x%x\n",
				record.u_id, record.u_id, addr );
			/* skip this record */
			continue;
		}
	}
	/* Eventually, we will malloc() this on a per-db basis */
	return( &rt_i );	/* OK */
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
rt_dir_lookup( str, noisy )
register char *str;
{
	register struct directory *dp;

	for( dp = rt_i.rti_DirHead; dp != DIR_NULL; dp=dp->d_forw )  {
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
rt_dir_add( name, laddr )
register char *name;
long laddr;
{
	register struct directory *dp;

	GETSTRUCT( dp, directory );
	dp->d_namep = rt_strdup( name );
	dp->d_addr = laddr;
	dp->d_forw = rt_i.rti_DirHead;
	rt_i.rti_DirHead = dp;
	return( dp );
}
