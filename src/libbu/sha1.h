/* ================ sha1.h ================ */
/*
  SHA-1 in C
  By Steve Reid <steve@edmweb.com>
  100% Public Domain
*/

#include "common.h"

#define SHA1_BLOCK_SIZE 64

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[SHA1_BLOCK_SIZE];
} SHA1_CTX;

__BEGIN_DECLS

void SHA1Transform(uint32_t state[5], const unsigned char buffer[SHA1_BLOCK_SIZE]);
void SHA1Init(SHA1_CTX* context);
#if defined(__GNUC__) && !defined(__clang__)
/* GCC's PGO inliner can misdiagnose short padding inputs as full blocks. */
__attribute__((noinline))
#endif
void SHA1Update(SHA1_CTX* context, const unsigned char* data, uint32_t len);
void SHA1Final(unsigned char digest[20], SHA1_CTX* context);

__END_DECLS

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
