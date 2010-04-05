
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file graphDisplayProxy.h
/// \brief #graphDisplayProxy class interface

#ifndef _GRAPH_DISPLAY_PROXY_H_
#define _GRAPH_DISPLAY_PROXY_H_

#include "abstractMixedGraph.h"


/// \addtogroup groupCanvasBuilder
/// @{

enum TNodeShapeMode
{
    NODE_SHAPE_POINT     = 0,
    NODE_SHAPE_CIRCULAR  = 1,
    NODE_SHAPE_BOX       = 2,
    NODE_SHAPE_BY_COLOUR = 3
};

enum TArrowDir
{
    ARROW_NONE     = 0,
    ARROW_FORWARD  = 1,
    ARROW_BACKWARD = 2,
    ARROW_BOTH     = 3
};

enum TDashMode
{
    LINE_STYLE_SOLID    = 0,
    LINE_STYLE_DOT      = 1,
    LINE_STYLE_DASH     = 2,
    LINE_STYLE_DASH_DOT = 3
};

/// \brief  Indices for adressing colours with a special meaning
enum {
    EXPOSED_COLOUR = 0, ///< Index representing the colour used for exposed nodes and edges
    NO_COLOUR      = 1, ///< Colour index matching the node colour NoNode
    OUTLINE_COLOUR = 2, ///< Index representing the colour used for outlining
    GRID_COLOUR    = 3, ///< Index representing the colour used for grid lines and the bounding box
    VAGUE_COLOUR   = 4, ///< Index representing a grey colour used for unspecified node or edge colours
    ZERO_COLOUR    = 5, ///< Colour index matching the node and / or edge colour 0
    MAX_COLOUR     = 26 ///< First colour index exeeding the colour table
};

/// \brief  Graph display proxy class
///
/// This class translates between graph object and canvas items. That is, polyline
/// items corresponding to graph edges are supplied with line colour, width and
/// stipple attributes, and with edge labels. 
///
/// The display proxy object also supplies with the coordinate transformation rules
/// between the graph layout and the canvas sheet. Layout points can be moved in
/// terms of the canvas sheet.

class graphDisplayProxy : public managedObject
{
public:

    enum TNodeColourMode
    {
        NODES_UNCOLOURED         = 0,
        NODES_COLOUR_BY_DISTANCE = 1,
        NODES_FIXED_COLOURS      = 2,
        NODES_COLOUR_BY_DEMAND   = 3,
        NODES_FLOATING_COLOURS   = 4
    };

    enum TArcVisibilityMode
    {
        ARC_DISPLAY_HIDE_ALL     = 0,
        ARC_DISPLAY_SUBGRAPH     = 1,
        ARC_DISPLAY_PREDECESSORS = 2,
        ARC_DISPLAY_PRED_SUB     = 3,
        ARC_DISPLAY_SHOW_ALL     = 4
    };

    enum TArrowDisplayMode
    {
        ARROWS_ARC_ORIENTATION   = 0,
        ARROWS_OFF               = 1,
        ARROWS_IMPL_ORIENTATION  = 2,
        ARROWS_FLOW_DIRECTION    = 3,
        ARROWS_LIKE_PREDECESSORS = 4
    };

    enum TArcWidthMode
    {
        ARC_WIDTH_UNIFORM          = 0,
        ARC_WIDTH_PREDECESSORS     = 1,
        ARC_WIDTH_SUBGRAPH         = 2,
        ARC_WIDTH_FLOW_LINEAR      = 3,
        ARC_WIDTH_FLOW_LOGARITHMIC = 4,
        ARC_WIDTH_EMPTY_FREE_FULL  = 5
    };

    enum TArcStippleMode
    {
        ARC_STIPPLE_OFF          = 0,
        ARC_STIPPLE_PREDECESSORS = 1,
        ARC_STIPPLE_FREE         = 2,
        ARC_STIPPLE_FRACTIONAL   = 3,
        ARC_STIPPLE_COLOURS      = 4,
        ARC_STIPPLE_VOID         = 5,
        ARC_STIPPLE_SUBGRAPH     = 6
    };

    enum TArcColourMode
    {
        ARCS_UNCOLOURED       = 0,
        ARCS_FIXED_COLOURS    = 1,
        ARCS_REPEAT_COLOURS   = 2,
        ARCS_FLOATING_COLOURS = 3,
        ARCS_COLOUR_PRED      = 4
    };

private:

    abstractMixedGraph &G; ///< The graph object to be displayed
    goblinController CFG;  ///< The context of the displayed object

    const TFloat* piG; ///<

    /// The nominal canvas width [mm], achieved at a globalZoom of 1.0
    double canvasWidth;

    /// The nominal canvas height [mm], achieved at a globalZoom of 1.0
    double canvasHeight;

    double leftMargin;   ///< The left-hand margin of the object display, [fraction] of canvasWidth
    double rightMargin;  ///< The right-hand margin of the object display, [fraction] of canvasWidth
    double topMargin;    ///< The top margin of the object display, [fraction] of canvasHeight
    double bottomMargin; ///< The bottom margin of the object display, [fraction] of canvasHeight

    double min[3];
    double max[3];
    double offset[3];
    double originalRange[3];

    TDim ix; ///< The original coordiante index which maps to the x canvas coordinate
    TDim iy; ///< The original coordiante index which maps to the y canvas coordinate

    double globalZoom;  ///< Zoom factor which aplies to anything on the sheet
    double objectZoom;  ///< Internal zoom factor fitting the graph layout into its bounding box
    double pixelWidth;  ///< The effective display width of a canvas pixel [mm]
    double pixelHeight; ///< The effective display height of a canvas pixel [mm]

    enum {LABEL_BUFFER_SIZE = 256};

    /// The node label format string as copied from the graph layout attribute pool
    char nodeLabelFormat[LABEL_BUFFER_SIZE];

    /// The arc label format string as copied from the graph layout attribute pool
    char arcLabelFormat[LABEL_BUFFER_SIZE];

    /// The effective (minimal) display width of a graph node in the canvas [number of pixels]
    double nodeWidth;

    /// The effective (minimal) display height of a graph node in the canvas [number of pixels]
    double nodeHeight;

    /// The effective (minimal) display height of a graph node in the canvas [number of pixels]
    double arrowSize;

    double arcLabelSep;


    /// \brief Spacing between arc label and line display
    double fineSpacing;

    /// \brief Spacing between adjacent bend control points
    double bendSpacing;

    /// \brief Spacing between adjacent graph nodes
    double nodeSpacing;

    /// \brief Node size, as percents of the default value
    double nodeSizeRel;

    /// \brief Node font size, as percents of the default value
    double nodeFontSize;

    /// \brief Arrow size, as percents of the default value
    double arrowSizeRel;

    /// \brief Arc font size, as percents of the default value
    double arcFontSize;

    /// \brief Applied node shape mode
    TNodeShapeMode nodeShapeMode;

    /// \brief Applied arc display mode
    TArcVisibilityMode arcVisibilityMode;

    /// \brief Applied arrow display mode
    TArrowDisplayMode arrowDisplayMode;

    /// \brief Applied arc stipple mode
    TArcStippleMode arcStippleMode;

    /// \brief Applied arc width mode
    TArcWidthMode arcWidthMode;

    /// \brief Minimum arc width value
    int arcWidthMin;

    /// \brief Maximum arc width value
    int arcWidthMax;


public:

    /// \brief Applied node colour mode
    TNodeColourMode nodeColourMode;

    /// \brief Applied arc colour mode
    TArcColourMode arcColourMode;

    TArc maxNodeColour;
    TArc maxEdgeColour;


    graphDisplayProxy(abstractMixedGraph &GC,double _pixelWidth,double _pixelHeight)
        throw(ERRejected);
    virtual ~graphDisplayProxy() throw();

    unsigned long   Allocated() const throw();

    unsigned long   Size() const throw();


    /// \brief  Determine the coordinate transformation between core library and canvas
    void  DetermineCoordinateTransformation() throw();

    /// \brief  Extract the layout parameters from the attribute pool
    void  ExtractLayoutParameters() throw();

    /// \brief  Synchronize the current graph object values
    void  Synchronize() throw();


    long  RoundToLong(double val) const throw();

    /// \brief Return the nominal canvas width [mm], achieved at a globalZoom of 1.0
    long  CanvasWidth() throw();

    /// \brief Return the nominal canvas height [mm], achieved at a globalZoom of 1.0
    long  CanvasHeight() throw();

    /// \brief Translate a colour index to an RGB value from a fixed colour table
    ///
    /// \param c            A colour index to be translated
    /// \return             A hexadecimal 6 digit RGB value
    unsigned long  RGBFixedColour(TIndex c) throw();

    /// \brief Translate a colour index to an RGB value representing floating colours
    ///
    /// \param c            A colour index to be translated
    /// \param maxColour    The maximal representable colour index
    /// \return             A hexadecimal 6 digit RGB value
    unsigned long  RGBSmoothColour(TIndex c,TIndex maxColour) throw();


    /// \brief  Determine the canvas X coordinate value of a projected X coordinate
    ///
    /// \param cx  A projected X coordinate value
    /// \return    The corresponding coordinate canvas value
    long  CanvasCX(double cx) throw();

    /// \brief  Determine the canvas Y coordinate value of a projected Y coordinate
    ///
    /// \param cy  A projected Y coordinate value
    /// \return    The corresponding coordinate canvas value
    long  CanvasCY(double cy) throw();

    /// \brief  Determine the canvas X coordinate value of a layout point
    ///
    /// \param p  A layout point index ranged [0,1,..,n+ni-1]
    /// \return   The ordinate value of v in the canvas
    long  CanvasCXOfPoint(TNode p) throw(ERRange);

    /// \brief  Determine the canvas Y coordinate value of a layout point
    ///
    /// \param p  A layout point index ranged [0,1,..,n+ni-1]
    /// \return   The abscissa value of p in the canvas
    long  CanvasCYOfPoint(TNode p) throw(ERRange);

    /// \brief  Determine the canvas X coordinate value of an arc label anchor point
    ///
    /// \param a  An arc index ranged [0,1,..,2*m+1]
    /// \return   The ordinate value of the arc label of a in the canvas
    long  CanvasCXOfArcLabelAnchor(TArc a) throw(ERRange);

    /// \brief  Determine the canvas Y coordinate value of an arc label anchor point
    ///
    /// \param a  An arc index ranged [0,1,..,2*m-1]
    /// \return   The abscissa value of the arc label of a in the canvas
    long  CanvasCYOfArcLabelAnchor(TArc a) throw(ERRange);

    /// \brief  Determine the canvas X coordinate value of a virtual port node
    ///
    /// Arcs are not routed in the canvas between the center points of its two
    /// graph end nodes. Instead, two virtual port nodes are introduced which
    /// occur on the border line of the visualization of the end nodes. This
    /// procedure is necessary for the correct alignment of arrow heads, but
    /// works perfectly only if the graph nodes are visualized by circles.
    ///
    /// \param u  A layout point index ranged [0,1,..,n+ni-1]. This point is
    ///           considered to be the control point adjacent with v on the processed arc
    /// \param v  A graph node index ranged [0,1,..,n-1]
    /// \return   The ordinate value of the virtual port node in the canvas
    long  CanvasCXOfPort(TNode u,TNode v) throw(ERRange);

    long  CanvasCXOfPort(TArc a) throw(ERRange);

    /// \brief  Determine the canvas Y coordinate value of a virtual port node
    ///
    /// This is the analogue of PortCX() for the canvas abscissa
    ///
    /// \param u  A layout point index ranged [0,1,..,n+ni-1]. This point is
    ///           considered to be the control point adjacent with v on the processed arc
    /// \param v  A graph node index ranged [0,1,..,n-1]
    /// \return   The abscissa value of the virtual port node in the canvas
    long  CanvasCYOfPort(TNode u,TNode v) throw(ERRange);

    long  CanvasCYOfPort(TArc a) throw(ERRange);


    /// \brief  Determine the canvas width of a graph node
    ///
    /// \param v  A node index ranged [0,1,..,n-1] or NoNode
    /// \return   The canvas width of v
    long  CanvasNodeWidth(TNode v=NoNode) throw(ERRange);

    /// \brief  Determine the canvas height of a graph node
    ///
    /// \param v  A node index ranged [0,1,..,n-1] or NoNode
    /// \return   The canvas height of v
    long  CanvasNodeHeight(TNode v=NoNode) throw(ERRange);

    /// \brief  Compose the label of a virtual graph node displayed in the legenda
    ///
    /// \param buffer     A character buffer to store the compound label
    /// \param length     The length of the character buffer
    /// \param nodeLabel  The name of the displayed virtual node
    /// \return  The buffer address
    char*  NodeLegenda(char* buffer,size_t length,char* nodeLabel) throw();

    /// \brief  Compose a label for a given node
    ///
    /// \param buffer  A character buffer to store the label
    /// \param length  The length of the character buffer
    /// \param v       The index of the displayed node
    /// \return  The buffer address
    ///
    /// This applies the node format string in the object's layout pool. The
    /// <code>\#\<digit\></code>' symbols are expanded to basic labels by means of BasicNodeLabel().
    char*  CompoundNodeLabel(char* buffer,size_t length,TNode v) throw(ERRange);

    /// \brief  Retrieve a basic label for a given node
    ///
    /// \param buffer  A character buffer to store the node label
    /// \param length  The length of the character buffer
    /// \param v       The index of the node to be displayed
    /// \param mode    The node display mode
    /// \return  The buffer address
    ///
    /// This writes a numeric string to buffer, depending on the display mode:
    /// - 1: The node index ranged [0,1,..,n-1]
    /// - 2: A finite node distance label or <code>*</code>
    /// - 3: A finite node potential or <code>*</code>
    /// - 4: A finit node colour index ranged [0,1,..,n-1] or <code>*</code>
    /// - 5: The node demand label
    /// - 6: The node index ranged [1,2,..,n]
    char*  BasicNodeLabel(char* buffer,size_t length,TNode v,int mode) throw(ERRange);

    /// \brief  Determine the canvas font size of node labels
    ///
    /// \return  The font size
    long  NodeLabelFontSize() throw();

    /// \brief  Decide if a given node has to be displayed
    ///
    /// \param v      A node index ranged [0,1,..,n-1]
    /// \retval true  If this node ought to be displayed
    bool  IsNodeMapped(TNode v) throw(ERRange);

    TNodeShapeMode  NodeShapeMode(TNode v=NoNode) throw(ERRange);

    /// \brief  Determine the RGB pen colour of a given node
    ///
    /// \param buffer  An 8 byte character buffer
    /// \param v       A node index ranged [0,1,..,n-1]
    ///
    /// This assigns a <code>\#rrggbb</code> colour pattern
    void  CanvasNodeColour(char* buffer,TNode v) throw(ERRange);

    /// \brief  Determine the canvas pen colour of a given node
    ///
    /// \param v  A node index ranged [0,1,..,n-1]
    /// \return   A colour table index
    ///
    /// If the original colour index is NoNode, NO_COLOUR is returned.
    /// In the case of gradient colours, the returned index might exeed MAX_COLOUR.
    /// Otherwise, original colour indices beyond the colour table are replaced
    /// by a valid index in the colour table, for example VAGUE_COLOUR.
    TIndex  CanvasNodeColour(TNode v) throw(ERRange);

    /// \brief  Determine the canvas size of an arrow head
    ///
    /// \return  The canvas arrow size
    double  CanvasArrowSize() throw();

    /// \brief  Compose the label of the virtual edge displayed in the legenda
    ///
    /// \param buffer    A character buffer to store the compound label
    /// \param length    The length of the character buffer
    /// \param arcLabel  The name of the displayed virtual edge
    /// \return  The buffer address
    char*  ArcLegenda(char* buffer,size_t length,char* arcLabel) throw();

    /// \brief  Determine a label for a given edge
    char*  CompoundArcLabel(char* buffer,size_t length,TArc a) throw(ERRange);

    char*  BasicArcLabel(char* buffer,size_t length,TArc a,int mode) throw(ERRange);

    /// \brief  Determine the canvas font size of arc labels
    ///
    /// \return  The font size in canvas pixels
    long  ArcLabelFontSize() throw();

    /// \brief  Determine the displayed arrows of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*m-1]
    /// \return   The arrow display directions
    ///
    /// This respects the arc orientation bit!. An ARROW_FORWARD return value for the
    /// arc a matches an ARROW_BACKWARD return value for the reverse arc index (a^1)
    TArrowDir  ArrowDirections(TArc a) throw();

    /// \brief  Decide if a given arc has to be displayed at all
    ///
    /// \param a      An arc index ranged [0,1,..,2*m-1]
    /// \retval true  If this arc ought to be displayed
    bool  IsArcMapped(TArc a) throw(ERRange);

    /// \brief  Determine the RGB pen colour of a given arc
    ///
    /// \param buffer  An 8 byte character buffer
    /// \param a       An arc index ranged [0,1,..,2*m-1]
    ///
    /// This assigns a <code>\#rrggbb</code> colour pattern
    void  CanvasArcColour(char* buffer,TArc a) throw(ERRange);

    /// \brief  Determine the canvas pen colour of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*m-1]
    /// \return   A colour table index
    ///
    /// If the original colour index is NoArc, VAGUE_COLOUR is returned.
    /// In the case of gradient colours, the returned index might exeed MAX_COLOUR.
    /// Otherwise, original colour indices beyond the colour table are replaced
    /// by a valid index in the colour table, for example VAGUE_COLOUR.
    TIndex  CanvasArcColour(TArc a) throw(ERRange);

    /// \brief  Determine the canvas width of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*m-1]
    /// \return   The line thickness in canvas pixels
    long  CanvasArcWidth(TArc a) throw(ERRange);

    /// \brief  Determine the canvas dash mode of a given arc
    ///
    /// \param a  An arc index ranged [0,1,..,2*m-1]
    /// \return   The canvas dash mode
    TDashMode  CanvasArcDashMode(TArc a) throw(ERRange);

    /// \brief  Place a layout point at given canvas coordinates
    ///
    /// \param p   A node index ranged [0,1,..,n+ni-1]
    /// \param cx  The desired canvas horizontal position
    /// \param cy  The desired canvas vertical position
    void  PlaceLayoutPoint(TNode p,long cx,long cy) throw(ERRange);

    /// \brief  Place an arc label anchor point at given canvas coordinates
    ///
    /// \param a   An arc index ranged [0,1,..,2*m+1]
    /// \param cx  The desired canvas horizontal position
    /// \param cy  The desired canvas vertical position
    void  PlaceArcLabelAnchor(TArc a,long cx,long cy) throw(ERRange);

};


/// @}


inline long graphDisplayProxy::RoundToLong(double val) const throw()
{
    return ((val<0) ? long(val-0.5) : long(val+0.5));
}


inline long graphDisplayProxy::CanvasWidth() throw()
{
    return RoundToLong(canvasWidth*globalZoom/pixelWidth);
}


inline long graphDisplayProxy::CanvasHeight() throw()
{
    return RoundToLong(canvasHeight*globalZoom/pixelHeight);
}


inline long graphDisplayProxy::CanvasCX(double cx) throw()
{
    return RoundToLong((cx*objectZoom + offset[ix])*globalZoom/pixelWidth);
}


inline long graphDisplayProxy::CanvasCY(double cy) throw()
{
    return RoundToLong((cy*objectZoom + offset[iy])*globalZoom/pixelHeight);
}


inline long graphDisplayProxy::CanvasCXOfPoint(TNode v) throw(ERRange)
{
    return CanvasCX(G.C(v,ix));
}


inline long graphDisplayProxy::CanvasCYOfPoint(TNode v) throw(ERRange)
{
    return CanvasCY(G.C(v,iy));
}


inline double graphDisplayProxy::CanvasArrowSize() throw()
{
    return arrowSize;
}


inline TNodeShapeMode graphDisplayProxy::NodeShapeMode(TNode v) throw(ERRange)
{
    return nodeShapeMode;
}

#endif
