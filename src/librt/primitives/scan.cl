/*
Copyright 2011-2012 Andreas Kloeckner
Copyright 2008-2011 NVIDIA Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Derived from thrust/detail/backend/cuda/detail/fast_scan.inl
within the Thrust project, https://code.google.com/p/thrust/

Direct browse link:
https://code.google.com/p/thrust/source/browse/thrust/detail/backend/cuda/detail/fast_scan.inl
*/
#define SWG_SIZE 128
#define UWG_SIZE 256

/**/
#define INPUT_EXPR(i)                       (input[i])
#define SCAN_EXPR(a,b,across_seg_boundary)  (a+b)
#define NEUTRAL                             0

#define SCAN_DTYPE                          uint
#define INDEX_DTYPE                         uint

#define IS_FIRST_LEVEL                      1

#define IS_SEGMENTED                        0
#define IS_SEG_START(i,a)                   (0)
/**/

#define REQD_WG_SIZE(X,Y,Z) __attribute__((reqd_work_group_size(X, Y, Z)))

typedef SCAN_DTYPE scan_type;
typedef INDEX_DTYPE index_type;

// NO_SEG_BOUNDARY is the largest representable integer in index_type.
// This assumption is used in code below.
#define NO_SEG_BOUNDARY UINT_MAX

#define K 8

__kernel
REQD_WG_SIZE(UWG_SIZE, 1, 1)
void NAME_PREFIX(_final_update)(
  __global scan_type *output,
  const index_type N,
  const index_type interval_size,
  __global scan_type *restrict interval_results,
  __global scan_type *restrict partial_scan_buffer
#if IS_SEGMENTED
      , __global index_type *restrict g_first_segment_start_in_interval
#endif
  )
{
#if USE_LOOKBEHIND_UPDATE
  __local scan_type ldata[UWG_SIZE];
#endif

  const index_type interval_begin = interval_size * get_group_id(0);
  const index_type interval_end = min(interval_begin + interval_size, N);

  // carry from last interval
  scan_type carry = NEUTRAL;

  if (get_group_id(0) != 0)
    carry = interval_results[get_group_id(0) - 1];

#if IS_SEGMENTED
  const index_type first_seg_start_in_interval = g_first_segment_start_in_interval[get_group_id(0)];
#endif

#if !USE_LOOKBEHIND_UPDATE
  // no look-behind ('prev_item' not in output_statement -> simpler)
  index_type update_i = interval_begin + get_local_id(0);

#if IS_SEGMENTED
  index_type seg_end = min(first_seg_start_in_interval, interval_end);
#endif

  for (; update_i < interval_end; update_i += UWG_SIZE) {
    scan_type partial_val = partial_scan_buffer[update_i];
    scan_type item = SCAN_EXPR(carry, partial_val,
#if IS_SEGMENTED
	(update_i >= seg_end)
#else
	false
#endif
	);

    index_type i = update_i;

    { OUTPUT_STATEMENT; }
  }
#else
  // allow look-behind ('prev_item' in output_statement -> complicated)

  // We are not allowed to branch across barriers at a granularity smaller
  // than the whole workgroup. Therefore, the for loop is group-global,
  // and there are lots of local ifs.

  index_type group_base = interval_begin;
  scan_type prev_item = carry; // (A)

  for (; group_base < interval_end; group_base += UWG_SIZE) {
    index_type update_i = group_base+get_local_id(0);

    // load a work group's worth of data
    if (update_i < interval_end) {
      scan_type tmp = partial_scan_buffer[update_i];

      tmp = SCAN_EXPR(carry, tmp,
#if IS_SEGMENTED
	  (update_i >= first_seg_start_in_interval)
#else
	  false
#endif
	  );

      ldata[get_local_id(0)] = tmp;

#if IS_SEGMENTED
      l_segment_start_flags[get_local_id(0)] = g_segment_start_flags[update_i];
#endif
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // find prev_item
    if (get_local_id(0) != 0)
      prev_item = ldata[get_local_id(0) - 1];
    /*
       else
       prev_item = carry (see (A)) OR last tail (see (B));
     */

    if (update_i < interval_end) {
#if IS_SEGMENTED
      if (l_segment_start_flags[get_local_id(0)])
	prev_item = NEUTRAL;
#endif

      scan_type item = ldata[get_local_id(0)];
      index_type i = update_i;
      { OUTPUT_STATEMENT; }
    }

    if (get_local_id(0) == 0)
      prev_item = ldata[UWG_SIZE - 1]; // (B)

    barrier(CLK_LOCAL_MEM_FENCE);
  }
#endif
}

__kernel
REQD_WG_SIZE(SWG_SIZE, 1, 1)
void NAME_PREFIX(_intervals)(
  __global scan_type *input,
  __global scan_type *restrict partial_scan_buffer,
  const index_type N,
  const index_type interval_size
#if IS_FIRST_LEVEL
      , __global scan_type *restrict interval_results
#endif
#if IS_SEGMENTED && IS_FIRST_LEVEL
      // NO_SEG_BOUNDARY if no segment boundary in interval.
      , __global index_type *restrict g_first_segment_start_in_interval
#endif
  )
{
  // padded in WG_SIZE to avoid bank conflicts
  // index K in first dimension used for carry storage
  __local scan_type ldata[K + 1][SWG_SIZE];

#if IS_SEGMENTED
  __local char l_segment_start_flags[K][SWG_SIZE];
  __local index_type l_first_segment_start_in_subtree[SWG_SIZE];

  // only relevant/populated for local id 0
  index_type first_segment_start_in_interval = NO_SEG_BOUNDARY;

  index_type first_segment_start_in_k_group, first_segment_start_in_subtree;
#endif

  const index_type interval_begin = interval_size * get_group_id(0);
  const index_type interval_end   = min(interval_begin + interval_size, N);

  const index_type unit_size  = K * SWG_SIZE;

  index_type unit_base = interval_begin;

  for(; unit_base + unit_size <= interval_end; unit_base += unit_size) {
    // Algorithm: Each work group is responsible for one contiguous
    // 'interval', of which there are just enough to fill all compute
    // units.  Intervals are split into 'units'. A unit is what gets
    // worked on in parallel by one work group.

    // Each unit has two axes--the local-id axis and the k axis.
    //
    // * * * * * * * * * * ----> lid
    // * * * * * * * * * *
    // * * * * * * * * * *
    // * * * * * * * * * *
    // * * * * * * * * * *
    // |
    // v k

    // This is a three-phase algorithm, in which first each interval
    // does its local scan, then a scan across intervals exchanges data
    // globally, and the final update adds the exchanged sums to each
    // interval.

    // Exclusive scan is realized by performing a right-shift inside
    // the final update.

    // read a unit's worth of data from global

    for(index_type k = 0; k < K; k++) {
      const index_type offset = k*SWG_SIZE + get_local_id(0);
      const index_type read_i = unit_base + offset;

      scan_type scan_value = INPUT_EXPR(read_i);

      const index_type o_mod_k = offset % K;
      const index_type o_div_k = offset / K;
      ldata[o_mod_k][o_div_k] = scan_value;

#if IS_SEGMENTED
      bool is_seg_start = IS_SEG_START(read_i, scan_value);
      l_segment_start_flags[o_mod_k][o_div_k] = is_seg_start;
#endif
    }

    // carry in from previous unit, if applicable.
#if IS_SEGMENTED
    barrier(CLK_LOCAL_MEM_FENCE);

    first_segment_start_in_k_group = NO_SEG_BOUNDARY;
    if (l_segment_start_flags[0][get_local_id(0)])
      first_segment_start_in_k_group = unit_base + K*get_local_id(0);
#endif

    if (get_local_id(0) == 0 && unit_base != interval_begin)
      ldata[0][0] = SCAN_EXPR(
        ldata[K][SWG_SIZE - 1], ldata[0][0],
#if IS_SEGMENTED
	(l_segment_start_flags[0][0])
#else
	false
#endif
	);

    barrier(CLK_LOCAL_MEM_FENCE);

    // scan along k (sequentially in each work item)
    scan_type sum = ldata[0][get_local_id(0)];

    for(index_type k = 1; k < K; k++) {
      scan_type tmp = ldata[k][get_local_id(0)];
      index_type seq_i = unit_base + K*get_local_id(0) + k;

#if IS_SEGMENTED
      if (l_segment_start_flags[k][get_local_id(0)]) {
        first_segment_start_in_k_group = min(first_segment_start_in_k_group, seq_i);
      }
#endif

      sum = SCAN_EXPR(sum, tmp,
#if IS_SEGMENTED
	(l_segment_start_flags[k][get_local_id(0)])
#else
	false
#endif
	);
      ldata[k][get_local_id(0)] = sum;
    }

    // store carry in out-of-bounds (padding) array entry in the K direction
    ldata[K][get_local_id(0)] = sum;

#if IS_SEGMENTED
    l_first_segment_start_in_subtree[get_local_id(0)] = first_segment_start_in_k_group;
#endif

    barrier(CLK_LOCAL_MEM_FENCE);

    // tree-based parallel scan along local id
    scan_type val = ldata[K][get_local_id(0)];

    for (index_type offset=1; offset <= SWG_SIZE; offset *= 2) {
      if (get_local_id(0) >= offset)
      {
	scan_type tmp = ldata[K][get_local_id(0) - offset];
	val = SCAN_EXPR(tmp, val,
#if IS_SEGMENTED
	    (l_first_segment_start_in_subtree[get_local_id(0)] != NO_SEG_BOUNDARY)
#else
	    false
#endif
	    );

#if IS_SEGMENTED
	// Prepare for l_first_segment_start_in_subtree, below.

	// Note that this update must take place *even* if we're
	// out of bounds.
	first_segment_start_in_subtree = min(
	    l_first_segment_start_in_subtree[get_local_id(0)],
	    l_first_segment_start_in_subtree[get_local_id(0) - offset]);
#endif
      }
#if IS_SEGMENTED
      else
      {
	first_segment_start_in_subtree = l_first_segment_start_in_subtree[get_local_id(0)];
      }
#endif

      barrier(CLK_LOCAL_MEM_FENCE);
      ldata[K][get_local_id(0)] = val;
#if IS_SEGMENTED
      l_first_segment_start_in_subtree[get_local_id(0)] = first_segment_start_in_subtree;
#endif
      barrier(CLK_LOCAL_MEM_FENCE);
    }

    // update local values
    if (get_local_id(0) > 0) {
      sum = ldata[K][get_local_id(0) - 1];

      for(index_type k = 0; k < K; k++) {
	  scan_type tmp = ldata[k][get_local_id(0)];
	  ldata[k][get_local_id(0)] = SCAN_EXPR(sum, tmp,
#if IS_SEGMENTED
	      (unit_base + K*get_local_id(0) + k >= first_segment_start_in_k_group)
#else
	      false
#endif
	      );
      }
    }

#if IS_SEGMENTED
    if (get_local_id(0) == 0) {
      // update interval-wide first-seg variable from current unit
      first_segment_start_in_interval = min(
	  first_segment_start_in_interval,
	  l_first_segment_start_in_subtree[SWG_SIZE-1]);
    }
#endif

    barrier(CLK_LOCAL_MEM_FENCE);

    // write data
    for(index_type k = 0; k < K; k++) {
      const index_type offset = k*SWG_SIZE + get_local_id(0);

      partial_scan_buffer[unit_base + offset] = ldata[offset % K][offset / K];
    }

    barrier(CLK_LOCAL_MEM_FENCE);
  }


  if (unit_base < interval_end) {
    // Algorithm: Each work group is responsible for one contiguous
    // 'interval', of which there are just enough to fill all compute
    // units.  Intervals are split into 'units'. A unit is what gets
    // worked on in parallel by one work group.

    // Each unit has two axes--the local-id axis and the k axis.
    //
    // * * * * * * * * * * ----> lid
    // * * * * * * * * * *
    // * * * * * * * * * *
    // * * * * * * * * * *
    // * * * * * * * * * *
    // |
    // v k

    // This is a three-phase algorithm, in which first each interval
    // does its local scan, then a scan across intervals exchanges data
    // globally, and the final update adds the exchanged sums to each
    // interval.

    // Exclusive scan is realized by performing a right-shift inside
    // the final update.

    // read a unit's worth of data from global

    for(index_type k = 0; k < K; k++) {
      const index_type offset = k*SWG_SIZE + get_local_id(0);
      const index_type read_i = unit_base + offset;

      if (read_i < interval_end) {
	scan_type scan_value = INPUT_EXPR(read_i);

	const index_type o_mod_k = offset % K;
	const index_type o_div_k = offset / K;
	ldata[o_mod_k][o_div_k] = scan_value;

#if IS_SEGMENTED
	bool is_seg_start = IS_SEG_START(read_i, scan_value);
	l_segment_start_flags[o_mod_k][o_div_k] = is_seg_start;
#endif
      }
    }

    // carry in from previous unit, if applicable.
#if IS_SEGMENTED
    barrier(CLK_LOCAL_MEM_FENCE);

    first_segment_start_in_k_group = NO_SEG_BOUNDARY;
    if (l_segment_start_flags[0][get_local_id(0)])
      first_segment_start_in_k_group = unit_base + K*get_local_id(0);
#endif

    if (get_local_id(0) == 0 && unit_base != interval_begin)
      ldata[0][0] = SCAN_EXPR(ldata[K][SWG_SIZE - 1], ldata[0][0],
#if IS_SEGMENTED
	  (l_segment_start_flags[0][0])
#else
	  false
#endif
	  );

    barrier(CLK_LOCAL_MEM_FENCE);

    // scan along k (sequentially in each work item)
    scan_type sum = ldata[0][get_local_id(0)];

    const index_type offset_end = interval_end - unit_base;

    for(index_type k = 1; k < K; k++) {
      scan_type tmp = ldata[k][get_local_id(0)];
      index_type seq_i = unit_base + K*get_local_id(0) + k;

#if IS_SEGMENTED
      if (l_segment_start_flags[k][get_local_id(0)]) {
        first_segment_start_in_k_group = min(first_segment_start_in_k_group, seq_i);
      }
#endif

      sum = SCAN_EXPR(sum, tmp,
#if IS_SEGMENTED
	(l_segment_start_flags[k][get_local_id(0)])
#else
	false
#endif
	);
      ldata[k][get_local_id(0)] = sum;
    }

    // store carry in out-of-bounds (padding) array entry in the K direction
    ldata[K][get_local_id(0)] = sum;

#if IS_SEGMENTED
    l_first_segment_start_in_subtree[get_local_id(0)] = first_segment_start_in_k_group;
#endif

    barrier(CLK_LOCAL_MEM_FENCE);

    // tree-based parallel scan along local id
    scan_type val = ldata[K][get_local_id(0)];

    for (index_type offset=1; offset <= SWG_SIZE; offset *= 2) {
      if (get_local_id(0) >= offset)
      {
	scan_type tmp = ldata[K][get_local_id(0) - offset];
	if (K*get_local_id(0) < offset_end) {
	  val = SCAN_EXPR(tmp, val,
#if IS_SEGMENTED
	      (l_first_segment_start_in_subtree[get_local_id(0)] != NO_SEG_BOUNDARY)
#else
	      false
#endif
	    );
	}

#if IS_SEGMENTED
	// Prepare for l_first_segment_start_in_subtree, below.

	// Note that this update must take place *even* if we're
	// out of bounds.
	first_segment_start_in_subtree = min(
	    l_first_segment_start_in_subtree[get_local_id(0)],
	    l_first_segment_start_in_subtree[get_local_id(0) - offset]);
#endif
      }
#if IS_SEGMENTED
      else
      {
	first_segment_start_in_subtree = l_first_segment_start_in_subtree[get_local_id(0)];
      }
#endif

      barrier(CLK_LOCAL_MEM_FENCE);
      ldata[K][get_local_id(0)] = val;
#if IS_SEGMENTED
      l_first_segment_start_in_subtree[get_local_id(0)] = first_segment_start_in_subtree;
#endif
      barrier(CLK_LOCAL_MEM_FENCE);
    }

    // update local values
    if (get_local_id(0) > 0) {
      sum = ldata[K][get_local_id(0) - 1];

      for(index_type k = 0; k < K; k++) {
	if (K * get_local_id(0) + k < offset_end) {
	  scan_type tmp = ldata[k][get_local_id(0)];
	  ldata[k][get_local_id(0)] = SCAN_EXPR(sum, tmp,
#if IS_SEGMENTED
	      (unit_base + K*get_local_id(0) + k >= first_segment_start_in_k_group)
#else
	      false
#endif
	      );
	}
      }
    }

#if IS_SEGMENTED
    if (get_local_id(0) == 0) {
      // update interval-wide first-seg variable from current unit
      first_segment_start_in_interval = min(
	  first_segment_start_in_interval,
	  l_first_segment_start_in_subtree[SWG_SIZE-1]);
    }
#endif

    barrier(CLK_LOCAL_MEM_FENCE);

    // write data
    for(index_type k = 0; k < K; k++) {
      const index_type offset = k*SWG_SIZE + get_local_id(0);

      if (unit_base + offset < interval_end) {
	partial_scan_buffer[unit_base + offset] = ldata[offset % K][offset / K];
      }
    }

    barrier(CLK_LOCAL_MEM_FENCE);
  }


  // write interval sum
#if IS_FIRST_LEVEL
  if (get_local_id(0) == 0) {
    interval_results[get_group_id(0)] = partial_scan_buffer[interval_end - 1];
#if IS_SEGMENTED
    g_first_segment_start_in_interval[get_group_id(0)] = first_segment_start_in_interval;
#endif
  }
#endif
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
