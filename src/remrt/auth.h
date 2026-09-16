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
 * Session authentication token generation and verification for remrt.
 *
 * Provides a simple shared-secret mechanism to prevent accidental
 * cross-talk between unrelated remrt sessions sharing TCP ports.
 *
 * DESIGN
 * ------
 * remrt generates a 256-bit (32-byte) random token at startup and
 * prints it to stderr for reference.  When remrt auto-launches an
 * rtsrv worker via SSH it appends "-S <token>" to the rtsrv command
 * line.  The rtsrv worker includes the token as a suffix of its
 * MSG_VERSION handshake string; remrt's ph_version() verifies the
 * token before accepting the worker.
 *
 * Workers that were started manually (passive connections) and do not
 * carry the token are still accepted to preserve backward
 * compatibility.  Pass '-A' to remrt for strict mode where ALL
 * connecting workers must authenticate.
 *
 * PORTABILITY
 * -----------
 * The token generation logic selects the strongest available entropy
 * source at compile time, in priority order:
 *   1. OpenSSL RAND_bytes()  (if HAVE_OPENSSL_RAND_H is defined)
 *   2. /dev/urandom          (POSIX platforms)
 *   3. CryptGenRandom()      (Windows)
 *   4. time()+PID seeded rand()  (last-resort fallback)
 *
 * C++17 note: std::random_device could be used here in a C++ context,
 * but this file must remain C-compatible for inclusion in the C
 * translation units of remrt and rtsrv.
 */

#ifndef REMRT_AUTH_H
#define REMRT_AUTH_H

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
#define REMRT_AUTH_TOKEN_LEN 64

/**
 * The separator used between the protocol version string and the
 * session token in the MSG_VERSION payload, e.g.
 *   "BRL-CAD REMRT Protocol v2.0 token=abc123..."
 */
#define REMRT_AUTH_TOKEN_PREFIX " token="


/**
 * Fill @p buf (at least REMRT_AUTH_TOKEN_LEN+1 bytes) with a freshly
 * generated, NUL-terminated, lower-case hex session token.
 *
 * Available only when REMRT_AUTH_IMPL is defined before including
 * this header (done in remrt.c).  rtsrv.c only receives and forwards
 * a token; it does not need to generate or verify one itself.
 */
#ifdef REMRT_AUTH_IMPL

static void
remrt_generate_token(char *buf)
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

    /* Last-resort fallback: mix several cheap entropy sources.
     * This is intentionally low-quality; prefer the platform random
     * paths above.  Using just time() is vulnerable to replay attacks
     * when ASLR is disabled, so we also fold in the PID and a
     * gethrtime/clock_gettime value when available. */
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
    buf[REMRT_AUTH_TOKEN_LEN] = '\0';
}


/**
 * Constant-time token comparison to resist timing side-channels.
 *
 * Returns 1 if @p provided matches @p expected (both must be exactly
 * REMRT_AUTH_TOKEN_LEN characters long), 0 otherwise.
 */
static int
remrt_verify_token(const char *provided, const char *expected)
{
    int diff = 0;
    size_t i;

    if (!provided || !expected)
	return 0;
    if (strlen(provided) != REMRT_AUTH_TOKEN_LEN
	|| strlen(expected) != REMRT_AUTH_TOKEN_LEN)
	return 0;

    for (i = 0; i < REMRT_AUTH_TOKEN_LEN; i++)
	diff |= (unsigned char)provided[i] ^ (unsigned char)expected[i];

    return (diff == 0) ? 1 : 0;
}

#endif /* REMRT_AUTH_IMPL */

#endif /* REMRT_AUTH_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
