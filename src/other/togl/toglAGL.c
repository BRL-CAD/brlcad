/* $Id$ */

/* vi:set sw=4 expandtab: */

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

struct FBInfo
{
    GLint   acceleration;
    GLint   colors;
    GLint   depth;
    GLint   samples;
    AGLPixelFormat pix;
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

static AGLPixelFormat
togl_pixelFormat(Togl *togl)
{
    GLint   attribs[32];
    int     na = 0;
    AGLPixelFormat pix;
    GDHandle display = NULL;
    FBInfo *info = NULL;
    int     count;

#if 0
    if (togl->MultisampleFlag && !hasMultisampling) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "multisampling not supported", TCL_STATIC);
        return NULL;
    }
#endif

    if (togl->PbufferFlag && !togl->RgbaFlag) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "puffer must be RGB[A]", TCL_STATIC);
        return NULL;
    }

    attribs[na++] = AGL_MINIMUM_POLICY;
    /* ask for hardware-accelerated onscreen */
    attribs[na++] = AGL_ACCELERATED;
    attribs[na++] = AGL_NO_RECOVERY;
    if (togl->RgbaFlag) {
        /* RGB[A] mode */
        attribs[na++] = AGL_RGBA;
        attribs[na++] = AGL_RED_SIZE;
        attribs[na++] = togl->RgbaRed;
        attribs[na++] = AGL_GREEN_SIZE;
        attribs[na++] = togl->RgbaGreen;
        attribs[na++] = AGL_BLUE_SIZE;
        attribs[na++] = togl->RgbaBlue;
        if (togl->AlphaFlag) {
            attribs[na++] = AGL_ALPHA_SIZE;
            attribs[na++] = togl->AlphaSize;
        }
    } else {
        /* Color index mode */
        attribs[na++] = AGL_BUFFER_SIZE;
        attribs[na++] = 8;
    }
    if (togl->DepthFlag) {
        attribs[na++] = AGL_DEPTH_SIZE;
        attribs[na++] = togl->DepthSize;
    }
    if (togl->DoubleFlag) {
        attribs[na++] = AGL_DOUBLEBUFFER;
    }
    if (togl->StencilFlag) {
        attribs[na++] = AGL_STENCIL_SIZE;
        attribs[na++] = togl->StencilSize;
    }
    if (togl->AccumFlag) {
        attribs[na++] = AGL_ACCUM_RED_SIZE;
        attribs[na++] = togl->AccumRed;
        attribs[na++] = AGL_ACCUM_GREEN_SIZE;
        attribs[na++] = togl->AccumGreen;
        attribs[na++] = AGL_ACCUM_BLUE_SIZE;
        attribs[na++] = togl->AccumBlue;
        if (togl->AlphaFlag) {
            attribs[na++] = AGL_ACCUM_ALPHA_SIZE;
            attribs[na++] = togl->AccumAlpha;
        }
    }
    if (togl->MultisampleFlag) {
        attribs[na++] = AGL_MULTISAMPLE;
#ifdef AGL_SAMPLES_ARB
        /* OS X 10.2 and later */
        attribs[na++] = AGL_SAMPLE_BUFFERS_ARB;
        attribs[na++] = 1;
        attribs[na++] = AGL_SAMPLES_ARB;
        attribs[na++] = 2;
#endif
    }
    if (togl->AuxNumber != 0) {
        attribs[na++] = AGL_AUX_BUFFERS;
        attribs[na++] = togl->AuxNumber;
    }
    if (togl->Stereo == TOGL_STEREO_NATIVE) {
        attribs[na++] = AGL_STEREO;
    }
    if (togl->FullscreenFlag) {
        attribs[na++] = AGL_FULLSCREEN;
        /* TODO: convert Tk screen to display device */
        display = GetMainDevice();
    }
    attribs[na++] = AGL_NONE;

    if ((pix = aglChoosePixelFormat(&display, togl->FullscreenFlag ? 1 : 0,
                            attribs)) == NULL) {
        Tcl_SetResult(togl->Interp, TCL_STUPID "couldn't choose pixel format",
                TCL_STATIC);
        return NULL;
    }

    /* TODO: since we aglDestroyPixelFormat elsewhere, this code may leak
     * memory if the pixel format choosen is not the original (because
     * aglDestroyPixelFormat will give an error). */
    count = 0;
    do {
        info = (FBInfo *) realloc(info, (count + 1) * sizeof (FBInfo));
        info[count].pix = pix;
        aglDescribePixelFormat(pix, AGL_ACCELERATED, &info[count].acceleration);
        aglDescribePixelFormat(pix, AGL_BUFFER_SIZE, &info[count].colors);
        aglDescribePixelFormat(pix, AGL_DEPTH_SIZE, &info[count].depth);
#ifdef AGL_SAMPLES_ARB
        aglDescribePixelFormat(pix, AGL_SAMPLES_ARB, &info[count].samples);
#else
        info[count].samples = 0;
#endif
        ++count;
    } while (pix = aglNextPixelFormat(pix));
    qsort(info, count, sizeof info[0], FBInfoCmp);
    pix = info[0].pix;
    free(info);
    return pix;
}

static int
togl_describePixelFormat(Togl *togl)
{
    AGLPixelFormat pixelformat;

    /* fill in RgbaFlag, DoubleFlag, and Stereo */
    pixelformat = (AGLPixelFormat) togl->PixelFormat;
    GLint   has_rgba, has_doublebuf, has_depth, has_accum, has_alpha,
            has_stencil, has_stereo, has_multisample;

    if (aglDescribePixelFormat(pixelformat, AGL_RGBA, &has_rgba)
            && aglDescribePixelFormat(pixelformat, AGL_DOUBLEBUFFER,
                    &has_doublebuf)
            && aglDescribePixelFormat(pixelformat, AGL_DEPTH_SIZE, &has_depth)
            && aglDescribePixelFormat(pixelformat, AGL_ACCUM_RED_SIZE,
                    &has_accum)
            && aglDescribePixelFormat(pixelformat, AGL_ALPHA_SIZE, &has_alpha)
            && aglDescribePixelFormat(pixelformat, AGL_STENCIL_SIZE,
                    &has_stencil)
            && aglDescribePixelFormat(pixelformat, AGL_STEREO, &has_stereo)
#ifdef AGL_SAMPLES_ARB
            && aglDescribePixelFormat(pixelformat, AGL_SAMPLES_ARB,
                    &has_multisample)
#endif
            ) {
        togl->RgbaFlag = (has_rgba != 0);
        togl->DoubleFlag = (has_doublebuf != 0);
        togl->DepthFlag = (has_depth != 0);
        togl->AccumFlag = (has_accum != 0);
        togl->AlphaFlag = (has_alpha != 0);
        togl->StencilFlag = (has_stencil != 0);
        togl->Stereo = (has_stereo ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE);
#ifdef AGL_SAMPLES_ARB
        togl->MultisampleFlag = (has_multisample != 0);
#else
        togl->MultisampleFlag = False;
#endif
        return True;
    } else {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "failed querying pixel format attributes",
                TCL_STATIC);
        return False;
    }
}

#define isPow2(x) (((x) & ((x) - 1)) == 0)

static AGLPbuffer
togl_createPbuffer(Togl *togl)
{
    GLint   min_size[2], max_size[2];
    Bool    hasPbuffer;
    const char *extensions;
    GLboolean good;
    GLint   target;
    GLint   virtualScreen;
    AGLPbuffer pbuf;

    extensions = (const char *) glGetString(GL_EXTENSIONS);
    hasPbuffer = (strstr(extensions, "GL_APPLE_pixel_buffer") != NULL);
    if (!hasPbuffer) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "pbuffers are not supported", TCL_STATIC);
        return NULL;
    }
    glGetIntegerv(GL_MIN_PBUFFER_VIEWPORT_DIMS_APPLE, min_size);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_size);
    virtualScreen = aglGetVirtualScreen(togl->Ctx);
    for (;;) {
        /* make sure we don't exceed the maximum size because if we do,
         * aglCreatePbuffer may succeed and later uses of the pbuffer fail */
        if (togl->Width < min_size[0])
            togl->Width = min_size[0];
        else if (togl->Width > max_size[0]) {
            if (togl->LargestPbufferFlag)
                togl->Width = max_size[0];
            else {
                Tcl_SetResult(togl->Interp,
                        TCL_STUPID "pbuffer too large", TCL_STATIC);
                return NULL;
            }
        }
        if (togl->Height < min_size[1])
            togl->Height = min_size[1];
        else if (togl->Height > max_size[1]) {
            if (togl->LargestPbufferFlag)
                togl->Height = max_size[1];
            else {
                Tcl_SetResult(togl->Interp,
                        TCL_STUPID "pbuffer too large", TCL_STATIC);
                return NULL;
            }
        }

        if (isPow2(togl->Width) && isPow2(togl->Height))
            target = GL_TEXTURE_2D;
        else
            target = GL_TEXTURE_RECTANGLE_ARB;

        good = aglCreatePBuffer(togl->Width, togl->Height, target,
                togl->AlphaFlag ? GL_RGBA : GL_RGB, 0, &pbuf);
        if (good) {
            /* aglSetPbuffer allocates the framebuffer space */
            if (aglSetPBuffer(togl->Ctx, pbuf, 0, 0, virtualScreen)) {
                return pbuf;
            }
        }
        if (!togl->LargestPbufferFlag
                || togl->Width == min_size[0] || togl->Height == min_size[1]) {
            Tcl_SetResult(togl->Interp,
                    TCL_STUPID "unable to create pbuffer", TCL_STATIC);
            return NULL;
        }
        /* largest unavailable, try something smaller */
        togl->Width = togl->Width / 2 + togl->Width % 2;
        togl->Height = togl->Width / 2 + togl->Height % 2;
    }
}

static void
togl_destroyPbuffer(Togl *togl)
{
    aglDestroyPBuffer(togl->pbuf);
}
