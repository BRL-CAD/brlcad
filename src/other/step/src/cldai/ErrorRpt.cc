//
//  File: ErrorRpt.cc
//
//  Description: Implementation of global error reporting functions 
//
//  Rev:         $Revision: 1.1 $
//  Created:     $Date: 1998/01/09 21:39:30 $
//  Author:      $Author: sauderd $
//
//  Copyright Industrial Technology Institute 1997 -- All Rights Reserved
//

#include <iostream.h>

#include <sdai.h>

//#include "sdaiimp.hh"
#include "ErrorMap.hh"
#include "ErrorRpt.hh"
//#include "SessionMgr.hh"

//extern SessionManager SessionMgr;

//
//  Report a server error
//
//    A server error is any error that is not an SDAI error
//
//    The error is mapped into a Part 26 SDAI error.
//
void reportServerError(ServerError_id error_id)
{
  long error_code = 0;
/*
  SCLP23(Session_ptr) session = SessionMgr.active_session_();


  if (session != SCLP23(Session)::_nil())
    {
      char * function_id = SessionMgr.function_id_();

      error_code = ErrorMap::serverErrorCode(error_id);

      char * description = ErrorMap::serverErrorDescription(error_id);

      session->recordError(function_id, error_code, description);
    }
  else // there is no active session for this user
    {
      error_code = ErrorMap::sdaiErrorCode(SCLP23(sdaiSS_NOPN));
    }

*/
  throw SCLP22(SDAIException)(error_code);
}

//
//  Report an ObjectStore error
//
//    The error is mapped into a Part 26 SDAI error.
//
void reportObjectStoreError(const char * message)
{
  long error_code = 0;

/*
  SCLP23(Session_ptr) session = SessionMgr.active_session_();

  long error_code;

  if (session != SCLP23(Session)::_nil())
    {
      char * function_id = SessionMgr.function_id_();

      error_code = ErrorMap::serverErrorCode(serverObjectStoreError);

      char * header = "ObjectStore Error: ";

      char * description = new char [strlen(header)+ strlen(message)+1];

      if (description != NULL)
        {
          sprintf(description, "%s%s", header, message);
          session->recordError(function_id, error_code, description);
        }
      else
        {
          reportServerError(serverNoMemoryError);
        }
    }
  else // there is no active session for this user
    {
      error_code = ErrorMap::sdaiErrorCode(SCLP23(sdaiSS_NOPN));
    }
*/
  throw SCLP22(SDAIException)(error_code);
}


//
//  Report an SDAI error
//
//    Part 23 binding errors are mapped into Part 26 binding errors
//
void reportSDAIError(SCLP23(Error_id) error_id)
{
  long error_code = 0;
/*
  SCLP23(Session_ptr) session = SessionMgr.active_session_();

  long error_code;

  if (session != SCLP23(Session)::_nil())
    {
      char * function_id = SessionMgr.function_id_();

      error_code = ErrorMap::sdaiErrorCode(error_id);

      char * description = ErrorMap::sdaiErrorDescription(error_id);

      cout << "Error description: " << description << endl;

      session->recordError(function_id, error_code, description);
    }
  else // there is no active session for this user
    {
      error_code = ErrorMap::sdaiErrorCode(SCLP23(sdaiSS_NOPN));
    }
*/
  throw SCLP22(SDAIException)(error_code);
}
