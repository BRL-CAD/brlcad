
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   listOfAuthors.cpp
/// \brief  Lists the code authors

#include "globals.h"


const TAuthorStruct listOfAuthors[NoAuthor] =
{
    // AuthCFP

    {
        "Christian Fremuth-Paeger",     // name
        "",                             // affiliation
        "fremuth@math.uni-augsburg.de"  // e-mail
    },


    // AuthBSchmidt

    {
        "Bernhard Schmidt",             // name
        "Department of Mathematics\nUniversity of Augsburg",
                                        // affiliation
        "bschmidt@uni-augsburg.de"      // e-mail
    },


    // AuthMSchwank

    {
        "Markus Schwank",               // name
        "",                             // affiliation
        ""                              // e-mail
    },


    // AuthBEisermann

    {
        "Birk Eisermann",               // name
        "Department of Mathematics\nUniversity of Augsburg",
                                        // affiliation
        "birk.eisermann@web.de"         // e-mail
    },


    // AuthAMakhorin

    {
        "Andrew Makhorin",              // name
        "Department for Applied Informatics\nMoscow Aviation Institute\nMoscow, Russia",
                                        // affiliation
        "mao@mai2.rcnet.ru"             // e-mail
    }
};
