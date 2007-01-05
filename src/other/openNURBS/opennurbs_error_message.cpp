/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"


void ON_ErrorMessage(
        int message_type, // 0=warning - serious problem that code is designed to handle
                          // 1=error - serious problem code will attempt to handle
                          //           The thing causing the error is a bug that must
                          //           be fixed.
                          // 2=assert failed - crash is nearly certain
        const char* sErrorMessage 
        )
{
  // error/warning/assert message is in sMessage[] buffer.  Modify this function
  // to do whatever you want to with the message.
  if ( sErrorMessage && sErrorMessage[0] ) 
  {
#if defined(ON_DEBUG)
    printf("\n%s\n",sErrorMessage);
#if defined(ON_OS_WINDOWS)
    ::OutputDebugStringA( "\n" );
    ::OutputDebugStringA( sErrorMessage );
    ::OutputDebugStringA( "\n\n" );
#if defined(ON_ERROR_USE_MESSAGE_BOX) && defined(MessageBox)
    if ( message_type >= 2 ) {
      // in your face message box while debugging
      strcat( sMessage, "\n\nOK = continue  CANCEL = exit" );
      if ( IDCANCEL == MessageBoxA( NULL, sMessage, "openNURBS ON_Assert FAILED", 
                                    MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TASKMODAL | MB_DEFBUTTON1) ) {
        exit(1);
      }
    }
#endif
#endif
#endif
  }
}
