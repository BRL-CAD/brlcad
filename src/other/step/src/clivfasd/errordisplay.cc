
#include <errordisplay.h>


ErrorDisplay::~ErrorDisplay()
{
    if(errorButSt != nil)
	errorButSt->Detach(this);
}

void ErrorDisplay::Update()
{
    int v = 0; 
    errorButSt->GetValue(v); 
    if(v == 1) { 
	errorButSt->SetValue(0);
	errorDisp->Message("\0");
    }
}

