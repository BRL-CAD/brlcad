/*                      T L S _ W R A P . H
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
/** @file tls_wrap.h
 *
 * TLS wrapping for the fbserv/client framebuffer protocol connection.
 *
 * OVERVIEW
 * --------
 * This module adds optional TLS encryption to the libpkg-based
 * transport used between fbserv (the framebuffer server) and remote
 * framebuffer clients (e.g. rt via if_remote.c).  Both ends communicate
 * over a plain TCP connection managed by libpkg.  When OpenSSL is
 * available at build time, we:
 *
 *   1. Attach a pair of I/O callbacks (pkc_tls_read / pkc_tls_write)
 *      to each struct pkg_conn so that all protocol data goes through
 *      SSL_read / SSL_write rather than raw recv / send.
 *   2. Negotiate TLS immediately after the TCP handshake completes —
 *      before the first PKG message is exchanged.
 *
 * fbserv acts as the TLS *server*: it generates a self-signed RSA-2048
 * certificate at startup (no external PKI infrastructure needed).
 * Client programs (if_remote.c) act as TLS *clients*: they connect to
 * fbserv, perform the client-side handshake, and verify the certificate
 * public-key pin supplied through a trusted launch or user channel.
 *
 * SECURITY MODEL
 * --------------
 *  - All framebuffer data, pixel writes, and protocol commands are
 *    encrypted in transit (TLS 1.2 or TLS 1.3, negotiated by OpenSSL).
 *  - The session token (from auth.h) authenticates clients to fbserv.
 *  - The public-key pin authenticates fbserv before the token is sent.
 *  - The cert/key can be overridden via FBSERV_TLS_CERT / FBSERV_TLS_KEY
 *    environment variables.
 *
 * GUARD MACRO
 * -----------
 * To avoid "defined but not used" warnings, the implementation
 * functions are wrapped in `#ifdef FBSERV_TLS_IMPL`.  Define that
 * macro before including this header in exactly one translation unit
 * per binary.  The FBSERV_TLS_CLIENT define selects client-only paths;
 * without it the server-side accept path is compiled.  Define
 * FBSERV_TLS_SERVER_CTX when the translation unit also needs to create
 * a server SSL_CTX.
 */

#ifndef FBSERV_TLS_WRAP_H
#define FBSERV_TLS_WRAP_H

#include "common.h"

#ifdef HAVE_OPENSSL_SSL_H

#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

#ifndef _WIN32
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/select.h>
#endif

#include "bu/datetime.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "pkg.h"

/** TLS handshake succeeded */
#define FBSERV_TLS_OK     1
/** TLS handshake failed */
#define FBSERV_TLS_ERR    0
/** TLS handshake needs more socket activity */
#define FBSERV_TLS_PENDING (-1)

/** Maximum time allowed for a TLS handshake. */
#define FBSERV_TLS_HANDSHAKE_TIMEOUT_MS 5000

/** SHA-256 certificate public-key fingerprint sizes. */
#define FBSERV_TLS_FINGERPRINT_LEN 32
#define FBSERV_TLS_FINGERPRINT_HEX_LEN (FBSERV_TLS_FINGERPRINT_LEN * 2)


#ifdef FBSERV_TLS_IMPL

static int _fbserv_tls_set_nonblocking(int fd, int *saved_flags);
static void _fbserv_tls_restore_blocking(int fd, int saved_flags);

/* -----------------------------------------------------------------------
 * Private: I/O callbacks that libpkg will invoke via pkc_tls_read /
 * pkc_tls_write.
 * --------------------------------------------------------------------- */

static ptrdiff_t
_fbserv_ssl_read(void *ctx, void *buf, size_t n)
{
    int ret = SSL_read((SSL *)ctx, buf, (int)n);
    if (ret <= 0) {
	int err = SSL_get_error((SSL *)ctx, ret);
	if (err == SSL_ERROR_ZERO_RETURN)
	    return 0;	/* clean EOF */
	return -1;
    }
    return (ptrdiff_t)ret;
}


static ptrdiff_t
_fbserv_ssl_write(void *ctx, const void *buf, size_t n)
{
    int ret = SSL_write((SSL *)ctx, buf, (int)n);
    if (ret <= 0)
	return -1;
    return (ptrdiff_t)ret;
}


static int
_fbserv_ssl_pending(void *ctx)
{
    SSL *ssl = (SSL *)ctx;
    unsigned char byte;
    int fd;
    int result;
    int saved_flags = 0;

    if (SSL_pending(ssl) > 0)
	return 1;
    if (!SSL_has_pending(ssl))
	return 0;
    fd = SSL_get_fd(ssl);
    if (!_fbserv_tls_set_nonblocking(fd, &saved_flags))
	return 0;
    result = SSL_peek(ssl, &byte, 1);
    _fbserv_tls_restore_blocking(fd, saved_flags);
    return result > 0;
}


/**
 * Cleanup callback registered in pkc_tls_free.  The underlying fd is closed
 * separately by pkg_close().
 */
static void
_fbserv_ssl_free(void *ctx)
{
    SSL *ssl = (SSL *)ctx;
    if (!ssl)
	return;
    /* Do not call SSL_shutdown() here.  pkg_close() is also used on failed or
     * unresponsive peers, and close_notify on a blocking socket is unbounded. */
    SSL_free(ssl);
}


/* -----------------------------------------------------------------------
 * Private: log OpenSSL errors to bu_log.
 * --------------------------------------------------------------------- */

static void
_fbserv_tls_log_errors(const char *prefix)
{
    unsigned long e;
    char buf[256];
    while ((e = ERR_get_error()) != 0) {
	ERR_error_string_n(e, buf, sizeof(buf));
	bu_log("%s: %s\n", prefix, buf);
    }
}


#if defined(FBSERV_TLS_CLIENT) || defined(FBSERV_TLS_SERVER_CTX)
static int
_fbserv_tls_cert_fingerprint(const X509 *cert,
	unsigned char fingerprint[FBSERV_TLS_FINGERPRINT_LEN])
{
    unsigned int fingerprint_len = 0;

    if (!cert || X509_pubkey_digest(cert, EVP_sha256(), fingerprint,
	&fingerprint_len) != 1)
	return 0;
    return fingerprint_len == (unsigned int)FBSERV_TLS_FINGERPRINT_LEN;
}
#endif


#ifdef FBSERV_TLS_SERVER_CTX
static int
_fbserv_tls_fingerprint_string(const X509 *cert, char *buffer,
	size_t buffer_size)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char fingerprint[FBSERV_TLS_FINGERPRINT_LEN];
    size_t i;

    if (!buffer || buffer_size < (size_t)FBSERV_TLS_FINGERPRINT_HEX_LEN + 1 ||
	!_fbserv_tls_cert_fingerprint(cert, fingerprint))
	return 0;
    for (i = 0; i < (size_t)FBSERV_TLS_FINGERPRINT_LEN; i++) {
	buffer[i * 2] = hex[fingerprint[i] >> 4];
	buffer[i * 2 + 1] = hex[fingerprint[i] & 0x0f];
    }
    buffer[FBSERV_TLS_FINGERPRINT_HEX_LEN] = '\0';
    return 1;
}
#endif


#ifdef FBSERV_TLS_CLIENT
static int
_fbserv_tls_hex_value(unsigned char character)
{
    if (character >= '0' && character <= '9')
	return character - '0';
    if (character >= 'a' && character <= 'f')
	return character - 'a' + 10;
    if (character >= 'A' && character <= 'F')
	return character - 'A' + 10;
    return -1;
}


static int
_fbserv_tls_parse_fingerprint(const char *text,
	unsigned char fingerprint[FBSERV_TLS_FINGERPRINT_LEN])
{
    size_t i;

    if (!text || strlen(text) != (size_t)FBSERV_TLS_FINGERPRINT_HEX_LEN)
	return 0;
    for (i = 0; i < (size_t)FBSERV_TLS_FINGERPRINT_LEN; i++) {
	int high = _fbserv_tls_hex_value((unsigned char)text[i * 2]);
	int low = _fbserv_tls_hex_value((unsigned char)text[i * 2 + 1]);

	if (high < 0 || low < 0)
	    return 0;
	fingerprint[i] = (unsigned char)((high << 4) | low);
    }
    return 1;
}
#endif


/* -----------------------------------------------------------------------
 * Server-side functions (not compiled when FBSERV_TLS_CLIENT is set)
 * --------------------------------------------------------------------- */

#ifdef FBSERV_TLS_SERVER_CTX

/* Private: generate a self-signed RSA-2048 certificate + key in-memory */
static int
_fbserv_tls_gen_selfsigned(SSL_CTX *ctx)
{
    EVP_PKEY *pkey = NULL;
    X509 *cert = NULL;
    X509_NAME *name = NULL;
    int ok = 0;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    pkey = EVP_RSA_gen(2048);
#else
    {
	RSA *rsa = RSA_new();
	BIGNUM *e = BN_new();
	pkey = EVP_PKEY_new();
	if (!pkey || !rsa || !e) {
	    if (rsa) RSA_free(rsa);
	    if (e)   BN_free(e);
	    goto out;
	}
	BN_set_word(e, RSA_F4);
	if (RSA_generate_key_ex(rsa, 2048, e, NULL) != 1) {
	    BN_free(e);
	    RSA_free(rsa);
	    EVP_PKEY_free(pkey);
	    pkey = NULL;
	    goto out;
	}
	BN_free(e);
	EVP_PKEY_assign_RSA(pkey, rsa);
    }
#endif

    if (!pkey) goto out;

    cert = X509_new();
    if (!cert) goto out;

    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600L);

    X509_set_pubkey(cert, pkey);

    name = X509_NAME_new();
    if (!name) goto out;
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
				   (const unsigned char *)"fbserv", -1, -1, 0) != 1) goto out;
    if (X509_set_subject_name(cert, name) != 1) goto out;
    if (X509_set_issuer_name(cert, name) != 1) goto out;

    if (X509_sign(cert, pkey, EVP_sha256()) == 0) goto out;

    if (SSL_CTX_use_certificate(ctx, cert) != 1) goto out;
    if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1) goto out;
    if (SSL_CTX_check_private_key(ctx) != 1) goto out;

    ok = 1;

out:
    if (!ok)
	_fbserv_tls_log_errors("fbserv TLS cert gen");
    if (name) X509_NAME_free(name);
    if (cert) X509_free(cert);
    if (pkey) EVP_PKEY_free(pkey);
    return ok;
}


/**
 * Create an SSL_CTX for the fbserv *server* side.  Generates a
 * self-signed RSA-2048 certificate in memory — no external cert files
 * are required.
 *
 * If @p certfile / @p keyfile are non-NULL the named PEM files are
 * loaded instead of generating a certificate.
 *
 * Returns a new SSL_CTX on success, NULL on failure.
 */
static SSL_CTX *
fbserv_tls_server_ctx(const char *certfile, const char *keyfile)
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
	_fbserv_tls_log_errors("SSL_CTX_new (fbserv server)");
	return NULL;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    if (certfile && keyfile) {
	if (SSL_CTX_use_certificate_chain_file(ctx, certfile) != 1
	    || SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) != 1
	    || SSL_CTX_check_private_key(ctx) != 1) {
	    _fbserv_tls_log_errors("fbserv TLS: loading cert/key files");
	    SSL_CTX_free(ctx);
	    return NULL;
	}
    } else {
	if (!_fbserv_tls_gen_selfsigned(ctx)) {
	    SSL_CTX_free(ctx);
	    return NULL;
	}
    }

    return ctx;
}


static int
fbserv_tls_server_fingerprint(SSL_CTX *ctx, char *buffer, size_t buffer_size)
{
    if (!ctx)
	return 0;
    return _fbserv_tls_fingerprint_string(SSL_CTX_get0_certificate(ctx),
	buffer, buffer_size);
}

#endif /* FBSERV_TLS_SERVER_CTX */


static int
_fbserv_tls_set_nonblocking(int fd, int *saved_flags)
{
#ifdef _WIN32
    u_long mode = 1;
    (void)saved_flags;
    return ioctlsocket((SOCKET)(uintptr_t)fd, FIONBIO, &mode) == 0;
#else
    int flags;

    if (!saved_flags || fd < 0 || fd >= FD_SETSIZE)
	return 0;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	return 0;
    *saved_flags = flags;
    return 1;
#endif
}


static void
_fbserv_tls_restore_blocking(int fd, int saved_flags)
{
#ifdef _WIN32
    u_long mode = 0;
    (void)saved_flags;
    (void)ioctlsocket((SOCKET)(uintptr_t)fd, FIONBIO, &mode);
#else
    (void)fcntl(fd, F_SETFL, saved_flags);
#endif
}


#ifdef FBSERV_TLS_CLIENT
static int
_fbserv_tls_wait(int fd, int ssl_error, int64_t deadline)
{
    fd_set read_fds;
    fd_set write_fds;
    int selected;

    while (1) {
	int64_t remaining = deadline - bu_gettime();
	struct timeval timeout;

	if (remaining <= 0)
	    return 0;
	timeout.tv_sec = (long)(remaining / 1000000);
	timeout.tv_usec = (long)(remaining % 1000000);
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
#ifdef _WIN32
	SOCKET socket_fd = (SOCKET)(uintptr_t)fd;
	if (ssl_error == SSL_ERROR_WANT_READ)
	    FD_SET(socket_fd, &read_fds);
	else
	    FD_SET(socket_fd, &write_fds);
	selected = select(0, &read_fds, &write_fds, NULL, &timeout);
	if (selected == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
	    continue;
#else
	if (ssl_error == SSL_ERROR_WANT_READ)
	    FD_SET(fd, &read_fds);
	else
	    FD_SET(fd, &write_fds);
	selected = select(fd + 1, &read_fds, &write_fds, NULL, &timeout);
	if (selected < 0 && errno == EINTR)
	    continue;
#endif
	return selected > 0;
    }
}


static int
_fbserv_tls_negotiate(struct pkg_conn *pc, int fd, int64_t deadline)
{
    const char request = 1;
    struct pkg_header *response_header;
    const size_t header_size = sizeof(struct pkg_header);
    const size_t response_size = header_size + 1;
    size_t available;

    if (pkg_send(MSG_FBTLS, &request, sizeof(request), pc) !=
	(int)sizeof(request))
	return 0;

    while ((available = (size_t)(pc->pkc_inend - pc->pkc_incur)) <
	header_size) {
	if (!_fbserv_tls_wait(fd, SSL_ERROR_WANT_READ, deadline) ||
	    pkg_suckin(pc) <= 0)
	    return 0;
    }

    response_header = (struct pkg_header *)&pc->pkc_inbuf[pc->pkc_incur];
    if (pkg_gshort((char *)response_header->pkh_magic) != PKG_MAGIC ||
	pkg_gshort((char *)response_header->pkh_type) != MSG_FBTLS ||
	pkg_glong((char *)response_header->pkh_len) != 1)
	return 0;

    while ((available = (size_t)(pc->pkc_inend - pc->pkc_incur)) <
	response_size) {
	if (!_fbserv_tls_wait(fd, SSL_ERROR_WANT_READ, deadline) ||
	    pkg_suckin(pc) <= 0)
	    return 0;
    }

    pc->pkc_incur += (int)response_size;
    return pc->pkc_inbuf[pc->pkc_incur - 1] == 1;
}
#endif


#ifndef FBSERV_TLS_CLIENT

struct fbserv_tls_accept_state {
    SSL *ssl;
};


static void
_fbserv_tls_accept_state_free(void *context)
{
    struct fbserv_tls_accept_state *state =
	(struct fbserv_tls_accept_state *)context;

    if (!state)
	return;
    if (state->ssl)
	SSL_free(state->ssl);
    bu_free(state, "fbserv TLS accept state");
}

/**
 * Advance a server-side handshake without waiting for socket readiness.
 *
 * Returns FBSERV_TLS_OK on success, FBSERV_TLS_PENDING when the caller
 * should retry after socket activity, and FBSERV_TLS_ERR on failure.
 */
static int
fbserv_tls_accept_step(SSL_CTX *ctx, struct pkg_conn *pc)
{
    struct fbserv_tls_accept_state *state;
    SSL *ssl;
    int fd;
    int saved_flags = 0;
    int result;
    int ssl_error;

    if (!ctx || !pc || pc->pkc_tx_kind != 0)
	return FBSERV_TLS_ERR;

    fd = pkg_get_read_fd(pc);
    if (!_fbserv_tls_set_nonblocking(fd, &saved_flags))
	return FBSERV_TLS_ERR;

    state = (struct fbserv_tls_accept_state *)pc->pkc_tls_ctx;
    if (!state) {
	state = (struct fbserv_tls_accept_state *)bu_calloc(1,
	    sizeof(struct fbserv_tls_accept_state),
	    "fbserv TLS accept state");
	state->ssl = SSL_new(ctx);
	if (!state->ssl || SSL_set_fd(state->ssl, fd) != 1) {
	    _fbserv_tls_log_errors("fbserv TLS accept setup");
	    _fbserv_tls_accept_state_free(state);
	    _fbserv_tls_restore_blocking(fd, saved_flags);
	    return FBSERV_TLS_ERR;
	}
	pkg_set_tls(pc, state, NULL, NULL, NULL,
	    _fbserv_tls_accept_state_free);
    }

    result = SSL_accept(state->ssl);
    if (result == 1) {
	ssl = state->ssl;
	state->ssl = NULL;
	_fbserv_tls_accept_state_free(state);
	pkg_set_tls(pc, ssl, _fbserv_ssl_read, _fbserv_ssl_write,
	    _fbserv_ssl_pending,
	    _fbserv_ssl_free);
	_fbserv_tls_restore_blocking(fd, saved_flags);
	return FBSERV_TLS_OK;
    }

    ssl_error = SSL_get_error(state->ssl, result);
    _fbserv_tls_restore_blocking(fd, saved_flags);
    if (ssl_error == SSL_ERROR_WANT_READ ||
	ssl_error == SSL_ERROR_WANT_WRITE)
	return FBSERV_TLS_PENDING;

    _fbserv_tls_log_errors("fbserv SSL_accept");
    return FBSERV_TLS_ERR;
}

#else  /* FBSERV_TLS_CLIENT — compile only the client-side functions */

/**
 * Create an SSL_CTX for the fbserv *client* side.  CA verification is
 * disabled because the server normally uses a per-session self-signed
 * certificate; fbserv_tls_connect() verifies its public-key pin instead.
 *
 * Returns a new SSL_CTX on success, NULL on failure.
 */
static SSL_CTX *
fbserv_tls_client_ctx(void)
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
	_fbserv_tls_log_errors("SSL_CTX_new (fbserv client)");
	return NULL;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    return ctx;
}


/**
 * Perform the TLS client-side handshake on a newly opened
 * struct pkg_conn.  On success the callbacks are set.
 *
 * Returns FBSERV_TLS_OK on success, FBSERV_TLS_ERR on failure.
 */
static int
fbserv_tls_connect(SSL_CTX *ctx, struct pkg_conn *pc,
	const char *expected_fingerprint)
{
    SSL *ssl = NULL;
    unsigned char expected[FBSERV_TLS_FINGERPRINT_LEN];
    unsigned char actual[FBSERV_TLS_FINGERPRINT_LEN];
    int fd;
    int saved_flags = 0;
    int status = FBSERV_TLS_ERR;
    int64_t deadline;

    if (!ctx || !pc || pc->pkc_tx_kind != 0 ||
	!_fbserv_tls_parse_fingerprint(expected_fingerprint, expected))
	return FBSERV_TLS_ERR;

    fd = pkg_get_read_fd(pc);
    if (!_fbserv_tls_set_nonblocking(fd, &saved_flags))
	return FBSERV_TLS_ERR;

    ssl = SSL_new(ctx);
    if (!ssl) {
	_fbserv_tls_log_errors("SSL_new (fbserv client)");
	goto tls_connect_done;
    }

    if (SSL_set_fd(ssl, fd) != 1) {
	_fbserv_tls_log_errors("SSL_set_fd (fbserv client)");
	goto tls_connect_done;
    }

    deadline = bu_gettime() +
	(int64_t)FBSERV_TLS_HANDSHAKE_TIMEOUT_MS * 1000;
    if (!_fbserv_tls_negotiate(pc, fd, deadline)) {
	bu_log("fbserv TLS capability negotiation failed\n");
	goto tls_connect_done;
    }
    while (1) {
	int result = SSL_connect(ssl);
	int ssl_error;

	if (result == 1)
	    break;
	ssl_error = SSL_get_error(ssl, result);
	if ((ssl_error != SSL_ERROR_WANT_READ &&
		ssl_error != SSL_ERROR_WANT_WRITE) ||
	    !_fbserv_tls_wait(fd, ssl_error, deadline)) {
	    if (bu_gettime() >= deadline)
		bu_log("fbserv TLS handshake timed out\n");
	    else
		_fbserv_tls_log_errors("fbserv SSL_connect");
	    goto tls_connect_done;
	}
    }

    {
	X509 *peer_cert;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	peer_cert = SSL_get1_peer_certificate(ssl);
#else
	peer_cert = SSL_get_peer_certificate(ssl);
#endif
	if (!peer_cert || !_fbserv_tls_cert_fingerprint(peer_cert, actual) ||
	    CRYPTO_memcmp(actual, expected, sizeof(actual)) != 0) {
	    bu_log("fbserv TLS server fingerprint mismatch\n");
	    if (peer_cert)
		X509_free(peer_cert);
	    goto tls_connect_done;
	}
	X509_free(peer_cert);
    }

    pkg_set_tls(pc, (void *)ssl,
		_fbserv_ssl_read, _fbserv_ssl_write, _fbserv_ssl_pending,
		_fbserv_ssl_free);
    ssl = NULL;
    status = FBSERV_TLS_OK;

tls_connect_done:
    if (ssl)
	SSL_free(ssl);
    _fbserv_tls_restore_blocking(fd, saved_flags);
    return status;
}

#endif /* !FBSERV_TLS_CLIENT / FBSERV_TLS_CLIENT */

#endif /* FBSERV_TLS_IMPL */

#endif /* HAVE_OPENSSL_SSL_H */

#endif /* FBSERV_TLS_WRAP_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
