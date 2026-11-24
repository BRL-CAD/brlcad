#ifndef ALPHANUM_H
#define ALPHANUM_H

/*
   The Alphanum Algorithm is an improved sorting algorithm for strings
   containing numbers.  Instead of sorting numbers in ASCII order like a
   standard sort, this algorithm sorts numbers in numeric order.

   The Alphanum Algorithm is discussed at http://www.DaveKoelle.com

   This implementation is Copyright (c) 2008 Dirk Jagdmann <doj@cubic.org>.
   It is a cleanroom implementation of the algorithm and not derived by
   other's works. In contrast to the versions written by Dave Koelle this
   source code is distributed with the libpng/zlib license.

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
   must not claim that you wrote the original software. If you use
   this software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
   must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.

Source:
https://github.com/facontidavide/PlotJuggler/blob/master/include/PlotJuggler/alphanum.hpp

This copy has been simplified down to the essential sorting comparison of C strings

*/

#include "common.h"
#include <ctype.h>
#include <string.h>

#include "bu/str.h"

/**
  compare l and r with C string comparison semantics, but using
  the "Alphanum Algorithm". This function is designed to read
  through the l and r strings only one time, for
  maximum performance. It does not allocate memory for
  substrings.

  @param l NULL-terminated C-style string
  @param r NULL-terminated C-style string
  @return negative if l<r, 0 if l equals r, positive if l>r
  */
static int alphanum_impl(const char *l, const char *r, void *UNUSED(arg))
{
    enum alphanum_mode_t { STRING, NUMBER } mode=STRING;
    const char *original_l = l;
    const char *original_r = r;

    while(*l && *r)
    {
	if(mode == STRING)
	{
	    char l_char, r_char;
	    while((l_char=*l) && (r_char=*r))
	    {
		// Check if this are digit characters.  Note that the unsigned
		// char cast will not handle non-ASCII characters "correctly"
		// in the sense of producing a local aware decision - we do
		// this for simplicity given most candidate inputs will be
		// ASCII, and we don't want to produce some form of sorting
		// decision the (relatively rare) cases where we hit a
		// non-ASCII character in the input string.
		const int l_digit = isdigit((unsigned char)l_char), r_digit=isdigit((unsigned char)r_char);
		// if both characters are digits, we continue in NUMBER mode
		if(l_digit && r_digit)
		{
		    mode=NUMBER;
		    break;
		}
		// if only the left character is a digit, we have a result
		if(l_digit) return -1;
		// if only the right character is a digit, we have a result
		if(r_digit) return +1;
		// compute the difference of both characters
		const int diff = l_char - r_char;
		// if they differ we have a result
		if(diff != 0) return diff;
		// otherwise process the next characters
		++l;
		++r;
	    }
	}
	else // mode==NUMBER
	{
	    const char *lend = l;
	    const char *rend = r;
	    const char *lsig = l;
	    const char *rsig = r;
	    while (isdigit((unsigned char)*lend)) lend++;
	    while (isdigit((unsigned char)*rend)) rend++;
	    while (lsig < lend && *lsig == '0') lsig++;
	    while (rsig < rend && *rsig == '0') rsig++;

	    /* Compare decimal digit runs directly.  Conversion through a machine
	     * integer misorders values beyond ULONG_MAX and base-zero conversion
	     * gives leading zeroes unintended octal semantics. */
	    size_t llen = (size_t)(lend - lsig);
	    size_t rlen = (size_t)(rend - rsig);
	    if (llen < rlen) return -1;
	    if (llen > rlen) return 1;
	    if (llen) {
		int diff = memcmp(lsig, rsig, llen);
		if (diff) return diff;
	    }
	    l=lend;
	    r=rend;

	    // otherwise we process the next substring in STRING mode
	    mode=STRING;
	}
    }

    if(*r) return -1;
    if(*l) return +1;
    /* Numerically equivalent runs may have different spellings (for example,
     * "1" and "01").  A lexical tiebreak makes this a total order, keeps
     * bounded heaps and full sorts consistent, and fulfills the documented
     * zero-only-for-equal contract. */
    return bu_strcmp(original_l, original_r);
}
#endif //ALPHANUM_H

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
