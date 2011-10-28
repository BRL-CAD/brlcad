#include <sdai.h>

SCLP23(sdaiObject)::SCLP23_NAME(sdaiObject)()
{
}

SCLP23(sdaiObject)::~SCLP23_NAME(sdaiObject)()
{
}

#ifdef PART26
//SCLP26(Application_instance_ptr) 
IDL_Application_instance_ptr 
SCLP23(sdaiObject)::create_TIE() 
{ 
    cout << "ERROR sdaiObject::create_TIE() called." << endl;  
    return 0;
}
#endif

