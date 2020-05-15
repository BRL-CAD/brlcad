/*                         I H O S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file ihost.c
 *
 * Internal host table routines.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#include "bnetwork.h"
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"

#include "./ihost.h"

#if defined(HAVE_GETHOSTNAME) && !defined(HAVE_DECL_GETHOSTNAME)
extern int gethostname(char *name, size_t len);
#endif

#if defined(HAVE_GETHOSTBYNAME) && !defined(HAVE_DECL_GETHOSTBYNAME) && !defined(_WINSOCKAPI_)
extern struct hostent *gethostbyname(const char *);
#endif

#if defined(HAVE_GETHOSTBYADDR) && !defined(HAVE_DECL_GETHOSTBYADDR) && !defined(_WINSOCKAPI_)
extern struct hostent *gethostbyaddr(const void *, socklen_t, int);
#endif

struct bu_list	HostHead;

/*
 * There is a problem in some hosts that gethostname() will only
 * return the host name and *not* the fully qualified host name
 * with domain name.
 *
 * gethostbyname() will return a host table (nameserver) entry
 * where h_name is the "official name", i.e. fully qualified.
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
 *  We have a hostent structure, of which, the only thing of interest is
 *  the host name.  Go from name to address back to name, to get formal name.
 *
 *  Used by host_lookup_by_addr, too.
 */
struct ihost *
host_lookup_by_hostent(const struct hostent * addr, int enter)
{
    struct ihost	*ihp;
    const struct hostent *	addr2;
    const struct hostent *	addr3;

    addr2 = gethostbyname(addr->h_name);
    if ( addr != addr2 )  {
	bu_log("host_lookup_by_hostent(%s) got %s?\n",
	       addr->h_name, addr2 ? addr2->h_name : "NULL" );
	return IHOST_NULL;
    }
    addr3 = gethostbyaddr(addr2->h_addr_list[0],
			  sizeof(struct in_addr), addr2->h_addrtype);
    if ( addr != addr3 )  {
	bu_log("host_lookup_by_hostent(%s) got %s?\n",
	       addr->h_name, addr3 ? addr3->h_name : "NULL" );
	return IHOST_NULL;
    }
    /* Now addr->h_name points to the "formal" name of the host */

    /* Search list for existing instance */
    for ( BU_LIST_FOR( ihp, ihost, &HostHead ) )  {
	CK_IHOST(ihp);

	if ( !BU_STR_EQUAL( ihp->ht_name, addr->h_name ) )
	    continue;
	return ihp;
    }
    if ( enter == 0 )
	return IHOST_NULL;

    /* If not found and enter==1, enter in host table w/defaults */
    /* Note: gethostbyxxx() routines keep stuff in a static buffer */
    return make_default_host( addr->h_name );
}

/*
 *  Add a new host entry to the list of known hosts, with
 *  default parameters.
 *  This routine is used to handle unexpected volunteers.
 */
struct ihost *
make_default_host(const char* name)
{
    struct ihost	*ihp;

    BU_ALLOC(ihp, struct ihost);
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

    return ihp;
}

struct ihost *
host_lookup_by_addr(const struct sockaddr_in * from, int enter)
{
    struct ihost	*ihp;
    struct hostent	*addr;
    unsigned long	addr_tmp;
    char		name[64];

    addr_tmp = from->sin_addr.s_addr;
    addr = gethostbyaddr( (char *)&from->sin_addr, sizeof (struct in_addr),
			  from->sin_family);
    if ( addr != NULL )  {
	ihp = host_lookup_by_hostent( addr, enter );
	if ( ihp )  return ihp;
    }

    /* Host name is not known */
    addr_tmp = ntohl(addr_tmp);
    sprintf( name, "%ld.%ld.%ld.%ld",
	     (addr_tmp>>24) & 0xff,
	     (addr_tmp>>16) & 0xff,
	     (addr_tmp>> 8) & 0xff,
	     (addr_tmp    ) & 0xff );
    if ( enter == 0 )  {
	bu_log("%s: unknown host\n", name);
	return IHOST_NULL;
    }

    /* See if this host has been previously entered by number */
    for ( BU_LIST_FOR( ihp, ihost, &HostHead ) )  {
	CK_IHOST(ihp);
	if ( BU_STR_EQUAL( ihp->ht_name, name ) )
	    return ihp;
    }

    /* Create a new hostent structure */
    return make_default_host( name );
}

struct ihost *
host_lookup_by_name(const char* name, int enter)
{
    struct sockaddr_in	sockhim;
    struct hostent		*addr;

    /* Determine name to be found */
    if ( isdigit( (int)*name ) )  {
	/* Numeric */
	sockhim.sin_family = AF_INET;
	sockhim.sin_addr.s_addr = inet_addr(name);
	return host_lookup_by_addr( &sockhim, enter );
    } else {
	addr = gethostbyname(name);
    }
    if ( addr == NULL )  {
	bu_log("%s:  bad host\n", name);
	return IHOST_NULL;
    }
    return host_lookup_by_hostent( addr, enter );
}

struct ihost *
host_lookup_of_fd(int fd)
{
    socklen_t	fromlen;
    struct sockaddr_in from;

    fromlen = sizeof(from);
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
