
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file graphDisplayProxy.cpp
/// \brief #graphDisplayProxy class implementation

#include "graphDisplayProxy.h"
#include "sparseRepresentation.h"


graphDisplayProxy::graphDisplayProxy(abstractMixedGraph &_G,double _pixelWidth,double _pixelHeight)
    throw(ERRejected) :
    managedObject(_G.Context()),G(_G), CFG(_G.Context(),true),
    piG(G.GetPotentials()),pixelWidth(_pixelWidth),pixelHeight(_pixelHeight)
{
    Synchronize();

    LogEntry(LOG_MEM,"...Graph display proxy instanciated");
}


graphDisplayProxy::~graphDisplayProxy() throw()
{
    LogEntry(LOG_MEM,"...Graph display proxy disallocated");
}


unsigned long graphDisplayProxy::Allocated() const throw()
{
    return 0;
}


unsigned long graphDisplayProxy::Size() const throw()
{
    return 255;
}


void graphDisplayProxy::ExtractLayoutParameters() throw()
{
    // Copy parameters from the graph layout pool or set default values
    G.GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);
    G.GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);
    G.GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    G.GetLayoutParameter(TokLayoutNodeSize,nodeSizeRel);
    G.GetLayoutParameter(TokLayoutNodeFontSize,nodeFontSize);
    G.GetLayoutParameter(TokLayoutArcFontSize,arcFontSize);
    G.GetLayoutParameter(TokLayoutArrowSize,arrowSizeRel);

    // For the time being, the page size is DIN A4, hardcoded
    canvasWidth = 297.0;
    canvasHeight = 210.0;

    // For the time being, the page margins are hard-coded
    leftMargin = topMargin = rightMargin = bottomMargin = 0.05;

    globalZoom = CFG.displayZoom;

    TLayoutModel layoutModel = G.LayoutModel();

    char* pNodeLabelFormat = NULL;
    G.GetLayoutParameter(TokLayoutNodeLabelFormat,pNodeLabelFormat);
    strncpy(nodeLabelFormat,pNodeLabelFormat,LABEL_BUFFER_SIZE);
    nodeLabelFormat[LABEL_BUFFER_SIZE-1] = 0;

    G.GetLayoutParameter(TokLayoutNodeShapeMode,nodeShapeMode,layoutModel);
    G.GetLayoutParameter(TokLayoutNodeColourMode,nodeColourMode);

    char* pArcLabelFormat = NULL;
    G.GetLayoutParameter(TokLayoutArcLabelFormat,pArcLabelFormat);
    strncpy(arcLabelFormat,pArcLabelFormat,LABEL_BUFFER_SIZE);
    arcLabelFormat[LABEL_BUFFER_SIZE-1] = 0;

    G.GetLayoutParameter(TokLayoutArcVisibilityMode,arcVisibilityMode);
    G.GetLayoutParameter(TokLayoutArrowDisplayMode,arrowDisplayMode);
    G.GetLayoutParameter(TokLayoutArcStippleMode,arcStippleMode);
    G.GetLayoutParameter(TokLayoutArcColourMode,arcColourMode);
    G.GetLayoutParameter(TokLayoutArcWidthMode,arcWidthMode);
    G.GetLayoutParameter(TokLayoutArcWidthMin,arcWidthMin);
    G.GetLayoutParameter(TokLayoutArcWidthMax,arcWidthMax);

    // To enforce a consistent set of layout parameters
    arcWidthMin = (arcWidthMin<1) ? 1 : arcWidthMin;
    arcWidthMax = (arcWidthMax>arcWidthMin) ? arcWidthMax : arcWidthMin;

    if (G.IsDense() && arcVisibilityMode==ARC_DISPLAY_SHOW_ALL)
    {
        arcVisibilityMode = ARC_DISPLAY_PRED_SUB;
    }
}


void graphDisplayProxy::DetermineCoordinateTransformation() throw()
{
    for (TDim i=0;i<3&& i<G.Dim();++i)
    {
        G.Layout_GetBoundingInterval(i,min[i],max[i]);
        originalRange[i] = (max[i]-min[i] > 0.5*nodeSpacing) ? max[i]-min[i] : 0.0;
    }

    ix = 0;
    iy = 1;

    double zoomX = (originalRange[ix]>0.0) ?
        canvasWidth *(1.0-rightMargin-leftMargin)/originalRange[ix] : 10000.0;
    double zoomY = (originalRange[iy]>0.0) ?
        canvasHeight*(1.0-bottomMargin-topMargin)/originalRange[iy] : 10000.0;

    objectZoom = (zoomX<zoomY) ? zoomX : zoomY;
    double totalZoom = objectZoom*globalZoom;

    offset[ix] = ((leftMargin+1.0-rightMargin )*0.5*canvasWidth  - (min[ix]+max[ix])*0.5*objectZoom);
    offset[iy] = ((topMargin +1.0-bottomMargin)*0.5*canvasHeight - (min[iy]+max[iy])*0.5*objectZoom);

    double fontScaleFactor = totalZoom * 0.1 * bendSpacing * nodeSizeRel / 100.0;
    arcFontSize  = 7.0  * arcFontSize /100.0 * fontScaleFactor;
    nodeFontSize = 12.0 * nodeFontSize/100.0 * fontScaleFactor;

    nodeWidth  = nodeSizeRel /100.0 * totalZoom / pixelWidth  * bendSpacing/5.0;
    nodeHeight = nodeSizeRel /100.0 * totalZoom / pixelHeight * bendSpacing/5.0;
    arrowSize  = arrowSizeRel/100.0 * totalZoom / (pixelWidth+pixelHeight) * 2.0;

    if (bendSpacing/2.0<nodeSpacing/10.0)
    {
        arrowSize *= bendSpacing/2.0;
    }
    else
    {
        arrowSize *= nodeSpacing/10.0;
    }

    arcLabelSep = fineSpacing*totalZoom;

    if (arcLabelSep<=2*arrowSize) arcLabelSep = 2*arrowSize;
}


void graphDisplayProxy::Synchronize() throw()
{
    ExtractLayoutParameters();
    DetermineCoordinateTransformation();
    piG = G.GetPotentials();

    if (nodeColourMode==NODES_FLOATING_COLOURS)
    {
        maxNodeColour = 1;

        for (TNode v=0;v<G.N();++v)
        {
            TNode c = G.NodeColour(v);
            if (c>maxNodeColour && c!=NoNode) maxNodeColour = c;
        }
    }

    if (arcColourMode==ARCS_FLOATING_COLOURS)
    {
        maxEdgeColour = 1;

        for (TArc a=0;a<G.M();++a)
        {
            TArc c = G.EdgeColour(2*a);
            if (c>maxEdgeColour && c!=NoArc) maxEdgeColour = c;
        }
    }
}


long graphDisplayProxy::CanvasCXOfArcLabelAnchor(TArc a) throw(ERRange)
{
    TNode p = G.ArcLabelAnchor(a);

    if (p!=NoNode) return CanvasCXOfPoint(p);

    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);

    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5) return CanvasCXOfPoint(u);

    dy /= norm;

    return long((CanvasCXOfPoint(u)+CanvasCXOfPoint(v))/2+arcLabelSep*dy);
}


long graphDisplayProxy::CanvasCYOfArcLabelAnchor(TArc a) throw(ERRange)
{
    TNode p = G.ArcLabelAnchor(a);

    if (p!=NoNode) return CanvasCYOfPoint(p);

    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);

    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5) return CanvasCYOfPoint(u);

    dx /= norm;

    return long((CanvasCYOfPoint(u)+CanvasCYOfPoint(v))/2-arcLabelSep*dx);
}


long graphDisplayProxy::CanvasCXOfPort(TNode u,TNode v) throw(ERRange)
{
    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5)
    {
        return CanvasCXOfPoint(v);
    }

    if (nodeShapeMode==NODE_SHAPE_POINT)
    {
        return long(CanvasCXOfPoint(v)-dx*7/norm);
    }
    else
    {
        return long(CanvasCXOfPoint(v)-dx*nodeWidth/norm);
    }
}


long graphDisplayProxy::CanvasCXOfPort(TArc a) throw(ERRange)
{
    TNode p = G.PortNode(a);

    if (p==NoNode) return CanvasCXOfPort(G.EndNode(a),G.StartNode(a));

    return CanvasCXOfPort(G.PortNode(a),G.StartNode(a));
}


long graphDisplayProxy::CanvasCYOfPort(TNode u,TNode v) throw(ERRange)
{
    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5)
    {
        return CanvasCYOfPoint(v);
    }

    if (nodeShapeMode==NODE_SHAPE_POINT)
    {
        return long(CanvasCYOfPoint(v)-dy*5/norm);
    }
    else
    {
        return long(CanvasCYOfPoint(v)-dy*nodeHeight/norm);
    }
}


long graphDisplayProxy::CanvasCYOfPort(TArc a) throw(ERRange)
{
    TNode p = G.PortNode(a);

    if (p==NoNode) return CanvasCYOfPort(G.EndNode(a),G.StartNode(a));

    return CanvasCYOfPort(G.PortNode(a),G.StartNode(a));
}


long graphDisplayProxy::CanvasNodeWidth(TNode v) throw(ERRange)
{
    if (v==NoNode) return long(nodeWidth+0.5);

    TNode p = G.ThreadSuccessor(v);

    if (p==NoNode) return long(nodeWidth+0.5);

    return long(nodeWidth + G.C(p,ix)*objectZoom*globalZoom/pixelWidth + 0.5);
}


long graphDisplayProxy::CanvasNodeHeight(TNode v) throw(ERRange)
{
    if (v==NoNode) return long(nodeHeight+0.5);

    TNode p = G.ThreadSuccessor(v);

    if (p==NoNode) return long(nodeHeight+0.5);

    return long(nodeHeight + G.C(p,iy)*objectZoom*globalZoom/pixelHeight + 0.5);
}


char* graphDisplayProxy::NodeLegenda(char* buffer,size_t length,char* nodeLabel) throw()
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(nodeLabelFormat) && offset<length;)
    {
        if (arcLabelFormat[i]=='#' && i<strlen(nodeLabelFormat)-1)
        {
            int mode = int(nodeLabelFormat[i+1]-'0');

            if (mode==1)
            {
                sprintf(buffer+offset,"%s",nodeLabel);
            }
            else if (mode>1 && mode<6)
            {
                const char* legenda[] =
                {
                    "d",
                    "pi",
                    "colour",
                    "B"
                };

                sprintf(buffer+offset,"%s(%s)",legenda[mode-2],nodeLabel);
            }

            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = nodeLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::CompoundNodeLabel(char* buffer,size_t length,TNode v) throw(ERRange)
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(nodeLabelFormat) && offset<length;)
    {
        if (nodeLabelFormat[i]=='#' && i<strlen(nodeLabelFormat)-1)
        {
            int mode = int(nodeLabelFormat[i+1]-'0');
            BasicNodeLabel(buffer+offset,length-offset,v,mode);
            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = nodeLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::BasicNodeLabel(char* buffer,size_t length,TNode v,int mode) throw(ERRange)
{
    switch (mode)
    {
        case 1:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(v));
            break;
        }
        case 2:
        {
            if (G.Dist(v)==InfFloat || G.Dist(v)==-InfFloat)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.Dist(v)));
            }
            break;
        }
        case 3:
        {
            if (G.Pi(v)==InfFloat || G.Pi(v)==-InfFloat)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.Pi(v)));
            }
            break;
        }
        case 4:
        {
            if (G.NodeColour(v)==NoNode)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%lu",static_cast<unsigned long>(G.NodeColour(v)));
            }
            break;
        }
        case 5:
        {
            sprintf(buffer,"%g",static_cast<double>(G.Demand(v)));
            break;
        }
        case 6:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(v)+1);
            break;
        }
        case 0:
        default:
        {
            buffer[0] = 0;
            break;
        }
    }

    return buffer;
}


long graphDisplayProxy::NodeLabelFontSize() throw()
{
    return long(nodeFontSize + 0.5);
}


bool graphDisplayProxy::IsNodeMapped(TNode v) throw(ERRange)
{
    if (G.HiddenNode(v)) return false;

    return true;
}


void graphDisplayProxy::CanvasNodeColour(char* buffer,TNode v) throw(ERRange)
{
    TIndex colourIndexInTable = CanvasNodeColour(v);
    unsigned long rgbColour = 0;

    if (nodeColourMode!=NODES_FLOATING_COLOURS)
    {
        rgbColour = RGBFixedColour(colourIndexInTable);
    }
    else
    {
        rgbColour = RGBSmoothColour(colourIndexInTable,ZERO_COLOUR+maxNodeColour);
    }

    sprintf(buffer,"#%06lX",rgbColour);
}


TIndex graphDisplayProxy::CanvasNodeColour(TNode v) throw(ERRange)
{
    if (nodeColourMode==NODES_UNCOLOURED)
    {
        return NO_COLOUR;
    }
    else if (nodeColourMode==NODES_COLOUR_BY_DISTANCE)
    {
        if (G.Dist(v)==InfFloat || G.Dist(v)==-InfFloat) return ZERO_COLOUR+1;

        return ZERO_COLOUR;
    }
    else if (nodeColourMode==NODES_COLOUR_BY_DEMAND)
    {
        if (G.Demand(v)>0) return ZERO_COLOUR+1;
        if (G.Demand(v)<0) return ZERO_COLOUR+2;

        return NO_COLOUR;
    }

    TIndex thisColour = G.NodeColour(v);

    if (thisColour==NoNode) return NO_COLOUR;

    if (nodeColourMode==NODES_FIXED_COLOURS && thisColour>=MAX_COLOUR-ZERO_COLOUR)
    {
        return VAGUE_COLOUR;
    }

    return ZERO_COLOUR + thisColour;
}


char* graphDisplayProxy::ArcLegenda(char* buffer,size_t length,char* arcLabel) throw()
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(arcLabelFormat) && offset<length;)
    {
        if (arcLabelFormat[i]=='#' && i<strlen(arcLabelFormat)-1)
        {
            int mode = int(arcLabelFormat[i+1]-'0');

            if (mode==1)
            {
                sprintf(buffer+offset,"%s",arcLabel);
            }
            else if (mode>1 && mode<8)
            {
                const char* legenda[] =
                {
                    "ucap",
                    "x",
                    "length",
                    "redlength",
                    "lcap",
                    "colour"
                };

                sprintf(buffer+offset,"%s(%s)",legenda[mode-2],arcLabel);
            }

            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = arcLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::CompoundArcLabel(char* buffer,size_t length,TArc a) throw(ERRange)
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(arcLabelFormat) && offset<length;)
    {
        if (arcLabelFormat[i]=='#' && i<strlen(arcLabelFormat)-1)
        {
            int mode = int(arcLabelFormat[i+1]-'0');
            BasicArcLabel(buffer+offset,length-offset,a,mode);
            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = arcLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::BasicArcLabel(char* buffer,size_t length,TArc a,int mode) throw(ERRange)
{
    switch (mode)
    {
        case 1:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(a/2));
            break;
        }
        case 2:
        {
            if (G.UCap(a)==InfCap)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.UCap(a)));
            }
            break;
        }
        case 3:
        {
            sprintf(buffer,"%g",fabs(G.Sub(a)));
            break;
        }
        case 4:
        {
            if (G.Length(a)==InfFloat || G.Length(a)==-InfFloat)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.Length(a)));
            }
            break;
        }
        case 5:
        {
            sprintf(buffer,"%g",static_cast<double>(G.RedLength(piG,a)));
            break;
        }
        case 6:
        {
            sprintf(buffer,"%g",static_cast<double>(G.LCap(a)));
            break;
        }
        case 7:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(a/2+1));
            break;
        }
        case 8:
        {
            if (G.EdgeColour(a)==NoArc)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%lu",static_cast<unsigned long>(G.EdgeColour(a)));
            }
            break;
        }
        case 0:
        default:
        {
            buffer[0] = 0;
            break;
        }
    }

    return buffer;
}


long graphDisplayProxy::ArcLabelFontSize() throw()
{
    return long(arcFontSize + 0.5);
}


TArrowDir graphDisplayProxy::ArrowDirections(TArc a) throw()
{
    TArrowDir thisDirections = ARROW_NONE;

    switch (arrowDisplayMode)
    {
        case ARROWS_OFF:
        {
            break;
        }
        case ARROWS_IMPL_ORIENTATION:
        {
            thisDirections = ARROW_FORWARD;
            break;
        }
        case ARROWS_FLOW_DIRECTION:
        {
            if (G.Sub(a)>CT.epsilon)
            {
                thisDirections = ARROW_FORWARD;
            }
            else if (G.Sub(a)<-CT.epsilon)
            {
                thisDirections = ARROW_BACKWARD;
            }

            break;
        }
        case ARROWS_LIKE_PREDECESSORS:
        {
            if (G.Pred(G.EndNode(a))==a)
            {
                thisDirections = ARROW_FORWARD;
            }
            else if (G.Pred(G.EndNode(a^1))==(a^1))
            {
                thisDirections = ARROW_BACKWARD;
            }

            break;
        }
        default:
        case ARROWS_ARC_ORIENTATION:
        {
            thisDirections = TArrowDir(G.Orientation(a));
        }
    }

    if (a&1)
    {
        if (thisDirections==ARROW_FORWARD) return ARROW_BACKWARD;
        if (thisDirections==ARROW_BACKWARD) return ARROW_FORWARD;
    }

    return thisDirections;
}


unsigned long graphDisplayProxy::RGBFixedColour(TIndex c)
    throw()
{
    if (c==EXPOSED_COLOUR) return 0xDDCCFF;
    if (c==NO_COLOUR)      return 0xFFFFFF;
    if (c==OUTLINE_COLOUR) return 0x000000;
    if (c==GRID_COLOUR)    return 0xA0A0A0;
    if (c==VAGUE_COLOUR)   return 0xA0A0A0;

    switch (c-ZERO_COLOUR)
    {
        case 0:  return 0x00FF00;
        case 1:  return 0xFF0000;
        case 2:  return 0x6060FF;
        case 3:  return 0xFFFF40;
        case 4:  return 0x00E0E0;
        case 5:  return 0xFF00E0;
        case 6:  return 0xE08000;
        case 7:  return 0xA0A0FF;
        case 8:  return 0xFF8080;
        case 9:  return 0xA0A000;
        case 10: return 0xff2d93;
        case 11: return 0x10A010;
        case 12: return 0x678bb2;
        case 13: return 0xb20787;
        case 14: return 0xb24513;
        case 15: return 0xb26586;
        case 16: return 0x315a6b;
        case 17: return 0xc4db30;
        case 18: return 0xff8800;
        case 19: return 0xab0ddb;
        case 20: return 0x88DD66;
    }

    // Reaching this takes c >= MAX_COLOUR

    return RGBFixedColour(VAGUE_COLOUR);
}


unsigned long graphDisplayProxy::RGBSmoothColour(TIndex c,TIndex maxColourIndex)
    throw()
{
    if (c<ZERO_COLOUR)    return RGBFixedColour(c);
    if (c>maxColourIndex) return RGBFixedColour(VAGUE_COLOUR);

    double ratio = double(c)/double(maxColourIndex+1);

    long unsigned rSat = 100;
    long unsigned bSat = 100;
    long unsigned gSat = 100;

    if (ratio<0.333)
    {
        rSat = (long unsigned)(ceil(0xFF*3*(0.333-ratio)));
        gSat = (long unsigned)(ceil(0xFF*3*ratio));
    }
    else if (ratio<0.667)
    {
        gSat = (long unsigned)(ceil(0xFF*3*(0.667-ratio)));
        bSat = (long unsigned)(ceil(0xFF*3*(ratio-0.333)));
    }
    else
    {
        bSat = (long unsigned)(ceil(0xFF*3*(1-ratio)));
        rSat = (long unsigned)(ceil(0xFF*3*(ratio-0.667)));
    }

    return 0x100*(0x100*rSat+gSat)+bSat;
}


bool graphDisplayProxy::IsArcMapped(TArc a) throw(ERRange)
{
    if (arcVisibilityMode==ARC_DISPLAY_HIDE_ALL) return false;

    if (G.HiddenArc(a)) return false;

    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);

    if (!IsNodeMapped(u) || !IsNodeMapped(v)) return false;

    if (G.Blocking(a) && G.Blocking(a^1)) return false;

    if (G.UCap(a)<CT.epsilon) return false;

    if (u==v && G.ArcLabelAnchor(a)==NoNode) return false;

    if (arcVisibilityMode==ARC_DISPLAY_SHOW_ALL) return true;

    bool isPredecessorArc = (G.Pred(v)==a || G.Pred(u)==(a^1));

    TFloat thisSub = fabs(G.Sub(a));
    bool isSubgraphArc = (thisSub>CT.epsilon);

    if (arcVisibilityMode==ARC_DISPLAY_SUBGRAPH)
    {
         return isSubgraphArc;
    }
    else if (arcVisibilityMode==ARC_DISPLAY_PREDECESSORS)
    {
        return isPredecessorArc;
    }
    else if (arcVisibilityMode==ARC_DISPLAY_PRED_SUB)
    {
        return isPredecessorArc | isSubgraphArc;
    }

    // Default rule is ARC_DISPLAY_SHOW_ALL
    return true;
}


void graphDisplayProxy::CanvasArcColour(char* buffer,TArc a) throw(ERRange)
{
    TIndex colourIndexInTable = CanvasArcColour(a);
    unsigned long rgbColour = 0;

    if (arcColourMode!=ARCS_FLOATING_COLOURS)
    {
        rgbColour = RGBFixedColour(colourIndexInTable);
    }
    else
    {
        rgbColour = RGBSmoothColour(colourIndexInTable,ZERO_COLOUR+maxEdgeColour);
    }

    sprintf(buffer,"#%06lX",rgbColour);
}


TIndex graphDisplayProxy::CanvasArcColour(TArc a) throw(ERRange)
{
    if (arcColourMode==ARCS_COLOUR_PRED)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);

        if (G.Pred(v)==a || G.Pred(u)==(a^1))
        {
            return EXPOSED_COLOUR;
        }
    }
    else if (arcColourMode==ARCS_UNCOLOURED)
    {
        return OUTLINE_COLOUR;
    }

    TIndex thisColour = G.EdgeColour(a);

    if (thisColour==NoArc) return VAGUE_COLOUR;

    if (arcColourMode==ARCS_REPEAT_COLOURS)
    {
        return ZERO_COLOUR + G.EdgeColour(a)%(MAX_COLOUR-ZERO_COLOUR);
    }
    else if (arcColourMode==ARCS_FIXED_COLOURS && thisColour>=MAX_COLOUR-ZERO_COLOUR)
    {
        return VAGUE_COLOUR;
    }

    return ZERO_COLOUR + thisColour;
}


long graphDisplayProxy::CanvasArcWidth(TArc a) throw(ERRange)
{
    TFloat thisSub = fabs(G.Sub(a));
    bool isSubgraphArc = (thisSub>CT.epsilon);
    long arcWidth = arcWidthMin;

    if (arcWidthMode==ARC_WIDTH_PREDECESSORS)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);

        if (G.Pred(v)==a || G.Pred(u)==(a^1))
        {
            arcWidth = arcWidthMax;
        }
    }
    else if (arcWidthMode==ARC_WIDTH_SUBGRAPH)
    {
        if (isSubgraphArc)
        {
            arcWidth = arcWidthMax;
        }
    }
    else if (arcWidthMode==ARC_WIDTH_FLOW_LINEAR)
    {
        arcWidth = arcWidthMin + int(thisSub*(arcWidthMax-arcWidthMin));
    }
    else if (arcWidthMode==ARC_WIDTH_FLOW_LOGARITHMIC)
    {
        arcWidth = arcWidthMin + int(log(thisSub+1.0)*(arcWidthMax-arcWidthMin));
    }
    else if (arcWidthMode==ARC_WIDTH_EMPTY_FREE_FULL)
    {
        if (thisSub<G.LCap(a)+CT.epsilon)
        {
            arcWidth = arcWidthMin;
        }
        else if (thisSub<G.UCap(a)-CT.epsilon)
        {
            arcWidth = (arcWidthMin+arcWidthMax)/2;
        }
        else
        {
            arcWidth = arcWidthMax;
        }
    }

    double arcWidthScaled = arcWidth*globalZoom;

    if (arcWidthScaled<1.0) return 1;

    return long(arcWidthScaled);
}


TDashMode graphDisplayProxy::CanvasArcDashMode(TArc a) throw(ERRange)
{
    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);
    TFloat thisSub = fabs(G.Sub(a));

    if (arcStippleMode==ARC_STIPPLE_PREDECESSORS)
    {
        if (G.Pred(v)==a || G.Pred(u)==(a^1))
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_FREE)
    {
        if (   thisSub>G.LCap(a)+CT.epsilon
            && thisSub<G.UCap(a)-CT.epsilon
            )
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_FRACTIONAL)
    {
        if (floor(thisSub)!=thisSub)
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_COLOURS)
    {
        if (G.EdgeColour(a)!=NoArc)
        {
            return TDashMode(int(G.EdgeColour(a))%4);
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_VOID)
    {
        if (thisSub<CT.epsilon)
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_SUBGRAPH)
    {
        if (thisSub>CT.epsilon)
        {
            return LINE_STYLE_DOT;
        }
    }

    return LINE_STYLE_SOLID;
}


void graphDisplayProxy::PlaceLayoutPoint(TNode p,long cx,long cy) throw(ERRange)
{
    TFloat ocx = (cx*pixelWidth/globalZoom  - offset[ix])/objectZoom;
    TFloat ocy = (cy*pixelHeight/globalZoom - offset[iy])/objectZoom;

    if (p<G.N())
    {
        ocx = round(ocx/nodeSpacing)*nodeSpacing;
        ocy = round(ocy/nodeSpacing)*nodeSpacing;
    }
    else if (bendSpacing>CT.epsilon)
    {
        ocx = round(ocx/bendSpacing)*bendSpacing;
        ocy = round(ocy/bendSpacing)*bendSpacing;
    }

    G.Representation() -> SetC(p,ix,ocx);
    G.Representation() -> SetC(p,iy,ocy);
}


void graphDisplayProxy::PlaceArcLabelAnchor(TArc a,long cx,long cy) throw(ERRange)
{
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    TNode p = GR->ProvideArcLabelAnchor(a);

    TFloat ocx = (cx*pixelWidth/globalZoom  - offset[ix])/objectZoom;
    TFloat ocy = (cy*pixelHeight/globalZoom - offset[iy])/objectZoom;

    if (fineSpacing>CT.epsilon)
    {
        ocx = round(ocx/fineSpacing)*fineSpacing;
        ocy = round(ocy/fineSpacing)*fineSpacing;
    }

    G.Representation() -> SetC(p,ix,ocx);
    G.Representation() -> SetC(p,iy,ocy);
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file graphDisplayProxy.cpp
/// \brief #graphDisplayProxy class implementation

#include "graphDisplayProxy.h"
#include "sparseRepresentation.h"


graphDisplayProxy::graphDisplayProxy(abstractMixedGraph &_G,double _pixelWidth,double _pixelHeight)
    throw(ERRejected) :
    managedObject(_G.Context()),G(_G), CFG(_G.Context(),true),
    piG(G.GetPotentials()),pixelWidth(_pixelWidth),pixelHeight(_pixelHeight)
{
    Synchronize();

    LogEntry(LOG_MEM,"...Graph display proxy instanciated");
}


graphDisplayProxy::~graphDisplayProxy() throw()
{
    LogEntry(LOG_MEM,"...Graph display proxy disallocated");
}


unsigned long graphDisplayProxy::Allocated() const throw()
{
    return 0;
}


unsigned long graphDisplayProxy::Size() const throw()
{
    return 255;
}


void graphDisplayProxy::ExtractLayoutParameters() throw()
{
    // Copy parameters from the graph layout pool or set default values
    G.GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);
    G.GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);
    G.GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    G.GetLayoutParameter(TokLayoutNodeSize,nodeSizeRel);
    G.GetLayoutParameter(TokLayoutNodeFontSize,nodeFontSize);
    G.GetLayoutParameter(TokLayoutArcFontSize,arcFontSize);
    G.GetLayoutParameter(TokLayoutArrowSize,arrowSizeRel);

    // For the time being, the page size is DIN A4, hardcoded
    canvasWidth = 297.0;
    canvasHeight = 210.0;

    // For the time being, the page margins are hard-coded
    leftMargin = topMargin = rightMargin = bottomMargin = 0.05;

    globalZoom = CFG.displayZoom;

    TLayoutModel layoutModel = G.LayoutModel();

    char* pNodeLabelFormat = NULL;
    G.GetLayoutParameter(TokLayoutNodeLabelFormat,pNodeLabelFormat);
    strncpy(nodeLabelFormat,pNodeLabelFormat,LABEL_BUFFER_SIZE);
    nodeLabelFormat[LABEL_BUFFER_SIZE-1] = 0;

    G.GetLayoutParameter(TokLayoutNodeShapeMode,nodeShapeMode,layoutModel);
    G.GetLayoutParameter(TokLayoutNodeColourMode,nodeColourMode);

    char* pArcLabelFormat = NULL;
    G.GetLayoutParameter(TokLayoutArcLabelFormat,pArcLabelFormat);
    strncpy(arcLabelFormat,pArcLabelFormat,LABEL_BUFFER_SIZE);
    arcLabelFormat[LABEL_BUFFER_SIZE-1] = 0;

    G.GetLayoutParameter(TokLayoutArcVisibilityMode,arcVisibilityMode);
    G.GetLayoutParameter(TokLayoutArrowDisplayMode,arrowDisplayMode);
    G.GetLayoutParameter(TokLayoutArcStippleMode,arcStippleMode);
    G.GetLayoutParameter(TokLayoutArcColourMode,arcColourMode);
    G.GetLayoutParameter(TokLayoutArcWidthMode,arcWidthMode);
    G.GetLayoutParameter(TokLayoutArcWidthMin,arcWidthMin);
    G.GetLayoutParameter(TokLayoutArcWidthMax,arcWidthMax);

    // To enforce a consistent set of layout parameters
    arcWidthMin = (arcWidthMin<1) ? 1 : arcWidthMin;
    arcWidthMax = (arcWidthMax>arcWidthMin) ? arcWidthMax : arcWidthMin;

    if (G.IsDense() && arcVisibilityMode==ARC_DISPLAY_SHOW_ALL)
    {
        arcVisibilityMode = ARC_DISPLAY_PRED_SUB;
    }
}


void graphDisplayProxy::DetermineCoordinateTransformation() throw()
{
    for (TDim i=0;i<3&& i<G.Dim();++i)
    {
        G.Layout_GetBoundingInterval(i,min[i],max[i]);
        originalRange[i] = (max[i]-min[i] > 0.5*nodeSpacing) ? max[i]-min[i] : 0.0;
    }

    ix = 0;
    iy = 1;

    double zoomX = (originalRange[ix]>0.0) ?
        canvasWidth *(1.0-rightMargin-leftMargin)/originalRange[ix] : 10000.0;
    double zoomY = (originalRange[iy]>0.0) ?
        canvasHeight*(1.0-bottomMargin-topMargin)/originalRange[iy] : 10000.0;

    objectZoom = (zoomX<zoomY) ? zoomX : zoomY;
    double totalZoom = objectZoom*globalZoom;

    offset[ix] = ((leftMargin+1.0-rightMargin )*0.5*canvasWidth  - (min[ix]+max[ix])*0.5*objectZoom);
    offset[iy] = ((topMargin +1.0-bottomMargin)*0.5*canvasHeight - (min[iy]+max[iy])*0.5*objectZoom);

    double fontScaleFactor = totalZoom * 0.1 * bendSpacing * nodeSizeRel / 100.0;
    arcFontSize  = 7.0  * arcFontSize /100.0 * fontScaleFactor;
    nodeFontSize = 12.0 * nodeFontSize/100.0 * fontScaleFactor;

    nodeWidth  = nodeSizeRel /100.0 * totalZoom / pixelWidth  * bendSpacing/5.0;
    nodeHeight = nodeSizeRel /100.0 * totalZoom / pixelHeight * bendSpacing/5.0;
    arrowSize  = arrowSizeRel/100.0 * totalZoom / (pixelWidth+pixelHeight) * 2.0;

    if (bendSpacing/2.0<nodeSpacing/10.0)
    {
        arrowSize *= bendSpacing/2.0;
    }
    else
    {
        arrowSize *= nodeSpacing/10.0;
    }

    arcLabelSep = fineSpacing*totalZoom;

    if (arcLabelSep<=2*arrowSize) arcLabelSep = 2*arrowSize;
}


void graphDisplayProxy::Synchronize() throw()
{
    ExtractLayoutParameters();
    DetermineCoordinateTransformation();
    piG = G.GetPotentials();

    if (nodeColourMode==NODES_FLOATING_COLOURS)
    {
        maxNodeColour = 1;

        for (TNode v=0;v<G.N();++v)
        {
            TNode c = G.NodeColour(v);
            if (c>maxNodeColour && c!=NoNode) maxNodeColour = c;
        }
    }

    if (arcColourMode==ARCS_FLOATING_COLOURS)
    {
        maxEdgeColour = 1;

        for (TArc a=0;a<G.M();++a)
        {
            TArc c = G.EdgeColour(2*a);
            if (c>maxEdgeColour && c!=NoArc) maxEdgeColour = c;
        }
    }
}


long graphDisplayProxy::CanvasCXOfArcLabelAnchor(TArc a) throw(ERRange)
{
    TNode p = G.ArcLabelAnchor(a);

    if (p!=NoNode) return CanvasCXOfPoint(p);

    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);

    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5) return CanvasCXOfPoint(u);

    dy /= norm;

    return long((CanvasCXOfPoint(u)+CanvasCXOfPoint(v))/2+arcLabelSep*dy);
}


long graphDisplayProxy::CanvasCYOfArcLabelAnchor(TArc a) throw(ERRange)
{
    TNode p = G.ArcLabelAnchor(a);

    if (p!=NoNode) return CanvasCYOfPoint(p);

    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);

    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5) return CanvasCYOfPoint(u);

    dx /= norm;

    return long((CanvasCYOfPoint(u)+CanvasCYOfPoint(v))/2-arcLabelSep*dx);
}


long graphDisplayProxy::CanvasCXOfPort(TNode u,TNode v) throw(ERRange)
{
    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5)
    {
        return CanvasCXOfPoint(v);
    }

    if (nodeShapeMode==NODE_SHAPE_POINT)
    {
        return long(CanvasCXOfPoint(v)-dx*7/norm);
    }
    else
    {
        return long(CanvasCXOfPoint(v)-dx*nodeWidth/norm);
    }
}


long graphDisplayProxy::CanvasCXOfPort(TArc a) throw(ERRange)
{
    TNode p = G.PortNode(a);

    if (p==NoNode) return CanvasCXOfPort(G.EndNode(a),G.StartNode(a));

    return CanvasCXOfPort(G.PortNode(a),G.StartNode(a));
}


long graphDisplayProxy::CanvasCYOfPort(TNode u,TNode v) throw(ERRange)
{
    double dx = CanvasCXOfPoint(v)-CanvasCXOfPoint(u);
    double dy = CanvasCYOfPoint(v)-CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    if (fabs(norm)<0.5)
    {
        return CanvasCYOfPoint(v);
    }

    if (nodeShapeMode==NODE_SHAPE_POINT)
    {
        return long(CanvasCYOfPoint(v)-dy*5/norm);
    }
    else
    {
        return long(CanvasCYOfPoint(v)-dy*nodeHeight/norm);
    }
}


long graphDisplayProxy::CanvasCYOfPort(TArc a) throw(ERRange)
{
    TNode p = G.PortNode(a);

    if (p==NoNode) return CanvasCYOfPort(G.EndNode(a),G.StartNode(a));

    return CanvasCYOfPort(G.PortNode(a),G.StartNode(a));
}


long graphDisplayProxy::CanvasNodeWidth(TNode v) throw(ERRange)
{
    if (v==NoNode) return long(nodeWidth+0.5);

    TNode p = G.ThreadSuccessor(v);

    if (p==NoNode) return long(nodeWidth+0.5);

    return long(nodeWidth + G.C(p,ix)*objectZoom*globalZoom/pixelWidth + 0.5);
}


long graphDisplayProxy::CanvasNodeHeight(TNode v) throw(ERRange)
{
    if (v==NoNode) return long(nodeHeight+0.5);

    TNode p = G.ThreadSuccessor(v);

    if (p==NoNode) return long(nodeHeight+0.5);

    return long(nodeHeight + G.C(p,iy)*objectZoom*globalZoom/pixelHeight + 0.5);
}


char* graphDisplayProxy::NodeLegenda(char* buffer,size_t length,char* nodeLabel) throw()
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(nodeLabelFormat) && offset<length;)
    {
        if (arcLabelFormat[i]=='#' && i<strlen(nodeLabelFormat)-1)
        {
            int mode = int(nodeLabelFormat[i+1]-'0');

            if (mode==1)
            {
                sprintf(buffer+offset,"%s",nodeLabel);
            }
            else if (mode>1 && mode<6)
            {
                const char* legenda[] =
                {
                    "d",
                    "pi",
                    "colour",
                    "B"
                };

                sprintf(buffer+offset,"%s(%s)",legenda[mode-2],nodeLabel);
            }

            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = nodeLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::CompoundNodeLabel(char* buffer,size_t length,TNode v) throw(ERRange)
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(nodeLabelFormat) && offset<length;)
    {
        if (nodeLabelFormat[i]=='#' && i<strlen(nodeLabelFormat)-1)
        {
            int mode = int(nodeLabelFormat[i+1]-'0');
            BasicNodeLabel(buffer+offset,length-offset,v,mode);
            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = nodeLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::BasicNodeLabel(char* buffer,size_t length,TNode v,int mode) throw(ERRange)
{
    switch (mode)
    {
        case 1:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(v));
            break;
        }
        case 2:
        {
            if (G.Dist(v)==InfFloat || G.Dist(v)==-InfFloat)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.Dist(v)));
            }
            break;
        }
        case 3:
        {
            if (G.Pi(v)==InfFloat || G.Pi(v)==-InfFloat)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.Pi(v)));
            }
            break;
        }
        case 4:
        {
            if (G.NodeColour(v)==NoNode)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%lu",static_cast<unsigned long>(G.NodeColour(v)));
            }
            break;
        }
        case 5:
        {
            sprintf(buffer,"%g",static_cast<double>(G.Demand(v)));
            break;
        }
        case 6:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(v)+1);
            break;
        }
        case 0:
        default:
        {
            buffer[0] = 0;
            break;
        }
    }

    return buffer;
}


long graphDisplayProxy::NodeLabelFontSize() throw()
{
    return long(nodeFontSize + 0.5);
}


bool graphDisplayProxy::IsNodeMapped(TNode v) throw(ERRange)
{
    if (G.HiddenNode(v)) return false;

    return true;
}


void graphDisplayProxy::CanvasNodeColour(char* buffer,TNode v) throw(ERRange)
{
    TIndex colourIndexInTable = CanvasNodeColour(v);
    unsigned long rgbColour = 0;

    if (nodeColourMode!=NODES_FLOATING_COLOURS)
    {
        rgbColour = RGBFixedColour(colourIndexInTable);
    }
    else
    {
        rgbColour = RGBSmoothColour(colourIndexInTable,ZERO_COLOUR+maxNodeColour);
    }

    sprintf(buffer,"#%06lX",rgbColour);
}


TIndex graphDisplayProxy::CanvasNodeColour(TNode v) throw(ERRange)
{
    if (nodeColourMode==NODES_UNCOLOURED)
    {
        return NO_COLOUR;
    }
    else if (nodeColourMode==NODES_COLOUR_BY_DISTANCE)
    {
        if (G.Dist(v)==InfFloat || G.Dist(v)==-InfFloat) return ZERO_COLOUR+1;

        return ZERO_COLOUR;
    }
    else if (nodeColourMode==NODES_COLOUR_BY_DEMAND)
    {
        if (G.Demand(v)>0) return ZERO_COLOUR+1;
        if (G.Demand(v)<0) return ZERO_COLOUR+2;

        return NO_COLOUR;
    }

    TIndex thisColour = G.NodeColour(v);

    if (thisColour==NoNode) return NO_COLOUR;

    if (nodeColourMode==NODES_FIXED_COLOURS && thisColour>=MAX_COLOUR-ZERO_COLOUR)
    {
        return VAGUE_COLOUR;
    }

    return ZERO_COLOUR + thisColour;
}


char* graphDisplayProxy::ArcLegenda(char* buffer,size_t length,char* arcLabel) throw()
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(arcLabelFormat) && offset<length;)
    {
        if (arcLabelFormat[i]=='#' && i<strlen(arcLabelFormat)-1)
        {
            int mode = int(arcLabelFormat[i+1]-'0');

            if (mode==1)
            {
                sprintf(buffer+offset,"%s",arcLabel);
            }
            else if (mode>1 && mode<8)
            {
                const char* legenda[] =
                {
                    "ucap",
                    "x",
                    "length",
                    "redlength",
                    "lcap",
                    "colour"
                };

                sprintf(buffer+offset,"%s(%s)",legenda[mode-2],arcLabel);
            }

            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = arcLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::CompoundArcLabel(char* buffer,size_t length,TArc a) throw(ERRange)
{
    size_t offset = 0;

    for (size_t i=0;i<=strlen(arcLabelFormat) && offset<length;)
    {
        if (arcLabelFormat[i]=='#' && i<strlen(arcLabelFormat)-1)
        {
            int mode = int(arcLabelFormat[i+1]-'0');
            BasicArcLabel(buffer+offset,length-offset,a,mode);
            i += 2;
            offset += strlen(buffer+offset);
        }
        else
        {
            buffer[offset++] = arcLabelFormat[i++];
        }
    }

    buffer[(offset<length) ? offset : length-1] = 0;

    return buffer;
}


char* graphDisplayProxy::BasicArcLabel(char* buffer,size_t length,TArc a,int mode) throw(ERRange)
{
    switch (mode)
    {
        case 1:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(a/2));
            break;
        }
        case 2:
        {
            if (G.UCap(a)==InfCap)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.UCap(a)));
            }
            break;
        }
        case 3:
        {
            sprintf(buffer,"%g",fabs(G.Sub(a)));
            break;
        }
        case 4:
        {
            if (G.Length(a)==InfFloat || G.Length(a)==-InfFloat)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%g",static_cast<double>(G.Length(a)));
            }
            break;
        }
        case 5:
        {
            sprintf(buffer,"%g",static_cast<double>(G.RedLength(piG,a)));
            break;
        }
        case 6:
        {
            sprintf(buffer,"%g",static_cast<double>(G.LCap(a)));
            break;
        }
        case 7:
        {
            sprintf(buffer,"%lu",static_cast<unsigned long>(a/2+1));
            break;
        }
        case 8:
        {
            if (G.EdgeColour(a)==NoArc)
            {
                sprintf(buffer,"*");
            }
            else
            {
                sprintf(buffer,"%lu",static_cast<unsigned long>(G.EdgeColour(a)));
            }
            break;
        }
        case 0:
        default:
        {
            buffer[0] = 0;
            break;
        }
    }

    return buffer;
}


long graphDisplayProxy::ArcLabelFontSize() throw()
{
    return long(arcFontSize + 0.5);
}


TArrowDir graphDisplayProxy::ArrowDirections(TArc a) throw()
{
    TArrowDir thisDirections = ARROW_NONE;

    switch (arrowDisplayMode)
    {
        case ARROWS_OFF:
        {
            break;
        }
        case ARROWS_IMPL_ORIENTATION:
        {
            thisDirections = ARROW_FORWARD;
            break;
        }
        case ARROWS_FLOW_DIRECTION:
        {
            if (G.Sub(a)>CT.epsilon)
            {
                thisDirections = ARROW_FORWARD;
            }
            else if (G.Sub(a)<-CT.epsilon)
            {
                thisDirections = ARROW_BACKWARD;
            }

            break;
        }
        case ARROWS_LIKE_PREDECESSORS:
        {
            if (G.Pred(G.EndNode(a))==a)
            {
                thisDirections = ARROW_FORWARD;
            }
            else if (G.Pred(G.EndNode(a^1))==(a^1))
            {
                thisDirections = ARROW_BACKWARD;
            }

            break;
        }
        default:
        case ARROWS_ARC_ORIENTATION:
        {
            thisDirections = TArrowDir(G.Orientation(a));
        }
    }

    if (a&1)
    {
        if (thisDirections==ARROW_FORWARD) return ARROW_BACKWARD;
        if (thisDirections==ARROW_BACKWARD) return ARROW_FORWARD;
    }

    return thisDirections;
}


unsigned long graphDisplayProxy::RGBFixedColour(TIndex c)
    throw()
{
    if (c==EXPOSED_COLOUR) return 0xDDCCFF;
    if (c==NO_COLOUR)      return 0xFFFFFF;
    if (c==OUTLINE_COLOUR) return 0x000000;
    if (c==GRID_COLOUR)    return 0xA0A0A0;
    if (c==VAGUE_COLOUR)   return 0xA0A0A0;

    switch (c-ZERO_COLOUR)
    {
        case 0:  return 0x00FF00;
        case 1:  return 0xFF0000;
        case 2:  return 0x6060FF;
        case 3:  return 0xFFFF40;
        case 4:  return 0x00E0E0;
        case 5:  return 0xFF00E0;
        case 6:  return 0xE08000;
        case 7:  return 0xA0A0FF;
        case 8:  return 0xFF8080;
        case 9:  return 0xA0A000;
        case 10: return 0xff2d93;
        case 11: return 0x10A010;
        case 12: return 0x678bb2;
        case 13: return 0xb20787;
        case 14: return 0xb24513;
        case 15: return 0xb26586;
        case 16: return 0x315a6b;
        case 17: return 0xc4db30;
        case 18: return 0xff8800;
        case 19: return 0xab0ddb;
        case 20: return 0x88DD66;
    }

    // Reaching this takes c >= MAX_COLOUR

    return RGBFixedColour(VAGUE_COLOUR);
}


unsigned long graphDisplayProxy::RGBSmoothColour(TIndex c,TIndex maxColourIndex)
    throw()
{
    if (c<ZERO_COLOUR)    return RGBFixedColour(c);
    if (c>maxColourIndex) return RGBFixedColour(VAGUE_COLOUR);

    double ratio = double(c)/double(maxColourIndex+1);

    long unsigned rSat = 100;
    long unsigned bSat = 100;
    long unsigned gSat = 100;

    if (ratio<0.333)
    {
        rSat = (long unsigned)(ceil(0xFF*3*(0.333-ratio)));
        gSat = (long unsigned)(ceil(0xFF*3*ratio));
    }
    else if (ratio<0.667)
    {
        gSat = (long unsigned)(ceil(0xFF*3*(0.667-ratio)));
        bSat = (long unsigned)(ceil(0xFF*3*(ratio-0.333)));
    }
    else
    {
        bSat = (long unsigned)(ceil(0xFF*3*(1-ratio)));
        rSat = (long unsigned)(ceil(0xFF*3*(ratio-0.667)));
    }

    return 0x100*(0x100*rSat+gSat)+bSat;
}


bool graphDisplayProxy::IsArcMapped(TArc a) throw(ERRange)
{
    if (arcVisibilityMode==ARC_DISPLAY_HIDE_ALL) return false;

    if (G.HiddenArc(a)) return false;

    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);

    if (!IsNodeMapped(u) || !IsNodeMapped(v)) return false;

    if (G.Blocking(a) && G.Blocking(a^1)) return false;

    if (G.UCap(a)<CT.epsilon) return false;

    if (u==v && G.ArcLabelAnchor(a)==NoNode) return false;

    if (arcVisibilityMode==ARC_DISPLAY_SHOW_ALL) return true;

    bool isPredecessorArc = (G.Pred(v)==a || G.Pred(u)==(a^1));

    TFloat thisSub = fabs(G.Sub(a));
    bool isSubgraphArc = (thisSub>CT.epsilon);

    if (arcVisibilityMode==ARC_DISPLAY_SUBGRAPH)
    {
         return isSubgraphArc;
    }
    else if (arcVisibilityMode==ARC_DISPLAY_PREDECESSORS)
    {
        return isPredecessorArc;
    }
    else if (arcVisibilityMode==ARC_DISPLAY_PRED_SUB)
    {
        return isPredecessorArc | isSubgraphArc;
    }

    // Default rule is ARC_DISPLAY_SHOW_ALL
    return true;
}


void graphDisplayProxy::CanvasArcColour(char* buffer,TArc a) throw(ERRange)
{
    TIndex colourIndexInTable = CanvasArcColour(a);
    unsigned long rgbColour = 0;

    if (arcColourMode!=ARCS_FLOATING_COLOURS)
    {
        rgbColour = RGBFixedColour(colourIndexInTable);
    }
    else
    {
        rgbColour = RGBSmoothColour(colourIndexInTable,ZERO_COLOUR+maxEdgeColour);
    }

    sprintf(buffer,"#%06lX",rgbColour);
}


TIndex graphDisplayProxy::CanvasArcColour(TArc a) throw(ERRange)
{
    if (arcColourMode==ARCS_COLOUR_PRED)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);

        if (G.Pred(v)==a || G.Pred(u)==(a^1))
        {
            return EXPOSED_COLOUR;
        }
    }
    else if (arcColourMode==ARCS_UNCOLOURED)
    {
        return OUTLINE_COLOUR;
    }

    TIndex thisColour = G.EdgeColour(a);

    if (thisColour==NoArc) return VAGUE_COLOUR;

    if (arcColourMode==ARCS_REPEAT_COLOURS)
    {
        return ZERO_COLOUR + G.EdgeColour(a)%(MAX_COLOUR-ZERO_COLOUR);
    }
    else if (arcColourMode==ARCS_FIXED_COLOURS && thisColour>=MAX_COLOUR-ZERO_COLOUR)
    {
        return VAGUE_COLOUR;
    }

    return ZERO_COLOUR + thisColour;
}


long graphDisplayProxy::CanvasArcWidth(TArc a) throw(ERRange)
{
    TFloat thisSub = fabs(G.Sub(a));
    bool isSubgraphArc = (thisSub>CT.epsilon);
    long arcWidth = arcWidthMin;

    if (arcWidthMode==ARC_WIDTH_PREDECESSORS)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);

        if (G.Pred(v)==a || G.Pred(u)==(a^1))
        {
            arcWidth = arcWidthMax;
        }
    }
    else if (arcWidthMode==ARC_WIDTH_SUBGRAPH)
    {
        if (isSubgraphArc)
        {
            arcWidth = arcWidthMax;
        }
    }
    else if (arcWidthMode==ARC_WIDTH_FLOW_LINEAR)
    {
        arcWidth = arcWidthMin + int(thisSub*(arcWidthMax-arcWidthMin));
    }
    else if (arcWidthMode==ARC_WIDTH_FLOW_LOGARITHMIC)
    {
        arcWidth = arcWidthMin + int(log(thisSub+1.0)*(arcWidthMax-arcWidthMin));
    }
    else if (arcWidthMode==ARC_WIDTH_EMPTY_FREE_FULL)
    {
        if (thisSub<G.LCap(a)+CT.epsilon)
        {
            arcWidth = arcWidthMin;
        }
        else if (thisSub<G.UCap(a)-CT.epsilon)
        {
            arcWidth = (arcWidthMin+arcWidthMax)/2;
        }
        else
        {
            arcWidth = arcWidthMax;
        }
    }

    double arcWidthScaled = arcWidth*globalZoom;

    if (arcWidthScaled<1.0) return 1;

    return long(arcWidthScaled);
}


TDashMode graphDisplayProxy::CanvasArcDashMode(TArc a) throw(ERRange)
{
    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);
    TFloat thisSub = fabs(G.Sub(a));

    if (arcStippleMode==ARC_STIPPLE_PREDECESSORS)
    {
        if (G.Pred(v)==a || G.Pred(u)==(a^1))
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_FREE)
    {
        if (   thisSub>G.LCap(a)+CT.epsilon
            && thisSub<G.UCap(a)-CT.epsilon
            )
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_FRACTIONAL)
    {
        if (floor(thisSub)!=thisSub)
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_COLOURS)
    {
        if (G.EdgeColour(a)!=NoArc)
        {
            return TDashMode(int(G.EdgeColour(a))%4);
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_VOID)
    {
        if (thisSub<CT.epsilon)
        {
            return LINE_STYLE_DOT;
        }
    }
    else if (arcStippleMode==ARC_STIPPLE_SUBGRAPH)
    {
        if (thisSub>CT.epsilon)
        {
            return LINE_STYLE_DOT;
        }
    }

    return LINE_STYLE_SOLID;
}


void graphDisplayProxy::PlaceLayoutPoint(TNode p,long cx,long cy) throw(ERRange)
{
    TFloat ocx = (cx*pixelWidth/globalZoom  - offset[ix])/objectZoom;
    TFloat ocy = (cy*pixelHeight/globalZoom - offset[iy])/objectZoom;

    if (p<G.N())
    {
        ocx = round(ocx/nodeSpacing)*nodeSpacing;
        ocy = round(ocy/nodeSpacing)*nodeSpacing;
    }
    else if (bendSpacing>CT.epsilon)
    {
        ocx = round(ocx/bendSpacing)*bendSpacing;
        ocy = round(ocy/bendSpacing)*bendSpacing;
    }

    G.Representation() -> SetC(p,ix,ocx);
    G.Representation() -> SetC(p,iy,ocy);
}


void graphDisplayProxy::PlaceArcLabelAnchor(TArc a,long cx,long cy) throw(ERRange)
{
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    TNode p = GR->ProvideArcLabelAnchor(a);

    TFloat ocx = (cx*pixelWidth/globalZoom  - offset[ix])/objectZoom;
    TFloat ocy = (cy*pixelHeight/globalZoom - offset[iy])/objectZoom;

    if (fineSpacing>CT.epsilon)
    {
        ocx = round(ocx/fineSpacing)*fineSpacing;
        ocy = round(ocy/fineSpacing)*fineSpacing;
    }

    G.Representation() -> SetC(p,ix,ocx);
    G.Representation() -> SetC(p,iy,ocy);
}
