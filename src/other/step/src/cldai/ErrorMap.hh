//
//  File: ErrorMap.hh
//
//  Description: Declaration of class ErrorMap. ErrorMap is used to 
//               map internal server errors, which include Part 23
//               binding errors, into Part26 binding errors and error
//               description strings. 
//
//  Rev:         $Revision: 1.1 $
//  Created:     $Date: 1998/01/09 21:39:30 $
//  Author:      $Author: sauderd $
//
//  Copyright Industrial Technology Institute 1997 -- All Rights Reserved
//

#ifndef ERRORMAP_HH
#define ERRORMAP_HH

#include <sdai.h>

enum ServerError_id
{
  serverNoError                      = 0,
  serverNoMemoryError                = 1,
  serverTypeConversionError          = 2,
  serverTimeFormatError              = 3,
  serverBadTransactionPointer        = 4,
  serverBadDatabasePointer           = 5,
  serverBadSessionPointer            = 6,
  serverBadRepositoryContentsPointer = 7,
  serverBadRepositoryPointer         = 8,
  serverBadRepositorySetPointer      = 9,
  serverBadModelSetPointer           = 10,
  serverBadModelPointer              = 11,
  serverBadPIDPointer                = 12,
  serverBadSchemaSetPointer          = 13,
  serverBadSessionSetPointer         = 14,
  serverBadPointer                   = 15,
  serverUnknownRepoType              = 16,
  serverObjectStoreError             = 17,
  serverUnknownError                 = 18
};

class ErrorMap
{
  public:

    static long serverErrorCode(ServerError_id);
    static char * serverErrorDescription(ServerError_id);

    static long sdaiErrorCode(SCLP23(Error_id));
    static char * sdaiErrorDescription(SCLP23(Error_id));

  private:

    static char * ServerErrorText[];
    static char * SdaiErrorText[];
};

#endif
