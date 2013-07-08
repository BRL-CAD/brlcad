
#ifndef SESSION_INSTANCE_H
#define SESSION_INSTANCE_H 1

#include <sc_export.h>
//#include <sdai.h>

class SC_DAI_EXPORT SDAI_Session_instance : public SDAI_sdaiObject {

    public:
        int x;

        SDAI_Session_instance();
        virtual ~SDAI_Session_instance();
};

typedef SDAI_Session_instance * SDAI_Session_instance_ptr;
typedef SDAI_Session_instance_ptr SDAI_Session_instance_var;

// the old names
//typedef SDAI_Session_instance SDAI_SessionInstance;
//typedef SDAI_Session_instance_ptr SDAI_SessionInstanceH;

#endif
