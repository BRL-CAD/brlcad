/*
 * png.c --
 *
 *  PNG photo image type, Tcl/Tk package
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * This Tk image format handler reads and writes PNG files in the standard
 * JFIF file format.  ("PNG" should be the format name.)  It can also read
 * and write strings containing base64-encoded PNG data.
 *
 * Author : Jan Nijtmans *
 * Date   : 2/13/97        *
 * Original implementation : Joel Crisp     *
 *
 * $Id$
 */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include <tcl.h>
#include <pngtcl.h>
#include <string.h>
#include <stdlib.h>

static int SetupPngLibrary _ANSI_ARGS_ ((Tcl_Interp *interp));

#define MORE_INITIALIZATION \
    if (SetupPngLibrary (interp) != TCL_OK) { return TCL_ERROR; }

#include "init.c"



#define COMPRESS_THRESHOLD 1024

typedef struct png_text_struct_compat
{
   int  compression;       /* compression value:
                             -1: tEXt, none
                              0: zTXt, deflate
                              1: iTXt, none
                              2: iTXt, deflate  */
   png_charp key;          /* keyword, 1-79 character description of "text" */
   png_charp text;         /* comment, may be an empty string (ie "")
                              or a NULL pointer */
   png_size_t text_length; /* length of the text string */
   png_size_t itxt_length; /* length of the itxt string */
   png_charp lang;         /* language code, 0-79 characters
                              or a NULL pointer */
   png_charp lang_key;     /* keyword translated UTF-8 string, 0 or more
                              chars or a NULL pointer */
} png_text_compat;

typedef struct cleanup_info {
    Tcl_Interp *interp;
    jmp_buf jmpbuf;
} cleanup_info;

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
		  included already in Tk_PhotoImageBlock */
} myblock;


/*
 * Prototypes for local procedures defined in this file:
 */

static int CommonMatchPNG _ANSI_ARGS_((tkimg_MFile *handle, int *widthPtr,
	int *heightPtr));

static int CommonReadPNG _ANSI_ARGS_((png_structp png_ptr,
        Tcl_Interp* interp, Tcl_Obj *format,
	Tk_PhotoHandle imageHandle, int destX, int destY, int width,
	int height, int srcX, int srcY));

static int CommonWritePNG _ANSI_ARGS_((Tcl_Interp *interp, png_structp png_ptr,
	png_infop info_ptr, Tcl_Obj *format,
	Tk_PhotoImageBlock *blockPtr));

static void tk_png_error _ANSI_ARGS_((png_structp, png_const_charp));

static void tk_png_warning _ANSI_ARGS_((png_structp, png_const_charp));

static int load_png_library _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * These functions are used for all Input/Output.
 */

static void	tk_png_read _ANSI_ARGS_((png_structp, png_bytep,
		    png_size_t));

static void	tk_png_write _ANSI_ARGS_((png_structp, png_bytep,
		    png_size_t));

static void	tk_png_flush _ANSI_ARGS_((png_structp));

/*
 *
 */

static int
SetupPngLibrary (interp)
    Tcl_Interp *interp;
{
    if (Png_InitStubs(interp, PNGTCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }
    return TCL_OK;
}

static void
tk_png_error(png_ptr, error_msg)
    png_structp png_ptr;
    png_const_charp error_msg;
{
    cleanup_info *info = (cleanup_info *) png_get_error_ptr(png_ptr);
    Tcl_AppendResult(info->interp, error_msg, (char *) NULL);
    longjmp(info->jmpbuf,1);
}

static void
tk_png_warning(png_ptr, error_msg)
    png_structp png_ptr;
    png_const_charp error_msg;
{
    return;
}

static void
tk_png_read(png_ptr, data, length)
    png_structp png_ptr;
    png_bytep data;
    png_size_t length;
{
    if (tkimg_Read((tkimg_MFile *) png_get_progressive_ptr(png_ptr),
	    (char *) data, (size_t) length) != (int) length) {
	png_error(png_ptr, "Read Error");
    }
}

static void
tk_png_write(png_ptr, data, length)
    png_structp png_ptr;
    png_bytep data;
    png_size_t length;
{
    if (tkimg_Write((tkimg_MFile *) png_get_progressive_ptr(png_ptr),
	    (char *) data, (size_t) length) != (int) length) {
	png_error(png_ptr, "Write Error");
    }
}

static void
tk_png_flush(png_ptr)
    png_structp png_ptr;
{
}

static int
ChnMatch (interp, chan, fileName, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    CONST char *fileName;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixChanMatchProc(&interp, &chan, &fileName, &format, &widthPtr, &heightPtr);

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    return CommonMatchPNG(&handle, widthPtr, heightPtr);
}

static int
ObjMatch (interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    tkimg_MFile handle;

    tkimg_FixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    if (!tkimg_ReadInit(data,'\211',&handle)) {
	return 0;
    }
    return CommonMatchPNG(&handle, widthPtr, heightPtr);
}

static int
CommonMatchPNG(handle, widthPtr, heightPtr)
    tkimg_MFile *handle;
    int *widthPtr, *heightPtr;
{
    unsigned char buf[8];

    if ((tkimg_Read(handle, (char *) buf, 8) != 8)
	    || (strncmp("\211\120\116\107\15\12\32\12", (char *) buf, 8) != 0)
	    || (tkimg_Read(handle, (char *) buf, 8) != 8)
	    || (strncmp("\111\110\104\122", (char *) buf+4, 4) != 0)
	    || (tkimg_Read(handle, (char *) buf, 8) != 8)) {
	return 0;
    }
    *widthPtr = (buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + buf[3];
    *heightPtr = (buf[4]<<24) + (buf[5]<<16) + (buf[6]<<8) + buf[7];
    return 1;
}

static int
ChnRead (interp, chan, fileName, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    CONST char *fileName;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    png_structp png_ptr;
    tkimg_MFile handle;
    cleanup_info cleanup;

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    cleanup.interp = interp;

    png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,
	    (png_voidp) &cleanup,tk_png_error,tk_png_warning);
    if (!png_ptr) return(0); 

    png_set_read_fn(png_ptr, (png_voidp) &handle, tk_png_read);

    return CommonReadPNG(png_ptr, interp, format, imageHandle, destX, destY,
	    width, height, srcX, srcY);
}

static int
ObjRead (interp, dataObj, format, imageHandle,
	destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Obj *dataObj;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    png_structp png_ptr;
    tkimg_MFile handle;
    cleanup_info cleanup;

    cleanup.interp = interp;

    png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,
	    (png_voidp) &cleanup,tk_png_error,tk_png_warning);
    if (!png_ptr) return TCL_ERROR; 

    tkimg_ReadInit(dataObj,'\211',&handle);

    png_set_read_fn(png_ptr,(png_voidp) &handle, tk_png_read);

    return CommonReadPNG(png_ptr, interp, format, imageHandle, destX, destY,
	    width, height, srcX, srcY);
}

static int
CommonReadPNG(png_ptr, interp, format, imageHandle, destX, destY,
	width, height, srcX, srcY)
    png_structp png_ptr;
    Tcl_Interp *interp;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
#define block bl.ck
    png_infop info_ptr;
    png_infop end_info;
    char **png_data = NULL;
    myblock bl;
    unsigned int I;
    png_uint_32 info_width, info_height;
    int bit_depth, color_type, interlace_type;
    int intent;

    info_ptr=png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_read_struct(&png_ptr,NULL,NULL);
	return(TCL_ERROR);
    }

    end_info=png_create_info_struct(png_ptr);
    if (!end_info) {
	png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
	return(TCL_ERROR);
    }

    if (setjmp((((cleanup_info *) png_get_error_ptr(png_ptr))->jmpbuf))) {
	if (png_data) {
	    ckfree((char *)png_data);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	return TCL_ERROR;
    }

    png_read_info(png_ptr,info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &info_width, &info_height, &bit_depth,
	&color_type, &interlace_type, (int *) NULL, (int *) NULL);

    if ((srcX + width) > (int) info_width) {
	width = info_width - srcX;
    }
    if ((srcY + height) > (int) info_height) {
	height = info_height - srcY;
    }
    if ((width <= 0) || (height <= 0)
	|| (srcX >= (int) info_width)
	|| (srcY >= (int) info_height)) {
	return TCL_OK;
    }

    tkimg_PhotoExpand(imageHandle, interp, destX + width, destY + height);

    Tk_PhotoGetImage(imageHandle, &block);

    if (png_set_strip_16 != NULL) {
	png_set_strip_16(png_ptr);
    } else if (bit_depth == 16) {
	block.offset[1] = 2;
	block.offset[2] = 4;
    }

    if (png_set_expand != NULL) {
	png_set_expand(png_ptr);
    }

    png_read_update_info(png_ptr,info_ptr);
    block.pixelSize = png_get_channels(png_ptr, info_ptr);
    block.pitch = png_get_rowbytes(png_ptr, info_ptr);

    if ((color_type & PNG_COLOR_MASK_COLOR) == 0) {
	/* grayscale image */
	block.offset[1] = 0;
	block.offset[2] = 0;
    }
    block.width = width;
    block.height = height;

    if ((color_type & PNG_COLOR_MASK_ALPHA)
	    || png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
	/* with alpha channel */
	block.offset[3] = block.pixelSize - 1;
    } else {
	/* without alpha channel */
	block.offset[3] = 0;
    }

    if (png_get_sRGB && png_get_sRGB(png_ptr, info_ptr, &intent)) {
	png_set_sRGB(png_ptr, info_ptr, intent);
    } else if (png_get_gAMA != NULL) {
	double gamma;
	if (!png_get_gAMA(png_ptr, info_ptr, &gamma)) {
	    gamma = 0.45455;
	}
	png_set_gamma(png_ptr, 1.0, gamma);
    }

    png_data= (char **) ckalloc(sizeof(char *) * info_height +
	    info_height * block.pitch);

    for(I=0;I<info_height;I++) {
	png_data[I]= ((char *) png_data) + (sizeof(char *) * info_height +
		I * block.pitch);
    }
    block.pixelPtr=(unsigned char *) (png_data[srcY]+srcX*block.pixelSize);

    png_read_image(png_ptr,(png_bytepp) png_data);

    tkimg_PhotoPutBlock(imageHandle,&block,destX,destY,width,height);

    ckfree((char *) png_data);
    png_destroy_read_struct(&png_ptr,&info_ptr,&end_info);

    return(TCL_OK);
}

static int
ChnWrite (interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    CONST char *filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    png_structp png_ptr;
    png_infop info_ptr;
    tkimg_MFile handle;
    int result;
    cleanup_info cleanup;
    Tcl_Channel chan = (Tcl_Channel) NULL;

    chan = tkimg_OpenFileChannel(interp, filename, 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = (char *) chan;
    handle.state = IMG_CHAN;

    cleanup.interp = interp;

    png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING,
	    (png_voidp) &cleanup,tk_png_error,tk_png_warning);
    if (!png_ptr) {
	Tcl_Close(NULL, chan);
	return TCL_ERROR;
    }

    info_ptr=png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_write_struct(&png_ptr,NULL);
	Tcl_Close(NULL, chan);
	return TCL_ERROR;
    }

    png_set_write_fn(png_ptr,(png_voidp) &handle, tk_png_write, tk_png_flush);

    result = CommonWritePNG(interp, png_ptr, info_ptr, format, blockPtr);
    Tcl_Close(NULL, chan);
    return result;
}

static int
StringWrite (interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    png_structp png_ptr;
    png_infop info_ptr;
    tkimg_MFile handle;
    int result;
    cleanup_info cleanup;
    Tcl_DString data;

    tkimg_FixStringWriteProc(&data, &interp, &dataPtr, &format, &blockPtr);

    cleanup.interp = interp;

    png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING,
	    (png_voidp) &cleanup, tk_png_error, tk_png_warning);
    if (!png_ptr) {
	return TCL_ERROR;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_write_struct(&png_ptr,NULL);
	return TCL_ERROR;
    }

    png_set_write_fn(png_ptr, (png_voidp) &handle, tk_png_write, tk_png_flush);

    tkimg_WriteInit(dataPtr, &handle);

    result = CommonWritePNG(interp, png_ptr, info_ptr, format, blockPtr);
    tkimg_Putc(IMG_DONE, &handle);
    if ((result == TCL_OK) && (dataPtr == &data)) {
	Tcl_DStringResult(interp, dataPtr);
    }
    return result;
}

static int
CommonWritePNG(interp, png_ptr, info_ptr, format, blockPtr)
    Tcl_Interp *interp;
    png_structp png_ptr;
    png_infop info_ptr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    int greenOffset, blueOffset, alphaOffset;
    int tagcount = 0;
    Tcl_Obj **tags = (Tcl_Obj **) NULL;
    int I, pass, number_passes, color_type;  
    int newPixelSize;
    png_bytep row_pointers = (png_bytep) NULL;

    if (tkimg_ListObjGetElements(interp, format, &tagcount, &tags) != TCL_OK) {
	return TCL_ERROR;
    }
    tagcount = (tagcount > 1) ? (tagcount/2 - 1) : 0;

    if (setjmp((((cleanup_info *) png_get_error_ptr(png_ptr))->jmpbuf))) {
	if (row_pointers) {
	    ckfree((char *) row_pointers);
	}
	png_destroy_write_struct(&png_ptr,&info_ptr);
	return TCL_ERROR;
    }
    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    alphaOffset = blockPtr->offset[0];
    if (alphaOffset < blockPtr->offset[2]) {
	alphaOffset = blockPtr->offset[2];
    }
    if (++alphaOffset < blockPtr->pixelSize) {
	alphaOffset -= blockPtr->offset[0];
    } else {
	alphaOffset = 0;
    }

    if (greenOffset || blueOffset) {
	color_type = PNG_COLOR_TYPE_RGB;
	newPixelSize = 3;
    } else {
	color_type = PNG_COLOR_TYPE_GRAY;
	newPixelSize = 1;
    }
    if (alphaOffset) {
	color_type |= PNG_COLOR_MASK_ALPHA;
	newPixelSize++;
#if 0 /* The function png_set_filler doesn't seem to work; don't known why :-( */
    } else if ((blockPtr->pixelSize==4) && (newPixelSize == 3)
	    && (png_set_filler != NULL)) {
	/*
	 * The set_filler() function doesn't need to be called
	 * because the code below can handle all necessary
	 * re-allocation of memory. Only it is more economically
	 * to let the PNG library do that, which is only
	 * possible with v0.95 and higher.
	 */
	png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
	newPixelSize++;
#endif
    }

    png_set_IHDR(png_ptr, info_ptr, blockPtr->width, blockPtr->height, 8,
	    color_type, PNG_INTERLACE_ADAM7, PNG_COMPRESSION_TYPE_BASE,
	    PNG_FILTER_TYPE_BASE);

    if (png_set_gAMA != NULL) {
	png_set_gAMA(png_ptr, info_ptr, 1.0);
    }

    if (tagcount > 0) {
	png_text_compat text;
	for(I=0;I<tagcount;I++) {
	    int length;
	    text.key = Tcl_GetStringFromObj(tags[2*I+1], (int *) NULL);
	    text.text = Tcl_GetStringFromObj(tags[2*I+2], &length);
	    text.text_length = length;
	    if (text.text_length>COMPRESS_THRESHOLD) { 
		text.compression = PNG_TEXT_COMPRESSION_zTXt;
	    } else {
		text.compression = PNG_TEXT_COMPRESSION_NONE;
	    }
	    text.lang = NULL;
	    png_set_text(png_ptr, info_ptr, (png_text *) &text, 1);
        }
    }
    png_write_info(png_ptr,info_ptr);

    number_passes = png_set_interlace_handling(png_ptr);

    if (blockPtr->pixelSize != newPixelSize) {
	int J, oldPixelSize;
	png_bytep src, dst;
	oldPixelSize = blockPtr->pixelSize;
	row_pointers = (png_bytep)
		ckalloc(blockPtr->width * newPixelSize);
	for (pass = 0; pass < number_passes; pass++) {
	    for(I=0; I<blockPtr->height; I++) {
		src = (png_bytep) blockPtr->pixelPtr
			+ I * blockPtr->pitch + blockPtr->offset[0];
		dst = row_pointers;
		for (J = blockPtr->width; J > 0; J--) {
		    memcpy(dst, src, newPixelSize);
		    src += oldPixelSize;
		    dst += newPixelSize;
		}
		png_write_row(png_ptr, row_pointers);
	    }
	}
	ckfree((char *) row_pointers);
	row_pointers = NULL;
    } else {
	for (pass = 0; pass < number_passes; pass++) {
	    for(I=0;I<blockPtr->height;I++) {
		png_write_row(png_ptr, (png_bytep) blockPtr->pixelPtr
			+ I * blockPtr->pitch + blockPtr->offset[0]);
	    }
	}
    }
    png_write_end(png_ptr,NULL);
    png_destroy_write_struct(&png_ptr,&info_ptr);

    return(TCL_OK);
}
