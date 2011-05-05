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

#include "brlcad_config.h"

#include <windows.h>
#include <GL/gl.h>
#include <wingdi.h>
#include <tk.h>
#include <tkPlatDecls.h>
#ifdef HAVE_GL_WGLEXT_H
# include <GL/wglext.h>
#else
# include "./wglext.h"
#endif

#ifndef PFD_SUPPORT_COMPOSITION
/* for Vista -- not needed because we don't use PFD_SUPPORT_GDI/BITMAP */
#  define PFD_SUPPORT_COMPOSITION 0x00008000
#endif

/* TODO: move these statics into global structure */
static PFNWGLGETEXTENSIONSSTRINGARBPROC getExtensionsString = NULL;
static PFNWGLCHOOSEPIXELFORMATARBPROC choosePixelFormat;
static PFNWGLGETPIXELFORMATATTRIBIVARBPROC getPixelFormatAttribiv;
static PFNWGLCREATEPBUFFERARBPROC createPbuffer = NULL;
static PFNWGLDESTROYPBUFFERARBPROC destroyPbuffer = NULL;
static PFNWGLGETPBUFFERDCARBPROC getPbufferDC = NULL;
static PFNWGLRELEASEPBUFFERDCARBPROC releasePbufferDC = NULL;
static PFNWGLQUERYPBUFFERARBPROC queryPbuffer = NULL;
static int hasMultisampling = FALSE;
static int hasPbuffer = FALSE;
static int hasARBPbuffer = FALSE;

static HWND
toglCreateTestWindow(HWND parent)
{
    static char ClassName[] = "ToglTestWindow";
    WNDCLASS wc;
    HINSTANCE instance = GetModuleHandle(NULL);
    HWND    wnd;
    HDC     dc;
    PIXELFORMATDESCRIPTOR pfd;
    int     pixelFormat;

    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = ClassName;
    if (!RegisterClass(&wc)) {
        DWORD   err = GetLastError();

        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            fprintf(stderr, "Unable to register Togl Test Window class\n");
            return NULL;
        }
    }

    wnd = CreateWindow(ClassName, "test OpenGL capabilities",
            WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 1, 1, parent, NULL, instance, NULL);
    if (wnd == NULL) {
        fprintf(stderr, "Unable to create temporary OpenGL window\n");
        return NULL;
    }
    dc = GetDC(wnd);

    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 3;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pixelFormat = ChoosePixelFormat(dc, &pfd);
    if (pixelFormat == 0) {
        fprintf(stderr, "Unable to choose simple pixel format\n");
        ReleaseDC(wnd, dc);
        return NULL;
    }
    if (!SetPixelFormat(dc, pixelFormat, &pfd)) {
        fprintf(stderr, "Unable to set simple pixel format\n");
        ReleaseDC(wnd, dc);
        return NULL;
    }

    ShowWindow(wnd, SW_HIDE);   // make sure it's hidden
    ReleaseDC(wnd, dc);
    return wnd;
}

struct FBInfo
{
    int     stereo;
    int     acceleration;
    int     colors;
    int     depth;
    int     samples;
    int     pixelFormat;
};
typedef struct FBInfo FBInfo;
static int FBAttribs[] = {
    /* must match order in FBInfo structure */
    WGL_STEREO_ARB,
    WGL_ACCELERATION_ARB,
    WGL_COLOR_BITS_ARB,
    WGL_DEPTH_BITS_ARB,
    WGL_SAMPLES_ARB,
};

#define NUM_FBAttribs (sizeof FBAttribs / sizeof FBAttribs[0])

static int
FBInfoCmp(const void *a, const void *b)
{
    /* 
     * 1. stereo is better
     * 2. full acceleration is better
     * 3. greater color bits is better
     * 4. greater depth bits is better
     * 5. more multisampling is better
     */
    const FBInfo *x = (const FBInfo *) a;
    const FBInfo *y = (const FBInfo *) b;

    if (x->stereo != y->stereo)
        return y->stereo - x->stereo;
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

static int
togl_pixelFormat(Togl *togl, HWND hwnd)
{
    /* return 0 when pixel format is unavailable. */
    int     pixelformat = 0;
    static int loadedOpenGL = FALSE;
    int     formats[256];
    UINT    numFormats;
    FBInfo *info;
    UINT    i;
    int     attribs[128];
    int     na = 0;

    if (!loadedOpenGL) {
        HWND    test = NULL;
        HDC     dc;
        HGLRC   rc;

        if (wglGetCurrentContext() != NULL) {
            dc = wglGetCurrentDC();
        } else {
            /* HWND hwnd = Tk_GetHWND(Tk_WindowId(togl->TkWin)); */

            test = toglCreateTestWindow(hwnd);
            if (test == NULL) {
                Tcl_SetResult(togl->Interp,
                        TCL_STUPID "can't create dummy OpenGL window",
                        TCL_STATIC);
                return 0;
            }

            dc = GetDC(test);
            rc = wglCreateContext(dc);
            wglMakeCurrent(dc, rc);
        }
        loadedOpenGL = TRUE;

        /* 
         * Now that we have an OpenGL window, we can initialize all
         * OpenGL information and figure out if multisampling is supported.
         */
        getExtensionsString = (PFNWGLGETEXTENSIONSSTRINGARBPROC)
                wglGetProcAddress("wglGetExtensionsStringARB");
        if (getExtensionsString == NULL)
            getExtensionsString = (PFNWGLGETEXTENSIONSSTRINGARBPROC)
                    wglGetProcAddress("wglGetExtensionsStringEXT");
        if (getExtensionsString) {
            const char *extensions = getExtensionsString(dc);

            if (strstr(extensions, "WGL_ARB_multisample") != NULL
                    || strstr(extensions, "WGL_EXT_multisample") != NULL)
                hasMultisampling = TRUE;

            if (strstr(extensions, "WGL_ARB_pixel_format") != NULL) {
                choosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)
                        wglGetProcAddress("wglChoosePixelFormatARB");
                getPixelFormatAttribiv = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)
                        wglGetProcAddress("wglGetPixelFormatAttribivARB");
                if (choosePixelFormat == NULL || getPixelFormatAttribiv == NULL) {
                    choosePixelFormat = NULL;
                    getPixelFormatAttribiv = NULL;
                }
            }
            if (choosePixelFormat == NULL
                    && strstr(extensions, "WGL_EXT_pixel_format") != NULL) {
                choosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)
                        wglGetProcAddress("wglChoosePixelFormatEXT");
                getPixelFormatAttribiv = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)
                        wglGetProcAddress("wglGetPixelFormatAttribivEXT");
                if (choosePixelFormat == NULL || getPixelFormatAttribiv == NULL) {
                    choosePixelFormat = NULL;
                    getPixelFormatAttribiv = NULL;
                }
            }
            if (createPbuffer == NULL
                    && strstr(extensions, "WGL_ARB_pbuffer") != NULL) {
                createPbuffer = (PFNWGLCREATEPBUFFERARBPROC)
                        wglGetProcAddress("wglCreatePbufferARB");
                destroyPbuffer = (PFNWGLDESTROYPBUFFERARBPROC)
                        wglGetProcAddress("wglDestroyPbufferARB");
                getPbufferDC = (PFNWGLGETPBUFFERDCARBPROC)
                        wglGetProcAddress("wglGetPbufferDCARB");
                releasePbufferDC = (PFNWGLRELEASEPBUFFERDCARBPROC)
                        wglGetProcAddress("wglReleasePbufferDCARB");
                queryPbuffer = (PFNWGLQUERYPBUFFERARBPROC)
                        wglGetProcAddress("wglQueryPbufferARB");
                if (createPbuffer == NULL || destroyPbuffer == NULL
                        || getPbufferDC == NULL || releasePbufferDC == NULL
                        || queryPbuffer == NULL) {
                    createPbuffer = NULL;
                    destroyPbuffer = NULL;
                    getPbufferDC = NULL;
                    releasePbufferDC = NULL;
                    queryPbuffer = NULL;
                } else {
                    hasPbuffer = TRUE;
                    hasARBPbuffer = TRUE;
                }
            }
            if (createPbuffer == NULL
                    && strstr(extensions, "WGL_EXT_pbuffer") != NULL) {
                createPbuffer = (PFNWGLCREATEPBUFFERARBPROC)
                        wglGetProcAddress("wglCreatePbufferEXT");
                destroyPbuffer = (PFNWGLDESTROYPBUFFERARBPROC)
                        wglGetProcAddress("wglDestroyPbufferEXT");
                getPbufferDC = (PFNWGLGETPBUFFERDCARBPROC)
                        wglGetProcAddress("wglGetPbufferDCEXT");
                releasePbufferDC = (PFNWGLRELEASEPBUFFERDCARBPROC)
                        wglGetProcAddress("wglReleasePbufferDCEXT");
                queryPbuffer = (PFNWGLQUERYPBUFFERARBPROC)
                        wglGetProcAddress("wglQueryPbufferEXT");
                if (createPbuffer == NULL || destroyPbuffer == NULL
                        || getPbufferDC == NULL || releasePbufferDC == NULL
                        || queryPbuffer == NULL) {
                    createPbuffer = NULL;
                    destroyPbuffer = NULL;
                    getPbufferDC = NULL;
                    releasePbufferDC = NULL;
                    queryPbuffer = NULL;
                } else {
                    hasPbuffer = TRUE;
                }
            }
        }

        /* No need to confirm multisampling is in glGetString(GL_EXTENSIONS)
         * because OpenGL driver is local */

        if (test != NULL) {
            /* cleanup by removing temporary OpenGL window */
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(rc);
            ReleaseDC(test, dc);
            DestroyWindow(test);
        }
    }

    if (togl->MultisampleFlag && !hasMultisampling) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "multisampling not supported", TCL_STATIC);
        return 0;
    }

    if (togl->PbufferFlag && !hasPbuffer) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "pbuffers are not supported", TCL_STATIC);
        return 0;
    }

    if (choosePixelFormat == NULL) {
        PIXELFORMATDESCRIPTOR pfd;

        /* Don't have the great wglChoosePixelFormatARB() function, so do it
         * the old way. */
        if (togl->MultisampleFlag) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "multisampling not supported", TCL_STATIC);
            return 0;
        }

        memset(&pfd, 0, sizeof pfd);
        pfd.nSize = sizeof pfd;
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL
                | PFD_SUPPORT_COMPOSITION;
        if (togl->DoubleFlag) {
            pfd.dwFlags |= PFD_DOUBLEBUFFER;
        }
        if (togl->Stereo == TOGL_STEREO_NATIVE) {
            pfd.dwFlags |= PFD_STEREO;
        }
        pfd.iPixelType = togl->RgbaFlag ? PFD_TYPE_RGBA : PFD_TYPE_COLORINDEX;
        pfd.cColorBits = togl->RgbaRed + togl->RgbaGreen + togl->RgbaBlue;
        /* Alpha bitplanes are not supported in the current generic OpenGL
         * implementation, but may be supported by specific hardware devices. */
        pfd.cAlphaBits = togl->AlphaFlag ? togl->AlphaSize : 0;
        pfd.cAccumBits = togl->AccumFlag ? (togl->AccumRed + togl->AccumGreen +
                togl->AccumBlue + togl->AccumAlpha) : 0;
        pfd.cDepthBits = togl->DepthFlag ? togl->DepthSize : 0;
        pfd.cStencilBits = togl->StencilFlag ? togl->StencilSize : 0;
        /* Auxiliary buffers are not supported in the current generic OpenGL
         * implementation, but may be supported by specific hardware devices. */
        pfd.cAuxBuffers = togl->AuxNumber;
        pfd.iLayerType = PFD_MAIN_PLANE;

        if ((pixelformat = ChoosePixelFormat(togl->tglGLHdc, &pfd)) == 0) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "couldn't choose pixel format", TCL_STATIC);
            return 0;
        }

        /* double check that we got the stereo format we requested */
        if (togl->Stereo == TOGL_STEREO_NATIVE) {
            DescribePixelFormat(togl->tglGLHdc, pixelformat, sizeof (pfd),
                    &pfd);
            if ((pfd.dwFlags & PFD_STEREO) == 0) {
                Tcl_SetResult(togl->Interp,
                        TCL_STUPID "couldn't choose stereo pixel format",
                        TCL_STATIC);
                return 0;
            }
        }
        return pixelformat;
    }
    // We have the new wglChoosePixelFormat!!
    if (togl->MultisampleFlag && !hasMultisampling) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "multisampling not supported", TCL_STATIC);
        return 0;
    }

    if (togl->PbufferFlag)
        attribs[na++] = WGL_DRAW_TO_PBUFFER_ARB;
    else
        attribs[na++] = WGL_DRAW_TO_WINDOW_ARB;
    attribs[na++] = GL_TRUE;
    attribs[na++] = WGL_SUPPORT_OPENGL_ARB;
    attribs[na++] = GL_TRUE;
    attribs[na++] = WGL_PIXEL_TYPE_ARB;
    if (!togl->RgbaFlag) {
        attribs[na++] = WGL_TYPE_COLORINDEX_ARB;
    } else {
        attribs[na++] = WGL_TYPE_RGBA_ARB;
        attribs[na++] = WGL_RED_BITS_ARB;
        attribs[na++] = togl->RgbaRed;
        attribs[na++] = WGL_GREEN_BITS_ARB;
        attribs[na++] = togl->RgbaGreen;
        attribs[na++] = WGL_BLUE_BITS_ARB;
        attribs[na++] = togl->RgbaBlue;
        if (togl->AlphaFlag) {
            attribs[na++] = WGL_ALPHA_BITS_ARB;
            attribs[na++] = togl->AlphaSize;
        }
    }
    if (togl->DepthFlag) {
        attribs[na++] = WGL_DEPTH_BITS_ARB;
        attribs[na++] = togl->DepthSize;
    }
    if (togl->DoubleFlag) {
        attribs[na++] = WGL_DOUBLE_BUFFER_ARB;
        attribs[na++] = GL_TRUE;
    }
    if (togl->StencilFlag) {
        attribs[na++] = WGL_STENCIL_BITS_ARB;
        attribs[na++] = togl->StencilSize;
    }
    if (togl->AccumFlag) {
        attribs[na++] = WGL_ACCUM_RED_BITS_ARB;
        attribs[na++] = togl->AccumRed;
        attribs[na++] = WGL_ACCUM_GREEN_BITS_ARB;
        attribs[na++] = togl->AccumGreen;
        attribs[na++] = WGL_ACCUM_BLUE_BITS_ARB;
        attribs[na++] = togl->AccumBlue;
        if (togl->AlphaFlag) {
            attribs[na++] = WGL_ACCUM_ALPHA_BITS_ARB;
            attribs[na++] = togl->AccumAlpha;
        }
    }
    if (togl->Stereo == TOGL_STEREO_NATIVE) {
        attribs[na++] = WGL_STEREO_ARB;
        attribs[na++] = GL_TRUE;
    }
    if (togl->MultisampleFlag) {
        attribs[na++] = WGL_SAMPLE_BUFFERS_ARB;
        attribs[na++] = 1;
        attribs[na++] = WGL_SAMPLES_ARB;
        attribs[na++] = 2;
    }
    if (togl->AuxNumber) {
        attribs[na++] = WGL_AUX_BUFFERS_ARB;
        attribs[na++] = togl->AuxNumber;
    }
    attribs[na++] = 0;          // must be last

    if (!choosePixelFormat(togl->tglGLHdc, &attribs[0], NULL, 256, formats,
                    &numFormats) || numFormats == 0) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "couldn't choose pixel format", TCL_STATIC);
        return 0;
    }

    /* 
     * Pick best format
     */
    info = (FBInfo *) malloc(numFormats * sizeof (FBInfo));
    for (i = 0; i != numFormats; ++i) {
        info[i].pixelFormat = formats[i];
        getPixelFormatAttribiv(togl->tglGLHdc, formats[i], 0,
                NUM_FBAttribs, FBAttribs, &info[i].stereo);
        /* revise attributes so larger is better */
        if (!togl->DepthFlag)
            info[i].depth = -info[i].depth;
        if (!togl->MultisampleFlag)
            info[i].samples = -info[i].samples;
        if (togl->Stereo != TOGL_STEREO_NATIVE)
            info[i].stereo = -info[i].stereo;
    }
    qsort(info, numFormats, sizeof info[0], FBInfoCmp);
    pixelformat = info[0].pixelFormat;
    /* double check that we got the stereo format we requested */
    if (togl->Stereo == TOGL_STEREO_NATIVE && !info[0].stereo) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "couldn't choose stereo pixel format", TCL_STATIC);
        free(info);
        return 0;
    }
    free(info);
    return pixelformat;
}

static int
togl_describePixelFormat(Togl *togl)
{
    if (getPixelFormatAttribiv == NULL) {
        PIXELFORMATDESCRIPTOR pfd;

        DescribePixelFormat(togl->tglGLHdc, (int) togl->PixelFormat,
                sizeof (pfd), &pfd);
        /* fill in flags normally passed in that affect behavior */
        togl->RgbaFlag = pfd.iPixelType == PFD_TYPE_RGBA;
        togl->DoubleFlag = (pfd.dwFlags & PFD_DOUBLEBUFFER) != 0;
        togl->DepthFlag = (pfd.cDepthBits != 0);
        togl->AccumFlag = (pfd.cAccumBits != 0);
        togl->AlphaFlag = (pfd.cAlphaBits != 0);
        togl->StencilFlag = (pfd.cStencilBits != 0);
        if ((pfd.dwFlags & PFD_STEREO) != 0)
            togl->Stereo = TOGL_STEREO_NATIVE;
        else
            togl->Stereo = TOGL_STEREO_NONE;
    } else {
        static int attribs[] = {
            WGL_PIXEL_TYPE_ARB,
            WGL_DOUBLE_BUFFER_ARB,
            WGL_DEPTH_BITS_ARB,
            WGL_ACCUM_RED_BITS_ARB,
            WGL_ALPHA_BITS_ARB,
            WGL_STENCIL_BITS_ARB,
            WGL_STEREO_ARB,
            WGL_SAMPLES_ARB
        };
#define NUM_ATTRIBS (sizeof attribs / sizeof attribs[0])
        int     info[NUM_ATTRIBS];

        getPixelFormatAttribiv(togl->tglGLHdc, (int) togl->PixelFormat, 0,
                NUM_ATTRIBS, attribs, info);
#undef NUM_ATTRIBS
        togl->RgbaFlag = info[0];
        togl->DoubleFlag = info[1];
        togl->DepthFlag = (info[2] != 0);
        togl->AccumFlag = (info[3] != 0);
        togl->AlphaFlag = (info[4] != 0);
        togl->StencilFlag = (info[5] != 0);
        togl->Stereo = info[6] ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE;
        togl->MultisampleFlag = (info[7] != 0);
    }
    return True;
}

static HPBUFFERARB
togl_createPbuffer(Togl *togl)
{
    int     attribs[32];
    int     na = 0;
    HPBUFFERARB pbuf;

    if (togl->LargestPbufferFlag) {
        attribs[na++] = WGL_PBUFFER_LARGEST_ARB;
        attribs[na++] = 1;
    }
    attribs[na] = 0;
    pbuf = createPbuffer(togl->tglGLHdc, (int) togl->PixelFormat, togl->Width,
            togl->Height, attribs);
    if (pbuf && togl->LargestPbufferFlag) {
        queryPbuffer(pbuf, WGL_PBUFFER_WIDTH_ARB, &togl->Width);
        queryPbuffer(pbuf, WGL_PBUFFER_HEIGHT_ARB, &togl->Height);
    }
    return pbuf;
}

static void
togl_destroyPbuffer(Togl *togl)
{
    destroyPbuffer(togl->pbuf);
}

#if 0
// From nvidia.com

Multisampling requires WGL_ARB_extension_string and WGL_ARB_pixel_format
wglGetProcAddress("wglGetExtensionsStringARB")
// From msdn.microsoft.com, various ways to enumerate PixelFormats
        void    CPixForm::OnClickedLastPfd()
{
    COpenGL gl;
    PIXELFORMATDESCRIPTOR pfd;

    // 
    // Get the hwnd of the view window.
    // 
    HWND    hwndview = GetViewHwnd();

    // 
    // Get a DC associated with the view window.
    // 
    HDC     dc =::GetDC(hwndview);
    int     nID = (m_nNextID > 1) ? m_nNextID-- : 1;

    // 
    // Get a description of the pixel format. If it is valid, then go and 
    // update the controls in the dialog box, otherwise do nothing.
    // 
    if (gl.DescribePixelFormat(dc, nID, sizeof (PIXELFORMATDESCRIPTOR), &pfd))
        UpdateDlg(&pfd);
    // 
    // Release the DC.
    // 
    ::ReleaseDC(hwndview, dc);
}

----------------------
        // local variables 
int     iMax;
PIXELFORMATDESCRIPTOR pfd;
int     iPixelFormat;

// initialize a pixel format index variable 
iPixelFormat = 1;

// keep obtaining and examining pixel format data... 
do {
    // try to obtain some pixel format data 
    iMax = DescribePixelFormat(dc, iPixelFormat, sizeof (pfd), &pfd);

    // if there was some problem with that...  
    if (iMax == 0)
        // return indicating failure 
        return (FALSE);

    // we have successfully obtained pixel format data 

    // let's examine the pixel format data... 
    myPixelFormatExaminer(&pfd);
}
// ...until we've looked at all the device context's pixel formats 
while (++iPixelFormat <= iMax);
#endif
