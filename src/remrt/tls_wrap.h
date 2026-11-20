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
 * TLS wrapping for the remrt/rtsrv protocol connection.
 *
 * OVERVIEW
 * --------
 * This module adds optional TLS encryption to the libpkg-based
 * transport used between remrt (the dispatcher) and rtsrv (the render
 * worker) processes.  Both ends communicate over a plain TCP
 * connection managed by libpkg.  When TLS is requested and OpenSSL is
 * available at build time, we:
 *
 *   1. Attach a pair of I/O callbacks (pkc_tls_read / pkc_tls_write)
 *      to each struct pkg_conn so that all protocol data goes through
 *      SSL_read / SSL_write rather than raw recv / send.
 *   2. Negotiate TLS immediately after the TCP handshake completes --
 *      before the first PKG message is exchanged.
 *
 * remrt acts as the TLS *server*: it generates a self-signed RSA-2048
 * certificate at startup (no external PKI infrastructure needed).
 * rtsrv acts as the TLS *client*: it connects to remrt, performs the
 * client-side handshake, and verifies the certificate public-key pin
 * supplied through the worker launch channel.
 *
 * SECURITY MODEL
 * --------------
 *  - Geometry data, pixel results, and protocol commands are encrypted
 *    against passive observation in transit.
 *  - The session token (from auth.h) authenticates rtsrv to remrt.
 *  - rtsrv pins the certificate public-key fingerprint supplied through
 *    the worker launch channel, authenticating remrt before PKG traffic.
 *
 * GUARD MACRO
 * -----------
 * To avoid "defined but not used" warnings, the implementation
 * functions (remrt_tls_*) are wrapped in `#ifdef REMRT_TLS_IMPL`.
 * Define that macro before including this header in exactly one
 * translation unit per binary (remrt.c and rtsrv.c each define it
 * once).  The constants REMRT_TLS_* are always visible.
 */

#ifndef REMRT_TLS_WRAP_H
#define REMRT_TLS_WRAP_H

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

#include "bnetwork.h"
#include "bu/datetime.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "pkg.h"

/** TLS is active on this run (set after successful handshake) */
#define REMRT_TLS_OK     1
/** TLS handshake failed */
#define REMRT_TLS_ERR    0
/** TLS handshake needs more socket activity */
#define REMRT_TLS_PENDING (-1)

/** Maximum time allowed for capability negotiation and TLS setup. */
#define REMRT_TLS_HANDSHAKE_TIMEOUT_MS 5000
/** Event-loop retry interval while a handshake is incomplete. */
#define REMRT_TLS_HANDSHAKE_POLL_MS 10

/** SHA-256 certificate public-key fingerprint sizes. */
#define REMRT_TLS_FINGERPRINT_LEN 32
#define REMRT_TLS_FINGERPRINT_HEX_LEN (REMRT_TLS_FINGERPRINT_LEN * 2)

/** Fixed pre-TLS exchange used to require matching TLS capability. */
static const unsigned char remrt_tls_request[] = "RTTLS1\r\n";
static const unsigned char remrt_tls_response[] = "RTTLS1OK";


#ifdef REMRT_TLS_IMPL

static int _remrt_tls_set_nonblocking(int fd, int *saved_flags);
static void _remrt_tls_restore_blocking(int fd, int saved_flags);

/* -----------------------------------------------------------------------
 * Private: I/O callbacks that libpkg will invoke via pkc_tls_read /
 * pkc_tls_write.
 * --------------------------------------------------------------------- */

static ptrdiff_t
_remrt_ssl_read(void *ctx, void *buf, size_t n)
{
    int ret = SSL_read((SSL *)ctx, buf, (int)n);
    if (ret <= 0) {
	int err = SSL_get_error((SSL *)ctx, ret);
	/* WANT_READ / WANT_WRITE are retried by the caller */
	if (err == SSL_ERROR_ZERO_RETURN)
	    return 0;	/* clean EOF */
	return -1;
    }
    return (ptrdiff_t)ret;
}


static ptrdiff_t
_remrt_ssl_write(void *ctx, const void *buf, size_t n)
{
    int ret = SSL_write((SSL *)ctx, buf, (int)n);
    if (ret <= 0)
	return -1;
    return (ptrdiff_t)ret;
}


static int
_remrt_ssl_pending(void *ctx)
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
    if (!_remrt_tls_set_nonblocking(fd, &saved_flags))
	return 0;
    result = SSL_peek(ssl, &byte, 1);
    _remrt_tls_restore_blocking(fd, saved_flags);
    return result > 0;
}


/**
 * Cleanup callback registered in pkc_tls_free.  The underlying fd is closed
 * separately by pkg_close().
 */
static void
_remrt_ssl_free(void *ctx)
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
_remrt_tls_log_errors(const char *prefix)
{
    unsigned long e;
    char buf[256];
    while ((e = ERR_get_error()) != 0) {
	ERR_error_string_n(e, buf, sizeof(buf));
	bu_log("%s: %s\n", prefix, buf);
    }
}


static int
_remrt_tls_cert_fingerprint(const X509 *cert,
	unsigned char fingerprint[REMRT_TLS_FINGERPRINT_LEN])
{
    unsigned int fingerprint_len = 0;

    if (!cert || X509_pubkey_digest(cert, EVP_sha256(), fingerprint,
	&fingerprint_len) != 1)
	return 0;
    return fingerprint_len == (unsigned int)REMRT_TLS_FINGERPRINT_LEN;
}


#ifndef RTSRV

struct remrt_tls_accept_state {
    SSL *ssl;
    unsigned char message[sizeof(remrt_tls_request) - 1];
    size_t transferred;
    int stage;
};


static void
_remrt_tls_accept_state_free(void *context)
{
    struct remrt_tls_accept_state *state =
	(struct remrt_tls_accept_state *)context;

    if (!state)
	return;
    if (state->ssl)
	SSL_free(state->ssl);
    bu_free(state, "remrt TLS accept state");
}


static int
_remrt_tls_accept_transfer_step(int fd, unsigned char *buffer, size_t size,
	size_t *transferred, int send_data)
{
    int count;

#ifdef _WIN32
    if (send_data)
	count = send((SOCKET)(uintptr_t)fd, (const char *)buffer + *transferred,
	    (int)(size - *transferred), 0);
    else
	count = recv((SOCKET)(uintptr_t)fd, (char *)buffer + *transferred,
	    (int)(size - *transferred), 0);
    if (count == SOCKET_ERROR) {
	int error = WSAGetLastError();
	return (error == WSAEINTR || error == WSAEWOULDBLOCK) ?
	    REMRT_TLS_PENDING : REMRT_TLS_ERR;
    }
#else
    if (send_data)
	count = (int)send(fd, buffer + *transferred, size - *transferred, 0);
    else
	count = (int)recv(fd, buffer + *transferred, size - *transferred, 0);
    if (count < 0)
	return (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) ?
	    REMRT_TLS_PENDING : REMRT_TLS_ERR;
#endif
    if (count <= 0)
	return REMRT_TLS_ERR;
    *transferred += (size_t)count;
    return *transferred == size ? REMRT_TLS_OK : REMRT_TLS_PENDING;
}
static int
_remrt_tls_fingerprint_string(const X509 *cert, char *buffer,
	size_t buffer_size)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char fingerprint[REMRT_TLS_FINGERPRINT_LEN];

    if (!buffer || buffer_size < (size_t)REMRT_TLS_FINGERPRINT_HEX_LEN + 1 ||
	!_remrt_tls_cert_fingerprint(cert, fingerprint))
	return 0;
    for (size_t i = 0; i < (size_t)REMRT_TLS_FINGERPRINT_LEN; i++) {
	buffer[i * 2] = hex[fingerprint[i] >> 4];
	buffer[i * 2 + 1] = hex[fingerprint[i] & 0x0f];
    }
    buffer[REMRT_TLS_FINGERPRINT_HEX_LEN] = '\0';
    return 1;
}
#endif


#ifdef RTSRV
static int
_remrt_tls_hex_value(unsigned char character)
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
_remrt_tls_parse_fingerprint(const char *text,
	unsigned char fingerprint[REMRT_TLS_FINGERPRINT_LEN])
{
    if (!text || strlen(text) != (size_t)REMRT_TLS_FINGERPRINT_HEX_LEN)
	return 0;
    for (size_t i = 0; i < (size_t)REMRT_TLS_FINGERPRINT_LEN; i++) {
	int high = _remrt_tls_hex_value((unsigned char)text[i * 2]);
	int low = _remrt_tls_hex_value((unsigned char)text[i * 2 + 1]);

	if (high < 0 || low < 0)
	    return 0;
	fingerprint[i] = (unsigned char)((high << 4) | low);
    }
    return 1;
}


static int
remrt_tls_fingerprint_valid(const char *text)
{
    unsigned char fingerprint[REMRT_TLS_FINGERPRINT_LEN];
    return _remrt_tls_parse_fingerprint(text, fingerprint);
}
#endif


static int
_remrt_tls_set_nonblocking(int fd, int *saved_flags)
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
_remrt_tls_restore_blocking(int fd, int saved_flags)
{
#ifdef _WIN32
    u_long mode = 0;
    (void)saved_flags;
    (void)ioctlsocket((SOCKET)(uintptr_t)fd, FIONBIO, &mode);
#else
    (void)fcntl(fd, F_SETFL, saved_flags);
#endif
}


#ifdef RTSRV
static int
_remrt_tls_wait(int fd, int wait_for_read, int64_t deadline)
{
    while (1) {
	fd_set read_fds;
	fd_set write_fds;
	int64_t remaining = deadline - bu_gettime();
	struct timeval timeout;
	int selected;

	if (remaining <= 0)
	    return 0;
	timeout.tv_sec = (long)(remaining / 1000000);
	timeout.tv_usec = (long)(remaining % 1000000);
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
#ifdef _WIN32
	SOCKET socket_fd = (SOCKET)(uintptr_t)fd;
	FD_SET(socket_fd, wait_for_read ? &read_fds : &write_fds);
	selected = select(0, &read_fds, &write_fds, NULL, &timeout);
	if (selected == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
	    continue;
#else
	FD_SET(fd, wait_for_read ? &read_fds : &write_fds);
	selected = select(fd + 1, &read_fds, &write_fds, NULL, &timeout);
	if (selected < 0 && errno == EINTR)
	    continue;
#endif
	return selected > 0;
    }
}


static int
_remrt_tls_transfer(int fd, unsigned char *buffer, size_t size,
	int send_data, int64_t deadline)
{
    size_t transferred = 0;

    while (transferred < size) {
	int count;

	if (!_remrt_tls_wait(fd, !send_data, deadline))
	    return 0;
#ifdef _WIN32
	if (send_data)
	    count = send((SOCKET)(uintptr_t)fd,
		(const char *)buffer + transferred,
		(int)(size - transferred), 0);
	else
	    count = recv((SOCKET)(uintptr_t)fd, (char *)buffer + transferred,
		(int)(size - transferred), 0);
	if (count == SOCKET_ERROR) {
	    int error = WSAGetLastError();
	    if (error == WSAEINTR || error == WSAEWOULDBLOCK)
		continue;
	    return 0;
	}
#else
	if (send_data)
	    count = (int)send(fd, buffer + transferred, size - transferred, 0);
	else
	    count = (int)recv(fd, buffer + transferred, size - transferred, 0);
	if (count < 0) {
	    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
		continue;
	    return 0;
	}
#endif
	if (count <= 0)
	    return 0;
	transferred += (size_t)count;
    }

    return 1;
}


static int
_remrt_tls_negotiate(int fd, int server, int64_t deadline)
{
    unsigned char message[sizeof(remrt_tls_request) - 1];
    const unsigned char *outgoing = server ? remrt_tls_response : remrt_tls_request;
    const unsigned char *expected = server ? remrt_tls_request : remrt_tls_response;

    if (server && !_remrt_tls_transfer(fd, message, sizeof(message), 0, deadline))
	return 0;
    if (server && memcmp(message, expected, sizeof(message)) != 0)
	return 0;
    if (!_remrt_tls_transfer(fd, (unsigned char *)outgoing,
	sizeof(message), 1, deadline))
	return 0;
    if (!server && !_remrt_tls_transfer(fd, message, sizeof(message), 0, deadline))
	return 0;
    return server || memcmp(message, expected, sizeof(message)) == 0;
}


static int
_remrt_tls_handshake(SSL_CTX *ctx, struct pkg_conn *pc, int server,
	const char *expected_fingerprint)
{
    SSL *ssl = NULL;
    unsigned char expected[REMRT_TLS_FINGERPRINT_LEN];
    int fd;
    int saved_flags = 0;
    int status = REMRT_TLS_ERR;
    int64_t deadline;

    if (!ctx || !pc || pc->pkc_tx_kind != 0 ||
	(!server && !_remrt_tls_parse_fingerprint(expected_fingerprint, expected)))
	return REMRT_TLS_ERR;
    fd = pkg_get_read_fd(pc);
    if (!_remrt_tls_set_nonblocking(fd, &saved_flags))
	return REMRT_TLS_ERR;

    deadline = bu_gettime() +
	(int64_t)REMRT_TLS_HANDSHAKE_TIMEOUT_MS * 1000;
    if (!_remrt_tls_negotiate(fd, server, deadline)) {
	bu_log("remrt TLS capability negotiation failed\n");
	goto handshake_done;
    }

    ssl = SSL_new(ctx);
    if (!ssl) {
	_remrt_tls_log_errors("SSL_new");
	goto handshake_done;
    }
    if (SSL_set_fd(ssl, fd) != 1) {
	_remrt_tls_log_errors("SSL_set_fd");
	goto handshake_done;
    }

    while (1) {
	int result = server ? SSL_accept(ssl) : SSL_connect(ssl);
	int ssl_error;

	if (result == 1)
	    break;
	ssl_error = SSL_get_error(ssl, result);
	if ((ssl_error != SSL_ERROR_WANT_READ &&
		ssl_error != SSL_ERROR_WANT_WRITE) ||
	    !_remrt_tls_wait(fd, ssl_error == SSL_ERROR_WANT_READ, deadline)) {
	    if (bu_gettime() >= deadline)
		bu_log("remrt TLS handshake timed out\n");
	    else
		_remrt_tls_log_errors(server ? "SSL_accept" : "SSL_connect");
	    goto handshake_done;
	}
    }

    if (!server) {
	unsigned char actual[REMRT_TLS_FINGERPRINT_LEN];
	X509 *peer_cert;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	peer_cert = SSL_get1_peer_certificate(ssl);
#else
	peer_cert = SSL_get_peer_certificate(ssl);
#endif
	if (!peer_cert || !_remrt_tls_cert_fingerprint(peer_cert, actual) ||
	    CRYPTO_memcmp(actual, expected, sizeof(actual)) != 0) {
	    bu_log("remrt TLS server fingerprint verification failed\n");
	    if (peer_cert)
		X509_free(peer_cert);
	    goto handshake_done;
	}
	X509_free(peer_cert);
    }

    pkg_set_tls(pc, (void *)ssl,
	_remrt_ssl_read, _remrt_ssl_write, _remrt_ssl_pending,
	_remrt_ssl_free);
    ssl = NULL;
    status = REMRT_TLS_OK;

handshake_done:
    if (ssl)
	SSL_free(ssl);
    _remrt_tls_restore_blocking(fd, saved_flags);
    return status;
}
#endif


/* -----------------------------------------------------------------------
 * Public API — server side (remrt only, not compiled in rtsrv)
 * --------------------------------------------------------------------- */

#ifndef RTSRV

/* -----------------------------------------------------------------------
 * Private: generate a self-signed RSA-2048 certificate + key in-memory
 * and load them into the SSL_CTX.  Returns 1 on success, 0 on error.
 * --------------------------------------------------------------------- */

static int
_remrt_tls_gen_selfsigned(SSL_CTX *ctx)
{
    EVP_PKEY *pkey = NULL;
    X509 *cert = NULL;
    X509_NAME *name = NULL;
    int ok = 0;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    /* OpenSSL 3.x: EVP_RSA_gen() handles key generation in one call */
    pkey = EVP_RSA_gen(2048);
#else
    /* OpenSSL 1.1.x: use the legacy RSA key generation API */
    {
	RSA *rsa = RSA_new();
	BIGNUM *e = BN_new();
	pkey = EVP_PKEY_new();
	if (!pkey || !rsa || !e) {
	    if (rsa) RSA_free(rsa);
	    if (e)   BN_free(e);
	    goto out;
	}
	BN_set_word(e, RSA_F4);          /* 65537 */
	if (RSA_generate_key_ex(rsa, 2048, e, NULL) != 1) {
	    BN_free(e);
	    RSA_free(rsa);
	    EVP_PKEY_free(pkey);
	    pkey = NULL;
	    goto out;
	}
	BN_free(e);
	EVP_PKEY_assign_RSA(pkey, rsa);  /* rsa now owned by pkey */
    }
#endif

    if (!pkey) goto out;

    /* Build X.509 certificate */
    cert = X509_new();
    if (!cert) goto out;

    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600L); /* 1 year */

    X509_set_pubkey(cert, pkey);

    name = X509_NAME_new();
    if (!name) goto out;
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
				   (const unsigned char *)"remrt", -1, -1, 0) != 1) goto out;
    if (X509_set_subject_name(cert, name) != 1) goto out;
    if (X509_set_issuer_name(cert, name) != 1) goto out;

    if (X509_sign(cert, pkey, EVP_sha256()) == 0) goto out;

    if (SSL_CTX_use_certificate(ctx, cert) != 1) goto out;
    if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1) goto out;
    if (SSL_CTX_check_private_key(ctx) != 1) goto out;

    ok = 1;

out:
    if (!ok)
	_remrt_tls_log_errors("remrt TLS cert gen");
    if (name) X509_NAME_free(name);
    if (cert) X509_free(cert);
    if (pkey) EVP_PKEY_free(pkey);
    return ok;
}


/**
 * Create an SSL_CTX for use by the remrt *server* side.  Generates a
 * self-signed RSA-2048 certificate in memory — no external cert files
 * are required.  Callers should free with SSL_CTX_free().
 *
 * If @p certfile / @p keyfile are non-NULL the named PEM files are
 * loaded instead of generating a certificate, allowing sites with
 * existing PKI to use proper certificates.
 *
 * Returns a new SSL_CTX on success, NULL on failure.
 */
static SSL_CTX *
remrt_tls_server_ctx(const char *certfile, const char *keyfile)
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
	_remrt_tls_log_errors("SSL_CTX_new (server)");
	return NULL;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    if (certfile && keyfile) {
	if (SSL_CTX_use_certificate_chain_file(ctx, certfile) != 1
	    || SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) != 1
	    || SSL_CTX_check_private_key(ctx) != 1) {
	    _remrt_tls_log_errors("TLS: loading cert/key files");
	    SSL_CTX_free(ctx);
	    return NULL;
	}
    } else {
	/* Auto-generate a self-signed cert */
	if (!_remrt_tls_gen_selfsigned(ctx)) {
	    SSL_CTX_free(ctx);
	    return NULL;
	}
    }

    return ctx;
}


static int
remrt_tls_server_fingerprint(SSL_CTX *ctx, char *buffer, size_t buffer_size)
{
    if (!ctx)
	return 0;
    return _remrt_tls_fingerprint_string(SSL_CTX_get0_certificate(ctx),
	buffer, buffer_size);
}


/**
 * Advance capability negotiation and TLS setup without waiting for socket
 * readiness.
 *
 * @param ctx   SSL_CTX created by remrt_tls_server_ctx()
 * @param pc    pkg_conn just returned by pkg_getclient()
 *
 * Returns REMRT_TLS_OK on success, REMRT_TLS_PENDING when the caller should
 * retry after socket activity, and REMRT_TLS_ERR on failure.
 * On failure the caller must close the connection because negotiation or
 * handshake bytes may already have been exchanged.
 */
static int
remrt_tls_accept_step(SSL_CTX *ctx, struct pkg_conn *pc)
{
    struct remrt_tls_accept_state *state;
    SSL *ssl;
    int fd;
    int saved_flags = 0;
    int status;
    int transport_mismatch = 0;

    if (!ctx || !pc || pc->pkc_tx_kind != 0)
	return REMRT_TLS_ERR;
    fd = pkg_get_read_fd(pc);
    if (!_remrt_tls_set_nonblocking(fd, &saved_flags))
	return REMRT_TLS_ERR;

    state = (struct remrt_tls_accept_state *)pc->pkc_tls_ctx;
    if (!state) {
	state = (struct remrt_tls_accept_state *)bu_calloc(1,
	    sizeof(struct remrt_tls_accept_state),
	    "remrt TLS accept state");
	pkg_set_tls(pc, state, NULL, NULL, NULL,
	    _remrt_tls_accept_state_free);
    }

    if (state->stage == 0) {
	status = _remrt_tls_accept_transfer_step(fd, state->message,
	    sizeof(state->message), &state->transferred, 0);
	if (status != REMRT_TLS_OK)
	    goto accept_done;
	if (memcmp(state->message, remrt_tls_request,
		sizeof(state->message)) != 0) {
	    transport_mismatch = 1;
	    status = REMRT_TLS_ERR;
	    goto accept_done;
	}
	memcpy(state->message, remrt_tls_response, sizeof(state->message));
	state->transferred = 0;
	state->stage = 1;
    }
    if (state->stage == 1) {
	status = _remrt_tls_accept_transfer_step(fd, state->message,
	    sizeof(state->message), &state->transferred, 1);
	if (status != REMRT_TLS_OK)
	    goto accept_done;
	state->ssl = SSL_new(ctx);
	if (!state->ssl || SSL_set_fd(state->ssl, fd) != 1) {
	    _remrt_tls_log_errors("remrt TLS accept setup");
	    status = REMRT_TLS_ERR;
	    goto accept_done;
	}
	state->stage = 2;
    }

    status = SSL_accept(state->ssl);
    if (status == 1) {
	ssl = state->ssl;
	state->ssl = NULL;
	_remrt_tls_accept_state_free(state);
	pkg_set_tls(pc, ssl, _remrt_ssl_read, _remrt_ssl_write,
	    _remrt_ssl_pending,
	    _remrt_ssl_free);
	status = REMRT_TLS_OK;
    } else {
	int ssl_error = SSL_get_error(state->ssl, status);
	if (ssl_error == SSL_ERROR_WANT_READ ||
	    ssl_error == SSL_ERROR_WANT_WRITE) {
	    status = REMRT_TLS_PENDING;
	} else {
	    _remrt_tls_log_errors("remrt SSL_accept");
	    status = REMRT_TLS_ERR;
	}
    }

accept_done:
    _remrt_tls_restore_blocking(fd, saved_flags);
    if (transport_mismatch)
	(void)pkg_send(MSG_CMD_REPLY, REMRT_CMD_REPLY_TLS_REQUIRED,
	    sizeof(REMRT_CMD_REPLY_TLS_REQUIRED), pc);
    return status;
}

#else  /* RTSRV — compile only the client-side functions */

/**
 * Create an SSL_CTX for use by the rtsrv *client* side.  CA verification is
 * disabled because the server normally uses a per-session self-signed cert;
 * remrt_tls_connect() verifies its public-key fingerprint instead.  Callers
 * should free with SSL_CTX_free().
 *
 * Returns a new SSL_CTX on success, NULL on failure.
 */
static SSL_CTX *
remrt_tls_client_ctx(void)
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
	_remrt_tls_log_errors("SSL_CTX_new (client)");
	return NULL;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /* The connection-specific public-key pin is checked after handshake. */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    return ctx;
}


/**
 * Perform the TLS client-side handshake on a newly opened struct pkg_conn
 * and require its certificate public key to match expected_fingerprint.
 * On success the callbacks are set.
 *
 * @param ctx   SSL_CTX created by remrt_tls_client_ctx()
 * @param pc    pkg_conn just returned by pkg_open()
 * @param expected_fingerprint expected SHA-256 public-key fingerprint
 *
 * Returns REMRT_TLS_OK on success, REMRT_TLS_ERR on failure.
 */
static int
remrt_tls_connect(SSL_CTX *ctx, struct pkg_conn *pc,
	const char *expected_fingerprint)
{
    return _remrt_tls_handshake(ctx, pc, 0, expected_fingerprint);
}

#endif /* !RTSRV / RTSRV */

#endif /* REMRT_TLS_IMPL */

#endif /* HAVE_OPENSSL_SSL_H */

#endif /* REMRT_TLS_WRAP_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
