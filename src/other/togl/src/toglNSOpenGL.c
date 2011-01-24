/* $Id$ */

/* vi:set sw=4 expandtab: */

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

static NSOpenGLPixelFormat *
togl_pixelFormat(Togl *togl)
{
    NSOpenGLPixelFormatAttribute   attribs[32];
    int     na = 0;
    NSOpenGLPixelFormat *pix;

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

    attribs[na++] = NSOpenGLPFAMinimumPolicy;
    /* ask for hardware-accelerated onscreen */
    attribs[na++] = NSOpenGLPFAAccelerated;
    attribs[na++] = NSOpenGLPFANoRecovery;
    if (togl->RgbaFlag) {
        /* RGB[A] mode */
        attribs[na++] = NSOpenGLPFAColorSize;
	attribs[na++] = togl->RgbaRed + togl->RgbaGreen + togl->RgbaBlue;
	/* NSOpenGL does not take separate red,green,blue sizes. */
        if (togl->AlphaFlag) {
            attribs[na++] = NSOpenGLPFAAlphaSize;
            attribs[na++] = togl->AlphaSize;
        }
    } else {
        /* Color index mode */
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "Color index mode not supported", TCL_STATIC);
        return NULL;
    }
    if (togl->DepthFlag) {
        attribs[na++] = NSOpenGLPFADepthSize;
        attribs[na++] = togl->DepthSize;
    }
    if (togl->DoubleFlag) {
        attribs[na++] = NSOpenGLPFADoubleBuffer;
    }
    if (togl->StencilFlag) {
        attribs[na++] = NSOpenGLPFAStencilSize;
        attribs[na++] = togl->StencilSize;
    }
    if (togl->AccumFlag) {
        attribs[na++] = NSOpenGLPFAAccumSize;
        attribs[na++] = togl->AccumRed + togl->AccumGreen + togl->AccumBlue + (togl->AlphaFlag ? togl->AccumAlpha : 0);
    }
    if (togl->MultisampleFlag) {
        attribs[na++] = NSOpenGLPFAMultisample;
        attribs[na++] = NSOpenGLPFASampleBuffers;
        attribs[na++] = 1;
        attribs[na++] = NSOpenGLPFASamples;
        attribs[na++] = 2;
    }
    if (togl->AuxNumber != 0) {
        attribs[na++] = NSOpenGLPFAAuxBuffers;
        attribs[na++] = togl->AuxNumber;
    }
    if (togl->Stereo == TOGL_STEREO_NATIVE) {
        attribs[na++] = NSOpenGLPFAStereo;
    }
    if (togl->FullscreenFlag) {
        attribs[na++] = NSOpenGLPFAFullScreen;
    }
    attribs[na++] = 0;	/* End of attributes. */

    pix = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    if (pix == nil) {
        Tcl_SetResult(togl->Interp, TCL_STUPID "couldn't choose pixel format",
                TCL_STATIC);
        return NULL;
    }
    return pix;
}

static int
togl_describePixelFormat(Togl *togl)
{
    NSOpenGLPixelFormat *pfmt = togl->PixelFormat;

    /* fill in RgbaFlag, DoubleFlag, and Stereo */
    GLint   has_rgba, has_doublebuf, has_depth, has_accum, has_alpha,
            has_stencil, has_stereo, has_multisample;

    GLint   vscr = 0;
    [pfmt getValues:&has_rgba forAttribute:NSOpenGLPFAColorSize forVirtualScreen:vscr];
    [pfmt getValues:&has_doublebuf forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:vscr];
    [pfmt getValues:&has_depth forAttribute:NSOpenGLPFADepthSize forVirtualScreen:vscr];
    [pfmt getValues:&has_accum forAttribute:NSOpenGLPFAAccumSize forVirtualScreen:vscr];
    [pfmt getValues:&has_alpha forAttribute:NSOpenGLPFAAlphaSize forVirtualScreen:vscr];
    [pfmt getValues:&has_stencil forAttribute:NSOpenGLPFAStencilSize forVirtualScreen:vscr];
    [pfmt getValues:&has_stereo forAttribute:NSOpenGLPFAStereo forVirtualScreen:vscr];
    [pfmt getValues:&has_multisample forAttribute:NSOpenGLPFASampleBuffers forVirtualScreen:vscr];

    togl->RgbaFlag = (has_rgba != 0);
    togl->DoubleFlag = (has_doublebuf != 0);
    togl->DepthFlag = (has_depth != 0);
    togl->AccumFlag = (has_accum != 0);
    togl->AlphaFlag = (has_alpha != 0);
    togl->StencilFlag = (has_stencil != 0);
    togl->Stereo = (has_stereo ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE);
    togl->MultisampleFlag = (has_multisample != 0);
    return True;
}

#define isPow2(x) (((x) & ((x) - 1)) == 0)

static NSOpenGLPixelBuffer *
togl_createPbuffer(Togl *togl)
{
    GLint   min_size[2], max_size[2];
    Bool    hasPbuffer;
    const char *extensions;
    GLint   target;
    GLint   virtualScreen;
    NSOpenGLPixelBuffer *pbuf;

    extensions = (const char *) glGetString(GL_EXTENSIONS);
    hasPbuffer = (strstr(extensions, "GL_APPLE_pixel_buffer") != NULL);
    if (!hasPbuffer) {
        Tcl_SetResult(togl->Interp,
                TCL_STUPID "pbuffers are not supported", TCL_STATIC);
        return NULL;
    }
    glGetIntegerv(GL_MIN_PBUFFER_VIEWPORT_DIMS_APPLE, min_size);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_size);
    virtualScreen = [togl->Ctx currentVirtualScreen];
    for (;;) {
        /* make sure we don't exceed the maximum size because if we do,
         * NSOpenGLPixelBuffer allocationmay succeed and later uses of
	 * the pbuffer fail
	 */
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

	pbuf = [[NSOpenGLPixelBuffer alloc] initWithTextureTarget:target
		textureInternalFormat:(togl->AlphaFlag ? GL_RGBA : GL_RGB)
		textureMaxMipMapLevel:0
		pixelsWide:togl->Width pixelsHigh:togl->Height];
        if (pbuf != nil) {
            /* setPixelBuffer allocates the framebuffer space */
	  [togl->Ctx setPixelBuffer:pbuf cubeMapFace:0 mipMapLevel:0 
	   currentVirtualScreen:virtualScreen];
	  return pbuf;
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
    [togl->pbuf release];
}
