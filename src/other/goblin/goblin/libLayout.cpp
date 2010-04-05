
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file libLayout.cpp
/// \brief Implementations of layout helper methods

#include "sparseRepresentation.h"
#include "staticStack.h"


TNode abstractMixedGraph::PortNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("PortNode",a);

    #endif

    TNode w = ArcLabelAnchor(a);

    if (w==NoNode) return NoNode;

    TNode x = ThreadSuccessor(w);

    if (x==NoNode) return NoNode;

    if (a%2==0) return x;

    do
    {
        w = x;
        x = ThreadSuccessor(w);
    }
    while (x!=NoNode);

    return w;
}


TNode sparseRepresentation::GetArcControlPoints(TArc a,TNode* layoutNode,TNode length,TPortMode portMode)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("GetArcControlPoints",a);

    #endif

    TNode nLayoutNodes = 0;

    if (portMode==PORTS_IMPLICIT)
    {
        layoutNode[nLayoutNodes] = StartNode(a);
        ++nLayoutNodes;
    }

    TNode w = ArcLabelAnchor(a);

    if (w==NoNode)
    {
        if (portMode==PORTS_IMPLICIT)
        {
            layoutNode[nLayoutNodes] = EndNode(a);
            ++nLayoutNodes;
        }

        return nLayoutNodes;
    }

    TNode x = ThreadSuccessor(w);

    if (x==NoNode)
    {
        if (portMode==PORTS_IMPLICIT)
        {
            layoutNode[nLayoutNodes] = EndNode(a);
            ++nLayoutNodes;
        }

        return nLayoutNodes;
    }

    if (a%2==0)
    {
        // Forward arc. Write in the thread order

        while (nLayoutNodes<length && x!=NoNode)
        {
            layoutNode[nLayoutNodes] = x;
            ++nLayoutNodes;
            x = ThreadSuccessor(x);
        }
    }
    else
    {
        // Backward arc. Write in the reverse thread order

        // First pass: Count control points
        do
        {
            x = ThreadSuccessor(x);
            ++nLayoutNodes;
        }
        while (x!=NoNode);

        // Second pass: Write list of layout nodes
        TNode indexToWrite = nLayoutNodes-1;
        x = ThreadSuccessor(w);

        do
        {
            if (indexToWrite<length) layoutNode[indexToWrite] = x;
            --indexToWrite;
            x = ThreadSuccessor(x);
        }
        while (x!=NoNode);
    }

    if (portMode==PORTS_IMPLICIT)
    {
        if (nLayoutNodes<length) layoutNode[nLayoutNodes] = EndNode(a);
        ++nLayoutNodes;
    }

    return nLayoutNodes;
}


TNode sparseRepresentation::ProvideArcLabelAnchor(TArc a)
    throw(ERRejected,ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("ProvideArcLabelAnchor",a);

    #endif

    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);

    if (!align)
    {
        align = layoutData.InitArray<TNode>(G,TokLayoutArcLabel,NoNode);
        LogEntry(LOG_MEM,"...Arc label points allocated");
    }

    if (align[a>>1]==NoNode)
    {
        align[a>>1] = InsertLayoutPoint();

        TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

        if (thread) thread[align[a>>1]] = NoNode;
    }

    return align[a>>1];
}


TNode sparseRepresentation::ProvidePortNode(TArc a)
    throw(ERRejected,ERRange)
{
    TNode v = ProvideArcLabelAnchor(a);

    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) thread = layoutData.InitArray<TNode>(G,TokLayoutThread,NoNode);

    TNode p = (thread[v]!=NoNode) ? thread[v] : InsertThreadSuccessor(v);

    thread = layoutData.GetArray<TNode>(TokLayoutThread);

    TNode q = (thread[p]!=NoNode) ? thread[p] : InsertThreadSuccessor(p);

    if (a%2==0) return p;

    while (thread[q]!=NoNode) q = thread[q];

    return q;
}


void sparseRepresentation::ProvideEdgeControlPoints(TArc a,TNode* layoutNode,TNode length,TPortMode portMode)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=mAct) NoSuchArc("ProvideEdgeControlPoints",a);

    #endif

    if (length==0 || (length<=2 && portMode==PORTS_IMPLICIT))
    {
        ReleaseEdgeControlPoints(2*a);
        GetArcControlPoints(2*a,layoutNode,length,portMode);
        return;
    }

    TNode nControlPoints = GetArcControlPoints(2*a,layoutNode,length,portMode);

    if (length==nControlPoints) return;

    ReleaseEdgeControlPoints(2*a);
    TNode w = ProvideArcLabelAnchor(2*a);
    w = InsertThreadSuccessor(w);
    nControlPoints = (portMode==PORTS_IMPLICIT) ? 3 : 1;

    while (nControlPoints<length)
    {
        w = InsertThreadSuccessor(w);
        ++nControlPoints;
    }

    GetArcControlPoints(2*a,layoutNode,length,portMode);
}


TNode sparseRepresentation::InsertArcControlPoint(TArc a,TNode x) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("InsertArcControlPoint",a);

    if (x==EndNode(a))
        Error(ERR_REJECTED,"InsertArcControlPoint","Cannot add a control point after the end node");

    #endif

    TNode w = ProvideArcLabelAnchor(a);

    if (!(a&1) && x==StartNode(a)) return ProvidePortNode(a);

    TNode z = ThreadSuccessor(w);
    TNode y = w;

    while (z!=x && z!=NoNode)
    {
        y = z;
        z = ThreadSuccessor(z);
    }

    #if defined(_FAILSAVE_)

    if (x!=z && x!=StartNode(a))
        Error(ERR_REJECTED,"InsertArcControlPoint","Invalid predecessor point");

    #endif

    if (a&1) return InsertThreadSuccessor(y);

    return InsertThreadSuccessor(z);
}


void abstractMixedGraph::ImportLayoutData(const abstractMixedGraph& G)
    throw()
{
    attributePool* layoutData  = LayoutData();
    attributePool* layoutDataG = G.LayoutData();

    if (layoutData && layoutDataG)
    {
        layoutDataG -> ExportAttributes(*layoutData);
        layoutData -> ReleaseAttribute(TokLayoutArcLabel);
        layoutData -> ReleaseAttribute(TokLayoutThread);
        layoutData -> ReleaseAttribute(TokLayoutHiddenNode);
        layoutData -> ReleaseAttribute(TokLayoutHiddenArc);
        layoutData -> ReleaseAttribute(TokLayoutExteriorArc);
    }
}


static bool GetDefaultLayoutParameter(TOptLayoutTokens token,int& defaultValue,TLayoutModel model)
    throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_INT) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_SINGLETON) return false;

    switch (token)
    {
        case TokLayoutModel:
        {
            defaultValue = static_cast<int>(LAYOUT_FREESTYLE_CURVES);
            return true;
        }
        case TokLayoutArcWidthMode:
        {
            defaultValue = static_cast<int>(graphDisplayProxy::ARC_WIDTH_UNIFORM);
            return true;
        }
        case TokLayoutArcStippleMode:
        {
            defaultValue = static_cast<int>(graphDisplayProxy::ARC_STIPPLE_OFF);
            return true;
        }
        case TokLayoutArcVisibilityMode :
        {
            defaultValue = static_cast<int>(graphDisplayProxy::ARC_DISPLAY_SHOW_ALL);
            return true;
        }
        case TokLayoutArcShapeMode :
        {
            switch (model)
            {
                case LAYOUT_STRAIGHT_2DIM:
                case LAYOUT_FREESTYLE_CURVES:
                {
                    defaultValue = static_cast<int>(canvasBuilder::ARC_SHAPE_SMOOTH);
                    return true;
                }
                case LAYOUT_LAYERED:
                case LAYOUT_FREESTYLE_POLYGONES:
                case LAYOUT_ORTHO_BIG:
                case LAYOUT_ORTHO_SMALL:
                case LAYOUT_VISIBILITY:
                case LAYOUT_KANDINSKI:
                {
                    defaultValue = static_cast<int>(canvasBuilder::ARC_SHAPE_POLYGONES);
                    return true;
                }
                case LAYOUT_DEFAULT:
                case LAYOUT_NONE:
                {
                    defaultValue = static_cast<int>(canvasBuilder::ARC_SHAPE_POLYGONES);
                    return true;
                }
            }
        }
        case TokLayoutArcColourMode :
        {
            defaultValue = static_cast<int>(graphDisplayProxy::ARCS_UNCOLOURED);
            return true;
        }
        case TokLayoutNodeShapeMode :
        {
            switch (model)
            {
                case LAYOUT_ORTHO_BIG:
                case LAYOUT_ORTHO_SMALL:
                case LAYOUT_VISIBILITY:
                case LAYOUT_KANDINSKI:
                {
                    defaultValue = static_cast<int>(NODE_SHAPE_BOX);
                    return true;
                }
                default:
                {
                    defaultValue = static_cast<int>(NODE_SHAPE_CIRCULAR);
                    return true;
                }
            }
        }
        case TokLayoutNodeColourMode :
        {
            defaultValue = static_cast<int>(graphDisplayProxy::NODES_UNCOLOURED);
            return true;
        }
        case TokLayoutNodeFontType :
        {
            defaultValue = static_cast<int>(canvasBuilder::FONT_HELVETICA_BOLD);
            return true;
        }
        case TokLayoutArcFontType :
        {
            defaultValue = static_cast<int>(canvasBuilder::FONT_HELVETICA_NARROW_OBLIQUE);
            return true;
        }
        case TokLayoutArrowDisplayMode:
        {
            defaultValue = static_cast<int>(graphDisplayProxy::ARROWS_ARC_ORIENTATION);
            return true;
        }
        case TokLayoutArrowPosMode:
        {
            defaultValue = static_cast<int>(canvasBuilder::ARROWS_AT_ENDS);
            return true;
        }
        case TokLayoutGridDisplayMode:
        {
            defaultValue = static_cast<int>(canvasBuilder::GRID_DISPLAY_OFF);
            return true;
        }
        case TokLayoutArcWidthMin:
        {
            defaultValue = static_cast<int>(1);
            return true;
        }
        case TokLayoutArcWidthMax:
        {
            defaultValue = static_cast<int>(3);
            return true;
        }
        default :
        {
        }
    }

    return false;
}


static bool GetDefaultLayoutParameter(TOptLayoutTokens token,double& defaultValue,TLayoutModel model)
    throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_DOUBLE) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_SINGLETON) return false;

    switch (token)
    {
        case TokLayoutNodeSize :
        case TokLayoutArrowSize :
        case TokLayoutNodeFontSize :
        case TokLayoutArcFontSize :
        {
            defaultValue = static_cast<double>(100.0);
            return true;
        }
        case TokLayoutFineSpacing :
        {
            defaultValue = static_cast<double>(1.0);
            return true;
        }
        case TokLayoutBendSpacing :
        {
            defaultValue = static_cast<double>(5.0);
            return true;
        }
        case TokLayoutNodeSpacing :
        {
            defaultValue = static_cast<double>(10.0);
            return true;
        }
        default :
        {
        }
    }

    return false;
}


static bool GetDefaultLayoutParameter(TOptLayoutTokens token,char*& defaultValue,TLayoutModel model)
    throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_CHAR) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_STRING) return false;

    switch (token)
    {
        case TokLayoutNodeLabelFormat :
        {
            defaultValue = "";
            return true;
        }
        case TokLayoutArcLabelFormat :
        {
            defaultValue = "";
            return true;
        }
        default :
        {
        }
    }

    return false;
}


bool abstractMixedGraph::GetLayoutParameter(const char* tokenStr,char* valueStr)
    const throw()
{
    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    int token = 0;

    for (;token<int(TokLayoutEndSection) &&
            strcmp(tokenStr,listOfLayoutPars[token].tokenLabel)!=0;++token) {};

    if (token==int(NoTokLayout)) return false;

    switch (listOfLayoutPars[token].arrayType)
    {
        case TYPE_INT:
        {
            if (token==TokLayoutModel)
            {
                sprintf(valueStr,"%i",LayoutModel());
                return true;
            }

            int* pValue = layoutData->GetArray<int>(token);

            if (pValue)
            {
                sprintf(valueStr,"%i",*pValue);
                return true;
            }

            int value;

            if (GetDefaultLayoutParameter(TOptLayoutTokens(token),value,LayoutModel()))
            {
                sprintf(valueStr,"%i",value);
                return true;
            }

            return false;
        }
        case TYPE_DOUBLE:
        {
            double* pValue = layoutData->GetArray<double>(token);

            if (pValue)
            {
                sprintf(valueStr,"%g",*pValue);
                return true;
            }

            double value;

            if (GetDefaultLayoutParameter(TOptLayoutTokens(token),value,LAYOUT_DEFAULT))
            {
                sprintf(valueStr,"%g",value);
                return true;
            }

            return false;
        }
        case TYPE_CHAR:
        {
            char* pValue = layoutData->GetArray<char>(token);

            if (pValue)
            {
                sprintf(valueStr,"%s",pValue);
                return true;
            }

            if (GetDefaultLayoutParameter(TOptLayoutTokens(token),pValue,LAYOUT_DEFAULT))
            {
                sprintf(valueStr,"%s",pValue);
                return true;
            }

            return false;
        }
        default:
        {
            // Other base types do not occur
        }
    }

    return false;
}


bool abstractMixedGraph::GetLayoutParameterImpl(
    TOptLayoutTokens token,int& value,TLayoutModel model) const throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_INT) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_SINGLETON) return false;

    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    int* pValue = layoutData->GetArray<int>(token);

    if (pValue)
    {
        value = *pValue;
        return true;
    }

    return GetDefaultLayoutParameter(token,value,model);
}


bool abstractMixedGraph::GetLayoutParameterImpl(
    TOptLayoutTokens token,double& value,TLayoutModel model) const throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_DOUBLE) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_SINGLETON) return false;

    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    double* pValue = layoutData->GetArray<double>(token);

    if (pValue)
    {
        value = *pValue;
        return true;
    }

    return GetDefaultLayoutParameter(token,value,model);
}


bool abstractMixedGraph::GetLayoutParameterImpl(
    TOptLayoutTokens token,char*& value,TLayoutModel model) const throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_CHAR) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_STRING) return false;

    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    value = layoutData->GetArray<char>(token);

    if (value) return true;

    return GetDefaultLayoutParameter(token,value,model);
}


bool abstractMixedGraph::SetLayoutParameter(const char* tokenStr,const char* valueStr) throw()
{
    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    int token = 0;

    for (;token<int(TokLayoutEndSection) &&
            strcmp(tokenStr,listOfLayoutPars[token].tokenLabel)!=0;++token) {};

    if (token==int(NoTokLayout)) return false;

    if (strcmp(valueStr,"*")==0)
    {
        layoutData -> ReleaseAttribute(token);
        return true;
    }

    switch (listOfLayoutPars[token].arrayType)
    {
        case TYPE_INT:
        {
            int value = atoi(valueStr);
            return SetLayoutParameter(TOptLayoutTokens(token),value,LayoutModel());
        }
        case TYPE_DOUBLE:
        {
            double value = atof(valueStr);
            return SetLayoutParameter(TOptLayoutTokens(token),value,LayoutModel());
        }
        case TYPE_CHAR:
        {
            return SetLayoutParameter(TOptLayoutTokens(token),valueStr,LayoutModel());
        }
        case TYPE_NODE_INDEX:
        case TYPE_ARC_INDEX:
        case TYPE_FLOAT_VALUE:
        case TYPE_CAP_VALUE:
        case TYPE_INDEX:
        case TYPE_ORIENTATION:
        case TYPE_BOOL:
        case TYPE_VAR_INDEX:
        case TYPE_RESTR_INDEX:
        case TYPE_SPECIAL:
        {
            // Unhandled so far
        }
    }

    return false;
}


bool abstractMixedGraph::SetLayoutParameterImpl(
    TOptLayoutTokens token,const int value,TLayoutModel model) throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_INT) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_SINGLETON) return false;

    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    int defaultValue;

    if (!GetDefaultLayoutParameter(token,defaultValue,model)) return false;

    if (value==defaultValue)
    {
        layoutData -> ReleaseAttribute(token);
    }
    else
    {
        layoutData -> InitAttribute(*this,token,value);
    }

    return true;
}


bool abstractMixedGraph::SetLayoutParameterImpl(
    TOptLayoutTokens token,const double value,TLayoutModel model) throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_DOUBLE) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_SINGLETON) return false;

    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    double defaultValue;

    if (!GetDefaultLayoutParameter(token,defaultValue,model)) return false;

    if (value==defaultValue)
    {
        layoutData -> ReleaseAttribute(token);
    }
    else
    {
        layoutData -> InitAttribute(*this,token,value);
    }

    return true;
}


bool abstractMixedGraph::SetLayoutParameterImpl(
    TOptLayoutTokens token,const char* value,TLayoutModel model) throw()
{
    if (listOfLayoutPars[token].arrayType!=TYPE_CHAR) return false;
    if (listOfLayoutPars[token].arrayDim!=DIM_STRING) return false;

    attributePool* layoutData = LayoutData();

    if (!layoutData) return false;

    char* defaultValue;

    if (!GetDefaultLayoutParameter(token,defaultValue,model)) return false;

    if (strcmp(defaultValue,value)==0)
    {
        layoutData -> ReleaseAttribute(token);
    }
    else
    {
        layoutData -> ImportArray<char>(token,value,strlen(value)+1);
    }

    return true;
}


TLayoutModel abstractMixedGraph::LayoutModel() const throw()
{
    int model = int(LAYOUT_FREESTYLE_CURVES);
    GetLayoutParameter(TokLayoutModel,model,LAYOUT_FREESTYLE_CURVES);
    return TLayoutModel(model);
}


void abstractMixedGraph::SetLayoutModel(TLayoutModel model) throw()
{
    if (!LayoutData()) return;

    int defaultModel = int(LAYOUT_FREESTYLE_CURVES);
    GetDefaultLayoutParameter(TokLayoutModel,defaultModel,LAYOUT_FREESTYLE_CURVES);

    if (model==TLayoutModel(defaultModel))
    {
        LayoutData() -> ReleaseAttribute(TokLayoutModel);
    }
    else
    {
        LayoutData() -> InitAttribute<int>(*this,TokLayoutModel,int(model));
    }

    LayoutData() -> ReleaseAttribute(TokLayoutArcShapeMode);
    LayoutData() -> ReleaseAttribute(TokLayoutNodeShapeMode);
    LayoutData() -> ReleaseAttribute(TokLayoutArrowSize);
}


void abstractMixedGraph::Layout_ConvertModel(TLayoutModel model) throw()
{
    TLayoutModel currentModel = LayoutModel();

    if (model==LAYOUT_DEFAULT)
    {
        int defaultModel = int(LAYOUT_FREESTYLE_CURVES);
        GetDefaultLayoutParameter(TokLayoutModel,defaultModel,LAYOUT_FREESTYLE_CURVES);
        model = TLayoutModel(defaultModel);
    }

    if (model==currentModel)
    {
        // Do nothing but reverting to the default parameters
        SetLayoutModel(model);
        return;
    }

    if (model==LAYOUT_STRAIGHT_2DIM)
    {
        // Clean up the drawing

        if (IsSparse() && Representation())
        {
            sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

            for (TNode v=0;v<n;++v) X -> ReleaseNodeControlPoints(v);

            for (TArc a=0;a<m;++a) X -> ReleaseEdgeControlPoints(2*a);

            for (TDim i=2;i<Dim();++i) X -> ReleaseCoordinate(i);
        }
    }
    else if (   model==LAYOUT_ORTHO_BIG
             && (   currentModel==LAYOUT_ORTHO_SMALL
                 || currentModel==LAYOUT_VISIBILITY
                 || currentModel==LAYOUT_KANDINSKI
                )
            )
    {
        // Transforming into a more general layout model does not
        // require any manipulations of the drawing. If the previous
        // drawing was in the Kandinsky model, some grid lines may
        // be saved, but this is not implemented yet
    }
    else if (currentModel!=LAYOUT_STRAIGHT_2DIM)
    {
        // As a default rule, convert from one model to another
        // by using the straight line model as an intermediate step
        Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    }

    SetLayoutModel(model);

    switch (model)
    {
        case LAYOUT_FREESTYLE_CURVES:
        case LAYOUT_FREESTYLE_POLYGONES:
        case LAYOUT_LAYERED:
        {
            TFloat spacing;
            GetLayoutParameter(TokLayoutNodeSpacing,spacing);
            SetLayoutParameter(TokLayoutBendSpacing,spacing/4.0,model);
            SetLayoutParameter(TokLayoutNodeSize,200.0,model);

            if (IsSparse() && Representation())
            {
                static_cast<sparseRepresentation*>(Representation()) -> Layout_ArcRouting();
            }
        }
        case LAYOUT_STRAIGHT_2DIM:
        {
            SetLayoutParameter(TokLayoutNodeSize,100.0,model);
            SetLayoutParameter(TokLayoutFineSpacing,0.0,model);
            break;
        }
        case LAYOUT_KANDINSKI:
        case LAYOUT_VISIBILITY:
        case LAYOUT_ORTHO_BIG:
        {
            SetLayoutParameter(TokLayoutNodeSize,500.0,model);

            TFloat spacing;
            GetLayoutParameter(TokLayoutNodeSpacing,spacing);
            SetLayoutParameter(TokLayoutBendSpacing,spacing/4.0,model);
            SetLayoutParameter(TokLayoutFineSpacing,spacing/8.0,model);
            break;
        }
        case LAYOUT_ORTHO_SMALL:
        {
            SetLayoutParameter(TokLayoutNodeSize,125.0,model);

            TFloat spacing;
            GetLayoutParameter(TokLayoutNodeSpacing,spacing);
            SetLayoutParameter(TokLayoutBendSpacing,spacing,model);
            SetLayoutParameter(TokLayoutFineSpacing,spacing/4.0,model);
            break;
        }
        case LAYOUT_DEFAULT:
        case LAYOUT_NONE:
        {
            break;
        }
    }
}


void abstractMixedGraph::SyncSpacingParameters(TOptLayoutTokens token,TFloat& spacing) throw()
{
    if (spacing<CT.epsilon)
    {
        GetLayoutParameter(token,spacing);
    }
    else
    {
        SetLayoutParameter(token,spacing);
    }
}


void abstractMixedGraph::WriteLayout(goblinExport* F) const throw()
{
    if (!LayoutData()) return;

    LayoutData() -> WritePool(*this,*F,"layout");
}


void abstractMixedGraph::ReadLayoutData(goblinImport& F) throw(ERParse)
{
    LayoutData() -> ReadPool(*this,F);

    TArc* pExteriorArc = LayoutData()->GetArray<TArc>(TokLayoutExteriorArc);

    if (pExteriorArc) MarkExteriorFace(*pExteriorArc);
}


void abstractMixedGraph::Layout_AlignWithOrigin() throw(ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("Layout_AlignWithOrigin");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_AlignWithOrigin","Coordinates are fixed");

    #endif

    if (n+ni == 0) return;

    for (TDim i=0;i<Dim();++i)
    {
        TFloat cMin = 0.0;
        TFloat cMax = 0.0;

        Layout_GetBoundingInterval(i,cMin,cMax);

        TFloat spacing = 0.0;
        GetLayoutParameter(TokLayoutNodeSpacing,spacing);

        TFloat shift = floor(cMin/spacing+0.5)*spacing;
/*
    if (MetricType()==METRIC_SPHERIC)
    {
        minX = floor(minX);
        minY = floor(minY);
    }
*/
        Layout_TransformCoordinate(i,cMin-shift,cMax-shift);
    }
}


void abstractMixedGraph::Layout_TransformCoordinate(TDim i,TFloat cMin,TFloat cMax)
    throw(ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("Layout_TransformCoordinate");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_TransformCoordinate","Coordinates are fixed");

    #endif

    TFloat oldMin = 0.0,oldMax = 0.0;
    Layout_GetBoundingInterval(i,oldMin,oldMax);

    if (fabs(oldMax-oldMin) < CT.epsilon)
    {
        // Degenerate case

        for (TNode v=0;v<n+ni;v++) X -> SetC(v,i,(cMax-cMin)/2);
    }
    else
    {
        // Regular case

        for (TNode v=0;v<n+ni;v++)
        {
            X -> SetC(v,i,cMin+(C(v,i)-oldMin)*(cMax-cMin)/(oldMax-oldMin));
        }
    }

    if (IsSparse())
        static_cast<sparseRepresentation*>(X) -> Layout_SetBoundingInterval(i,cMin,cMax);
}


void abstractMixedGraph::Layout_GetBoundingInterval(TDim i,TFloat& cMin,TFloat& cMax) const throw()
{
    TNode* pMin = NULL;
    TNode* pMax = NULL;

    if (Geometry())
    {
        pMin = Geometry()->GetArray<TNode>(TokGeoMinBound);
        pMax = Geometry()->GetArray<TNode>(TokGeoMaxBound);
    }

    if (pMin && pMax)
    {
        cMin = C(*pMin,i);
        cMax = C(*pMax,i);
    }
    else
    {
        cMin = C(0,i);
        cMax = C(0,i);

        for (TNode v=1;v<n;++v)
        {
            if (C(v,i)!=InfFloat)
            {
                cMin = (C(v,i)<cMin) ? C(v,i) : cMin;
                cMax = (C(v,i)>cMax) ? C(v,i) : cMax;
            }
        }

        TFloat spacing = 0.0;
        GetLayoutParameter(TokLayoutNodeSpacing,spacing);

        TLayoutModel model = LayoutModel();

        if (   model==LAYOUT_STRAIGHT_2DIM
            || model==LAYOUT_FREESTYLE_CURVES
            || model==LAYOUT_KANDINSKI
           )
        {
            // For the bounding box, consider the graph nodes only
        }
        else
        {
            // This addresses all kinds of grid layout models:
            // For the bounding box, also consider the arc control points
            for (TArc a=0;a<m;++a)
            {
                TNode p = PortNode(2*a);

                while (p!=NoNode)
                {
                    if (C(p,i)!=InfFloat)
                    {
                        cMin = (C(p,i)<cMin) ? C(p,i) : cMin;
                        cMax = (C(p,i)>cMax) ? C(p,i) : cMax;
                    }

                    p = ThreadSuccessor(p);
                }
            }
        }

        cMin -= spacing;
        cMax += spacing;
    }
}


void abstractMixedGraph::Layout_DefaultBoundingBox() throw()
{
    Layout_ReleaseBoundingBox();
    Layout_FreezeBoundingBox();
}


void abstractMixedGraph::Layout_FreezeBoundingBox() throw()
{
    if (!Representation() || IsDense()) return;

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    for (TDim i=0;i<Dim();++i)
    {
        TFloat cMin = 0.0;
        TFloat cMax = 0.0;

        Layout_GetBoundingInterval(i,cMin,cMax);
        X -> Layout_SetBoundingInterval(i,cMin,cMax);
    }
}


void sparseRepresentation::Layout_AdoptBoundingBox(abstractMixedGraph& H) throw()
{
    if (!H.Representation() || H.IsDense() || H.Dim()==0) return;

    TNode* pLower = H.Geometry()->GetArray<TNode>(TokGeoMinBound);
    TNode* pUpper = H.Geometry()->GetArray<TNode>(TokGeoMaxBound);

    if (!pLower || !pUpper) return;

    for (TDim i=0;i<H.Dim();++i)
    {
        Layout_SetBoundingInterval(i,H.C(*pLower,i),H.C(*pUpper,i));
    }
}


void sparseRepresentation::Layout_SetBoundingInterval(TDim i,TFloat cMin,TFloat cMax) throw()
{
    TNode* pMin = geometry.GetArray<TNode>(TokGeoMinBound);
    TNode* pMax = geometry.GetArray<TNode>(TokGeoMaxBound);

    if (!pMin || !pMax)
    {
        if (pMin || pMax)
            InternalError("Layout_SetBoundingInterval","Bounding box is corrupt");

        TFloat defMin[3] = {0.0,0.0,0.0};
        TFloat defMax[3] = {0.0,0.0,0.0};

        TDim j = 0;
        for (;j<Dim() && j<3;++j)
        {
            G.Layout_GetBoundingInterval(j,defMin[j],defMax[j]);
        }

        pMin = geometry.RawArray<TNode>(G,TokGeoMinBound);
        pMax = geometry.RawArray<TNode>(G,TokGeoMaxBound);
        *pMin = InsertLayoutPoint();
        *pMax = InsertLayoutPoint();

        for (j=0;j<Dim();++j)
        {
            SetC(*pMin,j,defMin[j]);
            SetC(*pMax,j,defMax[j]);
        }
    }

    SetC(*pMin,i,cMin);
    SetC(*pMax,i,cMax);
}


void abstractMixedGraph::Layout_ReleaseBoundingBox() throw()
{
    attributePool* geometry = Geometry();

    if (!geometry || IsDense()) return;

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TNode* pLower = geometry->GetArray<TNode>(TokGeoMinBound);
    TNode* pUpper = geometry->GetArray<TNode>(TokGeoMaxBound);

    if (pLower && pUpper)
    {
        if ((*pUpper)>(*pLower))
        {
            X -> DeleteNode(*pUpper);
            X -> DeleteNode(*pLower);
        }
        else
        {
            X -> DeleteNode(*pLower);
            X -> DeleteNode(*pUpper);
        }

        geometry -> ReleaseAttribute(TokGeoMinBound);
        geometry -> ReleaseAttribute(TokGeoMaxBound);
    }
    else if (pUpper)
    {
        X -> DeleteNode(*pUpper);
        geometry -> ReleaseAttribute(TokGeoMaxBound);
    }
    else if (pLower)
    {
        X -> DeleteNode(*pLower);
        geometry -> ReleaseAttribute(TokGeoMinBound);
    }
}


void sparseRepresentation::Layout_ArcRouting(TFloat spacing,bool drawLoops) throw()
{
    if (Dim()<2) return;

    LogEntry(LOG_METH,"Release edge control points...");

    THandle H = G.Investigate();
    investigator &I = G.Investigator(H);
    staticStack<TArc> S(2*mAct,CT);

    G.SyncSpacingParameters(TokLayoutBendSpacing,spacing);

    for (TArc a=0;a<mAct;a++) ReleaseEdgeControlPoints(2*a);

    TNode controlPoint[2];

    for (TNode u=0;u<nAct;u++)
    {
        for (TNode v=u;v<nAct;v++)
        {
            S.Init();

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                if (EndNode(a)==v && (u!=v || (a%2))) S.Insert(a);
            }

            I.Reset(u);

            if (v!=u)
            {
                if (S.Cardinality()>1)
                {
                    TFloat dx = C(v,0)-C(u,0);
                    TFloat dy = C(v,1)-C(u,1);
                    TFloat norm = sqrt(dx*dx+dy*dy);
                    TFloat ox = dy/norm;
                    TFloat oy = -dx/norm;
                    TFloat cx = (C(v,0)+C(u,0))/2-spacing*(S.Cardinality()-1)/2*ox;
                    TFloat cy = (C(v,1)+C(u,1))/2-spacing*(S.Cardinality()-1)/2*oy;

                    while (!S.Empty())
                    {
                        TArc a = S.Delete();

                        TNode w = ProvideArcLabelAnchor(a);
                        SetC(w,0,cx);
                        SetC(w,1,cy);

                        ProvideEdgeControlPoints(a>>1,controlPoint,1,PORTS_EXPLICIT);
                        SetC(controlPoint[0],0,cx);
                        SetC(controlPoint[0],1,cy);

                        cx += spacing*ox;
                        cy += spacing*oy;
                    }
                }
            }
            else if (drawLoops)
            {
                TFloat cx = C(v,0);
                TFloat cy = C(v,1)+spacing;

                while (!S.Empty())
                {
                    TArc a = S.Delete();

                    TNode w = ProvideArcLabelAnchor(a);
                    SetC(w,0,cx);
                    SetC(w,1,cy);

                    ProvideEdgeControlPoints(a>>1,controlPoint,2,PORTS_EXPLICIT);
                    SetC(controlPoint[0],0,cx+spacing/4);
                    SetC(controlPoint[0],1,cy);
                    SetC(controlPoint[1],0,cx-spacing/4);
                    SetC(controlPoint[1],1,cy);

                    cy += spacing;
                }
            }
        }
    }

    G.Close(H);
}


void sparseRepresentation::Layout_SubdivideArcs(TFloat spacing) throw()
{
    if (Dim()<2) return;

    LogEntry(LOG_METH,"Subdivide arcs...");

    G.SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    TFloat fineSpacing = 0.0;
    G.GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);
    G.Layout_ConvertModel(LAYOUT_LAYERED);

    for (TArc a=0;a<mAct;++a)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (v==u) continue;

        TFloat cu = C(u,1);
        TFloat cv = C(v,1);

        cu = ceil(cu/spacing-0.5)*spacing;
        cv = ceil(cv/spacing-0.5)*spacing;

        if (fabs(cv-cu)<spacing*1.5)
        {
            // Short arcs (which do not cross any layer) do not need control points
            ReleaseEdgeControlPoints(2*a);
            continue;
        }

        // Check if the current routing of arc a is valid
        TFloat sign = 1-2*(cv<cu);
        TNode w = ArcLabelAnchor(2*a);
        TFloat cw = cu;
        bool needsRerouting = (w==NoNode);

        while (!needsRerouting && fabs(cv-cw)>=spacing*1.5)
        {
            w = ThreadSuccessor(w);
            cw += sign*spacing;

            if (w==NoNode || fabs(C(w,1)-cw)>0.5*spacing) needsRerouting = true;
        }

        if (!needsRerouting && ThreadSuccessor(w)!=NoNode) needsRerouting = true;

        if (!needsRerouting) continue;

        ReleaseEdgeControlPoints(2*a);

        TFloat cm = (C(v,0)+C(u,0))/2;
        w = ProvideArcLabelAnchor(2*a);
        cw = cu+sign*spacing;
        SetC(w,0,cm+fineSpacing);
        SetC(w,1,(C(v,1)+C(u,1))/2);
        w = ProvidePortNode(2*a);
        SetC(w,0,C(v,0)*(cw-cu)/(cv-cu)+C(u,0)*(cv-cw)/(cv-cu));
        SetC(w,1,cw);

        while (fabs(cv-cw)>=spacing*1.5)
        {
            w = InsertThreadSuccessor(w);
            cw += sign*spacing;
            SetC(w,0,C(v,0)*(cw-cu)/(cv-cu)+C(u,0)*(cv-cw)/(cv-cu));
            SetC(w,1,cw);
        }
    }
}


void sparseRepresentation::Layout_AdoptArcRouting(abstractMixedGraph& G)
    throw()
{
    for (TArc a=0;a<mAct;a++)
    {
        TNode v = G.ArcLabelAnchor(2*a);

        if (v==NoNode) continue;

        TNode w = ProvideArcLabelAnchor(2*a);

        SetC(w,0,G.C(v,0));
        SetC(w,1,G.C(v,1));

        v = G.PortNode(2*a);

        while (v!=NoNode)
        {
            w = InsertThreadSuccessor(w);

            SetC(w,0,G.C(v,0));
            SetC(w,1,G.C(v,1));

            v = G.ThreadSuccessor(v);
        }
    }
}


void sparseRepresentation::Layout_GetNodeRange(TNode v,TDim i,TFloat& cMin,TFloat& cMax)
    throw()
{
    TNode p = G.ThreadSuccessor(v);

    TFloat cSpan = (p==NoNode) ? 0.0 : G.C(p,i);

    cMin = G.C(v,i)-cSpan;
    cMax = G.C(v,i)+cSpan;
}


void sparseRepresentation::Layout_SetNodeRange(TNode v,TDim i,TFloat cMin,TFloat cMax)
    throw()
{
    SetC(v,i,(cMin+cMax)/2.0);

    if (fabs(cMax-cMin)<CT.epsilon)
    {
        TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

        if (thread[v]!=NoNode)
        {
            for (TDim j=0;j<Dim();++j)
            {
                if (fabs(C(v,j)-C(thread[v],j))>CT.epsilon) return;
            }

            EraseLayoutNode(thread[v]);
            thread[v] = NoNode;
        }

        return;
    }

    TNode p = G.ThreadSuccessor(v);

    if (p==NoNode) p = InsertThreadSuccessor(v);

    SetC(p,i,(cMax-cMin)/2.0);
}


void abstractMixedGraph::Layout_Circular(TFloat spacing) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_Circular","Coordinates are fixed");

    #endif


    // If an outerplanar representation is available, compute a respective layout

    if (!IsDense() && m<=TArc(2*n-3) && Layout_Outerplanar(spacing)) return;

    if (GetPredecessors())
    {
        Layout_CircularByPredecessors(spacing);
    }
    else
    {
        Layout_CircularByColours(spacing);
    }
}


void abstractMixedGraph::Layout_CircularByPredecessors(TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_CircularByPredecessors",
            "Coordinates are fixed");

    #endif

    TArc* pred = GetPredecessors();

    if (pred)
    {
        // If a Hamiltonian cycle is available, display this

        TNode* index = new TNode[n];
        bool* pending = new bool[n];
        for (TNode v=0;v<n;v++) pending[v] = true;
        TNode thisPos = 0;

        for (TNode u=0;u<n;u++)
        {
            TNode v = u;

            while (pred[v]!=NoArc && pending[v])
            {
                index[thisPos++] = v;
                pending[v] = false;
                v = StartNode(pred[v]);
            }

            if (pending[v])
            {
                index[thisPos++] = v;
                pending[v] = false;
            }
        }

        delete[] pending;

        Layout_AssignCircularCoordinates(spacing,index);

        delete[] index;
    }
    else
    {
        // Display nodes in the order of indices

        Layout_AssignCircularCoordinates(spacing);
    }

    Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);
}


void abstractMixedGraph::Layout_CircularByColours(TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_CircularByColours",
            "Coordinates are fixed");

    #endif

    TNode* nodeColour = GetNodeColours();

    if (nodeColour)
    {
        // Display the order or clusters available by the
        // node colour labels. If no colours are present,
        // display the nodes in arbitrary order.

        goblinQueue<TNode,TFloat> *Q = NULL;

        if (nHeap!=NULL)
        {
            Q = nHeap;
            Q -> Init();
        }
        else Q = NewNodeHeap();

        for (TNode u=0;u<n;u++) Q->Insert(u,n*nodeColour[u]+u);

        TNode* index = new TNode[n];

        for (TNode u=0;u<n;u++) index[u] = Q->Delete();

        if (nHeap==NULL) delete Q;

        Layout_AssignCircularCoordinates(spacing,index);

        delete[] index;
    }
    else
    {
        // Display nodes in the order of indices

        Layout_AssignCircularCoordinates(spacing);
    }

    Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);
}


bool abstractMixedGraph::Layout_Outerplanar(TFloat spacing) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (IsDense())
        Error(ERR_REJECTED,"Layout_Outerplanar","Not an outerplanar graph");

    #endif

    TNode* thread = new TNode[n];
    for (TNode v=0;v<n;v++) thread[v] = NoNode;

    TNode tail = NoNode;
    TNode l = 0;

    for (TNode u=0;u<n;u++)
    {
        if (thread[u]!=NoNode || u==tail) continue;

        if (tail!=NoNode)
        {
            thread[tail] = u;
            l++;
        }

        tail = u;

        if (First(u)==NoArc) continue;

        TArc a = Right(First(u),u);
        TArc k = 0;

        while ((a^1)!=First(u))
        {
            TNode w = EndNode(a);

            if (thread[w]==NoNode && tail!=w)
            {
                thread[tail] = w;
                tail = w;
                l++;
            }

            a = Right(a^1,w);
            k++;

            if (k>2*m)
            {
                // Not a planar representation, leave the outer loop
                u = n;
                break;
            }
        }
    }

    if (l==n-1)
    {
        SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
        Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);

        TFloat rad = spacing*n/2/PI;

        TNode v = 0;
        TNode k = 0;

        while (v!=NoNode)
        {
            SetC(v,0,rad*(1+cos(k*2*PI/n)));
            SetC(v,1,rad*(1+sin(k*2*PI/n)));
            v = thread[v];
            k++;
        }

        if (Representation() && IsSparse())
        {
            static_cast<sparseRepresentation*>(Representation()) ->
                Layout_SetBoundingInterval(0,-spacing,2*rad+spacing);
            static_cast<sparseRepresentation*>(Representation()) ->
                Layout_SetBoundingInterval(1,-spacing,2*rad+spacing);
        }

        if (CT.methLocal==LOCAL_OPTIMIZE) Layout_ForceDirected(FDP_RESTRICTED);
    }

    delete[] thread;

    return (l==n-1);
}


void abstractMixedGraph::Layout_AssignCircularCoordinates(TFloat spacing,TNode* index)
    throw(ERRejected)
{
    if (spacing>0)
    {
        SetLayoutParameter(TokLayoutFineSpacing,0.0,LAYOUT_STRAIGHT_2DIM);
        SetLayoutParameter(TokLayoutBendSpacing,spacing,LAYOUT_STRAIGHT_2DIM);
        SetLayoutParameter(TokLayoutNodeSpacing,spacing,LAYOUT_STRAIGHT_2DIM);
    }
    else
    {
        GetLayoutParameter(TokLayoutNodeSpacing,spacing);
        SetLayoutParameter(TokLayoutFineSpacing,0.0,LAYOUT_STRAIGHT_2DIM);
        SetLayoutParameter(TokLayoutBendSpacing,spacing,LAYOUT_STRAIGHT_2DIM);
    }

    TFloat rad = spacing*n/2/PI;

    for (TNode u=0;u<n;u++)
    {
        TNode v = u;

        if (index) v = index[u];

        SetC(v,0,rad*(1+cos(u*2*PI/n)));
        SetC(v,1,rad*(1+sin(u*2*PI/n)));
    }

    if (Representation() && IsSparse())
    {
        static_cast<sparseRepresentation*>(Representation()) ->
            Layout_SetBoundingInterval(0,-spacing,2*rad+spacing);
        static_cast<sparseRepresentation*>(Representation()) ->
            Layout_SetBoundingInterval(1,-spacing,2*rad+spacing);
    }
}


void abstractMixedGraph::Layout_Equilateral(TFloat spacing) throw(ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("Layout_Equilateral");

    if (ExtractEmbedding(PLANEXT_DEFAULT)==NoNode)
    {
        Error(ERR_REJECTED,"Layout_Equilateral","Graph is not embedded");
    }

    #endif

    TArc exteriorArc = ExteriorArc();
    TNode fExterior = face[exteriorArc];

    #if defined(_FAILSAVE_)

    if (face[exteriorArc^1]==fExterior)
        Error(ERR_REJECTED,"Layout_Equilateral","Graph must be 2-connected");

    #endif

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);

    bool* placed = new bool[n];
    for (TNode v=0;v<n;v++) placed[v] = false;

    TArc aBasis = exteriorArc^1;
    X -> SetC(StartNode(aBasis),0,0);
    X -> SetC(StartNode(aBasis),1,0);
    X -> SetC(EndNode(aBasis),0,spacing);
    X -> SetC(EndNode(aBasis),1,0);

    staticStack<TArc> Q(2*m,CT);
    Q.Insert(aBasis);

    while (!Q.Empty())
    {
        aBasis = Q.Delete();

        TNode u = StartNode(aBasis);
        TNode v = EndNode(aBasis);

        if (u==v)
        {
            delete[] placed;
            Error(ERR_REJECTED,"Layout_Equilateral","Graph contains loops");
        }

        // Scan the face of aBase the first time: Determine the degree
        // of this face and memorize all adjacent faces
        TArc a = Right(aBasis^1,EndNode(aBasis));
        TNode degree = 1;

        while (a!=aBasis)
        {
            if (face[a^1]!=fExterior) Q.Insert(a^1);

            a = Right(a^1,EndNode(a));
            degree++;
        }

        TFloat radMax = spacing*0.5/sin(PI/degree);
        TFloat radMin = radMax*cos(PI/degree);
        TFloat ox = C(v,1)-C(u,1);
        TFloat oy = C(u,0)-C(v,0);
        TFloat norm = sqrt(ox*ox+oy*oy);
        TFloat xCenter = (C(u,0)+C(v,0))/2+ox/norm*radMin;
        TFloat yCenter = (C(u,1)+C(v,1))/2+oy/norm*radMin;
        TFloat alpha0 = atan2(C(u,0)-xCenter,C(u,1)-yCenter);

        // Scan this face the second time and assign coordinates
        a = Right(aBasis^1,v);
        TNode w = EndNode(a);
        TNode k = 2;

        while (w!=u)
        {
            X -> SetC(w,0,xCenter+radMax*sin(alpha0 + k*2*PI/degree));
            X -> SetC(w,1,yCenter+radMax*cos(alpha0 + k*2*PI/degree));

            a = Right(a^1,w);
            w = EndNode(a);
            k++;
        }
    }

    Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);

    Layout_DefaultBoundingBox();

    delete[] placed;
}
