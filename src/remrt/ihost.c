/*                         I H O S T . C
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
/** @file ihost.c
 *
 *  Internal host table routines.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "rtlist.h"
#include "raytrace.h"

#include "./ihost.h"


struct bu_list	HostHead;

/*
 *			G E T _ O U R _ H O S T N A M E
 *
 * There is a problem in some hosts that gethostname() will only
 * return the host name and *not* the fully qualified host name
 * with domain name.
 *
 * gethostbyname() will return a host table (nameserver) entry
 * where h_name is the "offical name", i.e. fully qualified.
 * Therefore the following piece of code.
 */
char *
get_our_hostname(void)
{
	char temp[512];
	struct hostent *hp;

	/* Init list head here */
	BU_LIST_INIT( &HostHead );

	gethostname(temp, sizeof(temp));

	hp = gethostbyname(temp);

	return bu_strdup(hp->h_name);
}

/*
 *			H O S T _ L O O K U P _ B Y _ H O S T E N T
 *
 *  We have a hostent structure, of which, the only thing of interest is
 *  the host name.  Go from name to address back to name, to get formal name.
 *
 *  Used by host_lookup_by_addr, too.
 */
struct ihost *
host_lookup_by_hostent(struct hostent *addr, int enter)
{
	register struct ihost	*ihp;
	struct hostent	*addr2;
	struct hostent	*addr3;

	addr2 = gethostbyname(addr->h_name);
	if( addr != addr2 )  {
		bu_log("host_lookup_by_hostent(%s) got %s?\n",
			addr->h_name, addr2 ? addr2->h_name : "NULL" );
		return IHOST_NULL;
	}
	addr3 = gethostbyaddr(addr2->h_addr_list[0],
	    sizeof(struct in_addr), addr2->h_addrtype);
	if( addr != addr3 )  {
		bu_log("host_lookup_by_hostent(%s) got %s?\n",
			addr->h_name, addr3 ? addr3->h_name : "NULL" );
		return IHOST_NULL;
	}
	/* Now addr->h_name points to the "formal" name of the host */

	/* Search list for existing instance */
	for( BU_LIST_FOR( ihp, ihost, &HostHead ) )  {
		CK_IHOST(ihp);

		if( strcmp( ihp->ht_name, addr->h_name ) != 0 )
			continue;
		return( ihp );
	}
	if( enter == 0 )
		return( IHOST_NULL );

	/* If not found and enter==1, enter in host table w/defaults */
	/* Note: gethostbyxxx() routines keep stuff in a static buffer */
	return( make_default_host( addr->h_name ) );
}

/*
 *			M A K E _ D E F A U L T _ H O S T
 *
 *  Add a new host entry to the list of known hosts, with
 *  default parameters.
 *  This routine is used to handle unexpected volunteers.
 */
struct ihost *
make_default_host(char *name)
{
	register struct ihost	*ihp;

	BU_GETSTRUCT( ihp, ihost );
	ihp->l.magic = IHOST_MAGIC;

	/* Make private copy of host name -- callers have static buffers */
	ihp->ht_name = bu_strdup( name );

	/* Default host parameters */
	ihp->ht_flags = 0x0;
	ihp->ht_when = HT_PASSIVE;
	ihp->ht_where = HT_CONVERT;
	ihp->ht_path = "/tmp";

	/* Add to linked list of known hosts */
	BU_LIST_INSERT( &HostHead, &ihp->l );

	return(ihp);
}

/*
 *			H O S T _ L O O K U P _ B Y _ A D D R
 */
struct ihost *
host_lookup_by_addr(struct sockaddr_in *from, int enter)
{
	register struct ihost	*ihp;
	struct hostent	*addr;
	unsigned long	addr_tmp;
	char		name[64];

	addr_tmp = from->sin_addr.s_addr;
	addr = gethostbyaddr( (char *)&from->sin_addr, sizeof (struct in_addr),
		from->sin_family);
	if( addr != NULL )  {
		ihp = host_lookup_by_hostent( addr, enter );
		if( ihp )  return ihp;
	}

	/* Host name is not known */
	addr_tmp = ntohl(addr_tmp);
	sprintf( name, "%ld.%ld.%ld.%ld",
		(addr_tmp>>24) & 0xff,
		(addr_tmp>>16) & 0xff,
		(addr_tmp>> 8) & 0xff,
		(addr_tmp    ) & 0xff );
	if( enter == 0 )  {
		bu_log("%s: unknown host\n", name);
		return( IHOST_NULL );
	}

	/* See if this host has been previously entered by number */
	for( BU_LIST_FOR( ihp, ihost, &HostHead ) )  {
		CK_IHOST(ihp);
		if( strcmp( ihp->ht_name, name ) == 0 )
			return( ihp );
	}

	/* Create a new hostent structure */
	return( make_default_host( name ) );
}

/*
 *			H O S T _ L O O K U P _ B Y _ N A M E
 */
struct ihost *
host_lookup_by_name(char *name, int enter)
{
	struct sockaddr_in	sockhim;
	struct hostent		*addr;

	/* Determine name to be found */
	if( isdigit( *name ) )  {
		/* Numeric */
		sockhim.sin_family = AF_INET;
		sockhim.sin_addr.s_addr = inet_addr(name);
		return( host_lookup_by_addr( &sockhim, enter ) );
	} else {
		addr = gethostbyname(name);
	}
	if( addr == NULL )  {
		bu_log("%s:  bad host\n", name);
		return( IHOST_NULL );
	}
	return( host_lookup_by_hostent( addr, enter ) );
}

/*
 *			H O S T _ L O O K U P _ O F _ F D
 */
struct ihost *
host_lookup_of_fd(int fd)
{
	auto socklen_t	fromlen;
	struct sockaddr_in from;

	fromlen = sizeof (from);
	if (getpeername(fd, (struct sockaddr *)&from, &fromlen) < 0) {
		perror("getpeername");
		return IHOST_NULL;
	}

	return host_lookup_by_addr( &from, 1 );
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
