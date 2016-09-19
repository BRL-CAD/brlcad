/*                     D Y N - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2010-2016 United States Government as represented by
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
/**
 *
 * This is a program to convert the geometric elements of an LS Dyna
 * keyword file to a BRL-CAD database file.
 *
 * Usage: dyn-g input.key output.g
 */
#include "common.h"

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <set>

#include "vmath.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/sort.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

struct dyna_node {
    long NID;
    point_t pnt;
};

struct dyna_element_shell {
    long EID;
    long PID;
    long nodal_pnts[4];
};

struct dyna_world {
    struct bu_ptbl *nodes;
    struct bu_ptbl *element_shells;
};

/* See if keyword matches pattern ^keyword* */
size_t
keyword_match(const char *key, std::string line)
{
    if (line.find(key, 0) != std::string::npos && !line.find(key, 0)) return 1;
    return 0;
}

/* Scrub spaces - this may not be necessary... */
/*
void
scrub_column(std::string &col)
{
    size_t startpos = col.find_first_not_of(" \t");
    if (std::string::npos != startpos) {
	col = col.substr(startpos);
    }
    size_t endpos = col.find_last_not_of(" \t");
    if (std::string::npos != endpos) {
	col = col.substr(0 ,endpos+1);
    }
}
*/

void
process_nodes(std::ifstream &infile, int offset, struct dyna_world *world)
{
    std::string line;
    int end_nodes = 0;
    infile.clear();
    infile.seekg(offset);
    while (std::getline(infile, line) && !end_nodes) {
	if (line.c_str()[0] == '*') {
	    end_nodes = 1;
	    break;
	}
	int i = 0;
	int point_ind = 0;
	struct dyna_node *node;
	BU_GET(node, struct dyna_node);
	while (i < 80 && i < (int)strlen(line.c_str())) {
	    int incr = (i < 7 || i > 55) ? 8 : 16;
	    std::string col = line.substr(i,incr);
	    //printf("col(%d): %s\n", i, col.c_str());
	    if (i < 7) {
		// node id
		char *endptr;
		long nodeid = strtol(col.c_str(), &endptr, 10);
		if (endptr == col.c_str()) {
		    bu_log("Error: string to long didn't work: %s\n", col.c_str());
		    node->NID = -1;
		} else {
		    node->NID = nodeid;
		}
	    }
	    if (incr == 16) {
		char *endptr;
		double doubleval = strtod(col.c_str(), &endptr);
		if (endptr == col.c_str()) {
		    bu_log("Error: string to double didn't work: %s\n", col.c_str());
		    node->pnt[point_ind] = 0.0;
		} else {
		    node->pnt[point_ind] = doubleval;
		}
		point_ind++;
	    }
	    i = i + incr;
	}
	bu_ptbl_ins(world->nodes, (long *)node);
    }
}


void
process_element_shell(std::ifstream &infile, int offset, struct dyna_world *world)
{
    std::string line;
    int end_es = 0;
    infile.clear();
    infile.seekg(offset);
    while (std::getline(infile, line) && !end_es) {
	if (line.c_str()[0] == '*') {
	    end_es = 1;
	    break;
	}
	int i = 0;
	int col_cnt = 0;
	struct dyna_element_shell *es;
	BU_GET(es, struct dyna_element_shell);
	es->nodal_pnts[3] = -1;
	while (i < 80 && i < (int)strlen(line.c_str())) {
	    int incr = 8;
	    std::string col = line.substr(i,incr);
	    //printf("col(%d): %s\n", i, col.c_str());
	    /* EID */
	    if (col_cnt == 0) {
		// node id
		char *endptr;
		long nodeid = strtol(col.c_str(), &endptr, 10);
		if (endptr == col.c_str()) {
		    bu_log("Error: string to long didn't work: %s\n", col.c_str());
		    es->EID = -1;
		} else {
		    es->EID = nodeid;
		}
	    }
	    /* PID */
	    if (col_cnt == 1) {
		// node id
		char *endptr;
		long esid = strtol(col.c_str(), &endptr, 10);
		if (endptr == col.c_str()) {
		    bu_log("Error: string to long didn't work: %s\n", col.c_str());
		    es->PID = -1;
		} else {
		    es->PID = esid;
		}
	    }
	    if (col_cnt > 1) {
		char *endptr;
		long npnt = strtol(col.c_str(), &endptr, 10);
		if (endptr == col.c_str()) {
		    es->nodal_pnts[col_cnt-2] = -1;
		} else {
		    es->nodal_pnts[col_cnt-2] = npnt;
		}
	    }
	    i = i + incr;
	    col_cnt++;
	}
	bu_ptbl_ins(world->element_shells, (long *)es);
    }
}


int
main(int argc, char **argv)
{
    struct dyna_world *world;

    if (argc != 3) {
	bu_exit(1, "Usage: dyn-g input.key out.g");
    }
    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Error: file %s not found, aborting.\n", argv[1]);
    }
    if (bu_file_exists(argv[2], NULL)) {
	bu_exit(1, "Error: file %s already exists.\n", argv[2]);
    }

    std::ifstream infile(argv[1]);
    if (!infile.is_open()) {
	bu_exit(1, "Error: unable to open %s for reading.\n", argv[1]);
    }

    BU_GET(world, struct dyna_world);
    BU_GET(world->nodes, struct bu_ptbl);
    BU_GET(world->element_shells, struct bu_ptbl);
    bu_ptbl_init(world->nodes, 8, "init node tbl");
    bu_ptbl_init(world->element_shells, 8, "init element tbl");

    std::string line;
    int nodepos = 0;
    int espos = 0;
    while (std::getline(infile, line)) {
	if (line.c_str()[0] == '*') {
	    size_t endpos = line.find_last_not_of(" \t");
	    std::string keyword = line.substr(1,endpos+1);
	    //printf("file position: %d\n", (int)infile.tellg());
	    if (keyword_match("ELEMENT", keyword)) {
		printf("Have element type: %s\n", keyword.c_str());
	    }
	    if (BU_STR_EQUAL(keyword.c_str(), "ELEMENT_SHELL")) {
		espos = (int)infile.tellg();
	    }
	    if (keyword_match("NODE", keyword)) {
		printf("%s\n", keyword.c_str());
		nodepos = (int)infile.tellg();
	    }
	}
    }
    process_nodes(infile, nodepos, world);
    process_element_shell(infile, espos, world);

    std::map<long,long> NID_to_world;
    for (size_t i = 0; i < BU_PTBL_LEN(world->nodes); i++) {
	struct dyna_node *n = (struct dyna_node *)BU_PTBL_GET(world->nodes, i);
	NID_to_world.insert(std::pair<long,long>(n->NID, (long)i));
    }
    std::map<long,long> EID_to_world;
    for (size_t i = 0; i < BU_PTBL_LEN(world->element_shells); i++) {
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, i);
	EID_to_world.insert(std::pair<long,long>(es->EID, (long)i));
    }

    struct rt_wdb *fd_out;
    if ((fd_out=wdb_fopen(argv[2])) == NULL) {
	bu_log("Error: cannot open BRL-CAD file (%s)\n", argv[2]);
	bu_exit(1, NULL);
    }

    /* Construct map of part ids to EIS */
    std::multimap<long,long> PID_to_EIS;
    std::set<long> PIDs;
    for (size_t i = 0; i < BU_PTBL_LEN(world->element_shells); i++) {
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, i);
	PID_to_EIS.insert(std::pair<long,long>(es->PID,es->EID));
	PIDs.insert(es->PID);
    }

    /* For each PID, make a bot with the set of EID elements associated with it */
    for (std::set<long>::iterator it = PIDs.begin(); it != PIDs.end(); it++) {
	std::set<long> EIDs;
	std::set<long> NIDs;
	std::pair<std::multimap<long,long>::iterator, std::multimap<long,long>::iterator> ret;
	ret = PID_to_EIS.equal_range(*it);
	for (std::multimap<long,long>::iterator eit=ret.first; eit != ret.second; eit++) {
	    long wid = EID_to_world.find(eit->second)->second;
	    struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, wid);
	    EIDs.insert(eit->second);
	    for (int ind = 0; ind < 4; ind++) {
		if (es->nodal_pnts[ind] != -1) NIDs.insert(es->nodal_pnts[ind]);
	    }
	}

	int array_ind = 0;
	std::map<long, long> NID_to_array;
	fastf_t *bot_vertices = (fastf_t *)bu_calloc(NIDs.size()*3, sizeof(fastf_t), "BoT vertices (Dyna nodes)");
	for (std::set<long>::iterator nit = NIDs.begin(); nit != NIDs.end(); nit++) {
	    long wid = NID_to_world.find(*nit)->second;
	    struct dyna_node *n = (struct dyna_node *)BU_PTBL_GET(world->nodes, wid);
	    NID_to_array.insert(std::pair<long, long>(*nit,(long)array_ind));
	    for (int j = 0; j < 3; j++) {
		bot_vertices[(array_ind*3)+j] = n->pnt[j];
	    }
	    array_ind++;
	}

	int *bot_faces = (int *)bu_calloc(EIDs.size() * 2 * 3, sizeof(int), "BoT faces (2 per Dyna element shell quad, 1 per triangle)");
	int eind = 0;
	for (std::set<long>::iterator eit = EIDs.begin(); eit != EIDs.end(); eit++) {
	    long wid = EID_to_world.find(*eit)->second;
	    struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, wid);
	    int np1 = (int)NID_to_array.find(es->nodal_pnts[0])->second;
	    int np2 = (int)NID_to_array.find(es->nodal_pnts[1])->second;
	    int np3 = (int)NID_to_array.find(es->nodal_pnts[2])->second;
	    int np4 = (int)NID_to_array.find(es->nodal_pnts[3])->second;
	    bot_faces[(eind*6)+0] = np1;
	    bot_faces[(eind*6)+1] = np3;
	    bot_faces[(eind*6)+2] = np2;
	    if (np4 != -1) {
		bot_faces[(eind*6)+3] = np1;
		bot_faces[(eind*6)+4] = np4;
		bot_faces[(eind*6)+5] = np3;
	    } else {
		bot_faces[(eind*6)+3] = -1;
		bot_faces[(eind*6)+4] = -1;
		bot_faces[(eind*6)+5] = -1;
	    }
	    eind++;
	}

	struct bu_vls ptname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ptname, "%ld.bot", *it);
	mk_bot(fd_out, bu_vls_addr(&ptname), RT_BOT_SURFACE, RT_BOT_UNORIENTED, NULL, NIDs.size(), EIDs.size() * 2, bot_vertices, bot_faces, NULL, NULL);
	bu_vls_free(&ptname);

    }


    wdb_close(fd_out);
    rt_clean_resource_complete(NULL, &rt_uniresource);

    for (size_t i = 0; i < BU_PTBL_LEN(world->nodes); i++) {
	struct dyna_node *n = (struct dyna_node *)BU_PTBL_GET(world->nodes, i);
	BU_PUT(n, struct dyna_node);
    }
    for (size_t i = 0; i < BU_PTBL_LEN(world->element_shells); i++) {
	struct dyna_element_shell *e = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, i);
	BU_PUT(e, struct dyna_element_shell);
    }

    bu_ptbl_free(world->nodes);
    bu_ptbl_free(world->element_shells);
    BU_PUT(world->element_shells, struct bu_ptbl);
    BU_PUT(world->nodes, struct bu_ptbl);
    BU_PUT(world, struct dyna_world);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
