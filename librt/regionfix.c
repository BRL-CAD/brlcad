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

#include <stdio.h>
#include <ctype.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./material.h"

#include "rdebug.h"

/*
 *			R E G I O N F I X
 *
 *  Apply any deltas to reg_regionid values
 *  to allow old applications that use the reg_regionid number
 *  to distinguish between different instances of the same
 *  prototype region.
 */
void
regionfix( ap, file )
struct application	*ap;
char			*file;
{
	FILE	*fp;
	char	*line;
	char	*tabp;
	int	linenum = 0;
	char	*err;
	register struct region	*rp;
	int	ret;
	int	oldid;
	int	newid;

	if( (fp = fopen( file, "r" )) == NULL )	 {
		perror(file);
		return;
	}

	while( (line = rt_read_cmd( fp )) != (char *) 0 )  {
		linenum++;
		/*  For now, establish a simple format:
		 *  regexp TAB formula SEMICOLON
		 */
		if( (tabp = strchr( line, '\t' )) == (char *)0 )  {
			rt_log("%s: missing TAB on line %d:\n%s\n", file, linenum, line );
			continue;		/* just ignore it */
		}
		*tabp++ = '\0';
		while( *tabp && isspace( *tabp ) )  tabp++;
		if( (err = re_comp(line)) != (char *)0 )  {
			rt_log("%s: line %d, re_comp error '%s'\n", file, line, err );
			continue;		/* just ignore it */
		}
		
		rp = ap->a_rt_i->HeadRegion;
		for( ; rp != REGION_NULL; rp = rp->reg_forw )  {
			ret = re_exec((char *)rp->reg_name);
			if(rdebug&RDEBUG_INSTANCE)  {
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
			if(rdebug&RDEBUG_INSTANCE)  {
				rt_log("%s instance %d:  region id changed from %d to %d\n",
					rp->reg_name, rp->reg_instnum,
					oldid, newid );
			}
			rp->reg_regionid = newid;
		}
		rt_free( line, "reg_expr line");
	}
	fclose( fp );
}
