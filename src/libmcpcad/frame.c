/*                        F R A M E . C
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
/** @file frame.c
 *
 * Length-prefixed message framing (see mcpcad.h for the wire format).
 *
 * A transport read() delivers bytes, not messages - one read may
 * carry half a header or three whole messages.  This layer runs a
 * small state machine over the byte stream:
 *
 *   HEADER --(magic ok, len ok)--> PAYLOAD --(len bytes)--> HEADER
 *      |            |
 *      |            +--(len too big)--> SKIP --(len bytes)--> HEADER
 *      +--(bad magic)--> DEAD (terminal: no resync without delimiters)
 *
 * Because the length is declared up front, the oversize verdict is
 * deterministic - it can never depend on how the kernel chunked the
 * stream (the flaw that killed the newline-framed v0).
 *
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/vls.h"
#include "mcpcad.h"


enum fb_state {
    FB_HEADER,   /* collecting the 6-byte frame header */
    FB_PAYLOAD,  /* collecting 'need' payload bytes */
    FB_SKIP,     /* discarding 'need' bytes of an oversized payload */
    FB_DEAD      /* bad magic; stream unrecoverable */
};

/* completed messages and framing faults, queued in arrival order */
struct fb_event {
    int code;             /* 1 = message, else MCPCAD_ERR_* */
    char *msg;            /* payload for code 1, NULL otherwise */
    struct fb_event *next;
};

struct mcpcad_framebuf {
    struct bu_vls buf;    /* undecoded bytes */
    enum fb_state state;
    size_t need;          /* payload bytes outstanding (PAYLOAD/SKIP) */
    struct fb_event *head, *tail;
};


static void
fb_enqueue(struct mcpcad_framebuf *fb, int code, char *msg)
{
    struct fb_event *ev;

    BU_GET(ev, struct fb_event);
    ev->code = code;
    ev->msg = msg;
    ev->next = NULL;
    if (fb->tail)
	fb->tail->next = ev;
    else
	fb->head = ev;
    fb->tail = ev;
}


struct mcpcad_framebuf *
mcpcad_framebuf_create(void)
{
    struct mcpcad_framebuf *fb;

    BU_GET(fb, struct mcpcad_framebuf);
    bu_vls_init(&fb->buf);
    fb->state = FB_HEADER;
    fb->need = 0;
    fb->head = fb->tail = NULL;
    return fb;
}


void
mcpcad_framebuf_destroy(struct mcpcad_framebuf *fb)
{
    char *msg = NULL;

    if (!fb)
	return;
    /* drain undelivered events, freeing any attached payloads */
    while (mcpcad_framebuf_next(fb, &msg) != 0) {
	if (msg)
	    bu_free(msg, "framebuf msg");
	msg = NULL;
    }
    bu_vls_free(&fb->buf);
    BU_PUT(fb, struct mcpcad_framebuf);
}


/* Binary-safe append.  bu_vls_strncat() calls strlen() on its input
 * and so requires NUL-terminated strings - transport reads are raw
 * bytes.  Extend, copy exactly len bytes, keep the buffer
 * NUL-terminated for any cstr() consumer. */
static void
fb_cat(struct mcpcad_framebuf *fb, const char *data, size_t len)
{
    size_t cur = bu_vls_strlen(&fb->buf);

    bu_vls_extend(&fb->buf, (unsigned int)(len + 1));
    memcpy(bu_vls_addr(&fb->buf) + cur, data, len);
    bu_vls_setlen(&fb->buf, cur + len);
    bu_vls_addr(&fb->buf)[cur + len] = '\0';
}


void
mcpcad_framebuf_append(struct mcpcad_framebuf *fb, const char *data,
		       size_t len)
{
    if (!fb || !data || !len || fb->state == FB_DEAD)
	return;

    fb_cat(fb, data, len);

    /* decode as much of the buffer as the state machine can consume */
    for (;;) {
	const unsigned char *b =
	    (const unsigned char *)bu_vls_cstr(&fb->buf);
	size_t avail = bu_vls_strlen(&fb->buf);

	if (fb->state == FB_HEADER) {
	    if (avail < MCPCAD_FRAME_HDRLEN)
		break;

	    if (b[0] != MCPCAD_FRAME_MAGIC0 || b[1] != MCPCAD_FRAME_MAGIC1) {
		/* not our protocol; without delimiters there is no
		 * resync point - everything from here on is noise */
		fb_enqueue(fb, MCPCAD_ERR_PROTO, NULL);
		fb->state = FB_DEAD;
		bu_vls_trunc(&fb->buf, 0);
		break;
	    }

	    fb->need = ((size_t)b[2] << 24) | ((size_t)b[3] << 16) |
		       ((size_t)b[4] << 8) | (size_t)b[5];
	    bu_vls_nibble(&fb->buf, MCPCAD_FRAME_HDRLEN);

	    if (fb->need >= MCPCAD_MAXLINE) {
		/* verdict from the header alone - deterministic
		 * regardless of how the payload bytes get chunked */
		fb_enqueue(fb, MCPCAD_ERR_OVERFLOW, NULL);
		fb->state = FB_SKIP;
	    } else {
		fb->state = FB_PAYLOAD;
	    }
	    continue;
	}

	if (fb->state == FB_PAYLOAD) {
	    char *msg;

	    if (avail < fb->need)
		break;

	    if (memchr(b, '\0', fb->need)) {
		/* a NUL would silently truncate the command at parse
		 * time - executing a hidden-prefix command is never
		 * safe, so the whole payload is dropped */
		fb_enqueue(fb, MCPCAD_ERR_PARSE, NULL);
	    } else {
		msg = (char *)bu_malloc(fb->need + 1, "framebuf msg");
		memcpy(msg, b, fb->need);
		msg[fb->need] = '\0';
		fb_enqueue(fb, 1, msg);
	    }
	    bu_vls_nibble(&fb->buf, (b_off_t)fb->need);
	    fb->need = 0;
	    fb->state = FB_HEADER;
	    continue;
	}

	/* FB_SKIP: count down the oversized payload without storing */
	{
	    size_t take = (avail < fb->need) ? avail : fb->need;
	    bu_vls_nibble(&fb->buf, (b_off_t)take);
	    fb->need -= take;
	    if (fb->need > 0)
		break;
	    fb->state = FB_HEADER;
	}
    }
}


int
mcpcad_framebuf_next(struct mcpcad_framebuf *fb, char **msgp)
{
    struct fb_event *ev;
    int code;

    if (msgp)
	*msgp = NULL;
    if (!fb || !fb->head)
	return 0;

    ev = fb->head;
    fb->head = ev->next;
    if (!fb->head)
	fb->tail = NULL;

    code = ev->code;
    if (msgp)
	*msgp = ev->msg;
    else if (ev->msg)
	bu_free(ev->msg, "framebuf msg");

    BU_PUT(ev, struct fb_event);
    return code;
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
