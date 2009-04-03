//
//  File: ErrorMap.cc
//
//  Description: Definitions for members of class ErrorMap
//
//  Rev:         $Revision: 1.1 $
//  Created:     $Date: 1998/01/09 21:39:30 $
//  Author:      $Author: sauderd $
//
//  Copyright Industrial Technology Institute 1997 -- All Rights Reserved
//

#include <sdai.h>
#include "ErrorMap.hh"

char * ErrorMap::ServerErrorText[] =
{
  "Server Error: no error",
  "Server Error: out of dynamic memory",
  "Server Error: type conversion error",
  "Server Error: event time format error",
  "Server Error: bad pointer to transaction encountered",
  "Server Error: bad pointer to database encountered",
  "Server Error: bad pointer to session encountered",
  "Server Error: bad pointer to repository contents encountered",
  "Server Error: bad pointer to repository encountered",
  "Server Error: bad pointer to set of repositories encountered",
  "Server Error: bad pointer to set of models encountered",
  "Server Error: bad pointer to model encountered",
  "Server Error: bad pointer to PID encountered",
  "Server Error: bad pointer to set of schema instances encountered",
  "Server Error: bad pointer to set of sessions encountered",
  "Server Error: bad pointer encountered",
  "Server Error: Unknown repository type",
  "Server Error: ObjectStore error",
  "Server Error: Unknown error"
};

char * ErrorMap::SdaiErrorText[] =
{
  "SDAI Error: (sdaiNO_ERR) No error", 
  "SDAI Error: (sdaiSS_OPN) Session is already open",
  "SDAI Error: (sdaiSS_NAVL) Session is not available",
  "SDAI Error: (sdaiSS_NOPN) Session is not open",
  "SDAI Error: (sdaiRP_NEXS) Repository does not exist",
  "SDAI Error: (sdaiRP_NAVL) Repository not available",
  "SDAI Error: (sdaiRP_OPN) Repository already opened",
  "SDAI Error: (sdaiRP_NOPN) Repository is not open",
  "SDAI Error: (sdaiTR_EAB) Transaction ended abnormally so it no longer exists",
  "SDAI Error: (sdaiTR_EXS) Transaction already exists",
  "SDAI Error: (sdaiTR_NAVL) Transaction not available",
  "SDAI Error: (sdaiTR_RW) Transaction read-write",
  "SDAI Error: (sdaiTR_NRW) Transaction not read-write",
  "SDAI Error: (sdaiTR_NEXS) Transaction does not exist",
  "SDAI Error: (sdaiMO_NDEQ) SDAI-model not domain-equivalent ",
  "SDAI Error: (sdaiMO_NEXS) SDAI-model does not exist",
  "SDAI Error: (sdaiMO_NVLD) SDAI-model invalid",
  "SDAI Error: (sdaiMO_DUP) SDAI-model duplicate",
  "SDAI Error: (sdaiMX_NRW) SDAI-model access not read-write",
  "SDAI Error: (sdaiMX_NDEF) SDAI-model access is not defined",
  "SDAI Error: (sdaiMX_RW) SDAI-model access read-write",
  "SDAI Error: (sdaiMX_RO) SDAI-model access read-only",
  "SDAI Error: (sdaiSD_NDEF) Schema definition is not defined",
  "SDAI Error: (sdaiED_NDEF) Entity definition not defined",
  "SDAI Error: (sdaiED_NDEQ) Entity definition not domain equivalent",
  "SDAI Error: (sdaiED_NVLD) Entity definition invalid",
  "SDAI Error: (sdaiRU_NDEF) Rule not defined ",
  "SDAI Error: (sdaiEX_NSUP) Expression evaluation not supported ",
  "SDAI Error: (sdaiAT_NVLD) Attribute invalid",
  "SDAI Error: (sdaiAT_NDEF) Attribute not defined",
  "SDAI Error: (sdaiSI_DUP) Schema instance duplicate",
  "SDAI Error: (sdaiSI_NEXS) Schema instance does not exist",
  "SDAI Error: (sdaiEI_NEXS) Entity instance does not exist ",
  "SDAI Error: (sdaiEI_NAVL) Entity instance not available",
  "SDAI Error: (sdaiEI_NVLD) Entity instance invalid",
  "SDAI Error: (sdaiEI_NEXP) Entity instance not exported",
  "SDAI Error: (sdaiSC_NEXS) Scope does not exist ",
  "SDAI Error: (sdaiSC_EXS) Scope exists ",
  "SDAI Error: (sdaiAI_NEXS) Aggregate instance does not exist ",
  "SDAI Error: (sdaiAI_NVLD) Aggregate instance invalid ",
  "SDAI Error: (sdaiAI_NSET) Aggregate instance is empty ",
  "SDAI Error: (sdaiVA_NVLD) Value invalid",
  "SDAI Error: (sdaiVA_NEXS) Value does not exist",
  "SDAI Error: (sdaiVA_NSET) Value not set ",
  "SDAI Error: (sdaiVT_NVLD) Value type invalid",
  "SDAI Error: (sdaiIR_NEXS) Iterator does not exist ",
  "SDAI Error: (sdaiIR_NSET) Current member is not defined",
  "SDAI Error: (sdaiIX_NVLD) Index invalid",
  "SDAI Error: (sdaiER_NSET) Event recording not set",
  "SDAI Error: (sdaiOP_NVLD) Operator invalid ",
  "SDAI Error: (sdaiFN_NAVL) Function not available",
  "SDAI Error: (sdaiSY_ERR) Underlying system error "
};

//
//  Return the error code of the error reported to the client
//
long ErrorMap::serverErrorCode(ServerError_id error_id)
{
   switch (error_id)
    {
  case serverNoError: return (long) SCLP22(sdaiNO_ERR);
  case serverNoMemoryError: return (long) SCLP22(sdaiSY_ERR);
  case serverTimeFormatError: return (long) SCLP22(sdaiSY_ERR);
  case serverUnknownError: return (long) SCLP22(sdaiSY_ERR);
    default: return (long) SCLP22(sdaiSY_ERR);
    }
}

//
//  Return a pointer to the string describing the server error
//
char * ErrorMap::serverErrorDescription(ServerError_id error_id)
{
  if (error_id >= serverNoError  && error_id <= serverUnknownError)
    {
      return ServerErrorText[error_id];
    }
  else
    {
      return ServerErrorText[serverUnknownError];
    }
}

//
//  Return the error code of the error reported to the client
//
long ErrorMap::sdaiErrorCode(SCLP23(Error_id) error_id)
{
   switch (error_id)
    {
  case SCLP23(sdaiNO_ERR): return SCLP22(sdaiNO_ERR);
  case SCLP23(sdaiSS_OPN): return SCLP22(sdaiSS_OPN);
  case SCLP23(sdaiSS_NAVL): return SCLP22(sdaiSS_NAVL);
  case SCLP23(sdaiSS_NOPN): return SCLP22(sdaiSS_NOPN);
  case SCLP23(sdaiRP_NEXS): return SCLP22(sdaiRP_NEXS);
  case SCLP23(sdaiRP_NAVL): return SCLP22(sdaiRP_NAVL);
  case SCLP23(sdaiRP_OPN): return SCLP22(sdaiRP_OPN);
  case SCLP23(sdaiRP_NOPN): return SCLP22(sdaiRP_NOPN);
  case SCLP23(sdaiTR_EAB): return SCLP22(sdaiTR_EAB);
  case SCLP23(sdaiTR_EXS): return SCLP22(sdaiTR_EXS);
  case SCLP23(sdaiTR_NAVL): return SCLP22(sdaiTR_NAVL);
  case SCLP23(sdaiTR_RW): return SCLP22(sdaiTR_RW);
  case SCLP23(sdaiTR_NRW): return SCLP22(sdaiTR_NRW);
  case SCLP23(sdaiTR_NEXS): return SCLP22(sdaiTR_NEXS);
  case SCLP23(sdaiMO_NDEQ): return SCLP22(sdaiMO_NDEQ);
  case SCLP23(sdaiMO_NEXS): return SCLP22(sdaiMO_NEXS);
  case SCLP23(sdaiMO_NVLD): return SCLP22(sdaiMO_NVLD);
  case SCLP23(sdaiMO_DUP): return SCLP22(sdaiMO_DUP);
  case SCLP23(sdaiMX_NRW): return SCLP22(sdaiMX_NRW);
  case SCLP23(sdaiMX_NDEF): return SCLP22(sdaiMX_NDEF);
  case SCLP23(sdaiMX_RW): return SCLP22(sdaiMX_RW);
  case SCLP23(sdaiMX_RO): return SCLP22(sdaiMX_RO);
  case SCLP23(sdaiSD_NDEF): return SCLP22(sdaiSD_NDEF);
  case SCLP23(sdaiED_NDEF): return SCLP22(sdaiED_NDEF);
  case SCLP23(sdaiED_NDEQ): return SCLP22(sdaiED_NDEQ);
  case SCLP23(sdaiED_NVLD): return SCLP22(sdaiED_NVLD);
  case SCLP23(sdaiRU_NDEF): return SCLP22(sdaiRU_NDEF);
  case SCLP23(sdaiEX_NSUP): return SCLP22(sdaiEX_NSUP);
  case SCLP23(sdaiAT_NVLD): return SCLP22(sdaiAT_NVLD);
  case SCLP23(sdaiAT_NDEF): return SCLP22(sdaiAT_NDEF);
  case SCLP23(sdaiSI_DUP): return SCLP22(sdaiSI_DUP);
  case SCLP23(sdaiSI_NEXS): return SCLP22(sdaiSI_NEXS);
  case SCLP23(sdaiEI_NEXS): return SCLP22(sdaiEI_NEXS);
  case SCLP23(sdaiEI_NAVL): return SCLP22(sdaiEI_NAVL);
  case SCLP23(sdaiEI_NVLD): return SCLP22(sdaiEI_NVLD);
  case SCLP23(sdaiEI_NEXP): return SCLP22(sdaiEI_NEXP);
  case SCLP23(sdaiSC_NEXS): return SCLP22(sdaiSC_NEXS);
  case SCLP23(sdaiSC_EXS): return SCLP22(sdaiSC_EXS);
  case SCLP23(sdaiAI_NEXS): return SCLP22(sdaiAI_NEXS);
  case SCLP23(sdaiAI_NVLD): return SCLP22(sdaiAI_NVLD);
  case SCLP23(sdaiAI_NSET): return SCLP22(sdaiAI_NSET);
  case SCLP23(sdaiVA_NVLD): return SCLP22(sdaiVA_NVLD);
  case SCLP23(sdaiVA_NEXS): return SCLP22(sdaiVA_NEXS);
  case SCLP23(sdaiVA_NSET): return SCLP22(sdaiVA_NSET);
  case SCLP23(sdaiVT_NVLD): return SCLP22(sdaiVT_NVLD);
  case SCLP23(sdaiIR_NEXS): return SCLP22(sdaiIR_NEXS);
  case SCLP23(sdaiIR_NSET): return SCLP22(sdaiIR_NSET);
  case SCLP23(sdaiIX_NVLD): return SCLP22(sdaiIX_NVLD);
  case SCLP23(sdaiER_NSET): return SCLP22(sdaiER_NSET);
  case SCLP23(sdaiOP_NVLD): return SCLP22(sdaiOP_NVLD);
  case SCLP23(sdaiFN_NAVL): return SCLP22(sdaiFN_NAVL);
  case SCLP23(sdaiSY_ERR): return SCLP22(sdaiSY_ERR);
  default: return SCLP22(sdaiSY_ERR);
    }
}

//
//  Return a pointer to the string describing the SDAI error
//
//   It uses the fact the Part23 error codes are multiples of 10.
//
char * ErrorMap::sdaiErrorDescription(SCLP23(Error_id) error_id)
{
  if (error_id >= SCLP23(sdaiNO_ERR)  && error_id <= SCLP23(sdaiSY_ERR))
    {
      return SdaiErrorText[(int) error_id / 10];
    }
  else
    {
      return SdaiErrorText[((int) SCLP23(sdaiSY_ERR)) / 10];
    }
}
