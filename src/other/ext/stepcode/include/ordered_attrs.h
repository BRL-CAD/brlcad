
#include <express/entity.h>
#include <express/variable.h>
#include "sc_export.h"

#ifdef __cplusplus

#include <vector>

extern "C" {
#endif /* __cplusplus */

typedef struct {
    Variable attr;
    Entity creator, redefiner, deriver;
} orderedAttr;

/**set the entity we're working on, init working variables */
extern SC_EXPRESS_EXPORT void orderedAttrsInit( Entity e );

/**free memory */
extern SC_EXPRESS_EXPORT void orderedAttrsCleanup();

/**get next attr; not thread safe (as if the rest of libexpress is) */
extern SC_EXPRESS_EXPORT const orderedAttr * nextAttr();
#ifdef __cplusplus
}

typedef std::vector<orderedAttr * > oaList;
oaList getAttrList();

#endif /* __cplusplus */
