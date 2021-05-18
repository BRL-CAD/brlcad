/*                        F B S E R V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup libstruct fb */
/** @{ */
/**
 *
 *  These are the Qt specific callbacks used for I/O between client
 *  and server.
 */
/** @} */

#include "common.h"

#include "bu/log.h"
#include "bu/vls.h"
#include "./fbserv.h"

/*
 * Communication error.  An error occurred on the PKG link.
 */
static void
comm_error(const char *str)
{
    bu_log("%s", str);
}

/* Check if we're already listening. */
int
qdm_is_listening(struct fbserv_obj *fbsp)
{
    if (fbsp->fbs_listener.fbsl_fd >= 0) {
	return 1;
    }
    return 0;
}

int
qdm_listen_on_port(struct fbserv_obj *fbsp, int available_port)
{
    struct bu_vls portname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&portname, "%d", available_port);
    fbsp->fbs_listener.fbsl_fd = pkg_permserver(bu_vls_cstr(&portname), 0, 0, comm_error);
    bu_vls_free(&portname);
    if (fbsp->fbs_listener.fbsl_fd >= 0)
	return 1;
    return 0;
}

void
qdm_open_server_handler(struct fbserv_obj *UNUSED(fbsp))
{
}

void
qdm_close_server_handler(struct fbserv_obj *UNUSED(fbsp))
{
}

void
qdm_open_client_handler(struct fbserv_obj *UNUSED(fbsp), int UNUSED(i), void *UNUSED(data))
{
}

void
qdm_close_client_handler(struct fbserv_obj *UNUSED(fbsp), int UNUSED(sub))
{
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
