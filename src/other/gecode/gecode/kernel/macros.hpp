/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
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

/**
 * \brief Check whether modification event \a me is failed, and forward failure.
 *
 * To be used inside the propagate member function of a propagator
 * or the commit member function of a brancher.
 * \ingroup TaskActor
 */
#define GECODE_ME_CHECK(me) do {                \
  if (::Gecode::me_failed(me))                  \
    return ::Gecode::ES_FAILED;                 \
  } while (0)

/**
 * \brief Check whether \a me is failed or modified, and forward failure.
 *
 * To be used inside the propagate member function of a propagator
 * or the commit member function of a brancher.
 * \ingroup TaskActor
 */
#define GECODE_ME_CHECK_MODIFIED(modified, me) do {        \
    ModEvent __me__ ## __LINE__ = (me);                    \
    if (::Gecode::me_failed(__me__ ## __LINE__))           \
      return ::Gecode::ES_FAILED;                          \
    modified |= ::Gecode::me_modified(__me__ ## __LINE__); \
  } while (0)

/**
 * \brief Check whether modification event \a me is failed, and fail space \a home.
 *
 * To be used inside post functions.
 * \ingroup TaskActor
 */
#define GECODE_ME_FAIL(me) do {                 \
  if (::Gecode::me_failed(me)) {                \
    (home).fail();                              \
    return;                                     \
  }} while (0)



/**
 * \brief Check whether execution status \a es is failed or subsumed, and
 * forward failure or subsumption.
 *
 * \ingroup TaskActor
 */
#define GECODE_ES_CHECK(es) do {                        \
    ::Gecode::ExecStatus __es__ ## __LINE__ = (es);     \
    if (__es__ ## __LINE__ < ::Gecode::ES_OK)           \
      return __es__ ## __LINE__;                        \
  } while (0)

/**
 * \brief Check whether execution status \a es is failed, and fail
 * space \a home.
 *
 * \ingroup TaskActor
 */
#define GECODE_ES_FAIL(es) do {                                 \
    ::Gecode::ExecStatus __es__ ## __LINE__ = (es);             \
    assert(__es__ ## __LINE__ != ::Gecode::__ES_SUBSUMED);      \
    if (__es__ ## __LINE__ < ::Gecode::ES_OK) {                 \
      (home).fail(); return;                                    \
    }                                                           \
  } while (0)

/**
 * \brief Rewrite propagator by executing post function
 *
 * \ingroup TaskActor
 */
#define GECODE_REWRITE(prop,post) do {                                   \
  Propagator& __p__ ## __LINE__ = (prop);                                \
  size_t     __s__ ## __LINE__  = __p__ ## __LINE__.dispose(home);       \
  ExecStatus __es__ ## __LINE__ = (post);                                \
  if (__es__ ## __LINE__ != ::Gecode::ES_OK)                             \
    return ::Gecode::ES_FAILED;                                          \
  return home.ES_SUBSUMED_DISPOSED(__p__ ## __LINE__,__s__ ## __LINE__); \
} while (0)

// STATISTICS: kernel-other
