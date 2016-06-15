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
    QMainWindow *win;
    QImage *qi_image;

    int alive;

    unsigned long qi_mode;
    unsigned long qi_flags;

    unsigned char *qi_mem;
    unsigned char *qi_pix;

    int qi_iwidth;
    int qi_iheight;

    int qi_qwidth;	/* Width of Qwindow */
    int qi_qheight;	/* Height of Qwindow */
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
qt_update(FBIO *ifp, int x1, int y1, int w, int h)
{
    struct qtinfo *qi = QI(ifp);

    memcpy(&(qi->qi_pix[((qi->qi_iheight - y1)*qi->qi_iwidth+x1)*sizeof(RGBpixel)]),
 	   &(qi->qi_mem[(y1*qi->qi_iwidth+x1)*sizeof(RGBpixel)]), w * h * sizeof(RGBpixel));

    QApplication::sendEvent(qi->win, new QEvent(QEvent::UpdateRequest));
    qi->qapp->processEvents();
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

    if ((qi->qi_pix = (unsigned char *) calloc(width*height*sizeof(RGBpixel),
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

    qi->alive = 1;

    /* Mark display ready */
    qi->qi_flags |= FLG_INIT;

    fb_log("qt_open\n");

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
    memcpy(&(qi->qi_mem[(y*qi->qi_iwidth+x)*sizeof(RGBpixel)]),
	   pixelp, count*sizeof(RGBpixel));

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
qt_flush(FBIO *UNUSED(ifp))
{
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
