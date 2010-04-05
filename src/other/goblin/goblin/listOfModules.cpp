
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   listOfModules.cpp
/// \brief  Lists the properties of functional source code modules

#include "globals.h"


const TModuleStruct listOfModules[NoModule] =
{
    // ModRoot

    {
        "Root module",     // Module name
        TimerSolve,         // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModLpSolve

    {
        "Simplex Code",     // Module name
        TimerLpSolve,       // Timer index
        AuthBSchmidt,       // First implementor
        AuthCFP,            // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModColour

    {
        "Node Colours",     // Module name
        TimerColour,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "November 2000",    // Date of encoding
        "February 2005",    // Latest revision
        RefBre79,           // Original publication
        RefGPR96,           // Authors reference
        NoReference         // Text book reference
    },


    // ModStable

    {
        "Stable Sets",      // Module name
        TimerStable,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "May 2001",         // Date of encoding
        "February 2005",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModBranch

    {
        "Branch and Bound", // Module name
        TimerBranch,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "February 2001",    // Date of encoding
        "January 2005",     // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModTSP

    {
        "TSP",              // Module name
        TimerTsp,           // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModSteiner

    {
        "Steiner Trees",    // Module name
        TimerSteiner,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "July 2005",        // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMaxCut

    {
        "Maximum Edge Cuts",// Module name
        TimerMaxCut,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "December 2004",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMinTree

    {
        "Spanning Trees",   // Module name
        TimerMinTree,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2005",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMaxFlow

    {
        "Maximum Flows",    // Module name
        TimerMaxFlow,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMinCFlow

    {
        "Min-Cost Flows",   // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModComponents

    {
        "Connected Components",
                            // Module name
        TimerComponents,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "November 2007",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMinCut

    {
        "Minimum Cuts",     // Module name
        TimerMinCut,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "November 2007",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModSPTree

    {
        "Shortest Paths",   // Module name
        TimerSPTree,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModTreePack

    {
        "Tree Packing",     // Module name
        TimerTreePack,      // Timer index
        AuthMSchwank,       // First implementor
        NoAuthor,           // Second implementor
        "November 2001",    // Date of encoding
        "September 2007",   // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMatching

    {
        "Matching",         // Module name
        TimerMatching,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "January 2005",     // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModTJoin

    {
        "T-Joins",          // Module name
        TimerTJoin,         // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefCPCS98           // Text book reference
    },


    // ModPostman

    {
        "Chinese Postman",  // Module name
        TimerTJoin,         // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefEdJo73,          // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModLpPricing

    {
        "LP Pricing",       // Module name
        TimerPricing,       // Timer index
        AuthBSchmidt,       // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModLpQTest

    {
        "LP Quotient Test", // Module name
        TimerQTest,         // Timer index
        AuthBSchmidt,       // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModLpPivoting

    {
        "LP Pivoting",      // Module name
        TimerPivoting,      // Timer index
        AuthBSchmidt,       // First implementor
        AuthCFP,            // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModCycleCancel

    {
        "Fractional Arc Canceling",
                            // Module name
        TimerCycleCancel,   // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMaxBalFlow

    {
        "Maximum Balanced Flows",
                            // Module name
        TimerMaxBalFlow,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMinCBalFlow

    {
        "Min-cost Balanced Flows",
                            // Module name
        TimerPrimalDual,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModPlanarity

    {
        "Planar Embedding",
                            // Module name
        TimerPlanarity,     // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "December 2003",    // Date of encoding
        "December 2006",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModGLPK

    {
        "GNU Linear Programming Kit",
                            // Module name
        TimerLpSolve,       // Timer index
        AuthAMakhorin,      // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModGraphLoad

    {
        "Native Graph Loader",
                            // Module name
        TimerIO,            // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModEdmondsKarp

    {
        "Shortest Path Method",
                            // Module name
        TimerMaxFlow,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefEdKa72,          // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModDinic

    {
        "Dinic Method",
                            // Module name
        TimerMaxFlow,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefDin70,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModPushRelabel

    {
        "Push/Relabel Method",
                            // Module name
        TimerMaxFlow,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefGoTa88,          // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModPrim

    {
        "Prim Method",
                            // Module name
        TimerMinTree,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefPrim57,          // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModPrim2

    {
        "Enhanced Prim Method",
                            // Module name
        TimerMinTree,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefAMO93,           // Original publication
        NoReference,        // Authors reference
        RefAMO93            // Text book reference
    },


    // ModKruskal

    {
        "Kruskal Method",
                            // Module name
        TimerMinTree,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefKru56,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModEdmondsArb

    {
        "Edmonds Method",
                            // Module name
        TimerMinTree,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "April 2006",       // Latest revision
        RefEdm67,           // Original publication
        NoReference,        // Authors reference
        RefGoMi84           // Text book reference
    },


    // ModPlanarityDMP

    {
        "DMP Planarity Method",
                            // Module name
        TimerPlanarity,     // Timer index
        AuthBEisermann,     // First implementor
        NoAuthor,           // Second implementor
        "August 2003",      // Date of encoding
        "September 2005",   // Latest revision
        RefDMP64,           // Original publication
        NoReference,        // Authors reference
        RefGou88            // Text book reference
    },


    // ModDijkstra

    {
        "Dijkstra Shortest Path Tree",
                            // Module name
        TimerSPTree,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefDij59,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModBellmanFord

    {
        "Negative Cycle Detection",
                            // Module name
        TimerSPTree,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefBel58,           // Original publication
        NoReference,        // Authors reference
        RefAMO93            // Text book reference
    },


    // ModFloydWarshall

    {
        "All Pair Shortest Path",
                            // Module name
        TimerSPTree,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefFlo62,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModFIFOLabelCorrect

    {
        "FIFO Negative Cycle",
                            // Module name
        TimerSPTree,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefAMO93            // Text book reference
    },


    // ModKarpMeanCycle

    {
        "Minimum Mean Cycle",
                            // Module name
        TimerSPTree,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefKar78,           // Original publication
        NoReference,        // Authors reference
        RefAMO93            // Text book reference
    },


    // ModKleinCanceling

    {
        "Klein Cycle Canceling",
                            // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefKle67,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModShortPath2

    {
        "Shortest Path Method",
                            // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefEdKa72,          // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModMeanCycleCanceling

    {
        "Mean Cycle Canceling",
                            // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefGoTa89,          // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModNetworkSimplex

    {
        "Network Simplex Method",
                            // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefAMO93            // Text book reference
    },


    // ModCostScaling

    {
        "Cost Scaling Method",
                            // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefGoTa90,          // Original publication
        NoReference,        // Authors reference
        RefAMO93            // Text book reference
    },


    // ModBusackerGowen

    {
        "Busacker/Gowen Method",
                            // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefBuGo61,          // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModSubgradOptTSP

    {
        "1-Tree Subgradient Method",
                            // Module name
        TimerLocalSearch,   // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefHeKa70,          // Original publication
        NoReference,        // Authors reference
        RefRei94            // Text book reference
    },


    // ModTreeApproxTSP

    {
        "TSP Tree Approximation",
                            // Module name
        TimerHeuristics,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModChristofides

    {
        "Christofides Approximation",
                            // Module name
        TimerHeuristics,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefChr76,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // Mod2OptTSP

    {
        "2-Opt Local Search",
                            // Module name
        TimerLocalSearch,   // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefCro58,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModNodeInsert

    {
        "Node Insertion",
                            // Module name
        TimerLocalSearch,   // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefRei94            // Text book reference
    },


    // ModRandomTour

    {
        "Random Tour",
                            // Module name
        TimerHeuristics,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModFarthestInsert

    {
        "Farthest Insertion",
                            // Module name
        TimerHeuristics,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModTreeApproxSteiner

    {
        "Steiner Tree Approximation",
                            // Module name
        TimerSteiner,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefPrSt02           // Text book reference
    },


    // ModStrongComponents

    {
        "Strong Components",// Module name
        TimerComponents,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefAHU83,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModBiconnectivity

    {
        "Biconnectivity",   // Module name
        TimerComponents,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefTar72,           // Original publication
        NoReference,        // Authors reference
        RefGou88            // Text book reference
    },


    // ModSchnorr

    {
        "Schnorr Method",
                            // Module name
        TimerStrongConn,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "November 2007",    // Latest revision
        RefSch79,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModHaoOrlin

    {
        "Push/Relabel Method",
                            // Module name
        TimerStrongConn,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefHaOr94,          // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModNodeIdentification

    {
        "Node Identification Method",
                            // Module name
        TimerMinCut,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefPaRi90,          // Original publication
        NoReference,        // Authors reference
        RefCPCS98           // Text book reference
    },


    // ModHierholzer

    {
        "Hierholzer Method",
                            // Module name
        TimerTJoin,         // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "February 2004",    // Date of encoding
        "",                 // Latest revision
        RefHie1873,         // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModDAGSearch

    {
        "DAG Search",
                            // Module name
        TimerSPTree,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefBel58,           // Original publication
        NoReference,        // Authors reference
        RefJun97            // Text book reference
    },


    // ModPrimalDual

    {
        "Primal-Dual Matching",
                            // Module name
        TimerPrimalDual,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefGoKa96,          // Original publication
        RefFrJu02,          // Authors reference
        NoReference         // Text book reference
    },


    // ModAnstee

    {
        "Cycle Canceling",  // Module name
        TimerPrimalDual,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefAns85,           // Original publication
        RefFrJu01b,         // Authors reference
        NoReference         // Text book reference
    },


    // ModMicaliVazirani

    {
        "Blocking Flows",   // Module name
        TimerMaxBalFlow,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefMiVa80,          // Original publication
        RefFrJu99b,         // Authors reference
        NoReference         // Text book reference
    },


    // ModBalAugment

    {
        "Successive Augmentation",
                            // Module name
        TimerMaxBalFlow,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefKoSt95,          // Original publication
        RefFrJu99a,         // Authors reference
        NoReference         // Text book reference
    },


    // ModBalScaling

    {
        "Capacity Scaling Method",
                            // Module name
        TimerMaxBalFlow,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        NoReference,        // Original publication
        RefFrJu01a,         // Authors reference
        NoReference         // Text book reference
    },


    // ModBNSExact

    {
        "Kocay/Stone Method",
                            // Module name
        TimerBNS,           // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefKoSt95,          // Original publication
        RefFrJu99a,         // Authors reference
        NoReference         // Text book reference
    },


    // ModBNSDepth

    {
        "Depth First Heuristics",
                            // Module name
        TimerBNS,           // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefKaMu74,          // Original publication
        RefFrJu99a,         // Authors reference
        NoReference         // Text book reference
    },


    // ModBNSBreadth

    {
        "Breadth First Heuristics",
                            // Module name
        TimerBNS,           // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        RefPaCo80,          // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModEnhancedPD

    {
        "Enhanced Primal-Dual",
                            // Module name
        TimerPrimalDual,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefAns87,           // Original publication
        RefFrJu02,          // Authors reference
        NoReference         // Text book reference
    },


    // ModBNS

    {
        "Balanced Network Search",
                            // Module name
        TimerMaxBalFlow,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "February 2009",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMaxCutTJoin

    {
        "Dual T-Join Method",
                            // Module name
        TimerHeuristics,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "February 2004",    // Date of encoding
        "",                 // Latest revision
        RefOrDo72,          // Original publication
        NoReference,        // Authors reference
        RefSch03            // Text book reference
    },


    // ModMaxCutGRASP

    {
        "GRASP Heuristic",
                            // Module name
        TimerHeuristics,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "March 2004",       // Date of encoding
        "",                 // Latest revision
        RefFPRR02,          // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModLMCOrder

    {
        "LMC Ordered Partition",
                            // Module name
        TimerPlanarity,     // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "July 2004",        // Date of encoding
        "May 2007",         // Latest revision
        RefKan96,           // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModConvexDrawing

    {
        "Plane Convex Drawing",
                            // Module name
        TimerPlanarity,     // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "July 2004",        // Date of encoding
        "",                 // Latest revision
        RefKan96,           // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModSpringEmbedder

    {
        "Spring Embedder",  // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "",                 // Latest revision
        RefEad84,           // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModGEM

    {
        "GEM Drawing",      // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        AuthBSchmidt,       // Second implementor
        "August 2004",      // Date of encoding
        "",                 // Latest revision
        RefFLM94,           // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModDakin

    {
        "MIP Branch & Bound",
                            // Module name
        TimerMIP,           // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "January 2005",     // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModMCFCapScaling

    {
        "Capacity Scaling", // Module name
        TimerMinCFlow,      // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "July 2005",        // Date of encoding
        "February 2009",    // Latest revision
        RefEdKa72,          // Original publication
        NoReference,        // Authors reference
        RefAMO93            // Text book reference
    },


    // ModKandinsky

    {
        "Kandinsky Drawing",// Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "September 2005",   // Date of encoding
        "November 2006",    // Latest revision
        RefBMT97,           // Original publication
        NoReference,        // Authors reference
        RefKaWa01           // Text book reference
    },


    // Mod4Orthogonal

    {
        "4-Orthogonal Drawing",// Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "October 2005",     // Date of encoding
        "August 2009",      // Latest revision
        RefBiKa94,          // Original publication
        NoReference,        // Authors reference
        RefKaWa01           // Text book reference
    },


    // ModVisibilityRepr

    {
        "Visibility Representation",
                            // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "October 2005",     // Date of encoding
        "",                 // Latest revision
        RefRoTa86,          // Original publication
        NoReference,        // Authors reference
        RefKan93            // Text book reference
    },


    // ModLayeredFDP

    {
        "Layered FDP",      // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "April 2006",       // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModFeedbackArcSet

    {
        "Feedback Arc Set", // Module name
        TimerMaxCut,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "May 2006",         // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefKaWa01           // Text book reference
    },


    // ModLayering

    {
        "Layer Assignment", // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "May 2006",         // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        RefKaWa01           // Text book reference
    },


    // ModSeriesParallel

    {
        "Series Parallel Embedding", // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "June 2006",        // Date of encoding
        "May 2007",         // Latest revision
        RefVTL82,           // Original publication
        NoReference,        // Authors reference
        RefKaWa01           // Text book reference
    },


    // ModStaircase

    {
        "Planar Kandinsky Drawing", // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "October 2006",     // Date of encoding
        "",                 // Latest revision
        RefFKK96,           // Original publication
        NoReference,        // Authors reference
        RefKaWa01           // Text book reference
    },


    // ModPlanarityHoTa

    {
        "Hopcroft/Tarjan Planarity Method",
                            // Module name
        TimerPlanarity,     // Timer index
        AuthBEisermann,     // First implementor
        NoAuthor,           // Second implementor
        "November 2006",    // Date of encoding
        "",                 // Latest revision
        RefHoTa74,          // Original publication
        RefEis06,           // Authors reference
        RefGou88            // Text book reference
    },


    // ModStrongConn

    {
        "Strong Connectivity",
                            // Module name
        TimerStrongConn,    // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "November 2007",    // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModForceDirected

    {
        "Force Directed Placement",
                            // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "",                 // Date of encoding
        "April 2008",       // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    },


    // ModChordality

    {
        "Chordal Graph Methods",
                            // Module name
        TimerStable,        // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "January 2009",     // Date of encoding
        "",                 // Latest revision
        RefHCPV00,          // Original publication
        NoReference,        // Authors reference
        RefSch03            // Text book reference
    },


    // ModOrthoFlowRed

    {
        "4-Orthogonal Flow Reduction",
                            // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "August 2009",      // Date of encoding
        "",                 // Latest revision
        RefDaKu87,          // Original publication
        NoReference,        // Authors reference
        RefKaWa01           // Text book reference
    },


    // ModOrthoFlowRed

    {
        "4-Orthogonal Compaction",
                            // Module name
        TimerDrawing,       // Timer index
        AuthCFP,            // First implementor
        NoAuthor,           // Second implementor
        "August 2009",      // Date of encoding
        "",                 // Latest revision
        NoReference,        // Original publication
        NoReference,        // Authors reference
        NoReference         // Text book reference
    }
};
