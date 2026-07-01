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
 * Per-client session: framing + parse + exec + reply, behind a
 * transport-agnostic write callback.
 *
 * This is the seam every transport plugs into - the TCP scaffold,
 * the future libpkg/stdio transport, and the unit tests' capture
 * callback all feed bytes in and receive reply bytes out through
 * the same two entry points.  Nothing below this file knows what
 * the transport is.
 *
 */

#include "common.h"

#include "bu/malloc.h"
#include "bu/vls.h"
#include "mcpcad.h"


struct mcpcad_session {
    struct ged *gedp;            /* not owned - host app manages it */
    struct mcpcad_framebuf *fb;  /* owned */
    mcpcad_write_t wcb;
    void *wctx;
};


struct mcpcad_session *
mcpcad_session_create(struct ged *gedp, mcpcad_write_t wcb, void *wctx)
{
    struct mcpcad_session *s;

    if (!wcb)
	return NULL;

    BU_GET(s, struct mcpcad_session);
    s->gedp = gedp;
    s->fb = mcpcad_framebuf_create();
    s->wcb = wcb;
    s->wctx = wctx;
    return s;
}


void
mcpcad_session_destroy(struct mcpcad_session *s)
{
    if (!s)
	return;
    mcpcad_framebuf_destroy(s->fb);
    BU_PUT(s, struct mcpcad_session);
}


/* emit one framed reply: MC + big-endian length + body, where body
 * is "OK\n" or "ERR <code>\n" followed by the payload */
static void
session_reply(struct mcpcad_session *s, int status,
	      const struct bu_vls *payload)
{
    struct bu_vls body = BU_VLS_INIT_ZERO;
    unsigned char hdr[MCPCAD_FRAME_HDRLEN];
    size_t blen;

    if (status == 0)
	bu_vls_strcpy(&body, "OK\n");
    else
	bu_vls_printf(&body, "ERR %d\n", status);

    if (payload && bu_vls_strlen(payload) > 0)
	bu_vls_vlscat(&body, payload);

    blen = bu_vls_strlen(&body);
    hdr[0] = MCPCAD_FRAME_MAGIC0;
    hdr[1] = MCPCAD_FRAME_MAGIC1;
    hdr[2] = (unsigned char)((blen >> 24) & 0xff);
    hdr[3] = (unsigned char)((blen >> 16) & 0xff);
    hdr[4] = (unsigned char)((blen >> 8) & 0xff);
    hdr[5] = (unsigned char)(blen & 0xff);

    s->wcb((const char *)hdr, sizeof(hdr), s->wctx);
    s->wcb(bu_vls_cstr(&body), blen, s->wctx);
    bu_vls_free(&body);
}


/* one diagnostic reply per framing fault */
static void
session_fault(struct mcpcad_session *s, int code)
{
    struct bu_vls diag = BU_VLS_INIT_ZERO;

    switch (code) {
	case MCPCAD_ERR_OVERFLOW:
	    bu_vls_strcpy(&diag, "declared payload length exceeds maximum");
	    break;
	case MCPCAD_ERR_PARSE:
	    bu_vls_strcpy(&diag, "payload contains NUL bytes");
	    break;
	case MCPCAD_ERR_PROTO:
	    bu_vls_strcpy(&diag, "not an mcpcad frame stream; closing");
	    break;
	default:
	    bu_vls_strcpy(&diag, "framing fault");
	    break;
    }
    session_reply(s, code, &diag);
    bu_vls_free(&diag);
}


int
mcpcad_session_input(struct mcpcad_session *s, const char *data, size_t len)
{
    if (!s || !data || !len)
	return 0;

    mcpcad_framebuf_append(s->fb, data, len);

    for (;;) {
	char *msg = NULL;
	int rc = mcpcad_framebuf_next(s->fb, &msg);

	if (rc == 0)
	    break;

	if (rc != 1) {
	    /* framing fault: reply, and on a dead stream tell the
	     * transport to hang up */
	    session_fault(s, rc);
	    if (rc == MCPCAD_ERR_PROTO)
		return MCPCAD_ERR_PROTO;
	    continue;
	}

	{
	    struct bu_vls payload = BU_VLS_INIT_ZERO;
	    struct mcpcad_cmd *c = mcpcad_cmd_parse(msg);

	    if (!c) {
		bu_vls_strcpy(&payload, "command rejected: empty, too "
			      "long, or contains control characters");
		session_reply(s, MCPCAD_ERR_PARSE, &payload);
	    } else {
		int status = mcpcad_cmd_exec(s->gedp, c, &payload);
		session_reply(s, status, &payload);
		mcpcad_cmd_free(c);
	    }

	    bu_vls_free(&payload);
	    bu_free(msg, "framebuf msg");
	}
    }

    return 0;
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
