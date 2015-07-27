#include "common.h"

#include <string.h> /* for memset */

#include "vmath.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "bn/mat.h"
#include "raytrace.h"
#include "analyze.h"

/* Note that we are only looking for gaps within the solid partitions of p - curr_comb is the
 * context we are examining for subtractions, and impacts beyond it are of no concern here.*/
HIDDEN int
find_missing_gaps(struct bu_ptbl *UNUSED(missing), struct bu_ptbl *UNUSED(p_orig), struct bu_ptbl *UNUSED(p), int UNUSED(max_cnt))
{
//1. Set up pointer arrays for both partition lists
//
//2. Iterate over the tbls and assign each array it's minimal partition pointer
//   based on the index stored in the minimal_partitions struct.
//
//3. Look for pointers that are NULL in p_orig array but non-null in p array -
//   that indicates removed material. For rays that have minimal_partitions in
//   both arrays, compare their gaps.  If there is a p_orig gap that exits the
//   gap at less than the smallest hit distance in p->hits or enters a gap at
//   greater than the max in p->hits, it is out of scope.  Otherwise, look for
//   it in the p->gaps array.  If it isn't there, it constitutes a missing
//   gap.  A new minimal_partitions struct is created and all gaps not found
//   are recorded in it (along with the ray information and index) and the
//   process is repeated for all rays.
//
//4. return the length of the missing bu_ptbl.
    return 0;
}

/* Pass in the parent brep rt prep and non-finalized comb prep to avoid doing them multiple times.
 * */
extern "C" int
analyze_find_subtracted(struct bu_ptbl *UNUSED(results), struct db_i *UNUSED(dbip), const char *UNUSED(pbrep), struct rt_gen_worker_vars *UNUSED(pbrep_rtvars), struct bu_vls *UNUSED(curr_comb), struct rt_gen_worker_vars *UNUSED(ccomb_rtvars), struct bu_ptbl *UNUSED(candidates), void *data)
{
    struct subbrep_object_data *curr_union_data = (struct subbrep_object_data *)data;

    if (!curr_union_data) return 0;


    // For each candidate:
    //
    // 1. Get a bbox from the faces.  Construct the rays to shoot from that
    // bbox.
    //
    // Note - remember step has to be reset for ALL of the rt_gen_worker_vars (original brep, control as well as canddiate) each time new rays are created (and any other resets...)
    //
    // 2. For the original brep and curr_comb (which are already prepped) shoot
    // the rays from the candidate.
    // struct bu_ptbl o_brep_results = BU_PTBL_INIT_ZERO;
    // struct bu_ptbl curr_comb_results = BU_PTBL_INIT_ZERO;
    //
    // analyze_get_solid_partitions(&o_brep_results, pbrep_rtvars, candidate_rays, ray_cnt, dbip, bu_vls_addr(&pbrep));
    // analyze_get_solid_partitions(&curr_comb_results, ccomb_rtvars, candidate_rays, ray_cnt, dbip, bu_vls_addr(&pbrep));
    //
    // 3. Compare the two partition/gap sets that result.
    //
    // int missing_gaps = find_missing_gaps(o_brep_results, curr_comb_results, ray_cnt);
    //
    // 4.  If there are missing gaps in curr_comb, prep candidate and
    // shoot the same rays against it.  If candidate 1) removes either all or a
    // subset of the missing material and 2) does not remove material that is
    // present in the results from the original brep, it is subtracted.  Add it
    // to the results set.
    //
    // Once all candidates are processed, return the BU_PTBL_LEN of results.
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
