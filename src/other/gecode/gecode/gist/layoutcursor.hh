/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef GECODE_GIST_LAYOUTCURSOR_HH
#define GECODE_GIST_LAYOUTCURSOR_HH

#include <gecode/gist/nodecursor.hh>
#include <gecode/gist/visualnode.hh>

namespace Gecode { namespace Gist {


  /// \brief A cursor that computes a tree layout for VisualNodes
  class LayoutCursor : public NodeCursor<VisualNode> {
  public:
    /// Constructor
    LayoutCursor(VisualNode* theNode,
                 const VisualNode::NodeAllocator& na);

    /// \name Cursor interface
    //@{
    /// Test if the cursor may move to the first child node
    bool mayMoveDownwards(void);
    /// Compute layout for current node
    void processCurrentNode(void);
    //@}
  };

}}

#include <gecode/gist/layoutcursor.hpp>

#endif

// STATISTICS: gist-any
