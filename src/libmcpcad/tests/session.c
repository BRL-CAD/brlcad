/*                      S E S S I O N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file session.c
 *
 * Unit tests for libmcpcad framing and session dispatch.
 *
 * Framing contract (length-prefixed, see mcpcad.h): MC + big-endian
 * u32 length + payload, in arbitrary chunk sizes.  Oversize verdicts
 * come from the header alone (deterministic), NUL payloads are
 * dropped, bad magic kills the stream.
 *
 * Session contract: one framed reply per message and per framing
 * fault, through the write callback; MCPCAD_ERR_PROTO propagates so
 * the transport knows to close.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "ged.h"
#include "mcpcad.h"


/* append one framed packet (raw payload, may contain NULs) to a vls */
static void
pkt_raw(struct bu_vls *out, const char *payload, size_t plen)
{
    size_t cur = bu_vls_strlen(out);

    bu_vls_extend(out, (unsigned int)(plen + MCPCAD_FRAME_HDRLEN + 1));
    {
	unsigned char *b = (unsigned char *)bu_vls_addr(out) + cur;
	b[0] = MCPCAD_FRAME_MAGIC0;
	b[1] = MCPCAD_FRAME_MAGIC1;
	b[2] = (unsigned char)((plen >> 24) & 0xff);
	b[3] = (unsigned char)((plen >> 16) & 0xff);
	b[4] = (unsigned char)((plen >> 8) & 0xff);
	b[5] = (unsigned char)(plen & 0xff);
	memcpy(b + MCPCAD_FRAME_HDRLEN, payload, plen);
    }
    bu_vls_setlen(out, cur + MCPCAD_FRAME_HDRLEN + plen);
}


static void
pkt(struct bu_vls *out, const char *payload)
{
    pkt_raw(out, payload, strlen(payload));
}


static int
expect_event(struct mcpcad_framebuf *fb, int code, const char *expected,
	     const char *what)
{
    char *msg = NULL;
    int rc = mcpcad_framebuf_next(fb, &msg);
    int failures = 0;

    if (rc != code) {
	printf("FAIL: %s: event code %d, expected %d\n", what, rc, code);
	failures++;
    }
    if (expected) {
	if (!msg || !BU_STR_EQUAL(msg, expected)) {
	    printf("FAIL: %s: msg \"%s\", expected \"%s\"\n",
		   what, msg ? msg : "(null)", expected);
	    failures++;
	}
    } else if (msg) {
	printf("FAIL: %s: unexpected msg \"%s\"\n", what, msg);
	failures++;
    }
    if (msg)
	bu_free(msg, "framebuf msg");
    return failures;
}


static int
frame_tests(void)
{
    int failures = 0;
    struct mcpcad_framebuf *fb = mcpcad_framebuf_create();
    struct bu_vls wire = BU_VLS_INIT_ZERO;

    if (!fb) {
	printf("FAIL: framebuf_create returned NULL\n");
	return 1;
    }

    /* no input, no events */
    failures += expect_event(fb, 0, NULL, "empty buffer");

    /* one whole packet in one chunk */
    pkt(&wire, "ls");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_event(fb, 1, "ls", "single packet");
    failures += expect_event(fb, 0, NULL, "drained after single");
    bu_vls_trunc(&wire, 0);

    /* header split 3+3, then payload split - no event until complete */
    pkt(&wire, "in sph1 sph 0 0 0 5");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire), 3);
    failures += expect_event(fb, 0, NULL, "split: 3 header bytes");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire) + 3, 3);
    failures += expect_event(fb, 0, NULL, "split: header only");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire) + 6, 5);
    failures += expect_event(fb, 0, NULL, "split: partial payload");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire) + 11,
			   bu_vls_strlen(&wire) - 11);
    failures += expect_event(fb, 1, "in sph1 sph 0 0 0 5", "split: complete");
    bu_vls_trunc(&wire, 0);

    /* three packets in one chunk - events pop in order */
    pkt(&wire, "ls");
    pkt(&wire, "title x");
    pkt(&wire, "kill a");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_event(fb, 1, "ls", "burst 1");
    failures += expect_event(fb, 1, "title x", "burst 2");
    failures += expect_event(fb, 1, "kill a", "burst 3");
    failures += expect_event(fb, 0, NULL, "burst drained");
    bu_vls_trunc(&wire, 0);

    /* oversize: verdict from the header, exactly one event, payload
     * skipped however it is chunked, next packet unharmed */
    {
	char *junk = (char *)bu_malloc(5000, "junk");
	size_t fed = 0;

	memset(junk, 'x', 5000);
	pkt_raw(&wire, junk, 5000);
	pkt(&wire, "ls");

	/* feed in 512-byte dribbles to prove chunk independence */
	while (fed < bu_vls_strlen(&wire)) {
	    size_t take = bu_vls_strlen(&wire) - fed;
	    if (take > 512)
		take = 512;
	    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire) + fed, take);
	    fed += take;
	}
	failures += expect_event(fb, MCPCAD_ERR_OVERFLOW, NULL, "oversize");
	failures += expect_event(fb, 1, "ls", "post-oversize recovery");
	failures += expect_event(fb, 0, NULL, "oversize drained");
	bu_free(junk, "junk");
	bu_vls_trunc(&wire, 0);
    }

    /* boundary: 4095-byte payload is the longest deliverable */
    {
	char *big = (char *)bu_malloc(MCPCAD_MAXLINE, "big");

	memset(big, 'a', MCPCAD_MAXLINE - 1);
	pkt_raw(&wire, big, MCPCAD_MAXLINE - 1);
	mcpcad_framebuf_append(fb, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
	{
	    char *msg = NULL;
	    int rc = mcpcad_framebuf_next(fb, &msg);
	    if (rc != 1 || !msg || strlen(msg) != MCPCAD_MAXLINE - 1) {
		printf("FAIL: boundary 4095: rc=%d\n", rc);
		failures++;
	    }
	    if (msg)
		bu_free(msg, "framebuf msg");
	}
	bu_vls_trunc(&wire, 0);

	memset(big, 'b', MCPCAD_MAXLINE);
	pkt_raw(&wire, big, MCPCAD_MAXLINE);
	mcpcad_framebuf_append(fb, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
	failures += expect_event(fb, MCPCAD_ERR_OVERFLOW, NULL,
				 "boundary 4096 rejected");
	failures += expect_event(fb, 0, NULL, "boundary drained");
	bu_free(big, "big");
	bu_vls_trunc(&wire, 0);
    }

    /* NUL inside payload: dropped, successor unharmed */
    pkt_raw(&wire, "ls\0junk", 7);
    pkt(&wire, "ls");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_event(fb, MCPCAD_ERR_PARSE, NULL, "NUL payload");
    failures += expect_event(fb, 1, "ls", "post-NUL recovery");
    bu_vls_trunc(&wire, 0);

    /* bad magic: PROTO once, then the stream is dead - even a valid
     * packet afterwards is ignored */
    mcpcad_framebuf_append(fb, "GET / HTTP/1.1\r\n", 16);
    failures += expect_event(fb, MCPCAD_ERR_PROTO, NULL, "bad magic");
    pkt(&wire, "ls");
    mcpcad_framebuf_append(fb, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_event(fb, 0, NULL, "dead stream ignores input");
    bu_vls_trunc(&wire, 0);

    mcpcad_framebuf_destroy(fb);
    mcpcad_framebuf_destroy(NULL);  /* no-op */
    bu_vls_free(&wire);
    return failures;
}


/* ---- session tests: capture callback IS the transport ---- */

static void
capture_write(const char *data, size_t len, void *ctx)
{
    struct bu_vls *sink = (struct bu_vls *)ctx;
    size_t cur = bu_vls_strlen(sink);

    bu_vls_extend(sink, (unsigned int)(len + 1));
    memcpy(bu_vls_addr(sink) + cur, data, len);
    bu_vls_setlen(sink, cur + len);
}


/* pop one framed reply off the front of the sink into body */
static int
pop_reply(struct bu_vls *sink, struct bu_vls *body, const char *what)
{
    const unsigned char *b = (const unsigned char *)bu_vls_cstr(sink);
    size_t avail = bu_vls_strlen(sink);
    size_t blen, i;

    bu_vls_trunc(body, 0);
    if (avail < MCPCAD_FRAME_HDRLEN ||
	b[0] != MCPCAD_FRAME_MAGIC0 || b[1] != MCPCAD_FRAME_MAGIC1) {
	printf("FAIL: %s: reply lacks valid frame header\n", what);
	return 1;
    }
    blen = ((size_t)b[2] << 24) | ((size_t)b[3] << 16) |
	   ((size_t)b[4] << 8) | (size_t)b[5];
    if (avail < MCPCAD_FRAME_HDRLEN + blen) {
	printf("FAIL: %s: reply truncated (have %zu, need %zu)\n",
	       what, avail, MCPCAD_FRAME_HDRLEN + blen);
	return 1;
    }
    for (i = 0; i < blen; i++)
	bu_vls_putc(body, (char)b[MCPCAD_FRAME_HDRLEN + i]);
    bu_vls_nibble(sink, (b_off_t)(MCPCAD_FRAME_HDRLEN + blen));
    return 0;
}


static int
expect_reply(struct bu_vls *sink, const char *status_prefix,
	     const char *contains, const char *what)
{
    struct bu_vls body = BU_VLS_INIT_ZERO;
    int failures = 0;

    if (pop_reply(sink, &body, what)) {
	bu_vls_free(&body);
	return 1;
    }
    if (strncmp(bu_vls_cstr(&body), status_prefix, strlen(status_prefix))) {
	printf("FAIL: %s: body \"%s\" lacks status prefix \"%s\"\n",
	       what, bu_vls_cstr(&body), status_prefix);
	failures++;
    }
    if (contains && !strstr(bu_vls_cstr(&body), contains)) {
	printf("FAIL: %s: body \"%s\" lacks \"%s\"\n",
	       what, bu_vls_cstr(&body), contains);
	failures++;
    }
    bu_vls_free(&body);
    return failures;
}


static int
session_tests(void)
{
    int failures = 0;
    char dbpath[MAXPATHLEN] = {0};
    struct ged *gedp;
    struct bu_vls sink = BU_VLS_INIT_ZERO;
    struct bu_vls wire = BU_VLS_INIT_ZERO;
    struct mcpcad_session *s;

    bu_dir(dbpath, MAXPATHLEN, BU_DIR_TEMP, "mcpcad_test_session.g", NULL);
    bu_file_delete(dbpath);
    gedp = ged_open("db", dbpath, 0);
    if (!gedp) {
	printf("INTERNAL: could not create scratch db\n");
	return 1;
    }

    s = mcpcad_session_create(gedp, capture_write, &sink);
    if (!s) {
	printf("FAIL: session_create returned NULL\n");
	ged_close(gedp);
	return 1;
    }

    /* happy path */
    pkt(&wire, "in s1 sph 0 0 0 5");
    mcpcad_session_input(s, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_reply(&sink, "OK\n", NULL, "create reply");
    bu_vls_trunc(&wire, 0);

    pkt(&wire, "ls");
    mcpcad_session_input(s, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_reply(&sink, "OK\n", "s1", "ls reply");
    bu_vls_trunc(&wire, 0);

    /* byte-at-a-time delivery: identical outcome */
    pkt(&wire, "ls");
    {
	size_t i;
	for (i = 0; i < bu_vls_strlen(&wire); i++)
	    mcpcad_session_input(s, bu_vls_cstr(&wire) + i, 1);
    }
    failures += expect_reply(&sink, "OK\n", "s1", "byte-at-a-time ls");
    bu_vls_trunc(&wire, 0);

    /* GED error propagates, session survives */
    pkt(&wire, "frobnicate now");
    mcpcad_session_input(s, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_reply(&sink, "ERR ", "frobnicate", "unknown cmd");
    bu_vls_trunc(&wire, 0);

    /* parse rejection: control char in payload */
    pkt(&wire, "ls \x01");
    mcpcad_session_input(s, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_reply(&sink, "ERR -4", NULL, "control char");
    bu_vls_trunc(&wire, 0);

    /* empty payload */
    pkt(&wire, "");
    mcpcad_session_input(s, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_reply(&sink, "ERR -4", NULL, "empty payload");
    bu_vls_trunc(&wire, 0);

    /* declared-oversize: deterministic ERR -3, then recovery */
    {
	char *junk = (char *)bu_malloc(5000, "junk");
	memset(junk, 'x', 5000);
	pkt_raw(&wire, junk, 5000);
	pkt(&wire, "ls");
	mcpcad_session_input(s, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
	failures += expect_reply(&sink, "ERR -3", "maximum", "oversize");
	failures += expect_reply(&sink, "OK\n", "s1", "post-oversize ls");
	bu_free(junk, "junk");
	bu_vls_trunc(&wire, 0);
    }

    /* still alive and correct after the abuse */
    pkt(&wire, "ls");
    mcpcad_session_input(s, bu_vls_cstr(&wire), bu_vls_strlen(&wire));
    failures += expect_reply(&sink, "OK\n", "s1", "post-abuse ls");
    bu_vls_trunc(&wire, 0);

    mcpcad_session_destroy(s);

    /* a NON-protocol client (e.g. curl sending HTTP): ERR -5 reply
     * and session_input tells the transport to hang up */
    s = mcpcad_session_create(gedp, capture_write, &sink);
    {
	int rc = mcpcad_session_input(s, "GET / HTTP/1.1\r\n", 16);
	if (rc != MCPCAD_ERR_PROTO) {
	    printf("FAIL: HTTP client: session_input returned %d, "
		   "expected MCPCAD_ERR_PROTO\n", rc);
	    failures++;
	}
	failures += expect_reply(&sink, "ERR -5", "closing", "HTTP client");
    }
    mcpcad_session_destroy(s);
    mcpcad_session_destroy(NULL);  /* no-op */

    ged_close(gedp);
    bu_file_delete(dbpath);
    bu_vls_free(&sink);
    bu_vls_free(&wire);
    return failures;
}


int
main(int argc, char *argv[])
{
    int failures = 0;

    bu_setprogname(argv[0]);

    if (argc > 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    failures += frame_tests();
    failures += session_tests();

    printf("session tests: %d failures\n", failures);
    return failures > 255 ? 255 : failures;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
