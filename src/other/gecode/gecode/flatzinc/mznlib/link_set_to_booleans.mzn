%
%  Main authors:
%     Guido Tack <tack@gecode.org>
%
%  Copyright:
%     Guido Tack, 2007
%
%  Last modified:
%     $Date$ by $Author$
%     $Revision$
%
%  This file is part of Gecode, the generic constraint
%  development environment:
%     http://www.gecode.org
%
%  Permission is hereby granted, free of charge, to any person obtaining
%  a copy of this software and associated documentation files (the
%  "Software"), to deal in the Software without restriction, including
%  without limitation the rights to use, copy, modify, merge, publish,
%  distribute, sublicense, and/or sell copies of the Software, and to
%  permit persons to whom the Software is furnished to do so, subject to
%  the following conditions:
%
%  The above copyright notice and this permission notice shall be
%  included in all copies or substantial portions of the Software.
%
%  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
%  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
%  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
%  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
%  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
%  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
%  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
%
%

predicate gecode_link_set_to_booleans(var set of int: s, array[int] of var bool: b, int: idx);

predicate link_set_to_booleans(var set of int: s, array[int] of var bool: b) =
    assert(ub(s) subset index_set(b),
        "link_set_to_booleans: the index set of b must be a superset of the possible values of s",
        if min(index_set(b)) < 0 then
          forall(i in index_set(b)) ( b[i] <-> i in s )
        else
          gecode_link_set_to_booleans(s,b,min(index_set(b)))
        endif
    );