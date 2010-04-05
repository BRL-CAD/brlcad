
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file incrementalGeometry.cpp
/// \brief #incrementalGeometry class implementation

#include "incrementalGeometry.h"


incrementalGeometry::incrementalGeometry(abstractMixedGraph& GG,TArc ll)
    throw() : managedObject(GG.Context()), G(GG)
{
    m = G.M();
    l = ll;

    freeID = 0;
    numbersValid = false;
    leftMost = NoArc;
    topLine = NoArc;

    rowID = new TArc[l];
    colID = new TArc[l];
    prev = new TArc[2*l];
    next = new TArc[2*l];
    idToNumber = new TArc[2*l];

    this -> LogEntry(LOG_MEM,"...Floating geometry instanciated");
}


unsigned long incrementalGeometry::Size() const throw()
{
    return
          sizeof(incrementalGeometry)
        + managedObject::Allocated()
        + incrementalGeometry::Allocated();
}


unsigned long incrementalGeometry::Allocated() const throw()
{
    return 8*sizeof(TArc)*l;
}


char* incrementalGeometry::Display() const throw()
{
    return NULL;
}


incrementalGeometry::~incrementalGeometry() throw()
{
    delete[] rowID;
    delete[] colID;

    delete[] prev;
    delete[] next;
    delete[] idToNumber;

    this -> LogEntry(LOG_MEM,"...Floating geometry disallocated");
}


void incrementalGeometry::Init(TArc v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=l) NoSuchItem("Init",v);

    #endif

    for (TArc u=0;u<l;u++)
    {
        rowID[u] = NoArc;
        colID[u] = NoArc;
    }

    prev[0] = next[0] = NoArc;
    prev[1] = next[1] = NoArc;

    rowID[v] = 0;
    colID[v] = 1;

    topLine = bottomLine = 0;
    leftMost = rightMost = 1;

    freeID = 2;
}


void incrementalGeometry::InsertColumnLeftOf(TArc u,TArc v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l) NoSuchItem("InsertColumnLeftOf",u);
    if (v>=l) NoSuchItem("InsertColumnLeftOf",v);

    if (freeID==0)
        Error(ERR_REJECTED,"InsertColumnLeftOf","Geometry is not initialized");

    if (colID[v]!=NoArc)
        Error(ERR_REJECTED,"InsertColumnLeftOf","A column has already been assigned");

    #endif

    colID[v] = freeID++;
    numbersValid = false;

    TArc savedID = prev[colID[u]];
    prev[colID[u]] = colID[v];
    next[colID[v]] = colID[u];
    prev[colID[v]] = savedID;

    if (leftMost==colID[u])
    {
        leftMost = colID[v];
    }
    else
    {
        next[savedID] = colID[v];
    }
}


void incrementalGeometry::InsertColumnRightOf(TArc u,TArc v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l) NoSuchItem("InsertColumnRightOf",u);
    if (v>=l) NoSuchItem("InsertColumnRightOf",v);

    if (freeID==0)
        Error(ERR_REJECTED,"InsertColumnRightOf","Geometry is not initialized");

    if (colID[v]!=NoArc)
        Error(ERR_REJECTED,"InsertColumnRightOf","A column has already been assigned");

    #endif

    colID[v] = freeID++;
    numbersValid = false;

    TArc savedID = next[colID[u]];
    next[colID[u]] = colID[v];
    prev[colID[v]] = colID[u];
    next[colID[v]] = savedID;

    if (rightMost==colID[u])
    {
        rightMost = colID[v];
    }
    else
    {
        prev[savedID] = colID[v];
    }
}


void incrementalGeometry::InsertRowAtopOf(TArc u,TArc v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l) NoSuchItem("InsertRowAtopOf",u);
    if (v>=l) NoSuchItem("InsertRowAtopOf",v);

    if (freeID==0)
        Error(ERR_REJECTED,"InsertRowAtopOf","Geometry is not initialized");

    if (rowID[v]!=NoArc)
        Error(ERR_REJECTED,"InsertRowAtopOf","A row has already been assigned");

    #endif

    rowID[v] = freeID++;
    numbersValid = false;

    TArc savedID = prev[rowID[u]];
    prev[rowID[u]] = rowID[v];
    next[rowID[v]] = rowID[u];
    prev[rowID[v]] = savedID;

    if (topLine==rowID[u])
    {
        topLine = rowID[v];
    }
    else
    {
        next[savedID] = rowID[v];
    }
}


void incrementalGeometry::InsertRowBelowOf(TArc u,TArc v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l) NoSuchItem("InsertRowBelowOf",u);
    if (v>=l) NoSuchItem("InsertRowBelowOf",v);

    if (freeID==0)
        Error(ERR_REJECTED,"InsertRowBelowOf","Geometry is not initialized");

    if (rowID[v]!=NoArc)
        Error(ERR_REJECTED,"InsertRowBelowOf","A row has already been assigned");

    #endif

    rowID[v] = freeID++;
    numbersValid = false;

    TArc savedID = next[rowID[u]];
    next[rowID[u]] = rowID[v];
    prev[rowID[v]] = rowID[u];
    next[rowID[v]] = savedID;

    if (bottomLine==rowID[u])
    {
        bottomLine = rowID[v];
    }
    else
    {
        prev[savedID] = rowID[v];
    }
}


void incrementalGeometry::ShareRowWith(TArc u,TArc v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l)
        NoSuchItem("ShareRowWith",u);
    if (v>=l)
        NoSuchItem("ShareRowWith",v);

    if (rowID[u]==NoArc)
        Error(ERR_REJECTED,"ShareRowWith","Missing row assignment");

    if (rowID[v]!=NoArc)
        Error(ERR_REJECTED,"ShareRowWith","A row has already been assigned");

    #endif

    rowID[v] = rowID[u];
}


void incrementalGeometry::ShareColumnWith(TArc u,TArc v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l) NoSuchItem("ShareColumnWith",u);
    if (v>=l) NoSuchItem("ShareColumnWith",v);

    if (colID[u]==NoArc)
        Error(ERR_REJECTED,"ShareColumnWith","Missing column assignment");

    if (colID[v]!=NoArc)
        Error(ERR_REJECTED,"ShareColumnWith","A column has already been assigned");

    #endif

    colID[v] = colID[u];
}


void incrementalGeometry::AssignNumbers() throw(ERRejected)
{
    TArc thisID = leftMost;
    maxRowNumber = 0;

    while (thisID!=NoArc)
    {
        idToNumber[thisID] = maxRowNumber++;
        thisID = next[thisID];
    }

    thisID = topLine;
    maxColumnNumber = 0;

    while (thisID!=NoArc)
    {
        idToNumber[thisID] = maxColumnNumber++;
        thisID = next[thisID];
    }

    numbersValid = true;
}


TArc incrementalGeometry::RowNumber(TArc u) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l) NoSuchItem("RowNumber",u);

    if (rowID[u]==NoArc)
        Error(ERR_REJECTED,"RowNumber","Missing row assignment");

    #endif

    if (!numbersValid) AssignNumbers();

    return idToNumber[rowID[u]];
}


TArc incrementalGeometry::ColumnNumber(TArc u) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=l) NoSuchItem("ColumnNumber",u);

    if (colID[u]==NoArc)
        Error(ERR_REJECTED,"ColumnNumber","Missing column assignment");

    #endif

    if (!numbersValid) AssignNumbers();

    return idToNumber[colID[u]];
}


TArc incrementalGeometry::MaxRowNumber() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!numbersValid)
        Error(ERR_REJECTED,"MaxRowNumber","Missing row number assignment");

    #endif

    return maxRowNumber;
}


TArc incrementalGeometry::MaxColumnNumber() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!numbersValid)
        Error(ERR_REJECTED,"MaxColumnNumber","Missing column number assignment");

    #endif

    return maxColumnNumber;
}
