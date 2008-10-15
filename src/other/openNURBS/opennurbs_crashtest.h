/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2007 Robert McNeel & Associates. All rights reserved.
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

#if !defined(ON_CRASH_TEST_STATICS)

#error See instructions in the following comment.

/*
#define ON_CRASH_TEST_STATICS
#include "opennurbs_crashtest.h"
#undef ON_CRASH_TEST_STATICS
int CrashTestHelper( int crash_type, ON_TextLog& textlog )
{
  //crash_type
  //0 = NullPtrDeRef
  //1 = BogusPtrDeRef
  //2 = CallNullFuncPtr
  //3 = CallBogusFuncPtr
  //4 = Call_abort
  //5 = Call_exit99
  //6 = DivideByDoubleZero
  //7 = DivideByFloatZero
  //8 = DivideByIntZero
  //9 = logNeg
  return CrashTestHelper(crash_type,textlog);
}
*/

#endif

///////////////////////////////////////////////////////////////////////////////
//
// For testing crash handling.
//

typedef int (*CRASHTEST__FUNCTION__POINTER__)(int);

static
void CrashTestHelper_GetNullIntPrt(int ** pp )
{
  *pp = 0;
}

static
void CrashTestHelper_GetBogusIntPtr(int ** pp )
{
  INT_PTR i = 0xCACAF00D;
  *pp = (int*)i;
}

static
void CrashTestHelper_GetNullFuncPtr(CRASHTEST__FUNCTION__POINTER__* pp )
{
  *pp = 0;
}

static
void CrashTestHelper_GetBogusFuncPtr(CRASHTEST__FUNCTION__POINTER__* pp )
{
  INT_PTR i = 0xCACAF00D;
  *pp = (CRASHTEST__FUNCTION__POINTER__)i;
}

static
int CrashTestHelper_DerefNullIntPtr( ON_TextLog& textlog, int crash_type, int* stack_ptr )
{
  int* ptr;
  CrashTestHelper_GetNullIntPrt(&ptr); // sets ptr = NULL
  textlog.Print("NULL ptr = %08x\n",ptr);
  *stack_ptr = *ptr;    // dereferences NULL pointer and crashes
  textlog.Print("*ptr = %d\n",*ptr);
  return crash_type;
}

static
int CrashTestHelper_DerefBogusIntPtr( ON_TextLog& textlog, int crash_type, int* stack_ptr )
{
  int* ptr;
  CrashTestHelper_GetBogusIntPtr(&ptr); // sets ptr = 0xCACAF00D
  textlog.Print("Bogus ptr = %08x\n",ptr);
  *stack_ptr = *ptr;     // dereferences bogus pointer and crashes
  textlog.Print("*ptr = %d\n",*ptr);
  return crash_type;
}

static
int CrashTestHelper_CallNullFuncPtr( ON_TextLog& textlog, int crash_type, int* stack_ptr )
{
  CRASHTEST__FUNCTION__POINTER__ fptr;
  CrashTestHelper_GetNullFuncPtr(&fptr); // sets ptr = NULL
  textlog.Print("NULL function f = %08x\n",fptr);
  *stack_ptr = fptr(crash_type); // dereferences NULL function pointer and crashes
  textlog.Print("f(%d) = %d\n",crash_type,*stack_ptr);
  return crash_type;
}

static
int CrashTestHelper_CallBoguslFuncPtr( ON_TextLog& textlog, int crash_type, int* stack_ptr )
{
  CRASHTEST__FUNCTION__POINTER__ fptr;
  CrashTestHelper_GetBogusFuncPtr(&fptr); // sets ptr = NULL
  textlog.Print("Bogus function f = %08x\n",fptr);
  *stack_ptr = fptr(crash_type); // dereferences bogus function pointer and crashes
  textlog.Print("f(%d) = %d\n",crash_type,*stack_ptr);
  return crash_type;
}

static
bool CrashTestHelper_DivideByFloatZero( ON_TextLog& textlog, const char* zero )
{
  double z;
  sscanf( zero, "%g", &z );
  float fz = (float)z;
  textlog.Print("float fz = %f\n",fz);
  float fy = 13.0f/fz;
  textlog.Print("double 13.0f/fz = %f\n",fy);
  return (0.123f != fy);
}

static
bool CrashTestHelper_DivideByDoubleZero( ON_TextLog& textlog, const char* zero )
{
  double z;
  sscanf( zero, "%g", &z );
  textlog.Print("double z = %g\n",z);
  double y = 13.0/z;
  textlog.Print("double 13.0/z = %g\n",y);
  return ON_IsValid(y);
}

static
bool CrashTestHelper_DivideByIntZero( ON_TextLog& textlog, const char* zero )
{
  int iz = 0, iy = 0;
  double z;
  sscanf( zero, "%g", &z );
  iz = (int)z;
  textlog.Print("int iz = %d\n",iz);
  iy = 17/iz;
  textlog.Print("17/iz = %d\n",iy);
  return (123456 != iy);
}

static
bool CrashTestHelper_LogNegativeNumber( ON_TextLog& textlog, const char* minus_one )
{
  double z;
  sscanf( minus_one, "%g", &z );
  textlog.Print("z = %g\n",z);
  double y = log(z);
  textlog.Print("log(z) = %g\n",y);
  return ON_IsValid(y);
}

/*
Description:
  Create a condition that should result in a crash.
  The intended use is for testing application crash handling.
Parameters:
  crash_type - [in]
    0: dereference NULL data pointer
    1: dereference bogus data pointer (0xCACAF00D)
    2: dereference NULL function pointer
    3: dereference bogus function pointer (0xCACAF00D)
    4: call abort()
    5: call exit(99);
    6: divide by zero
    7: log(negative number) - should call math error handler
Returns:
  Value of crash_type parameter.
*/
static
int CrashTestHelper( int crash_type, ON_TextLog& textlog )
{
  // Note: The code and calls are intentionally obtuse
  //       so that it is difficult for an optimizer to
  //       not perform the calculation.
  int stack_int = 0;
  int rc = crash_type;

  switch( crash_type )
  {
  case 0: // dereference NULL pointer
    rc = CrashTestHelper_DerefNullIntPtr( textlog, crash_type, &stack_int );
    break;

  case 1: // dereference bogus pointer
    rc = CrashTestHelper_DerefBogusIntPtr( textlog, crash_type, &stack_int );
    break;

  case 2: // corrupt stack so return sets IP to bogus value
    rc = CrashTestHelper_CallNullFuncPtr( textlog, crash_type, &stack_int );
    break;

  case 3: // divide by zero
    rc = CrashTestHelper_CallBoguslFuncPtr( textlog, crash_type, &stack_int );
    break;

  case 4:
    abort();
    textlog.Print("abort() didn't crash.\n");
    break;

  case 5:
    exit(99);
    textlog.Print("exit(99) didn't crash.\n");
    break;

  case 6:
    if ( CrashTestHelper_DivideByDoubleZero( textlog, "0.0" ) )
    {
      textlog.Print("Divide by double 0.0 didn't crash - exception must have been handled or ignored.\n");
    }
    break;

  case 7:
    if ( CrashTestHelper_DivideByFloatZero( textlog, "0.0" ) )
    {
      textlog.Print("Divide by float 0.0f didn't crash - exception must have been handled or ignored.\n");
    }
    break;

  case 8:
    if ( CrashTestHelper_DivideByIntZero( textlog, "0.0" ) )
    {
      textlog.Print("Divide by int 0 didn't crash - exception must have been handled or ignored.\n");
    }
    break;

  case 9:
    if ( CrashTestHelper_LogNegativeNumber( textlog, "-1.0" ) )
    {
      textlog.Print("log(negative number) didn't crash - exception must have been handled or ignored.\n");
    }
    break;

  default:
    break;
  }

  return rc;
}

