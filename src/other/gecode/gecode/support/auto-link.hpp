/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2008
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * Define auto-linking for MSVC
 *
 */

#if defined(_MSC_VER)

#if defined(_M_IX86)
#define GECODE_DLL_PLATFORM "x86"
#elif defined(_M_IA64)
#define GECODE_DLL_PLATFORM "ia64"
#elif defined(_M_X64)
#define GECODE_DLL_PLATFORM "x64"
#else
#error Unsupported platform.
#endif

#if defined(_DEBUG)
#define GECODE_DLL_RELEASE "d"
#else
#define GECODE_DLL_RELEASE "r"
#endif

#pragma comment(lib, \
  GECODE_DLL_USERPREFIX "Gecode" GECODE_LIBRARY_NAME\
  "-" GECODE_LIBRARY_VERSION \
  "-" GECODE_DLL_RELEASE "-" GECODE_DLL_PLATFORM GECODE_DLL_USERSUFFIX)

#undef GECODE_DLL_PLATFORM
#undef GECODE_DLL_RELEASE

#endif

#undef GECODE_LIBRARY_NAME

// STATISTICS: support-any
