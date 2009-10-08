/*                  M A K E B U I L D I N G . C
 * BRL-CAD
 *
 * Copyright (c) 2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file makebuilding.c
 *
 * high level MakeBuilding functionality.
 *
 *
 */

#include "common.h"
#include "mkbuilding.h"


void
mkbldg_makeWallSegment(char *name, struct rt_wdb *db_fileptr, point_t bbp1, point_t bbp2)
{
    size_t nameLen = strlen(name);

    
    mkbldg_makeframe ("frame", db_fileptr, bbp1, bbp2, 1.5*25.4);
    
}

void
mkbldg_makeframe(char* name, struct rt_wdb *db_fileptr, point_t p1, point_t p2, int thickness)
{
    size_t nameLen;
    char *newName = "";
    size_t suffixLen;
    char *suffix = "";
    point_t bottomP2, topP1, leftP1, leftP2, rightP1, rightP2;
    
    nameLen = strlen(name);

// make the combo:
    struct wmember combo;
    struct bu_list *child_list = &combo.l;

    BU_LIST_INIT(child_list);

    /*
     * Bottom
     */

// build name
    suffix = "_bottom";
    suffixLen = strlen(suffix);

    newName = (char *)calloc(suffixLen + nameLen + 1, sizeof (char));
    strcat(newName, name);
    strcat(newName, suffix);

// calc points
    VSET(bottomP2, p2[0], p2[1], (p1[2] + thickness) );
    mk_rpp(db_fileptr, newName, p1, bottomP2);

//Add to child list.
    (void)mk_addmember(newName, child_list, NULL, WMOP_UNION);
 
    free(newName);


    /*
     * Top
     */

// build name
    suffix = "_top";
    suffixLen = strlen(suffix);

    newName = (char *)calloc(suffixLen + nameLen + 1, sizeof (char));
    strcat(newName, name);
    strcat(newName, suffix);

// calc points
    VSET(topP1, p1[0], p1[1], (p2[2] - thickness) );
    mk_rpp(db_fileptr, newName, topP1, p2);

//Add to child list.
    (void)mk_addmember(newName, child_list, NULL, WMOP_UNION);
 
    free(newName);


    /*
     * Left
     */

// build name
    suffix = "_left";
    suffixLen = strlen(suffix);

    newName = (char *)calloc(suffixLen + nameLen + 1, sizeof (char));
    strcat(newName, name);
    strcat(newName, suffix);

// calc points
    VSET(leftP1, p1[0], p1[1], (p1[2] + thickness) );
    VSET(leftP2, p2[0], (p1[1] + thickness), (p2[2] - thickness) );
    mk_rpp(db_fileptr, newName, leftP1, leftP2);

//Add to child list.
    (void)mk_addmember(newName, child_list, NULL, WMOP_UNION);
 
    free(newName);


    /*
     * Right
     */

// build name
    suffix = "_right";
    suffixLen = strlen(suffix);

    newName = (char *)calloc(suffixLen + nameLen + 1, sizeof (char));
    strcat(newName, name);
    strcat(newName, suffix);

// calc points

    VSET(rightP1, p1[0], (p2[1] - thickness), (p1[2] + thickness) );
    VSET(rightP2, p2[0], p2[1], (p2[2] - thickness) );
    mk_rpp(db_fileptr, newName, rightP1, rightP2);

//Add to child list.
    (void)mk_addmember(newName, child_list, NULL, WMOP_UNION);
 
    free(newName);


//make the combo
    unsigned char rgb[3];
    VSET(rgb, 64, 180, 96);
    mk_lcomb(db_fileptr, name, &combo, 1, NULL, NULL, rgb, 0);

    
}


void
mkbldg_make2x6(char *name, struct rt_wdb *db_filepointer, int length)
{
    point_t p1, p2;

    VSET(p1, 0.0, 0.0, 0.0);
    VSET(p2, (25.4*5.5), (25.4*1.5), length);

    mk_rpp(db_filepointer, name, p1, p2);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
