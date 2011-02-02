/* $XConsortium: LookupCmap.c,v 1.10 94/04/17 20:16:11 rws Exp $ */

/* 
 
Copyright (c) 1989  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

/*
 * Author:  Donna Converse, MIT X Consortium
 */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <StdCmap.h>

static Status lookup();

/*
 * To create a standard colormap if one does not currently exist, or
 * replace the currently existing standard colormap, use 
 * XmuLookupStandardColormap().
 *
 * Given a screen, a visual, and a property, XmuLookupStandardColormap()
 * will determine the best allocation for the property under the specified
 * visual, and determine the whether to create a new colormap or to use
 * the default colormap of the screen.  It will call XmuStandardColormap()
 * to create the standard colormap.
 *
 * If replace is true, any previous definition of the property will be 
 * replaced.  If retain is true, the property and the colormap will be
 * made permanent for the duration of the server session.  However,
 * pre-existing property definitions which are not replaced cannot be made
 * permanent by a call to XmuLookupStandardColormap(); a request to retain 
 * resources pertains to newly created resources.
 *
 * Returns 0 on failure, non-zero on success.  A request to create a 
 * standard colormap upon a visual which cannot support such a map is
 * considered a failure.  An example of this would be requesting any
 * standard colormap property on a monochrome visual, or, requesting an
 * RGB_BEST_MAP on a display whose colormap size is 16.
 */

Status XmuLookupStandardColormap(dpy, screen, visualid, depth, property,
				 replace, retain)
    Display		*dpy;		/* specifies X server connection */
    int			screen; 	/* specifies screen of display */
    VisualID		visualid;	/* specifies the visual type */
    unsigned int	depth;		/* specifies  the visual type */
    Atom		property;	/* a standard colormap property */
    Bool		replace;	/* specifies whether to replace */
    Bool		retain;		/* specifies whether to retain */
{
    Display		*odpy;		/* original display connection */
    XStandardColormap	*colormap;	
    XVisualInfo		vinfo_template, *vinfo;	/* visual */
    long		vinfo_mask;
    unsigned long	r_max, g_max, b_max;	/* allocation */
    int			count;	
    Colormap		cmap;			/* colormap ID */
    Status		status = 0;


    /* Match the requested visual */

    vinfo_template.visualid = visualid;	
    vinfo_template.screen = screen;
    vinfo_template.depth = depth;
    vinfo_mask = VisualIDMask | VisualScreenMask | VisualDepthMask;
    if ((vinfo = XGetVisualInfo(dpy, vinfo_mask, &vinfo_template, &count)) ==
	NULL)
	return 0;

    /* Monochrome visuals have no standard maps */

    if (vinfo->colormap_size <= 2) {
	XFree((char *) vinfo);
	return 0;	
    }

    /* If the requested property already exists on this screen, and, 
     * if the replace flag has not been set to true, return success.
     * lookup() will remove a pre-existing map if replace is true.
     */

    if (lookup(dpy, screen, visualid, property, (XStandardColormap *) NULL,
	       replace) && !replace) {
	XFree((char *) vinfo);
	return 1;
    }

    /* Determine the best allocation for this property under the requested
     * visualid and depth, and determine whether or not to use the default
     * colormap of the screen.
     */

    if (!XmuGetColormapAllocation(vinfo, property, &r_max, &g_max, &b_max)) {
	XFree((char *) vinfo);
	return 0;
    }

    cmap = (property == XA_RGB_DEFAULT_MAP &&
	    visualid == XVisualIDFromVisual(DefaultVisual(dpy, screen)))
	? DefaultColormap(dpy, screen) : None;

    /* If retaining resources, open a new connection to the same server */

    if (retain) {
	odpy = dpy;
	if ((dpy = XOpenDisplay(XDisplayString(odpy))) == NULL) {
	    XFree((char *) vinfo);
	    return 0;
	}
    }

    /* Create the standard colormap */

    colormap = XmuStandardColormap(dpy, screen, visualid, depth, property,
				   cmap, r_max, g_max, b_max);

    /* Set the standard colormap property */

    if (colormap) {
	XGrabServer(dpy);

	if (lookup(dpy, screen, visualid, property, colormap, replace) &&
	    !replace) {
	    /* Someone has defined the property since we last looked.
	     * Since we will not replace it, release our own resources.
	     * If this is the default map, our allocations will be freed 
	     * when this connection closes.
	     */
	    if (colormap->killid == ReleaseByFreeingColormap)
		XFreeColormap(dpy, colormap->colormap);
	}
	else if (retain) {
		XSetCloseDownMode(dpy, RetainPermanent);
	}
	XUngrabServer(dpy);
	XFree((char *) colormap);
	status = 1;
    }

    if (retain)
	XCloseDisplay(dpy);
    XFree((char *) vinfo);
    return status;
}

/***************************************************************************/

/* Lookup a standard colormap property.  If the property is RGB_DEFAULT_MAP,
 * the visualid is used to determine whether the indicated standard colormap
 * exists.  If the map exists and replace is true, delete the resources used
 * by the map and remove the property.  Return true if the map exists,
 * or did exist and was deleted; return false if the map was not found.
 *
 * Note that this is not the way that a Status return is normally used.
 *
 * If new is not NULL, new points to an XStandardColormap structure which
 * describes a standard colormap of the specified property.  It will be made
 * a standard colormap of the screen if none already exists, or if replace 
 * is true.
 */

static Status lookup(dpy, screen, visualid, property, new, replace)
    Display		*dpy;		/* specifies display connection */
    int			screen;		/* specifies screen number */
    VisualID		visualid;	/* specifies visualid for std map */
    Atom		property;	/* specifies colormap property name */
    XStandardColormap	*new;		/* specifies a standard colormap */
    Bool		replace;	/* specifies whether to replace */
{
    register int	i;
    int			count;
    XStandardColormap	*stdcmaps, *s;
    Window		win = RootWindow(dpy, screen);

    /* The property does not already exist */

    if (! XGetRGBColormaps(dpy, win, &stdcmaps, &count, property)) {
	if (new)
	    XSetRGBColormaps(dpy, win, new, 1, property);
	return 0;
    }

    /* The property exists and is not describing the RGB_DEFAULT_MAP */

    if (property != XA_RGB_DEFAULT_MAP) {
	if (replace) {
	    XmuDeleteStandardColormap(dpy, screen, property);
	    if (new)
		XSetRGBColormaps(dpy, win, new, 1, property);
	}
	XFree((char *)stdcmaps);
	return 1;
    }

    /* The property exists and is RGB_DEFAULT_MAP */
    
    for (i=0, s=stdcmaps; (i < count) && (s->visualid != visualid); i++, s++)
	;

    /* No RGB_DEFAULT_MAP property matches the given visualid */

    if (i == count) {
	if (new) {
	    XStandardColormap	*m, *maps;

	    s = (XStandardColormap *) malloc((unsigned) ((count+1) * sizeof
					      (XStandardColormap)));

	    for (i = 0, m = s, maps = stdcmaps; i < count; i++, m++, maps++) {
		m->colormap   = maps->colormap;
		m->red_max    = maps->red_max;
		m->red_mult   = maps->red_mult;
		m->green_max  = maps->green_max;
		m->green_mult = maps->green_mult;
		m->blue_max   = maps->blue_max;
		m->blue_mult  = maps->blue_mult;
		m->base_pixel = maps->base_pixel;
		m->visualid   = maps->visualid;
		m->killid     = maps->killid;
	    }
	    m->colormap   = new->colormap;
	    m->red_max    = new->red_max;
	    m->red_mult   = new->red_mult;
	    m->green_max  = new->green_max;
	    m->green_mult = new->green_mult;
	    m->blue_max   = new->blue_max;
	    m->blue_mult  = new->blue_mult;
	    m->base_pixel = new->base_pixel;
	    m->visualid   = new->visualid;
	    m->killid     = new->killid;

	    XSetRGBColormaps(dpy, win, s, ++count, property);
	    free((char *) s);
	}
	XFree((char *) stdcmaps);
	return 0;
    }

    /* Found an RGB_DEFAULT_MAP property with a matching visualid */

    if (replace) {
	/* Free old resources first - we may need them, particularly in 
	 * the default colormap of the screen.  However, because of this,
	 * it is possible that we will destroy the old resource and fail 
	 * to create a new one if XmuStandardColormap() fails.
	 */

	if (count == 1) {
	    XmuDeleteStandardColormap(dpy, screen, property);
	    if (new)
		XSetRGBColormaps(dpy, win, new, 1, property);
	}
	else {
	    XStandardColormap	*map;

	    /* s still points to the matching standard colormap */

	    if (s->killid == ReleaseByFreeingColormap) {
		if ((s->colormap != None) &&
		    (s->colormap != DefaultColormap(dpy, screen)))
		    XFreeColormap(dpy, s->colormap);
	    }
	    else if (s->killid != None)
		XKillClient(dpy, s->killid);

	    map = (new) ? new : stdcmaps + --count;

	    s->colormap   = map->colormap;
	    s->red_max    = map->red_max;
	    s->red_mult   = map->red_mult;
	    s->green_max  = map->green_max;
	    s->green_mult = map->green_mult;
	    s->blue_max   = map->blue_max;
	    s->blue_mult  = map->blue_mult;
	    s->visualid   = map->visualid;
	    s->killid     = map->killid;

	    XSetRGBColormaps(dpy, win, stdcmaps, count, property);
	}
    }
    XFree((char *) stdcmaps);
    return 1;
}
