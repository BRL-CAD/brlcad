/*                         I H O S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
 * Uses getaddrinfo()/getnameinfo() (POSIX.1-2001, available on all
 * modern platforms including Windows Vista+) instead of the deprecated
 * gethostbyname()/gethostbyaddr() functions.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
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
#include "bnetwork.h"
#include "bio.h"

#include "vmath.h"
#include "bu/interrupt.h"
#include "raytrace.h"

#include "./ihost.h"

#ifndef NI_MAXHOST
#  define NI_MAXHOST 1025
#endif

#if defined(HAVE_GETHOSTNAME) && !defined(HAVE_DECL_GETHOSTNAME)
extern int gethostname(char *name, size_t len);
#endif

struct bu_list	HostHead;


/*
 * Add a new host entry to the list of known hosts with default
 * parameters.  Used to handle unexpected volunteers.
 */
struct ihost *
make_default_host(const char *name)
{
    struct ihost *ihp;

    BU_ALLOC(ihp, struct ihost);
    ihp->l.magic = IHOST_MAGIC;

    /* Make private copy of host name */
    ihp->ht_name = bu_strdup(name);

    /* Default host parameters */
    ihp->ht_flags = 0x0;
    ihp->ht_when = HT_PASSIVE;
    ihp->ht_where = HT_CONVERT;
    ihp->ht_path = "/tmp";

    /* Add to linked list of known hosts */
    BU_LIST_INSERT(&HostHead, &ihp->l);

    return ihp;
}


/*
 * Return the fully-qualified canonical hostname of the local machine.
 * Uses getaddrinfo() with AI_CANONNAME for portability.
 * Initialises HostHead as a side-effect (called once at startup).
 */
char *
get_our_hostname(void)
{
    char temp[512] = {0};
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char *result;

    /* Init list head here */
    BU_LIST_INIT(&HostHead);

    gethostname(temp, sizeof(temp) - 1);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_flags    = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(temp, NULL, &hints, &res) == 0 && res != NULL) {
	result = bu_strdup(res->ai_canonname ? res->ai_canonname : temp);
	freeaddrinfo(res);
    } else {
	result = bu_strdup(temp);
    }
    return result;
}


/*
 * Resolve a canonical hostname from a connected socket's peer address.
 * Falls back to numeric dotted-quad / colon-hex notation if name
 * resolution fails.
 *
 * Replaces the old gethostbyaddr()-based approach.
 *
 * Loopback special-case: on many CI / container hosts getnameinfo() on
 * 127.0.0.1 returns the machine's FQDN or "127.0.0.1" rather than
 * "localhost".  If the peer is in 127.0.0.0/8 and the resolved name is
 * not already in HostHead, we also check for a registered "localhost"
 * entry so that the HT_CD path configured in .remrtrc is honoured
 * regardless of the local resolver behaviour.
 */
struct ihost *
host_lookup_by_addr(const struct sockaddr_in *from, int enter)
{
    struct ihost *ihp;
    char name[NI_MAXHOST];
    int ret;
    int is_loopback = 0;

    /* Detect 127.0.0.0/8 loopback addresses */
    if (from->sin_family == AF_INET) {
	unsigned long a = ntohl(from->sin_addr.s_addr);
	is_loopback = ((a >> 24) == 127);
    }

    /* Try to get a human-readable hostname */
    ret = getnameinfo((const struct sockaddr *)from,
		      (socklen_t)sizeof(*from),
		      name, (socklen_t)sizeof(name),
		      NULL, 0,
		      0 /* NI_NAMEREQD would fail fast on unresolvable addrs */);
    if (ret == 0) {
	/* Check whether this resolved name is already in the table */
	for (BU_LIST_FOR(ihp, ihost, &HostHead)) {
	    CK_IHOST(ihp);
	    if (BU_STR_EQUAL(ihp->ht_name, name))
		return ihp;
	}
	/* For loopback peers, also accept a registered "localhost" entry
	 * when the resolver returned a different (but equivalent) name. */
	if (is_loopback) {
	    for (BU_LIST_FOR(ihp, ihost, &HostHead)) {
		CK_IHOST(ihp);
		if (BU_STR_EQUAL(ihp->ht_name, "localhost"))
		    return ihp;
	    }
	}
	if (enter)
	    return make_default_host(name);
	bu_log("%s: unknown host\n", name);
	return IHOST_NULL;
    }

    /* Fall back to numeric form */
    ret = getnameinfo((const struct sockaddr *)from,
		      (socklen_t)sizeof(*from),
		      name, (socklen_t)sizeof(name),
		      NULL, 0,
		      NI_NUMERICHOST);
    if (ret != 0) {
	/* Last resort — manual dotted-quad from raw IPv4 address */
	unsigned long addr_tmp = ntohl(from->sin_addr.s_addr);
	snprintf(name, sizeof(name), "%lu.%lu.%lu.%lu",
		 (addr_tmp >> 24) & 0xff, (addr_tmp >> 16) & 0xff,
		 (addr_tmp >>  8) & 0xff, (addr_tmp      ) & 0xff);
    }

    if (enter == 0) {
	bu_log("%s: unknown host\n", name);
	return IHOST_NULL;
    }

    /* Check whether this numeric address was previously entered */
    for (BU_LIST_FOR(ihp, ihost, &HostHead)) {
	CK_IHOST(ihp);
	if (BU_STR_EQUAL(ihp->ht_name, name))
	    return ihp;
    }

    /* For loopback numeric addresses (127.x.x.x), also accept a
     * registered "localhost" entry — same reasoning as above.     */
    if (is_loopback) {
	for (BU_LIST_FOR(ihp, ihost, &HostHead)) {
	    CK_IHOST(ihp);
	    if (BU_STR_EQUAL(ihp->ht_name, "localhost"))
		return ihp;
	}
    }

    return make_default_host(name);
}


/*
 * Look up a host by name, resolving to canonical form via getaddrinfo().
 * Numeric dotted-quad addresses are also accepted and forwarded to
 * host_lookup_by_addr().
 */
struct ihost *
host_lookup_by_name(const char *name, int enter)
{
    struct ihost *ihp;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char canon[NI_MAXHOST];
    int ret;

    if (isdigit((int)*name)) {
	/* Numeric address — build a sockaddr_in and look up by address */
	struct sockaddr_in sockhim;
	memset(&sockhim, 0, sizeof(sockhim));
	sockhim.sin_family = AF_INET;
	sockhim.sin_addr.s_addr = inet_addr(name);
	return host_lookup_by_addr(&sockhim, enter);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;   /* IPv4 for compat with sockaddr_in API */
    hints.ai_flags    = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(name, NULL, &hints, &res);
    if (ret != 0) {
	bu_log("%s: bad host (%s)\n", name, gai_strerror(ret));
	return IHOST_NULL;
    }

    /* Use canonical name if available */
    if (res->ai_canonname && res->ai_canonname[0] != '\0')
	bu_strlcpy(canon, res->ai_canonname, sizeof(canon));
    else
	bu_strlcpy(canon, name, sizeof(canon));
    freeaddrinfo(res);

    /* Search existing table */
    for (BU_LIST_FOR(ihp, ihost, &HostHead)) {
	CK_IHOST(ihp);
	if (BU_STR_EQUAL(ihp->ht_name, canon))
	    return ihp;
    }

    if (enter == 0) {
	bu_log("%s: bad host\n", name);
	return IHOST_NULL;
    }

    return make_default_host(canon);
}


struct ihost *
host_lookup_of_fd(int fd)
{
    socklen_t fromlen;
    struct sockaddr_in from;

    fromlen = sizeof(from);
    if (getpeername(fd, (struct sockaddr *)&from, &fromlen) < 0) {
	perror("getpeername");
	return IHOST_NULL;
    }

    return host_lookup_by_addr(&from, 1);
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
