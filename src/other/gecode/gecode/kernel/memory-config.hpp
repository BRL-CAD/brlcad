/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Bugfixes provided by:
 *     Zandra Norman
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

namespace Gecode {

  /**
   * \brief Parameters defining memory management policy for spaces
   * \ingroup FuncMemSpace
   */
  namespace MemoryConfig {
    /**
     * \brief How many heap chunks should be cached at most
     */
    const unsigned int n_hc_cache = 4*4;

    /**
     * \brief Minimal size of a heap chunk requested from the OS
     */
    const size_t hcsz_min =  1024;
    /**
     * \brief Maximal size of a heap chunk requested from the OS
     *
     * Maximal is not strictly true, if a contiguous memory chunk is
     * requested that exceeds \a hcsz_max, a chunk will be allocated
     * that fits that request.
     */
    const size_t hcsz_max = 32 * 1024;
    /**
     * \brief Increment ratio for chunk size
     *
     * If a space has requested \a hcsz_inc_ratio chunks of heap memory,
     * the chunk size is doubled.
     */
    const int hcsz_inc_ratio = 8;
    /**
     * \brief Decrement ratio for chunk size
     *
     * When a space is cloned, the new clone normally inherits the
     * current chunk size from the original space. However, if the
     * original space has requested less than \a hcsz_dec_ratio
     * heap chunks of the current chunk size, the current chunk size
     * for the clone is halfed.
     */
    const int hcsz_dec_ratio = 8;

    /**
     * \brief Unit size for free lists
     *
     * The unit size (given as binary logarithm) defines how big
     * a unit of memory for free lists is. Also, it defines the
     * alignment. Sizes of free list objects must be multiples of
     * the unit size.
     *
     * Currently, for 32 bit machines, the unit size is 4 bytes.
     * For 64 bit machines, it is 8 bytes.
     */
    const int fl_unit_size = ((sizeof(void*) == 4) ? 2 : 3);
    /**
     * \brief Minimal size for free list element
     *
     * The minimal size is given in the number of free list units.
     *
     * Currently, for 32 bit machines, the minimal size is 8 bytes.
     * For 64 bit machines, it is 16 bytes.
     */
    const int fl_size_min  = ((sizeof(void*) == 4) ? 2 : 2);
    /**
     * \brief Maximal size for free list element
     *
     * The maximal size is given in the number of free list units.
     *
     * Currently, for 32 bit machines, the maximal size is 12 bytes.
     * For 64 bit machines, it is 24 bytes.
     */
    const int fl_size_max  = ((sizeof(void*) == 4) ? 3 : 3);
    /**
     * \brief Number of free lists elements to allocate
     *
     * When a request for a free list element can not be fulfilled, as
     * the free list is empty and there is also no reusable memory
     * available, allocate \a fl_refill free list elements.
     */
    const int fl_refill = 8;
    /**
     * \brief Memory alignment
     *
     * Memory alignment is controlled by the macro GECODE_MEMORY_ALIGNMENT.
     * If it is not defined, it assumed to be 8. Otherwise, the defined
     * value is used.
     */
#ifndef GECODE_MEMORY_ALIGNMENT
#define GECODE_MEMORY_ALIGNMENT 8
#endif
    /**
     * \brief Size of region area
     *
     * The region area can be used in a stack fashion through access
     * from a space. If the a request exceeds the current free space,
     * memory will be allocated from the heap.
     */
    const size_t region_area_size = 32 * 1024;

    /// Align size \a s to the required alignment
    void align(size_t& s);

    /*
     * Alignment
     *
     */
    forceinline void
    align(size_t& s) {
      s += ((GECODE_MEMORY_ALIGNMENT -
             (s & (GECODE_MEMORY_ALIGNMENT - 1))) &
            (GECODE_MEMORY_ALIGNMENT - 1));
    }

  }
}

// STATISTICS: kernel-memory
