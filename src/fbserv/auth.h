/*                         A U T H . H
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file auth.h
 *
 * Session authentication token generation and verification for fbserv.
 *
 * Provides a simple shared-secret mechanism to prevent accidental
 * cross-talk between unrelated fbserv sessions sharing TCP ports.
 *
 * DESIGN
 * ------
 * fbserv generates a 256-bit (32-byte) random token at startup and
 * prints it to stderr for reference.  Client programs (e.g. rt, pix-fb)
 * that need to authenticate read the token from the FBSERV_TOKEN
 * environment variable OR from the -S <token> command-line option and
 * send it to the server via the MSG_FBAUTH message before MSG_FBOPEN.
 *
 * For the embedded fbserv (in MGED/Archer/qged), the hosting application
 * generates a token when starting the fbserv listener and passes it to
 * child processes via the FBSERV_TOKEN environment variable.
 *
 * Clients that do not carry the token are still accepted to preserve
 * backward compatibility.  Pass '-A' to fbserv for strict mode where
 * ALL connecting clients must authenticate.
 *
 * PORTABILITY
 * -----------
 * The token generation logic selects the strongest available entropy
 * source at compile time, in priority order:
 *   1. OpenSSL RAND_bytes()  (if HAVE_OPENSSL_RAND_H is defined)
 *   2. /dev/urandom          (POSIX platforms)
 *   3. CryptGenRandom()      (Windows)
 *   4. time()+PID seeded rand()  (last-resort fallback)
 */

#ifndef FBSERV_AUTH_H
#define FBSERV_AUTH_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bu/process.h"
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#ifdef HAVE_OPENSSL_RAND_H
#  include <openssl/rand.h>
#endif

#ifdef HAVE_WINDOWS_H
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <wincrypt.h>
#endif

/**
 * Length of the hex-encoded authentication token string (not counting
 * the NUL terminator).  32 raw bytes => 64 hex characters.
 */
#define FBSERV_AUTH_TOKEN_LEN 64

/**
 * Environment variable that client programs check for the session token.
 */
#define FBSERV_AUTH_TOKEN_ENVVAR "FBSERV_TOKEN"


/**
 * Fill @p buf (at least FBSERV_AUTH_TOKEN_LEN+1 bytes) with a freshly
 * generated, NUL-terminated, lower-case hex session token.
 *
 * Compiled only when FBSERV_AUTH_SERVER is defined before including
 * this header.  Only fbserv.c (the main dispatcher TU) needs token
 * generation; protocol handlers and clients only verify or forward.
 */
#ifdef FBSERV_AUTH_SERVER
static void
fbserv_generate_token(char *buf)
{
    unsigned char raw[32];
    int i;
    int ok = 0;

    memset(raw, 0, sizeof(raw));

#ifdef HAVE_OPENSSL_RAND_H
    if (!ok && RAND_bytes(raw, (int)sizeof(raw)) == 1)
	ok = 1;
#endif

#ifdef HAVE_WINDOWS_H
    if (!ok) {
	HCRYPTPROV hProv = 0;
	if (CryptAcquireContextW(&hProv, NULL, NULL,
				 PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
	    if (CryptGenRandom(hProv, (DWORD)sizeof(raw), raw))
		ok = 1;
	    CryptReleaseContext(hProv, 0);
	}
    }
#else
    /* POSIX: read from /dev/urandom */
    if (!ok) {
	FILE *fp = fopen("/dev/urandom", "rb");
	if (fp) {
	    if (fread(raw, 1, sizeof(raw), fp) == sizeof(raw))
		ok = 1;
	    fclose(fp);
	}
    }
#endif

    /* Last-resort fallback: mix several cheap entropy sources */
    if (!ok) {
#ifdef HAVE_SYS_TIME_H
	struct timeval tv;
	unsigned int seed;
	gettimeofday(&tv, NULL);
	seed = (unsigned int)tv.tv_sec
	    ^ (unsigned int)tv.tv_usec
	    ^ (unsigned int)bu_pid();
#else
	unsigned int seed = (unsigned int)time(NULL)
	    ^ (unsigned int)bu_pid();
#endif
	srand(seed);
	for (i = 0; i < 32; i++)
	    raw[i] = (unsigned char)(rand() & 0xff);
    }

    /* Encode as lower-case hex */
    for (i = 0; i < 32; i++)
	snprintf(buf + i * 2, 3, "%02x", (unsigned int)raw[i]);
    buf[FBSERV_AUTH_TOKEN_LEN] = '\0';
}
#endif /* FBSERV_AUTH_SERVER */


/**
 * Constant-time token comparison to resist timing side-channels.
 *
 * Compiled only when FBSERV_AUTH_IMPL is defined before including
 * this header.  Protocol handlers (server.c, libdm/fbserv.c) define
 * FBSERV_AUTH_IMPL to get this function; fbserv.c uses FBSERV_AUTH_SERVER
 * for generation only and does NOT need to call verify directly.
 */
#ifdef FBSERV_AUTH_IMPL
static int
fbserv_verify_token(const char *provided, const char *expected)
{
    int diff = 0;
    size_t i;

    if (!provided || !expected)
	return 0;
    if (strlen(provided) != FBSERV_AUTH_TOKEN_LEN
	|| strlen(expected) != FBSERV_AUTH_TOKEN_LEN)
	return 0;

    for (i = 0; i < FBSERV_AUTH_TOKEN_LEN; i++)
	diff |= (unsigned char)provided[i] ^ (unsigned char)expected[i];

    return (diff == 0) ? 1 : 0;
}

#endif /* FBSERV_AUTH_IMPL */

#endif /* FBSERV_AUTH_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
