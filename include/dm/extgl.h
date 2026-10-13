/*                       E X T G L . H
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
/** @addtogroup libdm */
/** @{ */
/** @file dm/extgl.h
 *
 * Host callbacks for the toolkit-neutral external OpenGL display manager.
 */

#ifndef DM_EXTGL_H
#define DM_EXTGL_H

#include "common.h"

#include <stdint.h>

__BEGIN_DECLS

#define DM_EXTGL_API 1

/**
 * Description of an OpenGL context owned by an application or GUI toolkit.
 * The context must support OpenGL compatibility-profile drawing.
 * The structure is copied by dm_open(), so the caller may release the
 * descriptor after that call.  The callback data and its OpenGL context must
 * remain valid until dm_close().
 */
struct dm_extgl_context {
    uint32_t api_version;
    void *data;

    /**
     * Make the host context current and bind its draw and read framebuffers.
     * Return zero on success.
     */
    int (*make_current)(void *data);

    /** Report physical framebuffer dimensions.  Return zero on success. */
    int (*get_framebuffer_size)(void *data, int *width, int *height);

    /**
     * Present a completed frame.  May be NULL when the host presents after
     * its drawing callback returns.
     */
    int (*present)(void *data);

    /**
     * Return a bu_malloc() allocated RGB or RGBA image.  May be NULL to use
     * libdm's capture implementation, which reads from the active draw buffer.
     */
    int (*get_display_image)(void *data, unsigned char **image, int flip,
                             int alpha);

    /** Draw at the current OpenGL raster position.  May be NULL. */
    int (*draw_string)(void *data, const char *text, int size);

    /** Report text dimensions in physical pixels.  May be NULL. */
    int (*string_bbox)(void *data, const char *text, int size,
                       double *width, double *height);
};

__END_DECLS

#endif /* DM_EXTGL_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
