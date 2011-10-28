
#include <dpmenuitem.h>

void 
DPMenuItem::Do() 
{
    (owner->*func)();
}
