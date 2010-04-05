
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseRepresentation.cpp
/// \brief  #denseRepresentation class implementation

#include "denseRepresentation.h"
#include "canvasBuilder.h"


denseRepresentation::denseRepresentation(const abstractMixedGraph& _G,TOption options)
    throw() : managedObject(_G.Context()), graphRepresentation(_G)
{
    // Lower [upper] capacity bounds are 0[1] by default

    if (!(options & OPT_COMPLETE))
    {
        TCap ucap = 0;
        representation.MakeAttribute<TCap>(G,TokReprUCap,attributePool::ATTR_ALLOW_NULL,&ucap);
    }

    sub = NULL;
    G.SetLayoutParameter(TokLayoutArcVisibilityMode,graphDisplayProxy::ARC_DISPLAY_PRED_SUB,LAYOUT_DEFAULT);

    LogEntry(LOG_MEM,"...Dense graph structure instanciated");
}


TArc denseRepresentation::InsertArc(TArc aa,TCap _u,TFloat _c,TCap _l)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (G.IsReferenced())
        Error(ERR_REJECTED,"InsertArc","Object is referenced");

    if (aa>=mMax)
        NoSuchArc("InsertArc",aa);

    #endif

    if ((Length(2*aa)!=_c) && UCap(2*aa)>0)
        Error(MSG_WARN,"InsertArc","Labelling conflict");

    SetLength(2*aa,_c);

    SetUCap(2*aa,UCap(2*aa)+_u);

    if (_l>0)
    {
        SetLCap(2*aa,LCap(2*aa)+_l);

        if (LCap(2*aa)>sub->Key(aa)) sub -> ChangeKey(aa,LCap(2*aa));
    }

    return aa;
}


unsigned long denseRepresentation::Size()
    const throw()
{
    return
          sizeof(denseRepresentation)
        + managedObject::Allocated()
        + graphRepresentation::Allocated()
        + denseRepresentation::Allocated();
}


unsigned long denseRepresentation::Allocated()
    const throw()
{
    return 0;
}


denseRepresentation::~denseRepresentation()
    throw()
{
    ReleaseSubgraph();

    LogEntry(LOG_MEM,"...Dense graph structure disallocated");
}


void denseRepresentation::NewSubgraph(TArc card)
    throw(ERRejected)
{
    if (sub==NULL)
    {
        sub = new goblinHashTable<TArc,TFloat>(mMax,card,0,CT);
        LogEntry(LOG_MEM,"...Sparse subgraph allocated");
    }
    else Error(ERR_REJECTED,"NewSubgraph","A subgraph is already present");
}


void denseRepresentation::ReleaseSubgraph()
    throw()
{
    if (sub!=NULL)
    {
        delete sub;
        LogEntry(LOG_MEM,"...Sparse subgraph disallocated");
        sub = NULL;
    }
}


void denseRepresentation::SetSub(TArc a,TFloat multiplicity)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetSub",a);

    #endif

    TCap thisCap = UCap(a);
    TFloat thisSub = Sub(a);

    #if defined(_FAILSAVE_)

    if (   (thisSub>0 && fabs(multiplicity)<LCap(a))
        || (thisCap<InfCap && fabs(multiplicity)>thisCap)
       )
    {
        AmountOutOfRange("SetSub",multiplicity);
    }

    #endif

    if (sub==NULL) NewSubgraph();

    G.AdjustDegrees(a,multiplicity-sub->Key(a>>1));
    sub -> ChangeKey(a>>1,multiplicity);
}


void denseRepresentation::SetSubRelative(TArc a,TFloat lambda)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetSubRelative",a);

    #endif

    TCap thisCap = UCap(a);
    TFloat thisSub = Sub(a);

    #if defined(_FAILSAVE_)

    if (   (thisSub>0 && fabs(thisSub+lambda)<LCap(a))
        || (thisCap<InfCap && fabs(thisSub+lambda)>thisCap)
       )
    {
        AmountOutOfRange("SetSubRelative",lambda);
    }

    #endif

    if (!sub) NewSubgraph();

    sub -> ChangeKey(a>>1,sub->Key(a>>1)+lambda);
    G.AdjustDegrees(a,lambda);
}


TFloat denseRepresentation::Sub(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("Sub",a);

    #endif

    if (sub==NULL) return LCap(a);
    else return LCap(a)+sub->Key(a>>1);
}
