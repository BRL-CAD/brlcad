/*                     I F _ O S G L . C P P
 * BRL-CAD
 *
 * Copyright (c) 1989-2016 United States Government as represented by
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
/** @addtogroup libfb */
/** @{ */
/** @file if_osgl.cpp
 *
 * An OpenGL framebuffer using OpenSceneGraph.
 *
 * There are several different Frame Buffer modes supported.  Set your
 * environment FB_FILE to the appropriate type.
 *
 * (see the modeflag definitions below).  /dev/osgl[options]
 *
 * This code is basically a port of the 4d Framebuffer interface from
 * IRIS GL to OpenGL, using OpenSceneGraph for portability.
 *
 */
/** @} */


#ifdef IF_OSGL

#include "common.h"
extern "C" {
#include "fb_private.h"
}
#include "fb/fb_osgl.h"

#include <osg/GLExtensions>

#define CJDEBUG 0
#define DIRECT_COLOR_VISUAL_ALLOWED 0

/* XXX - arbitrary upper bound */
#define XMAXSCREEN 16383
#define YMAXSCREEN 16383

HIDDEN int osgl_nwindows = 0; 	/* number of open windows */
/*HIDDEN XColor color_cell[256];*/		/* used to set colormap */


/*
 * Structure of color map in shared memory region.  Has exactly the
 * same format as the SGI hardware "gammaramp" map Note that only the
 * lower 8 bits are significant.
 */
struct osgl_cmap {
    short cmr[256];
    short cmg[256];
    short cmb[256];
};


/*
 * This defines the format of the in-memory framebuffer copy.  The
 * alpha component and reverse order are maintained for compatibility
 * with /dev/sgi
 */
struct osgl_pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
};


/* Clipping structure for zoom/pan operations */
struct osgl_clip {
    int xpixmin;	/* view clipping planes clipped to pixel memory space*/
    int xpixmax;
    int ypixmin;
    int ypixmax;
    int xscrmin;	/* view clipping planes */
    int xscrmax;
    int yscrmin;
    int yscrmax;
    double oleft;	/* glOrtho parameters */
    double oright;
    double otop;
    double obottom;

};


/*
 * Per window state information, overflow area.
 */
struct sgiinfo {
    short mi_cmap_flag;		/* enabled when there is a non-linear map in memory */
    int mi_shmid;
    int mi_memwidth;		/* width of scanline in if_mem */
    short mi_xoff;		/* X viewport offset, rel. window*/
    short mi_yoff;		/* Y viewport offset, rel. window*/
    int mi_pid;			/* for multi-cpu check */
    int mi_parent;		/* PID of linger-mode process */
    struct osgl_pixel mi_scanline[XMAXSCREEN+1];	/* one scanline */
};


/*
 * Per window state information particular to the OpenGL interface
 */
struct osglinfo {
    osgViewer::Viewer *viewer;
    osg::Image *image;
    osg::TextureRectangle *texture;
    osg::Geometry *pictureQuad;
    osg::Group *root;
    osg::Timer *timer;
    int last_update_time;
    int cursor_on;
    int use_texture;

    osg::GraphicsContext *glc;
    osg::GraphicsContext::Traits *traits;
    int firstTime;
    int alive;
    long event_mask;		/* event types to be received */
    int cmap_size;		/* hardware colormap size */
    int win_width;		/* actual window width */
    int win_height;		/* actual window height */
    int vp_width;		/* actual viewport width */
    int vp_height;		/* actual viewport height */
    struct osgl_clip clip;	/* current view clipping */
    /*Window cursor;*/
    /*XVisualInfo *vip;*/	/* pointer to info on current visual */
    /*Colormap xcmap;*/		/* xstyle color map */
    int use_ext_ctrl;		/* for controlling the Ogl graphics engine externally */
};


#define SGI(ptr) ((struct sgiinfo *)((ptr)->u1.p))
#define SGIL(ptr) ((ptr)->u1.p)	/* left hand side version */
#define OSGL(ptr) ((struct osglinfo *)((ptr)->u6.p))
#define OSGLL(ptr) ((ptr)->u6.p)	/* left hand side version */
#define if_mem u2.p		/* shared memory pointer */
#define if_cmap u3.p		/* color map in shared memory */
#define CMR(x) ((struct osgl_cmap *)((x)->if_cmap))->cmr
#define CMG(x) ((struct osgl_cmap *)((x)->if_cmap))->cmg
#define CMB(x) ((struct osgl_cmap *)((x)->if_cmap))->cmb
#define if_zoomflag u4.l	/* zoom > 1 */
#define if_mode u5.l		/* see MODE_* defines */

#define MARGIN 4		/* # pixels margin to screen edge */

#define CLIP_XTRA 1

#define WIN_L (ifp->if_max_width - ifp->if_width - MARGIN)
#define WIN_T (ifp->if_max_height - ifp->if_height - MARGIN)

/*
 * The mode has several independent bits:
 *
 * SHARED -vs- MALLOC'ed memory for the image
 * TRANSIENT -vs- LINGERING windows
 * Windowed -vs- Centered Full screen
 * Suppress dither -vs- dither
 * DrawPixels -vs- CopyPixels
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)	/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)	/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)	/* leave window up after closing*/

#define MODE_4MASK	(1<<3)
#define MODE_4NORMAL	(0<<3)	/* dither if it seems necessary */
#define MODE_4NODITH	(1<<3)	/* suppress any dithering */

#define MODE_7MASK	(1<<6)
#define MODE_7NORMAL	(0<<6)	/* install colormap in hardware if possible*/
#define MODE_7SWCMAP	(1<<6)	/* use software colormapping */

/* and copy current view to front */
#define MODE_15MASK	(1<<14)
#define MODE_15NORMAL	(0<<14)
#define MODE_15ZAP	(1<<14)	/* zap the shared memory segment */
#if 0
HIDDEN struct modeflags {
    const char c;
    long mask;
    long value;
    const char *help;
} modeflags[] = {
    { 'p',	MODE_1MASK, MODE_1MALLOC,
      "Private memory - else shared" },
    { 'l',	MODE_2MASK, MODE_2LINGERING,
      "Lingering window" },
    { 't',	MODE_2MASK, MODE_2TRANSIENT,
      "Transient window" },
    { 'd',  MODE_4MASK, MODE_4NODITH,
      "Suppress dithering - else dither if not 24-bit buffer" },
    { 'c',	MODE_7MASK, MODE_7SWCMAP,
      "Perform software colormap - else use hardware colormap if possible" },
    { 'z',	MODE_15MASK, MODE_15ZAP,
      "Zap (free) shared memory.  Can also be done with fbfree command" },
    { '\0', 0, 0, "" }
};
#endif

/* OpenSceneGraph interactions for a framebuffer viewer */
#include "osg_fb_manipulator.h"

/*
 * Note: unlike sgi_xmit_scanlines, this function updates an arbitrary
 * rectangle of the frame buffer
 */
HIDDEN void
osgl_xmit_scanlines(register fb *ifp, int ybase, int nlines, int xbase, int npix)
{
    register int y;
    register int n;
    int sw_cmap;	/* !0 => needs software color map */
    struct osgl_clip *clp;

    /* Caller is expected to handle attaching context, etc. */

    clp = &(OSGL(ifp)->clip);

    if (SGI(ifp)->mi_cmap_flag) {
	sw_cmap = 1;
    } else {
	sw_cmap = 0;
    }

    if (xbase > clp->xpixmax || ybase > clp->ypixmax)
	return;
    if (xbase < clp->xpixmin)
	xbase = clp->xpixmin;
    if (ybase < clp->ypixmin)
	ybase = clp->ypixmin;

    if ((xbase + npix -1) > clp->xpixmax)
	npix = clp->xpixmax - xbase + 1;
    if ((ybase + nlines - 1) > clp->ypixmax)
	nlines = clp->ypixmax - ybase + 1;

    if (!OSGL(ifp)->use_ext_ctrl) {
	    /*
	     * Blank out areas of the screen around the image, if
	     * exposed.  In COPY mode, this is done in
	     * backbuffer_to_screen().
	     */

	    /* Blank out area left of image */
	    glColor3b(0, 0, 0);
	    if (clp->xscrmin < 0) glRecti(clp->xscrmin - CLIP_XTRA,
					  clp->yscrmin - CLIP_XTRA,
					  CLIP_XTRA,
					  clp->yscrmax + CLIP_XTRA);

	    /* Blank out area below image */
	    if (clp->yscrmin < 0) glRecti(clp->xscrmin - CLIP_XTRA,
					  clp->yscrmin - CLIP_XTRA,
					  clp->xscrmax + CLIP_XTRA,
					  CLIP_XTRA);

	    /* Blank out area right of image */
	    if (clp->xscrmax >= ifp->if_width) glRecti(ifp->if_width - CLIP_XTRA,
						       clp->yscrmin - CLIP_XTRA,
						       clp->xscrmax + CLIP_XTRA,
						       clp->yscrmax + CLIP_XTRA);

	    /* Blank out area above image */
	    if (clp->yscrmax >= ifp->if_height) glRecti(clp->xscrmin - CLIP_XTRA,
							ifp->if_height- CLIP_XTRA,
							clp->xscrmax + CLIP_XTRA,
							clp->yscrmax + CLIP_XTRA);

    }

    if (sw_cmap) {
	/* Software colormap each line as it's transmitted */
	register int x;
	register struct osgl_pixel *osglp;
	register struct osgl_pixel *op;

	y = ybase;
	if (CJDEBUG) printf("Doing sw colormap xmit\n");
	/* Perform software color mapping into temp scanline */
	op = SGI(ifp)->mi_scanline;
	for (n=nlines; n>0; n--, y++) {
	    if (!OSGL(ifp)->viewer) {
		osglp = (struct osgl_pixel *)&ifp->if_mem[(y*SGI(ifp)->mi_memwidth)*sizeof(struct osgl_pixel) ];
	    } else {
		osglp = (struct osgl_pixel *)(OSGL(ifp)->image->data(0,y,0));
	    }
	    for (x=xbase+npix-1; x>=xbase; x--) {
		op[x].red   = CMR(ifp)[osglp[x].red];
		op[x].green = CMG(ifp)[osglp[x].green];
		op[x].blue  = CMB(ifp)[osglp[x].blue];
	    }

	    glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	    glRasterPos2i(xbase, y);
	    glDrawPixels(npix, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
		    (const GLvoid *) op);

	}

    } else {
	/* No need for software colormapping */
	glPixelStorei(GL_UNPACK_ROW_LENGTH, SGI(ifp)->mi_memwidth);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, xbase);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, ybase);

	glRasterPos2i(xbase, ybase);
	glDrawPixels(npix, nlines, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
		(const GLvoid *) ifp->if_mem);
    }
}


HIDDEN void
osgl_cminit(register fb *ifp)
{
    register int i;

    for (i = 0; i < 256; i++) {
	CMR(ifp)[i] = i;
	CMG(ifp)[i] = i;
	CMB(ifp)[i] = i;
    }
}


/******************* Shared Memory Support ******************************/
HIDDEN int
osgl_getmem(fb *ifp)
{
    int shm_result;
    int pixsize;
    int size;
    int i;
    char *sp;
    int new_mem = 0;

    errno = 0;

#ifdef HAVE_SYS_SHM_H
    if ((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC)
#endif
    {
	/*
	 * In this mode, only malloc as much memory as is needed.
	 */
	SGI(ifp)->mi_memwidth = ifp->if_width;
	pixsize = ifp->if_height * ifp->if_width * sizeof(struct osgl_pixel);
	size = pixsize + sizeof(struct osgl_cmap);

	sp = (char *)calloc(1, size);
	if (sp == 0) {
	    fb_log("osgl_getmem: frame buffer memory malloc failed\n");
	    goto fail;
	}
	new_mem = 1;
	goto success;
    }

#ifdef HAVE_SYS_SHM_H
    /* The shared memory section never changes size */
    SGI(ifp)->mi_memwidth = ifp->if_max_width;

    /*
     * On some platforms lrectwrite() runs off the end!  So, provide a
     * pad area of 2 scanlines.  (1 line is enough, but this avoids
     * risk of damage to colormap table.)
     */
    pixsize = (ifp->if_max_height+2) * ifp->if_max_width *
	sizeof(struct osgl_pixel);

    size = pixsize + sizeof(struct osgl_cmap);


    shm_result = bu_shmget(&(SGI(ifp)->mi_shmid), &sp, SHMEM_KEY, (size_t)size);

    if (shm_result == 1) goto fail;
    if (shm_result == -1) new_mem = 1;
#endif

success:
    ifp->if_mem = sp;
    ifp->if_cmap = sp + pixsize;	/* cmap at end of area */
    i = CMB(ifp)[255];		/* try to deref last word */
    CMB(ifp)[255] = i;

    /* Provide non-black colormap on creation of new shared mem */
    if (new_mem)
	osgl_cminit(ifp);
    return 0;
fail:
#ifdef HAVE_SYS_SHM_H
    fb_log("osgl_getmem:  Unable to attach to shared memory.\n");
#endif
    if ((sp = (char *)calloc(1, size)) == NULL) {
	fb_log("osgl_getmem:  malloc failure\n");
	return -1;
    }
    new_mem = 1;
    goto success;
}


void
osgl_zapmem(void)
{
#ifdef HAVE_SYS_SHM_H
    int shmid;
    int i;

    if ((shmid = shmget(SHMEM_KEY, 0, 0)) < 0) {
	fb_log("osgl_zapmem shmget failed, errno=%d\n", errno);
	return;
    }

    i = shmctl(shmid, IPC_RMID, 0);
    if (i < 0) {
	fb_log("osgl_zapmem shmctl failed, errno=%d\n", errno);
	return;
    }
    fb_log("if_osgl: shared memory released\n");
#endif
}


/**
 * Given:- the size of the viewport in pixels (vp_width, vp_height)
 *	 - the size of the framebuffer image (if_width, if_height)
 *	 - the current view center (if_xcenter, if_ycenter)
 * 	 - the current zoom (if_xzoom, if_yzoom)
 * Calculate:
 *	 - the position of the viewport in image space
 *		(xscrmin, xscrmax, yscrmin, yscrmax)
 *	 - the portion of the image which is visible in the viewport
 *		(xpixmin, xpixmax, ypixmin, ypixmax)
 */
void
osgl_clipper(register fb *ifp)
{
    register struct osgl_clip *clp;
    register int i;
    double pixels;

    clp = &(OSGL(ifp)->clip);

    i = OSGL(ifp)->vp_width/(2*ifp->if_xzoom);
    clp->xscrmin = ifp->if_xcenter - i;
    i = OSGL(ifp)->vp_width/ifp->if_xzoom;
    clp->xscrmax = clp->xscrmin + i;
    pixels = (double) i;
    clp->oleft = ((double) clp->xscrmin) - 0.25*pixels/((double) OSGL(ifp)->vp_width);
    clp->oright = clp->oleft + pixels;

    i = OSGL(ifp)->vp_height/(2*ifp->if_yzoom);
    clp->yscrmin = ifp->if_ycenter - i;
    i = OSGL(ifp)->vp_height/ifp->if_yzoom;
    clp->yscrmax = clp->yscrmin + i;
    pixels = (double) i;
    clp->obottom = ((double) clp->yscrmin) - 0.25*pixels/((double) OSGL(ifp)->vp_height);
    clp->otop = clp->obottom + pixels;

    clp->xpixmin = clp->xscrmin;
    clp->xpixmax = clp->xscrmax;
    clp->ypixmin = clp->yscrmin;
    clp->ypixmax = clp->yscrmax;

    if (clp->xpixmin < 0) {
	clp->xpixmin = 0;
    }

    if (clp->ypixmin < 0) {
	clp->ypixmin = 0;
    }

	if (clp->xpixmax > ifp->if_width-1) {
	    clp->xpixmax = ifp->if_width-1;
	}
	if (clp->ypixmax > ifp->if_height-1) {
	    clp->ypixmax = ifp->if_height-1;
	}
    }

int
osgl_configureWindow(fb *ifp, int width, int height)
{
    if (width == OSGL(ifp)->win_width &&
	height == OSGL(ifp)->win_height)
	return 1;

    ifp->if_width = ifp->if_max_width = width;
    ifp->if_height = ifp->if_max_height = height;

    OSGL(ifp)->win_width = OSGL(ifp)->vp_width = width;
    OSGL(ifp)->win_height = OSGL(ifp)->vp_height = height;

    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;
    ifp->if_yzoom = 1;
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;

    osgl_getmem(ifp);
    osgl_clipper(ifp);

    glViewport(0, 0, OSGL(ifp)->win_width, OSGL(ifp)->win_height);
    return 0;
}


HIDDEN void
osgl_do_event(fb *ifp)
{
    if (OSGL(ifp)->firstTime) {
	OSGL(ifp)->firstTime = 0;
    }

    if (OSGL(ifp)->viewer)
	(*OSGL(ifp)->viewer).frame();
}

/**
 * Check for a color map being linear in R, G, and B.  Returns 1 for
 * linear map, 0 for non-linear map (i.e., non-identity map).
 */
HIDDEN int
is_linear_cmap(register fb *ifp)
{
    register int i;

    for (i = 0; i < 256; i++) {
	if (CMR(ifp)[i] != i) return 0;
	if (CMG(ifp)[i] != i) return 0;
	if (CMB(ifp)[i] != i) return 0;
    }
    return 1;
}


HIDDEN int
fb_osgl_open(fb *ifp, const char *UNUSED(file), int width, int height)
{
    FB_CK_FB(ifp);

    ifp->if_mode = MODE_2LINGERING;

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("fb_osgl_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((ifp->u6.p = (char *)calloc(1, sizeof(struct osglinfo))) == NULL) {
	fb_log("fb_osgl_open:  osglinfo malloc failed\n");
	return -1;
    }

    /* use defaults if invalid width and height specified */
    if (width > 0)
	ifp->if_width = width;
    if (height > 0)
	ifp->if_height = height;

    /* use max values if width and height are greater */
    if (width > ifp->if_max_width)
	ifp->if_width = ifp->if_max_width;
    if (height > ifp->if_max_height)
	ifp->if_height = ifp->if_max_height;

    /* initialize window state variables before calling osgl_getmem */
    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;	/* for zoom fakeout */
    ifp->if_yzoom = 1;	/* for zoom fakeout */
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (osgl_getmem(ifp) < 0)
	return -1;

    /* Make sure OpenSceneGraph knows to look in the root lib directory */
    /* TODO - The OpenSceneGraph logic for handling plugins isn't multi-config
     * aware - we're going to have to add that to make this mechanism work on Windows */
    {
	std::string rel_path = std::string(bu_brlcad_dir("lib", 0)) + std::string("/osgPlugins");
	const char *root_path = bu_brlcad_root(rel_path.c_str(), 0);
	osgDB::FilePathList paths = osgDB::Registry::instance()->getLibraryFilePathList();
	if (root_path) {
	    std::string libpathstring(root_path);
	    /* The first entry is the final installed path - prefer that to the local
	     * bu_brlcad_root lib directory.  This means our new path should be the
	     * second entry in the list - insert it accordingly. */
	    osgDB::FilePathList::iterator in_itr=++(paths.begin());
	    paths.insert(in_itr, libpathstring);
	    osgDB::Registry::instance()->setLibraryFilePathList(paths);
	}
	//for(osgDB::FilePathList::const_iterator libpath=osgDB::Registry::instance()->getLibraryFilePathList().begin(); libpath!=osgDB::Registry::instance()->getLibraryFilePathList().end(); ++libpath) std::cout << *libpath << "\n";
    }


    OSGL(ifp)->timer = new osg::Timer;
    OSGL(ifp)->last_update_time = 0;

    OSGL(ifp)->viewer = new osgViewer::Viewer();
    int woffset = 40;
    osgViewer::SingleWindow *sw = new osgViewer::SingleWindow(0+woffset, 0+woffset, ifp->if_width, ifp->if_height);
    OSGL(ifp)->viewer->apply(sw);
    osg::Camera *camera = OSGL(ifp)->viewer->getCamera();
    camera->setClearColor(osg::Vec4(0.0f,0.0f,0.0f,1.0f));
    camera->setViewMatrix(osg::Matrix::identity());

    OSGL(ifp)->root = new osg::Group;
    OSGL(ifp)->viewer->setSceneData(OSGL(ifp)->root);

    /* Need to realize and render a frame before we inquire what GL extensions are supported,
     * or Bad Things happen to the OpenGL state */
    OSGL(ifp)->use_texture = 0;
    OSGL(ifp)->viewer->realize();
    OSGL(ifp)->viewer->frame();
    OSGL(ifp)->glc = camera->getGraphicsContext();
    unsigned int contextID = OSGL(ifp)->glc->getState()->getContextID();
    bool have_pixbuff = osg::isGLExtensionSupported(contextID, "GL_ARB_pixel_buffer_object");
    if (have_pixbuff) {
	std::cout << "Have GL_ARB_pixel_buffer_object\n";
    }

    OSGL(ifp)->image = new osg::Image;
    OSGL(ifp)->image->setImage(ifp->if_width, ifp->if_height, 1, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, (unsigned char *)ifp->if_mem, osg::Image::NO_DELETE);
    OSGL(ifp)->image->setPixelBufferObject(new osg::PixelBufferObject(OSGL(ifp)->image));
    OSGL(ifp)->pictureQuad = osg::createTexturedQuadGeometry(osg::Vec3(0.0f,0.0f,0.0f),
	    osg::Vec3(ifp->if_width,0.0f,0.0f), osg::Vec3(0.0f,0.0f, ifp->if_height), 0.0f, 0.0, OSGL(ifp)->image->s(), OSGL(ifp)->image->t());
    OSGL(ifp)->texture = new osg::TextureRectangle(OSGL(ifp)->image);
    /*OSGL(ifp)->texture->setFilter(osg::Texture::MIN_FILTER,osg::Texture::LINEAR);
    OSGL(ifp)->texture->setFilter(osg::Texture::MAG_FILTER,osg::Texture::LINEAR);
    OSGL(ifp)->texture->setWrap(osg::Texture::WRAP_R,osg::Texture::REPEAT);*/
    OSGL(ifp)->pictureQuad->getOrCreateStateSet()->setTextureAttributeAndModes(0, OSGL(ifp)->texture, osg::StateAttribute::ON);

    if (have_pixbuff) {
	OSGL(ifp)->use_texture = 1;
	osg::Geode *geode = new osg::Geode;
	osg::StateSet* stateset = geode->getOrCreateStateSet();
	stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
	geode->addDrawable(OSGL(ifp)->pictureQuad);
	OSGL(ifp)->root->addChild(geode);
	osg::Vec3 topleft(0.0f, 0.0f, 0.0f);
	osg::Vec3 bottomright(ifp->if_width, ifp->if_height, 0.0f);
	camera->setProjectionMatrixAsOrtho2D(-ifp->if_width/2,ifp->if_width/2,-ifp->if_height/2, ifp->if_height/2);
    } else {
	/* Emulate xmit_scanlines drawing in OSG as a fallback... */
	OSGL(ifp)->use_texture = 0;
	osg::Vec3 topleft(0.0f, 0.0f, 0.0f);
	osg::Vec3 bottomright(ifp->if_width, ifp->if_height, 0.0f);
	camera->setProjectionMatrixAsOrtho2D(-ifp->if_width/2,ifp->if_width/2,-ifp->if_height/2, ifp->if_height/2);
    }

    OSGL(ifp)->viewer->setCameraManipulator( new osgGA::FrameBufferManipulator() );
    OSGL(ifp)->viewer->addEventHandler(new osgGA::StateSetManipulator(OSGL(ifp)->viewer->getCamera()->getOrCreateStateSet()));

    KeyHandler *kh = new KeyHandler(*(OSGL(ifp)->root));
    kh->fbp = ifp;
    OSGL(ifp)->viewer->addEventHandler(kh);

    OSGL(ifp)->cursor_on = 1;


    OSGL(ifp)->timer->setStartTick();
    /* count windows */
    osgl_nwindows++;

    OSGL(ifp)->alive = 1;
    OSGL(ifp)->firstTime = 1;

    return 0;

}



int
_osgl_open_existing(fb *ifp, int width, int height, void *glc, void *traits)
{

    /*XXX for now use private memory */
    ifp->if_mode = MODE_1MALLOC;

    /*
     * Allocate extension memory sections,
     * addressed by SGI(ifp)->mi_xxx and OSGL(ifp)->xxx
     */

    if ((SGIL(ifp) = (char *)calloc(1, sizeof(struct sgiinfo))) == NULL) {
	fb_log("fb_osgl_open:  sgiinfo malloc failed\n");
	return -1;
    }
    if ((OSGLL(ifp) = (char *)calloc(1, sizeof(struct osglinfo))) == NULL) {
	fb_log("fb_osgl_open:  osglinfo malloc failed\n");
	return -1;
    }

    OSGL(ifp)->use_ext_ctrl = 1;

    SGI(ifp)->mi_shmid = -1;	/* indicate no shared memory */
    ifp->if_width = ifp->if_max_width = width;
    ifp->if_height = ifp->if_max_height = height;

    OSGL(ifp)->win_width = OSGL(ifp)->vp_width = width;
    OSGL(ifp)->win_height = OSGL(ifp)->vp_height = height;

    OSGL(ifp)->cursor_on = 1;

    /* initialize window state variables before calling osgl_getmem */
    ifp->if_zoomflag = 0;
    ifp->if_xzoom = 1;	/* for zoom fakeout */
    ifp->if_yzoom = 1;	/* for zoom fakeout */
    ifp->if_xcenter = width/2;
    ifp->if_ycenter = height/2;
    SGI(ifp)->mi_pid = bu_process_id();

    /* Attach to shared memory, potentially with a screen repaint */
    if (osgl_getmem(ifp) < 0)
	return -1;

    OSGL(ifp)->viewer = NULL;
    OSGL(ifp)->glc = (osg::GraphicsContext *)glc;
    OSGL(ifp)->traits = (osg::GraphicsContext::Traits *)traits;

    ++osgl_nwindows;

    OSGL(ifp)->alive = 1;
    OSGL(ifp)->firstTime = 1;

    osgl_clipper(ifp);

    return 0;
}

HIDDEN struct fb_platform_specific *
osgl_get_fbps(uint32_t magic)
{
    struct fb_platform_specific *fb_ps = NULL;
    struct osgl_fb_info *data = NULL;
    BU_GET(fb_ps, struct fb_platform_specific);
    BU_GET(data, struct osgl_fb_info);
    fb_ps->magic = magic;
    fb_ps->data = data;
    return fb_ps;
}


HIDDEN void
osgl_put_fbps(struct fb_platform_specific *fbps)
{
    BU_CKMAG(fbps, FB_OSGL_MAGIC, "osgl framebuffer");
    BU_PUT(fbps->data, struct osgl_fb_info);
    BU_PUT(fbps, struct fb_platform_specific);
    return;
}

HIDDEN int
osgl_open_existing(fb *ifp, int width, int height, struct fb_platform_specific *fb_p)
{
    struct osgl_fb_info *osgl_internal = (struct osgl_fb_info *)fb_p->data;
    BU_CKMAG(fb_p, FB_OSGL_MAGIC, "osgl framebuffer");
    return _osgl_open_existing(ifp, width, height, osgl_internal->glc, osgl_internal->traits);
}


HIDDEN int
osgl_final_close(fb *ifp)
{

    if (CJDEBUG) {
	printf("osgl_final_close: All done...goodbye!\n");
    }

    if (OSGL(ifp)->viewer) {
	OSGL(ifp)->viewer->setDone(true);
    } else {
	if (OSGL(ifp)->glc) {
	    OSGL(ifp)->glc->makeCurrent();
	    OSGL(ifp)->glc->releaseContext();
	}
    }

    if (SGIL(ifp) != NULL) {
	/* free up memory associated with image */
#ifdef HAVE_SYS_SHM_H
	if (SGI(ifp)->mi_shmid != -1) {
	    /* detach from shared memory */
	    if (shmdt(ifp->if_mem) == -1) {
		fb_log("fb_osgl_close shmdt failed, errno=%d\n",
		       errno);
		return -1;
	    }
	} else {
#endif
	    /* free private memory */
	    (void)free(ifp->if_mem);
#ifdef HAVE_SYS_SHM_H
	}
#endif
	/* free state information */
	(void)free((char *)SGIL(ifp));
	SGIL(ifp) = NULL;
    }

    if (OSGLL(ifp) != NULL) {
	(void)free((char *)OSGLL(ifp));
	OSGLL(ifp) = NULL;
    }

    osgl_nwindows--;
    return 0;
}


HIDDEN int
osgl_flush(fb *UNUSED(ifp))
{
    glFlush();
    return 0;
}


HIDDEN int
fb_osgl_close(fb *ifp)
{
    osgl_flush(ifp);

    /* only the last open window can linger -
     * call final_close if not lingering
     */
    if (osgl_nwindows > 1 ||
	    (ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT)
	return osgl_final_close(ifp);

    if (CJDEBUG)
	printf("fb_osgl_close: remaining open to linger awhile.\n");

    /*
     * else:
     *
     * LINGER mode.  Don't return to caller until user mouses "close"
     * menu item.  This may delay final processing in the calling
     * function for some time, but the assumption is that the user
     * wishes to compare this image with others.
     *
     * Since we plan to linger here, long after our invoker expected
     * us to be gone, be certain that no file descriptors remain open
     * to associate us with pipelines, network connections, etc., that
     * were ALREADY ESTABLISHED before the point that fb_open() was
     * called.
     *
     * The simple for i=0..20 loop will not work, because that smashes
     * some window-manager files.  Therefore, we content ourselves
     * with eliminating stdin, in the hopes that this will
     * successfully terminate any pipes or network connections.
     * Standard error/out may be used to print framebuffer debug
     * messages, so they're kept around.
     */
    fclose(stdin);

    if (OSGL(ifp)->viewer) {
	return (*OSGL(ifp)->viewer).ViewerBase::run();
    } else {
	/* Shouldn't get here - lingering windows should be using the viewer,
	 * and embedded ones should use osgl_close_existing */
	return 0;
    }
}


int
osgl_close_existing(fb *ifp)
{
    if (SGIL(ifp) != NULL) {
#ifdef HAVE_SYS_SHM_H
	/* free up memory associated with image */
	if (SGI(ifp)->mi_shmid != -1) {
	    /* detach from shared memory */
	    if (shmdt(ifp->if_mem) == -1) {
		fb_log("fb_osgl_close: shmdt failed, errno=%d\n",
		       errno);
		return -1;
	    }
	} else {
#endif
	    /* free private memory */
	    (void)free(ifp->if_mem);
#ifdef HAVE_SYS_SHM_H
	}
#endif
	/* free state information */
	(void)free((char *)SGIL(ifp));
	SGIL(ifp) = NULL;
    }

    if (OSGLL(ifp) != NULL) {
	(void)free((char *)OSGLL(ifp));
	OSGLL(ifp) = NULL;
    }

    return 0;
}


/*
 * Handle any pending input events
 */
HIDDEN int
osgl_poll(fb *ifp)
{
    osgl_do_event(ifp);

    if (OSGL(ifp)->alive < 0)
	return 1;
    else
	return 0;
}


/*
 * Free shared memory resources, and close.
 */
HIDDEN int
osgl_free(fb *ifp)
{
    int ret;

    if (CJDEBUG) printf("entering osgl_free\n");
    /* Close the framebuffer */
    ret = osgl_final_close(ifp);

    if ((ifp->if_mode & MODE_1MASK) == MODE_1SHARED) {
	/* If shared mem, release the shared memory segment */
	osgl_zapmem();
    }
    return ret;
}


HIDDEN int
osgl_clear(fb *ifp, unsigned char *pp)
{
    struct osgl_pixel bg;
    register struct osgl_pixel *osglp;
    register int cnt;
    register int y;

    if (CJDEBUG) printf("entering osgl_clear\n");

    /* Set clear colors */
    if (pp != RGBPIXEL_NULL) {
	bg.alpha = 0;
	bg.red   = (pp)[RED];
	bg.green = (pp)[GRN];
	bg.blue  = (pp)[BLU];
    } else {
	bg.alpha = 0;
	bg.red   = 0;
	bg.green = 0;
	bg.blue  = 0;
    }

    /* Flood rectangle in shared memory */
    for (y = 0; y < ifp->if_height; y++) {
	if (!OSGL(ifp)->viewer) {
	    osglp = (struct osgl_pixel *)&ifp->if_mem[(y*SGI(ifp)->mi_memwidth+0)*sizeof(struct osgl_pixel) ];
	} else {
	    osglp = (struct osgl_pixel *)(OSGL(ifp)->image->data(0,y,0));
	}
	for (cnt = ifp->if_width-1; cnt >= 0; cnt--) {
	    *osglp++ = bg;	/* struct copy */
	}
    }

    if (OSGL(ifp)->use_ext_ctrl) {
	return 0;
    }

    if (OSGL(ifp)->viewer) {
	OSGL(ifp)->image->dirty();
	if (OSGL(ifp)->timer->time_m() - OSGL(ifp)->last_update_time > 10) {
	    OSGL(ifp)->viewer->frame();
	    OSGL(ifp)->last_update_time = OSGL(ifp)->timer->time_m();
	}
    } else {
	OSGL(ifp)->glc->makeCurrent();

	if (pp != RGBPIXEL_NULL) {
	    glClearColor(pp[RED]/255.0, pp[GRN]/255.0, pp[BLU]/255.0, 0.0);
	} else {
	    glClearColor(0, 0, 0, 0);
	}

	glClear(GL_COLOR_BUFFER_BIT);
	OSGL(ifp)->glc->swapBuffers();

	/* unattach context for other threads to use */
	OSGL(ifp)->glc->releaseContext();
    }

    return 0;
}


HIDDEN int
osgl_view(fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
    struct osgl_clip *clp;

    if (CJDEBUG) printf("entering osgl_view\n");

    if (xzoom < 1) xzoom = 1;
    if (yzoom < 1) yzoom = 1;
    if (ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	&& ifp->if_xzoom == xzoom && ifp->if_yzoom == yzoom)
	return 0;

    if (xcenter < 0 || xcenter >= ifp->if_width)
	return -1;
    if (ycenter < 0 || ycenter >= ifp->if_height)
	return -1;
    if (xzoom >= ifp->if_width || yzoom >= ifp->if_height)
	return -1;

    ifp->if_xcenter = xcenter;
    ifp->if_ycenter = ycenter;
    ifp->if_xzoom = xzoom;
    ifp->if_yzoom = yzoom;

    if (ifp->if_xzoom > 1 || ifp->if_yzoom > 1)
	ifp->if_zoomflag = 1;
    else ifp->if_zoomflag = 0;


    if (OSGL(ifp)->use_ext_ctrl) {
	osgl_clipper(ifp);
    } else {

	if (!OSGL(ifp)->viewer && OSGL(ifp)->glc) {
	    OSGL(ifp)->glc->makeCurrent();

	    /* Set clipping matrix and zoom level */
	    glMatrixMode(GL_PROJECTION);
	    glLoadIdentity();

	    osgl_clipper(ifp);
	    clp = &(OSGL(ifp)->clip);
	    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
	    glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

	    osgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    OSGL(ifp)->glc->swapBuffers();
	    glFlush();

	    /* unattach context for other threads to use */
	    OSGL(ifp)->glc->releaseContext();
	}
    }

    return 0;
}


HIDDEN int
osgl_getview(fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
    if (CJDEBUG) printf("entering osgl_getview\n");

    *xcenter = ifp->if_xcenter;
    *ycenter = ifp->if_ycenter;
    *xzoom = ifp->if_xzoom;
    *yzoom = ifp->if_yzoom;

    return 0;
}


/* read count pixels into pixelp starting at x, y */
HIDDEN ssize_t
osgl_read(fb *ifp, int x, int y, unsigned char *pixelp, size_t count)
{
    size_t n;
    size_t scan_count;	/* # pix on this scanline */
    register unsigned char *cp;
    ssize_t ret;
    register struct osgl_pixel *osglp;

    if (CJDEBUG) printf("entering osgl_read\n");

    if (x < 0 || x >= ifp->if_width ||
	y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;
    cp = (unsigned char *)(pixelp);

    while (count) {
	if (y >= ifp->if_height)
	    break;

	if (count >= (size_t)(ifp->if_width-x))
	    scan_count = ifp->if_width-x;
	else
	    scan_count = count;

	if (!OSGL(ifp)->viewer) {
	    osglp = (struct osgl_pixel *)&ifp->if_mem[(y*SGI(ifp)->mi_memwidth+x)*sizeof(struct osgl_pixel) ];
	} else {
	    osglp = (struct osgl_pixel *)(OSGL(ifp)->image->data(0,y,0));
	}

	n = scan_count;
	while (n) {
	    cp[RED] = osglp->red;
	    cp[GRN] = osglp->green;
	    cp[BLU] = osglp->blue;
	    osglp++;
	    cp += 3;
	    n--;
	}
	ret += scan_count;
	count -= scan_count;
	x = 0;
	/* Advance upwards */
	if (++y >= ifp->if_height)
	    break;
    }
    return ret;
}


/* write count pixels from pixelp starting at xstart, ystart */
HIDDEN ssize_t
osgl_write(fb *ifp, int xstart, int ystart, const unsigned char *pixelp, size_t count)
{
    register int x;
    register int y;
    size_t scan_count;  /* # pix on this scanline */
    size_t pix_count;   /* # pixels to send */
    ssize_t ret;

    FB_CK_FB(ifp);

    if (CJDEBUG) printf("entering osgl_write\n");

    /* fast exit cases */
    pix_count = count;
    if (pix_count == 0)
	return 0;	/* OK, no pixels transferred */

    x = xstart;
    y = ystart;

    if (x < 0 || x >= ifp->if_width ||
	    y < 0 || y >= ifp->if_height)
	return -1;

    ret = 0;

    if (OSGL(ifp)->viewer) {

	while (pix_count) {
	    void *scanline;

	    if (y >= ifp->if_height)
		break;

	    if (pix_count >= (size_t)(ifp->if_width-x))
		scan_count = (size_t)(ifp->if_width-x);
	    else
		scan_count = pix_count;

	    if (OSGL(ifp)->use_texture) {
		scanline = (void *)(OSGL(ifp)->image->data(0,y,0));
		memcpy(scanline, pixelp, scan_count*3);
	    } else {
		/* Emulate xmit_scanlines drawing in OSG as a fallback when textures don't work... */
		osg::ref_ptr<osg::Image> scanline_image = new osg::Image;
		scanline_image->allocateImage(ifp->if_width, 1, 1, GL_RGB, GL_UNSIGNED_BYTE);
		scanline = (void *)scanline_image->data();
		memcpy(scanline, pixelp, scan_count*3);
		osg::ref_ptr<osg::DrawPixels> scanline_obj = new osg::DrawPixels;
		scanline_obj->setPosition(osg::Vec3(-ifp->if_width/2, 0, -ifp->if_height/2 + y));
		scanline_obj->setImage(scanline_image);
		osg::ref_ptr<osg::Geode> new_geode = new osg::Geode;
		osg::StateSet* stateset = new_geode->getOrCreateStateSet();
		stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
		new_geode->addDrawable(scanline_obj.get());
		OSGL(ifp)->root->addChild(new_geode.get());
	    }
	    ret += scan_count;
	    pix_count -= scan_count;
	    x = 0;
	    if (++y >= ifp->if_height)
		break;
	}

	OSGL(ifp)->image->dirty();
	if (OSGL(ifp)->timer->time_m() - OSGL(ifp)->last_update_time > 10) {
	    OSGL(ifp)->viewer->frame();
	    OSGL(ifp)->last_update_time = OSGL(ifp)->timer->time_m();
	}

	return ret;

    } else {

	register unsigned char *cp;
	int ybase;

	ybase = ystart;
	cp = (unsigned char *)(pixelp);

	while (pix_count) {
	    size_t n;
	    register struct osgl_pixel *osglp;

	    if (y >= ifp->if_height)
		break;

	    if (pix_count >= (size_t)(ifp->if_width-x))
		scan_count = (size_t)(ifp->if_width-x);
	    else
		scan_count = pix_count;

	    if (!OSGL(ifp)->viewer) {
		osglp = (struct osgl_pixel *)&ifp->if_mem[(y*SGI(ifp)->mi_memwidth+x)*sizeof(struct osgl_pixel) ];
	    } else {
		osglp = (struct osgl_pixel *)(OSGL(ifp)->image->data(0,y,0));
	    }

	    n = scan_count;
	    if ((n & 3) != 0) {
		/* This code uses 60% of all CPU time */
		while (n) {
		    /* alpha channel is always zero */
		    osglp->red   = cp[RED];
		    osglp->green = cp[GRN];
		    osglp->blue  = cp[BLU];
		    osglp++;
		    cp += 3;
		    n--;
		}
	    } else {
		while (n) {
		    /* alpha channel is always zero */
		    osglp[0].red   = cp[RED+0*3];
		    osglp[0].green = cp[GRN+0*3];
		    osglp[0].blue  = cp[BLU+0*3];
		    osglp[1].red   = cp[RED+1*3];
		    osglp[1].green = cp[GRN+1*3];
		    osglp[1].blue  = cp[BLU+1*3];
		    osglp[2].red   = cp[RED+2*3];
		    osglp[2].green = cp[GRN+2*3];
		    osglp[2].blue  = cp[BLU+2*3];
		    osglp[3].red   = cp[RED+3*3];
		    osglp[3].green = cp[GRN+3*3];
		    osglp[3].blue  = cp[BLU+3*3];
		    osglp += 4;
		    cp += 3*4;
		    n -= 4;
		}
	    }
	    ret += scan_count;
	    pix_count -= scan_count;
	    x = 0;
	    if (++y >= ifp->if_height)
		break;
	}

	if (!OSGL(ifp)->use_ext_ctrl) {
	    OSGL(ifp)->glc->makeCurrent();
	    if (xstart + count < (size_t)ifp->if_width) {
		osgl_xmit_scanlines(ifp, ybase, 1, xstart, count);
	    } else {
		/* Normal case -- multi-pixel write */
		osgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    }
	    OSGL(ifp)->glc->swapBuffers();
	    glFlush();
	    /* unattach context for other threads to use */
	    OSGL(ifp)->glc->releaseContext();
	}

	return ret;

    }

    return 0;
}


/*
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
osgl_writerect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct osgl_pixel *osglp;

    if (CJDEBUG) printf("entering osgl_writerect\n");

    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	if (!OSGL(ifp)->viewer) {
	    osglp = (struct osgl_pixel *)&ifp->if_mem[(y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct osgl_pixel) ];
	} else {
	    osglp = (struct osgl_pixel *)(OSGL(ifp)->image->data(0,y,0));
	}
	for (x = xmin; x < xmin+width; x++) {
	    /* alpha channel is always zero */
	    osglp->red   = cp[RED];
	    osglp->green = cp[GRN];
	    osglp->blue  = cp[BLU];
	    osglp++;
	    cp += 3;
	}
    }

    if (!OSGL(ifp)->use_ext_ctrl) {
	if (OSGL(ifp)->viewer) {
	    OSGL(ifp)->image->dirty();
	    if (OSGL(ifp)->timer->time_m() - OSGL(ifp)->last_update_time > 10) {
		OSGL(ifp)->viewer->frame();
		OSGL(ifp)->last_update_time = OSGL(ifp)->timer->time_m();
	    }
	} else {
	    OSGL(ifp)->glc->makeCurrent();
	    osgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    OSGL(ifp)->glc->swapBuffers();
	    /* unattach context for other threads to use */
	    OSGL(ifp)->glc->releaseContext();
	}
    }

    return width*height;
}


/*
 * The task of this routine is to reformat the pixels into SGI
 * internal form, and then arrange to have them sent to the screen
 * separately.
 */
HIDDEN int
osgl_bwwriterect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
    register int x;
    register int y;
    register unsigned char *cp;
    register struct osgl_pixel *osglp;

    if (CJDEBUG) printf("entering osgl_bwwriterect\n");

    if (width <= 0 || height <= 0)
	return 0;  /* do nothing */
    if (xmin < 0 || xmin+width > ifp->if_width ||
	ymin < 0 || ymin+height > ifp->if_height)
	return -1; /* no can do */

    cp = (unsigned char *)(pp);
    for (y = ymin; y < ymin+height; y++) {
	if (!OSGL(ifp)->viewer) {
	    osglp = (struct osgl_pixel *)&ifp->if_mem[(y*SGI(ifp)->mi_memwidth+xmin)*sizeof(struct osgl_pixel) ];
	} else {
	    osglp = (struct osgl_pixel *)(OSGL(ifp)->image->data(0,y,0));
	}
	for (x = xmin; x < xmin+width; x++) {
	    register int val;
	    /* alpha channel is always zero */
	    osglp->red   = (val = *cp++);
	    osglp->green = val;
	    osglp->blue  = val;
	    osglp++;
	}
    }

    if (!OSGL(ifp)->use_ext_ctrl) {
	if (OSGL(ifp)->viewer) {
	    OSGL(ifp)->image->dirty();
	    if (OSGL(ifp)->timer->time_m() - OSGL(ifp)->last_update_time > 10) {
		OSGL(ifp)->viewer->frame();
		OSGL(ifp)->last_update_time = OSGL(ifp)->timer->time_m();
	    }
	} else {
	    OSGL(ifp)->glc->makeCurrent();
	    osgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    OSGL(ifp)->glc->swapBuffers();
	    /* unattach context for other threads to use */
	    OSGL(ifp)->glc->releaseContext();
	}
    }

    return width*height;
}


HIDDEN int
osgl_rmap(register fb *ifp, register ColorMap *cmp)
{
    register int i;

    if (CJDEBUG) printf("entering osgl_rmap\n");

    /* Just parrot back the stored colormap */
    for (i = 0; i < 256; i++) {
	cmp->cm_red[i]   = CMR(ifp)[i]<<8;
	cmp->cm_green[i] = CMG(ifp)[i]<<8;
	cmp->cm_blue[i]  = CMB(ifp)[i]<<8;
    }
    return 0;
}


HIDDEN int
osgl_wmap(register fb *ifp, register const ColorMap *cmp)
{
    register int i;
    int prev;	/* !0 = previous cmap was non-linear */

    if (CJDEBUG) printf("entering osgl_wmap\n");

    prev = SGI(ifp)->mi_cmap_flag;
    if (cmp == COLORMAP_NULL) {
	osgl_cminit(ifp);
    } else {
	for (i = 0; i < 256; i++) {
	    CMR(ifp)[i] = cmp-> cm_red[i]>>8;
	    CMG(ifp)[i] = cmp-> cm_green[i]>>8;
	    CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
	}
    }
    SGI(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);


    if (!OSGL(ifp)->use_ext_ctrl) {
	/* if current and previous maps are linear, return */
	if (SGI(ifp)->mi_cmap_flag == 0 && prev == 0) return 0;

	/* Software color mapping, trigger a repaint */
	if (OSGL(ifp)->viewer) {
	    OSGL(ifp)->image->dirty();
	    if (OSGL(ifp)->timer->time_m() - OSGL(ifp)->last_update_time > 10) {
		OSGL(ifp)->viewer->frame();
		OSGL(ifp)->last_update_time = OSGL(ifp)->timer->time_m();
	    }
	} else {
	    OSGL(ifp)->glc->makeCurrent();
	    osgl_xmit_scanlines(ifp, 0, ifp->if_height, 0, ifp->if_width);
	    OSGL(ifp)->glc->swapBuffers();
	    /* unattach context for other threads to use, also flushes */
	    OSGL(ifp)->glc->releaseContext();
	}
    }

    return 0;
}


HIDDEN int
osgl_help(fb *ifp)
{
    fb_log("Description: %s\n", ifp->if_type);
    fb_log("Device: %s\n", ifp->if_name);
    fb_log("Max width height: %d %d\n",
	   ifp->if_max_width,
	   ifp->if_max_height);
    fb_log("Default width height: %d %d\n",
	   ifp->if_width,
	   ifp->if_height);
    fb_log("Usage: /dev/osgl\n");

    fb_log("\nCurrent internal state:\n");
    fb_log("	mi_cmap_flag=%d\n", SGI(ifp)->mi_cmap_flag);
    fb_log("	osgl_nwindows=%d\n", osgl_nwindows);
    return 0;
}


HIDDEN int
osgl_setcursor(fb *ifp, const unsigned char *UNUSED(bits), int UNUSED(xbits), int UNUSED(ybits), int UNUSED(xorig), int UNUSED(yorig))
{
    FB_CK_FB(ifp);

    // If it should ever prove desirable to alter the cursor or disable it, here's how it is done:
    // dynamic_cast<osgViewer::GraphicsWindow*>(camera->getGraphicsContext()))->setCursor(osgViewer::GraphicsWindow::NoCursor);

    return 0;
}


HIDDEN int
osgl_cursor(fb *UNUSED(ifp), int UNUSED(mode), int UNUSED(x), int UNUSED(y))
{

    fb_log("osgl_cursor\n");
    return 0;
}


int
osgl_refresh(fb *ifp, int x, int y, int w, int h)
{
    int mm;
    struct osgl_clip *clp;

    if (w < 0) {
	w = -w;
	x -= w;
    }

    if (h < 0) {
	h = -h;
	y -= h;
    }

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    osgl_clipper(ifp);
    clp = &(OSGL(ifp)->clip);
    glOrtho(clp->oleft, clp->oright, clp->obottom, clp->otop, -1.0, 1.0);
    glPixelZoom((float) ifp->if_xzoom, (float) ifp->if_yzoom);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glViewport(0, 0, OSGL(ifp)->win_width, OSGL(ifp)->win_height);
    osgl_xmit_scanlines(ifp, y, h, x, w);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(mm);

    if (!OSGL(ifp)->use_ext_ctrl) {
	glFlush();
    }

    return 0;
}


/* This is the ONLY thing that we normally "export" */
fb osgl_interface =
{
    0,			/* magic number slot */
    FB_OSGL_MAGIC,
    fb_osgl_open,	/* open device */
    osgl_open_existing,    /* existing device_open */
    osgl_close_existing,    /* existing device_close */
    osgl_get_fbps,         /* get platform specific memory */
    osgl_put_fbps,         /* free platform specific memory */
    fb_osgl_close,	/* close device */
    osgl_clear,		/* clear device */
    osgl_read,		/* read pixels */
    osgl_write,		/* write pixels */
    osgl_rmap,		/* read colormap */
    osgl_wmap,		/* write colormap */
    osgl_view,		/* set view */
    osgl_getview,	/* get view */
    osgl_setcursor,	/* define cursor */
    osgl_cursor,		/* set cursor */
    fb_sim_getcursor,	/* get cursor */
    fb_sim_readrect,	/* read rectangle */
    osgl_writerect,	/* write rectangle */
    fb_sim_bwreadrect,
    osgl_bwwriterect,	/* write rectangle */
    osgl_configureWindow,
    osgl_refresh,
    osgl_poll,		/* process events */
    osgl_flush,		/* flush output */
    osgl_free,		/* free resources */
    osgl_help,		/* help message */
    bu_strdup("OpenSceneGraph OpenGL"),	/* device description */
    XMAXSCREEN+1,	/* max width */
    YMAXSCREEN+1,	/* max height */
    bu_strdup("/dev/osgl"),		/* short device name */
    512,		/* default/current width */
    512,		/* default/current height */
    -1,			/* select file desc */
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

/* Because class is actually used to access a struct
 * entry in this file, preserve our redefinition
 * of class for the benefit of avoiding C++ name
 * collisions until the end of this file */
#undef class

#else

/* quell empty-compilation unit warnings */
static const int unused = 0;

#endif /* IF_OSGL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
