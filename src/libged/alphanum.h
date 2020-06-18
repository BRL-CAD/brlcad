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
#include <stdlib.h>
#include <ctype.h>

/**
  compare l and r with C string comparison semantics, but using
  the "Alphanum Algorithm". This function is designed to read
  through the l and r strings only one time, for
  maximum performance. It does not allocate memory for
  substrings. It can either use the C-library functions isdigit()
  and atoi() to honour your locale settings, when recognizing
  digit characters when you "#define ALPHANUM_LOCALE=1" or use
  it's own digit character handling which only works with ASCII
  digit characters, but provides better performance.

  @param l NULL-terminated C-style string
  @param r NULL-terminated C-style string
  @return negative if l<r, 0 if l equals r, positive if l>r
  */
int alphanum_impl(const char *l, const char *r, void *UNUSED(arg))
{
    enum alphanum_mode_t { STRING, NUMBER } mode=STRING;

    while(*l && *r)
    {
	if(mode == STRING)
	{
	    char l_char, r_char;
	    while((l_char=*l) && (r_char=*r))
	    {
		// check if this are digit characters
		const int l_digit = isdigit(l_char), r_digit=isdigit(r_char);
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
	    // get the left number
	    char *end;
	    unsigned long l_int=strtoul(l, &end, 0);
	    l=end;

	    // get the right number
	    unsigned long r_int=strtoul(r, &end, 0);
	    r=end;

	    // if the difference is not equal to zero, we have a comparison result
	    const long diff=l_int-r_int;
	    if(diff != 0)
		return diff;

	    // otherwise we process the next substring in STRING mode
	    mode=STRING;
	}
    }

    if(*r) return -1;
    if(*l) return +1;
    return 0;
}

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
