/*             D B L _ T O _ F R O M _ L O N G _ T E S T . C X X
 * BRL-CAD
 *
 * Published in 2022 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file dbl_to_int_test.cxx
 *
 * This is an experiment testing a way to detect, for a given floating point
 * number, the maximum number of digits we can save if we convert it to a long.
 * We "snap" from floating point to integer space when we need certain
 * categories of math operations to be robust (floating point types have some
 * nasty and subtle limits in that respect.)
 *
 * The idea is that for any given geometry, its vertex coordinates will be part
 * of a small subset of the total floating point space, so if we find the
 * number in the set that bounds our upper limit on how large a factor we can
 * use without overflowing when we convert it to integer, using that factor
 * will result in less information loss when we transition from integer back
 * into floating point - i.e., we'll be saving as much info as the constraints
 * of the integer container size and input sizes allow.  (Clipper uses a
 * snapping technique for robust polygon boolean operations, but with a fixed
 * scale factor.)
 *
 * Here we're just testing a single number, but the idea would be to apply this
 * individually to each number in a set, and find the smallest viable scale
 * factor to apply to that set.  This way we would lose at little information
 * as possible due to the type conversion.
 */

#include "common.h"
#include <limits>
#include <iomanip>
#include <iostream>
#include <math.h>
#ifdef BUILD_BRLCAD
#include "bu/app.h"
#endif

int main(int ac, const char **av)
{
#ifdef BUILD_BRLCAD
    bu_setprogname(av[0]);
#endif
    long log2max = log2(std::numeric_limits<long>::max());
    std::cout << "log2 of long max: " << log2max << "\n";
    double test_num = -23423423.234211111;
    std::cout << "test number: " << std::setprecision(15) << test_num << "\n";
    long log2testnum = (long)log2(fabs(test_num));
    std::cout << "log2 of fabs of test number: " << log2testnum << "\n";
    long log2_delta = log2max - log2testnum - 1;
    std::cout << "Difference between test num log2 and max limit log2: " << log2_delta << "\n";
    double scale = pow(2, log2_delta);
    std::cout << "Multiplicative scalar (pow(2, log2_diff)): " << std::setprecision(15) << scale << "\n";
    long long_store = static_cast<long>(test_num * scale);
    std::cout << "Long conversion of test num: " << long_store << "\n";
    double dbl_restore = static_cast<double>(long_store / scale);
    std::cout << "Conversion of long back to double: " << std::setprecision(15) << dbl_restore << "\n";
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
