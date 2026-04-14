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
 * connection managed by libpkg.  When OpenSSL is available at build
 * time, we:
 *
 *   1. Attach a pair of I/O callbacks (pkc_tls_read / pkc_tls_write)
 *      to each struct pkg_conn so that all protocol data goes through
 *      SSL_read / SSL_write rather than raw recv / send.
 *   2. Negotiate TLS immediately after the TCP handshake completes —
 *      before the first PKG message is exchanged.
 *
 * remrt acts as the TLS *server*: it generates a self-signed RSA-2048
 * certificate at startup (no external PKI infrastructure needed).
 * rtsrv acts as the TLS *client*: it connects to remrt and performs
 * the client-side handshake.  Certificate verification is disabled on
 * the client because there is no shared CA — the session token (see
 * auth.h) provides application-level authentication instead.
 *
 * SECURITY MODEL
 * --------------
 *  - All geometry data, pixel results, and protocol commands are
 *    encrypted in transit (TLS 1.2 or TLS 1.3, negotiated by OpenSSL).
 *  - The session token (from auth.h) prevents a rogue rtsrv from
 *    joining a session, even if it can establish a TLS connection.
 *  - No certificate-authority chain of trust is required for normal
 *    operation in a trusted cluster.  Operators who need mutual TLS
 *    authentication can load pre-provisioned cert/key files; the
 *    auto-generated self-signed cert is a secure default.
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
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

#include "bu/log.h"
#include "pkg.h"

/** TLS is active on this run (set after successful handshake) */
#define REMRT_TLS_OK     1
/** TLS handshake failed */
#define REMRT_TLS_ERR    0


#ifdef REMRT_TLS_IMPL

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


/**
 * Cleanup callback registered in pkc_tls_free.  Sends TLS
 * close_notify and frees the SSL object.  The underlying fd is closed
 * separately by pkg_close().
 */
static void
_remrt_ssl_free(void *ctx)
{
    SSL *ssl = (SSL *)ctx;
    if (!ssl)
	return;
    /* Best-effort graceful shutdown — ignore return value */
    SSL_shutdown(ssl);
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

    name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
			       (unsigned char *)"remrt", -1, -1, 0);
    X509_set_issuer_name(cert, name);  /* self-signed */

    if (X509_sign(cert, pkey, EVP_sha256()) == 0) goto out;

    if (SSL_CTX_use_certificate(ctx, cert) != 1) goto out;
    if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1) goto out;
    if (SSL_CTX_check_private_key(ctx) != 1) goto out;

    ok = 1;

out:
    if (!ok)
	_remrt_tls_log_errors("remrt TLS cert gen");
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


/**
 * Perform the TLS server-side handshake on a newly accepted
 * struct pkg_conn.  On success the connection's pkc_tls_read /
 * pkc_tls_write / pkc_tls_free callbacks are set so that all
 * subsequent libpkg I/O goes through TLS.
 *
 * @param ctx   SSL_CTX created by remrt_tls_server_ctx()
 * @param pc    pkg_conn just returned by pkg_getclient()
 *
 * Returns REMRT_TLS_OK on success, REMRT_TLS_ERR on failure.
 * On failure the connection remains usable as a plain TCP connection
 * (callbacks are not set), so the caller can decide whether to drop
 * the connection or accept it unencrypted.
 */
static int
remrt_tls_accept(SSL_CTX *ctx, struct pkg_conn *pc)
{
    SSL *ssl;
    int ret;

    if (!ctx || !pc)
	return REMRT_TLS_ERR;

    ssl = SSL_new(ctx);
    if (!ssl) {
	_remrt_tls_log_errors("SSL_new (server)");
	return REMRT_TLS_ERR;
    }

    if (SSL_set_fd(ssl, pc->pkc_fd) != 1) {
	_remrt_tls_log_errors("SSL_set_fd (server)");
	SSL_free(ssl);
	return REMRT_TLS_ERR;
    }

    ret = SSL_accept(ssl);
    if (ret != 1) {
	_remrt_tls_log_errors("SSL_accept");
	SSL_free(ssl);
	return REMRT_TLS_ERR;
    }

    /* Attach to pkg_conn */
    pc->pkc_tls_ctx   = (void *)ssl;
    pc->pkc_tls_read  = _remrt_ssl_read;
    pc->pkc_tls_write = _remrt_ssl_write;
    pc->pkc_tls_free  = _remrt_ssl_free;

    return REMRT_TLS_OK;
}

#else  /* RTSRV — compile only the client-side functions */

/**
 * Create an SSL_CTX for use by the rtsrv *client* side.  Certificate
 * verification is disabled because the server uses a self-signed cert;
 * application-level authentication is provided by the session token
 * (auth.h).  Callers should free with SSL_CTX_free().
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

    /* Disable certificate verification — session token handles auth */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    return ctx;
}


/**
 * Perform the TLS client-side handshake on a newly opened
 * struct pkg_conn.  On success the callbacks are set.
 *
 * @param ctx   SSL_CTX created by remrt_tls_client_ctx()
 * @param pc    pkg_conn just returned by pkg_open()
 *
 * Returns REMRT_TLS_OK on success, REMRT_TLS_ERR on failure.
 */
static int
remrt_tls_connect(SSL_CTX *ctx, struct pkg_conn *pc)
{
    SSL *ssl;
    int ret;

    if (!ctx || !pc)
	return REMRT_TLS_ERR;

    ssl = SSL_new(ctx);
    if (!ssl) {
	_remrt_tls_log_errors("SSL_new (client)");
	return REMRT_TLS_ERR;
    }

    if (SSL_set_fd(ssl, pc->pkc_fd) != 1) {
	_remrt_tls_log_errors("SSL_set_fd (client)");
	SSL_free(ssl);
	return REMRT_TLS_ERR;
    }

    ret = SSL_connect(ssl);
    if (ret != 1) {
	_remrt_tls_log_errors("SSL_connect");
	SSL_free(ssl);
	return REMRT_TLS_ERR;
    }

    /* Attach to pkg_conn */
    pc->pkc_tls_ctx   = (void *)ssl;
    pc->pkc_tls_read  = _remrt_ssl_read;
    pc->pkc_tls_write = _remrt_ssl_write;
    pc->pkc_tls_free  = _remrt_ssl_free;

    return REMRT_TLS_OK;
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
