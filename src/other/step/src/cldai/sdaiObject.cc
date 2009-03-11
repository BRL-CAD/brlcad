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

#ifdef __OSTORE__
void 
SCLP23(sdaiObject)::Access_hook_in(void *object, 
	enum os_access_reason reason, void *user_data, 
	void *start_range, void *end_range)
{
    cout << "****WARNING: virtual sdaiObject::Access_hook_in() called" << endl;
    SCLP23(sdaiObject_ptr) ent = (SCLP23(sdaiObject_ptr))object;
/*
    ent->Access_hook_in(object, reason, user_data, start_range, end_range);
*/
}
#endif
