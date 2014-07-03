/*                          D M - P E X . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file dm-pex.h
 *
 */

#ifndef DM_PEX_H
#define DM_PEX_H

#include "common.h"

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width, 0..height
 */
#define	GED_TO_Xx(_dmp, x) ((int)(((x)/4096.0+0.5)*((struct pex_vars *)((_dmp)->dmr_vars))->width))
#define	GED_TO_Xy(_dmp, x) ((int)((0.5-(x)/4096.0)*((struct pex_vars *)((_dmp)->dmr_vars))->height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)((struct pex_vars *)((_dmp)->dmr_vars))->width - 0.5) * 4095))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)((struct pex_vars *)((_dmp)->dmr_vars))->height) * 4095))

#define TRY_DEPTHCUE 0
#define Pex_MV_O(_m) offsetof(struct modifiable_pex_vars, _m)

struct modifiable_pex_vars {
#if TRY_DEPTHCUE
    int cue;
#endif
    int perspective_mode;
    int dummy_perspective;
};

struct pex_vars {
    struct bu_list l;
    Display *dpy;
    Window win;
    Tk_Window xtkwin;
    int width;
    int height;
    int omx, omy;
    unsigned int mb_mask;
    int perspective_angle;
    XFontStruct *fontstruct;
    GC gc;
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
    Pixmap pix;
    int pix_width, pix_height;
#endif
    PEXRenderer renderer;
    PEXRendererAttributes rattrs;
    int is_monochrome;
    unsigned long black, gray, white, yellow, red, blue;
    unsigned long bd, bg, fg;   /* color of border, background, foreground */
    struct modifiable_pex_vars mvars;
};

__BEGIN_DECLS

extern void Pex_configure_window_shape();
extern void Pex_establish_perspective();
extern void Pex_set_perspective();
extern struct pex_vars head_pex_vars;

__END_DECLS

#endif /* DM_PEX_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
