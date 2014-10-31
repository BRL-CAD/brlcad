/*                      B N E T W O R K . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file bnetwork.h
 *
 * BRL-CAD private system compatibility wrapper header that provides
 * declarations for native and standard system NETWORKING routines.
 *
 * This header is commonly used in leu of including the following:
 * winsock2.h (not select, fd_set), netinet/in.h, netinet/tcp.h,
 * arpa/inet.h (htonl, ntohl, etc)
 *
 * This header does not belong to any BRL-CAD library but may used by
 * all of them.  Consider this header PRIVATE and subject to change,
 * NOT TO BE USED BY THIRD PARTIES.
 *
 * The below logic should not rely on common.h's HAVE_* defines and
 * should not be including the common.h header.  This is intended to
 * be a stand-alone portability header intended to be independent of
 * build system, reusable by external projects.
 */

#ifndef BNETWORK_H
#define BNETWORK_H

/* make sure this header always comes before bio.h due to system
 * header ordering requirements.  this is mostly a windows issue, but
 * we want to detect the issue early.
 */
#if defined(BIO_H)
#  error "The #include for bio.h must come after this header (Windows Sockets portability)."
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#  ifndef _WINSOCKAPI_
#    include <winsock2.h> /* link against ws2_32 library */

#   undef rad1 /* Win32 radio button 1 */
#   undef rad2 /* Win32 radio button 2 */
#   undef small /* defined as part of the Microsoft Interface Definition Language (MIDL) */
#   undef IN
#   undef OUT

#  endif
#else
#  include <sys/types.h>
#  include <netinet/in.h> /* sockaddr */
#  include <netinet/tcp.h> /* for TCP_NODELAY sockopt */
#  include <arpa/inet.h> /* hton/ntoh, inet_addr functions */
#  include <sys/socket.h> /* accept, connect, send, recv, ... */
#endif

#endif /* BNETWORK_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
