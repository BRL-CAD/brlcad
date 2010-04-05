
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2006
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file poolTables.cpp
/// \brief All parse tables for attribute pools
///
/// The tables in this file are used to parse graph definition files.
/// Attribute pools also need the tables to reconstruct the types
/// of specific attributes, and to maintain consistent value ranges
/// when the dimensions change

#include "globals.h"


/// \brief Top-level description of a native graph definition file
///
/// This table is not assigned with a special attribute pool, but serves as
/// the entry point for the graph definition parser. The "objectives" section
/// of earlier library versions has been merged here.
const TPoolTable listOfGraphPars[TokGraphEndSection] =
{
    {   "definition",   TYPE_SPECIAL,       DIM_SINGLETON,      TokGraphRepresentation  },
    {   "objectives",   TYPE_SPECIAL,       DIM_SINGLETON,      TokGraphObjectives      },
    {   "geometry",     TYPE_SPECIAL,       DIM_SINGLETON,      TokGraphGeometry        },
    {   "layout",       TYPE_SPECIAL,       DIM_SINGLETON,      TokGraphLayout          },
    {   "solutions",    TYPE_SPECIAL,       DIM_SINGLETON,      TokGraphRegisters       },
    {   "configure",    TYPE_SPECIAL,       DIM_SINGLETON,      TokGraphConfigure       },
    {   "commodities",  TYPE_INT,           DIM_SINGLETON,      TokGraphCommodities     },
    {   "bound",        TYPE_FLOAT_VALUE,   DIM_SINGLETON,      TokGraphBound           },
    {   "length",       TYPE_SPECIAL,       DIM_SINGLETON,      TokGraphLength          }
};


/// \brief The table assigned with the #graphRepresentation::representation pool
const TPoolTable listOfReprPars[TokReprEndSection] =
{
    {   "ucap",         TYPE_CAP_VALUE,     DIM_GRAPH_ARCS,     TokReprUCap             },
    {   "lcap",         TYPE_CAP_VALUE,     DIM_GRAPH_ARCS,     TokReprLCap             },
    {   "length",       TYPE_FLOAT_VALUE,   DIM_GRAPH_ARCS,     TokReprLength           },
    {   "demand",       TYPE_CAP_VALUE,     DIM_GRAPH_NODES,    TokReprDemand           },
    {   "directed",     TYPE_ORIENTATION,   DIM_GRAPH_ARCS,     TokReprOrientation      },
    {   "first",        TYPE_SPECIAL,       DIM_GRAPH_NODES,    TokReprFirst            },
    {   "right",        TYPE_SPECIAL,       DIM_ARCS_TWICE,     TokReprRight            },
    {   "startNode",    TYPE_SPECIAL,       DIM_ARCS_TWICE,     TokReprStartNode        },
    {   "incidences",   TYPE_SPECIAL,       DIM_SINGLETON,      TokReprIncidences       },
    {   "nodes",        TYPE_SPECIAL,       DIM_SINGLETON,      TokReprNNodes           },
    {   "arcs",         TYPE_SPECIAL,       DIM_SINGLETON,      TokReprNArcs            },
    {   "comm0",        TYPE_FLOAT_VALUE,   DIM_GRAPH_ARCS,     TokReprLength           }
};


/// \brief The table assigned with the #graphRepresentation::geometry pool
const TPoolTable listOfGeometryPars[TokGeoEndSection] =
{
    {   "dim",          TYPE_INT,           DIM_SINGLETON,      TokGeoDim               },
    {   "metrics",      TYPE_INT,           DIM_SINGLETON,      TokGeoMetric            },
    {   "coordinates",  TYPE_SPECIAL,       DIM_SINGLETON,      TokGeoCoordinates       },
    {   "axis0",        TYPE_FLOAT_VALUE,   DIM_LAYOUT_NODES,   TokGeoAxis0             },
    {   "axis1",        TYPE_FLOAT_VALUE,   DIM_LAYOUT_NODES,   TokGeoAxis1             },
    {   "axis2",        TYPE_FLOAT_VALUE,   DIM_LAYOUT_NODES,   TokGeoAxis2             },
    {   "minBound",     TYPE_NODE_INDEX,    DIM_SINGLETON,      TokGeoMinBound          },
    {   "maxBound",     TYPE_NODE_INDEX,    DIM_SINGLETON,      TokGeoMaxBound          }
};


/// \brief The table assigned with the #graphRepresentation::layoutData pool
const TPoolTable listOfLayoutPars[TokLayoutEndSection] =
{
    {   "model",            TYPE_INT,         DIM_SINGLETON,      TokLayoutModel            },
    {   "style",            TYPE_INT,         DIM_SINGLETON,      TokLayoutModel            },
    {   "align",            TYPE_NODE_INDEX,  DIM_GRAPH_ARCS,     TokLayoutArcLabel         },
    {   "thread",           TYPE_NODE_INDEX,  DIM_LAYOUT_NODES,   TokLayoutThread           },
    {   "interpolate",      TYPE_NODE_INDEX,  DIM_LAYOUT_NODES,   TokLayoutThread           },
    {   "hiddenNode",       TYPE_BOOL,        DIM_GRAPH_NODES,    TokLayoutHiddenNode       },
    {   "hiddenArc",        TYPE_BOOL,        DIM_GRAPH_ARCS,     TokLayoutHiddenArc        },
    {   "exteriorArc",      TYPE_ARC_INDEX,   DIM_SINGLETON,      TokLayoutExteriorArc      },
    {   "arcWidthMode",     TYPE_INT,         DIM_SINGLETON,      TokLayoutArcWidthMode     },
    {   "arcWidthMin",      TYPE_INT,         DIM_SINGLETON,      TokLayoutArcWidthMin      },
    {   "arcWidthMax",      TYPE_INT,         DIM_SINGLETON,      TokLayoutArcWidthMax      },
    {   "arcStippleMode",   TYPE_INT,         DIM_SINGLETON,      TokLayoutArcStippleMode   },
    {   "arcVisibilityMode", TYPE_INT,        DIM_SINGLETON,      TokLayoutArcVisibilityMode },
    {   "arcShapeMode",     TYPE_INT,         DIM_SINGLETON,      TokLayoutArcShapeMode     },
    {   "arcColourMode",    TYPE_INT,         DIM_SINGLETON,      TokLayoutArcColourMode    },
    {   "arrowDisplayMode", TYPE_INT,         DIM_SINGLETON,      TokLayoutArrowDisplayMode },
    {   "arrowPosMode",     TYPE_INT,         DIM_SINGLETON,      TokLayoutArrowPosMode     },
    {   "arrowSize",        TYPE_DOUBLE,      DIM_SINGLETON,      TokLayoutArrowSize        },
    {   "nodeShapeMode",    TYPE_INT,         DIM_SINGLETON,      TokLayoutNodeShapeMode    },
    {   "nodeColourMode",   TYPE_INT,         DIM_SINGLETON,      TokLayoutNodeColourMode   },
    {   "nodeSize",         TYPE_DOUBLE,      DIM_SINGLETON,      TokLayoutNodeSize         },
    {   "nodeFontSize",     TYPE_DOUBLE,      DIM_SINGLETON,      TokLayoutNodeFontSize     },
    {   "arcFontSize",      TYPE_DOUBLE,      DIM_SINGLETON,      TokLayoutArcFontSize      },
    {   "nodeFontType",     TYPE_INT,         DIM_SINGLETON,      TokLayoutNodeFontType     },
    {   "arcFontType",      TYPE_INT,         DIM_SINGLETON,      TokLayoutArcFontType      },
    {   "gridDisplayMode",  TYPE_INT,         DIM_SINGLETON,      TokLayoutGridDisplayMode  },
    {   "fineSpacing",      TYPE_DOUBLE,      DIM_SINGLETON,      TokLayoutFineSpacing      },
    {   "bendSpacing",      TYPE_DOUBLE,      DIM_SINGLETON,      TokLayoutBendSpacing      },
    {   "nodeSpacing",      TYPE_DOUBLE,      DIM_SINGLETON,      TokLayoutNodeSpacing      },
    {   "nodeLabelFormat",  TYPE_CHAR,        DIM_STRING,         TokLayoutNodeLabelFormat  },
    {   "arcLabelFormat",   TYPE_CHAR,        DIM_STRING,         TokLayoutArcLabelFormat   }
};


/// \brief The table assigned with the #abstractMixedGraph::registers pool
const TPoolTable listOfRegisters[TokRegEndSection] =
{
    {   "label",        TYPE_FLOAT_VALUE,   DIM_GRAPH_NODES,    TokRegLabel             },
    {   "predecessor",  TYPE_ARC_INDEX,     DIM_GRAPH_NODES,    TokRegPredecessor       },
    {   "subgraph",     TYPE_SPECIAL,       DIM_GRAPH_ARCS,     TokRegSubgraph          },
    {   "potential",    TYPE_FLOAT_VALUE,   DIM_GRAPH_NODES,    TokRegPotential         },
    {   "nodeColour",   TYPE_NODE_INDEX,    DIM_GRAPH_NODES,    TokRegNodeColour        },
    {   "edgeColour",   TYPE_ARC_INDEX,     DIM_GRAPH_ARCS,     TokRegEdgeColour        },
    {   "colour",       TYPE_NODE_INDEX,    DIM_GRAPH_NODES,    TokRegNodeColour        },
    {   "originalNode", TYPE_INDEX,         DIM_GRAPH_NODES,    TokRegOriginalNode      },
    {   "originalArc",  TYPE_INDEX,         DIM_GRAPH_ARCS,     TokRegOriginalArc       },

    {"V_T",          TYPE_BOOL,         DIM_GRAPH_NODES,    TokV_T},
    {"EdgeType",     TYPE_INT,          DIM_ARCS_TWICE,     TokEdgeType},
    {"PreO",         TYPE_NODE_INDEX,   DIM_GRAPH_NODES,    TokPreO},
    {"PostO",        TYPE_NODE_INDEX,   DIM_GRAPH_NODES,    TokPostO},
    {"Low1",         TYPE_NODE_INDEX,   DIM_GRAPH_ARCS,     TokLow1},
    {"Low2",         TYPE_NODE_INDEX,   DIM_GRAPH_ARCS,     TokLow2}

};

