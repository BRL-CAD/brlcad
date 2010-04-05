
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   listOfTimers.cpp
/// \brief  Lists the properties of all timer objects

#include "globals.h"


const TTimerStruct listOfTimers[NoTimer] =
{
    // TimerSolve

    {
        "Thread Execution",  // Timer name
        true                 // Basic timer
    },


    // TimerIO

    {
        "File I/O",         // Timer name
        false               // Basic timer
    },


    // TimerUnionFind

    {
        "Union / Find",     // Timer name
        false               // Basic timer
    },


    // TimerHash

    {
        "Hash Tables",      // Timer name
        false               // Basic timer
    },


    // TimerPrioQ

    {
        "Priority Queues",  // Timer name
        false               // Basic timer
    },


    // TimerPricing

    {
        "Pricing",          // Timer name
        true                // Basic timer
    },


    // TimerQTest

    {
        "Quotient Tests",   // Timer name
        true                // Basic timer
    },


    // TimerPivoting

    {
        "Pivoting",         // Timer name
        true                // Basic timer
    },


    // TimerSPTree

    {
        "Shortest Paths",   // Timer name
        true                // Full featured timer
    },


    // TimerMinTree

    {
        "Spanning Trees",   // Timer name
        true                // Full featured timer
    },


    // TimerComponents

    {
        "Components",       // Timer name
        true                // Full featured timer
    },


    // TimerMaxFlow

    {
        "Maximum Flows",    // Timer name
        true                // Full featured timer
    },


    // TimerMinCut

    {
        "Minimum Cuts",     // Timer name
        true                // Full featured timer
    },


    // TimerMinCFlow

    {
        "Min-Cost Flows",   // Timer name
        true                // Full featured timer
    },


    // TimerMatching

    {
        "Matching",         // Timer name
        true                // Full featured timer
    },


    // TimerBranch

    {
        "Branch and Bound",   // Timer name
        true                // Full featured timer
    },


    // TimerLpSolve

    {
        "LP Solver",        // Timer name
        true                // Full featured timer
    },


    // TimerTsp

    {
        "TSP",              // Timer name
        true                // Full featured timer
    },


    // TimerColour

    {
        "Colouring",        // Timer name
        true                // Full featured timer
    },


    // TimerSteiner

    {
        "Steiner Trees",    // Timer name
        true                // Full featured timer
    },


    // TimerStable

    {
        "Stable Sets",      // Timer name
        true                // Full featured timer
    },


    // TimerMaxCut

    {
        "Maximum Cuts",     // Timer name
        true                // Full featured timer
    },


    // TimerTreePack

    {
        "Tree Packings",    // Timer name
        true                // Full featured timer
    },


    // TimerTJoin

    {
        "T-Joins",          // Timer name
        true                // Full featured timer
    },


    // TimerCycleCancel

    {
        "Cycle Canceling",  // Timer name
        false               // Full featured timer
    },


    // TimerMaxBalFlow

    {
        "Balanced Flows",   // Timer name
        true                // Full featured timer
    },


    // TimerPrimalDual

    {
        "Primal-Dual",      // Timer name
        true                // Full featured timer
    },


    // TimerPlanarity

    {
        "Planarity",        // Timer name
        true                // Full featured timer
    },


    // TimerDrawing

    {
        "Drawing",          // Timer name
        true                // Full featured timer
    },


    // TimerMIP

    {
        "Mixed Integer",    // Timer name
        true                // Full featured timer
    },


    // TimerBNS

    {
        "Balanced Search",  // Timer name
        true                // Full featured timer
    },


    // TimerLocalSearch

    {
        "Local Search",     // Timer name
        true                // Full featured timer
    },


    // TimerHeuristics

    {
        "Construction Heuristics",
                            // Timer name
        true                // Full featured timer
    },


    // TimerStrongConn

    {
        "Strong Connectivity",
                            // Timer name
        true                // Full featured timer
    }
};
