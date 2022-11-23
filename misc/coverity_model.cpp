/*              C O V E R I T Y _ M O D E L . C P P
 * BRL-CAD
 *
 * Published in 2022 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file coverity_model.cpp
 *
 * This file defines a Coverity Model used to avoid false positives in analysis
 * reporting.  See https://scan.coverity.com/tune for more information.
 */

typedef unsigned int uint32_t;

void bu_bomb(const char* msg) {
    __coverity_panic__();
}

void bu_exit(int status, const char *fmt, ...) {
    __coverity_panic__();
}

void bu_badmagic(const uint32_t *ptr, uint32_t magic, const char *str, const char *file, int line) {
    __coverity_panic__();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
