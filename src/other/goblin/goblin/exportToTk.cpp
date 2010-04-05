
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   exportToTk.cpp
/// \brief  #exportToTk class implementation

#include "exportToTk.h"


exportToTk::exportToTk(const abstractMixedGraph& _G,const char* expFileName) throw(ERFile) :
    canvasBuilder(_G,_G.Context().pixelWidth,_G.Context().pixelHeight), expFile(expFileName, ios::out)
{
    if (!expFile)
    {
        sprintf(CT.logBuffer,"Could not open export file %s, io_state %d",
            expFileName,expFile.rdstate());
        Error(ERR_FILE,"exportToTk",CT.logBuffer);
    }

    expFile.flags(expFile.flags() | ios::right);
    expFile.setf(ios::floatfield);
    expFile.precision(5);

    canvasName = "$goblinCanvas";

    expFile << "set goblinCanvasObjects {" << endl;

    int minXNodeGrid = int(floor(minX/nodeSpacing-0.5));
    int maxXNodeGrid = int( ceil(maxX/nodeSpacing+0.5));
    int minYNodeGrid = int(floor(minY/nodeSpacing-0.5));
    int maxYNodeGrid = int(ceil((maxY+CFG.legenda)/nodeSpacing+0.5));

    expFile << "  {-1 " << ID_UPPER_LEFT << " line {"
        << int(DP.CanvasCX(minXNodeGrid*nodeSpacing)) << " "
        << int(DP.CanvasCY(minYNodeGrid*nodeSpacing)) << "} {} } \\" << endl;
    expFile << "  {-1 " << ID_LOWER_RIGHT << " line {"
        << int(DP.CanvasCX(maxXNodeGrid*nodeSpacing)) << " "
        << int(DP.CanvasCY(maxYNodeGrid*nodeSpacing)) << "} {} } \\" << endl;

    if (strcmp(CFG.wallpaper,"")!=0)
    {
//        expFile << "image create photo wallpaper -file \""
//            << CFG.wallpaper << "\"" << endl;
//        expFile << "$goblinCanvas create image 0 0 -image wallpaper"
//            << " -anchor nw" << endl;
    }
}


unsigned long exportToTk::Size() const throw()
{
    return
          sizeof(exportToTk)
        + managedObject::Allocated();
}


unsigned long exportToTk::Allocated() const throw()
{
    return 0;
}


void exportToTk::DisplayLegenda(long xm,long ym,long radius) throw()
{
    long xl = xm-radius;
    long xr = xm+radius;
    long nodeWidth  = long(DP.CanvasNodeWidth());
    long nodeHeight = long(DP.CanvasNodeHeight());
    long arrowSize  = long(DP.CanvasArrowSize());
    long textShift  = arrowSize;

    if (!G.IsUndirected())
    {
        expFile << "  {-1 " << ID_GRAPH_EDGE << " line {"
            << xl << " " << ym << " " << xr-nodeWidth << " " << ym
            << "} {-width 2 -joinstyle bevel";

        if (arrowPosMode==ARROWS_CENTERED)
        {
            expFile << "} } \\" << endl;
            WriteArrow(NoNode,xm+arrowSize,ym,1,0);
            textShift = 2*arrowSize;
        }
        else
        {
            expFile << " -arrow last -arrowshape {"
                << arrowSize*2 << " " << arrowSize*3
                << " " << arrowSize*1 << "}" << "} } \\" << endl;
        }
    }
    else
    {
        expFile << "  {-1 " << ID_GRAPH_EDGE << " line {"
            << xl << " " << ym << " " << xr-nodeWidth << " " << ym
            << "} {-width 2 -joinstyle bevel} } \\" << endl;
    }

    DP.ArcLegenda(tmpLabelBuffer,LABEL_BUFFER_SIZE,"a");

    if (tmpLabelBuffer[0]!=0)
    {
        expFile << "  {-1 " << ID_EDGE_LABEL << " text {"
            << xm << " " << ym-textShift << "} {-text {"
            << tmpLabelBuffer << "} -anchor c -font \""
            << "-adobe-" << unixFontType[arcFontType] << "-"
            << int(DP.ArcLabelFontSize()) << "-0-0-0-p-0-iso8859-1"
            << "\"} } \\" << endl;
    }

    switch (DP.NodeShapeMode())
    {
        case NODE_SHAPE_POINT:
        {
            WriteSmallNode(NoNode,xl,ym);
            WriteSmallNode(NoNode,xr,ym);
            WriteNodeLegenda(xl,ym+nodeWidth,"u");
            WriteNodeLegenda(xr,ym+nodeHeight,"v");

            return;
        }
        case NODE_SHAPE_BY_COLOUR:
        case NODE_SHAPE_CIRCULAR:
        {
            WriteCircularNode(NoNode,xl,ym,"#ffffff");
            WriteCircularNode(NoNode,xr,ym,"#ffffff");
            WriteNodeLegenda(xl,ym,"u");
            WriteNodeLegenda(xr,ym,"v");

            return;
        }
        case NODE_SHAPE_BOX:
        {
            WriteRectangularNode(NoNode,xl,ym,"#ffffff");
            WriteRectangularNode(NoNode,xr,ym,"#ffffff");
            WriteNodeLegenda(xl,ym,"u");
            WriteNodeLegenda(xr,ym,"v");

            return;
        }
    }
}


void exportToTk::WriteNodeLegenda(long x,long y,char* nodeLabel) throw()
{
    DP.NodeLegenda(tmpLabelBuffer,LABEL_BUFFER_SIZE,nodeLabel);

    if (tmpLabelBuffer[0]==0) return;

    expFile << "  {-1 " << ID_NODE_LABEL << " text {"
        << x << " " << y << "} {-text {"
        << tmpLabelBuffer << "} -anchor c -font \""
        << "-adobe-" << unixFontType[nodeFontType] << "-"
        << int(DP.NodeLabelFontSize()) << "-0-0-0-p-0-iso8859-1"
        << "\"} } \\" << endl;
}


void exportToTk::WritePolyLine(vector<double>& cx,vector<double>& cy,
    TDashMode dashMode,int width,TArrowDir displayedArrows,
    TIndex colourIndex,int depth) throw()
{
    // Write coordinates
    vector<double>::iterator px = cx.begin();
    vector<double>::iterator py = cy.begin();

    expFile << "  {" << 0 << " " << ID_GRID_LINE << " line {";

    while (px!=cx.end() && py!=cy.end())
    {
        expFile << long(*px) << " " << long(*py) << " ";
        ++px;
        ++py;
    }

    expFile << "} {";


    // Write stipple mode
    const char* dashModes[] = {""," -dash ."," -dash -"," -dash -."};
    expFile << dashModes[dashMode%4];


    // Write line colour
    char rgbColour[8];

    if (colourIndex<ZERO_COLOUR || DP.arcColourMode!=graphDisplayProxy::ARCS_FLOATING_COLOURS)
    {
        sprintf(rgbColour,"#%06lX",DP.RGBFixedColour(colourIndex));
    }
    else
    {
        sprintf(rgbColour,"#%06lX",DP.RGBSmoothColour(colourIndex,ZERO_COLOUR+DP.maxEdgeColour));
    }

    expFile << " -fill " << rgbColour;

    if (arcShapeMode==ARC_SHAPE_SMOOTH)
    {
        expFile << " -smooth true";
    }


    // Write arrow display mode
    if (displayedArrows!=ARROW_NONE)
    {
        if (displayedArrows==ARROW_FORWARD)
        {
            expFile << " -arrow last";
        }
        else if (displayedArrows==ARROW_BACKWARD)
        {
            expFile << " -arrow first";
        }
        else
        {
            expFile << " -arrow both";
        }

        double arrowSize = DP.CanvasArrowSize();
        expFile << " -arrowshape {"
                << long(arrowSize*2) << " " << long(arrowSize*3)
                << " " << long(arrowSize*1) << "}";
    }


    // 
    if (cx.size()>2) expFile << " -joinstyle bevel";


    // Write line width
    expFile << " -width " << width << "} } \\" << endl;
}


void exportToTk::WriteArrow(TArc a,long xtop,long ytop,double dx,double dy) throw()
{
    double arrowSize = DP.CanvasArrowSize();
    double ox = dy;
    double oy = -dx;

    expFile << "  {" << a << " " << ID_ARROW << " polygon {"
        << long(xtop) << " "
        << long(ytop) << " "
        << long(xtop-(2*dx+ox)*arrowSize) << " "
        << long(ytop-(2*dy+oy)*arrowSize) << " "
        << long(xtop-dx*arrowSize) << " "
        << long(ytop-dy*arrowSize) << " "
        << long(xtop-(2*dx-ox)*arrowSize) << " "
        << long(ytop-(2*dy-oy)*arrowSize) << " "
        << long(xtop) << " "
        << long(ytop)
        << "} {-outline #000000 -fill #000000} } \\" << endl;
}


void exportToTk::WriteArcLabel(TArc a,long xm,long ym) throw()
{
    DP.CompoundArcLabel(tmpLabelBuffer,LABEL_BUFFER_SIZE,2*a);

    if (tmpLabelBuffer[0]==0) return;

    expFile << "  {" << a << " " << ID_EDGE_LABEL << " text {"
        << xm << " " << ym << "} {-text {" << tmpLabelBuffer
        << "} -anchor c -font \""
        << "-adobe-" << unixFontType[arcFontType] << "-"
        << int(DP.ArcLabelFontSize()) << "-0-0-0-p-0-iso8859-1"
        << "\"} } \\" << endl;
}


void exportToTk::DisplayNode(TNode v) throw()
{
    char fillColour[8];
    DP.CanvasNodeColour(fillColour,v);

    long x = DP.CanvasCXOfPoint(v);
    long y = DP.CanvasCYOfPoint(v);

    switch (DP.NodeShapeMode(v))
    {
        case NODE_SHAPE_POINT:
        {
            WriteSmallNode(v,x,y);
            TNode p = G.ThreadSuccessor(v);

            if (p==NoNode)
            {
                WriteNodeLabel(v,x+long(DP.CanvasNodeWidth(v)),y+long(DP.CanvasNodeHeight(v)));
            }
            else
            {
                WriteNodeLabel(v,DP.CanvasCXOfPoint(p),DP.CanvasCYOfPoint(p));
            }

            return;
         }
         case NODE_SHAPE_CIRCULAR:
         {
             WriteCircularNode(v,x,y,fillColour);
             WriteNodeLabel(v,x,y);

             return;
         }
         case NODE_SHAPE_BOX:
         {
             WriteRectangularNode(v,x,y,fillColour);
             WriteNodeLabel(v,x,y);

             return;
         }
         case NODE_SHAPE_BY_COLOUR:
         {
             WriteRegularNode(v,x,y,fillColour);
             WriteNodeLabel(v,x,y);

             return;
         }
    }
}


void exportToTk::WriteSmallNode(TNode v,long x,long y) throw()
{
    int radius = 5;

    if (v==NoNode) expFile << "  {-1";
    else expFile << "  {" << v;

    expFile << " " << ID_GRAPH_NODE << " oval {"
        << x-radius << " " << y-radius << " " << x+radius << " " << y+radius
        << "} {-outline #000000 -fill #000000 -width 1} } \\" << endl;
}


void exportToTk::WriteCircularNode(TNode v,long x,long y,char* fillColour) throw()
{
    double radiusX = DP.CanvasNodeWidth(v);
    double radiusY = DP.CanvasNodeHeight(v);

    if (v==NoNode) expFile << "  {-1";
    else expFile << "  {" << v;

    expFile << " " << ID_GRAPH_NODE << " oval {"
        << RoundToLong(x-radiusX) << " " << RoundToLong(y-radiusY) << " "
        << RoundToLong(x+radiusX) << " " << RoundToLong(y+radiusY)
        << "} {-outline #000000 -fill " << fillColour << "} } \\" << endl;
}


void exportToTk::WriteRectangularNode(TNode v,long x,long y,char* fillColour) throw()
{
    long width  = DP.CanvasNodeWidth(v);
    long height = DP.CanvasNodeHeight(v);

    if (v==NoNode) expFile << "  {-1";
    else expFile << "  {" << v;

    expFile << " " << ID_GRAPH_NODE << " rectangle {"
        << (x-width) << " " << (y-height) << " "
        << (x+width) << " " << (y+height)
        << "} {-outline #000000 -fill " << fillColour << "} } \\" << endl;
}


void exportToTk::WriteRegularNode(TNode v,long x,long y,char* fillColour) throw()
{
    if (G.NodeColour(v)==0 || G.NodeColour(v)>G.N())
    {
        return WriteCircularNode(v,x,y,fillColour);
    }

    if (v==NoNode) expFile << "  {-1";
    else expFile << "  {" << v;

    unsigned nVertices = G.NodeColour(v)+3;
    double radiusX = DP.CanvasNodeWidth(v)/cos(PI/nVertices);
    double radiusY = DP.CanvasNodeHeight(v)/cos(PI/nVertices);

    expFile << " " << ID_GRAPH_NODE << " poly {";

    for (unsigned i=0;i<nVertices;++i)
    {
        double xRel = radiusX*sin((i+0.5)*2*PI/nVertices);
        double yRel = radiusY*cos((i+0.5)*2*PI/nVertices);

        expFile << RoundToLong(x-xRel) << " " << RoundToLong(y+yRel) << " ";
    }

    expFile << "} {-outline #000000 -fill " << fillColour << "} } \\" << endl;
}


void exportToTk::WriteNodeLabel(TNode v,long x,long y) throw()
{
    DP.CompoundNodeLabel(tmpLabelBuffer,LABEL_BUFFER_SIZE,v);

    if (tmpLabelBuffer[0]==0) return;

    expFile << "  {" << v << " " << ID_NODE_LABEL << " text {"
        << x << " " << y << "} {-text {" << tmpLabelBuffer
        << "}" << " -anchor c -font \""
        << "-adobe-" << unixFontType[nodeFontType] << "-"
        << int(DP.NodeLabelFontSize()) << "-0-0-0-p-0-iso8859-1"
        << "\"} } \\" << endl;
}


void exportToTk::DisplayArtificialNode(TNode v) throw()
{
    long x = DP.CanvasCXOfPoint(v);
    long y = DP.CanvasCYOfPoint(v);
    int radius = 4;

    expFile << "  {" << v << " " << ID_BEND_NODE << " rectangle {"
        << x-radius << " " << y-radius << " " << x+radius << " " << y+radius
        << "} {-outline #000000 -fill #000000} } \\" << endl;
}


exportToTk::~exportToTk() throw()
{
    for (TNode i=0;i<G.NI();++i) DisplayArtificialNode(G.N()+i);

    expFile << "}" << endl;
    expFile.close();
}
