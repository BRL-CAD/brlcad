#ifdef __O3DB__
#include <OpenOODB.h>
#endif

// define this to be the name of the display window object for 
// STEP entity instance editing or define your own.

class StepEntityEditor
{
  public:
    StepEntityEditor() {};
    ~StepEntityEditor() {};
};

extern void DeleteSEE(StepEntityEditor *se);
