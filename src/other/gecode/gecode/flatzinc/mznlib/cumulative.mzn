%
%  Main authors:
%     Guido Tack <tack@gecode.org>
%     Mikael lagerkvist <lagerkvist@gecode.org>
%
%  Copyright:
%     Guido Tack, 2007
%     Mikael Lagerkvist, 2009
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

predicate cumulative(array[int] of var int: s,
                     array[int] of var int: d,
                     array[int] of var int: r, var int: b) =              
    assert(index_set(s) == index_set(d) /\ index_set(s) == index_set(r),
        "cumulative: the 3 array arguments must have identical index sets",
        assert(lb_array(d) >= 0 /\ lb_array(r) >= 0,
            "cumulative: durations and resource usages must be non-negative",
             cumulatives(s, d, r, b)
        )
    );

% Specialized cumulatives propagator is only for fixed bounds
predicate cumulatives(array[int] of var int: s,
                      array[int] of var int: d,
                      array[int] of var int: r, var int: b);
