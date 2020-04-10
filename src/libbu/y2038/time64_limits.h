/*
Copyright (c) 2007-2010  Michael G Schwern

The MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/*
   Maximum and minimum inputs your system's respective time functions
   can correctly handle.  time64.h will use your system functions if
   the input falls inside these ranges and corresponding USE_SYSTEM_*
   constant is defined.
*/

#ifndef TIME64_LIMITS_H
#define TIME64_LIMITS_H

/* Max/min for localtime() */
#define SYSTEM_LOCALTIME_MAX     2147483647
#define SYSTEM_LOCALTIME_MIN    -2147483647-1

/* Max/min for gmtime() */
#define SYSTEM_GMTIME_MAX        2147483647
#define SYSTEM_GMTIME_MIN       -2147483647-1

/* MODIFIED: Moved max/min for mktime() to time64.c to avoid having to
   initialize it with system-dependent field ordering.
*/

/* MODIFIED: Removed SYSTEM_TIMEGM_MAX/MIN since they are not used
   anywhere
*/

#endif /* TIME64_LIMITS_H */
