/* $XConsortium: DelCmap.c,v 1.2 94/04/17 20:15:58 converse Exp $ */

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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* To remove any standard colormap property, use XmuDeleteStandardColormap().
 * XmuDeleteStandardColormap() will remove the specified property from the
 * specified screen, releasing any resources used by the colormap(s) of the
 * property if possible.
 */

void XmuDeleteStandardColormap(dpy, screen, property)
    Display	*dpy;		/* specifies the X server to connect to */
    int		screen;		/* specifies the screen of the display */
    Atom	property;	/* specifies the standard colormap property */
{
    XStandardColormap	*stdcmaps, *s;
    int			count = 0;

    if (XGetRGBColormaps(dpy, RootWindow(dpy, screen), &stdcmaps, &count,
			 property))
    {
	for (s=stdcmaps; count > 0; count--, s++) {
	    if ((s->killid == ReleaseByFreeingColormap) &&
		(s->colormap != None) &&
		(s->colormap != DefaultColormap(dpy, screen)))
		XFreeColormap(dpy, s->colormap);
	    else if (s->killid != None)
		XKillClient(dpy, s->killid);
	}
	XDeleteProperty(dpy, RootWindow(dpy, screen), property);
	XFree((char *) stdcmaps);
	XSync(dpy, False);
    }
}

