#ifndef P23SDAI_H
#define P23SDAI_H

/*
* NIST STEP Core Class Library
* clstepcore/sdai.h
* April 1997
* David Sauder
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.

* This file is included to support the proposed SDAI/C++ Language Binding
* specified in Annex D of STEP Part 22.

* This file specifies the appropriate naming conventions and NULL
* values for the EXPRESS base types.
*/

#include "sc_cf.h"
#include <sc_export.h>

extern const char * SCLversion;

#include <ctype.h>
#include <string>


#include "dictdefs.h"

#include "baseType.h"
#include "Str.h"
#include "errordesc.h"

typedef std::string Express_id;

class TypeDescriptor;
class EntityDescriptor;
class SelectTypeDescriptor;
class InstMgr;

#include "STEPattributeList.h"

class STEPattributeList;
class STEPattribute;

#ifndef BINARY_DELIM
#define BINARY_DELIM '\"'
#endif

//  STRING
#define S_STRING_NULL        ""


typedef long           SDAI_Integer;
typedef short          SDAI_Short;
typedef unsigned short SDAI_UShort;
typedef unsigned long  SDAI_ULong;
typedef double         SDAI_Real;

// C++ from values.h DAS PORT
extern SC_CORE_EXPORT const SDAI_Integer SDAI_INT_NULL;
extern SC_CORE_EXPORT const SDAI_Real SDAI_REAL_NULL;
// arbitrary choice by me for number DAS
extern SC_CORE_EXPORT const SDAI_Real SDAI_NUMBER_NULL;


enum SDAI_Access_type {
    SDAI_sdaiRO,
    SDAI_sdaiRW,
    SDAI_Access_type_unset
};

enum SDAI_Commit_mode {
    SDAI_sdaiCOMMIT,
    SDAI_sdaiABORT
} ;

enum SDAI_AttrFlag {
    SDAI_sdaiSET,
    SDAI_sdaiUNSET
} ;

enum SDAI_ImplementationClass  {
    SDAI_sdaiCLASS1,
    SDAI_sdaiCLASS2,
    SDAI_sdaiCLASS3,
    SDAI_sdaiCLASS4,
    SDAI_sdaiCLASS5
};  // conflict with part 22 EXPRESS:

enum SDAI_Error_id {
    //
    // error codes taken from 10303-23, Jan 28, 1997.
    // ISO TC184/SC4/WG11 N 004
    //
    SDAI_sdaiNO_ERR  =    0,   // No error
    SDAI_sdaiSS_OPN  =   10,   // Session open
    SDAI_sdaiSS_NAVL =   20,   // Session not available
    SDAI_sdaiSS_NOPN =   30,   // Session is not open
    SDAI_sdaiRP_NEXS =   40,   // Repository does not exist
    SDAI_sdaiRP_NAVL =   50,   // Repository not available
    SDAI_sdaiRP_OPN  =   60,   // Repository already opened
    SDAI_sdaiRP_NOPN =   70,   // Repository is not open
    SDAI_sdaiTR_EAB  =   80,   // Transaction ended abnormally so it no longer exists
    SDAI_sdaiTR_EXS  =   90,   // Transaction exists
    SDAI_sdaiTR_NAVL =  100,   // Transaction not available
    SDAI_sdaiTR_RW   =  110,   // Transaction read-write
    SDAI_sdaiTR_NRW  =  120,   // Transaction not read-write
    SDAI_sdaiTR_NEXS =  130,   // Transaction does not exist
    SDAI_sdaiMO_NDEQ =  140,   // SDAI-model not domain-equivalent
    SDAI_sdaiMO_NEXS =  150,   // SDAI-model does not exist
    SDAI_sdaiMO_NVLD =  160,   // SDAI-model invalid
    SDAI_sdaiMO_DUP  =  170,   // SDAI-model duplicate
    SDAI_sdaiMX_NRW  =  180,   // SDAI-model access not read-write
    SDAI_sdaiMX_NDEF =  190,   // SDAI-model access is not defined
    SDAI_sdaiMX_RW   =  200,   // SDAI-model access read-write
    SDAI_sdaiMX_RO   =  210,   // SDAI-model access read-only
    SDAI_sdaiSD_NDEF =  220,   // Schema definition is not defined
    SDAI_sdaiED_NDEF =  230,   // Entity definition not defined
    SDAI_sdaiED_NDEQ =  240,   // Entity definition not domain equivalent
    SDAI_sdaiED_NVLD =  250,   // Entity definition invalid
//  SDAI_sdaiED_NAVL =  250,   // Entity definition not available
    SDAI_sdaiRU_NDEF =  260,   // Rule not defined
    SDAI_sdaiEX_NSUP =  270,   // Expression evaluation not supported
    SDAI_sdaiAT_NVLD =  280,   // Attribute invalid
    SDAI_sdaiAT_NDEF =  290,   // Attribute not defined
    SDAI_sdaiSI_DUP  =  300,   // Schema instance duplicate
    SDAI_sdaiSI_NEXS =  310,   // Schema instance does not exist
    SDAI_sdaiEI_NEXS =  320,   // Entity instance does not exist
    SDAI_sdaiEI_NAVL =  330,   // Entity instance not available
    SDAI_sdaiEI_NVLD =  340,   // Entity instance invalid
    SDAI_sdaiEI_NEXP =  350,   // Entity instance not exported
    SDAI_sdaiSC_NEXS =  360,   // Scope does not exist
    SDAI_sdaiSC_EXS =  370,   // Scope exists
    SDAI_sdaiAI_NEXS =  380,   // Aggregate instance does not exist
    SDAI_sdaiAI_NVLD =  390,   // Aggregate instance invalid
    SDAI_sdaiAI_NSET =  400,   // Aggregate instance is empty
    SDAI_sdaiVA_NVLD =  410,   // Value invalid
    SDAI_sdaiVA_NEXS =  420,   // Value does not exist
    SDAI_sdaiVA_NSET =  430,   // Value not set
    SDAI_sdaiVT_NVLD =  440,   // Value type invalid
    SDAI_sdaiIR_NEXS =  450,   // Iterator does not exist
    SDAI_sdaiIR_NSET =  460,   // Current member is not defined
    SDAI_sdaiIX_NVLD =  470,   // Index invalid
    SDAI_sdaiER_NSET =  480,   // Event recording not set
    SDAI_sdaiOP_NVLD =  490,   // Operator invalid
    SDAI_sdaiFN_NAVL =  500,   // Function not available
    SDAI_sdaiSY_ERR  = 1000    // Underlying system error
};

typedef char * SDAI_Time_stamp;
typedef char * SDAI_Entity_name;
typedef char * SDAI_Schema_name;

#include <sdaiString.h>

#include <sdaiBinary.h>

// define Object which I am calling sdaiObject for now - DAS
#include <sdaiObject.h>

/******************************************************************************
ENUMERATION
    These types do not use the interface specified.
    The interface used is to enumerated values.  The enumerations are
    mapped in the following way:
    *  enumeration-item-from-schema ==> enumeration item in c++ enum clause
    *  all enumeration items in c++ enum are in upper case
    *  the value ENUM_NULL is used to represent NULL for all enumerated types
 *****************************************************************************/

#include <sdaiEnum.h>

/******************************************************************************
BOOLEAN and LOGICAL

    Logical are implemented as an enumeration in sdaiEnum.h:

    Boolean are implemented as an enumeration in sdaiEnum.h:

******************************************************************************/

// ***note*** this file needs classes from sdaiEnum.h
// define DAObjectID and classes PID, PID_DA, PID_SDAI, DAObject, DAObject_SDAI
#include <sdaiDaObject.h>


#include <sdaiApplication_instance.h>
#include <sdaiApplication_instance_set.h>

/******************************************************************************
SELECT

    Selects are represented as subclasses of the SDAI_Select class in
    sdaiSelect.h

******************************************************************************/
#include <sdaiSelect.h>

class SDAI_Model_contents;
typedef SDAI_Model_contents * SDAI_Model_contents_ptr;
typedef SDAI_Model_contents_ptr SDAI_Model_contents_var;

#include <sdaiModel_contents_list.h>

#include <sdaiSession_instance.h>
#include <sdaiEntity_extent.h>
#include <sdaiEntity_extent_set.h>
#include <sdaiModel_contents.h>

//  ENTITY
extern SC_CORE_EXPORT SDAI_Application_instance NilSTEPentity;
#define ENTITY_NULL        &NilSTEPentity
#define NULL_ENTITY        &NilSTEPentity
#define S_ENTITY_NULL        &NilSTEPentity


typedef SDAI_Application_instance STEPentity;
typedef SDAI_Application_instance * STEPentity_ptr;
typedef STEPentity_ptr STEPentity_var;

typedef SDAI_Application_instance * STEPentityPtr;
typedef SDAI_Application_instance * STEPentityH;

extern SC_CORE_EXPORT SDAI_Application_instance *
ReadEntityRef( istream & in, ErrorDescriptor * err, const char * tokenList,
               InstMgr * instances, int addFileId );

#define SdaiInteger SDAI_Integer
#define SdaiReal SDAI_Real

#define STEPenumeration SDAI_Enum
#define SdaiSelect SDAI_Select
#define SdaiString SDAI_String
#define SdaiBinary SDAI_Binary

#define        S_INT_NULL    SDAI_INT_NULL
#define S_REAL_NULL   SDAI_REAL_NULL
#define S_NUMBER_NULL SDAI_NUMBER_NULL

/******************************************************************************
AGGREGATE TYPES

    Aggregate types are accessed generically.  (There are not seperate
    classes for the different types of aggregates.)  Aggregates are
    implemented through the STEPaggregate class.

******************************************************************************/

inline SDAI_BOOLEAN * create_BOOLEAN() {
    return new SDAI_BOOLEAN ;
}

inline SDAI_LOGICAL * create_LOGICAL() {
    return new SDAI_LOGICAL ;
}

// below is outdated
typedef SDAI_Select * SdaiSelectH;

#endif
