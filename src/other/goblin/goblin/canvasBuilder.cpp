
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file canvasBuilder.cpp
/// \brief #canvasBuilder class implementation

#include "canvasBuilder.h"


canvasBuilder::canvasBuilder(const abstractMixedGraph &GC,double _pixelWidth,double _pixelHeight)
    throw(ERRejected) :
    managedObject(GC.Context()),
    G(GC), CFG(GC.Context(),true),
    DP(const_cast<abstractMixedGraph&>(GC),_pixelWidth,_pixelHeight)
{
    ExtractParameters();
    ComputeBoundingBox();

    CFG.logMeth = 0;

    LogEntry(LOG_MEM,"...Display object instanciated");
}


canvasBuilder::~canvasBuilder() throw()
{
    LogEntry(LOG_MEM,"...Display object disallocated");
}


unsigned long canvasBuilder::Allocated() const throw()
{
    return 255;
}


void canvasBuilder::ComputeBoundingBox() throw()
{
    G.Layout_GetBoundingInterval(0,minX,maxX);
    G.Layout_GetBoundingInterval(1,minY,maxY);
}


void canvasBuilder::ExtractParameters() throw()
{
    TLayoutModel layoutModel = G.LayoutModel();

    // Copy parameters from the graph layout pool or set default values
    G.GetLayoutParameter(TokLayoutArcShapeMode,arcShapeMode,layoutModel);
    G.GetLayoutParameter(TokLayoutArcFontType,arcFontType);

    G.GetLayoutParameter(TokLayoutArrowPosMode,arrowPosMode);

    G.GetLayoutParameter(TokLayoutNodeFontType,nodeFontType);

    G.GetLayoutParameter(TokLayoutGridDisplayMode,gridDisplayMode);
    G.GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    portMode = (layoutModel==LAYOUT_KANDINSKI || layoutModel==LAYOUT_VISIBILITY || layoutModel==LAYOUT_ORTHO_BIG) ?
        PORTS_EXPLICIT : PORTS_IMPLICIT;
}


void canvasBuilder::DisplayGraph() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (G.Dim()==0)
    {
        G.Error(ERR_REJECTED,"canvasBuilder","Missing geometric embedding");
    }

    #endif

    DisplayPageLayout();

    for (TArc a=0;a<G.M();++a)
    {
        if (!DP.IsArcMapped(2*a)) continue;

        DisplayArc(a);
    }

    for (TNode v=0;v<G.N();++v)
    {
        if (!DP.IsNodeMapped(v)) continue;

        DisplayNode(v);
    }

    if (CFG.legenda>0)
    {
        long xm = long(DP.CanvasCX((minX+maxX)/2.0));
        long ym = long(DP.CanvasCY(maxY+CFG.legenda));

        DisplayLegenda(xm,ym,long((DP.CanvasCX(maxX)-DP.CanvasCX(minX))/4.0));
    }
}


void canvasBuilder::DisplayPageLayout() throw()
{
    const int GRID_DEPTH = 255;

    // Display some bounding box in any case. This may be transparent however.
    // This only covers the effective display region of the graph object, not the whole sheet
    TIndex borderColour = (gridDisplayMode==GRID_DISPLAY_OFF) ? NO_COLOUR : GRID_COLOUR;

    WriteStraightLine(DP.CanvasCX(minX),DP.CanvasCY(minY),DP.CanvasCX(minX),DP.CanvasCY(maxY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasCX(maxX),DP.CanvasCY(minY),DP.CanvasCX(maxX),DP.CanvasCY(maxY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasCX(minX),DP.CanvasCY(minY),DP.CanvasCX(maxX),DP.CanvasCY(minY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasCX(minX),DP.CanvasCY(maxY),DP.CanvasCX(maxX),DP.CanvasCY(maxY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);

    if (gridDisplayMode==GRID_DISPLAY_OFF) return;

    // Display a bounding box covering the whole sheet
    WriteStraightLine(0.0,0.0,0.0,DP.CanvasHeight(),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasWidth(),0.0,DP.CanvasWidth(),DP.CanvasHeight(),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(0.0,0.0,DP.CanvasWidth(),0.0,
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(0.0,DP.CanvasHeight(),DP.CanvasWidth(),DP.CanvasHeight(),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);

    if (nodeSpacing<CT.epsilon) return;

    // Display grid lines in the interior of the bounding box
    int minXNodeGrid = int(ceil(minX/nodeSpacing+CT.epsilon));
    int maxXNodeGrid = int(floor(maxX/nodeSpacing-CT.epsilon));
    int minYNodeGrid = int(ceil(minY/nodeSpacing+CT.epsilon));
    int maxYNodeGrid = int(floor((maxY+CFG.legenda)/nodeSpacing-CT.epsilon));

    int i=0;

    for (i=minXNodeGrid;i<=maxXNodeGrid;++i)
    {
        WriteStraightLine(
            DP.CanvasCX(i*nodeSpacing),DP.CanvasCY(minY),
            DP.CanvasCX(i*nodeSpacing),DP.CanvasCY(maxY),
            LINE_STYLE_DOT,1,ARROW_NONE,GRID_COLOUR,GRID_DEPTH);
    }

    for (i=minYNodeGrid;i<=maxYNodeGrid;++i)
    {
        WriteStraightLine(
            DP.CanvasCX(minX),DP.CanvasCY(i*nodeSpacing),
            DP.CanvasCX(maxX),DP.CanvasCY(i*nodeSpacing),
            LINE_STYLE_DOT,1,ARROW_NONE,GRID_COLOUR,GRID_DEPTH);
    }
}


void canvasBuilder::DisplayArc(TArc a) throw()
{
    TNode u = G.StartNode(2*a);
    TNode v = G.EndNode(2*a);

    TArrowDir displayedArrows = DP.ArrowDirections(2*a);
    TArrowDir displayedExteriorArrows = ARROW_NONE;
    TArrowDir displayedCenteredArrows = ARROW_NONE;

    if (arrowPosMode==ARROWS_CENTERED)
    {
        if (displayedArrows!=ARROW_BOTH)
        {
            displayedCenteredArrows = displayedArrows;
        }
    }
    else
    {
        displayedExteriorArrows = displayedArrows;
    }

    int width = DP.CanvasArcWidth(2*a);
    TDashMode dashMode = DP.CanvasArcDashMode(2*a);
    TIndex colourIndex = DP.CanvasArcColour(2*a);
    int depth = (dashMode==LINE_STYLE_SOLID) ? 101 : 100;

    WriteArc(a,u,v,dashMode,width,displayedExteriorArrows,colourIndex,depth);

    if (displayedCenteredArrows!=ARROW_NONE)
    {
        TNode y = G.PortNode(2*a);
        TNode x = u;

        while (y!=NoNode)
        {
            if (portMode==PORTS_IMPLICIT || x!=u)
            {
                if (displayedCenteredArrows==ARROW_FORWARD) DisplayArrow(a,x,y);
                if (displayedCenteredArrows==ARROW_BACKWARD) DisplayArrow(a,y,x);
            }

            x = y;
            y = G.ThreadSuccessor(y);
        }

        if (x!=v && portMode==PORTS_IMPLICIT)
        {
            if (displayedCenteredArrows==ARROW_FORWARD) DisplayArrow(a,x,v);
            if (displayedCenteredArrows==ARROW_BACKWARD) DisplayArrow(a,v,x);
        }
    }

    if (u!=v || G.ArcLabelAnchor(2*a)!=NoNode)
    {
        long xm = DP.CanvasCXOfArcLabelAnchor(2*a);
        long ym = DP.CanvasCYOfArcLabelAnchor(2*a);
        WriteArcLabel(a,xm,ym);
    }
}


void canvasBuilder::WriteArc(TArc a,TNode u,TNode v,TDashMode dashMode,int width,
    TArrowDir displayedArrows,TIndex colourIndex,int depth) throw()
{
    vector<double> cx;
    vector<double> cy;

    TNode w = G.PortNode(2*a);

    if (w==NoNode)
    {
        cx.push_back(DP.CanvasCXOfPort(v,u));
        cy.push_back(DP.CanvasCYOfPort(v,u));
        cx.push_back(DP.CanvasCXOfPort(u,v));
        cy.push_back(DP.CanvasCYOfPort(u,v));
    }
    else
    {
        if (portMode==PORTS_IMPLICIT)
        {
            cx.push_back(DP.CanvasCXOfPort(w,u));
            cy.push_back(DP.CanvasCYOfPort(w,u));
        }

        TNode x = w;
        while (x!=NoNode)
        {
            double bx = DP.CanvasCXOfPoint(x);
            double by = DP.CanvasCYOfPoint(x);

            if ( cx.size()!=1 &&
                (   bx<DP.CanvasCX(minX)-CT.epsilon
                 || bx>DP.CanvasCX(maxX)+CT.epsilon
                 || by<DP.CanvasCY(minY)-CT.epsilon
                 || by>DP.CanvasCY(maxY)+CT.epsilon
                )
               )
            {
                // Bend node is outside the bounding box

                if (cx.size()>1)
                    WriteArcSegment(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);

                cx.clear();
                cy.clear();
            }
            else
            {
                cx.push_back(bx);
                cy.push_back(by);
            }

            w = x;
            x = G.ThreadSuccessor(x);
        }

        if (portMode==PORTS_IMPLICIT)
        {
            cx.push_back(DP.CanvasCXOfPort(w,v));
            cy.push_back(DP.CanvasCYOfPort(w,v));
        }
    }

    if (cx.size()>1) WriteArcSegment(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);
}


void canvasBuilder::WriteArcSegment(vector<double>& cx,vector<double>& cy,TDashMode dashMode,int width,
    TArrowDir displayedArrows,TIndex colourIndex,int depth) throw()
{
    if (dashMode==LINE_STYLE_SOLID)
    {
        WritePolyLine(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);
    }
    else
    {
        if (colourIndex!=OUTLINE_COLOUR)
        {
            WritePolyLine(cx,cy,LINE_STYLE_SOLID,width,displayedArrows,colourIndex,depth+1);
        }
        else
        {
            WritePolyLine(cx,cy,LINE_STYLE_SOLID,width,displayedArrows,ZERO_COLOUR + 3,depth+1);
        }

        WritePolyLine(cx,cy,dashMode,width,displayedArrows,OUTLINE_COLOUR,depth);
    }
}


void canvasBuilder::WriteStraightLine(double cx1,double cy1,double cx2,double cy2,
    TDashMode dashMode,int width,TArrowDir displayedArrows,
    TIndex colourIndex,int depth) throw()
{
    vector<double> cx(2);
    vector<double> cy(2);

    cx[0] = cx1;
    cy[0] = cy1;
    cx[1] = cx2;
    cy[1] = cy2;

    WritePolyLine(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);
}


void canvasBuilder::DisplayArrow(TArc a,TNode u,TNode v) throw()
{
    double dx = DP.CanvasCXOfPoint(v)-DP.CanvasCXOfPoint(u);
    double dy = DP.CanvasCYOfPoint(v)-DP.CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    // Cannot display the arrow head if u and v coincide in the canvas
    if (fabs(norm)<0.5) return;

    dx = dx/norm;
    dy = dy/norm;

    WriteArrow(a,
        long((DP.CanvasCXOfPoint(u)+DP.CanvasCXOfPoint(v))/2+DP.CanvasArrowSize()*dx),
        long((DP.CanvasCYOfPoint(u)+DP.CanvasCYOfPoint(v))/2+DP.CanvasArrowSize()*dy),
        dx,dy);
}


const char* unixFontType[] =
{
    "times-medium-r-normal-", //    FONT_TIMES_ROMAN = 0,
    "times-medium-i-normal-", //    FONT_TIMES_ITALIC = 1,
    "times-bold-r-normal-", //    FONT_TIMES_BOLD = 2,
    "times-bold-i-normal-", //    FONT_TIMES_BOLD_ITALIC = 3,
    "itc avant garde-medium-r-normal-sans", //    FONT_AVANTGARDE_BOOK = 4,
    "itc avant garde-medium-o-normal-sans", //    FONT_AVANTGARDE_BOOK_OBLIQUE = 5,
    "itc avant garde-demi-r-normal-sans",   //    FONT_AVANTGARDE_DEMI = 6,
    "itc avant garde-demi-o-normal-sans",   //    FONT_AVANTGARDE_DEMI_OBLIQUE = 7,
    "itc bookman-light-r-normal-", //    FONT_BOOKMAN_LIGHT = 8,
    "itc bookman-light-i-normal-", //    FONT_BOOKMAN_LIGHT_ITALIC = 9,
    "itc bookman l-medium-r-normal-", //    FONT_BOOKMAN_DEMI = 10,
    "itc bookman l-medium-i-normal-", //    FONT_BOOKMAN_DEMI_ITALIC = 11,
    "courier-medium-r-normal-",   //    FONT_COURIER = 12,
    "courier-medium-o-normal-",   //    FONT_COURIER_OBLIQUE = 13,
    "courier-bold-r-normal-",     //    FONT_COURIER_BOLD = 14,
    "courier-bold-o-normal-",     //    FONT_COURIER_BOLD_OBLIQUE = 15,
    "helvetica-medium-r-normal-", //    FONT_HELVETICA = 16,
    "helvetica-medium-o-normal-", //    FONT_HELVETICA_OBLIQUE = 17,
    "helvetica-bold-r-normal-",   //    FONT_HELVETICA_BOLD = 18,
    "helvetica-bold-o-normal-",   //    FONT_HELVETICA_BOLD_OBLIQUE = 19,
    "helvetica-medium-r-narrow-", //    FONT_HELVETICA_NARROW = 20,
    "helvetica-medium-o-narrow-", //    FONT_HELVETICA_NARROW_OBLIQUE = 21,
    "helvetica-bold-r-narrow-",   //    FONT_HELVETICA_NARROW_BOLD = 22,
    "helvetica-bold-o-narrow-",   //    FONT_HELVETICA_NARROW_BOLD_OBLIQUE = 23,
    "new century schoolbook-medium-r-normal-", //    FONT_NEW_CENTURY_SCHOOLBOOK_ROMAN = 24,
    "new century schoolbook-medium-i-normal-", //    FONT_NEW_CENTURY_SCHOOLBOOK_ITALIC = 25,
    "new century schoolbook-bold-r-normal-",   //    FONT_NEW_CENTURY_SCHOOLBOOK_BOLD = 26,
    "new century schoolbook-bold-i-normal-",   //    FONT_NEW_CENTURY_SCHOOLBOOK_BOLD_ITALIC = 27,
    "palatino-medium-r-normal-", //    FONT_PALATINO_ROMAN = 28,
    "palatino-medium-i-normal-", //    FONT_PALATINO_ITALIC = 29,
    "palatino-bold-r-normal-",   //    FONT_PALATINO_BOLD = 30,
    "palatino-bold-i-normal-",   //    FONT_PALATINO_BOLD_ITALIC = 31,
    "symbol-medium-r-normal-",   //    FONT_SYMBOL = 32,
    "itc zapf chancery-medium-i-normal-", //    FONT_ZAPF_CHANCERY_MEDIUM_ITALIC = 33,
    "itc zapf dingbats-medium-r-normal-"  //    FONT_ZAPF_DINGBATS = 34
};

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file canvasBuilder.cpp
/// \brief #canvasBuilder class implementation

#include "canvasBuilder.h"


canvasBuilder::canvasBuilder(const abstractMixedGraph &GC,double _pixelWidth,double _pixelHeight)
    throw(ERRejected) :
    managedObject(GC.Context()),
    G(GC), CFG(GC.Context(),true),
    DP(const_cast<abstractMixedGraph&>(GC),_pixelWidth,_pixelHeight)
{
    ExtractParameters();
    ComputeBoundingBox();

    CFG.logMeth = 0;

    LogEntry(LOG_MEM,"...Display object instanciated");
}


canvasBuilder::~canvasBuilder() throw()
{
    LogEntry(LOG_MEM,"...Display object disallocated");
}


unsigned long canvasBuilder::Allocated() const throw()
{
    return 255;
}


void canvasBuilder::ComputeBoundingBox() throw()
{
    G.Layout_GetBoundingInterval(0,minX,maxX);
    G.Layout_GetBoundingInterval(1,minY,maxY);
}


void canvasBuilder::ExtractParameters() throw()
{
    TLayoutModel layoutModel = G.LayoutModel();

    // Copy parameters from the graph layout pool or set default values
    G.GetLayoutParameter(TokLayoutArcShapeMode,arcShapeMode,layoutModel);
    G.GetLayoutParameter(TokLayoutArcFontType,arcFontType);

    G.GetLayoutParameter(TokLayoutArrowPosMode,arrowPosMode);

    G.GetLayoutParameter(TokLayoutNodeFontType,nodeFontType);

    G.GetLayoutParameter(TokLayoutGridDisplayMode,gridDisplayMode);
    G.GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    portMode = (layoutModel==LAYOUT_KANDINSKI || layoutModel==LAYOUT_VISIBILITY || layoutModel==LAYOUT_ORTHO_BIG) ?
        PORTS_EXPLICIT : PORTS_IMPLICIT;
}


void canvasBuilder::DisplayGraph() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (G.Dim()==0)
    {
        G.Error(ERR_REJECTED,"canvasBuilder","Missing geometric embedding");
    }

    #endif

    DisplayPageLayout();

    for (TArc a=0;a<G.M();++a)
    {
        if (!DP.IsArcMapped(2*a)) continue;

        DisplayArc(a);
    }

    for (TNode v=0;v<G.N();++v)
    {
        if (!DP.IsNodeMapped(v)) continue;

        DisplayNode(v);
    }

    if (CFG.legenda>0)
    {
        long xm = long(DP.CanvasCX((minX+maxX)/2.0));
        long ym = long(DP.CanvasCY(maxY+CFG.legenda));

        DisplayLegenda(xm,ym,long((DP.CanvasCX(maxX)-DP.CanvasCX(minX))/4.0));
    }
}


void canvasBuilder::DisplayPageLayout() throw()
{
    const int GRID_DEPTH = 255;

    // Display some bounding box in any case. This may be transparent however.
    // This only covers the effective display region of the graph object, not the whole sheet
    TIndex borderColour = (gridDisplayMode==GRID_DISPLAY_OFF) ? NO_COLOUR : GRID_COLOUR;

    WriteStraightLine(DP.CanvasCX(minX),DP.CanvasCY(minY),DP.CanvasCX(minX),DP.CanvasCY(maxY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasCX(maxX),DP.CanvasCY(minY),DP.CanvasCX(maxX),DP.CanvasCY(maxY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasCX(minX),DP.CanvasCY(minY),DP.CanvasCX(maxX),DP.CanvasCY(minY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasCX(minX),DP.CanvasCY(maxY),DP.CanvasCX(maxX),DP.CanvasCY(maxY),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);

    if (gridDisplayMode==GRID_DISPLAY_OFF) return;

    // Display a bounding box covering the whole sheet
    WriteStraightLine(0.0,0.0,0.0,DP.CanvasHeight(),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(DP.CanvasWidth(),0.0,DP.CanvasWidth(),DP.CanvasHeight(),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(0.0,0.0,DP.CanvasWidth(),0.0,
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);
    WriteStraightLine(0.0,DP.CanvasHeight(),DP.CanvasWidth(),DP.CanvasHeight(),
        LINE_STYLE_SOLID,1,ARROW_NONE,borderColour,GRID_DEPTH);

    if (nodeSpacing<CT.epsilon) return;

    // Display grid lines in the interior of the bounding box
    int minXNodeGrid = int(ceil(minX/nodeSpacing+CT.epsilon));
    int maxXNodeGrid = int(floor(maxX/nodeSpacing-CT.epsilon));
    int minYNodeGrid = int(ceil(minY/nodeSpacing+CT.epsilon));
    int maxYNodeGrid = int(floor((maxY+CFG.legenda)/nodeSpacing-CT.epsilon));

    int i=0;

    for (i=minXNodeGrid;i<=maxXNodeGrid;++i)
    {
        WriteStraightLine(
            DP.CanvasCX(i*nodeSpacing),DP.CanvasCY(minY),
            DP.CanvasCX(i*nodeSpacing),DP.CanvasCY(maxY),
            LINE_STYLE_DOT,1,ARROW_NONE,GRID_COLOUR,GRID_DEPTH);
    }

    for (i=minYNodeGrid;i<=maxYNodeGrid;++i)
    {
        WriteStraightLine(
            DP.CanvasCX(minX),DP.CanvasCY(i*nodeSpacing),
            DP.CanvasCX(maxX),DP.CanvasCY(i*nodeSpacing),
            LINE_STYLE_DOT,1,ARROW_NONE,GRID_COLOUR,GRID_DEPTH);
    }
}


void canvasBuilder::DisplayArc(TArc a) throw()
{
    TNode u = G.StartNode(2*a);
    TNode v = G.EndNode(2*a);

    TArrowDir displayedArrows = DP.ArrowDirections(2*a);
    TArrowDir displayedExteriorArrows = ARROW_NONE;
    TArrowDir displayedCenteredArrows = ARROW_NONE;

    if (arrowPosMode==ARROWS_CENTERED)
    {
        if (displayedArrows!=ARROW_BOTH)
        {
            displayedCenteredArrows = displayedArrows;
        }
    }
    else
    {
        displayedExteriorArrows = displayedArrows;
    }

    int width = DP.CanvasArcWidth(2*a);
    TDashMode dashMode = DP.CanvasArcDashMode(2*a);
    TIndex colourIndex = DP.CanvasArcColour(2*a);
    int depth = (dashMode==LINE_STYLE_SOLID) ? 101 : 100;

    WriteArc(a,u,v,dashMode,width,displayedExteriorArrows,colourIndex,depth);

    if (displayedCenteredArrows!=ARROW_NONE)
    {
        TNode y = G.PortNode(2*a);
        TNode x = u;

        while (y!=NoNode)
        {
            if (portMode==PORTS_IMPLICIT || x!=u)
            {
                if (displayedCenteredArrows==ARROW_FORWARD) DisplayArrow(a,x,y);
                if (displayedCenteredArrows==ARROW_BACKWARD) DisplayArrow(a,y,x);
            }

            x = y;
            y = G.ThreadSuccessor(y);
        }

        if (x!=v && portMode==PORTS_IMPLICIT)
        {
            if (displayedCenteredArrows==ARROW_FORWARD) DisplayArrow(a,x,v);
            if (displayedCenteredArrows==ARROW_BACKWARD) DisplayArrow(a,v,x);
        }
    }

    if (u!=v || G.ArcLabelAnchor(2*a)!=NoNode)
    {
        long xm = DP.CanvasCXOfArcLabelAnchor(2*a);
        long ym = DP.CanvasCYOfArcLabelAnchor(2*a);
        WriteArcLabel(a,xm,ym);
    }
}


void canvasBuilder::WriteArc(TArc a,TNode u,TNode v,TDashMode dashMode,int width,
    TArrowDir displayedArrows,TIndex colourIndex,int depth) throw()
{
    vector<double> cx;
    vector<double> cy;

    TNode w = G.PortNode(2*a);

    if (w==NoNode)
    {
        cx.push_back(DP.CanvasCXOfPort(v,u));
        cy.push_back(DP.CanvasCYOfPort(v,u));
        cx.push_back(DP.CanvasCXOfPort(u,v));
        cy.push_back(DP.CanvasCYOfPort(u,v));
    }
    else
    {
        if (portMode==PORTS_IMPLICIT)
        {
            cx.push_back(DP.CanvasCXOfPort(w,u));
            cy.push_back(DP.CanvasCYOfPort(w,u));
        }

        TNode x = w;
        while (x!=NoNode)
        {
            double bx = DP.CanvasCXOfPoint(x);
            double by = DP.CanvasCYOfPoint(x);

            if ( cx.size()!=1 &&
                (   bx<DP.CanvasCX(minX)-CT.epsilon
                 || bx>DP.CanvasCX(maxX)+CT.epsilon
                 || by<DP.CanvasCY(minY)-CT.epsilon
                 || by>DP.CanvasCY(maxY)+CT.epsilon
                )
               )
            {
                // Bend node is outside the bounding box

                if (cx.size()>1)
                    WriteArcSegment(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);

                cx.clear();
                cy.clear();
            }
            else
            {
                cx.push_back(bx);
                cy.push_back(by);
            }

            w = x;
            x = G.ThreadSuccessor(x);
        }

        if (portMode==PORTS_IMPLICIT)
        {
            cx.push_back(DP.CanvasCXOfPort(w,v));
            cy.push_back(DP.CanvasCYOfPort(w,v));
        }
    }

    if (cx.size()>1) WriteArcSegment(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);
}


void canvasBuilder::WriteArcSegment(vector<double>& cx,vector<double>& cy,TDashMode dashMode,int width,
    TArrowDir displayedArrows,TIndex colourIndex,int depth) throw()
{
    if (dashMode==LINE_STYLE_SOLID)
    {
        WritePolyLine(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);
    }
    else
    {
        if (colourIndex!=OUTLINE_COLOUR)
        {
            WritePolyLine(cx,cy,LINE_STYLE_SOLID,width,displayedArrows,colourIndex,depth+1);
        }
        else
        {
            WritePolyLine(cx,cy,LINE_STYLE_SOLID,width,displayedArrows,ZERO_COLOUR + 3,depth+1);
        }

        WritePolyLine(cx,cy,dashMode,width,displayedArrows,OUTLINE_COLOUR,depth);
    }
}


void canvasBuilder::WriteStraightLine(double cx1,double cy1,double cx2,double cy2,
    TDashMode dashMode,int width,TArrowDir displayedArrows,
    TIndex colourIndex,int depth) throw()
{
    vector<double> cx(2);
    vector<double> cy(2);

    cx[0] = cx1;
    cy[0] = cy1;
    cx[1] = cx2;
    cy[1] = cy2;

    WritePolyLine(cx,cy,dashMode,width,displayedArrows,colourIndex,depth);
}


void canvasBuilder::DisplayArrow(TArc a,TNode u,TNode v) throw()
{
    double dx = DP.CanvasCXOfPoint(v)-DP.CanvasCXOfPoint(u);
    double dy = DP.CanvasCYOfPoint(v)-DP.CanvasCYOfPoint(u);
    double norm = sqrt(dx*dx+dy*dy);

    // Cannot display the arrow head if u and v coincide in the canvas
    if (fabs(norm)<0.5) return;

    dx = dx/norm;
    dy = dy/norm;

    WriteArrow(a,
        long((DP.CanvasCXOfPoint(u)+DP.CanvasCXOfPoint(v))/2+DP.CanvasArrowSize()*dx),
        long((DP.CanvasCYOfPoint(u)+DP.CanvasCYOfPoint(v))/2+DP.CanvasArrowSize()*dy),
        dx,dy);
}


const char* unixFontType[] =
{
    "times-medium-r-normal-", //    FONT_TIMES_ROMAN = 0,
    "times-medium-i-normal-", //    FONT_TIMES_ITALIC = 1,
    "times-bold-r-normal-", //    FONT_TIMES_BOLD = 2,
    "times-bold-i-normal-", //    FONT_TIMES_BOLD_ITALIC = 3,
    "itc avant garde-medium-r-normal-sans", //    FONT_AVANTGARDE_BOOK = 4,
    "itc avant garde-medium-o-normal-sans", //    FONT_AVANTGARDE_BOOK_OBLIQUE = 5,
    "itc avant garde-demi-r-normal-sans",   //    FONT_AVANTGARDE_DEMI = 6,
    "itc avant garde-demi-o-normal-sans",   //    FONT_AVANTGARDE_DEMI_OBLIQUE = 7,
    "itc bookman-light-r-normal-", //    FONT_BOOKMAN_LIGHT = 8,
    "itc bookman-light-i-normal-", //    FONT_BOOKMAN_LIGHT_ITALIC = 9,
    "itc bookman l-medium-r-normal-", //    FONT_BOOKMAN_DEMI = 10,
    "itc bookman l-medium-i-normal-", //    FONT_BOOKMAN_DEMI_ITALIC = 11,
    "courier-medium-r-normal-",   //    FONT_COURIER = 12,
    "courier-medium-o-normal-",   //    FONT_COURIER_OBLIQUE = 13,
    "courier-bold-r-normal-",     //    FONT_COURIER_BOLD = 14,
    "courier-bold-o-normal-",     //    FONT_COURIER_BOLD_OBLIQUE = 15,
    "helvetica-medium-r-normal-", //    FONT_HELVETICA = 16,
    "helvetica-medium-o-normal-", //    FONT_HELVETICA_OBLIQUE = 17,
    "helvetica-bold-r-normal-",   //    FONT_HELVETICA_BOLD = 18,
    "helvetica-bold-o-normal-",   //    FONT_HELVETICA_BOLD_OBLIQUE = 19,
    "helvetica-medium-r-narrow-", //    FONT_HELVETICA_NARROW = 20,
    "helvetica-medium-o-narrow-", //    FONT_HELVETICA_NARROW_OBLIQUE = 21,
    "helvetica-bold-r-narrow-",   //    FONT_HELVETICA_NARROW_BOLD = 22,
    "helvetica-bold-o-narrow-",   //    FONT_HELVETICA_NARROW_BOLD_OBLIQUE = 23,
    "new century schoolbook-medium-r-normal-", //    FONT_NEW_CENTURY_SCHOOLBOOK_ROMAN = 24,
    "new century schoolbook-medium-i-normal-", //    FONT_NEW_CENTURY_SCHOOLBOOK_ITALIC = 25,
    "new century schoolbook-bold-r-normal-",   //    FONT_NEW_CENTURY_SCHOOLBOOK_BOLD = 26,
    "new century schoolbook-bold-i-normal-",   //    FONT_NEW_CENTURY_SCHOOLBOOK_BOLD_ITALIC = 27,
    "palatino-medium-r-normal-", //    FONT_PALATINO_ROMAN = 28,
    "palatino-medium-i-normal-", //    FONT_PALATINO_ITALIC = 29,
    "palatino-bold-r-normal-",   //    FONT_PALATINO_BOLD = 30,
    "palatino-bold-i-normal-",   //    FONT_PALATINO_BOLD_ITALIC = 31,
    "symbol-medium-r-normal-",   //    FONT_SYMBOL = 32,
    "itc zapf chancery-medium-i-normal-", //    FONT_ZAPF_CHANCERY_MEDIUM_ITALIC = 33,
    "itc zapf dingbats-medium-r-normal-"  //    FONT_ZAPF_DINGBATS = 34
};
