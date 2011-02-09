/* $Id$ */

/* vi:set sw=4 expandtab: */

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

/* TODO: fullscreen support */

#undef DEBUG_GLX

static PFNGLXCHOOSEFBCONFIGPROC chooseFBConfig = NULL;
static PFNGLXGETFBCONFIGATTRIBPROC getFBConfigAttrib = NULL;
static PFNGLXGETVISUALFROMFBCONFIGPROC getVisualFromFBConfig = NULL;
static PFNGLXCREATEPBUFFERPROC createPbuffer = NULL;
static PFNGLXCREATEGLXPBUFFERSGIXPROC createPbufferSGIX = NULL;
static PFNGLXDESTROYPBUFFERPROC destroyPbuffer = NULL;
static PFNGLXQUERYDRAWABLEPROC queryPbuffer = NULL;
static Bool hasMultisampling = False;
static Bool hasPbuffer = False;

struct FBInfo
{
    int     acceleration;
    int     samples;
    int     depth;
    int     colors;
    GLXFBConfig fbcfg;
    XVisualInfo *visInfo;
};
typedef struct FBInfo FBInfo;

static int
FBInfoCmp(const void *a, const void *b)
{
    /* 
     * 1. full acceleration is better
     * 2. greater color bits is better
     * 3. greater depth bits is better
     * 4. more multisampling is better
     */
    const FBInfo *x = (const FBInfo *) a;
    const FBInfo *y = (const FBInfo *) b;

    if (x->acceleration != y->acceleration)
        return y->acceleration - x->acceleration;
    if (x->colors != y->colors)
        return y->colors - x->colors;
    if (x->depth != y->depth)
        return y->depth - x->depth;
    if (x->samples != y->samples)
        return y->samples - x->samples;
    return 0;
}

#ifdef DEBUG_GLX
static int
fatal_error(Display *dpy, XErrorEvent * event)
{
    char    buf[256];

    XGetErrorText(dpy, event->error_code, buf, sizeof buf);
    fprintf(stderr, "%s\n", buf);
    abort();
}
#endif

static XVisualInfo *
togl_pixelFormat(Togl *togl, int scrnum)
{
    int     attribs[256];
    int     na = 0;
    int     i;
    XVisualInfo *visinfo;
    FBInfo *info;
    static int loadedOpenGL = False;

    if (!loadedOpenGL) {
        int     dummy;
        int     major, minor;
        const char *extensions;

        /* Make sure OpenGL's GLX extension supported */
        if (!glXQueryExtension(togl->display, &dummy, &dummy)) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "X server is missing OpenGL GLX extension",
                    TCL_STATIC);
            return NULL;
        }

        loadedOpenGL = True;
#ifdef DEBUG_GLX
        (void) XSetErrorHandler(fatal_error);
#endif

        glXQueryVersion(togl->display, &major, &minor);
        extensions = glXQueryExtensionsString(togl->display, scrnum);

        if (major > 1 || (major == 1 && minor >= 3)) {
            chooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)
                    Togl_GetProcAddr("glXChooseFBConfig");
            getFBConfigAttrib = (PFNGLXGETFBCONFIGATTRIBPROC)
                    Togl_GetProcAddr("glXGetFBConfigAttrib");
            getVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)
                    Togl_GetProcAddr("glXGetVisualFromFBConfig");
            createPbuffer = (PFNGLXCREATEPBUFFERPROC)
                    Togl_GetProcAddr("glXCreatePbuffer");
            destroyPbuffer = (PFNGLXDESTROYPBUFFERPROC)
                    Togl_GetProcAddr("glXDestroyPbuffer");
            queryPbuffer = (PFNGLXQUERYDRAWABLEPROC)
                    Togl_GetProcAddr("glXQueryDrawable");
            if (createPbuffer && destroyPbuffer && queryPbuffer) {
                hasPbuffer = True;
            } else {
                createPbuffer = NULL;
                destroyPbuffer = NULL;
                queryPbuffer = NULL;
            }
        }
        if (major == 1 && minor == 2) {
            chooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)
                    Togl_GetProcAddr("glXChooseFBConfigSGIX");
            getFBConfigAttrib = (PFNGLXGETFBCONFIGATTRIBPROC)
                    Togl_GetProcAddr("glXGetFBConfigAttribSGIX");
            getVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)
                    Togl_GetProcAddr("glXGetVisualFromFBConfigSGIX");
            if (strstr(extensions, "GLX_SGIX_pbuffer") != NULL) {
                createPbufferSGIX = (PFNGLXCREATEGLXPBUFFERSGIXPROC)
                        Togl_GetProcAddr("glXCreateGLXPbufferSGIX");
                destroyPbuffer = (PFNGLXDESTROYPBUFFERPROC)
                        Togl_GetProcAddr("glXDestroyGLXPbufferSGIX");
                queryPbuffer = (PFNGLXQUERYDRAWABLEPROC)
                        Togl_GetProcAddr("glXQueryGLXPbufferSGIX");
                if (createPbufferSGIX && destroyPbuffer && queryPbuffer) {
                    hasPbuffer = True;
                } else {
                    createPbufferSGIX = NULL;
                    destroyPbuffer = NULL;
                    queryPbuffer = NULL;
                }
            }
        }
        if (chooseFBConfig) {
            /* verify that chooseFBConfig works (workaround Mesa 6.5 bug) */
            int     n = 0;
            GLXFBConfig *cfgs;

            attribs[n++] = GLX_RENDER_TYPE;
            attribs[n++] = GLX_RGBA_BIT;
            attribs[n++] = None;

            cfgs = chooseFBConfig(togl->display, scrnum, attribs, &n);
            if (cfgs == NULL || n == 0) {
                chooseFBConfig = NULL;
            }
            XFree(cfgs);
        }
        if (chooseFBConfig == NULL
                || getFBConfigAttrib == NULL || getVisualFromFBConfig == NULL) {
            chooseFBConfig = NULL;
            getFBConfigAttrib = NULL;
            getVisualFromFBConfig = NULL;
        }
        if (hasPbuffer && !chooseFBConfig) {
            hasPbuffer = False;
        }

        if ((major > 1 || (major == 1 && minor >= 4))
                || strstr(extensions, "GLX_ARB_multisample") != NULL
                || strstr(extensions, "GLX_SGIS_multisample") != NULL) {
            /* Client GLX supports multisampling, but does the server? Well, we 
             * can always ask. */
            hasMultisampling = True;
        }
    }

    if (togl->MultisampleFlag && !hasMultisampling) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "multisampling not supported", TCL_STATIC);
        return NULL;
    }

    if (togl->PbufferFlag && !hasPbuffer) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "pbuffers are not supported", TCL_STATIC);
        return NULL;
    }

    /* 
     * Only use the newer glXGetFBConfig if there's an explicit need for it
     * because it is buggy on many systems:
     *  (1) NVidia 96.43.07 on Linux, single-buffered windows don't work
     *  (2) Mesa 6.5.X and earlier fail
     */
    if (chooseFBConfig) {
        /* have new glXGetFBConfig! */
        int     count;
        GLXFBConfig *cfgs;

        attribs[na++] = GLX_RENDER_TYPE;
        if (togl->RgbaFlag) {
            /* RGB[A] mode */
            attribs[na++] = GLX_RGBA_BIT;
            attribs[na++] = GLX_RED_SIZE;
            attribs[na++] = togl->RgbaRed;
            attribs[na++] = GLX_GREEN_SIZE;
            attribs[na++] = togl->RgbaGreen;
            attribs[na++] = GLX_BLUE_SIZE;
            attribs[na++] = togl->RgbaBlue;
            if (togl->AlphaFlag) {
                attribs[na++] = GLX_ALPHA_SIZE;
                attribs[na++] = togl->AlphaSize;
            }
        } else {
            /* Color index mode */
            attribs[na++] = GLX_COLOR_INDEX_BIT;
            attribs[na++] = GLX_BUFFER_SIZE;
            attribs[na++] = 1;
        }
        if (togl->DepthFlag) {
            attribs[na++] = GLX_DEPTH_SIZE;
            attribs[na++] = togl->DepthSize;
        }
        if (togl->DoubleFlag) {
            attribs[na++] = GLX_DOUBLEBUFFER;
            attribs[na++] = True;
        }
        if (togl->StencilFlag) {
            attribs[na++] = GLX_STENCIL_SIZE;
            attribs[na++] = togl->StencilSize;
        }
        if (togl->AccumFlag) {
            attribs[na++] = GLX_ACCUM_RED_SIZE;
            attribs[na++] = togl->AccumRed;
            attribs[na++] = GLX_ACCUM_GREEN_SIZE;
            attribs[na++] = togl->AccumGreen;
            attribs[na++] = GLX_ACCUM_BLUE_SIZE;
            attribs[na++] = togl->AccumBlue;
            if (togl->AlphaFlag) {
                attribs[na++] = GLX_ACCUM_ALPHA_SIZE;
                attribs[na++] = togl->AccumAlpha;
            }
        }
        if (togl->Stereo == TOGL_STEREO_NATIVE) {
            attribs[na++] = GLX_STEREO;
            attribs[na++] = True;
        }
        if (togl->MultisampleFlag) {
            attribs[na++] = GLX_SAMPLE_BUFFERS_ARB;
            attribs[na++] = 1;
            attribs[na++] = GLX_SAMPLES_ARB;
            attribs[na++] = 2;
        }
        if (togl->PbufferFlag) {
            attribs[na++] = GLX_DRAWABLE_TYPE;
            attribs[na++] = GLX_WINDOW_BIT | GLX_PBUFFER_BIT;
        }
        if (togl->AuxNumber != 0) {
            attribs[na++] = GLX_AUX_BUFFERS;
            attribs[na++] = togl->AuxNumber;
        }
        attribs[na++] = None;

        cfgs = chooseFBConfig(togl->display, scrnum, attribs, &count);
        if (cfgs == NULL || count == 0) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "couldn't choose pixel format", TCL_STATIC);
            return NULL;
        }
        /* 
         * Pick best format
         */
        info = (FBInfo *) malloc(count * sizeof (FBInfo));
        for (i = 0; i != count; ++i) {
            info[i].visInfo = getVisualFromFBConfig(togl->display, cfgs[i]);
            info[i].fbcfg = cfgs[i];
            getFBConfigAttrib(togl->display, cfgs[i], GLX_CONFIG_CAVEAT,
                    &info[i].acceleration);
            getFBConfigAttrib(togl->display, cfgs[i], GLX_BUFFER_SIZE,
                    &info[i].colors);
            getFBConfigAttrib(togl->display, cfgs[i], GLX_DEPTH_SIZE,
                    &info[i].depth);
            getFBConfigAttrib(togl->display, cfgs[i], GLX_SAMPLES,
                    &info[i].samples);
            /* revise attributes so larger is better */
            info[i].acceleration = -(info[i].acceleration - GLX_NONE);
            if (!togl->DepthFlag)
                info[i].depth = -info[i].depth;
            if (!togl->MultisampleFlag)
                info[i].samples = -info[i].samples;
        }
        qsort(info, count, sizeof info[0], FBInfoCmp);

        togl->fbcfg = info[0].fbcfg;
        visinfo = info[0].visInfo;
        for (i = 1; i != count; ++i)
            XFree(info[i].visInfo);
        free(info);
        XFree(cfgs);
        return visinfo;
    }

    /* use original glXChooseVisual */

    attribs[na++] = GLX_USE_GL;
    if (togl->RgbaFlag) {
        /* RGB[A] mode */
        attribs[na++] = GLX_RGBA;
        attribs[na++] = GLX_RED_SIZE;
        attribs[na++] = togl->RgbaRed;
        attribs[na++] = GLX_GREEN_SIZE;
        attribs[na++] = togl->RgbaGreen;
        attribs[na++] = GLX_BLUE_SIZE;
        attribs[na++] = togl->RgbaBlue;
        if (togl->AlphaFlag) {
            attribs[na++] = GLX_ALPHA_SIZE;
            attribs[na++] = togl->AlphaSize;
        }
    } else {
        /* Color index mode */
        attribs[na++] = GLX_BUFFER_SIZE;
        attribs[na++] = 1;
    }
    if (togl->DepthFlag) {
        attribs[na++] = GLX_DEPTH_SIZE;
        attribs[na++] = togl->DepthSize;
    }
    if (togl->DoubleFlag) {
        attribs[na++] = GLX_DOUBLEBUFFER;
    }
    if (togl->StencilFlag) {
        attribs[na++] = GLX_STENCIL_SIZE;
        attribs[na++] = togl->StencilSize;
    }
    if (togl->AccumFlag) {
        attribs[na++] = GLX_ACCUM_RED_SIZE;
        attribs[na++] = togl->AccumRed;
        attribs[na++] = GLX_ACCUM_GREEN_SIZE;
        attribs[na++] = togl->AccumGreen;
        attribs[na++] = GLX_ACCUM_BLUE_SIZE;
        attribs[na++] = togl->AccumBlue;
        if (togl->AlphaFlag) {
            attribs[na++] = GLX_ACCUM_ALPHA_SIZE;
            attribs[na++] = togl->AccumAlpha;
        }
    }
    if (togl->Stereo == TOGL_STEREO_NATIVE) {
        attribs[na++] = GLX_STEREO;
    }
    if (togl->AuxNumber != 0) {
        attribs[na++] = GLX_AUX_BUFFERS;
        attribs[na++] = togl->AuxNumber;
    }
    attribs[na++] = None;

    visinfo = glXChooseVisual(togl->display, scrnum, attribs);
    if (visinfo == NULL) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "couldn't choose pixel format", TCL_STATIC);
        return NULL;
    }
    return visinfo;
}

static int
togl_describePixelFormat(Togl *togl)
{
    int     tmp = 0;

    /* fill in flags normally passed in that affect behavior */
    (void) glXGetConfig(togl->display, togl->VisInfo, GLX_RGBA,
            &togl->RgbaFlag);
    (void) glXGetConfig(togl->display, togl->VisInfo, GLX_DOUBLEBUFFER,
            &togl->DoubleFlag);
    (void) glXGetConfig(togl->display, togl->VisInfo, GLX_DEPTH_SIZE, &tmp);
    togl->DepthFlag = (tmp != 0);
    (void) glXGetConfig(togl->display, togl->VisInfo, GLX_ACCUM_RED_SIZE, &tmp);
    togl->AccumFlag = (tmp != 0);
    (void) glXGetConfig(togl->display, togl->VisInfo, GLX_ALPHA_SIZE, &tmp);
    togl->AlphaFlag = (tmp != 0);
    (void) glXGetConfig(togl->display, togl->VisInfo, GLX_STENCIL_SIZE, &tmp);
    togl->StencilFlag = (tmp != 0);
    (void) glXGetConfig(togl->display, togl->VisInfo, GLX_STEREO, &tmp);
    togl->Stereo = tmp ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE;
    if (hasMultisampling) {
        (void) glXGetConfig(togl->display, togl->VisInfo, GLX_SAMPLES, &tmp);
        togl->MultisampleFlag = (tmp != 0);
    }
    return True;
}

static Tcl_ThreadDataKey togl_XError;
struct ErrorData
{
    int     error_code;
    XErrorHandler prevHandler;
};
typedef struct ErrorData ErrorData;

static int
togl_HandleXError(Display *dpy, XErrorEvent * event)
{
    ErrorData *data = Tcl_GetThreadData(&togl_XError, (int) sizeof (ErrorData));

    data->error_code = event->error_code;
    return 0;
}

static void
togl_SetupXErrorHandler()
{
    ErrorData *data = Tcl_GetThreadData(&togl_XError, (int) sizeof (ErrorData));

    data->error_code = Success; /* 0 */
    data->prevHandler = XSetErrorHandler(togl_HandleXError);
}

static int
togl_CheckForXError(const Togl *togl)
{
    ErrorData *data = Tcl_GetThreadData(&togl_XError, (int) sizeof (ErrorData));

    XSync(togl->display, False);
    (void) XSetErrorHandler(data->prevHandler);
    return data->error_code;
}

static GLXPbuffer
togl_createPbuffer(Togl *togl)
{
    int     attribs[32];
    int     na = 0;
    GLXPbuffer pbuf;

    togl_SetupXErrorHandler();
    if (togl->LargestPbufferFlag) {
        attribs[na++] = GLX_LARGEST_PBUFFER;
        attribs[na++] = True;
    }
    attribs[na++] = GLX_PRESERVED_CONTENTS;
    attribs[na++] = True;
    if (createPbuffer) {
        attribs[na++] = GLX_PBUFFER_WIDTH;
        attribs[na++] = togl->Width;
        attribs[na++] = GLX_PBUFFER_HEIGHT;
        attribs[na++] = togl->Width;
        attribs[na++] = None;
        pbuf = createPbuffer(togl->display, togl->fbcfg, attribs);
    } else {
        attribs[na++] = None;
        pbuf = createPbufferSGIX(togl->display, togl->fbcfg, togl->Width,
                togl->Height, attribs);
    }
    if (togl_CheckForXError(togl) || pbuf == None) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "unable to allocate pbuffer", TCL_STATIC);
        return None;
    }
    if (pbuf && togl->LargestPbufferFlag) {
        int     tmp;

        queryPbuffer(togl->display, pbuf, GLX_WIDTH, &tmp);
        if (tmp != 0)
            togl->Width = tmp;
        queryPbuffer(togl->display, pbuf, GLX_HEIGHT, &tmp);
        if (tmp != 0)
            togl->Height = tmp;
    }
    return pbuf;
}

static void
togl_destroyPbuffer(Togl *togl)
{
    destroyPbuffer(togl->display, togl->pbuf);
}
