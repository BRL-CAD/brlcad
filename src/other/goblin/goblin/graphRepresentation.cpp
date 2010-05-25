
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   graphRepresentation.cpp
/// \brief  #graphRepresentation class implementation

#include "graphRepresentation.h"


// ------------------------------------------------------


const TFloat graphRepresentation::defaultLength      = 1.0;
const TCap   graphRepresentation::defaultUCap        = 1;
const TCap   graphRepresentation::defaultLCap        = 0;
const TCap   graphRepresentation::defaultDemand      = 0;
const TFloat graphRepresentation::defaultC           = 0.0;
const char   graphRepresentation::defaultOrientation = 0;

graphRepresentation::graphRepresentation(const abstractMixedGraph& GC)
    throw() : managedObject(GC.Context()),
    G(const_cast<abstractMixedGraph&>(GC)),
    representation(listOfReprPars,TokReprEndSection,attributePool::ATTR_ALLOW_NULL),
    geometry(listOfGeometryPars,TokGeoEndSection,attributePool::ATTR_FULL_RANK),
    layoutData(listOfLayoutPars,TokLayoutEndSection,attributePool::ATTR_FULL_RANK)
{
    nMax = nAct = G.N();
    mMax = mAct = G.M();
    lMax = lAct = G.L();

    if (CT.randLength && mAct>0)
    {
        TFloat* length = representation.RawArray<TFloat>(G,TokReprLength);

        for (TArc a=0;a<mAct;a++) length[a] = TFloat(CT.SignedRand());

        LogEntry(LOG_MEM,"...Length labels allocated");
    }

    if (CT.randGeometry)
    {
        geometry.InitAttribute<TFloat>(G,TokGeoDim,2);
        TFloat* axis0 = geometry.RawArray<TFloat>(G,TokGeoAxis0);
        TFloat* axis1 = geometry.RawArray<TFloat>(G,TokGeoAxis1);

        for (TNode v=0;v<nAct;v++)
        {
            axis0[v] = TFloat(CT.SignedRand());
            axis1[v] = TFloat(CT.SignedRand());
        }

        LogEntry(LOG_MEM,"...Nodes embedded into plane");
    }

    LogEntry(LOG_MEM,"...Generic graph allocated");
}


void graphRepresentation::Reserve(TNode _n,TArc _m,TNode _l)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (nMax>0 || mMax>0 || lMax>0)
    {
        Error(ERR_REJECTED,"Reserve","Graph structure must be initial");
    }

    if (_n>=CT.MaxNode())
    {
        sprintf(CT.logBuffer,"Number of graph nodes is out of range: %lu",
            static_cast<unsigned long>(_n));
        Error(ERR_REJECTED,"Reserve",CT.logBuffer);
    }

    if (2*_m>CT.MaxArc()-2)
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(_m));
        Error(ERR_REJECTED,"Reserve",CT.logBuffer);
    }

    if (_l>=CT.MaxNode())
    {
        sprintf(CT.logBuffer,"Number of layout nodes is out of range: %lu",
            static_cast<unsigned long>(_l));
        Error(ERR_REJECTED,"Reserve",CT.logBuffer);
    }

    #endif

    nMax = nAct = _n;
    mMax = mAct = _m;
    lMax = lAct = _l;
}


size_t graphRepresentation::SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw()
{
    switch (arrayDim)
    {
        case DIM_GRAPH_NODES:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(nMax);
            }
            else return size_t(nAct);
        }
        case DIM_GRAPH_ARCS:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(mMax);
            }
            else return size_t(mAct);
        }
        case DIM_ARCS_TWICE:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(2*mMax);
            }
            else return size_t(2*mAct);
        }
        case DIM_LAYOUT_NODES:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(lMax);
            }
            else return size_t(lAct);
        }
        case DIM_SINGLETON:
        {
            return size_t(1);
        }
        default:
        {
            return size_t(0);
        }
    }
}


unsigned long graphRepresentation::Allocated()
    const throw()
{
    return 0;
}


graphRepresentation::~graphRepresentation()
    throw()
{
    LogEntry(LOG_MEM,"...Generic graph disallocated");
}


void graphRepresentation::SetUCap(TArc a,TCap _ucap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetUCap",a);

    if (_ucap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_ucap);
        Error(ERR_RANGE,"SetUCap",CT.logBuffer);
    }

    #endif

    if (Sub(a)>_ucap) SetSub(a,_ucap);

    attribute<TCap>* ucap = representation.GetAttribute<TCap>(TokReprUCap);

    if (!ucap)
    {
        if (_ucap==defaultUCap) return;

        ucap = representation.InitAttribute<TCap>(G,TokReprUCap,defaultUCap);
    }
    else if (ucap->Size()==0)
    {
        if (_ucap==ucap->DefaultValue()) return;

        ucap -> SetCapacity(mMax);
        ucap -> IncreaseSize(mAct);
    }

    ucap -> SetValue(a>>1,_ucap);
}


void graphRepresentation::SetCUCap(TCap _ucap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (_ucap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_ucap);
        Error(ERR_RANGE,"SetCUCap",CT.logBuffer);
    }

    #endif

    ReleaseSubgraph();

    if (_ucap!=defaultUCap)
    {
        representation.MakeAttribute<TCap>(G,TokReprUCap,attributePool::ATTR_ALLOW_NULL,&_ucap);
    }
    else
    {
        representation.ReleaseAttribute(TokReprUCap);
    }
}


void graphRepresentation::SetLCap(TArc a,TCap _lcap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetLCap",a);

    if (_lcap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_lcap);
        Error(ERR_RANGE,"SetLCap",CT.logBuffer);
    }

    #endif

    TFloat thisSub = Sub(a);

    if (fabs(thisSub)<_lcap)
    {
        if (thisSub>=0)
        {
            SetSub(a,_lcap);
        }
        else
        {
            SetSub(a,-_lcap);
        }
    }

    attribute<TCap>* lcap = representation.GetAttribute<TCap>(TokReprLCap);

    if (!lcap)
    {
        if (_lcap==defaultLCap) return;

        lcap = representation.InitAttribute<TCap>(G,TokReprLCap,defaultLCap);
    }
    else if (lcap->Size()==0)
    {
        if (_lcap==lcap->DefaultValue()) return;

        lcap -> SetCapacity(mMax);
        lcap -> IncreaseSize(mAct);
    }

    lcap -> SetValue(a>>1,_lcap);
}


void graphRepresentation::SetCLCap(TCap _lcap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (_lcap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_lcap);
        Error(ERR_RANGE,"SetCLCap",CT.logBuffer);
    }

    #endif

    ReleaseSubgraph();

    if (_lcap!=defaultLCap)
    {
        representation.MakeAttribute<TCap>(G,TokReprLCap,attributePool::ATTR_ALLOW_NULL,&_lcap);
    }
    else
    {
        representation.ReleaseAttribute(TokReprLCap);
    }
}


void graphRepresentation::SetDemand(TNode v,TCap _demand)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("SetDemand",v);

    #endif

    attribute<TCap>* demand = representation.GetAttribute<TCap>(TokReprDemand);

    if (!demand)
    {
        if (_demand==defaultDemand) return;

        demand = representation.InitAttribute<TCap>(G,TokReprDemand,defaultDemand);
    }
    else if (demand->Size()==0)
    {
        if (_demand==demand->DefaultValue()) return;

        demand -> SetCapacity(nMax);
        demand -> IncreaseSize(nAct);
    }

    demand -> SetValue(v,_demand);
}


void graphRepresentation::SetCDemand(TCap _demand)
    throw()
{
    if (_demand!=defaultDemand)
    {
        representation.MakeAttribute<TCap>(G,TokReprDemand,attributePool::ATTR_ALLOW_NULL,&_demand);
    }
    else
    {
        representation.ReleaseAttribute(TokReprDemand);
    }
}


void graphRepresentation::SetMetricType(abstractMixedGraph::TMetricType metricType) throw()
{
    attribute<int>* attrMetric = geometry.GetAttribute<int>(TokGeoMetric);

    if (metricType==abstractMixedGraph::METRIC_DISABLED)
    {
        if (attrMetric) geometry.ReleaseAttribute(TokGeoMetric);
    }
    else
    {
        if (!attrMetric)
        {
            attrMetric = geometry.InitAttribute<int>(
                            *this,TokGeoMetric,int(abstractMixedGraph::METRIC_DISABLED));
        }

        attrMetric -> SetValue(0,int(metricType));
    }
}


TFloat graphRepresentation::Length(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("Length",a);

    #endif

    abstractMixedGraph::TMetricType metricType =
        abstractMixedGraph::TMetricType(geometry.GetValue<int>(
            TokGeoMetric,0,int(abstractMixedGraph::METRIC_DISABLED)));

    if (metricType==abstractMixedGraph::METRIC_DISABLED)
    {
        return representation.GetValue<TFloat>(TokReprLength,a>>1,defaultLength);
    }

    TNode v1 = G.StartNode(a);
    TNode v2 = G.EndNode(a);

    if (v1==v2) return InfFloat;

    TFloat x1 = G.C(v1,0);
    TFloat y1 = G.C(v1,1);
    TFloat x2 = G.C(v2,0);
    TFloat y2 = G.C(v2,1);

    if (metricType==abstractMixedGraph::METRIC_SPHERIC)
    {
        const double radius = 6378.388;

        double deg = int(x1);
        double min = x1-deg;
        double latitude1 = PI*(deg+5.0*min/3.0)/180.0;
        deg = int(y1);
        min = y1-deg;
        double longitude1 = PI*(deg+5.0*min/3.0)/180.0;

        deg = int(x2);
        min = x2-deg;
        double latitude2 = PI*(deg+5.0*min/3.0)/180.0;
        deg = int(y2);
        min = y2-deg;
        double longitude2 = PI*(deg+5.0*min/3.0)/180.0;

        double q1 = cos(longitude1-longitude2);
        double q2 = cos(latitude1-latitude2);
        double q3 = cos(latitude1+latitude2);
        return int(radius*acos(0.5*((1.0+q1)*q2-(1.0-q1)*q3))+1.0);
    }

    TFloat dx = fabs(x1-x2);
    TFloat dy = fabs(y1-y2);

    if (metricType==abstractMixedGraph::METRIC_MANHATTAN) return floor(dx+dy+0.5);
    if (metricType==abstractMixedGraph::METRIC_EUCLIDIAN) return floor(sqrt(dx*dx+dy*dy)+0.5);

    // metricType==METRIC_MAXIMUM
    return ((dx>dy) ? floor(dx+0.5) : floor(dy+0.5));
}


TFloat graphRepresentation::MaxLength()
    const throw()
{
    if (G.MetricType()==abstractMixedGraph::METRIC_DISABLED)
    {
        return representation.MaxValue<TFloat>(TokReprLength,defaultLength);
    }

    TFloat maxLength = -InfFloat;

    for (TArc a=0;a<mAct;a++)
    {
        TFloat thisLength = Length(2*a);

        if (thisLength>maxLength) maxLength = thisLength;
    }

    return maxLength;
}


void graphRepresentation::SetLength(TArc a,TFloat _length)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetLength",a);

    #endif

    attribute<TFloat>* length = representation.GetAttribute<TFloat>(TokReprLength);

    if (!length)
    {
        if (_length==defaultLength) return;

        length = representation.InitAttribute<TFloat>(G,TokReprLength,defaultLength);
    }
    else if (length->Size()==0)
    {
        if (_length==length->DefaultValue()) return;

        length -> SetCapacity(mMax);
        length -> IncreaseSize(mAct);
    }

    length -> SetValue(a>>1,_length);
}


void graphRepresentation::SetCLength(TFloat _length)
    throw()
{
    if (_length!=defaultLength)
    {
        representation.MakeAttribute<TFloat>(G,TokReprLength,attributePool::ATTR_ALLOW_NULL,&_length);
    }
    else
    {
        representation.ReleaseAttribute(TokReprLength);
    }
}


void graphRepresentation::SetOrientation(TArc a,char _orientation)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetOrientation",a);

    if (_orientation>=4)
        Error(ERR_RANGE,"SetOrientation","Illegal assignment");

    #endif

    attribute<char>* orientation = representation.GetAttribute<char>(TokReprOrientation);

    if (!orientation)
    {
        if (_orientation==defaultOrientation) return;

        orientation = representation.InitAttribute<char>(G,TokReprOrientation,defaultOrientation);
    }
    else if (orientation->Size()==0)
    {
        if (_orientation==orientation->DefaultValue()) return;

        orientation -> SetCapacity(mMax);
        orientation -> IncreaseSize(mAct);
    }

    orientation -> SetValue(a>>1,_orientation);
}


void graphRepresentation::SetCOrientation(char _orientation)
    throw()
{
    #if defined(_FAILSAVE_)

    if (_orientation>=4)
        Error(ERR_RANGE,"SetCOrientation","Illegal assignment");

    #endif

    if (_orientation!=defaultOrientation)
    {
        representation.MakeAttribute<char>(G,TokReprOrientation,attributePool::ATTR_ALLOW_NULL,&_orientation);
    }
    else
    {
        representation.ReleaseAttribute(TokReprOrientation);
    }
}


TDim graphRepresentation::Dim()
    const throw()
{
    attribute<TFloat>* coord = NULL;

    for (TDim i=3;i>0;)
    {
        --i;
        coord = geometry.GetAttribute<TFloat>(TokGeoAxis0+i);
        if (coord && coord->Size()>0)
        {
            if (coord->MaxValue()>coord->MinValue()) return i+1;
        }
        else
        {
            // Shrink to a constant ?
        }
    }

    return 0;
}


void graphRepresentation::SetC(TNode v,TDim i,TFloat pos)
    throw(ERRange)
{
   #if defined(_FAILSAVE_)

    if (v>=lAct)
        NoSuchNode("SetC",v);

    if (i>2) return;

    #endif

    attribute<TFloat>* axis = geometry.GetAttribute<TFloat>(TokGeoAxis0+i);

    if (!axis && pos!=defaultC)
    {
        axis = geometry.InitAttribute<TFloat>(G,TokGeoAxis0+i,defaultC);
    }

    if (axis) axis -> SetValue(v,pos);
}


void graphRepresentation::ReleaseCoordinate(TDim i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>2)
    {
        sprintf(CT.logBuffer,"No such coordinate: %lu",static_cast<unsigned long>(i));
        Error(ERR_RANGE,"ReleaseCoord",CT.logBuffer);
    }

    #endif

    geometry.ReleaseAttribute(TokGeoAxis0+i);
}


bool graphRepresentation::HiddenNode(TNode v)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct) NoSuchNode("HiddenNode",v);

    #endif

    return false;
}


bool graphRepresentation::HiddenArc(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("HiddenArc",a);

    #endif

    return false;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   graphRepresentation.cpp
/// \brief  #graphRepresentation class implementation

#include "graphRepresentation.h"


// ------------------------------------------------------


const TFloat graphRepresentation::defaultLength      = 1.0;
const TCap   graphRepresentation::defaultUCap        = 1;
const TCap   graphRepresentation::defaultLCap        = 0;
const TCap   graphRepresentation::defaultDemand      = 0;
const TFloat graphRepresentation::defaultC           = 0.0;
const char   graphRepresentation::defaultOrientation = 0;

graphRepresentation::graphRepresentation(const abstractMixedGraph& GC)
    throw() : managedObject(GC.Context()),
    G(const_cast<abstractMixedGraph&>(GC)),
    representation(listOfReprPars,TokReprEndSection,attributePool::ATTR_ALLOW_NULL),
    geometry(listOfGeometryPars,TokGeoEndSection,attributePool::ATTR_FULL_RANK),
    layoutData(listOfLayoutPars,TokLayoutEndSection,attributePool::ATTR_FULL_RANK)
{
    nMax = nAct = G.N();
    mMax = mAct = G.M();
    lMax = lAct = G.L();

    if (CT.randLength && mAct>0)
    {
        TFloat* length = representation.RawArray<TFloat>(G,TokReprLength);

        for (TArc a=0;a<mAct;a++) length[a] = TFloat(CT.SignedRand());

        LogEntry(LOG_MEM,"...Length labels allocated");
    }

    if (CT.randGeometry)
    {
        geometry.InitAttribute<TFloat>(G,TokGeoDim,2);
        TFloat* axis0 = geometry.RawArray<TFloat>(G,TokGeoAxis0);
        TFloat* axis1 = geometry.RawArray<TFloat>(G,TokGeoAxis1);

        for (TNode v=0;v<nAct;v++)
        {
            axis0[v] = TFloat(CT.SignedRand());
            axis1[v] = TFloat(CT.SignedRand());
        }

        LogEntry(LOG_MEM,"...Nodes embedded into plane");
    }

    LogEntry(LOG_MEM,"...Generic graph allocated");
}


void graphRepresentation::Reserve(TNode _n,TArc _m,TNode _l)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (nMax>0 || mMax>0 || lMax>0)
    {
        Error(ERR_REJECTED,"Reserve","Graph structure must be initial");
    }

    if (_n>=CT.MaxNode())
    {
        sprintf(CT.logBuffer,"Number of graph nodes is out of range: %lu",
            static_cast<unsigned long>(_n));
        Error(ERR_REJECTED,"Reserve",CT.logBuffer);
    }

    if (2*_m>CT.MaxArc()-2)
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(_m));
        Error(ERR_REJECTED,"Reserve",CT.logBuffer);
    }

    if (_l>=CT.MaxNode())
    {
        sprintf(CT.logBuffer,"Number of layout nodes is out of range: %lu",
            static_cast<unsigned long>(_l));
        Error(ERR_REJECTED,"Reserve",CT.logBuffer);
    }

    #endif

    nMax = nAct = _n;
    mMax = mAct = _m;
    lMax = lAct = _l;
}


size_t graphRepresentation::SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw()
{
    switch (arrayDim)
    {
        case DIM_GRAPH_NODES:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(nMax);
            }
            else return size_t(nAct);
        }
        case DIM_GRAPH_ARCS:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(mMax);
            }
            else return size_t(mAct);
        }
        case DIM_ARCS_TWICE:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(2*mMax);
            }
            else return size_t(2*mAct);
        }
        case DIM_LAYOUT_NODES:
        {
            if (size==SIZE_RESERVED)
            {
                return size_t(lMax);
            }
            else return size_t(lAct);
        }
        case DIM_SINGLETON:
        {
            return size_t(1);
        }
        default:
        {
            return size_t(0);
        }
    }
}


unsigned long graphRepresentation::Allocated()
    const throw()
{
    return 0;
}


graphRepresentation::~graphRepresentation()
    throw()
{
    LogEntry(LOG_MEM,"...Generic graph disallocated");
}


void graphRepresentation::SetUCap(TArc a,TCap _ucap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetUCap",a);

    if (_ucap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_ucap);
        Error(ERR_RANGE,"SetUCap",CT.logBuffer);
    }

    #endif

    if (Sub(a)>_ucap) SetSub(a,_ucap);

    attribute<TCap>* ucap = representation.GetAttribute<TCap>(TokReprUCap);

    if (!ucap)
    {
        if (_ucap==defaultUCap) return;

        ucap = representation.InitAttribute<TCap>(G,TokReprUCap,defaultUCap);
    }
    else if (ucap->Size()==0)
    {
        if (_ucap==ucap->DefaultValue()) return;

        ucap -> SetCapacity(mMax);
        ucap -> IncreaseSize(mAct);
    }

    ucap -> SetValue(a>>1,_ucap);
}


void graphRepresentation::SetCUCap(TCap _ucap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (_ucap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_ucap);
        Error(ERR_RANGE,"SetCUCap",CT.logBuffer);
    }

    #endif

    ReleaseSubgraph();

    if (_ucap!=defaultUCap)
    {
        representation.MakeAttribute<TCap>(G,TokReprUCap,attributePool::ATTR_ALLOW_NULL,&_ucap);
    }
    else
    {
        representation.ReleaseAttribute(TokReprUCap);
    }
}


void graphRepresentation::SetLCap(TArc a,TCap _lcap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetLCap",a);

    if (_lcap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_lcap);
        Error(ERR_RANGE,"SetLCap",CT.logBuffer);
    }

    #endif

    TFloat thisSub = Sub(a);

    if (fabs(thisSub)<_lcap)
    {
        if (thisSub>=0)
        {
            SetSub(a,_lcap);
        }
        else
        {
            SetSub(a,-_lcap);
        }
    }

    attribute<TCap>* lcap = representation.GetAttribute<TCap>(TokReprLCap);

    if (!lcap)
    {
        if (_lcap==defaultLCap) return;

        lcap = representation.InitAttribute<TCap>(G,TokReprLCap,defaultLCap);
    }
    else if (lcap->Size()==0)
    {
        if (_lcap==lcap->DefaultValue()) return;

        lcap -> SetCapacity(mMax);
        lcap -> IncreaseSize(mAct);
    }

    lcap -> SetValue(a>>1,_lcap);
}


void graphRepresentation::SetCLCap(TCap _lcap)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (_lcap<0)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %g",_lcap);
        Error(ERR_RANGE,"SetCLCap",CT.logBuffer);
    }

    #endif

    ReleaseSubgraph();

    if (_lcap!=defaultLCap)
    {
        representation.MakeAttribute<TCap>(G,TokReprLCap,attributePool::ATTR_ALLOW_NULL,&_lcap);
    }
    else
    {
        representation.ReleaseAttribute(TokReprLCap);
    }
}


void graphRepresentation::SetDemand(TNode v,TCap _demand)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("SetDemand",v);

    #endif

    attribute<TCap>* demand = representation.GetAttribute<TCap>(TokReprDemand);

    if (!demand)
    {
        if (_demand==defaultDemand) return;

        demand = representation.InitAttribute<TCap>(G,TokReprDemand,defaultDemand);
    }
    else if (demand->Size()==0)
    {
        if (_demand==demand->DefaultValue()) return;

        demand -> SetCapacity(nMax);
        demand -> IncreaseSize(nAct);
    }

    demand -> SetValue(v,_demand);
}


void graphRepresentation::SetCDemand(TCap _demand)
    throw()
{
    if (_demand!=defaultDemand)
    {
        representation.MakeAttribute<TCap>(G,TokReprDemand,attributePool::ATTR_ALLOW_NULL,&_demand);
    }
    else
    {
        representation.ReleaseAttribute(TokReprDemand);
    }
}


void graphRepresentation::SetMetricType(abstractMixedGraph::TMetricType metricType) throw()
{
    attribute<int>* attrMetric = geometry.GetAttribute<int>(TokGeoMetric);

    if (metricType==abstractMixedGraph::METRIC_DISABLED)
    {
        if (attrMetric) geometry.ReleaseAttribute(TokGeoMetric);
    }
    else
    {
        if (!attrMetric)
        {
            attrMetric = geometry.InitAttribute<int>(
                            *this,TokGeoMetric,int(abstractMixedGraph::METRIC_DISABLED));
        }

        attrMetric -> SetValue(0,int(metricType));
    }
}


TFloat graphRepresentation::Length(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("Length",a);

    #endif

    abstractMixedGraph::TMetricType metricType =
        abstractMixedGraph::TMetricType(geometry.GetValue<int>(
            TokGeoMetric,0,int(abstractMixedGraph::METRIC_DISABLED)));

    if (metricType==abstractMixedGraph::METRIC_DISABLED)
    {
        return representation.GetValue<TFloat>(TokReprLength,a>>1,defaultLength);
    }

    TNode v1 = G.StartNode(a);
    TNode v2 = G.EndNode(a);

    if (v1==v2) return InfFloat;

    TFloat x1 = G.C(v1,0);
    TFloat y1 = G.C(v1,1);
    TFloat x2 = G.C(v2,0);
    TFloat y2 = G.C(v2,1);

    if (metricType==abstractMixedGraph::METRIC_SPHERIC)
    {
        const double radius = 6378.388;

        double deg = int(x1);
        double min = x1-deg;
        double latitude1 = PI*(deg+5.0*min/3.0)/180.0;
        deg = int(y1);
        min = y1-deg;
        double longitude1 = PI*(deg+5.0*min/3.0)/180.0;

        deg = int(x2);
        min = x2-deg;
        double latitude2 = PI*(deg+5.0*min/3.0)/180.0;
        deg = int(y2);
        min = y2-deg;
        double longitude2 = PI*(deg+5.0*min/3.0)/180.0;

        double q1 = cos(longitude1-longitude2);
        double q2 = cos(latitude1-latitude2);
        double q3 = cos(latitude1+latitude2);
        return int(radius*acos(0.5*((1.0+q1)*q2-(1.0-q1)*q3))+1.0);
    }

    TFloat dx = fabs(x1-x2);
    TFloat dy = fabs(y1-y2);

    if (metricType==abstractMixedGraph::METRIC_MANHATTAN) return floor(dx+dy+0.5);
    if (metricType==abstractMixedGraph::METRIC_EUCLIDIAN) return floor(sqrt(dx*dx+dy*dy)+0.5);

    // metricType==METRIC_MAXIMUM
    return ((dx>dy) ? floor(dx+0.5) : floor(dy+0.5));
}


TFloat graphRepresentation::MaxLength()
    const throw()
{
    if (G.MetricType()==abstractMixedGraph::METRIC_DISABLED)
    {
        return representation.MaxValue<TFloat>(TokReprLength,defaultLength);
    }

    TFloat maxLength = -InfFloat;

    for (TArc a=0;a<mAct;a++)
    {
        TFloat thisLength = Length(2*a);

        if (thisLength>maxLength) maxLength = thisLength;
    }

    return maxLength;
}


void graphRepresentation::SetLength(TArc a,TFloat _length)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetLength",a);

    #endif

    attribute<TFloat>* length = representation.GetAttribute<TFloat>(TokReprLength);

    if (!length)
    {
        if (_length==defaultLength) return;

        length = representation.InitAttribute<TFloat>(G,TokReprLength,defaultLength);
    }
    else if (length->Size()==0)
    {
        if (_length==length->DefaultValue()) return;

        length -> SetCapacity(mMax);
        length -> IncreaseSize(mAct);
    }

    length -> SetValue(a>>1,_length);
}


void graphRepresentation::SetCLength(TFloat _length)
    throw()
{
    if (_length!=defaultLength)
    {
        representation.MakeAttribute<TFloat>(G,TokReprLength,attributePool::ATTR_ALLOW_NULL,&_length);
    }
    else
    {
        representation.ReleaseAttribute(TokReprLength);
    }
}


void graphRepresentation::SetOrientation(TArc a,char _orientation)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetOrientation",a);

    if (_orientation>=4)
        Error(ERR_RANGE,"SetOrientation","Illegal assignment");

    #endif

    attribute<char>* orientation = representation.GetAttribute<char>(TokReprOrientation);

    if (!orientation)
    {
        if (_orientation==defaultOrientation) return;

        orientation = representation.InitAttribute<char>(G,TokReprOrientation,defaultOrientation);
    }
    else if (orientation->Size()==0)
    {
        if (_orientation==orientation->DefaultValue()) return;

        orientation -> SetCapacity(mMax);
        orientation -> IncreaseSize(mAct);
    }

    orientation -> SetValue(a>>1,_orientation);
}


void graphRepresentation::SetCOrientation(char _orientation)
    throw()
{
    #if defined(_FAILSAVE_)

    if (_orientation>=4)
        Error(ERR_RANGE,"SetCOrientation","Illegal assignment");

    #endif

    if (_orientation!=defaultOrientation)
    {
        representation.MakeAttribute<char>(G,TokReprOrientation,attributePool::ATTR_ALLOW_NULL,&_orientation);
    }
    else
    {
        representation.ReleaseAttribute(TokReprOrientation);
    }
}


TDim graphRepresentation::Dim()
    const throw()
{
    attribute<TFloat>* coord = NULL;

    for (TDim i=3;i>0;)
    {
        --i;
        coord = geometry.GetAttribute<TFloat>(TokGeoAxis0+i);
        if (coord && coord->Size()>0)
        {
            if (coord->MaxValue()>coord->MinValue()) return i+1;
        }
        else
        {
            // Shrink to a constant ?
        }
    }

    return 0;
}


void graphRepresentation::SetC(TNode v,TDim i,TFloat pos)
    throw(ERRange)
{
   #if defined(_FAILSAVE_)

    if (v>=lAct)
        NoSuchNode("SetC",v);

    if (i>2) return;

    #endif

    attribute<TFloat>* axis = geometry.GetAttribute<TFloat>(TokGeoAxis0+i);

    if (!axis && pos!=defaultC)
    {
        axis = geometry.InitAttribute<TFloat>(G,TokGeoAxis0+i,defaultC);
    }

    if (axis) axis -> SetValue(v,pos);
}


void graphRepresentation::ReleaseCoordinate(TDim i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>2)
    {
        sprintf(CT.logBuffer,"No such coordinate: %lu",static_cast<unsigned long>(i));
        Error(ERR_RANGE,"ReleaseCoord",CT.logBuffer);
    }

    #endif

    geometry.ReleaseAttribute(TokGeoAxis0+i);
}


bool graphRepresentation::HiddenNode(TNode v)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct) NoSuchNode("HiddenNode",v);

    #endif

    return false;
}


bool graphRepresentation::HiddenArc(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("HiddenArc",a);

    #endif

    return false;
}
