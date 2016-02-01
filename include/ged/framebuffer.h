/*                  F R A M E B U F F E R . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @addtogroup ged_framebuffer
 *
 * Geometry EDiting Library Database Framebuffer Related Functions.
 *
 */
/** @{ */
/** @file ged/framebuffer.h */

#ifndef GED_FRAMEBUFFER_H
#define GED_FRAMEBUFFER_H

#include "common.h"
#include "fb.h"
#include "ged/defines.h"

__BEGIN_DECLS

#define GED_CHECK_FBSERV(_gedp, _flags) \
    if (_gedp->ged_fbsp == FBSERV_OBJ_NULL) { \
	int ged_check_view_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_view_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A framebuffer server object does not exist."); \
	} \
	return (_flags); \
    }

#define GED_CHECK_FBSERV_FBP(_gedp, _flags) \
    if (_gedp->ged_fbsp->fbs_fbp == FB_NULL) { \
	int ged_check_view_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_view_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A framebuffer IO structure does not exist."); \
	} \
	return (_flags); \
    }


/**
 * Fb2pix writes a framebuffer image to a .pix file.
 */
GED_EXPORT extern int ged_fb2pix(struct ged *gedp, int argc, const char *argv[]);

/**
 * Fclear clears a framebuffer.
 */
GED_EXPORT extern int ged_fbclear(struct ged *gedp, int argc, const char *argv[]);


/**
 * Pix2fb reads a pix file into a framebuffer.
 */
GED_EXPORT extern int ged_pix2fb(struct ged *gedp, int argc, const char *argv[]);



__END_DECLS

#endif /* GED_FRAMEBUFFER_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
