
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   managedObject.cpp
/// \brief  #managedObject class interface

#include "managedObject.h"


managedObject::managedObject(goblinController& thisContext,
    TOption options) throw() : CT(thisContext)
{
    refCounter = 0;

    if (!options) OH = CT.InsertObject(this);
    else OH = NoHandle;

    objectName = NULL;

    LogEntry(LOG_MEM,"...Data object inserted into context");
}


managedObject::~managedObject() throw()
{
    #if defined(_FAILSAVE_)

    if (refCounter)
    {
        InternalError("managedObject","Object is referenced");
    }

    #endif

    if (OH!=NoHandle) CT.DeleteObject(this);

    if (objectName) delete[] objectName;

    LogEntry(LOG_MEM,"...Data object disallocated");
}


unsigned long managedObject::Allocated() const throw()
{
    if (objectName) return strlen(objectName)+1;

    return 0;
}


char* managedObject::Display() const throw(ERFile,ERRejected)
{
    Error(ERR_REJECTED,"Display","This object cannot be viewed");

    throw ERRejected();
}


void managedObject::ExportToXFig(const char* fileName) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::ExportToTk(const char* fileName) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::ExportToDot(const char* fileName) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::ExportToAscii(const char* fileName,TOption format) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::NoSuchNode(const char* methodName,TNode v) const throw(ERRange)
{
    if (v==NoNode) sprintf(CT.logBuffer,"Undefined node");
    else sprintf(CT.logBuffer,"No such node: %lu",static_cast<unsigned long>(v));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoMoreArcs(const char* methodName,TNode v) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"No more arcs: %lu",static_cast<unsigned long>(v));

    CT.Error(ERR_REJECTED,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchArc(const char* methodName,TArc a) const throw(ERRange)
{
    if (a==NoArc) sprintf(CT.logBuffer,"Undefined arc");
    else sprintf(CT.logBuffer,"No such arc: %lu",static_cast<unsigned long>(a));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::CancelledArc(const char* methodName,TArc a) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"Cancelled arc: %lu",static_cast<unsigned long>(a));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchHandle(const char* methodName,THandle H) const throw(ERRejected)
{
    if (H==NoHandle) sprintf(CT.logBuffer,"Undefined handle");
    else sprintf(CT.logBuffer,"No such handle: %lu",static_cast<unsigned long>(H));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchItem(const char* methodName,unsigned long i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(i));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchItem(const char* methodName,unsigned short i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such item: %u",i);

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchIndex(const char* methodName,unsigned long i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such index: %lu",i);

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchIndex(const char* methodName,unsigned short i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such index: %u",i);

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchCoordinate(const char* methodName,TDim i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such coordinate: %lu",static_cast<unsigned long>(i));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoRepresentation(const char* methodName) const throw(ERRejected)
{
    CT.Error(ERR_REJECTED,OH,methodName,"Graph must be represented");
}


void managedObject::NoSparseRepresentation(const char* methodName) const throw(ERRejected)
{
    CT.Error(ERR_REJECTED,OH,methodName,"Graph must have a sparse representation");
}


void managedObject::UnknownOption(const char* methodName,int opt) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"Unknown option: %d",opt);

    CT.Error(ERR_REJECTED,OH,methodName,CT.logBuffer);
}


void managedObject::AmountOutOfRange(const char* methodName,TFloat alpha) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"Amount out of range: %g",alpha);

    CT.Error(ERR_REJECTED,OH,methodName,CT.logBuffer);
}


const char* managedObject::Label() const throw()
{
    if (objectName) return objectName;

    return "unnamed";
}


void managedObject::SetLabel(const char* l) throw()
{
    if (l!=NULL)
    {
        int newLength = strlen(l)+1;

        if (objectName)
        {
            objectName = static_cast<char*>(GoblinRealloc(objectName,newLength));
        }
        else
        {
            objectName = new char[newLength];
        }

        strcpy(objectName,l);
    }
    else if (objectName)
    {
        delete[] objectName;
        objectName = NULL;
    }
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   managedObject.cpp
/// \brief  #managedObject class interface

#include "managedObject.h"


managedObject::managedObject(goblinController& thisContext,
    TOption options) throw() : CT(thisContext)
{
    refCounter = 0;

    if (!options) OH = CT.InsertObject(this);
    else OH = NoHandle;

    objectName = NULL;

    LogEntry(LOG_MEM,"...Data object inserted into context");
}


managedObject::~managedObject() throw()
{
    #if defined(_FAILSAVE_)

    if (refCounter)
    {
        InternalError("managedObject","Object is referenced");
    }

    #endif

    if (OH!=NoHandle) CT.DeleteObject(this);

    if (objectName) delete[] objectName;

    LogEntry(LOG_MEM,"...Data object disallocated");
}


unsigned long managedObject::Allocated() const throw()
{
    if (objectName) return strlen(objectName)+1;

    return 0;
}


char* managedObject::Display() const throw(ERFile,ERRejected)
{
    Error(ERR_REJECTED,"Display","This object cannot be viewed");

    throw ERRejected();
}


void managedObject::ExportToXFig(const char* fileName) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::ExportToTk(const char* fileName) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::ExportToDot(const char* fileName) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::ExportToAscii(const char* fileName,TOption format) const
    throw(ERFile,ERRejected)
{
    throw ERRejected();
}


void managedObject::NoSuchNode(const char* methodName,TNode v) const throw(ERRange)
{
    if (v==NoNode) sprintf(CT.logBuffer,"Undefined node");
    else sprintf(CT.logBuffer,"No such node: %lu",static_cast<unsigned long>(v));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoMoreArcs(const char* methodName,TNode v) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"No more arcs: %lu",static_cast<unsigned long>(v));

    CT.Error(ERR_REJECTED,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchArc(const char* methodName,TArc a) const throw(ERRange)
{
    if (a==NoArc) sprintf(CT.logBuffer,"Undefined arc");
    else sprintf(CT.logBuffer,"No such arc: %lu",static_cast<unsigned long>(a));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::CancelledArc(const char* methodName,TArc a) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"Cancelled arc: %lu",static_cast<unsigned long>(a));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchHandle(const char* methodName,THandle H) const throw(ERRejected)
{
    if (H==NoHandle) sprintf(CT.logBuffer,"Undefined handle");
    else sprintf(CT.logBuffer,"No such handle: %lu",static_cast<unsigned long>(H));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchItem(const char* methodName,unsigned long i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such item: %lu",static_cast<unsigned long>(i));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchItem(const char* methodName,unsigned short i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such item: %u",i);

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchIndex(const char* methodName,unsigned long i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such index: %lu",i);

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchIndex(const char* methodName,unsigned short i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such index: %u",i);

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoSuchCoordinate(const char* methodName,TDim i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such coordinate: %lu",static_cast<unsigned long>(i));

    CT.Error(ERR_RANGE,OH,methodName,CT.logBuffer);
}


void managedObject::NoRepresentation(const char* methodName) const throw(ERRejected)
{
    CT.Error(ERR_REJECTED,OH,methodName,"Graph must be represented");
}


void managedObject::NoSparseRepresentation(const char* methodName) const throw(ERRejected)
{
    CT.Error(ERR_REJECTED,OH,methodName,"Graph must have a sparse representation");
}


void managedObject::UnknownOption(const char* methodName,int opt) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"Unknown option: %d",opt);

    CT.Error(ERR_REJECTED,OH,methodName,CT.logBuffer);
}


void managedObject::AmountOutOfRange(const char* methodName,TFloat alpha) const throw(ERRejected)
{
    sprintf(CT.logBuffer,"Amount out of range: %g",alpha);

    CT.Error(ERR_REJECTED,OH,methodName,CT.logBuffer);
}


const char* managedObject::Label() const throw()
{
    if (objectName) return objectName;

    return "unnamed";
}


void managedObject::SetLabel(const char* l) throw()
{
    if (l!=NULL)
    {
        int newLength = strlen(l)+1;

        if (objectName)
        {
            objectName = static_cast<char*>(GoblinRealloc(objectName,newLength));
        }
        else
        {
            objectName = new char[newLength];
        }

        strcpy(objectName,l);
    }
    else if (objectName)
    {
        delete[] objectName;
        objectName = NULL;
    }
}
