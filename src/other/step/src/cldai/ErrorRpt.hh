//
//  File: ErrorRpt.hh
//
//  Description: Declaration of global error reporting functions
//
//  Rev:         $Revision: 1.1 $
//  Created:     $Date: 1998/01/09 21:39:30 $
//  Author:      $Author: sauderd $
//
//  Copyright Industrial Technology Institute 1997 -- All Rights Reserved
//

#ifndef ERRORRPT_HH
#define ERRORRPT_HH

#include <sdai.h>
#include "ErrorMap.hh"

void reportServerError(ServerError_id error_id);

void reportObjectStoreError(const char * message);

void reportSDAIError(SCLP23(Error_id) error_id);

#endif
