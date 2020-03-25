/* ==========================================================================
 * hexdump.h - hexdump.c
 * --------------------------------------------------------------------------
 * Copyright (c) 2013  William Ahern
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ==========================================================================
 */
#ifndef HEXDUMP_H
#define HEXDUMP_H


/*
 * H E X D U M P  V E R S I O N  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define HXD_VERSION HXD_V_REL
#define HXD_VENDOR "william@25thandClement.com"

#define HXD_V_REL 0x20181221
#define HXD_V_ABI 0x20130210
#define HXD_V_API 0x20130412

int hxd_version(void);
const char *hxd_vendor(void);

int hxd_v_rel(void);
int hxd_v_abi(void);
int hxd_v_api(void);


/*
 * H E X D U M P  E R R O R  I N T E R F A C E S
 *
 * Hexdump internal error conditions are returned using regular int objects.
 * System errors are loaded from errno as soon as encountered, and the value
 * returned through the API like internal errors. DO NOT check errno, which
 * may have been overwritten by subsequent error handling code. Internal
 * errors are negative and utilize a simple high-order-byte namespacing
 * protocol. This works because ISO C and POSIX guarantee that all system
 * error codes are positive.
 *
 * hxd_strerror() will forward system errors to strerror(3).
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define HXD_EBASE -(('D' << 24) | ('U' << 16) | ('M' << 8) | 'P')
#define HXD_ERROR(error) ((error) >= XD_EBASE && (error) < XD_ELAST)


enum hxd_errors {
	HXD_EFORMAT = HXD_EBASE,
	/* a compile-time error signaling an invalid format string, format
	   unit, or conversion specification syntax */

	HXD_EDRAINED,
	/* a compile-time error signaling that preceding conversions have
	   already drained the input block */

	HXD_ENOTSUPP,
	/* a compile-time error returned for valid but unsupported
	   conversion specifications */

	HXD_EOOPS,
	/* something horrible happened */

	HXD_ELAST
}; /* enum hxd_errors */

#define hxd_error_t int /* for documentation purposes only */

const char *hxd_strerror(hxd_error_t);


/*
 * H E X D U M P  C O R E  I N T E R F A C E S
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct hexdump;

struct hexdump *hxd_open(hxd_error_t *);

void hxd_close(struct hexdump *);

void hxd_reset(struct hexdump *);

#define HXD_BYTEORDER(x)  (0x03 & (x))
#define HXD_NATIVE         0x00
#define HXD_NETWORK        HXD_BIG_ENDIAN
#define HXD_BIG_ENDIAN     0x01
#define HXD_LITTLE_ENDIAN  0x02
#define HXD_NOPADDING      0x04

hxd_error_t hxd_compile(struct hexdump *, const char *, int);

const char *hxd_help(struct hexdump *);

size_t hxd_blocksize(struct hexdump *);

hxd_error_t hxd_write(struct hexdump *, const void *, size_t);

hxd_error_t hxd_flush(struct hexdump *);

size_t hxd_read(struct hexdump *, void *, size_t);


/*
 * H E X D U M P  C O M M O N  F O R M A T S
 *
 * Predefined formats for hexdump(1) -b, -c, -C, -d, -o, -x, and xxd(1) -i.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define HEXDUMP_b "\"%07.7_ax \" 16/1 \"%03o \" \"\\n\""
#define HEXDUMP_c "\"%07.7_ax \" 16/1 \"%3_c \" \"\\n\""
#define HEXDUMP_C "\"%08.8_ax  \" 8/1 \"%02x \" \"  \" 8/1 \"%02x \"\n" \
                  "\"  |\" 16/1 \"%_p\" \"|\\n\""
#define HEXDUMP_d "\"%07.7_ax \" 8/2 \"  %05u \" \"\\n\""
#define HEXDUMP_o "\"%07.7_ao   \" 8/2 \" %06o \" \"\\n\""
#define HEXDUMP_x "\"%07.7_ax \" 8/2 \"   %04x \" \"\\n\""

#define HEXDUMP_i "\"  \" 12/1? \"0x%02x, \" \"\\n\""


/*
 * H E X D U M P  L U A  I N T E R F A C E S
 *
 * When built with -DHEXDUMP_LUALIB then luaopen_hexdump() will return a
 * Lua module table:
 *
 *   local hexdump = require"hexdump"
 *
 *   hexdump.NATIVE 
 *   hexdump.NETWORK
 *   hexdump.BIG_ENDIAN
 *   hexdump.LITTLE_ENDIAN
 *     Bitwise flags which configure word byte order. The default is the
 *     native byte order.
 *
 *   hexdump.NOPADDING
 *     Bitwise flag which disables padding; instead, formatting units are
 *     skipped entirely when the block buffer is too short.
 *
 *   hexdump.b 
 *   hexdump.c
 *   hexdump.C
 *   hexdump.d
 *   hexdump.o
 *   hexdump.x
 *     Predefined format strings of the hexdump(1) options -x, -c, -C, -d,
 *     -o, and -x, respectively.
 *
 *   hexdump.new()
 *     Returns new context, just like hxd_open.
 *
 *   hexdump.apply(fmt:string, [flags:int,] data:string, ...)
 *     Returns a formatted string, memoizing the context object for later
 *     reuse with the same format.
 *
 * The module table also has a __call metamethod, which forwards to .apply.
 * This allows doing require"hexdump"('/1 "%.2x"', "0123456789").
 *
 * Every context is a simple object with methods identical to the C library,
 * including:
 *
 *   :reset()
 *     Resets the internal buffers. Returns true.
 *
 *   :compile(fmt:string)
 *     Parses and compiles the format string according to the rules of BSD
 *     hexdump(1). Returns true on success, or throws an error on failure.
 *
 *   :blocksize()
 *     Returns the block size of any compiled format string.
 *
 *   :write(data:string)
 *     Processes the data string. The string DOES NOT have to be the same
 *     length as the block size. It can be any size, although the formatted
 *     output is buffered until drained with :read, so it's better to write
 *     smallish chunks when processing large files. Returns true on success,
 *     or throws an error on failure.
 *
 *   :flush()
 *     Processes any data (less than the block size) in the input buffer
 *     as-if EOF was received. Returns true on success, or throws an error
 *     on failure.
 *
 *   :read()
 *     Drains and returns the output buffer as a string.
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int luaopen_hexdump(/* pointer to lua_State */);


#endif /* HEXDUMP_H */
