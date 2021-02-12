/*                       I F _ Q T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file if_qt.cpp
 *
 * A Qt Frame Buffer.
 *
 */
/** @} */

#include "common.h"
#include "./fb_qt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#include "dm.h"
#include "private.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "bu/str.h"

extern struct fb qt_interface;

class QMainWindow: public QWindow {
    public:
	QMainWindow(struct fb *ifp, QImage *image, QWindow *parent = 0);
	~QMainWindow();

	virtual void render(QPainter *painter);
	public slots:
	    void renderNow();

    protected:
	bool event(QEvent *event);

	void resizeEvent(QResizeEvent *event);
	void exposeEvent(QExposeEvent *event);

    private:
	struct fb *ifp;
	QImage *image;
	QBackingStore *m_backingStore;
	bool m_update_pending;
};

struct qtinfo {
    QApplication *qapp;
    QWindow *win;
    QImage *qi_image;
    void **qi_parent_img;
    QPainter *qi_painter;

    int alive;
    int *drawFb;

    unsigned long qi_mode;
    unsigned long qi_flags;

    unsigned char *qi_mem;
    unsigned char *qi_pix;

    int qi_iwidth;
    int qi_iheight;

    int qi_ilf;		/* Image coordinate of LLHC image */
    int qi_ibt;		/* pixel */
    int qi_irt;		/* Image coordinate of URHC image */
    int qi_itp;		/* pixel */

    int qi_ilf_w;	/* Width of leftmost image pixels */
    int qi_irt_w;	/* Width of rightmost image pixels */
    int qi_ibt_h;	/* Height of bottommost image pixels */
    int qi_itp_h;	/* Height of topmost image pixels */

    int qi_qwidth;	/* Width of Qwindow */
    int qi_qheight;	/* Height of Qwindow */

    int qi_xlf;		/* X-coord of leftmost pixels */
    int qi_xrt;		/* X-coord of rightmost pixels */
    int qi_xtp;		/* Y-coord of topmost pixels */
    int qi_xbt;		/* Y-coord of bottommost pixels */

    ColorMap *qi_rgb_cmap;	/* User's libfb colormap */
    unsigned char *qi_redmap;
    unsigned char *qi_blumap;
    unsigned char *qi_grnmap;
};

#define QI(ptr) ((struct qtinfo *)((ptr)->i->u1.p))
#define QI_SET(ptr, val) ((ptr)->i->u1.p) = (char *) val;

/* Flags in qi_flags */
#define FLG_LINCMAP 0x10	/* We're using a linear colormap */
#define FLG_INIT    0x40	/* Display is fully initialized */

/* Mode flags for open */
#define MODE1_MASK	(1<<1)
#define MODE1_TRANSIENT	(0<<1)
#define MODE1_LINGERING (1<<1)

#define MODEV_MASK	(7<<1)

#define MODE10_MASK	(1<<10)
#define MODE10_MALLOC	(0<<10)
#define MODE10_SHARED	(1<<10)

#define MODE11_MASK	(1<<11)
#define MODE11_NORMAL	(0<<11)
#define MODE11_ZAP	(1<<11)

static struct modeflags {
    char c;
    unsigned long mask;
    unsigned long value;
    const char *help;
} modeflags[] = {
    { 'l',	MODE1_MASK, MODE1_LINGERING,
	"Lingering window" },
    { 't',	MODE1_MASK, MODE1_TRANSIENT,
	"Transient window" },
    { 's',  MODE10_MASK, MODE10_SHARED,
	"Use shared memory backing store" },
    { '\0', 0, 0, "" },
};


HIDDEN void
qt_updstate(struct fb *ifp)
{
    struct qtinfo *qi = QI(ifp);

    int xwp, ywp;		/* Size of Qt window in image pixels */
    int xrp, yrp;		/* Leftover Qt pixels */

    int tp_h, bt_h;		/* Height of top/bottom image pixel slots */
    int lf_w, rt_w;		/* Width of left/right image pixel slots */

    int want, avail;		/* Wanted/available image pixels */

    FB_CK_FB(ifp->i);

    /*
     * Set ?wp to the number of whole zoomed image pixels we could display
     * in the window.
     */
    xwp = qi->qi_qwidth / ifp->i->if_xzoom;
    ywp = qi->qi_qheight / ifp->i->if_yzoom;

    /*
     * Set ?rp to the number of leftover pixels we have, after displaying
     * wp whole zoomed image pixels.
     */
    xrp = qi->qi_qwidth % ifp->i->if_xzoom;
    yrp = qi->qi_qheight % ifp->i->if_yzoom;

    /*
     * Force ?wp to be the same as the window width (mod 2).  This
     * keeps the image from jumping around when using large zoom
     * factors.
     */
    if (xwp && (xwp ^ qi->qi_qwidth) & 1) {
	xwp--;
	xrp += ifp->i->if_xzoom;
    }

    if (ywp && (ywp ^ qi->qi_qheight) & 1) {
	ywp--;
	yrp += ifp->i->if_yzoom;
    }

    /*
     * Now we calculate the height/width of the outermost image pixel
     * slots.  If we've got any leftover pixels, we'll make
     * truncated slots out of them; if not, the outermost ones end up
     * full size.  We'll adjust ?wp to be the number of full and
     * truncated slots available.
     */
    switch (xrp) {
	case 0:
	    lf_w = ifp->i->if_xzoom;
	    rt_w = ifp->i->if_xzoom;
	    break;

	case 1:
	    lf_w = 1;
	    rt_w = ifp->i->if_xzoom;
	    xwp += 1;
	    break;

	default:
	    lf_w = xrp / 2;
	    rt_w = xrp - lf_w;
	    xwp += 2;
	    break;
    }

    switch (yrp) {
	case 0:
	    tp_h = ifp->i->if_yzoom;
	    bt_h = ifp->i->if_yzoom;
	    break;

	case 1:
	    tp_h = 1;
	    bt_h = ifp->i->if_yzoom;
	    ywp += 1;
	    break;

	default:
	    tp_h = yrp / 2;
	    bt_h = yrp - tp_h;
	    ywp += 2;
	    break;
    }

    /*
     * We've now divided our Qt window up into image pixel slots as
     * follows:
     *
     * - All slots are xzoom by yzoom pixels in size, except:
     *     slots in the top row are tp_h pixels high
     *     slots in the bottom row are bt_h pixels high
     *     slots in the left column are lf_w pixels wide
     *     slots in the right column are rt_w pixels wide
     * - The window is xwp by ywp slots in size.
     */

    /*
     * We can think of xcenter as being "number of pixels we'd like
     * displayed on the left half of the screen".  We have xwp/2
     * pixels available on the left half.  We use this information to
     * calculate the remaining parameters as noted.
     */
    want = ifp->i->if_xcenter;
    avail = xwp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be butted
	 * up against the left edge, so
	 *  - the leftmost pixels will have an x coordinate of 0;
	 *  - the leftmost column of image pixels will be as wide as the
	 *    leftmost column of image pixel slots; and
	 *  - the leftmost image pixel displayed will have an x
	 *    coordinate equal to the number of pixels that didn't fit.
	 */
	qi->qi_xlf = 0;
	qi->qi_ilf_w = lf_w;
	qi->qi_ilf = want - avail;
    } else {
	/*
	 * Not enough image pixels to fill the area.  We'll be offset
	 * from the left edge, so
	 *  - the leftmost pixels will have an x coordinate equal
	 *    to the number of pixels taken up by the unused image
	 *    pixel slots;
	 *  - the leftmost column of image pixels will be as wide as the
	 *    xzoom width; and
	 *  - the leftmost image pixel displayed will have a zero
	 *    x coordinate.
	 */
	qi->qi_xlf = lf_w + (avail - want - 1) * ifp->i->if_xzoom;
	qi->qi_ilf_w = ifp->i->if_xzoom;
	qi->qi_ilf = 0;
    }

    /* Calculation for bottom edge. */
    want = ifp->i->if_ycenter;
    avail = ywp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be
	 * butted up against the bottom edge, so
	 *  - the bottommost pixels will have a y coordinate
	 *    equal to the window height minus 1;
	 *  - the bottommost row of image pixels will be as tall as the
	 *    bottommost row of image pixel slots; and
	 *  - the bottommost image pixel displayed will have a y
	 *    coordinate equal to the number of pixels that didn't fit.
	 */
	qi->qi_xbt = qi->qi_qheight - 1;
	qi->qi_ibt_h = bt_h;
	qi->qi_ibt = want - avail;
    } else {
	/*
	 * Not enough image pixels to fill the area.  We'll be
	 * offset from the bottom edge, so
	 *  - the bottommost pixels will have a y coordinate equal
	 *    to the window height, less the space taken up by the
	 *    unused image pixel slots, minus 1;
	 *  - the bottom row of image pixels will be as tall as the
	 *    yzoom width; and
	 *  - the bottommost image pixel displayed will have a zero
	 *    y coordinate.
	 */
	qi->qi_xbt = qi->qi_qheight - (bt_h + (avail - want - 1) *
		ifp->i->if_yzoom) - 1;
	qi->qi_ibt_h = ifp->i->if_yzoom;
	qi->qi_ibt = 0;
    }

    /* Calculation for right edge. */
    want = qi->qi_iwidth - ifp->i->if_xcenter;
    avail =  xwp - xwp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be
	 * butted up against the right edge, so
	 *  - the rightmost pixels will have an x coordinate equal
	 *    to the window width minus 1;
	 *  - the rightmost column of image pixels will be as wide as
	 *    the rightmost column of image pixel slots; and
	 *  - the rightmost image pixel displayed will have an x
	 *    coordinate equal to the center plus the number of pixels
	 *    that fit, minus 1.
	 */
	qi->qi_xrt = qi->qi_qwidth - 1;
	qi->qi_irt_w = rt_w;
	qi->qi_irt = ifp->i->if_xcenter + avail - 1;
    } else {
	/*
	 * Not enough image pixels to fill the area.  We'll be
	 * offset from the right edge, so
	 *  - the rightmost pixels will have an x coordinate equal
	 *    to the window width, less the space taken up by the
	 *    unused image pixel slots, minus 1;
	 *  - the rightmost column of image pixels will be as wide as
	 *    the xzoom width; and
	 *  - the rightmost image pixel displayed will have an x
	 *    coordinate equal to the width of the image minus 1.
	 */
	qi->qi_xrt = qi->qi_qwidth - (rt_w + (avail - want - 1) *
		ifp->i->if_xzoom) - 1;
	qi->qi_irt_w = ifp->i->if_xzoom;
	qi->qi_irt = qi->qi_iwidth - 1;
    }

    /* Calculation for top edge. */
    want = qi->qi_iheight - ifp->i->if_ycenter;
    avail = ywp - ywp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be
	 * butted up against the top edge, so
	 *  - the topmost pixels will have a y coordinate of 0;
	 *  - the topmost row of image pixels will be as tall as
	 *    the topmost row of image pixel slots; and
	 *  - the topmost image pixel displayed will have a y
	 *    coordinate equal to the center plus the number of pixels
	 *    that fit, minus 1.
	 */
	qi->qi_xtp = 0;
	qi->qi_itp_h = tp_h;
	qi->qi_itp = ifp->i->if_ycenter + avail - 1;
    } else {
	/*
	 * Not enough image pixels to fill the area.  We'll be
	 * offset from the top edge, so
	 *  - the topmost pixels will have a y coordinate equal
	 *    to the space taken up by the unused image pixel slots;
	 *  - the topmost row of image pixels will be as tall as
	 *    the yzoom height; and
	 *  - the topmost image pixel displayed will have a y
	 *    coordinate equal to the height of the image minus 1.
	 */
	qi->qi_xtp = tp_h + (avail - want - 1) * ifp->i->if_yzoom;
	qi->qi_itp_h = ifp->i->if_yzoom;
	qi->qi_itp = qi->qi_iheight - 1;
    }
}

__BEGIN_DECLS

void
qt_configureWindow(struct fb *ifp, int width, int height)
{
    struct qtinfo *qi = QI(ifp);

    FB_CK_FB(ifp->i);

    if (!qi) {
	return;
    }

    if (width == qi->qi_qwidth && height == qi->qi_qheight) {
	return;
    }

    ifp->i->if_width = width;
    ifp->i->if_height = height;

    qi->qi_qwidth = qi->qi_iwidth = width;
    qi->qi_qheight = qi->qi_iheight = height;

    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    /* destroy old image struct and image buffers */
    delete qi->qi_image;
    free(qi->qi_pix);

    if ((qi->qi_pix = (unsigned char *) calloc((width + 1) * (height + 1) * sizeof(RGBpixel),
		    sizeof(char))) == NULL) {
	fb_log("qt_open_existing: pix malloc failed");
    }

    qi->qi_image = new QImage(qi->qi_pix, width, height, QImage::Format_RGB888);
    *qi->qi_parent_img = qi->qi_image;

    qt_updstate(ifp);
}

HIDDEN int
qt_configure_window(struct fb *ifp, int width, int height)
{
    qt_configureWindow(ifp, width, height);
    return 0;
}

HIDDEN int
qt_refresh(struct fb *UNUSED(ifp), int UNUSED(x), int UNUSED(y), int UNUSED(w), int UNUSED(h))
{
    return 0;
}

int
qt_close_existing(struct fb *ifp)
{
    struct qtinfo *qi = QI(ifp);
    FB_CK_FB(ifp->i);

    if (qi->qi_image)
	delete qi->qi_image;

    free((char *)qi);

    return 0;
}

__END_DECLS


HIDDEN void
qt_update(struct fb *ifp, int x1, int y1, int w, int h)
{
    struct qtinfo *qi = QI(ifp);

    int x2 = x1 + w - 1;	/* Convert to rectangle corners */
    int y2 = y1 + h - 1;

    int x1wd, x2wd, y1ht, y2ht;
    int ox, oy;
    int xdel, ydel;
    int xwd, xht;

    unsigned char *ip;
    unsigned char *op;

    int x, y;

    unsigned int a_pixel;

    unsigned char *red = qi->qi_redmap;
    unsigned char *grn = qi->qi_grnmap;
    unsigned char *blu = qi->qi_blumap;

    FB_CK_FB(ifp->i);

    /*
     * Figure out sizes of outermost image pixels
     */
    x1wd = (x1 == qi->qi_ilf) ? qi->qi_ilf_w : ifp->i->if_xzoom;
    x2wd = (x2 == qi->qi_irt) ? qi->qi_irt_w : ifp->i->if_xzoom;
    y1ht = (y1 == qi->qi_ibt) ? qi->qi_ibt_h : ifp->i->if_yzoom;
    y2ht = (y2 == qi->qi_itp) ? qi->qi_itp_h : ifp->i->if_yzoom;

    /* Compute ox: offset from left edge of window to left pixel */
    xdel = x1 - qi->qi_ilf;
    if (xdel) {
	ox = x1wd + ((xdel - 1) * ifp->i->if_xzoom) + qi->qi_xlf;
    } else {
	ox = qi->qi_xlf;
    }

    /* Compute oy: offset from top edge of window to bottom pixel */
    ydel = y1 - qi->qi_ibt;
    if (ydel) {
	oy = qi->qi_xbt - (y1ht + ((ydel - 1) * ifp->i->if_yzoom));
    } else {
	oy = qi->qi_xbt;
    }

    if (x2 == x1) {
	xwd = x1wd;
    } else {
	xwd = x1wd + x2wd + ifp->i->if_xzoom * (x2 - x1 - 1);
    }

    if (y2 == y1) {
	xht = y1ht;
    } else {
	xht = y1ht + y2ht + ifp->i->if_yzoom * (y2 - y1 - 1);
    }

    /*
     * Set pointers to start of source and destination areas; note
     * that we're going from lower to higher image coordinates, so
     * irgb increases, but since images are in quadrant I and Qt uses
     * quadrant IV, opix _decreases_.
     */
    ip = &(qi->qi_mem[(y1 * qi->qi_iwidth + x1) *
	    sizeof (RGBpixel)]);
    op = &qi->qi_pix[oy * qi->qi_image->bytesPerLine() + ox];

    for (y = y1; y <= y2; y++) {
	unsigned char *line_irgb;
	unsigned char *p;

	/* Save pointer to start of line */
	line_irgb = ip;
	p = (unsigned char *)op;

	/* For the first line, convert/copy pixels */
	for (x = x1; x <= x2; x++) {
	    int pxwd;
	    /* Calculate # pixels needed */
	    if (x == x1) {
		pxwd = x1wd;
	    } else if (x == x2) {
		pxwd = x2wd;
	    } else {
		pxwd = ifp->i->if_xzoom;
	    }

	    /*
	     * Construct a pixel with the color components
	     * in the right places as described by the
	     * red, green and blue masks.
	     */
	    if (qi->qi_flags & FLG_LINCMAP) {
		a_pixel  = (line_irgb[0] << 16);
		a_pixel |= (line_irgb[1] << 8);
		a_pixel |= (line_irgb[2]);
	    } else {
		a_pixel  = (red[line_irgb[0]] << 16);
		a_pixel |= (grn[line_irgb[1]] << 8);
		a_pixel |= (blu[line_irgb[2]]);
	    }

	    while (pxwd--) {
		*p++ = (a_pixel >> 16) & 0xff;
		*p++ = (a_pixel >> 8) & 0xff;
		*p++ = a_pixel & 0xff;
	    }

	    /*
	     * Move to the next input line.
	     */
	    line_irgb += sizeof (RGBpixel);
	}

	/*
	 * move to the next line
	 */
	op -= qi->qi_image->bytesPerLine();

	ip += qi->qi_iwidth * sizeof(RGBpixel);
    }

    if (qi->alive == 0) {
	qi->qi_painter->drawImage(ox, oy - xht + 1, *qi->qi_image, ox, oy - xht + 1, xwd, xht);
    }

    QApplication::sendEvent(qi->win, new QEvent(QEvent::UpdateRequest));
    qi->qapp->processEvents();
}


HIDDEN int
qt_rmap(struct fb *ifp, ColorMap *cmp)
{
    struct qtinfo *qi = QI(ifp);
    FB_CK_FB(ifp->i);

    memcpy(cmp, qi->qi_rgb_cmap, sizeof (ColorMap));

    return 0;
}


HIDDEN int
qt_wmap(struct fb *ifp, const ColorMap *cmp)
{
    struct qtinfo *qi = QI(ifp);
    ColorMap *map = qi->qi_rgb_cmap;
    int waslincmap;
    int i;
    unsigned char *red = qi->qi_redmap;
    unsigned char *grn = qi->qi_grnmap;
    unsigned char *blu = qi->qi_blumap;

    FB_CK_FB(ifp->i);

    /* Did we have a linear colormap before this call? */
    waslincmap = qi->qi_flags & FLG_LINCMAP;

    /* Clear linear colormap flag, since it may be changing */
    qi->qi_flags &= ~FLG_LINCMAP;

    /* Copy in or generate colormap */

    if (cmp) {
	if (cmp != map)
	    memcpy(map, cmp, sizeof (ColorMap));
    } else {
	fb_make_linear_cmap(map);
	qi->qi_flags |= FLG_LINCMAP;
    }

    /* Decide if this colormap is linear */
    if (!(qi->qi_flags & FLG_LINCMAP)) {
	int nonlin = 0;

	for (i = 0; i < 256; i++)
	    if (map->cm_red[i] >> 8 != i ||
		    map->cm_green[i] >> 8 != i ||
		    map->cm_blue[i] >> 8 != i) {
		nonlin = 1;
		break;
	    }

	if (!nonlin)
	    qi->qi_flags |= FLG_LINCMAP;
    }

    /*
     * If it was linear before, and they're making it linear again,
     * there's nothing to do.
     */
    if (waslincmap && qi->qi_flags & FLG_LINCMAP)
	return 0;

    /* Copy into our fake colormap arrays. */
    for (i = 0; i < 256; i++) {
	red[i] = map->cm_red[i] >> 8;
	grn[i] = map->cm_green[i] >> 8;
	blu[i] = map->cm_blue[i] >> 8;
    }

    /*
     * If we're initialized, redraw the screen to make changes
     * take effect.
     */
    if (qi->qi_flags & FLG_INIT)
	qt_update(ifp, 0, 0, qi->qi_iwidth, qi->qi_iheight);

    return 0;
}

HIDDEN int
qt_setup(struct fb *ifp, int width, int height)
{
    struct qtinfo *qi = QI(ifp);
    int argc = 1;
    char *argv[] = {(char *)"Frame buffer"};
    FB_CK_FB(ifp->i);

    qi->qi_qwidth = width;
    qi->qi_qheight = height;

    qi->qapp = new QApplication(argc, argv);

    if ((qi->qi_pix = (unsigned char *) calloc(width * height * sizeof(RGBpixel),
		    sizeof(char))) == NULL) {
	fb_log("qt_open: pix malloc failed");
    }

    qi->qi_image = new QImage(qi->qi_pix, width, height, QImage::Format_RGB888);

    qi->win = new QMainWindow(ifp, qi->qi_image);
    qi->win->setWidth(width);
    qi->win->setHeight(height);
    qi->win->show();

    while(!qi->win->isExposed()) {
	qi->qapp->processEvents();
    }

    return 0;
}

HIDDEN void
qt_destroy(struct qtinfo *qi)
{
    if (qi) {
	free(qi);
    }
}

HIDDEN int
qt_open(struct fb *ifp, const char *file, int width, int height)
{
    struct qtinfo *qi;

    unsigned long mode;
    size_t size;
    unsigned char *mem = NULL;

    FB_CK_FB(ifp->i);
    mode = MODE1_LINGERING;

    if (file != NULL) {
	const char *cp;
	char modebuf[80];
	char *mp;
	int alpha;
	struct modeflags *mfp;

	if (bu_strncmp(file, ifp->i->if_name, strlen(ifp->i->if_name))) {
	    mode = 0;
	} else {
	    alpha = 0;
	    mp = &modebuf[0];
	    cp = &file[sizeof("/dev/Qt")-1];
	    while (*cp != '\0' && !isspace((int)(*cp))) {
		*mp++ = *cp;
		if (isdigit((int)(*cp))) {
		    cp++;
		    continue;
		}
		alpha++;
		for (mfp = modeflags; mfp->c != '\0'; mfp++) {
		    if (mfp->c == *cp) {
			mode = (mode&~mfp->mask)|mfp->value;
			break;
		    }
		}
		if (mfp->c == '\0' && *cp != '-') {
		    fb_log("if_qt: unknown option '%c' ignored\n", *cp);
		}
		cp++;
	    }
	    *mp = '\0';
	    if (!alpha)
		mode |= atoi(modebuf);
	}
    }

    if (width <= 0)
	width = ifp->i->if_width;
    if(height <= 0)
	height = ifp->i->if_height;
    if (width > ifp->i->if_max_width)
	width = ifp->i->if_max_width;
    if (height > ifp->i->if_max_height)
	height = ifp->i->if_max_height;

    ifp->i->if_width = width;
    ifp->i->if_height = height;

    ifp->i->if_xzoom = 1;
    ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    if ((qi = (struct qtinfo *)calloc(1, sizeof(struct qtinfo))) ==
	    NULL) {
	fb_log("qt_open: qtinfo malloc failed\n");
	return -1;
    }
    QI_SET(ifp, qi);

    qi->qi_mode = mode;
    qi->qi_iwidth = width;
    qi->qi_iheight = height;

    /* allocate backing store */
    size = ifp->i->if_max_height * ifp->i->if_max_width * sizeof(RGBpixel) + sizeof (*qi->qi_rgb_cmap);;
    if ((mem = (unsigned char *)malloc(size)) == 0) {
	fb_log("if_qt: Unable to allocate %zu bytes of backing \
		store\n  Run shell command 'limit datasize unlimited' and try again.\n", size);
	return -1;
    }
    qi->qi_rgb_cmap = (ColorMap *) mem;
    qi->qi_redmap = (unsigned char *)malloc(256);
    qi->qi_grnmap = (unsigned char *)malloc(256);
    qi->qi_blumap = (unsigned char *)malloc(256);

    if (!qi->qi_redmap || !qi->qi_grnmap || !qi->qi_blumap) {
	fb_log("if_qt: Can't allocate colormap memory\n");
	return -1;
    }
    qi->qi_mem = (unsigned char *) mem + sizeof (*qi->qi_rgb_cmap);

    /* Set up an Qt window */
    if (qt_setup(ifp, width, height) < 0) {
	qt_destroy(qi);
	return -1;
    }

    qt_updstate(ifp);

    qi->alive = 1;

    /* Mark display ready */
    qi->qi_flags |= FLG_INIT;

    /* Set up default linear colormap */
    qt_wmap(ifp, NULL);

    return 0;
}


HIDDEN struct fb_platform_specific *
qt_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    struct qt_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    BU_GET(data, struct qt_fb_info);
    fb_ps->magic = magic;
    fb_ps->data = data;
    return fb_ps;
}


HIDDEN void
qt_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_QT_MAGIC, "Qt framebuffer");
    BU_PUT(fbps->data, struct qt_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}

int
_qt_open_existing(struct fb *ifp, int width, int height, void *qapp, void *qwin, void *qpainter, void *draw, void **qimg)
{
    struct qtinfo *qi;

    unsigned long mode;
    size_t size;
    unsigned char *mem = NULL;

    FB_CK_FB(ifp->i);

    mode = MODE1_LINGERING;

    if (width <= 0)
	width = ifp->i->if_width;
    if(height <= 0)
	height = ifp->i->if_height;
    if (width > ifp->i->if_max_width)
	width = ifp->i->if_max_width;
    if (height > ifp->i->if_max_height)
	height = ifp->i->if_max_height;

    ifp->i->if_width = width;
    ifp->i->if_height = height;

    ifp->i->if_xzoom = 1;
    ifp->i->if_yzoom = 1;
    ifp->i->if_xcenter = width/2;
    ifp->i->if_ycenter = height/2;

    if ((qi = (struct qtinfo *)calloc(1, sizeof(struct qtinfo))) ==
	    NULL) {
	fb_log("qt_open: qtinfo malloc failed\n");
	return -1;
    }
    QI_SET(ifp, qi);

    qi->qi_mode = mode;
    qi->qi_iwidth = width;
    qi->qi_iheight = height;

    /* allocate backing store */
    size = ifp->i->if_max_height * ifp->i->if_max_width * sizeof(RGBpixel) + sizeof (*qi->qi_rgb_cmap);;
    if ((mem = (unsigned char *)malloc(size)) == 0) {
	fb_log("if_qt: Unable to allocate %zu bytes of backing \
		store\n  Run shell command 'limit datasize unlimited' and try again.\n", size);
	return -1;
    }
    qi->qi_rgb_cmap = (ColorMap *) mem;
    qi->qi_redmap = (unsigned char *)malloc(256);
    qi->qi_grnmap = (unsigned char *)malloc(256);
    qi->qi_blumap = (unsigned char *)malloc(256);

    if (!qi->qi_redmap || !qi->qi_grnmap || !qi->qi_blumap) {
	fb_log("if_qt: Can't allocate colormap memory\n");
	return -1;
    }
    qi->qi_mem = (unsigned char *) mem + sizeof (*qi->qi_rgb_cmap);

    /* Set up an Qt window */
    qi->qi_qwidth = width;
    qi->qi_qheight = height;

    qi->qapp = (QApplication *)qapp;

    if ((qi->qi_pix = (unsigned char *) calloc(width * height * sizeof(RGBpixel),
		    sizeof(char))) == NULL) {
	fb_log("qt_open_existing: pix malloc failed");
    }

    qi->qi_image = new QImage(qi->qi_pix, width, height, QImage::Format_RGB888);
    *qimg = qi->qi_image;

    qi->qi_parent_img = qimg;
    qi->qi_painter = (QPainter *)qpainter;
    qi->win = (QWindow *)qwin;

    qt_updstate(ifp);
    qi->alive = 0;
    qi->drawFb = (int *)draw;

    /* Set up default linear colormap */
    qt_wmap(ifp, NULL);

    /* Mark display ready */
    qi->qi_flags |= FLG_INIT;

    return 0;
}

HIDDEN int
qt_open_existing(struct fb *ifp, int width, int height, struct fb_platform_specific *fb_p)
{
    struct qt_fb_info *qt_internal = (struct qt_fb_info *)fb_p->data;
    BU_CKMAG(fb_p, FB_QT_MAGIC, "qt framebuffer");
    return _qt_open_existing(ifp, width, height, qt_internal->qapp, qt_internal->qwin,
	    qt_internal->qpainter, qt_internal->draw, qt_internal->qimg);
}


HIDDEN int
qt_close(struct fb *ifp)
{
    struct qtinfo *qi = QI(ifp);

    /* if a window was created wait for user input and process events */
    if (qi->alive == 1) {
	return qi->qapp->exec();
    }

    qt_destroy(qi);

    return 0;
}


HIDDEN int
qt_clear(struct fb *ifp, unsigned char *pp)
{
    struct qtinfo *qi = QI(ifp);

    int red, grn, blu;
    int npix;
    int n;
    unsigned char *cp;

    FB_CK_FB(ifp->i);

    if (pp == (unsigned char *)NULL) {
	red = grn = blu = 0;
    } else {
	red = pp[0];
	grn = pp[1];
	blu = pp[2];
    }

    /* Clear the backing store */
    npix = qi->qi_iwidth * qi->qi_qheight;

    if (red == grn && red == blu) {
	memset(qi->qi_mem, red, npix*3);
    } else {
	cp = qi->qi_mem;
	n = npix;
	while (n--) {
	    *cp++ = red;
	    *cp++ = grn;
	    *cp++ = blu;
	}
    }

    qt_update(ifp, 0, 0, qi->qi_iwidth, qi->qi_iheight);

    return 0;
}


HIDDEN ssize_t
qt_read(struct fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    struct qtinfo *qi = QI(ifp);
    size_t maxcount;

    FB_CK_FB(ifp->i);

    /* check origin bounds */
    if (x < 0 || x >= qi->qi_iwidth || y < 0 || y >= qi->qi_iheight)
	return -1;

    /* clip read length */
    maxcount = qi->qi_iwidth * (qi->qi_iheight - y) - x;
    if (count > maxcount)
	count = maxcount;

    memcpy(pixelp, &(qi->qi_mem[(y*qi->qi_iwidth + x)*sizeof(RGBpixel)]), count*sizeof(RGBpixel));

    return count;
}


HIDDEN ssize_t
qt_write(struct fb *ifp, int x, int y, const unsigned char *pixelp, size_t count)
{
    struct qtinfo *qi = QI(ifp);
    size_t maxcount;

    FB_CK_FB(ifp->i);

    /* Check origin bounds */
    if (x < 0 || x >= qi->qi_iwidth || y < 0 || y >= qi->qi_iheight)
	return -1;

    /* Clip write length */
    maxcount = qi->qi_iwidth * (qi->qi_iheight - y) - x;
    if (count > maxcount)
	count = maxcount;

    /* Save it in 24bit backing store */
    memcpy(&(qi->qi_mem[(y * qi->qi_iwidth + x) * sizeof(RGBpixel)]),
	    pixelp, count * sizeof(RGBpixel));

    if (qi->alive == 0) {
	if (*qi->drawFb == 0)
	    *qi->drawFb = 1;
    }

    if (x + count <= (size_t)qi->qi_iwidth) {
	qt_update(ifp, x, y, count, 1);
    } else {
	size_t ylines, tcount;

	tcount = count - (qi->qi_iwidth - x);
	ylines = 1 + (tcount + qi->qi_iwidth - 1) / qi->qi_iwidth;

	qt_update(ifp, 0, y, qi->qi_iwidth, ylines);
    }
    return count;
}


HIDDEN int
qt_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct qtinfo *qi = QI(ifp);

    FB_CK_FB(ifp->i);

    /* bypass if no change */
    if (ifp->i->if_xcenter == xcenter && ifp->i->if_ycenter == ycenter
	    && ifp->i->if_xzoom == xcenter && ifp->i->if_yzoom == ycenter)
	return 0;

    /* check bounds */
    if (xcenter < 0 || xcenter >= qi->qi_iwidth
	    || ycenter < 0 || ycenter >= qi->qi_iheight)
	return -1;
    if (xzoom <= 0 || xzoom >= qi->qi_iwidth/2
	    || yzoom <= 0 || yzoom >= qi->qi_iheight/2)
	return -1;

    ifp->i->if_xcenter = xcenter;
    ifp->i->if_ycenter = ycenter;
    ifp->i->if_xzoom = xzoom;
    ifp->i->if_yzoom = yzoom;

    qt_updstate(ifp);
    qt_update(ifp, 0, 0, qi->qi_iwidth, qi->qi_iheight);

    return 0;
}


HIDDEN int
qt_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    FB_CK_FB(ifp->i);

    *xcenter = ifp->i->if_xcenter;
    *ycenter = ifp->i->if_ycenter;
    *xzoom = ifp->i->if_xzoom;
    *yzoom = ifp->i->if_yzoom;

    return 0;
}


HIDDEN int
qt_setcursor(struct fb *UNUSED(ifp), const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    fb_log("qt_setcursor\n");

    return 0;
}


HIDDEN int
qt_cursor(struct fb *UNUSED(ifp), int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    fb_log("qt_cursor\n");

    return 0;
}


HIDDEN int
qt_getcursor(struct fb *UNUSED(ifp), int *UNUSED(mode), int *UNUSED(x), int *UNUSED(y))
{
    fb_log("qt_getcursor\n");

    return 0;
}


HIDDEN int
qt_readrect(struct fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
    struct qtinfo *qi = QI(ifp);
    FB_CK_FB(ifp->i);

    /* Clip arguments */
    if (xmin < 0)
	xmin = 0;
    if (ymin < 0)
	ymin = 0;
    if (xmin + width > qi->qi_iwidth)
	width = qi->qi_iwidth - xmin;
    if (ymin + height > qi->qi_iheight)
	height = qi->qi_iheight - ymin;

    /* Do copy to backing store */
    if (xmin == 0 && width == qi->qi_iwidth) {
	/* We can do it all in one copy */
	memcpy(pp, &(qi->qi_mem[ymin * qi->qi_iwidth *
		    sizeof (RGBpixel)]),
		width * height * sizeof (RGBpixel));
    } else {
	/* Need to do individual lines */
	int ht = height;
	unsigned char *p = &(qi->qi_mem[(ymin * qi->qi_iwidth + xmin) *
		sizeof (RGBpixel)]);

	while (ht--) {
	    memcpy(pp, p, width * sizeof (RGBpixel));
	    p += qi->qi_iwidth * sizeof (RGBpixel);
	    pp += width * sizeof (RGBpixel);
	}
    }

    return width * height;
}


HIDDEN int
qt_writerect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    struct qtinfo *qi = QI(ifp);
    FB_CK_FB(ifp->i);

    /* Clip arguments */
    if (xmin < 0)
	xmin = 0;
    if (ymin < 0)
	ymin = 0;
    if (xmin + width > qi->qi_iwidth)
	width = qi->qi_iwidth - xmin;
    if (ymin + height > qi->qi_iheight)
	height = qi->qi_iheight - ymin;

    /* Do copy to backing store */
    if (xmin == 0 && width == qi->qi_iwidth) {
	/* We can do it all in one copy */
	memcpy(&(qi->qi_mem[ymin * qi->qi_iwidth * sizeof (RGBpixel)]),
		pp, width * height * sizeof (RGBpixel));
    } else {
	/* Need to do individual lines */
	int ht = height;
	unsigned char *p = &(qi->qi_mem[(ymin * qi->qi_iwidth + xmin) *
		sizeof (RGBpixel)]);

	while (ht--) {
	    memcpy(p, pp, width * sizeof (RGBpixel));
	    p += qi->qi_iwidth * sizeof (RGBpixel);
	    pp += width * sizeof (RGBpixel);
	}
    }

    /* Flush to screen */
    qt_update(ifp, xmin, ymin, width, height);

    return width * height;
}


HIDDEN int
qt_help(struct fb *ifp)
{
    fb_log("Description: %s\n", qt_interface.i->if_type);
    fb_log("Device: %s\n", ifp->i->if_name);
    fb_log("Max width/height: %d %d\n",
	    qt_interface.i->if_max_width,
	    qt_interface.i->if_max_height);
    fb_log("Default width/height: %d %d\n",
	    qt_interface.i->if_width,
	    qt_interface.i->if_height);
    fb_log("Useful for Benchmarking/Debugging\n");
    return 0;
}


HIDDEN void
qt_handle_event(struct fb *ifp, QEvent *event)
{
    struct qtinfo *qi = QI(ifp);
    FB_CK_FB(ifp->i);

    switch (event->type()) {
	case QEvent::MouseButtonPress:
	    {
		QMouseEvent *ev = (QMouseEvent *)event;
		int button = ev->button();

		switch (button) {
		    case Qt::LeftButton:
			break;
		    case Qt::MiddleButton:
			{
			    int x, sy;
			    int ix, isy;
			    unsigned char *cp;

			    x = ev->x();
			    sy = qi->qi_qheight - ev->y() - 1;

			    x -= qi->qi_xlf;
			    sy -= qi->qi_qheight - qi->qi_xbt - 1;
			    if (x < 0 || sy < 0) {
				fb_log("No RGB (outside image) 1\n");
				break;
			    }

			    if (x < qi->qi_ilf_w)
				ix = qi->qi_ilf;
			    else
				ix = qi->qi_ilf + (x - qi->qi_ilf_w + ifp->i->if_xzoom - 1) / ifp->i->if_xzoom;

			    if (sy < qi->qi_ibt_h)
				isy = qi->qi_ibt;
			    else
				isy = qi->qi_ibt + (sy - qi->qi_ibt_h + ifp->i->if_yzoom - 1) / ifp->i->if_yzoom;

			    if (ix >= qi->qi_iwidth || isy >= qi->qi_iheight) {
				fb_log("No RGB (outside image) 2\n");
				break;
			    }

			    cp = &(qi->qi_mem[(isy*qi->qi_iwidth + ix)*3]);
			    fb_log("At image (%d, %d), real RGB=(%3d %3d %3d)\n",
				    ix, isy, cp[0], cp[1], cp[2]);

			    break;
			}
		    case Qt::RightButton:
			qi->qapp->exit();
			break;
		}
		break;
	    }
	case 6 /* QEvent::KeyPress */:
	    {
		QKeyEvent *ev = (QKeyEvent *)event;
		if (ev->key() == Qt::Key_H)
		    qt_help(ifp);
		break;
	    }
	default:
	    break;
    }

    return;
}


HIDDEN int
qt_poll(struct fb *UNUSED(ifp))
{
    fb_log("qt_poll\n");

    return 0;
}


HIDDEN int
qt_flush(struct fb *ifp)
{
    struct qtinfo *qi = QI(ifp);

    qi->qapp->processEvents();

    return 0;
}


HIDDEN int
qt_free(struct fb *UNUSED(ifp))
{
    fb_log("qt_free\n");

    return 0;
}


struct fb_impl qt_interface_impl =  {
    0,
    FB_QT_MAGIC,
    qt_open,		/* device_open */
    qt_open_existing,
    qt_close_existing,
    qt_get_fbps,
    qt_put_fbps,
    qt_close,		/* device_close */
    qt_clear,		/* device_clear */
    qt_read,		/* buffer_read */
    qt_write,		/* buffer_write */
    qt_rmap,		/* colormap_read */
    qt_wmap,		/* colormap_write */
    qt_view,		/* set view */
    qt_getview,	/* get view */
    qt_setcursor,	/* define cursor */
    qt_cursor,	/* set cursor */
    qt_getcursor,	/* get cursor */
    qt_readrect,	/* rectangle read */
    qt_writerect,	/* rectangle write */
    qt_readrect,	/* bw rectangle read */
    qt_writerect,	/* bw rectangle write */
    qt_configure_window,
    qt_refresh,
    qt_poll,		/* handle events */
    qt_flush,		/* flush output */
    qt_free,		/* free resources */
    qt_help,		/* help message */
    (char *)"Qt Device",/* device description */
    FB_XMAXSCREEN,	/* max width */
    FB_YMAXSCREEN,	/* max height */
    (char *)"/dev/Qt",	/* short device name */
    512,		/* default/current width */
    512,		/* default/current height */
    -1,			/* select fd */
    -1,			/* file descriptor */
    1, 1,		/* zoom */
    256, 256,		/* window center */
    0, 0, 0,		/* cursor */
    PIXEL_NULL,		/* page_base */
    PIXEL_NULL,		/* page_curp */
    PIXEL_NULL,		/* page_endp */
    -1,			/* page_no */
    0,			/* page_dirty */
    0L,			/* page_curpos */
    0L,			/* page_pixels */
    0,			/* debug */
    50000,		/* refresh rate */
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};

struct fb qt_interface =  { &qt_interface_impl };

#ifdef DM_PLUGIN
static const struct fb_plugin finfo = { &qt_interface };

extern "C" {
    COMPILER_DLLEXPORT const struct fb_plugin *fb_plugin_info()
    {
	return &finfo;
    }
}
#endif


/**
 * ===================================================== Main window class ===============================================
 */

QMainWindow::QMainWindow(struct fb *fbp, QImage *img, QWindow *win)
: QWindow(win)
    , m_update_pending(false)
{
    m_backingStore = new QBackingStore(this);
    image = img;
    ifp = fbp;
    create();
}

QMainWindow::~QMainWindow()
{
    delete m_backingStore;
    close();
}

void QMainWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed()) {
	renderNow();
    }
}

void QMainWindow::resizeEvent(QResizeEvent *resizeEv)
{
    m_backingStore->resize(resizeEv->size());
    if (isExposed())
	renderNow();
}

bool QMainWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::UpdateRequest) {
	m_update_pending = false;
	renderNow();
	return true;
    }

    qt_handle_event(ifp, ev);

    return QWindow::event(ev);
}

void QMainWindow::renderNow()
{
    if (!isExposed()) {
	return;
    }

    QRect rect(0, 0, width(), height());
    m_backingStore->beginPaint(rect);

    QPaintDevice *device = m_backingStore->paintDevice();
    QPainter painter(device);

    render(&painter);

    painter.end();
    m_backingStore->endPaint();
    m_backingStore->flush(rect);
}

void QMainWindow::render(QPainter *painter)
{
    painter->drawPixmap(0, 0, QPixmap::fromImage(*image));
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
