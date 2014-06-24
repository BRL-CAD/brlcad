/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2005
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

#ifndef __GECODE_ITER_HH__
#define __GECODE_ITER_HH__

#include <gecode/kernel.hh>

namespace Gecode {
  /// Range and value iterators
  namespace Iter {
    /// Range iterators
    namespace Ranges {}
    /// Value iterators
    namespace Values {}
  }
}

#include <gecode/iter/ranges-operations.hpp>
#include <gecode/iter/ranges-minmax.hpp>
#include <gecode/iter/ranges-list.hpp>

#include <gecode/iter/ranges-append.hpp>
#include <gecode/iter/ranges-array.hpp>
#include <gecode/iter/ranges-cache.hpp>
#include <gecode/iter/ranges-compl.hpp>
#include <gecode/iter/ranges-diff.hpp>
#include <gecode/iter/ranges-empty.hpp>
#include <gecode/iter/ranges-inter.hpp>
#include <gecode/iter/ranges-map.hpp>
#include <gecode/iter/ranges-minus.hpp>
#include <gecode/iter/ranges-negative.hpp>
#include <gecode/iter/ranges-offset.hpp>
#include <gecode/iter/ranges-positive.hpp>
#include <gecode/iter/ranges-scale.hpp>
#include <gecode/iter/ranges-singleton.hpp>
#include <gecode/iter/ranges-union.hpp>
#include <gecode/iter/ranges-values.hpp>
#include <gecode/iter/ranges-add.hpp>
#include <gecode/iter/ranges-singleton-append.hpp>
#include <gecode/iter/ranges-rangelist.hpp>

#include <gecode/iter/values-array.hpp>
#include <gecode/iter/values-bitset.hpp>
#include <gecode/iter/values-bitsetoffset.hpp>
#include <gecode/iter/values-list.hpp>

#include <gecode/iter/values-inter.hpp>
#include <gecode/iter/values-map.hpp>
#include <gecode/iter/values-minus.hpp>
#include <gecode/iter/values-negative.hpp>
#include <gecode/iter/values-offset.hpp>
#include <gecode/iter/values-positive.hpp>
#include <gecode/iter/values-ranges.hpp>
#include <gecode/iter/values-singleton.hpp>
#include <gecode/iter/values-union.hpp>
#include <gecode/iter/values-unique.hpp>

#include <gecode/iter/ranges-size.hpp>

#endif

// STATISTICS: iter-any
