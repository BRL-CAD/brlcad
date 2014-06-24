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
predicate gecode_inverse_set(array[int] of var set of int: f,
    array[int] of var set of int: invf, int: xoff, int: yoff);

predicate inverse_set(array[int] of var set of int: f,
                      array[int] of var set of int: invf) =
    forall(i in index_set(f)) (
           f[i] subset index_set(invf)
    ) /\
    forall(j in index_set(invf)) (
        invf[j] subset index_set(f)
    ) /\
    if (min(index_set(f)) >= 0 /\ min(index_set(invf)) >= 0) then
        gecode_inverse_set(f,invf,min(index_set(f)),min(index_set(invf)))
    else
        forall(i in index_set(f), j in index_set(invf)) (
            (j in f[i] <-> i in invf[j])
        )
    endif;
