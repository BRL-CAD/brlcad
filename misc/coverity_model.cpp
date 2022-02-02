/* This file defines a Coverity Model used to avoid false positives
 * in analysis reporting.  See https://scan.coverity.com/tune for
 * more information. */

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


/* BRL-CAD doesn't implement cryptographic functions for security
 * purposes - don't DC.WEAK_CRYPTO flag the random functions */

int rand(void) {
  /* ignore */
}

double drand48(void) {
  /* ignore */
}

