/*                       I F _ Q T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @addtogroup if */
/** @{ */
/** @file if_qt.cpp
 *
 * A Qt Frame Buffer.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#include <QApplication>
#include <QPainter>
#include <QWindow>
#include <QBackingStore>
#include <QResizeEvent>
#include <QImage>

#include "fb.h"
#include "bu/file.h"
#include "bu/str.h"

class QMainWindow: public QWindow {

public:
    QMainWindow(QImage *image, QWindow *parent = 0);
    ~QMainWindow();

    virtual void render(QPainter *painter);
public slots:
    void renderNow();

protected:
    bool event(QEvent *event);

    void resizeEvent(QResizeEvent *event);
    void exposeEvent(QExposeEvent *event);

private:
    QImage *image;
    QBackingStore *m_backingStore;
    bool m_update_pending;
};

struct qtinfo {
    QApplication *qapp;
    QWindow *win;
    QImage *qi_image;
    QPainter *qi_painter;

    int alive;

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
};

#define QI(ptr) ((struct qtinfo *)((ptr)->u1.p))
#define QI_SET(ptr, val) ((ptr)->u1.p) = (char *) val;

/* Flags in qi_flags */
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
qt_updstate(FBIO *ifp)
{
    struct qtinfo *qi = QI(ifp);

    int xwp, ywp;		/* Size of Qt window in image pixels */
    int xrp, yrp;		/* Leftover Qt pixels */

    int tp_h, bt_h;		/* Height of top/bottom image pixel slots */
    int lf_w, rt_w;		/* Width of left/right image pixel slots */

    int want, avail;		/* Wanted/available image pixels */

    FB_CK_FBIO(ifp);

    /*
     * Set ?wp to the number of whole zoomed image pixels we could display
     * in the X window.
     */
    xwp = qi->qi_qwidth / ifp->if_xzoom;
    ywp = qi->qi_qheight / ifp->if_yzoom;

    /*
     * Set ?rp to the number of leftover X pixels we have, after displaying
     * wp whole zoomed image pixels.
     */
    xrp = qi->qi_qwidth % ifp->if_xzoom;
    yrp = qi->qi_qheight % ifp->if_yzoom;

    /*
     * Force ?wp to be the same as the window width (mod 2).  This
     * keeps the image from jumping around when using large zoom
     * factors.
     */

    if (xwp && (xwp ^ qi->qi_qwidth) & 1) {
	xwp--;
	xrp += ifp->if_xzoom;
    }

    if (ywp && (ywp ^ qi->qi_qheight) & 1) {
	ywp--;
	yrp += ifp->if_yzoom;
    }

    /*
     * Now we calculate the height/width of the outermost image pixel
     * slots.  If we've got any leftover X pixels, we'll make
     * truncated slots out of them; if not, the outermost ones end up
     * full size.  We'll adjust ?wp to be the number of full and
     * truncated slots available.
     */
    switch (xrp) {
	case 0:
	    lf_w = ifp->if_xzoom;
	    rt_w = ifp->if_xzoom;
	    break;

	case 1:
	    lf_w = 1;
	    rt_w = ifp->if_xzoom;
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
	    tp_h = ifp->if_yzoom;
	    bt_h = ifp->if_yzoom;
	    break;

	case 1:
	    tp_h = 1;
	    bt_h = ifp->if_yzoom;
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
     * - All slots are xzoom by yzoom X pixels in size, except:
     *     slots in the top row are tp_h X pixels high
     *     slots in the bottom row are bt_h X pixels high
     *     slots in the left column are lf_w X pixels wide
     *     slots in the right column are rt_w X pixels wide
     * - The window is xwp by ywp slots in size.
     */

    /*
     * We can think of xcenter as being "number of pixels we'd like
     * displayed on the left half of the screen".  We have xwp/2
     * pixels available on the left half.  We use this information to
     * calculate the remaining parameters as noted.
     */

    want = ifp->if_xcenter;
    avail = xwp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be butted
	 * up against the left edge, so
	 *  - the leftmost X pixels will have an x coordinate of 0;
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
	 *  - the leftmost X pixels will have an x coordinate equal
	 *    to the number of pixels taken up by the unused image
	 *    pixel slots;
	 *  - the leftmost column of image pixels will be as wide as the
	 *    xzoom width; and
	 *  - the leftmost image pixel displayed will have a zero
	 *    x coordinate.
	 */

	qi->qi_xlf = lf_w + (avail - want - 1) * ifp->if_xzoom;
	qi->qi_ilf_w = ifp->if_xzoom;
	qi->qi_ilf = 0;
    }

    /* Calculation for bottom edge. */

    want = ifp->if_ycenter;
    avail = ywp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be
	 * butted up against the bottom edge, so
	 *  - the bottommost X pixels will have a y coordinate
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
	 *  - the bottommost X pixels will have a y coordinate equal
	 *    to the window height, less the space taken up by the
	 *    unused image pixel slots, minus 1;
	 *  - the bottom row of image pixels will be as tall as the
	 *    yzoom width; and
	 *  - the bottommost image pixel displayed will have a zero
	 *    y coordinate.
	 */

	qi->qi_xbt = qi->qi_qheight - (bt_h + (avail - want - 1) *
				       ifp->if_yzoom) - 1;
	qi->qi_ibt_h = ifp->if_yzoom;
	qi->qi_ibt = 0;
    }

    /* Calculation for right edge. */

    want = qi->qi_iwidth - ifp->if_xcenter;
    avail =  xwp - xwp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be
	 * butted up against the right edge, so
	 *  - the rightmost X pixels will have an x coordinate equal
	 *    to the window width minus 1;
	 *  - the rightmost column of image pixels will be as wide as
	 *    the rightmost column of image pixel slots; and
	 *  - the rightmost image pixel displayed will have an x
	 *    coordinate equal to the center plus the number of pixels
	 *    that fit, minus 1.
	 */

	qi->qi_xrt = qi->qi_qwidth - 1;
	qi->qi_irt_w = rt_w;
	qi->qi_irt = ifp->if_xcenter + avail - 1;
    } else {
	/*
	 * Not enough image pixels to fill the area.  We'll be
	 * offset from the right edge, so
	 *  - the rightmost X pixels will have an x coordinate equal
	 *    to the window width, less the space taken up by the
	 *    unused image pixel slots, minus 1;
	 *  - the rightmost column of image pixels will be as wide as
	 *    the xzoom width; and
	 *  - the rightmost image pixel displayed will have an x
	 *    coordinate equal to the width of the image minus 1.
	 */

	qi->qi_xrt = qi->qi_qwidth - (rt_w + (avail - want - 1) *
				      ifp->if_xzoom) - 1;
	qi->qi_irt_w = ifp->if_xzoom;
	qi->qi_irt = qi->qi_iwidth - 1;
    }

    /* Calculation for top edge. */

    want = qi->qi_iheight - ifp->if_ycenter;
    avail = ywp - ywp/2;
    if (want >= avail) {
	/*
	 * Just enough or too many pixels to display.  We'll be
	 * butted up against the top edge, so
	 *  - the topmost X pixels will have a y coordinate of 0;
	 *  - the topmost row of image pixels will be as tall as
	 *    the topmost row of image pixel slots; and
	 *  - the topmost image pixel displayed will have a y
	 *    coordinate equal to the center plus the number of pixels
	 *    that fit, minus 1.
	 */

	qi->qi_xtp = 0;
	qi->qi_itp_h = tp_h;
	qi->qi_itp = ifp->if_ycenter + avail - 1;
    } else {
	/*
	 * Not enough image pixels to fill the area.  We'll be
	 * offset from the top edge, so
	 *  - the topmost X pixels will have a y coordinate equal
	 *    to the space taken up by the unused image pixel slots;
	 *  - the topmost row of image pixels will be as tall as
	 *    the yzoom height; and
	 *  - the topmost image pixel displayed will have a y
	 *    coordinate equal to the height of the image minus 1.
	 */

	qi->qi_xtp = tp_h + (avail - want - 1) * ifp->if_yzoom;
	qi->qi_itp_h = ifp->if_yzoom;
	qi->qi_itp = qi->qi_iheight - 1;
    }

}

__BEGIN_DECLS

void
qt_configureWindow(FBIO *ifp, int width, int height)
{
    struct qtinfo *qi = QI(ifp);

    FB_CK_FBIO(ifp);

    if (!qi) {
	return;
    }

    if (width == qi->qi_qwidth && height == qi->qi_qheight) {
	return;
    }

    ifp->if_width = width;
    ifp->if_height = height;

    qi->qi_qwidth = qi->qi_iwidth = width;
    qi->qi_qheight = qi->qi_iheight = height;

    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;

    /* destroy old image struct and image buffers */
    delete qi->qi_image;
    delete qi->qi_pix;

    if ((qi->qi_pix = (unsigned char *) calloc(width * height * sizeof(RGBpixel),
	sizeof(char))) == NULL) {
	fb_log("qt_open_existing: pix malloc failed");
    }

    qi->qi_image = new QImage(qi->qi_pix, width, height, QImage::Format_RGB888);

    qt_updstate(ifp);

    fb_log("configure_win %d %d\n", ifp->if_height, ifp->if_width);
}

__END_DECLS


HIDDEN void
qt_update(FBIO *ifp, int x1, int y1, int w, int h)
{
    struct qtinfo *qi = QI(ifp);

    int x2 = x1 + w - 1;	/* Convert to rectangle corners */
    int y2 = y1 + h - 1;

    int x1wd, y1ht;
    int ox, oy;
    int xdel, ydel;

    unsigned char *ip;
    unsigned char *op;

    int j, k;

    FB_CK_FBIO(ifp);

    /*
     * Figure out sizes of outermost image pixels
     */
    x1wd = (x1 == qi->qi_ilf) ? qi->qi_ilf_w : ifp->if_xzoom;
    y1ht = (y1 == qi->qi_ibt) ? qi->qi_ibt_h : ifp->if_yzoom;

    /* Compute ox: offset from left edge of window to left pixel */
    xdel = x1 - qi->qi_ilf;
    if (xdel) {
	ox = x1wd + ((xdel - 1) * ifp->if_xzoom) + qi->qi_xlf;
    } else {
	ox = qi->qi_xlf;
    }


    /* Compute oy: offset from top edge of window to bottom pixel */
    ydel = y1 - qi->qi_ibt;
    if (ydel) {
	oy = qi->qi_xbt - (y1ht + ((ydel - 1) * ifp->if_yzoom));
    } else {
	oy = qi->qi_xbt;
    }


    /*
     * Set pointers to start of source and destination areas; note
     * that we're going from lower to higher image coordinates, so
     * irgb increases, but since images are in quadrant I and Qt uses
     * quadrant IV, opix _decreases_.
     */
    ip = &(qi->qi_mem[(y1 * qi->qi_iwidth + x1) *
				 sizeof (RGBpixel)]);
    op = (unsigned char *) &qi->qi_pix[oy *
				 qi->qi_qwidth + ox];

    for (j = y2 - y1 + 1; j; j--) {
	unsigned char *lip;
	unsigned char *lop;

	lip = ip;
	lop = op;

	for (k = x2 - x1 + 1; k; k--) {
	    *lop++ = *lip;
	    lip += sizeof (RGBpixel);
	}

	ip += qi->qi_iwidth * sizeof (RGBpixel);
	op -= qi->qi_image->bytesPerLine();
    }

  /*  if (flags & BLIT_DISP) {
	XPutImage(xi->xi_dpy, xi->xi_win, xi->xi_gc, xi->xi_image,
		  ox, oy - xht + 1, ox, oy - xht + 1, xwd, xht);
    }*/


/*    struct qtinfo *qi = QI(ifp);
    int i,j;
    unsigned int k;

    for (i = y1; i < y1 + h; i++) {
	for (j = x1; j < x1 + w; j++) {
	    for (k = 0; k < sizeof(RGBpixel); k++)
		qi->qi_pix[((qi->qi_iheight - i) * qi->qi_iwidth + j) * sizeof(RGBpixel) + k] = qi->qi_mem[(i * qi->qi_iwidth + j) * sizeof(RGBpixel) + k];
	}
    }
    memcpy(&(qi->qi_pix[(y1*qi->qi_iwidth+x1)*sizeof(RGBpixel)]),
 	   &(qi->qi_mem[(y1*qi->qi_iwidth+x1)*sizeof(RGBpixel)]), w * h * sizeof(RGBpixel));
*/


    QApplication::sendEvent(qi->win, new QEvent(QEvent::UpdateRequest));
    qi->qapp->processEvents();

    if (qi->alive == 0) {
	qi->qi_painter->drawImage(x1, y1, *qi->qi_image, x1, y1, w, h);
    }
}

HIDDEN int
qt_setup(FBIO *ifp, int width, int height)
{
    struct qtinfo *qi = QI(ifp);
    int argc = 1;
    char *argv[] = {(char *)"Frame buffer"};
    FB_CK_FBIO(ifp);

    qi->qi_qwidth = width;
    qi->qi_qheight = height;

    qi->qapp = new QApplication(argc, argv);

    if ((qi->qi_pix = (unsigned char *) calloc(width * height * sizeof(RGBpixel),
	sizeof(char))) == NULL) {
	fb_log("qt_open: pix malloc failed");
    }

    qi->qi_image = new QImage(qi->qi_pix, width, height, QImage::Format_RGB888);

    qi->win = new QMainWindow(qi->qi_image);
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
qt_open(FBIO *ifp, const char *file, int width, int height)
{
    struct qtinfo *qi;

    unsigned long mode;
    size_t size;

    FB_CK_FBIO(ifp);
    mode = MODE1_LINGERING;

    if (file != NULL) {
	const char *cp;
	char modebuf[80];
	char *mp;
	int alpha;
	struct modeflags *mfp;

	if (bu_strncmp(file, ifp->if_name, strlen(ifp->if_name))) {
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
	width = ifp->if_width;
    if(height <= 0)
	height = ifp->if_height;
    if (width > ifp->if_max_width)
	width = ifp->if_max_width;
    if (height > ifp->if_max_height)
	height = ifp->if_max_height;

    ifp->if_width = width;
    ifp->if_height = height;

    ifp->if_xzoom = 1;
    ifp->if_yzoom = 1;
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;

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
    size = ifp->if_max_height * ifp->if_max_width * sizeof(RGBpixel);
    if ((qi->qi_mem = (unsigned char *)malloc(size)) == 0) {
	fb_log("if_qt: Unable to allocate %d bytes of backing \
		store\n  Run shell command 'limit datasize unlimited' and try again.\n", size);
	return -1;
    }

    /* Set up an Qt window */
    if (qt_setup(ifp, width, height) < 0) {
	qt_destroy(qi);
	return -1;
    }

    qt_updstate(ifp);

    qi->alive = 1;

    /* Mark display ready */
    qi->qi_flags |= FLG_INIT;

    fb_log("qt_open\n");

    return 0;
}

int
_qt_open_existing(FBIO *ifp, int width, int height, void *qapp, void *qwin, void *qpainter)
{
    struct qtinfo *qi;

    unsigned long mode;
    size_t size;

    FB_CK_FBIO(ifp);

    mode = MODE1_LINGERING;

    if (width <= 0)
	width = ifp->if_width;
    if(height <= 0)
	height = ifp->if_height;
    if (width > ifp->if_max_width)
	width = ifp->if_max_width;
    if (height > ifp->if_max_height)
	height = ifp->if_max_height;

    ifp->if_width = width;
    ifp->if_height = height;

    ifp->if_xzoom = 1;
    ifp->if_yzoom = 1;
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;

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
    size = ifp->if_max_height * ifp->if_max_width * sizeof(RGBpixel);
    if ((qi->qi_mem = (unsigned char *)malloc(size)) == 0) {
	fb_log("if_qt: Unable to allocate %d bytes of backing \
		store\n  Run shell command 'limit datasize unlimited' and try again.\n", size);
	return -1;
    }

    /* Set up an Qt window */
    qi->qi_qwidth = width;
    qi->qi_qheight = height;

    qi->qapp = (QApplication *)qapp;

    if ((qi->qi_pix = (unsigned char *) calloc(width * height * sizeof(RGBpixel),
	sizeof(char))) == NULL) {
	fb_log("qt_open_existing: pix malloc failed");
    }

    qi->qi_image = new QImage(qi->qi_pix, width, height, QImage::Format_RGB888);

    qi->win = (QWindow *)qwin;
    qi->qi_painter = (QPainter *)qpainter;

    qt_updstate(ifp);
    qi->alive = 0;

    /* Mark display ready */
    qi->qi_flags |= FLG_INIT;

    fb_log("_qt_open_existing %d %d\n", ifp->if_max_height, ifp->if_max_width);

    return 0;
}


HIDDEN int
qt_close(FBIO *ifp)
{
    struct qtinfo *qi = QI(ifp);

    fb_log("qt_close\n");

    /* if a window was created wait for user input and process events */
    if (qi->alive == 1) {
	return qi->qapp->exec();
    }

    qt_destroy(qi);

    return 0;
}


HIDDEN int
qt_clear(FBIO *UNUSED(ifp), unsigned char *UNUSED(pp))
{
    fb_log("qt_clear\n");

    return 0;
}


HIDDEN ssize_t
qt_read(FBIO *UNUSED(ifp), int UNUSED(x), int UNUSED(y), unsigned char *UNUSED(pixelp), size_t count)
{
    fb_log("qt_read\n");

    return count;
}


HIDDEN ssize_t
qt_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, size_t count)
{
    struct qtinfo *qi = QI(ifp);
    size_t maxcount;

    FB_CK_FBIO(ifp);

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
qt_rmap(FBIO *UNUSED(ifp), ColorMap *UNUSED(cmp))
{
    fb_log("qt_rmap\n");

    return 0;
}


HIDDEN int
qt_wmap(FBIO *UNUSED(ifp), const ColorMap *UNUSED(cmp))
{
    fb_log("qt_wmap\n");

    return 0;
}


HIDDEN int
qt_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct qtinfo *qi = QI(ifp);

    FB_CK_FBIO(ifp);

    /* bypass if no change */
    if (ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	&& ifp->if_xzoom == xcenter && ifp->if_yzoom == ycenter)
	return 0;

    /* check bounds */
    if (xcenter < 0 || xcenter >= qi->qi_iwidth
	|| ycenter < 0 || ycenter >= qi->qi_iheight)
	return -1;
    if (xzoom <= 0 || xzoom >= qi->qi_iwidth/2
	|| yzoom <= 0 || yzoom >= qi->qi_iheight/2)
	return -1;

    ifp->if_xcenter = xcenter;
    ifp->if_ycenter = ycenter;
    ifp->if_xzoom = xzoom;
    ifp->if_yzoom = yzoom;

    /* TODO make changes */

    fb_log("qt_view\n");

    return 0;
}


HIDDEN int
qt_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    FB_CK_FBIO(ifp);

    *xcenter = ifp->if_xcenter;
    *ycenter = ifp->if_ycenter;
    *xzoom = ifp->if_xzoom;
    *yzoom = ifp->if_yzoom;

    return 0;
}


HIDDEN int
qt_setcursor(FBIO *UNUSED(ifp), const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    fb_log("qt_setcursor\n");

    return 0;
}


HIDDEN int
qt_cursor(FBIO *UNUSED(ifp), int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{
    fb_log("qt_cursor\n");

    return 0;
}


HIDDEN int
qt_getcursor(FBIO *UNUSED(ifp), int *UNUSED(mode), int *UNUSED(x), int *UNUSED(y))
{
    fb_log("qt_getcursor\n");

    return 0;
}


HIDDEN int
qt_readrect(FBIO *UNUSED(ifp), int UNUSED(xmin), int UNUSED(ymin), int width, int height, unsigned char *UNUSED(pp))
{
    fb_log("qt_readrect\n");

    return width*height;
}


HIDDEN int
qt_writerect(FBIO *UNUSED(ifp), int UNUSED(xmin), int UNUSED(ymin), int width, int height, const unsigned char *UNUSED(pp))
{
    fb_log("qt_writerect\n");

    return width*height;
}


HIDDEN int
qt_poll(FBIO *UNUSED(ifp))
{
    fb_log("qt_poll\n");

    return 0;
}


HIDDEN int
qt_flush(FBIO *ifp)
{
    struct qtinfo *qi = QI(ifp);

    qi->qapp->processEvents();

    fb_log("qt_flush\n");

    return 0;
}


HIDDEN int
qt_free(FBIO *UNUSED(ifp))
{
    fb_log("qt_free\n");

    return 0;
}


HIDDEN int
qt_help(FBIO *ifp)
{
    fb_log("Description: %s\n", qt_interface.if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width/height: %d %d\n",
	   qt_interface.if_max_width,
	   qt_interface.if_max_height);
    fb_log("Default width/height: %d %d\n",
	   qt_interface.if_width,
	   qt_interface.if_height);
    fb_log("Useful for Benchmarking/Debugging\n");
    return 0;
}


FBIO qt_interface =  {
    0,
    qt_open,		/* device_open */
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
    qt_poll,		/* handle events */
    qt_flush,		/* flush output */
    qt_free,		/* free resources */
    qt_help,		/* help message */
    (char *)"Qt Device",/* device description */
    2048,		/* max width */
    2048,		/* max height */
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
    {0}, /* u1 */
    {0}, /* u2 */
    {0}, /* u3 */
    {0}, /* u4 */
    {0}, /* u5 */
    {0}  /* u6 */
};


/**
 * ===================================================== Main window class ===============================================
 */

QMainWindow::QMainWindow(QImage *img, QWindow *win)
    : QWindow(win)
    , m_update_pending(false)
{
    m_backingStore = new QBackingStore(this);
    create();
    image = img;
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
