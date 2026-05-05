/*                      B N E T W O R K . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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
/** @addtogroup bu_bnetwork
 *
 * @brief
 * BRL-CAD system compatibility wrapper header that provides declarations for
 * native and standard system NETWORKING routines.
 *
 * This header is commonly used in lieu of including the following: winsock2.h
 * (not select, fd_set), netinet/in.h, netinet/tcp.h, arpa/inet.h (htonl,
 * ntohl, etc)
 *
 * The logic in this header should not rely on common.h's HAVE_* defines and
 * should not be including the common.h header.  This is intended to be a
 * stand-alone portability header intended to be independent of build system,
 * reusable by external projects.
 */

/** @{ */
/** @file bnetwork.h */

#ifndef BNETWORK_H
#define BNETWORK_H

#if defined(_WIN32) && !defined(__CYGWIN__)
#  ifndef _WINSOCKAPI_
/* Prevent windows.h (pulled in by winsock2.h) from defining min/max macros,
 * which would conflict with C++ template min/max */
#    ifndef NOMINMAX
#      define NOMINMAX
#    endif
#    include <winsock2.h> /* link against ws2_32 library */
#    include <ws2tcpip.h> /* provides extensions */
#    undef rad1 /* Win32 radio button 1 */
#    undef rad2 /* Win32 radio button 2 */
#    undef small /* defined as part of the Microsoft Interface Definition Language (MIDL) */
#  endif
#else
#  include <sys/types.h>
#  include <netinet/in.h> /* sockaddr */
#  include <netinet/tcp.h> /* for TCP_NODELAY sockopt */
#  include <arpa/inet.h> /* hton/ntoh, inet_addr functions */
#  include <sys/socket.h> /* accept, connect, send, recv, ... */
/* for c90/c99 compatibility */
#  if !defined(HAVE_DECL_HTONL) && !defined(htonl)
extern uint32_t htonl(uint32_t);
extern uint32_t ntohl(uint32_t);
#  endif
#endif

/* INADDR_* constants come from <netinet/in.h> on POSIX and <winsock2.h> on
 * Windows.  Some strict-mode builds or Unity-build ordering issues can leave
 * them undefined even when the underlying header was nominally processed;
 * provide safe fallbacks so callers that include bnetwork.h always have them. */
#ifndef INADDR_ANY
#  define INADDR_ANY       ((uint32_t)0x00000000)
#endif
#ifndef INADDR_LOOPBACK
#  define INADDR_LOOPBACK  ((uint32_t)0x7f000001)
#endif
#ifndef INADDR_BROADCAST
#  define INADDR_BROADCAST ((uint32_t)0xffffffff)
#endif

#endif /* BNETWORK_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
