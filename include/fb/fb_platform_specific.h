/*                        F B _ X 2 4 . H
 * BRL-CAD
 *
 * Copyright (c) 1994 Sun Microsystems, Inc. - All Rights Reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL SUN MICROSYSTEMS INC. BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING
 * OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF SUN
 * MICROSYSTEMS INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * SUN MICROSYSTEMS INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER
 * IS ON AN "AS IS" BASIS, AND SUN MICROSYSTEMS INC. HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */
/** @addtogroup if */
/** @{*/
/** @file fb_X24.h
 *
 * Structure holding information necessary for embedding a
 * framebuffer in an X11 parent window.  This is NOT public API
 * for libfb, and is not guaranteed to be stable from one
 * release to the next.
 *
 */
/** @} */

#ifdef FB_INTERNAL_X24_API
#ifdef IF_X

#include "common.h"
#include <X11/X.h>

struct X24_fb_info {
    Display *dpy;
    Window win;
    Window cwinp;
    Colormap cmap;
    XVisualInfo *vip;
    GC gc;
};

#endif /* IF_X */
#endif /* FB_INTERNAL_X24_API */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
