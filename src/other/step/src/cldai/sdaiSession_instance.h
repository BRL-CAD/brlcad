
#ifndef SESSION_INSTANCE_H
#define SESSION_INSTANCE_H 1

//#include <sdai.h>

class SCLP23_NAME(Session_instance) : public SCLP23_NAME(sdaiObject) 
{

  public:
    int x;

    SCLP23_NAME(Session_instance)();
    virtual ~SCLP23_NAME(Session_instance)();
//    SCLP23_NAME(Session_instance)(const SCLP23_NAME(Session_instance)& si) {}

//    static SCLP23_NAME(Session_instance_ptr) 
//	_duplicate(SCLP23_NAME(Session_instance_ptr) sip);
//    static SCLP23_NAME(Session_instance_ptr) 
//	_narrow(SCLP23_NAME(Object_ptr) o);
//    static SCLP23_NAME(Session_instance_ptr) _nil();

};

typedef SCLP23_NAME(Session_instance)* SCLP23_NAME(Session_instance_ptr);
typedef SCLP23_NAME(Session_instance_ptr) SCLP23_NAME(Session_instance_var);

// the old names
//typedef SCLP23_NAME(Session_instance) SCLP23_NAME(SessionInstance);
//typedef SCLP23_NAME(Session_instance_ptr) SCLP23_NAME(SessionInstanceH);

#endif
