/*
 *			D I R . H
 *
 * The in-core object directory
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
 *	This software is Copyright (C) 1985-2004 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

struct directory  {
	char		*d_namep;	/* pointer to name string */
	long		d_addr;		/* disk address in obj file */
	short		d_flags;	/* flags */
	short		d_len;		/* # of db granules used by obj */
	short		d_nref;		/* # times referenced by COMBs */
	struct directory *d_forw;	/* forward link */
};
#define DIR_NULL	((struct directory *) NULL)

#define DIR_SOLID	0x1		/* this name is a solid */
#define DIR_COMB	0x2		/* combination */
#define DIR_REGION	0x4		/* region */
#define DIR_BRANCH	0x8		/* branch name */

#define LOOKUP_QUIET	0
#define LOOKUP_NOISY	1

#ifndef NAMESIZE
#define NAMESIZE		16
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
