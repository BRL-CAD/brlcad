#include "common.cl"

#if !RT_SINGLE_HIT

#define BOOL_STACKSIZE	64

/**
 * Flattened RPN version of the infix union tree.
 */
#define UOP_NOP		-1
#define UOP_UNION	-2
#define UOP_INTERSECT	-3
#define UOP_SUBTRACT	-4
#define UOP_XOR		-5

/* Boolean values.  Not easy to change, but defined symbolically */
#define BOOL_FALSE	0
#define BOOL_TRUE	1

/* Operations on dynamic bitarrays */
inline uint
bindex(const uint b)
{
    return (b >> 5);
}

inline uint
bmask(const uint b)
{
    return (1 << (b & 31));
}

inline uint
isset(global uint *bitset, const uint offset, const uint b)
{
    return (bitset[offset + bindex(b)] & bmask(b));
}

inline uint
clr(global uint *bitset, const uint offset, const uint b)
{
    return (bitset[offset + bindex(b)] &= ~bmask(b));
}

inline uint
set(global uint *bitset, const uint offset, const uint b)
{
    return (bitset[offset + bindex(b)] |= bmask(b));
}

/**
 * When duplicating partitions, the bitarray representing the segments
 * of the partition also has to be copied
 */
inline void
copy_bv(global uint *bitset, const uint bv_index, const uint copy_to, const uint copy_from)
{
    for (uint i = 0; i < bv_index; i++) {
        bitset[copy_to + i] = bitset[copy_from + i];
    }
}

/**
 * Update 'back_pp' and 'forw_pp' values when inserting new partitions
 * Update head value when inserting at head
 *
 * Head partition: 'back_pp' = head
 * Tail partition: 'forw_pp' = UINT_MAX
 */
inline void
insert_partition_pp(global struct partition *partitions, global uint *ipartition, size_t id, uint *head, uint new, uint old)
{
    if (ipartition[id] == 0)
        return;

    if (*head == old) {
        partitions[old].back_pp = new;
        partitions[new].back_pp = new;
        partitions[new].forw_pp = old;
        *head = new;
    } else {
	partitions[partitions[old].back_pp].forw_pp = new;
	partitions[new].back_pp = partitions[old].back_pp;
	partitions[new].forw_pp = old;
	partitions[old].back_pp = new;
    }
}

/**
 * Update 'back_pp' and 'forw_pp' values when appending new partitions
 * Update tail value
 *
 * Head partition: 'back_pp' = head
 * Tail partition: 'forw_pp' = UINT_MAX
 */
inline void
append_partition_pp(global struct partition *partitions, global uint *ipartition, size_t id, uint new, uint *tail)
{
    if (ipartition[id] == 0) {
        partitions[new].back_pp = new;
        partitions[new].forw_pp = UINT_MAX;
        *tail = new;
    } else {
	partitions[new].back_pp = *tail;
	partitions[new].forw_pp = UINT_MAX;
	partitions[*tail].forw_pp = new;
	*tail = new;
    }
}

inline void
initialize_partition(global struct partition *partitions, const uint pp_idx)
{
    if (pp_idx != UINT_MAX) {
        partitions[pp_idx].inflip = 0;
        partitions[pp_idx].outflip = 0;
        partitions[pp_idx].region_id = UINT_MAX;
    }
}

/**
 * If a zero thickness segment abuts another partition, it will be
 * fused in, later.
 *
 * If it is free standing, then it will remain as a zero thickness
 * partition, which probably signals going through some solid an odd
 * number of times, or hitting an NMG wire edge or NMG lone vertex.
 */
void
bool_weave0seg(RESULT_TYPE segp, global struct partition *partitions, global uint *ipartition, global uint *h, global uint *segs_bv, const uint bv_index, uint k, size_t id, uint start_index, uint *head)
{
    global struct partition *pp;
    global struct partition *newpp;

    //bool_weave0seg() with empty partition list
    if (ipartition[id] == 0)
	return;

    /* See if this segment ends before start of first partition */
    if (segp->seg_out.hit_dist < partitions[*head].inhit.hit_dist) {
	newpp = &partitions[start_index + ipartition[id]];
        initialize_partition(partitions, start_index + ipartition[id]);

	newpp->inseg = k;
	newpp->inhit = segp->seg_in;
	newpp->outseg = k;
	newpp->outhit = segp->seg_out;
	set(segs_bv, (start_index + ipartition[id]) * bv_index, k-h[id]);
	insert_partition_pp(partitions, ipartition, id, head, start_index + ipartition[id], *head);
	ipartition[id]++;
	return;
    }

    /*
     * Cases: seg at start of pt, in middle of pt, at end of pt, or
     * past end of pt but before start of next pt.
     *
     * XXX For the first 3 cases, we might want to make a new 0-len pt,
     * XXX especially as the NMG ray-tracer starts reporting wire hits.
     */
    for (uint current_index = *head; current_index != UINT_MAX; current_index = partitions[current_index].forw_pp) {
	pp = &partitions[current_index];
	if (NEAR_EQUAL(segp->seg_in.hit_dist, pp->inhit.hit_dist, rti_tol_dist) ||
	    NEAR_EQUAL(segp->seg_out.hit_dist, pp->inhit.hit_dist, rti_tol_dist)
	   )
	    return;

	if (NEAR_EQUAL(segp->seg_in.hit_dist, pp->outhit.hit_dist, rti_tol_dist) ||
	    NEAR_EQUAL(segp->seg_out.hit_dist, pp->outhit.hit_dist, rti_tol_dist)
	   )
	    return;

	if (segp->seg_out.hit_dist <= pp->outhit.hit_dist &&
	    segp->seg_in.hit_dist >= pp->inhit.hit_dist)
	    return;

	if (pp->forw_pp != UINT_MAX && segp->seg_out.hit_dist < partitions[pp->forw_pp].inhit.hit_dist) {
	    //0-len segment after existing partition, but before next partition.
	    newpp = &partitions[start_index + ipartition[id]];
            initialize_partition(partitions, start_index + ipartition[id]);

	    newpp->inseg = k;
	    newpp->inhit = segp->seg_in;
	    newpp->outseg = k;
	    newpp->outhit = segp->seg_out;
	    set(segs_bv, (start_index + ipartition[id]) * bv_index, k-h[id]);
	    insert_partition_pp(partitions, ipartition, id, head, start_index + ipartition[id], pp->forw_pp);
	    ipartition[id]++;
	    return;
	}
    }
}

__kernel void
rt_boolweave(global struct partition *partitions, global uint *ipartition, RESULT_TYPE segs,
        global uint *h, global uint *segs_bv, const int cur_pixel,
        const int last_pixel, const int max_depth, global uint *head_partition)
{
    const size_t id = get_global_size(0)*get_global_id(1)+get_global_id(0);

    if (id >= (last_pixel-cur_pixel+1))
	return;

    const int pixelnum = cur_pixel+id;

    global struct partition *pp;
    double diff, diff_se;

    head_partition[id] = UINT_MAX;

    uint start_index = 2 * h[id];
    uint head_pp = start_index;
    uint tail_pp = start_index;
    uint bv_index = max_depth/32 + 1;

    for (uint k=h[id]; k!=h[id+1]; k++) {
	RESULT_TYPE segp = segs+k;

	global struct partition *newpp;
	uint lastseg;
	global struct hit *lasthit;
	bool lastflip = 0;
	uint j;

	/* Make nearly zero be exactly zero */
	if (NEAR_ZERO(segp->seg_in.hit_dist, rti_tol_dist))
	    segp->seg_in.hit_dist = 0;
	if (NEAR_ZERO(segp->seg_out.hit_dist, rti_tol_dist))
	    segp->seg_out.hit_dist = 0;

	/* Totally ignore things behind the start position */
	if (segp->seg_in.hit_dist < -10.0)
	    continue;

	if (!(segp->seg_in.hit_dist >= -INFINITY && segp->seg_out.hit_dist <= INFINITY))
	    continue;

	if (segp->seg_in.hit_dist > segp->seg_out.hit_dist)
	    continue;

	diff = segp->seg_in.hit_dist - segp->seg_out.hit_dist;

	/*
	 * Weave this segment into the existing partitions, creating
	 * new partitions as necessary.
	 */
	if (ipartition[id] == 0) {
	    /* No partitions yet, simple! */
	    pp = &partitions[start_index + ipartition[id]];
            initialize_partition(partitions, start_index + ipartition[id]);

	    pp->inseg = k;
	    pp->inhit = segp->seg_in;
	    pp->outseg = k;
	    pp->outhit = segp->seg_out;
	    set(segs_bv, (start_index + ipartition[id]) * bv_index, k-h[id]);
	    append_partition_pp(partitions, ipartition, id, start_index + ipartition[id], &tail_pp);
	    ipartition[id]++;
	} else if (NEAR_ZERO(diff, rti_tol_dist)) {
	    /* Check for zero-thickness segment, within tol */
	    bool_weave0seg(segp, partitions, ipartition, h, segs_bv, bv_index, k, id, start_index, &head_pp);
	} else if (ipartition[id] > 0 && segp->seg_in.hit_dist >= partitions[tail_pp].outhit.hit_dist) {
	    /*
	     * Segment starts exactly at last partition's end, or
	     * beyond last partitions end.  Make new partition.
	     */
	    pp = &partitions[start_index + ipartition[id]];
            initialize_partition(partitions, start_index + ipartition[id]);

	    pp->inseg = k;
	    pp->inhit = segp->seg_in;
	    pp->outseg = k;
	    pp->outhit = segp->seg_out;
	    set(segs_bv, (start_index + ipartition[id]) * bv_index, k-h[id]);
	    append_partition_pp(partitions, ipartition, id, start_index + ipartition[id], &tail_pp);
	    ipartition[id]++;
	} else {
	    /* Loop through current partition list weaving the current
	     * input segment into the list. The following three variables
	     * keep track of the current starting point of the input
	     * segment. The starting point of the segment moves to higher
	     * hit_dist values (as it is woven in) until it is entirely
	     * consumed.
	     */
	    lastseg = k;
	    lasthit = &segp->seg_in;
	    lastflip = 0;
	    for (j = head_pp; j != UINT_MAX; j = partitions[j].forw_pp) {
		pp = &partitions[j];
		diff_se = lasthit->hit_dist - pp->outhit.hit_dist;

		if (diff_se > rti_tol_dist) {
		    /* Seg starts beyond the END of the
		     * current partition.
		     *	PPPP
		     *	    SSSS
		     * Advance to next partition.
		     */
		    continue;
		}
		diff = lasthit->hit_dist - pp->inhit.hit_dist;
		if (diff_se > -(rti_tol_dist) && diff > rti_tol_dist) {
		    /*
		     * Seg starts almost "precisely" at the
		     * end of the current partition.
		     *	PPPP
		     *	    SSSS
		     * FUSE an exact match of the endpoints,
		     * advance to next partition.
		     */
		    lasthit->hit_dist = pp->outhit.hit_dist;
		    continue;
		}

		/*
		 * diff < ~~0
		 * Seg starts before current partition ends
		 *	PPPPPPPPPPP
		 *	  SSSS...
		 */
		if (diff >= rti_tol_dist) {
		    /*
		     * lasthit->hit_dist > pp->pt_inhit->hit_dist
		     * pp->pt_inhit->hit_dist < lasthit->hit_dist
		     *
		     * Segment starts after partition starts,
		     * but before the end of the partition.
		     * Note:  pt_seglist will be updated in equal_start.
		     *	PPPPPPPPPPPP
		     *	     SSSS...
		     *	newpp|pp
		     */
		    /* new partition is the span before seg joins partition */
		    newpp = &partitions[start_index + ipartition[id]];
		    *newpp = *pp;
		    copy_bv(segs_bv, bv_index, (start_index + ipartition[id]) * bv_index, j * bv_index);

		    pp->inseg = k;
		    pp->inhit = segp->seg_in;
		    pp->inflip = 0;
		    newpp->outseg = k;
		    newpp->outhit = segp->seg_in;
		    newpp->outflip = 1;
		    insert_partition_pp(partitions, ipartition, id, &head_pp, start_index + ipartition[id], j);
		    ipartition[id]++;
		} else if (diff > -(rti_tol_dist)) {
		    /*
		     * Make a subtle but important distinction here.  Even
		     * though the two distances are "equal" within
		     * tolerance, they are not exactly the same.  If the
		     * new segment is slightly closer to the ray origin,
		     * then use its IN point.
		     *
		     * This is an attempt to reduce the deflected normals
		     * sometimes seen along the edges of e.g. a cylinder
		     * unioned with an ARB8, where the ray hits the top of
		     * the cylinder and the *side* face of the ARB8 rather
		     * than the top face of the ARB8.
		     */
		    diff = segp->seg_in.hit_dist - pp->inhit.hit_dist;
		    if (pp->back_pp == head_pp || partitions[pp->back_pp].outhit.hit_dist <=
			    segp->seg_in.hit_dist) {
			if (NEAR_ZERO(diff, rti_tol_dist) &&
				diff < 0) {
			    pp->inseg = k;
			    pp->inhit = segp->seg_in;
			    pp->inflip = 0;
			}
		    }
		} else {
		    /*
		     * diff < ~~0
		     *
		     * Seg starts before current partition starts,
		     * but after the previous partition ends.
		     *	SSSSSSSS...
		     *	     PPPPP...
		     *	newpp|pp
		     */
		    newpp = &partitions[start_index + ipartition[id]];
                    initialize_partition(partitions, start_index + ipartition[id]);

		    set(segs_bv, (start_index + ipartition[id]) * bv_index, k-h[id]);
		    newpp->inseg = lastseg;
		    newpp->inhit = *lasthit;
		    newpp->inflip = lastflip;
		    diff = segp->seg_out.hit_dist - pp->inhit.hit_dist;
		    if (diff < -(rti_tol_dist)) {
			/*
			 * diff < ~0
			 * Seg starts and ends before current
			 * partition, but after previous
			 * partition ends (diff < 0).
			 *		SSSS
			 *	pppp		PPPPP...
			 *		newpp	pp
			 */
			newpp->outseg = k;
			newpp->outhit = segp->seg_out;
			newpp->outflip = 0;
			insert_partition_pp(partitions, ipartition, id, &head_pp, start_index + ipartition[id], j);
			ipartition[id]++;
			break;
		    } else if (diff < rti_tol_dist) {
			/*
			 * diff ~= 0
			 *
			 * Seg starts before current
			 * partition starts, and ends at or
			 * near the start of the partition.
			 * (diff == 0).  FUSE the points.
			 *	SSSSSS
			 *	     PPPPP
			 *	newpp|pp
			 * NOTE: only copy hit point, not
			 * normals or norm private stuff.
			 */
			newpp->outseg = k;
			newpp->outhit = segp->seg_out;
			newpp->outhit.hit_dist = pp->inhit.hit_dist;
			newpp->outflip = 0;
			insert_partition_pp(partitions, ipartition, id, &head_pp, start_index + ipartition[id], j);
			ipartition[id]++;
			break;
		    }
		    /*
		     * Seg starts before current partition
		     * starts, and ends after the start of the
		     * partition.  (diff > 0).
		     *	SSSSSSSSSS
		     *	      PPPPPPP
		     *	newpp| pp | ...
		     */
		    newpp->outseg = pp->inseg;
		    newpp->outhit = pp->inhit;
		    newpp->outflip = 1;
		    lastseg = pp->inseg;
		    lasthit = &pp->inhit;
		    lastflip = newpp->outflip;
		    insert_partition_pp(partitions, ipartition, id, &head_pp, start_index + ipartition[id], j);
		    ipartition[id]++;
		}

		/*
		 * Segment and partition start at (roughly) the same
		 * point.  When fusing 2 points together i.e., when
		 * NEAR_ZERO(diff, tol) is true, the two points MUST
		 * be forced to become exactly equal!
		 */
		diff = segp->seg_out.hit_dist - pp->outhit.hit_dist;
		if (diff > rti_tol_dist) {
		    /*
		     * Seg & partition start at roughly
		     * the same spot,
		     * seg extends beyond partition end.
		     *	PPPP
		     *	SSSSSSSS
		     *	pp  |  newpp
		     */
		    set(segs_bv, j * bv_index, k-h[id]);

		    lasthit = &pp->outhit;
		    lastseg = pp->outseg;
		    lastflip = 1;
		    continue;
		}
		if (diff > -(rti_tol_dist)) {
		    /*
		     * diff ~= 0
		     * Segment and partition start & end
		     * (nearly) together.
		     *	 PPPP
		     *	 SSSS
		     */
		    set(segs_bv, j * bv_index, k-h[id]);
		    break;
		} else {
		    /*
		     * diff < ~0
		     *
		     * Segment + Partition start together,
		     * segment ends before partition ends.
		     *	PPPPPPPPPP
		     *	SSSSSS
		     *	newpp| pp
		     */
		    newpp = &partitions[start_index + ipartition[id]];
		    *newpp = *pp;
		    copy_bv(segs_bv, bv_index, (start_index + ipartition[id]) * bv_index, j * bv_index);

		    set(segs_bv, (start_index + ipartition[id]) * bv_index, k-h[id]);
		    newpp->outseg = k;
		    newpp->outhit = segp->seg_out;
		    newpp->outflip = 0;
		    pp->inseg = k;
		    pp->inhit = segp->seg_out;
		    pp->inflip = 1;
		    insert_partition_pp(partitions, ipartition, id, &head_pp, start_index + ipartition[id], j);
		    ipartition[id]++;
		    break;
		}
		/* NOTREACHED */
	    }
	    /*
	     * Segment has portion which extends beyond the end
	     * of the last partition.  Tack on the remainder.
	     *  	PPPPP
	     *  	     SSSSS
	     */
	    if (ipartition[id] > 0 && j == UINT_MAX) {
		newpp = &partitions[start_index + ipartition[id]];
                initialize_partition(partitions, start_index + ipartition[id]);

		set(segs_bv, (start_index + ipartition[id]) * bv_index, k-h[id]);
		newpp->inseg = lastseg;
		newpp->inhit = *lasthit;
		newpp->inflip = lastflip;
		newpp->outseg = k;
		newpp->outhit = segp->seg_out;
		append_partition_pp(partitions, ipartition, id, start_index + ipartition[id], &tail_pp);
		ipartition[id]++;
	    }
	}
    }

    if (ipartition[id] > 0) {
	/* Store the head index of the first partition in this ray */
	head_partition[id] = head_pp;
    }
}

/**
 * Produce representations of all postfix bool trees in the regions.
 * Debug function - use when running opencl on the CPU
 */
void
pr_all_rtrees(const int total_regions, global struct bool_region *bregions, global union tree_rpn *rtree)
{
    for (uint j = 0; j < total_regions; j++) {
        printf("\npostfix: ");
        for (uint i=bregions[j].rtree_offset; i< bregions[j].rtree_offset + bregions[j].reg_nrtree; i++) {
	    switch (rtree[i].uop) {
		case UOP_NOP:
		printf("NOP");
		break;

		case UOP_UNION:
		printf("U");
		break;
		case UOP_INTERSECT:
		printf("+");
		break;
		case UOP_SUBTRACT:
		printf("-");
		break;
		case UOP_XOR:
		printf("XOR");
		break;

		default:
		printf("%ld", rtree[i].st_bit);
		break;
	    }
	    if (i != bregions[j].rtree_offset + bregions[j].reg_nrtree-1)
		printf(" ");
        }
        printf("\n");
    }
    printf("\n\n");
}

int
bool_eval(global struct partition *partitions, global uint *ipartition, RESULT_TYPE segs,
        global uint *h, global uint *segs_bv, const uint bv_index, uint offset, size_t id,
        global struct bool_region *bregions, global union tree_rpn *rtree, const uint region_index)
{
    uchar stack[BOOL_STACKSIZE];

    size_t stackend;
    uchar a, b, ret;

    stack[0] = 0;
    stackend = 0;

    for (uint i = bregions[region_index].rtree_offset; i < bregions[region_index].rtree_offset + bregions[region_index].reg_nrtree; i++) {
	if (stackend >= BOOL_STACKSIZE)
	    return -1;

        switch (rtree[i].uop) {
            case UOP_NOP:
            stack[stackend++] = 0;
            break;

            case UOP_UNION:
            b = stack[--stackend];
            a = stack[--stackend];
            stack[stackend++] = (a || b);
            break;
            case UOP_INTERSECT:
            b = stack[--stackend];
            a = stack[--stackend];
            stack[stackend++] = (a && b);
            break;
            case UOP_SUBTRACT:
            b = stack[--stackend];
            a = stack[--stackend];
            stack[stackend++] = (a && !b);
            break;
            case UOP_XOR:
            b = stack[--stackend];
            a = stack[--stackend];
            stack[stackend++] = (a ^ b);
            break;

            default:
	    {
		const long st_bit = rtree[i].st_bit;
		global struct partition *pp;
		RESULT_TYPE segp;
		ret = 0;

		pp = &partitions[offset];
		//iterate over segments of partition
		for (uint i = 0; i < bv_index; i++) {
		    uint mask = segs_bv[offset * bv_index + i];
		    while (mask != 0) {
			uint lz = clz(mask);
			uint k = h[id] + (31 - lz);
			if (isset(segs_bv, offset * bv_index, k - h[id]) != 0) {
			    RESULT_TYPE segp = segs+k;

			    if (segp->seg_sti == st_bit) {
				ret = 1;
				break;
			    }
			}
			// clear bit in mask
			mask &= ~(1 << (31 - lz));
		    }
                    if (ret) break;
		}
		stack[stackend++] = ret;
		break;
            }
        }
    }

    return stack[0];
}

/**
 * For each segment's solid that lies in this partition, add
 * the list of regions that refer to that solid into the
 * "regiontable" bitarray.
 */
void
build_regiontable(global uint *regions_table, RESULT_TYPE segs,
        global uint *segs_bv, global uint *regiontable, const uint pp_idx, const uint seg_idx,
        const uint bv_index, const uint rt_index, const size_t id)
{
    RESULT_TYPE segp;

    /* Iterate over segments of partition */
    for (uint i = 0; i < bv_index; i++) {
	uint mask = segs_bv[pp_idx * bv_index + i];
	while (mask != 0) {
            uint lz = clz(mask);
	    uint k = seg_idx + (31 - lz);
	    if (isset(segs_bv, pp_idx * bv_index, k - seg_idx) != 0) {
		segp = segs+k;

		/* Search for all regions involved in this partition */
		for (uint m = 0; m < rt_index; m++) {
		    regiontable[id * rt_index + m] |= regions_table[segp->seg_sti * rt_index + m];
		}
	    }
	    // clear bit in mask
	    mask &= ~(1 << (31 - lz));
	}
    }
}

void
reset_regiontable(global uint *regiontable, const size_t id, const uint rt_index)
{
    for (uint k = 0; k < rt_index; k++) {
        regiontable[id * rt_index + k] = 0;
    }
}

int
rt_defoverlap(global struct partition *partitions, const uint pp_idx, global struct bool_region *reg1,
	      const uint reg1_id, global struct bool_region *reg2, const uint reg2_id, const uint headpp_idx)
{

    global struct partition *pp;

    /*
     * Apply heuristics as to which region should claim partition.
     */
    if (reg1->reg_aircode != 0) {
	/* reg1 was air, replace with reg2 */
	return 2;
    }

    pp = &partitions[pp_idx];
    if (pp->back_pp != headpp_idx) {
	if (partitions[pp->back_pp].region_id == reg1_id)
	    return 1;
	if (partitions[pp->back_pp].region_id == reg2_id)
	    return 2;
    }

    /* To provide some consistency from ray to ray, use lowest bit # */
    if (reg1->reg_bit < reg2->reg_bit)
	return 1;
    return 2;
}

void
rt_default_multioverlap(global struct partition *partitions, global struct bool_region *bregions, global uint *regiontable,
			const uint pp_idx, const uint total_regions, const uint headpp_idx, const size_t id)
{
    global struct bool_region *lastregion;
    int code, ret;

    uint rt_index = total_regions/32 +1;
    uint lastregion_id;

    // Get first region of the regiontable
    for (uint i = 0; i < rt_index; i++) {
	uint mask = regiontable[id * rt_index + i];
	ret = 0;
	while (mask != 0) {
	    uint lz = clz(mask);
	    uint k = (i * 32) + (31 - lz);
	    if (isset(regiontable, id * rt_index, k) != 0) {
		lastregion = &bregions[k];
		lastregion_id = k;
		ret = 1;
		break;
	    }
	    // clear bit in mask
	    mask &= ~(1 << (31 - lz));
	}
	if (ret) break;
    }

    /* Examine the overlapping regions, pairwise */
    for (uint i = 0; i < rt_index; i++) {
	uint mask = regiontable[id * rt_index + i];
	while (mask != 0) {
	    uint lz = clz(mask);
	    uint k = (i * 32) + (31 - lz);
	    if (k != lastregion_id && isset(regiontable, id * rt_index, k) != 0) {
		global struct bool_region *regp;
		regp = &bregions[k];

		code = -1;
		/*
		 * Two or more regions claim this partition
		 */
		if (lastregion->reg_aircode != 0 && regp->reg_aircode == 0) {
		    /* last region is air, replace with solid regp */
		    code = 2;
		} else if (lastregion->reg_aircode == 0 && regp->reg_aircode != 0) {
		    /* last region solid, regp is air, keep last */
		    code = 1;
		} else if (lastregion->reg_aircode != 0 &&
			   regp->reg_aircode != 0 &&
			   regp->reg_aircode == lastregion->reg_aircode) {
		    /* both are same air, keep last */
		    code = 1;
		} else {
		    /*
		     * Hand overlap to old-style application-specific
		     * overlap handler, or default.
		     * 0 = destroy partition,
		     * 1 = keep part, claiming region=lastregion
		     * 2 = keep part, claiming region=regp
		     */
		    code = rt_defoverlap(partitions, pp_idx, lastregion, lastregion_id, regp, k, headpp_idx);
		}

		/* Implement the policy in "code" */
		switch (code) {
		    case 0:
			/*
			 * Destroy the whole partition.
			 * Reset regiontable
			 */
			reset_regiontable(regiontable, id, rt_index);
			return;
		    case 1:
			/* Keep partition, claiming region = lastregion */
			clr(regiontable, id * rt_index, k);
			break;
		    case 2:
			/* Keep partition, claiming region = regp */
			clr(regiontable, id * rt_index, lastregion_id);
			lastregion = regp;
			lastregion_id = k;
			break;
		}
	    }
	    // clear bit in mask
	    mask &= ~(1 << (31 - lz));
	}
    }
}

__kernel void
rt_boolfinal(global struct partition *partitions, global uint *ipartition, RESULT_TYPE segs,
        global uint *h, global uint *segs_bv, const int max_depth,
        global struct bool_region *bregions, const uint total_regions, global union tree_rpn *rtree,
        global uint *regiontable, const int cur_pixel, const int last_pixel,
        global uint *regions_table, const uint regions_table_size, global uint *head_partition)
{
    const size_t id = get_global_size(0)*get_global_id(1)+get_global_id(0);

    if (id >= (last_pixel-cur_pixel+1))
	return;

    uint head;
    uint lastregion_idx;
    int claiming_regions;
    double diff;
    uint bv_index = max_depth/32 + 1;
    uint rt_index = total_regions/32 +1;

    uint lastpp_eval_idx = UINT_MAX;

    //No partitions
    if (head_partition[id] == UINT_MAX) {
	return;
    }

    //Get first partition of the ray
    head = head_partition[id];
    head_partition[id] = UINT_MAX;

    //iterate over partitions
    for (uint current_index = head; current_index != UINT_MAX; current_index = partitions[current_index].forw_pp) {
	global struct partition *pp = &partitions[current_index];

	claiming_regions = 0;
	/* Force "very thin" partitions to have exactly zero thickness. */
	if (NEAR_EQUAL(pp->inhit.hit_dist, pp->outhit.hit_dist, rti_tol_dist)) {
	    pp->outhit.hit_dist = pp->inhit.hit_dist;
	}

	/* Sanity checks on sorting. */
	if (pp->inhit.hit_dist > pp->outhit.hit_dist) {
	    /* Inverted partition */
	    return;
	}

	if (pp->forw_pp != UINT_MAX) {
	    diff = pp->outhit.hit_dist - partitions[pp->forw_pp].inhit.hit_dist;
	    if (!ZERO(diff)) {
		if (NEAR_ZERO(diff, rti_tol_dist)) {
		    /* Fusing 2 partitions */
		    partitions[pp->forw_pp].inhit.hit_dist = pp->outhit.hit_dist;
		} else if (diff > 0) {
		    /* Sorting defect */
		    return;
		}
	    }
	}

	/* Start with a clean state when evaluating this partition */
	reset_regiontable(regiontable, id, rt_index);

	/*
	 * Build regiontable bitarray of all the regions involved in this
	 * partitions to later evaluate the partitions against the involved
	 * regions and to resolve any overlap that may occur
	 */
	build_regiontable(regions_table, segs, segs_bv, regiontable, current_index, h[id], bv_index, rt_index, id);

	/* Evaluate the boolean trees of any regions involved */
	for (uint i = 0; i < rt_index; i++) {
	    uint mask = regiontable[id * rt_index + i];
	    while (mask != 0) {
		uint lz = clz(mask);
		uint k = (i * 32) + (31 - lz);
		if (isset(regiontable, id * rt_index, k) != 0) {

		    if (bregions[k].reg_all_unions) {
			claiming_regions++;
			lastregion_idx = k;
		    }

		    else if (bool_eval(partitions, ipartition, segs, h, segs_bv, bv_index, current_index, id, bregions, rtree, k) == BOOL_TRUE) {
			/* This region claims partition */
			claiming_regions++;
			lastregion_idx = k;
		    }
		}
		// clear bit in mask
		mask &= ~(1 << (31 - lz));
	    }
	}

	if (claiming_regions == 0)
	    continue;

	if (claiming_regions > 1) {
	    /* There is an overlap between two or more regions */
	    rt_default_multioverlap(partitions, bregions, regiontable, current_index, total_regions, head, id);

	    /* Count number of remaining regions, s/b 0 or 1 */
	    claiming_regions = 0;
	    for (uint i = 0; i < rt_index; i++) {
		uint mask = regiontable[id * rt_index + i];
		while (mask != 0) {
		    uint lz = clz(mask);
		    uint k = (i * 32) + (31 - lz);

		    if (isset(regiontable, id * rt_index, k) != 0) {
			claiming_regions++;
			lastregion_idx = k;
		    }
		    // clear bit in mask
		    mask &= ~(1 << (31 - lz));
		}
	    }

	    /* If claiming_regions != 1, discard partition. */
	    if (claiming_regions != 1)
		continue;
	}

	/*
	 * claiming_regions == 1
	 *
	 * Partition evaluated
	 */
	{
	    global struct partition *lastpp;

            if (head_partition[id] == UINT_MAX) {
                /* First partition evaluated for this ray
                 * Start shading at this partition index
                 */
                head_partition[id] = current_index;
            }

	    /* Record the "owning" region. */
	    pp->region_id = lastregion_idx;

	    /* See if this new partition extends the previous last
	     * partition, "exactly" matching.
	     */
	    if (lastpp_eval_idx != UINT_MAX) {
		/* there is one last partition evaluated for this ray */
		lastpp = &partitions[lastpp_eval_idx];
                lastpp->forw_pp = current_index;
	    }

	    if (lastpp_eval_idx != UINT_MAX && lastregion_idx == lastpp->region_id &&
		    NEAR_EQUAL(pp->inhit.hit_dist, lastpp->outhit.hit_dist, rti_tol_dist)) {
		/* same region, merge by extending last final partition */
		lastpp->outhit = pp->outhit;
		lastpp->outseg = pp->outseg;
		lastpp->outflip = pp->outflip;

		/* Don't lose the fact that the two solids of this
		 * partition contributed.
		 */
		set(segs_bv, lastpp_eval_idx + (bv_index - 1), pp->inseg - h[id]);
		set(segs_bv, lastpp_eval_idx + (bv_index - 1), pp->outseg - h[id]);
	    } else {
		lastpp_eval_idx = current_index;
	    }
	}
    }
}

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
