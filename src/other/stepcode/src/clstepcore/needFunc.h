#include <scl_export.h>

// define this to be the name of the display window object for
// STEP entity instance editing or define your own.

class SCL_CORE_EXPORT StepEntityEditor {
    public:
        StepEntityEditor() {};
        ~StepEntityEditor() {};
};

extern void DeleteSEE( StepEntityEditor * se );
