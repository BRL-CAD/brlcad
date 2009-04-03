
#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#include <dpmenuitem.h>

void 
DPMenuItem::Do() 
{
    (owner->*func)();
}
