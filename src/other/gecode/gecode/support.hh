/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2007
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

#ifndef __GECODE_SUPPORT_HH__
#define __GECODE_SUPPORT_HH__

#include <cassert>

#include <gecode/support/config.hpp>

/*
 * Linking and compiler workarounds
 *
 */
#if !defined(GECODE_STATIC_LIBS) && \
    (defined(__CYGWIN__) || defined(__MINGW32__) || defined(_MSC_VER))

/**
  * \brief Workaround for a bug in the Microsoft C++ compiler
  *
  * Details for the bug can be found at
  * http://support.microsoft.com/?scid=kb%3Ben-us%3B122675&x=9&y=13
  */
#define GECODE_MSC_VIRTUAL virtual

#ifdef GECODE_BUILD_SUPPORT
#define GECODE_SUPPORT_EXPORT __declspec( dllexport )
#else
#define GECODE_SUPPORT_EXPORT __declspec( dllimport )
#endif
#define GECODE_VTABLE_EXPORT

#else

#define GECODE_MSC_VIRTUAL

#ifdef GECODE_GCC_HAS_CLASS_VISIBILITY
#define GECODE_SUPPORT_EXPORT __attribute__ ((visibility("default")))
#define GECODE_VTABLE_EXPORT __attribute__ ((visibility("default")))
#else
#define GECODE_SUPPORT_EXPORT
#define GECODE_VTABLE_EXPORT
#endif

#endif

// Configure auto-linking
#ifndef GECODE_BUILD_SUPPORT
#define GECODE_LIBRARY_NAME "Support"
#include <gecode/support/auto-link.hpp>
#endif

// Configure threads
#ifdef GECODE_THREADS_WINDOWS
#define GECODE_HAS_THREADS
#endif

#ifdef GECODE_THREADS_PTHREADS
#define GECODE_HAS_THREADS
#endif


/*
 * Basic support needed everywhere
 *
 */

#include <gecode/support/macros.hpp>
#include <gecode/support/exception.hpp>
#include <gecode/support/cast.hpp>
#include <gecode/support/thread.hpp>
#include <gecode/support/heap.hpp>
#include <gecode/support/marked-pointer.hpp>
#include <gecode/support/int-type.hpp>

/*
 * Common datastructures and algorithms
 *
 */

#include <gecode/support/bitset-base.hpp>
#include <gecode/support/bitset.hpp>
#include <gecode/support/bitset-offset.hpp>
#include <gecode/support/block-allocator.hpp>
#include <gecode/support/dynamic-array.hpp>
#include <gecode/support/dynamic-queue.hpp>
#include <gecode/support/dynamic-stack.hpp>
#include <gecode/support/random.hpp>
#include <gecode/support/sort.hpp>
#include <gecode/support/static-stack.hpp>


/*
 * Operating system support
 *
 */

#ifdef GECODE_THREADS_WINDOWS
#include <gecode/support/thread/windows.hpp>
#endif
#ifdef GECODE_THREADS_PTHREADS
#include <gecode/support/thread/pthreads.hpp>
#endif
#ifndef GECODE_HAS_THREADS
#include <gecode/support/thread/none.hpp>
#endif

#include <gecode/support/thread/thread.hpp>

#include <gecode/support/timer.hpp>
#include <gecode/support/hw-rnd.hpp>

#endif

// STATISTICS: support-any
