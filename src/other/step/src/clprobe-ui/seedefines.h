#ifndef seedefines_h
#define seedefines_h

/*
* NIST Data Probe Class Library
* clprobe-ui/seedefines.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: seedefines.h,v 3.0.1.1 1997/11/05 23:01:08 sauderd DP3.1 $ */ 

#define SEE_SAVE_COMPLETE 1001
#define SEE_SAVE_INCOMPLETE 1002
#define SEE_CANCEL 1003
#define SEE_DELETE 1004
#define SEE_REPLICATE 1005
#define SEE_EDIT 1006
#define SEE_MARK 1007
#define SEE_LIST 1008
#define SEE_MODIFY 1009
#define SEE_VIEW 1010
#define SEE_ATTR_UNDO 1011
#define SEE_PIN 1012
#define SEE_NOT_PIN 1013
#define SEE_OPEN_SED 1016
#define SEE_ATTR_INFO 1017

#define SEE_NEXT 1014
#define SEE_PREV 1015

#define CONTROL_A ('\001')
#define CONTROL_C ('\003')
#define CONTROL_D ('\004')
#define CONTROL_E ('\005')
#define CONTROL_I ('\011')
#define CONTROL_L ('\f')
#define CONTROL_M ('\r')
#define CONTROL_N ('\016')
#define CARRAIGE_RETURN ('\r')
#define CONTROL_P ('\020')
#define CONTROL_R ('\022')
#define CONTROL_S ('\023')
#define CONTROL_T ('\024')
#define CONTROL_U ('\025')
#define CONTROL_X ('\030')

	// single terminating characters for see editors
static const char *seeTermChars = "\r\016\020\021";

#endif
