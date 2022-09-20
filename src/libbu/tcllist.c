/* The Tcl ASCII list format is used for text representations of many BRL-CAD data types.
 * The code below lets libbu handle splitting a string using that list format into an
 * argc/argv array.  Code is based off of the Tcl code that performs these functions, and
 * has the following license:
 *
 * Copyright (c) 1994-2018 Regents of the University of
 * California, Sun Microsystems, Inc., Scriptics Corporation, ActiveState
 * Corporation and other parties.  The following terms apply to all files
 * associated with the software unless explicitly disclaimed in
 * individual files.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense, the
 * software shall be classified as "Commercial Computer Software" and the
 * Government shall have only "Restricted Rights" as defined in Clause
 * 252.227-7013 (b) (3) of DFARs.  Notwithstanding the foregoing, the
 * authors grant the U.S. Government and others acting in its behalf
 * permission to use and distribute the software in accordance with the
 * terms specified in this license.
 */

#include "common.h"

#include <string.h>
#include <ctype.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/file.h"
#include "bu/exit.h"

static int
_bu_tcl_utf_to_ushort(
    register const char *src,   /* The UTF-8 string. */
    register unsigned short *chPtr)/* Filled with the unsigned short represented by
				    * the UTF-8 string. */
{
    register int byte;

    /*
     * Unroll 1 to 3 byte UTF-8 sequences, use loop to handle longer ones.
     */

    byte = *((unsigned char *) src);
    if (byte < 0xC0) {
	/*
	 * Handles properly formed UTF-8 characters between 0x01 and 0x7F.
	 * Also treats \0 and naked trail bytes 0x80 to 0xBF as valid
	 * characters representing themselves.
	 */

	*chPtr = (unsigned short) byte;
	return 1;
    } else if (byte < 0xE0) {
	if ((src[1] & 0xC0) == 0x80) {
	    /*
	     * Two-byte-character lead-byte followed by a trail-byte.
	     */

	    *chPtr = (unsigned short) (((byte & 0x1F) << 6) | (src[1] & 0x3F));
	    return 2;
	}

	/*
	 * A two-byte-character lead-byte not followed by trail-byte
	 * represents itself.
	 */

	*chPtr = (unsigned short) byte;
	return 1;
    } else if (byte < 0xF0) {
	if (((src[1] & 0xC0) == 0x80) && ((src[2] & 0xC0) == 0x80)) {
	    /*
	     * Three-byte-character lead byte followed by two trail bytes.
	     */

	    *chPtr = (unsigned short) (((byte & 0x0F) << 12)
				       | ((src[1] & 0x3F) << 6) | (src[2] & 0x3F));
	    return 3;
	}

	/*
	 * A three-byte-character lead-byte not followed by two trail-bytes
	 * represents itself.
	 */

	*chPtr = (unsigned short) byte;
	return 1;
    }

    *chPtr = (unsigned short) byte;
    return 1;
}

static int
_bu_tcl_ushort_to_utf(
    int ch,                     /* The unsigned short to be stored in the
				 * buffer. */
    char *buf)                  /* Buffer in which the UTF-8 representation of
				 * the unsigned short is stored. Buffer must be
				 * large enough to hold the UTF-8 character
				 * (at most 3 bytes). */
{
    if ((ch > 0) && (ch < 0x80)) {
	buf[0] = (char) ch;
	return 1;
    }
    if (ch >= 0) {
	if (ch <= 0x7FF) {
	    buf[1] = (char) ((ch | 0x80) & 0xBF);
	    buf[0] = (char) ((ch >> 6) | 0xC0);
	    return 2;
	}
	if (ch <= 0xFFFF) {
	three:
	    buf[2] = (char) ((ch | 0x80) & 0xBF);
	    buf[1] = (char) (((ch >> 6) | 0x80) & 0xBF);
	    buf[0] = (char) ((ch >> 12) | 0xE0);
	    return 3;
	}

    }

    ch = 0xFFFD;
    goto three;
}

static int
_bu_tcl_parse_hex(
    const char *src,            /* First character to parse. */
    int numBytes,               /* Max number of byes to scan */
    unsigned short *resultPtr)     /* Points to storage provided by caller where
				    * the unsigned short resulting from the
				    * conversion is to be written. */
{
    unsigned short result = 0;
    register const char *p = src;

    while (numBytes--) {
	unsigned char digit = (unsigned char)(*p);

	if (!isxdigit(digit)) {
	    break;
	}

	++p;
	result <<= 4;

	if (digit >= 'a') {
	    result |= (10 + digit - 'a');
	} else if (digit >= 'A') {
	    result |= (10 + digit - 'A');
	} else {
	    result |= (digit - '0');
	}
    }

    *resultPtr = result;
    return (p - src);
}

static int
_bu_tcl_parse_backslash(
    const char *src,            /* Points to the backslash character of a a
				 * backslash sequence. */
    int numBytes,               /* Max number of bytes to scan. */
    int *readPtr,               /* NULL, or points to storage where the number
				 * of bytes scanned should be written. */
    char *dst)                  /* NULL, or points to buffer where the UTF-8
				 * encoding of the backslash sequence is to be
				 * written. At most 3 bytes will be
				 * written there. */
{
    register const char *p = src+1;
    unsigned short result;
    int count;
    int ch;
    char buf[3];
    const unsigned char utf_byte_cnts[256] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3};

    if (numBytes == 0) {
	if (readPtr != NULL) {
	    *readPtr = 0;
	}
	return 0;
    }

    if (dst == NULL) {
	dst = buf;
    }

    if (numBytes == 1) {
	/*
	 * Can only scan the backslash, so return it.
	 */

	result = '\\';
	count = 1;
	goto done;
    }

    count = 2;
    switch (*p) {
	/*
	 * Note: in the conversions below, use absolute values (e.g., 0xa)
	 * rather than symbolic values (e.g. \n) that get converted by the
	 * compiler. It's possible that compilers on some platforms will do
	 * the symbolic conversions differently, which could result in
	 * non-portable Tcl scripts.
	 */

	case 'a':
	    result = 0x7;
	    break;
	case 'b':
	    result = 0x8;
	    break;
	case 'f':
	    result = 0xc;
	    break;
	case 'n':
	    result = 0xa;
	    break;
	case 'r':
	    result = 0xd;
	    break;
	case 't':
	    result = 0x9;
	    break;
	case 'v':
	    result = 0xb;
	    break;
	case 'x':
	    count += _bu_tcl_parse_hex(p+1, numBytes-1, &result);
	    if (count == 2) {
		/*
		 * No hexadigits -> This is just "x".
		 */

		result = 'x';
	    } else {
		/*
		 * Keep only the last byte (2 hex digits).
		 */
		result = (unsigned char) result;
	    }
	    break;
	case 'u':
	    count += _bu_tcl_parse_hex(p+1, (numBytes > 5) ? 4 : numBytes-1, &result);
	    if (count == 2) {
		/*
		 * No hexadigits -> This is just "u".
		 */
		result = 'u';
	    }
	    break;
	case '\n':
	    count--;
	    do {
		p++;
		count++;
	    } while ((count < numBytes) && ((*p == ' ') || (*p == '\t')));
	    result = ' ';
	    break;
	case 0:
	    result = '\\';
	    count = 1;
	    break;
	default:
	    /*
	     * Check for an octal number \oo?o?
	     */

	    if (isdigit((unsigned char)(*p)) && ((unsigned char)(*p) < '8')) {  /* INTL: digit */
		result = (unsigned char)(*p - '0');
		p++;
		if ((numBytes == 2) || !isdigit((unsigned char)(*p))  /* INTL: digit */
		    || ((unsigned char)(*p) >= '8')) {
		    break;
		}
		count = 3;
		result = (unsigned char)((result << 3) + (*p - '0'));
		p++;
		if ((numBytes == 3) || !isdigit((unsigned char)(*p))  /* INTL: digit */
		    || ((unsigned char)(*p) >= '8')) {
		    break;
		}
		count = 4;
		result = (unsigned char)((result << 3) + (*p - '0'));
		break;
	    }

	    /*
	     * We have to convert here in case the user has put a backslash in
	     * front of a multi-byte utf-8 character. While this means nothing
	     * special, we shouldn't break up a correct utf-8 character. [Bug
	     * #217987] test subst-3.2
	     */

	    ch = *((unsigned char *) p);
	    if (numBytes - 1 >= utf_byte_cnts[ch]) {
		count = _bu_tcl_utf_to_ushort(p, &result) + 1;   /* +1 for '\' */
	    } else {
		char utfBytes[3];

		memcpy(utfBytes, p, (size_t) (numBytes - 1));
		utfBytes[numBytes - 1] = '\0';
		count = _bu_tcl_utf_to_ushort(utfBytes, &result) + 1;
	    }
	    break;
    }

done:
    if (readPtr != NULL) {
	*readPtr = count;
    }
    return _bu_tcl_ushort_to_utf((int) result, dst);
}


static int
_bu_tcl_find_element(
    const char *list,           /* Points to the first byte of a string
				 * containing a Tcl list with zero or more
				 * elements (possibly in braces). */
    int listLength,             /* Number of bytes in the list's string. */
    const char **elementPtr,    /* Where to put address of first significant
				 * character in first element of list. */
    const char **nextPtr,       /* Fill in with location of character just
				 * after all white space following end of
				 * argument (next arg or end of list). */
    int *sizePtr,               /* If non-zero, fill in with size of
				 * element. */
    int *bracePtr)              /* If non-zero, fill in with non-zero/zero to
				 * indicate that arg was/wasn't in braces. */
{
    const char *p = list;
    const char *elemStart;      /* Points to first byte of first element. */
    const char *limit;          /* Points just after list's last byte. */
    int openBraces = 0;         /* Brace nesting level during parse. */
    int inQuotes = 0;
    int size = 0;               /* lint. */
    int numChars;

    /*
     * Skim off leading white space and check for an opening brace or quote.
     * We treat embedded NULLs in the list as bytes belonging to a list
     * element.
     */

    if (bracePtr != NULL) {
	*bracePtr = 0;
    }

    limit = (list + listLength);
    while ((p < limit) && (isspace((unsigned char)(*p)))) { /* INTL: ISO space. */
	p++;
    }
    if (p == limit) {           /* no element found */
	elemStart = limit;
	goto done;
    }

    if (*p == '{') {
	openBraces = 1;
	p++;
    } else if (*p == '"') {
	inQuotes = 1;
	p++;
    }
    elemStart = p;
    if (bracePtr != NULL) {
	*bracePtr = openBraces;
    }

    /*
     * Find element's end (a space, close brace, or the end of the string).
     */

    while (p < limit) {
	switch (*p) {
	    /*
	     * Open brace: don't treat specially unless the element is in
	     * braces. In this case, keep a nesting count.
	     */

	    case '{':
		if (openBraces != 0) {
		    openBraces++;
		}
		break;

		/*
		 * Close brace: if element is in braces, keep nesting count and
		 * quit when the last close brace is seen.
		 */

	    case '}':
		if (openBraces > 1) {
		    openBraces--;
		} else if (openBraces == 1) {
		    size = (p - elemStart);
		    p++;
		    if ((p >= limit)
			|| isspace((unsigned char)(*p))) {        /* INTL: ISO space. */
			goto done;
		    }

		    /*
		     * Garbage after the closing brace; return an error.
		     */
		    return 1;
		}
		break;

		/*
		 * Backslash: skip over everything up to the end of the backslash
		 * sequence.
		 */

	    case '\\':
		(void)_bu_tcl_parse_backslash(p, (int)strlen(p), &numChars, NULL);
		p += (numChars - 1);
		break;

		/*
		 * Space: ignore if element is in braces or quotes; otherwise
		 * terminate element.
		 */

	    case ' ':
	    case '\f':
	    case '\n':
	    case '\r':
	    case '\t':
	    case '\v':
		if ((openBraces == 0) && !inQuotes) {
		    size = (p - elemStart);
		    goto done;
		}
		break;

		/*
		 * Double-quote: if element is in quotes then terminate it.
		 */

	    case '"':
		if (inQuotes) {
		    size = (p - elemStart);
		    p++;
		    if ((p >= limit)
			|| isspace((unsigned char)(*p))) {        /* INTL: ISO space */
			goto done;
		    }

		    /*
		     * Garbage after the closing quote; return an error.
		     */
		    return 1;
		}
		break;
	}
	p++;
    }

    /*
     * End of list: terminate element.
     */
    if (p == limit) {
	if (openBraces != 0) {
	    return 1;
	} else if (inQuotes) {
	    return 1;
	}
	size = (p - elemStart);
    }

done:
    while ((p < limit) && (isspace((unsigned char)(*p)))) { /* INTL: ISO space. */
	p++;
    }
    *elementPtr = elemStart;
    *nextPtr = p;
    if (sizePtr != 0) {
	*sizePtr = size;
    }
    return 0;
}


static int
_bu_tcl_copy_collapse(
    int count,                  /* Number of characters to copy from src. */
    const char *src,            /* Copy from here... */
    char *dst)                  /* ... to here. */
{
    register char c;
    int numRead;
    int newCount = 0;
    int backslashCount;

    for (c = *src;  count > 0;  src++, c = *src, count--) {
	if (c == '\\') {
	    backslashCount = _bu_tcl_parse_backslash(src, (int)strlen(src), &numRead, dst);
	    dst += backslashCount;
	    newCount += backslashCount;
	    src += numRead-1;
	    count -= numRead-1;
	} else {
	    *dst = c;
	    dst++;
	    newCount++;
	}
    }
    *dst = 0;
    return newCount;
}

int
bu_argv_from_tcl_list(const char *list_str, int *argc, const char ***argv)
{
    const char **largv, *l, *element;
    char *p;
    int length, size, i, result, elSize, brace;

    /*
     * Figure out how much space to allocate. There must be enough space for
     * both the array of pointers and also for a copy of the list. To estimate
     * the number of pointers needed, count the number of space characters in
     * the list.
     */

    for (size = 2, l = list_str; *l != 0; l++) {
	if (isspace((unsigned char)(*l))) {
	    size++;

	    /*
	     * Consecutive space can only count as a single list delimiter.
	     */

	    while (1) {
		char next = *(l + 1);

		if (next == '\0') {
		    break;
		}
		++l;
		if (isspace((unsigned char)(next))) {
		    continue;
		}
		break;
	    }
	}
    }

    length = l - list_str;
    largv = (const char **)bu_calloc(size + length + 1, sizeof(char *), "bu_tcl_list largv");
    for (i = 0, p = ((char *) largv) + size*sizeof(char *); *list_str != 0;  i++) {
	const char *prevList = list_str;

	result = _bu_tcl_find_element(list_str, length, &element, &list_str, &elSize, &brace);
	length -= (list_str - prevList);
	if (result != 0) {
	    bu_free((char *) largv, "free argv");
	    return result;
	}
	if (*element == 0) {
	    break;
	}
	if (i >= size) {
	    bu_free((char *) largv, "free argv");
	    return 1;
	}
	largv[i] = p;
	if (brace) {
	    memcpy(p, element, (size_t) elSize);
	    p += elSize;
	    *p = 0;
	    p++;
	} else {
	    _bu_tcl_copy_collapse(elSize, element, p);
	    p += elSize+1;
	}
    }

    largv[i] = NULL;
    *argv = largv;
    *argc = i;
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
