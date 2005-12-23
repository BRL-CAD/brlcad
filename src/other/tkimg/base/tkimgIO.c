/*
 * tkimg.c --
 *
 *  Generic interface to XML parsers.
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * Zveno Pty Ltd makes this software and associated documentation
 * available free of charge for any purpose.  You may make copies
 * of the software but you must include all of this notice on any copy.
 *
 * Zveno Pty Ltd does not warrant that this software is error free
 * or fit for any purpose.  Zveno Pty Ltd disclaims any liability for
 * all claims, expenses, losses, damages and costs any user may incur
 * as a result of using, copying or modifying the software.
 *
 * $Id$
 *
 */

#include "tk.h"
#include "tkimg.h"

/*
 * Prototypes for procedures defined later in this file:
 */

/* Variables needed for optional read buffer. See tkimg_ReadBuffer. */
#define BUFLEN 4096
static int
	useReadBuf = 0,
        bufStart   = -1,
        bufEnd     = -1;
static char 
        readBuf[BUFLEN];

static int char64 _ANSI_ARGS_((int c));


/*
 *--------------------------------------------------------------------------
 * char64 --
 *
 *	This procedure converts a base64 ascii character into its binary
 *	equivalent. This code is a slightly modified version of the
 *	char64 proc in N. Borenstein's metamail decoder.
 *
 * Results:
 *	The binary value, or an error code.
 *
 * Side effects:
 *	None.
 *--------------------------------------------------------------------------
 */

static int
char64(c)
    int c;
{
    switch(c) {
	case 'A': return 0;	case 'B': return 1;	case 'C': return 2;
	case 'D': return 3;	case 'E': return 4;	case 'F': return 5;
	case 'G': return 6;	case 'H': return 7;	case 'I': return 8;
	case 'J': return 9;	case 'K': return 10;	case 'L': return 11;
	case 'M': return 12;	case 'N': return 13;	case 'O': return 14;
	case 'P': return 15;	case 'Q': return 16;	case 'R': return 17;
	case 'S': return 18;	case 'T': return 19;	case 'U': return 20;
	case 'V': return 21;	case 'W': return 22;	case 'X': return 23;
	case 'Y': return 24;	case 'Z': return 25;	case 'a': return 26;
	case 'b': return 27;	case 'c': return 28;	case 'd': return 29;
	case 'e': return 30;	case 'f': return 31;	case 'g': return 32;
	case 'h': return 33;	case 'i': return 34;	case 'j': return 35;
	case 'k': return 36;	case 'l': return 37;	case 'm': return 38;
	case 'n': return 39;	case 'o': return 40;	case 'p': return 41;
	case 'q': return 42;	case 'r': return 43;	case 's': return 44;
	case 't': return 45;	case 'u': return 46;	case 'v': return 47;
	case 'w': return 48;	case 'x': return 49;	case 'y': return 50;
	case 'z': return 51;	case '0': return 52;	case '1': return 53;
	case '2': return 54;	case '3': return 55;	case '4': return 56;
	case '5': return 57;	case '6': return 58;	case '7': return 59;
	case '8': return 60;	case '9': return 61;	case '+': return 62;
	case '/': return 63;

	case ' ': case '\t': case '\n': case '\r': case '\f': return IMG_SPACE;
	case '=': return IMG_PAD;
	case '\0': return IMG_DONE;
	default: return IMG_BAD;
    }
}

/*
 *--------------------------------------------------------------
 *
 * tkimg_ReadBuffer -- 
 *	Initialize optional read buffer.
 *
 *	The optional read buffer may be used for compressed image file
 *	formats (ex. RLE), where the file has to be read byte by byte.
 *	This option is only available when reading from an image file,
 *	i.e. "image create -file ..."
 *
 *	CAUTION:
 *	- Use this option only, when you do NOT use file seeks.
 *    	- Use tkimg_ReadBuffer (1) to initialize the read buffer before usage.
 *	  Use tkimg_ReadBuffer (0) to switch off the read buffer after usage.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Changes the static variables needed for the read buffer.
 *
 *--------------------------------------------------------------
 */

void tkimg_ReadBuffer (onOff) 
int onOff;
{
    useReadBuf = onOff;
    if (onOff) {
	memset (readBuf, 0, BUFLEN);
	bufStart = -1;
	bufEnd   = -1;
    }
}

/*
 *--------------------------------------------------------------------------
 * tkimg_Read --
 *
 *  This procedure returns a buffer from the stream input. This stream
 *  could be anything from a base-64 encoded string to a Channel.
 *
 * Results:
 *  The number of characters successfully read from the input
 *
 * Side effects:
 *  The tkimg_MFile state could change.
 *--------------------------------------------------------------------------
 */

int
tkimg_Read(handle, dst, count)
    tkimg_MFile *handle;	/* mmdecode "file" handle */
    char *dst;		/* where to put the result */
    int count;		/* number of bytes */
{
    register int i, c;
    int bytesRead, bytesToRead;
    char *dstPtr;

    switch (handle->state) {
      case IMG_STRING:
	if (count > handle->length) {
	    count = handle->length;
	}
	if (count) {
	    memcpy(dst, handle->data, count);
	    handle->length -= count;
	    handle->data += count;
	}
	return count;
      case IMG_CHAN:
	if (!useReadBuf) {
	    return Tcl_Read((Tcl_Channel) handle->data, dst, count);
	}
	dstPtr = dst;
	bytesToRead = count;
	bytesRead = 0;
	while (bytesToRead > 0) {
	    #ifdef DEBUG_LOCAL
		printf ("bytesToRead=%d bytesRead=%d (bufStart=%d bufEnd=%d)\n",
			 bytesToRead, bytesRead, bufStart, bufEnd);
	    #endif
	    if (bufStart < 0) {
		bufEnd = Tcl_Read((Tcl_Channel)handle->data, readBuf, BUFLEN)-1;
		#ifdef DEBUG_LOCAL
		    printf ("Reading new %d bytes into buffer "
                            "(bufStart=%d bufEnd=%d)\n", 
                            BUFLEN, bufStart, bufEnd);
		#endif
		bufStart = 0;
	   	if (bufEnd < 0) 
		    return bufEnd;
	    }
	    if (bufStart + bytesToRead <= bufEnd +1) {
		#ifdef DEBUG_LOCAL
		    printf ("All in buffer: memcpy %d bytes\n", bytesToRead);
		#endif
		/* All bytes already in the buffer. Just copy them to dst. */
		memcpy (dstPtr, readBuf + bufStart, bytesToRead);
		bufStart += bytesToRead;
		if (bufStart > BUFLEN)
		    bufStart = -1;
		return bytesRead + bytesToRead;
	    } else {
		#ifdef DEBUG_LOCAL
		    printf ("Copy rest of buffer: memcpy %d bytes\n",
                            bufEnd+1-bufStart);
		#endif
		memcpy (dstPtr, readBuf + bufStart, bufEnd+1 - bufStart);
		bytesRead += (bufEnd +1 - bufStart);
		bytesToRead -= (bufEnd+1 - bufStart);
		bufStart = -1;
		dstPtr += bytesRead;
	    }
	}
    }

    for(i=0; i<count && (c=tkimg_Getc(handle)) != IMG_DONE; i++) {
	*dst++ = c;
    }
    return i;
}

/*
 *--------------------------------------------------------------------------
 *
 * tkimg_Getc --
 *
 *  This procedure returns the next input byte from a stream. This stream
 *  could be anything from a base-64 encoded string to a Channel.
 *
 * Results:
 *  The next byte (or IMG_DONE) is returned.
 *
 * Side effects:
 *  The tkimg_MFile state could change.
 *
 *--------------------------------------------------------------------------
 */

int
tkimg_Getc(handle)
   tkimg_MFile *handle;			/* Input stream handle */
{
    int c;
    int result = 0;			/* Initialization needed only to prevent
					 * gcc compiler warning */
    if (handle->state == IMG_DONE) {
	return IMG_DONE;
    }

    if (handle->state == IMG_STRING) {
	if (!handle->length--) {
	    handle->state = IMG_DONE;
	    return IMG_DONE;
	}
	return *handle->data++;
    }

    do {
	if (!handle->length--) {
	    handle->state = IMG_DONE;
	    return IMG_DONE;
	}
	c = char64(*handle->data++);
    } while (c == IMG_SPACE);

    if (c > IMG_SPECIAL) {
	handle->state = IMG_DONE;
	return IMG_DONE;
    }

    switch (handle->state++) {
	case 0:
	    handle->c = c<<2;
	    result = tkimg_Getc(handle);
	    break;
	case 1:
	    result = handle->c | (c>>4);
	    handle->c = (c&0xF)<<4;
	    break;
	case 2:
	    result = handle->c | (c>>2);
	    handle->c = (c&0x3)<<6;
	    break;
	case 3:
	    result = handle->c | c;
	    handle->state = 0;
	    break;
    }
    return result;
}

/*
 *-----------------------------------------------------------------------
 * tkimg_Write --
 *
 *  This procedure is invoked to put imaged data into a stream
 *  using tkimg_Putc.
 *
 * Results:
 *  The return value is the number of characters "written"
 *
 * Side effects:
 *  The base64 handle will change state.
 *
 *-----------------------------------------------------------------------
 */

int
tkimg_Write(handle, src, count)
    tkimg_MFile *handle;	/* mmencode "file" handle */
    CONST char *src;	/* where to get the data */
    int count;		/* number of bytes */
{
    register int i;
    int curcount, bufcount;

    if (handle->state == IMG_CHAN) {
	return Tcl_Write((Tcl_Channel) handle->data, (char *) src, count);
    }
    curcount = handle->data - Tcl_DStringValue(handle->buffer);
    bufcount = curcount + count + count/3 + count/52 + 1024;

    /* make sure that the DString contains enough space */
    if (bufcount >= (handle->buffer->spaceAvl)) {
	Tcl_DStringSetLength(handle->buffer, bufcount + 4096);
	handle->data = Tcl_DStringValue(handle->buffer) + curcount;
    }
    /* write the data */
    for (i=0; (i<count) && (tkimg_Putc(*src++, handle) != IMG_DONE); i++) {
	/* empty loop body */
    }
    return i;
}

/*
 *-----------------------------------------------------------------------
 *
 * tkimg_Putc --
 *
 *  This procedure encodes and writes the next byte to a base64
 *  encoded string.
 *
 * Results:
 *  The written byte is returned.
 *
 * Side effects:
 *  the base64 handle will change state.
 *
 *-----------------------------------------------------------------------
 */

static char base64_table[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

int
tkimg_Putc(c, handle)
    register int c;		/* character to be written */
    register tkimg_MFile *handle;	/* handle containing decoder data and state */
{
    /* In fact, here should be checked first if the dynamic
     * string contains enough space for the next character.
     * This would be very expensive to do for each character.
     * Therefore we just allocate 1024 bytes immediately in
     * the beginning and also take a 1024 bytes margin inside
     * every tkimg_Write. At least this check is done then only
     * every 256 bytes, which is much faster. Because the GIF
     * header is less than 1024 bytes and pixel data is
     * written in 256 byte portions, this should be safe.
     */

    if (c == IMG_DONE) {
	switch(handle->state) {
	    case 0:
		break;
	    case 1:
		*handle->data++ = base64_table[(handle->c<<4)&63];
		*handle->data++ = '='; *handle->data++ = '='; break;
	    case 2:
		*handle->data++ = base64_table[(handle->c<<2)&63];
		*handle->data++ = '='; break;
	    default:
		handle->state = IMG_DONE;
		return IMG_DONE;
	}
	Tcl_DStringSetLength(handle->buffer,
		(handle->data) - Tcl_DStringValue(handle->buffer));
	handle->state = IMG_DONE;
	return IMG_DONE;
    }

    if (handle->state == IMG_CHAN) {
	char ch = (char) c;
	return (Tcl_Write((Tcl_Channel) handle->data, &ch, 1)>0) ? c : IMG_DONE;
    }

    c &= 0xff;
    switch (handle->state++) {
	case 0:
	    *handle->data++ = base64_table[(c>>2)&63]; break;
	case 1:
	    c |= handle->c << 8;
	    *handle->data++ = base64_table[(c>>4)&63]; break;
	case 2:
	    handle->state = 0;
	    c |= handle->c << 8;
	    *handle->data++ = base64_table[(c>>6)&63];
	    *handle->data++ = base64_table[c&63]; break;
    }
    handle->c = c;
    if (handle->length++ > 52) {
	handle->length = 0;
	*handle->data++ = '\n';
    }
    return c & 0xff;
};

/*
 *-------------------------------------------------------------------------
 * tkimg_WriteInit --
 *  This procedure initializes a base64 decoder handle for writing
 *
 * Results:
 *  none
 *
 * Side effects:
 *  the base64 handle is initialized
 *
 *-------------------------------------------------------------------------
 */

void
tkimg_WriteInit(buffer, handle)
    Tcl_DString *buffer;
    tkimg_MFile *handle;		/* mmencode "file" handle */
{
    Tcl_DStringSetLength(buffer, buffer->spaceAvl);
    handle->buffer = buffer;
    handle->data = Tcl_DStringValue(buffer);
    handle->state = 0;
    handle->length = 0;
}

/*
 *-------------------------------------------------------------------------
 * tkimg_ReadInit --
 *  This procedure initializes a base64 decoder handle for reading.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  the base64 handle is initialized
 *
 *-------------------------------------------------------------------------
 */

int
tkimg_ReadInit(data, c, handle)
    Tcl_Obj *data;		/* string containing initial mmencoded data */
    int c;
    tkimg_MFile *handle;		/* mmdecode "file" handle */
{
    handle->data = tkimg_GetByteArrayFromObj(data, &handle->length);
    if (*handle->data == c) {
	handle->state = IMG_STRING;
	return 1;
    }
    c = base64_table[(c>>2)&63];

    while((handle->length) && (char64(*handle->data) == IMG_SPACE)) {
	handle->data++;
	handle->length--;
    }
    if (c != *handle->data) {
	handle->state = IMG_DONE;
	return 0;
    }
    handle->state = 0;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * tkimg_OpenFileChannel --
 *
 *	Open a file channel in binary mode. If permissions is 0, the
 *	file will be opened in read mode, otherwise in write mode.
 *
 * Results:
 *	The same as Tcl_OpenFileChannel, only the file will
 *	always be opened in binary mode without encoding.
 *
 * Side effects:
 *	If function fails, an error message will be left in the
 *	interpreter.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
tkimg_OpenFileChannel(interp, fileName, permissions)
    Tcl_Interp *interp;
    CONST char *fileName;
    int permissions;
{
    Tcl_Channel chan = Tcl_OpenFileChannel(interp, (char *) fileName,
	    permissions?"w":"r", permissions);
    if (!chan) {
	return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(interp, chan, "-buffersize", "131072") != TCL_OK) {        Tcl_Close(interp, chan);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK) {
	Tcl_Close(interp, chan);
	return (Tcl_Channel) NULL;
    }
    return chan;
}

