/*
 *			V L I S T . C
 *
 *  Routines for the import and export of vlist chains as:
 *	Network independent binary,
 *	BRL-extended UNIX-Plot files.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1992 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "externs.h"

/*
 *			R T _ V L I S T _ E X P O R T
 *
 *  Convert a vlist chain into a blob of network-independent binary,
 *  for transmission to another machine.
 *  The result is stored in a vls string, so that both the address
 *  and length are available conveniently.
 */
void
rt_vlist_export( vls, hp, name )
struct rt_vls	*vls;
struct rt_list	*hp;
CONST char	*name;
{
	register struct rt_vlist	*vp;
	int		nelem;
	int		namelen;
	int		nbytes;
	unsigned char	*buf;
	unsigned char	*bp;

	RT_VLS_CHECK(vls);

	/* Count number of element in the vlist */
	nelem = 0;
	for( RT_LIST_FOR( vp, rt_vlist, hp ) )  {
		nelem += vp->nused;
	}

	/* Build output buffer for binary transmission
	 * nelem[4], String[n+1], cmds[nelem*1], pts[3*nelem*8]
	 */
	namelen = strlen(name)+1;
	nbytes = namelen + 4 + nelem * (1+3*8) + 2;

	rt_vls_setlen( vls, nbytes );
	buf = (unsigned char *)rt_vls_addr(vls);
	bp = rt_plong( buf, nelem );
	strncpy( bp, name, namelen );
	bp += namelen;

	/* Output cmds, as bytes */
	for( RT_LIST_FOR( vp, rt_vlist, hp ) )  {
		register int	i;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		for( i = 0; i < nused; i++ )  {
			*bp++ = *cmd++;
		}
	}

	/* Output points, as three 8-byte doubles */
	for( RT_LIST_FOR( vp, rt_vlist, hp ) )  {
		register int	i;
		register int	nused = vp->nused;
		register point_t *pt = vp->pt;
		for( i = 0; i < nused; i++,pt++ )  {
			htond( bp, (char *)pt, 3 );
			bp += 3*8;
		}
	}
}

/*
 *			R T _ V L I S T _ I M P O R T
 *
 *  Convert a blob of network-independent binary prepared by vls_vlist()
 *  and received from another machine, into a vlist chain.
 */
void
rt_vlist_import( hp, namevls, buf )
struct rt_list	*hp;
struct rt_vls	*namevls;
CONST unsigned char	*buf;
{
	register struct rt_vlist	*vp;
	CONST register unsigned char	*bp;
	CONST unsigned char		*pp;		/* point pointer */
	int		nelem;
	int		namelen;
	int		i;
	point_t		point;

	RT_VLS_CHECK(namevls);

	nelem = rt_glong( buf );
	bp = buf+4;

	namelen = strlen(bp)+1;
	rt_vls_strncpy( namevls, (char *)bp, namelen );
	bp += namelen;

	pp = bp + nelem*1;

	for( i=0; i < nelem; i++ )  {
		int	cmd;

		cmd = *bp++;
		ntohd( (char *)point, pp, 3 );
		pp += 3*8;
		/* This macro might be expanded inline, for performance */
		RT_ADD_VLIST( hp, point, cmd );
	}
}

/*
 *			R T _ V L I S T _ C L E A N U P
 *
 *  The macro RT_FREE_VLIST() simply appends to the list &rt_g.rtg_vlfree.
 *  Now, give those structures back to rt_free().
 */
void
rt_vlist_cleanup()
{
	register struct rt_vlist	*vp;

	while( RT_LIST_WHILE( vp, rt_vlist, &rt_g.rtg_vlfree ) )  {
		RT_CK_VLIST( vp );
		RT_LIST_DEQUEUE( &(vp->l) );
		rt_free( (char *)vp, "rt_vlist" );
	}
}

/*
 *			R T _ V L I S T _ T O _ U P L O T
 *
 *  Output a vlist as an extended 3-D floating point UNIX-Plot file.
 *  You provide the file.
 */
void
rt_vlist_to_uplot( fp, vhead )
FILE		*fp;
struct rt_list	*vhead;
{
	register struct rt_vlist	*vp;

	for( RT_LIST_FOR( vp, rt_vlist, vhead ) )  {
		register int		i;
		register int		nused = vp->nused;
		register CONST int	*cmd = vp->cmd;
		register point_t	 *pt = vp->pt;

		for( i = 0; i < nused; i++,cmd++,pt++ )  {
			switch( *cmd )  {
			case RT_VLIST_POLY_START:
				break;
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_LINE_MOVE:
				pdv_3move( fp, *pt );
				break;
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
			case RT_VLIST_LINE_DRAW:
				pdv_3cont( fp, *pt );
				break;
			default:
				rt_log("rt_vlist_to_uplot: unknown vlist cmd x%x\n",
					*cmd );
			}
		}
	}
}
