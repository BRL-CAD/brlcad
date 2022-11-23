/*
 * tkMacOSXImage.c --
 *
 *	The code in this file provides an interface for XImages,
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 2001-2009, Apple Inc.
 * Copyright (c) 2005-2009 Daniel A. Steffen <das@users.sourceforge.net>
 * Copyright (c) 2017-2021 Marc Culler.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "xbytes.h"

static CGImageRef CreateCGImageFromPixmap(Drawable pixmap);
static CGImageRef CreateCGImageFromDrawableRect( Drawable drawable,
	   int x, int y, unsigned int width, unsigned int height);

/* Pixel formats
 *
 * Tk uses the XImage structure defined in Xlib.h for storing images.  The
 * image data in an XImage is a 32-bit aligned array of bytes.  Interpretation
 * of that data is not specified, but the structure includes parameters which
 * provide interpretation hints so that an application can use a family of
 * different data structures.
 *
 * The possible values for the XImage format field are XYBitmap, XYPixmap and
 * ZPixmap.  The macOS port does not support the XYPixmap format.  This means
 * that bitmap images are stored as a single bit plane (XYBitmap) and that
 * color images are stored as a sequence of pixel values (ZPixmap).
 *
 * For a ZPixmap, the number of bits allocated to each pixel is specified by
 * the bits_per_pixel field of the XImage structure.  The functions in this
 * module which convert between XImage and native CGImage or NSImage structures
 * only support XImages with 32 bits per pixel.  The ImageGetPixel and PutPixel
 * implementations in this file allow 1, 4, 8, 16 or 32 bits per pixel, however.
 *
 * In tkImgPhInstance.c the layout used for pixels is determined by the values
 * of the red_mask, blue_mask and green_mask fields in the XImage structure.
 * The Aqua port always sets red_mask = 0xFF0000, green_mask = 0xFF00, and
 * blue_mask = 0xFF. This means that a 32bpp ZPixmap XImage uses ARGB32 pixels,
 * with small-endian byte order BGRA. The data array for such an XImage can be
 * passed directly to construct a CGBitmapImageRep if one specifies the
 * bitmapInfo as kCGBitmapByteOrder32Big | kCGImageAlphaLast.
 *
 * The structures below describe the bitfields in two common 32 bpp pixel
 * layouts.  Note that bit field layouts are compiler dependent. The layouts
 * shown in the comments are those produced by clang and gcc.  Also note
 * that kCGBitmapByteOrder32Big is consistently set when creating CGImages or
 * CGImageBitmapReps.
 */

/* RGBA32 0xRRGGBBAA (Byte order is RGBA on big-endian systems.)
 * This is used by NSBitmapImageRep when the bitmapFormat property is 0,
 * the default value.
 */

typedef struct RGBA32pixel_t {
    unsigned red: 8;
    unsigned green: 8;
    unsigned blue: 8;
    unsigned alpha: 8;
} RGBA32pixel;

/*
 * ARGB32 0xAARRGGBB (Byte order is ARGB on big-endian systems.)
 * This is used by Aqua Tk for XImages and by NSBitmapImageReps whose
 * bitmapFormat property is NSBitmapFormatAlphaFirst.
 */

typedef struct ARGB32pixel_t {
    unsigned blue: 8;
    unsigned green: 8;
    unsigned red: 8;
    unsigned alpha: 8;
} ARGB32pixel;

typedef union pixel32_t {
    unsigned int uint;
    RGBA32pixel rgba;
    ARGB32pixel argb;
} pixel32;

#pragma mark XImage handling

int
_XInitImageFuncPtrs(
    TCL_UNUSED(XImage *)) /* image */
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXCreateCGImageWithXImage --
 *
 *	Create CGImage from XImage, copying the image data.  Called
 *      in Tk_PutImage and (currently) nowhere else.
 *
 * Results:
 *	CGImage, release after use.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void ReleaseData(
    void *info,
    TCL_UNUSED(const void *), /* data */
    TCL_UNUSED(size_t))       /* size */
{
    ckfree(info);
}

CGImageRef
TkMacOSXCreateCGImageWithXImage(
    XImage *image,
    uint32_t alphaInfo)
{
    CGImageRef img = NULL;
    size_t bitsPerComponent, bitsPerPixel;
    size_t len = image->bytes_per_line * image->height;
    const CGFloat *decode = NULL;
    CGBitmapInfo bitmapInfo;
    CGDataProviderRef provider = NULL;
    char *data = NULL;
    CGDataProviderReleaseDataCallback releaseData = ReleaseData;

    if (image->bits_per_pixel == 1) {
	/*
	 * BW image
	 */

	/* Reverses the sense of the bits */
	static const CGFloat decodeWB[2] = {1, 0};
	decode = decodeWB;

	bitsPerComponent = 1;
	bitsPerPixel = 1;
	if (image->bitmap_bit_order != MSBFirst) {
	    char *srcPtr = image->data + image->xoffset;
	    char *endPtr = srcPtr + len;
	    char *destPtr = (data = (char *)ckalloc(len));

	    while (srcPtr < endPtr) {
		*destPtr++ = xBitReverseTable[(unsigned char)(*(srcPtr++))];
	    }
	} else {
	    data = (char *)memcpy(ckalloc(len), image->data + image->xoffset, len);
	}
	if (data) {
	    provider = CGDataProviderCreateWithData(data, data, len,
		    releaseData);
	}
	if (provider) {
	    img = CGImageMaskCreate(image->width, image->height,
		    bitsPerComponent, bitsPerPixel, image->bytes_per_line,
		    provider, decode, 0);
	}
    } else if ((image->format == ZPixmap) && (image->bits_per_pixel == 32)) {

	/*
	 * Color image
	 */

	CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

	if (image->width == 0 && image->height == 0) {

	    /*
	     * CGCreateImage complains on early macOS releases.
	     */

	    return NULL;
	}
	bitsPerComponent = 8;
	bitsPerPixel = 32;
	bitmapInfo = kCGBitmapByteOrder32Big | alphaInfo;
	data = (char *)memcpy(ckalloc(len), image->data + image->xoffset, len);
	if (data) {
	    provider = CGDataProviderCreateWithData(data, data, len,
		    releaseData);
	}
	if (provider) {
	    img = CGImageCreate(image->width, image->height, bitsPerComponent,
		    bitsPerPixel, image->bytes_per_line, colorspace, bitmapInfo,
		    provider, decode, 0, kCGRenderingIntentDefault);
	    CFRelease(provider);
	}
	if (colorspace) {
	    CFRelease(colorspace);
	}
    } else {
	TkMacOSXDbgMsg("Unsupported image type");
    }
    return img;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyImage --
 *
 *	Destroys storage associated with an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deallocates the image.
 *
 *----------------------------------------------------------------------
 */

static int
DestroyImage(
    XImage *image)
{
    if (image) {
	if (image->data) {
	    ckfree(image->data);
	}
	ckfree(image);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageGetPixel --
 *
 *	Get a single pixel from an image.
 *
 * Results:
 *      The XColor structure contains an unsigned long field named pixel which
 *      identifies the color.  This function returns the unsigned long that
 *      would be used as the pixel value of an XColor that has the same red
 *      green and blue components as the XImage pixel at the specified
 *      location.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned long
ImageGetPixel(
    XImage *image,
    int x,
    int y)
{
    unsigned char r = 0, g = 0, b = 0;

    /*
     * Compute 8 bit red green and blue values, which are passed as inputs to
     * TkMacOSXRGBPixel to produce the pixel value.
     */

    if (image && image->data) {
	unsigned char *srcPtr = ((unsigned char*) image->data)
		+ (y * image->bytes_per_line)
		+ (((image->xoffset + x) * image->bits_per_pixel) / NBBY);

	switch (image->bits_per_pixel) {
	case 32: /* 8 bits per channel */
	    {
		ARGB32pixel *pixel = (ARGB32pixel *)srcPtr;
		r = pixel->red;
		g = pixel->green;
		b = pixel->blue;
	    }
	    break;
	case 16: /* 5 bits per channel */
	    r = (*((unsigned short*) srcPtr) >> 7) & 0xf8;
	    g = (*((unsigned short*) srcPtr) >> 2) & 0xf8;
	    b = (*((unsigned short*) srcPtr) << 3) & 0xf8;
	    break;
	case 8: /* 2 bits per channel */
	    r = (*srcPtr << 2) & 0xc0;
	    g = (*srcPtr << 4) & 0xc0;
	    b = (*srcPtr << 6) & 0xc0;
	    r |= r >> 2 | r >> 4 | r >> 6;
	    g |= g >> 2 | g >> 4 | g >> 6;
	    b |= b >> 2 | b >> 4 | b >> 6;
	    break;
	case 4: { /* 1 bit per channel */
	    unsigned char c = (x % 2) ? *srcPtr : (*srcPtr >> 4);

	    r = (c & 0x04) ? 0xff : 0;
	    g = (c & 0x02) ? 0xff : 0;
	    b = (c & 0x01) ? 0xff : 0;
	    break;
	}
	case 1: /* Black-white bitmap. */
	    r = g = b = ((*srcPtr) & (0x80 >> (x % 8))) ? 0xff : 0;
	    break;
	}
    }

    return TkMacOSXRGBPixel(r, g, b);
}

/*
 *----------------------------------------------------------------------
 *
 * ImagePutPixel --
 *
 *	Set a single pixel in an image.  The pixel is provided as an unsigned
 *      32-bit integer.  The value of that integer is interpreted by assuming
 *      that its low-order N bits have the format specified by the XImage,
 *      where N is equal to the bits_per_pixel field of the XImage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ImagePutPixel(
    XImage *image,
    int x,
    int y,
    unsigned long pixel)
{
    if (image && image->data) {
	unsigned char *dstPtr = ((unsigned char*) image->data)
		+ (y * image->bytes_per_line)
		+ (((image->xoffset + x) * image->bits_per_pixel) / NBBY);

	if (image->bits_per_pixel == 32) {
	    *((unsigned int*) dstPtr) = pixel;
	} else {
	    switch (image->bits_per_pixel) {
	    case 16:
		*((unsigned short*) dstPtr) = pixel & 0xffff;
		break;
	    case 8:
		*dstPtr = pixel & 0xff;
		break;
	    case 4: {
		*dstPtr = (x % 2) ? ((*dstPtr & 0xf0) | (pixel & 0x0f)) :
			((*dstPtr & 0x0f) | ((pixel << 4) & 0xf0));
		break;
		}
	    case 1:
		*dstPtr = pixel ? (*dstPtr | (0x80 >> (x % 8))) :
			(*dstPtr & ~(0x80 >> (x % 8)));
		break;
	    }
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XCreateImage --
 *
 *	Allocates storage for a new XImage.
 *
 * Results:
 *	Returns a newly allocated XImage.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XImage *
XCreateImage(
    Display* display,
    TCL_UNUSED(Visual*),  /* visual */
    unsigned int depth,
    int format,
    int offset,
    char* data,
    unsigned int width,
    unsigned int height,
    int bitmap_pad,
    int bytes_per_line)
{
    XImage *ximage;

    display->request++;
    ximage = (XImage *)ckalloc(sizeof(XImage));

    ximage->height = height;
    ximage->width = width;
    ximage->depth = depth;
    ximage->xoffset = offset;
    ximage->format = format;
    ximage->data = data;
    ximage->obdata = NULL;

    if (format == ZPixmap) {
	ximage->bits_per_pixel = 32;
	ximage->bitmap_unit = 32;
    } else {
	ximage->bits_per_pixel = 1;
	ximage->bitmap_unit = 8;
    }
    if (bitmap_pad) {
	ximage->bitmap_pad = bitmap_pad;
    } else {
	/*
	 * Use 16 byte alignment for best Quartz perfomance.
	 */

	ximage->bitmap_pad = 128;
    }
    if (bytes_per_line) {
	ximage->bytes_per_line = bytes_per_line;
    } else {
	ximage->bytes_per_line = ((width * ximage->bits_per_pixel +
		(ximage->bitmap_pad - 1)) >> 3) &
		~((ximage->bitmap_pad >> 3) - 1);
    }
#ifdef WORDS_BIGENDIAN
    ximage->byte_order = MSBFirst;
    ximage->bitmap_bit_order = MSBFirst;
#else
    ximage->byte_order = LSBFirst;
    ximage->bitmap_bit_order = LSBFirst;
#endif
    ximage->red_mask = 0x00FF0000;
    ximage->green_mask = 0x0000FF00;
    ximage->blue_mask = 0x000000FF;
    ximage->f.create_image = NULL;
    ximage->f.destroy_image = DestroyImage;
    ximage->f.get_pixel = ImageGetPixel;
    ximage->f.put_pixel = ImagePutPixel;
    ximage->f.sub_image = NULL;
    ximage->f.add_pixel = NULL;

    return ximage;
}

/*
 *----------------------------------------------------------------------
 *
 * TkPutImage, XPutImage, TkpPutRGBAImage --
 *
 *	These functions, which all have the same signature, copy a rectangular
 *      subimage of an XImage into a drawable.  The first two are identical on
 *      macOS.  They assume that the XImage data has the structure of a 32bpp
 *      ZPixmap in which the image data is an array of 32bit integers packed
 *      with 8 bit values for the Red Green and Blue channels.  They ignore the
 *      fourth byte.  The function TkpPutRGBAImage assumes that the XImage data
 *      has been extended by using the fourth byte to store an 8-bit Alpha
 *      value.  (The Alpha data is assumed not to pre-multiplied).  The image
 *      is then drawn into the drawable using standard Porter-Duff Source Atop
 *      Composition (kCGBlendModeSourceAtop in Apple's Core Graphics).
 *
 *      The TkpPutRGBAImage function is used by TkImgPhotoDisplay to render photo
 *      images if the compile-time variable TK_CAN_RENDER_RGBA is defined in
 *      a platform's tkXXXXPort.h header, as is the case for the macOS Aqua port.
 *
 * Results:
 *	These functions return either BadDrawable or Success.
 *
 * Side effects:
 *	Draws the image on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

#define USE_ALPHA kCGImageAlphaLast
#define IGNORE_ALPHA kCGImageAlphaNoneSkipLast

static int
TkMacOSXPutImage(
    uint32_t pixelFormat,
    Display* display,		/* Display. */
    Drawable drawable,		/* Drawable to place image on. */
    GC gc,			/* GC to use. */
    XImage* image,		/* Image to place. */
    int src_x,			/* Source X & Y. */
    int src_y,
    int dest_x,			/* Destination X & Y. */
    int dest_y,
    unsigned int width,	        /* Same width & height for both */
    unsigned int height)	/* destination and source. */
{
    TkMacOSXDrawingContext dc;
    MacDrawable *macDraw = (MacDrawable *)drawable;
    int result = Success;
    display->request++;
    if (!TkMacOSXSetupDrawingContext(drawable, gc, &dc)) {
	return BadDrawable;
    }
    if (dc.context) {
	CGRect bounds, srcRect, dstRect;
	CGImageRef img = TkMacOSXCreateCGImageWithXImage(image, pixelFormat);

	/*
	 * The CGContext for a pixmap is RGB only, with A = 0.
	 */

	if (!(macDraw->flags & TK_IS_PIXMAP)) {
	    CGContextSetBlendMode(dc.context, kCGBlendModeSourceAtop);
	}
	if (img) {
	    bounds = CGRectMake(0, 0, image->width, image->height);
	    srcRect = CGRectMake(src_x, src_y, width, height);
	    dstRect = CGRectMake(dest_x, dest_y, width, height);
	    TkMacOSXDrawCGImage(drawable, gc, dc.context,
				img, gc->foreground, gc->background,
				bounds, srcRect, dstRect);
	    CFRelease(img);
	} else {
	    TkMacOSXDbgMsg("Invalid source drawable");
	    result = BadDrawable;
	}
    } else {
	TkMacOSXDbgMsg("Invalid destination drawable");
	result = BadDrawable;
    }
    TkMacOSXRestoreDrawingContext(&dc);
    return result;
}

int XPutImage(
    Display* display,
    Drawable drawable,
    GC gc,
    XImage* image,
    int src_x,
    int src_y,
    int dest_x,
    int dest_y,
    unsigned int width,
    unsigned int height) {
    return TkMacOSXPutImage(IGNORE_ALPHA, display, drawable, gc, image,
			    src_x, src_y, dest_x, dest_y, width, height);
}

int TkPutImage(
    TCL_UNUSED(unsigned long *),
    TCL_UNUSED(int),
    Display* display,
    Drawable drawable,
    GC gc,
    XImage* image,
    int src_x,
    int src_y,
    int dest_x,
    int dest_y,
    unsigned int width,
    unsigned int height) {
    return TkMacOSXPutImage(IGNORE_ALPHA, display, drawable, gc, image,
		     src_x, src_y, dest_x, dest_y, width, height);
}

int TkpPutRGBAImage(
    Display* display,
    Drawable drawable,
    GC gc,
    XImage* image,
    int src_x,
    int src_y,
    int dest_x,
    int dest_y,
    unsigned int width,
    unsigned int height) {
    return TkMacOSXPutImage(USE_ALPHA, display, drawable, gc, image,
		     src_x, src_y, dest_x, dest_y, width, height);
}


/*
 *----------------------------------------------------------------------
 *
 * CreateCGImageFromDrawableRect
 *
 *	Extract image data from a MacOSX drawable as a CGImage.  The drawable
 *      may be either a pixmap or a window, but there issues in the case of
 *      a window.
 *
 *      CreateCGImageFromDrawableRect is called by XGetImage and XCopyArea.
 *      The Tk core uses these two functions on some platforms in order to
 *      implement explicit double-buffered drawing -- a pixmap is copied from a
 *      window, modified using CPU-based graphics composition, and then copied
 *      back to the window.  Platforms, such as macOS, on which the system
 *      provides double-buffered drawing and GPU-based composition operations
 *      can avoid calls to XGetImage and XCopyArea from the core by defining
 *      the compile-time variable TK_NO_DOUBLE_BUFFERING.  Nonetheless, these
 *      two functions are in the stubs table and therefore could be used by
 *      extensions.
 *
 *      The implementation here does not always work correctly when the source
 *      is a window.  The original version of this function relied on
 *      [NSBitmapImageRep initWithFocusedViewRect:view_rect] which was
 *      deprecated by Apple in OSX 10.14 and also required the use of other
 *      deprecated functions such as [NSView lockFocus]. Apple's suggested
 *      replacement is [NSView cacheDisplayInRect: toBitmapImageRep:] and that
 *      is being used here.  However, cacheDisplayInRect works by calling
 *      [NSView drawRect] after setting the current graphics context to be one
 *      which draws to a bitmap.  There are situations in which this can be
 *      used, e.g. when taking a screenshot of a window.  But it cannot be used
 *      as part of a normal display procedure, using the copy-modify-paste
 *      paradigm that is the basis of the explicit double-buffering.  Since the
 *      copy operation will call the same display procedure that is calling
 *      this function via XGetImage or XCopyArea, this would create an infinite
 *      recursion.
 *
 *      An alternative to the copy-modify-paste paradigm is to use GPU-based
 *      graphics composition, clipping to the specified rectangle.  That is
 *      the approach that must be followed by display procedures on macOS.
 *
 * Results:
 *	Returns an NSBitmapRep representing the image of the given rectangle of
 *      the given drawable. This object is retained. The caller is responsible
 *      for releasing it.
 *
 *      NOTE: The x,y coordinates should be relative to a coordinate system
 *      with origin at the top left, as used by XImage and CGImage, not bottom
 *      left as used by NSView.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static CGImageRef
CreateCGImageFromDrawableRect(
    Drawable drawable,
    int x,
    int y,
    unsigned int width,
    unsigned int height)
{
    MacDrawable *mac_drawable = (MacDrawable *)drawable;
    CGContextRef cg_context = NULL;
    CGRect image_rect = CGRectMake(x, y, width, height);
    CGImageRef cg_image = NULL, result = NULL;
    unsigned char *imageData = NULL;
    if (mac_drawable->flags & TK_IS_PIXMAP) {
	cg_context = TkMacOSXGetCGContextForDrawable(drawable);
	if (cg_context) {
	    cg_image = CGBitmapContextCreateImage((CGContextRef) cg_context);
	}
    } else {
	NSView *view = TkMacOSXGetNSViewForDrawable(mac_drawable);
	if (view == nil) {
	    TkMacOSXDbgMsg("Invalid source drawable");
	    return NULL;
	}
	NSSize size = view.frame.size;
	NSUInteger view_width = size.width, view_height = size.height;
        NSUInteger bytesPerPixel = 4,
	    bytesPerRow = bytesPerPixel * view_width,
	    bitsPerComponent = 8;
        imageData = ckalloc(view_height * bytesPerRow);
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	cg_context = CGBitmapContextCreate(imageData, view_width, view_height,
			 bitsPerComponent, bytesPerRow, colorSpace,
			 kCGImageAlphaPremultipliedLast |
			 kCGBitmapByteOrder32Big);
	CFRelease(colorSpace);
	[view.layer renderInContext:cg_context];
    }
    if (cg_context) {
	cg_image = CGBitmapContextCreateImage(cg_context);
	CGContextRelease(cg_context);
    }
    if (cg_image) {
	result = CGImageCreateWithImageInRect(cg_image, image_rect);
	CGImageRelease(cg_image);
    }
    ckfree(imageData);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateCGImageFromPixmap --
 *
 *	Create a CGImage from an X Pixmap.
 *
 * Results:
 *	CGImage, release after use.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static CGImageRef
CreateCGImageFromPixmap(
    Drawable pixmap)
{
    CGImageRef img = NULL;
    CGContextRef context = TkMacOSXGetCGContextForDrawable(pixmap);

    if (context) {
	img = CGBitmapContextCreateImage(context);
    }
    return img;
}

/*
 *----------------------------------------------------------------------
 *
 * XGetImage --
 *
 *	This function copies data from a pixmap or window into an XImage.  It
 *      is essentially never used. At one time it was called by
 *      pTkImgPhotoDisplay, but that is no longer the case. Currently it is
 *      called two places, one of which is requesting an XY image which we do
 *      not support.  It probably does not work correctly -- see the comments
 *      for CGImageFromDrawableRect.
 *
 * Results:
 *	Returns a newly allocated XImage containing the data from the given
 *	rectangle of the given drawable, or NULL if the XImage could not be
 *	constructed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XImage *
XGetImage(
    Display *display,
    Drawable drawable,
    int x,
    int y,
    unsigned int width,
    unsigned int height,
    TCL_UNUSED(unsigned long), /* plane_mask */
    int format)
{
    NSBitmapImageRep* bitmapRep = nil;
    NSUInteger bitmap_fmt = 0;
    XImage* imagePtr = NULL;
    char *bitmap = NULL;
    int depth = 32, offset = 0, bitmap_pad = 0;
    unsigned int bytes_per_row, size, row, n, m;

    if (format == ZPixmap) {
	CGImageRef cgImage;
	if (width == 0 || height == 0) {
	    return NULL;
	}

	cgImage = CreateCGImageFromDrawableRect(drawable, x, y, width, height);
	if (cgImage) {
	    bitmapRep = [NSBitmapImageRep alloc];
	    [bitmapRep initWithCGImage:cgImage];
	    CFRelease(cgImage);
	} else {
	    TkMacOSXDbgMsg("XGetImage: Failed to construct CGImage");
	    return NULL;
	}
	bitmap_fmt = [bitmapRep bitmapFormat];
	size = [bitmapRep bytesPerPlane];
	bytes_per_row = [bitmapRep bytesPerRow];
	bitmap = (char *)ckalloc(size);
	if ((bitmap_fmt != 0 && bitmap_fmt != NSBitmapFormatAlphaFirst)
	    || [bitmapRep samplesPerPixel] != 4
	    || [bitmapRep isPlanar] != 0
	    || bytes_per_row < 4 * width
	    || size != bytes_per_row * height) {
	    TkMacOSXDbgMsg("XGetImage: Unrecognized bitmap format");
	    [bitmapRep release];
	    return NULL;
	}
	memcpy(bitmap, (char *)[bitmapRep bitmapData], size);
	[bitmapRep release];

	for (row = 0, n = 0; row < height; row++, n += bytes_per_row) {
	    for (m = n; m < n + 4*width; m += 4) {
		pixel32 pixel = *((pixel32 *)(bitmap + m));
		if (bitmap_fmt == 0) { // default format

		    /*
		     * This pixel is in ARGB32 format.  We need RGBA32.
		     */

		    pixel32 flipped;
		    flipped.rgba.red = pixel.argb.red;
		    flipped.rgba.green = pixel.argb.green;
		    flipped.rgba.blue = pixel.argb.blue;
		    flipped.rgba.alpha = pixel.argb.alpha;
		    *((pixel32 *)(bitmap + m)) = flipped;
		} else { // bitmap_fmt = NSBitmapFormatAlphaFirst
		    *((pixel32 *)(bitmap + m)) = pixel;
		}
	    }
	}
	imagePtr = XCreateImage(display, NULL, depth, format, offset,
		(char*) bitmap, width, height,
		bitmap_pad, bytes_per_row);
    } else {

	/*
	 * There are some calls to XGetImage in the generic Tk code which pass
	 * an XYPixmap rather than a ZPixmap.  XYPixmaps should be handled
	 * here.
	 */

	TkMacOSXDbgMsg("XGetImage does not handle XYPixmaps at the moment.");
    }
    return imagePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyArea --
 *
 *	Copies image data from one drawable to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Image data is moved from a window or bitmap to a second window or bitmap.
 *
 *----------------------------------------------------------------------
 */

int
XCopyArea(
    Display *display,		/* Display. */
    Drawable src,		/* Source drawable. */
    Drawable dst,		/* Destination drawable. */
    GC gc,			/* GC to use. */
    int src_x,			/* X & Y, width & height */
    int src_y,			/* define the source rectangle */
    unsigned int width,		/* that will be copied. */
    unsigned int height,
    int dest_x,			/* Dest X & Y on dest rect. */
    int dest_y)
{
    TkMacOSXDrawingContext dc;
    MacDrawable *srcDraw = (MacDrawable *)src;
    CGImageRef img = NULL;
    CGRect bounds, srcRect, dstRect;

    display->request++;
    if (!width || !height) {
	return BadDrawable;
    }

    if (!TkMacOSXSetupDrawingContext(dst, gc, &dc)) {
	TkMacOSXDbgMsg("Failed to setup drawing context.");
	return BadDrawable;
    }

    if (!dc.context) {
	TkMacOSXDbgMsg("Invalid destination drawable - no context.");
	return BadDrawable;
    }

    if (srcDraw->flags & TK_IS_PIXMAP) {
	img = CreateCGImageFromPixmap(src);
    } else if (TkMacOSXGetNSWindowForDrawable(src)) {
	img = CreateCGImageFromDrawableRect(src, src_x, src_y, width, height);
    } else {
	TkMacOSXDbgMsg("Invalid source drawable - neither window nor pixmap.");
    }

    if (img) {
	bounds = CGRectMake(0, 0, srcDraw->size.width, srcDraw->size.height);
	srcRect = CGRectMake(src_x, src_y, width, height);
	dstRect = CGRectMake(dest_x, dest_y, width, height);
	TkMacOSXDrawCGImage(dst, gc, dc.context, img,
		gc->foreground, gc->background, bounds, srcRect, dstRect);
	CFRelease(img);
    } else {
	TkMacOSXDbgMsg("Failed to construct CGImage.");
    }

    TkMacOSXRestoreDrawingContext(&dc);
    return Success;
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyPlane --
 *
 *	Copies a bitmap from a source drawable to a destination drawable. The
 *	plane argument specifies which bit plane of the source contains the
 *	bitmap. Note that this implementation ignores the gc->function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the destination drawable.
 *
 *----------------------------------------------------------------------
 */

int
XCopyPlane(
    Display *display,		/* Display. */
    Drawable src,		/* Source drawable. */
    Drawable dst,		/* Destination drawable. */
    GC gc,				/* GC to use. */
    int src_x,			/* X & Y, width & height */
    int src_y,			/* define the source rectangle */
    unsigned int width,	/* that will be copied. */
    unsigned int height,
    int dest_x,			/* Dest X & Y on dest rect. */
    int dest_y,
    unsigned long plane)	/* Which plane to copy. */
{
    TkMacOSXDrawingContext dc;
    MacDrawable *srcDraw = (MacDrawable *)src;
    MacDrawable *dstDraw = (MacDrawable *)dst;
    CGRect bounds, srcRect, dstRect;
    display->request++;
    if (!width || !height) {
	/* TkMacOSXDbgMsg("Drawing of empty area requested"); */
	return BadDrawable;
    }
    if (plane != 1) {
	Tcl_Panic("Unexpected plane specified for XCopyPlane");
    }
    if (srcDraw->flags & TK_IS_PIXMAP) {
	if (!TkMacOSXSetupDrawingContext(dst, gc, &dc)) {
	    return BadDrawable;
	}

	CGContextRef context = dc.context;

	if (context) {
	    CGImageRef img = CreateCGImageFromPixmap(src);

	    if (img) {
		TkpClipMask *clipPtr = (TkpClipMask *) gc->clip_mask;
		unsigned long imageBackground  = gc->background;

                if (clipPtr && clipPtr->type == TKP_CLIP_PIXMAP) {
		    srcRect = CGRectMake(src_x, src_y, width, height);
		    CGImageRef mask = CreateCGImageFromPixmap(
			    clipPtr->value.pixmap);
		    CGImageRef submask = CGImageCreateWithImageInRect(
			    img, srcRect);
		    CGRect rect = CGRectMake(dest_x, dest_y, width, height);

		    rect = CGRectOffset(rect, dstDraw->xOff, dstDraw->yOff);
		    CGContextSaveGState(context);

		    /*
		     * Move the origin of the destination to top left.
		     */

		    CGContextTranslateCTM(context,
			    0, rect.origin.y + CGRectGetMaxY(rect));
		    CGContextScaleCTM(context, 1, -1);

		    /*
		     * Fill with the background color, clipping to the mask.
		     */

		    CGContextClipToMask(context, rect, submask);
		    TkMacOSXSetColorInContext(gc, gc->background, dc.context);
		    CGContextFillRect(context, rect);

		    /*
		     * Fill with the foreground color, clipping to the
		     * intersection of img and mask.
		     */

		    CGImageRef subimage = CGImageCreateWithImageInRect(
			    img, srcRect);
		    CGContextClipToMask(context, rect, subimage);
		    TkMacOSXSetColorInContext(gc, gc->foreground, context);
		    CGContextFillRect(context, rect);
		    CGContextRestoreGState(context);
		    CGImageRelease(img);
		    CGImageRelease(mask);
		    CGImageRelease(submask);
		    CGImageRelease(subimage);
		} else {
		    bounds = CGRectMake(0, 0,
			    srcDraw->size.width, srcDraw->size.height);
		    srcRect = CGRectMake(src_x, src_y, width, height);
		    dstRect = CGRectMake(dest_x, dest_y, width, height);
		    TkMacOSXDrawCGImage(dst, gc, dc.context, img,
			    gc->foreground, imageBackground, bounds,
			    srcRect, dstRect);
		    CGImageRelease(img);
		}
	    } else {
		/* no image */
		TkMacOSXDbgMsg("Invalid source drawable");
	    }
	} else {
	    TkMacOSXDbgMsg("Invalid destination drawable - "
		    "could not get a bitmap context.");
	}
	TkMacOSXRestoreDrawingContext(&dc);
	return Success;
    } else {
	/*
	 * Source drawable is a Window, not a Pixmap.
	 */

	return XCopyArea(display, src, dst, gc, src_x, src_y, width, height,
		dest_x, dest_y);
    }
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
