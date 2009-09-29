#include <needFunc.h>

///////////////////////////////////////////////////////////////////////////////
// Function defined as a stub (necessary to use the scl)
// The purpose of this function is to allow the DisplayNode object to delete 
// an object that it knows nothing about.  It was made generic so that the scl
// could be used with any display toolkit.
//
// This function is called by the DisplayNode object
// This function needs to be defined outside the SCL libraries.  It needs to do
// two things:
// 1) unmap the StepEntityEditor window if it is mapped.
// 2) delete the StepEntityEditor window
// To see an example of this function used with the Data Probe look in
// ../clprobe-ui/StepEntEditor.cc  Look at DeleteSEE() and ~StepEntityEditor().
///////////////////////////////////////////////////////////////////////////////
void DeleteSEE(StepEntityEditor *se)  
{
    delete se;
}
