/*
 *			R E G I O N F I X
 *
 *  Subroutines for adjusting old GIFT-style region-IDs,
 *  to take into account the presence of instancing.
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
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSregionfix[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#if USE_REGCOMP
#include <regex.h>
#endif
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "rtstring.h"
#include "raytrace.h"

#include "./debug.h"

/*
 *			R T_ R E G I O N F I X
 *
 *  Apply any deltas to reg_regionid values
 *  to allow old applications that use the reg_regionid number
 *  to distinguish between different instances of the same
 *  prototype region.
 *
 *  Called once, from rt_prep(), before raytracing begins.
 */
void
rt_regionfix( rtip )
struct rt_i	*rtip;
{
	FILE	*fp;
	char	*file;
	char	*line;
	char	*tabp;
	int	linenum = 0;
	char	*err;
	register struct region	*rp;
	int	ret;
	int	oldid;
	int	newid;
	struct rt_vls	name;

	RT_CK_RTI(rtip);

	/*  If application has provided an alternative file name
	 *  before rt_prep() was called, then use that.
	 *  Otherwise, replace ".g" suffix on database name
	 *  with ".regexp".
	 */
	rt_vls_init(&name);
	file = rtip->rti_region_fix_file;
	if( file == (char *)NULL )  {
		rt_vls_strcpy( &name, rtip->rti_dbip->dbi_filename );
		if( (tabp = strrchr( rt_vls_addr(&name), '.' )) != NULL )  {
			/* Chop off "." and suffix */
			rt_vls_trunc( &name, tabp-rt_vls_addr(&name) );
		}
		rt_vls_strcat( &name, ".regexp" );
		file = rt_vls_addr(&name);
	}

	if( (fp = fopen( file, "r" )) == NULL )	 {
		if( rtip->rti_region_fix_file ) perror(file);
		rt_vls_free(&name);
		return;
	}
	rt_log("librt/rt_regionfix(%s):  Modifying instanced region-ids.\n", file);

	while( (line = rt_read_cmd( fp )) != (char *) 0 )  {
#if USE_REGCOMP
		regex_t	re_space;
#endif
		linenum++;
		/*  For now, establish a simple format:
		 *  regexp TAB [more_white_space] formula SEMICOLON
		 */
		if( (tabp = strchr( line, '\t' )) == (char *)0 )  {
			rt_log("%s: missing TAB on line %d:\n%s\n", file, linenum, line );
			continue;		/* just ignore it */
		}
		*tabp++ = '\0';
		while( *tabp && isspace( *tabp ) )  tabp++;
#if USE_REGCOMP
		if( (ret = regcomp(&re_space,line,0)) != 0 )  {
			rt_log("%s: line %d, regcomp error '%d'\n", file, line, ret );
			continue;		/* just ignore it */
		}
#else
		if( (err = re_comp(line)) != (char *)0 )  {
			rt_log("%s: line %d, re_comp error '%s'\n", file, line, err );
			continue;		/* just ignore it */
		}
#endif
		
		rp = rtip->HeadRegion;
		for( ; rp != REGION_NULL; rp = rp->reg_forw )  {
#if USE_REGCOMP
			ret = regexec(&re_space, (char *)rp->reg_name, 0, 0,0);
#else				      
			ret = re_exec((char *)rp->reg_name);
#endif
			if(rt_g.debug&DEBUG_INSTANCE)  {
				rt_log("'%s' %s '%s'\n", line,
					ret==1 ? "==" : "!=",
					rp->reg_name);
			}
			if( (ret) == 0  )
				continue;	/* didn't match */
			if( ret == -1 )  {
				rt_log("%s: line %d, invalid regular expression\n", file, linenum);
				break;		/* on to next RE */
			}
			/*
			 *  RE matched this name, perform indicated operation
			 *  For now, choices are limited.  Later this might
			 *  become an interpreted expression.  For now:
			 *	99	replace old region id with "num"
			 *	+99	increment old region id with "num"
			 *		(which may itself be a negative number)
			 *	+uses	increment old region id by the
			 *		current instance (use) count.
			 */
			oldid = rp->reg_regionid;
			if( strcmp( tabp, "+uses" ) == 0  )  {
				newid = oldid + rp->reg_instnum;
			} else if( *tabp == '+' )  {
				newid = oldid + atoi( tabp+1 );
			} else {
				newid = atoi( tabp );
				if( newid == 0 )  rt_log("%s, line %d Warning:  new id = 0\n", file, linenum );
			}
			if(rt_g.debug&DEBUG_INSTANCE)  {
				rt_log("%s instance %d:  region id changed from %d to %d\n",
					rp->reg_name, rp->reg_instnum,
					oldid, newid );
			}
			rp->reg_regionid = newid;
		}
#if USE_REGCOMP
		regfree(&re_space);
#endif
		rt_free( line, "reg_expr line");
	}
	fclose( fp );
	rt_vls_free(&name);
}
