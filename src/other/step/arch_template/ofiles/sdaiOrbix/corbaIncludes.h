//#ifndef _ODI_OSSG_
//#endif

#ifdef PART26

#include "/proj/pdevel/scl3-1/arch-gnu-solaris/ofiles/sdaiOrbix/sdaitypes.hh"
#include "/proj/pdevel/scl3-1/arch-gnu-solaris/ofiles/sdaiOrbix/appinstance.hh"

//#define EXTRA_CORBA_PARAMS , CORBA::Environment &IT_env=CORBA::IT_chooseDefaultEnv ()
//#define EXTRA_CORBA_PARAMS , CORBA::Environment &IT_env=CORBA::default_environment
//#define CORBA_THROW throw (CORBA::SystemException);

#else

//#define EXTRA_CORBA_PARAMS 

//#define CORBA_THROW 

#endif
