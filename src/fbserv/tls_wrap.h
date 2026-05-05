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
 * fbserv and perform the client-side handshake.  Certificate verification
 * is disabled on the client because there is no shared CA — the session
 * token (see auth.h) provides application-level authentication instead.
 *
 * SECURITY MODEL
 * --------------
 *  - All framebuffer data, pixel writes, and protocol commands are
 *    encrypted in transit (TLS 1.2 or TLS 1.3, negotiated by OpenSSL).
 *  - The session token (from auth.h) prevents a rogue client from
 *    joining a session, even if it can establish a TLS connection.
 *  - The cert/key can be overridden via FBSERV_TLS_CERT / FBSERV_TLS_KEY
 *    environment variables.
 *
 * GUARD MACRO
 * -----------
 * To avoid "defined but not used" warnings, the implementation
 * functions are wrapped in `#ifdef FBSERV_TLS_IMPL`.  Define that
 * macro before including this header in exactly one translation unit
 * per binary.  The FBSERV_TLS_CLIENT define selects client-only paths;
 * without it the server-side functions are compiled.
 */

#ifndef FBSERV_TLS_WRAP_H
#define FBSERV_TLS_WRAP_H

#include "common.h"

#ifdef HAVE_OPENSSL_SSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

#include "bu/log.h"
#include "pkg.h"

/** TLS handshake succeeded */
#define FBSERV_TLS_OK     1
/** TLS handshake failed */
#define FBSERV_TLS_ERR    0


#ifdef FBSERV_TLS_IMPL

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


/**
 * Cleanup callback registered in pkc_tls_free.  Sends TLS
 * close_notify and frees the SSL object.  The underlying fd is closed
 * separately by pkg_close().
 */
static void
_fbserv_ssl_free(void *ctx)
{
    SSL *ssl = (SSL *)ctx;
    if (!ssl)
	return;
    SSL_shutdown(ssl);
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


/* -----------------------------------------------------------------------
 * Server-side functions (not compiled when FBSERV_TLS_CLIENT is set)
 * --------------------------------------------------------------------- */

#ifndef FBSERV_TLS_CLIENT

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

    name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
			       (unsigned char *)"fbserv", -1, -1, 0);
    X509_set_issuer_name(cert, name);

    if (X509_sign(cert, pkey, EVP_sha256()) == 0) goto out;

    if (SSL_CTX_use_certificate(ctx, cert) != 1) goto out;
    if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1) goto out;
    if (SSL_CTX_check_private_key(ctx) != 1) goto out;

    ok = 1;

out:
    if (!ok)
	_fbserv_tls_log_errors("fbserv TLS cert gen");
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


/**
 * Perform the TLS server-side handshake on a newly accepted
 * struct pkg_conn.  On success the connection's pkc_tls_read /
 * pkc_tls_write / pkc_tls_free callbacks are set so that all
 * subsequent libpkg I/O goes through TLS.
 *
 * Returns FBSERV_TLS_OK on success, FBSERV_TLS_ERR on failure.
 */
static int
fbserv_tls_accept(SSL_CTX *ctx, struct pkg_conn *pc)
{
    SSL *ssl;
    int ret;

    if (!ctx || !pc)
	return FBSERV_TLS_ERR;

    ssl = SSL_new(ctx);
    if (!ssl) {
	_fbserv_tls_log_errors("SSL_new (fbserv server)");
	return FBSERV_TLS_ERR;
    }

    if (SSL_set_fd(ssl, pkg_get_read_fd(pc)) != 1) {
	_fbserv_tls_log_errors("SSL_set_fd (fbserv server)");
	SSL_free(ssl);
	return FBSERV_TLS_ERR;
    }

    ret = SSL_accept(ssl);
    if (ret != 1) {
	_fbserv_tls_log_errors("fbserv SSL_accept");
	SSL_free(ssl);
	return FBSERV_TLS_ERR;
    }

    pkg_set_tls(pc, (void *)ssl,
		_fbserv_ssl_read, _fbserv_ssl_write, _fbserv_ssl_free);

    return FBSERV_TLS_OK;
}

#else  /* FBSERV_TLS_CLIENT — compile only the client-side functions */

/**
 * Create an SSL_CTX for the fbserv *client* side.  Certificate
 * verification is disabled because the server uses a self-signed cert;
 * application-level authentication is provided by the session token
 * (auth.h).
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
fbserv_tls_connect(SSL_CTX *ctx, struct pkg_conn *pc)
{
    SSL *ssl;
    int ret;

    if (!ctx || !pc)
	return FBSERV_TLS_ERR;

    ssl = SSL_new(ctx);
    if (!ssl) {
	_fbserv_tls_log_errors("SSL_new (fbserv client)");
	return FBSERV_TLS_ERR;
    }

    if (SSL_set_fd(ssl, pkg_get_read_fd(pc)) != 1) {
	_fbserv_tls_log_errors("SSL_set_fd (fbserv client)");
	SSL_free(ssl);
	return FBSERV_TLS_ERR;
    }

    ret = SSL_connect(ssl);
    if (ret != 1) {
	_fbserv_tls_log_errors("fbserv SSL_connect");
	SSL_free(ssl);
	return FBSERV_TLS_ERR;
    }

    pkg_set_tls(pc, (void *)ssl,
		_fbserv_ssl_read, _fbserv_ssl_write, _fbserv_ssl_free);

    return FBSERV_TLS_OK;
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
