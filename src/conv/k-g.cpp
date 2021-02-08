/*                       K - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2010-2021 United States Government as represented by
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
 * Usage: k-g input.key output.g
 */
#include "common.h"

#include <iostream>
#include <algorithm>

#ifndef HAVE_DECL_FSEEKO
#include "bio.h" /* for b_off_t */
extern "C" int fseeko(FILE *, b_off_t, int);
extern "C" b_off_t ftello(FILE *);
#endif
#include <fstream>

#include <string>
#include <map>
#include <set>

#include "vmath.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/opt.h"
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

struct dyna_element_solid {
    long EID;
    long PID;
    int pntcnt;
    long nodal_pnts[8];
};

struct dyna_element_shell {
    long EID;
    long PID;
    long nodal_pnts[4];
};

struct dyna_part {
    const char *heading;
    long PID;
};

struct dyna_world {
    struct bu_ptbl *nodes;
    struct bu_ptbl *element_shells;
    struct bu_ptbl *element_solids;
    struct bu_ptbl *parts;
};

/* See if keyword matches pattern ^keyword* */
size_t
keyword_match(const char *key, std::string line)
{
    if (line.find(key, 0) != std::string::npos && !line.find(key, 0)) return 1;
    return 0;
}

/* Scrub spaces from beginning and end of string */
void
scrub_string(std::string &col)
{
    size_t startpos = col.find_first_not_of(" \t\n\v\f\r");
    if (std::string::npos != startpos) {
	col = col.substr(startpos);
    }
    size_t endpos = col.find_last_not_of(" \t\n\v\f\r");
    if (std::string::npos != endpos) {
	col = col.substr(0 ,endpos+1);
    }
}

/* There is a lot of information (potentially) stored in a part.  For now, we
 * care about the HEADING and the part id. */
void
process_parts(std::ifstream &infile, int offset, struct dyna_world *world)
{
    std::string line;
    int end_part = 0;
    infile.clear();
    infile.seekg(offset);
    int line_cnt = 0;
    struct dyna_part *part;
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    BU_GET(part, struct dyna_part);
    part->heading = NULL;
    part->PID = -1;
    while (std::getline(infile, line) && !end_part) {
	if (line.c_str()[0] == '$') continue;
	if (line_cnt == 2 || line.c_str()[0] == '*') {
	    end_part = 1;
	    break;
	}
	if (line_cnt == 0) {
	    scrub_string(line);
	    if (line.find_first_not_of(" \t\n\v\f\r") != std::string::npos) {
		std::replace(line.begin(), line.end(), '#', '_');
		std::replace(line.begin(), line.end(), ' ', '_');
		std::replace(line.begin(), line.end(), '\t','_');
		std::replace(line.begin(), line.end(), '&', '_');
		std::replace(line.begin(), line.end(), '(', '_');
		std::replace(line.begin(), line.end(), ')', '_');
		std::replace(line.begin(), line.end(), '/', '_');
		bu_vls_sprintf(&pname, "%s", line.c_str());
		part->heading = bu_strdup(bu_vls_addr(&pname));
	    }
	    line_cnt++;
	    continue;
	}
	if (line_cnt == 1) {

	    /* The Dyna keyword file spec allows comma separated files as well as
	     * column formatted files. Try to figure out what we're dealing with.*/
	    if (line.find(" ") == std::string::npos && line.find(",") != std::string::npos) {
		bu_exit(1, "Error: This looks like a csv formatted k file, which is not yet supported.");
	    }


	    std::string col = line.substr(0,10);
	    char *endptr;
	    long pid = strtol(col.c_str(), &endptr, 10);
	    if (endptr == col.c_str()) {
		bu_log("Error: string to long didn't work: %s\n", col.c_str());
		part->PID = -1;
	    } else {
		part->PID = pid;
	    }
	    line_cnt++;
	    continue;
	}
    }
    if (part->heading == NULL) {
	bu_vls_sprintf(&pname, "part_");
	part->heading = bu_strdup(bu_vls_addr(&pname));
    }
    bu_vls_free(&pname);
    bu_ptbl_ins(world->parts, (long *)part);
}

void
process_nodes(std::ifstream &infile, int offset, struct dyna_world *world)
{
    std::string line;
    int end_nodes = 0;
    infile.clear();
    infile.seekg(offset);
    while (std::getline(infile, line) && !end_nodes) {
	if (line.c_str()[0] == '$') continue;
	if (line.c_str()[0] == '*') {
	    end_nodes = 1;
	    break;
	}
	int i = 0;
	int point_ind = 0;
	struct dyna_node *node;
	BU_GET(node, struct dyna_node);

	/* The Dyna keyword file spec allows comma separated files as well as
	 * column formatted files. Try to figure out what we're dealing with.*/
	if (line.find(" ") == std::string::npos && line.find(",") != std::string::npos) {
	    bu_exit(1, "Error: This looks like a csv formatted k file, which is not yet supported.");
	}

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
		    // Some files will have blank entries in columns - assuming intent is
		    // to have that number be zero.  Only complain if we don't have an
		    // all whitespace column
		    if (col.find_first_not_of(" \t\n\v\f\r") != std::string::npos) {
			bu_log("Error: string to double didn't work: %s\n", col.c_str());
		    }
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
process_element_solid(std::ifstream &infile, int offset, struct dyna_world *world)
{
    std::string line;
    int end_es = 0;
    infile.clear();
    infile.seekg(offset);
    char *endptr;
    while (std::getline(infile, line) && !end_es) {
	if (line.c_str()[0] == '$') continue;
	if (line.c_str()[0] == '*') {
	    end_es = 1;
	    break;
	}
	int i = 0;
	int col_cnt = 0;
	int next_line = 0;
	int npnts = 0;
	struct dyna_element_solid *es;
	BU_GET(es, struct dyna_element_solid);
	es->pntcnt = 8;

	/* The Dyna keyword file spec allows comma separated files as well as
	 * column formatted files. Try to figure out what we're dealing with.*/
	if (line.find(" ") == std::string::npos && line.find(",") != std::string::npos) {
	    bu_exit(1, "Error: This looks like a csv formatted k file, which is not yet supported.");
	}


	while (i < 80 && i < (int)strlen(line.c_str())) {
	    int incr = 8;
	    std::string col = line.substr(i,incr);
	    if (!next_line && col_cnt == 0) {
		long eid = strtol(col.c_str(), &endptr, 10);
		//printf("es eid: %s\n", col.c_str());
		es->EID = (endptr == col.c_str()) ? -1 : eid;
	    }
	    if (!next_line && col_cnt == 1) {
		long pid = strtol(col.c_str(), &endptr, 10);
		es->PID = (endptr == col.c_str()) ? -1 : pid;

		// If all whitespace after first two columns, next line
		// is presumed to hold the nodes per LS-DYNA Manual.
		// Otherwise, assume nodes are in the remaining columns
		// on this line and continue as normal.
		std::string remainder = line.substr(i,incr);
		if (remainder.find_first_not_of(" \t\n\v\f\r") == std::string::npos) {
		    std::getline(infile, line);
		    while (line.c_str()[0] == '$' && line.c_str()[0] != '*') {
			std::getline(infile, line);
		    };
		    i = 0;
		    col_cnt = 0;
		    next_line = 1;
		}
	    }
	    if ((col_cnt > 1 && col_cnt < 10) || next_line) {
		long npnt = strtol(col.c_str(), &endptr, 10);
		es->nodal_pnts[npnts] = (endptr == col.c_str()) ? -1 : npnt;
		if (es->nodal_pnts[npnts] == -1) {
		    if (npnts == 4) {
			es->pntcnt = 4;
		    } else if (npnts == 6) {
			es->pntcnt = 6;
		    } else {
			bu_log("Error reading point %d\n", npnts+1);
			break;
		    }
		} else {
		    if (npnts > 3 && npnts < 5 && es->nodal_pnts[3] == es->nodal_pnts[npnts]) {
			/* We have a duplicate point - we're done */
			es->pntcnt = 4;
			es->nodal_pnts[npnts] = -1;
			break;
		    }
		    if (npnts > 5 && es->nodal_pnts[5] == es->nodal_pnts[npnts]) {
			/* We have a duplicate point - we're done */
			es->pntcnt = 6;
			es->nodal_pnts[npnts] = -1;
			break;
		    }
		    npnts++;
		}
	    }
	    i = i + incr;
	    col_cnt++;
	}
	bu_ptbl_ins(world->element_solids, (long *)es);
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
	if (line.c_str()[0] == '$') continue;
	if (line.c_str()[0] == '*') {
	    end_es = 1;
	    break;
	}
	int i = 0;
	int col_cnt = 0;
	struct dyna_element_shell *es;
	BU_GET(es, struct dyna_element_shell);
	es->nodal_pnts[0] = -1;
	es->nodal_pnts[1] = -1;
	es->nodal_pnts[2] = -1;
	es->nodal_pnts[3] = -1;

	/* The Dyna keyword file spec allows comma separated files as well as
	 * column formatted files. Try to figure out what we're dealing with.*/
	if (line.find(" ") == std::string::npos && line.find(",") != std::string::npos) {
	    bu_exit(1, "Error: This looks like a csv formatted k file, which is not yet supported.");
	}


	while (i < 80 && i < (int)strlen(line.c_str())) {
	    int incr = 8;
	    std::string col = line.substr(i,incr);
	    //printf("col(%d): %s\n", i, col.c_str());
	    /* EID */
	    if (col_cnt == 0) {
		// node id
		char *endptr;
		long nodeid = strtol(col.c_str(), &endptr, 10);
		es->EID = (endptr == col.c_str()) ? -1 : nodeid;
	    }
	    /* PID */
	    if (col_cnt == 1) {
		// node id
		char *endptr;
		long esid = strtol(col.c_str(), &endptr, 10);
		es->PID = (endptr == col.c_str()) ? -1 : esid;
	    }
	    if (col_cnt > 1 && col_cnt < 6) {
		char *endptr;
		long npnt = strtol(col.c_str(), &endptr, 10);
		es->nodal_pnts[col_cnt-2] = (endptr == col.c_str()) ? -1 : npnt;
	    }
	    i = i + incr;
	    col_cnt++;
	}
	bu_ptbl_ins(world->element_shells, (long *)es);
    }
}


void
add_element_shell_set(std::map<long,long> &EIDSHELLS_to_world, std::set<long> EIDs, std::map<long,long> &NID_to_world,
       	struct wmember *head, struct dyna_world *world, struct rt_wdb *fd_out, int print_map)
{
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    struct bu_vls node_map = BU_VLS_INIT_ZERO;
    struct bu_vls face_map = BU_VLS_INIT_ZERO;
    struct bu_vls elem_node_map = BU_VLS_INIT_ZERO;


    std::set<long> NIDs;
    for (std::set<long>::iterator sit = EIDs.begin(); sit != EIDs.end(); sit++) {
	long wid = EIDSHELLS_to_world.find(*sit)->second;
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, wid);
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
	if (print_map) {
	    bu_vls_printf(&node_map, "%ld,%ld\n", (long)array_ind, *nit);
	}
	for (int j = 0; j < 3; j++) {
	    bot_vertices[(array_ind*3)+j] = n->pnt[j];
	}
	array_ind++;
    }

    // The actual face count may not be EIDs.size() * 2 if we have triangle elements, so
    // we need to bookkeep.
    int eind = 0;
    int *bot_faces = (int *)bu_calloc(EIDs.size() * 2 * 3, sizeof(int), "BoT faces (2 per Dyna element shell quad, 1 per triangle)");
    for (std::set<long>::iterator eit = EIDs.begin(); eit != EIDs.end(); eit++) {
	long wid = EIDSHELLS_to_world.find(*eit)->second;
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, wid);
	int np1 = (int)NID_to_array.find(es->nodal_pnts[0])->second;
	int np2 = (int)NID_to_array.find(es->nodal_pnts[1])->second;
	int np3 = (int)NID_to_array.find(es->nodal_pnts[2])->second;
	int np4 = (int)NID_to_array.find(es->nodal_pnts[3])->second;
	bot_faces[(eind*3)+0] = np1;
	bot_faces[(eind*3)+1] = np2;
	bot_faces[(eind*3)+2] = np3;
	if (print_map) {
	    bu_vls_printf(&face_map, "%ld,%ld\n", (long)eind, *eit);
	    if (es->nodal_pnts[3] != -1) {
		bu_vls_printf(&elem_node_map, "%ld:%ld,%ld,%ld,%ld\n", es->EID, es->nodal_pnts[0], es->nodal_pnts[1], es->nodal_pnts[2], es->nodal_pnts[3]);
	    } else {
		bu_vls_printf(&elem_node_map, "%ld:%ld,%ld,%ld\n", es->EID, es->nodal_pnts[0], es->nodal_pnts[1], es->nodal_pnts[2]);
	    }
	}
	eind++;
	if (es->nodal_pnts[3] != -1) {
	    bot_faces[(eind*3)+0] = np1;
	    bot_faces[(eind*3)+1] = np3;
	    bot_faces[(eind*3)+2] = np4;
	    if (print_map) {
		bu_vls_printf(&face_map, "%ld,%ld\n", (long)eind, *eit);
	    }
	    eind++;
	}
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    if (print_map) {
	bu_vls_sprintf(&sname, "all_element_shell.bot");
    } else {
	bu_vls_sprintf(&sname, "uncategorized.bot");
    }
    mk_bot(fd_out, bu_vls_addr(&sname), RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, NIDs.size(), eind, bot_vertices, bot_faces, NULL, NULL);
    /* Add the BoT to the parent Comb*/
    (void)mk_addmember(bu_vls_addr(&sname), &(*head).l, NULL, WMOP_UNION);

    if (print_map) {
	/* Add the node and face mappings as attributes to the bot */
	struct directory *dp = db_lookup(fd_out->dbip, bu_vls_addr(&sname), LOOKUP_QUIET);
	(void)bu_avs_add(&avs, "vert_map", bu_vls_addr(&node_map));
	(void)bu_avs_add(&avs, "face_map", bu_vls_addr(&face_map));
	(void)bu_avs_add(&avs, "elem_node_map", bu_vls_addr(&elem_node_map));
	db5_update_attributes(dp, &avs, fd_out->dbip);
    }

    bu_vls_free(&sname);
    bu_vls_free(&node_map);
    bu_vls_free(&face_map);
    bu_vls_free(&elem_node_map);
}


void
add_element_shell_mesh(std::map<long,long> &EIDSHELLS_to_world, std::map<long,long> &NID_to_world,
       	std::multimap<long,long> &PID_to_EISSHELLS, std::set<long> &EIDSHELLS, long pid, struct wmember *head,
       	struct dyna_world *world, struct rt_wdb *fd_out)
{

    std::set<long> EIDs;
    std::set<long> NIDs;
    std::pair<std::multimap<long,long>::iterator, std::multimap<long,long>::iterator> ret;
    ret = PID_to_EISSHELLS.equal_range(pid);
    for (std::multimap<long,long>::iterator eit=ret.first; eit != ret.second; eit++) {
	long wid = EIDSHELLS_to_world.find(eit->second)->second;
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, wid);
	EIDs.insert(eit->second);
	EIDSHELLS.erase(eit->second);
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
	long wid = EIDSHELLS_to_world.find(*eit)->second;
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, wid);
	int np1 = (int)NID_to_array.find(es->nodal_pnts[0])->second;
	int np2 = (int)NID_to_array.find(es->nodal_pnts[1])->second;
	int np3 = (int)NID_to_array.find(es->nodal_pnts[2])->second;
	int np4 = (int)NID_to_array.find(es->nodal_pnts[3])->second;
	bot_faces[(eind*6)+0] = np1;
	bot_faces[(eind*6)+1] = np2;
	bot_faces[(eind*6)+2] = np3;
	if (np4 != -1) {
	    bot_faces[(eind*6)+3] = np1;
	    bot_faces[(eind*6)+4] = np3;
	    bot_faces[(eind*6)+5] = np4;
	} else {
	    bot_faces[(eind*6)+3] = -1;
	    bot_faces[(eind*6)+4] = -1;
	    bot_faces[(eind*6)+5] = -1;
	}
	eind++;
    }

    struct bu_vls sname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sname, "%ld.bot", pid);
    mk_bot(fd_out, bu_vls_addr(&sname), RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, NIDs.size(), EIDs.size() * 2, bot_vertices, bot_faces, NULL, NULL);
    /* Add the BoT to the parent Comb*/
    (void)mk_addmember(bu_vls_addr(&sname), &(*head).l, NULL, WMOP_UNION);
    bu_vls_free(&sname);
}

void
add_element_solid(long wid, std::map<long,long> &NID_to_world, struct wmember *head, struct dyna_world *world, struct rt_wdb *fd_out)
{
    struct dyna_element_solid *es = (struct dyna_element_solid *)BU_PTBL_GET(world->element_solids, wid);
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname, "%ld.s", es->EID);
    if (es->pntcnt == 4) {
	/* ARB4 */
	fastf_t pnts[3*4];
	for (int i = 0; i < 4; i++) {
	    struct dyna_node *n = (struct dyna_node *)BU_PTBL_GET(world->nodes, NID_to_world.find(es->nodal_pnts[i])->second);
	    for (int j = 0; j < 3; j++) pnts[i*3+j] = n->pnt[j];
	}
	mk_arb4(fd_out, bu_vls_addr(&pname), pnts);
    } else if (es->pntcnt == 6) {
	/* ARB6 */
	fastf_t pnts[3*6];
	for (int i = 0; i < 6; i++) {
	    struct dyna_node *n = (struct dyna_node *)BU_PTBL_GET(world->nodes, NID_to_world.find(es->nodal_pnts[i])->second);
	    for (int j = 0; j < 3; j++) pnts[i*3+j] = n->pnt[j];
	}
	mk_arb6(fd_out, bu_vls_addr(&pname), pnts);
    } else {
	/* ARB8 */
	fastf_t pnts[3*8];
	for (int i = 0; i < 8; i++) {
	    struct dyna_node *n = (struct dyna_node *)BU_PTBL_GET(world->nodes, NID_to_world.find(es->nodal_pnts[i])->second);
	    for (int j = 0; j < 3; j++) pnts[i*3+j] = n->pnt[j];
	}
	mk_arb8(fd_out, bu_vls_addr(&pname), pnts);
    }
    /* Add the ARB to the parent Comb*/
    (void)mk_addmember(bu_vls_addr(&pname), &(*head).l, NULL, WMOP_UNION);
    bu_vls_free(&pname); 
}

int
main(int argc, char **argv)
{
    struct wmember all_head;
    struct dyna_world *world;
    int all_elements_one_bot = 0;
    int need_help = 0;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    int uac = 0;

    bu_setprogname(argv[0]);

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",             "",   NULL, &need_help,    "Print help and exit");
    BU_OPT(d[1], "",  "aggregate-element-shells",   "",   NULL, &all_elements_one_bot,    "Rather than grouping ELEMENT_SHELL entities into separate BoTs by part, put them all in one BoT.  Also adds mappings from ELEMENT_SHELL and NODE id numbers to BoT face and vertex indices as attributes (face_map and vert_map) on the BoT");
    BU_OPT_NULL(d[2]);

    argv++; argc--;
    uac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d);

    if (uac == -1) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    if (need_help || argc < 2) {
	int ret = (argc < 2) ? 1 : 0;
	char *help = bu_opt_describe(d, NULL);
	bu_log("Usage: k-g [options] input.key out.g\nOptions:\n%s\n", help);
	if (help) bu_free(help, "help str");
	bu_exit(ret, NULL);
    }
    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(1, "Error: file %s not found, aborting.\n", argv[0]);
    }
    if (bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Error: file %s already exists.\n", argv[1]);
    }

    std::ifstream infile(argv[0]);
    if (!infile.is_open()) {
	bu_exit(1, "Error: unable to open %s for reading.\n", argv[0]);
    }

    /* Initialize the dyna world storage */
    BU_GET(world, struct dyna_world);
    BU_GET(world->nodes, struct bu_ptbl);
    BU_GET(world->element_shells, struct bu_ptbl);
    BU_GET(world->element_solids, struct bu_ptbl);
    BU_GET(world->parts, struct bu_ptbl);
    bu_ptbl_init(world->nodes, 8, "init node tbl");
    bu_ptbl_init(world->element_shells, 8, "init element tbl");
    bu_ptbl_init(world->element_solids, 8, "init element tbl");
    bu_ptbl_init(world->parts, 8, "init part tbl");

    /* all_head will hold all the parts/regions */
    BU_LIST_INIT(&all_head.l);


    /* TODO: This is inefficient and ugly - I should just call the processing
     * bits, return the new line pos if needed, and continue. */
    std::string line;
    std::set<int> nodes;
    std::set<int> parts;
    std::set<int> element_shells;
    std::set<int> element_solids;
    while (std::getline(infile, line)) {
	if (line.c_str()[0] == '$') continue;
	if (line.c_str()[0] == '*') {
	    size_t endpos = line.find_last_not_of(" \t\r");
	    std::string keyword = line.substr(1,endpos);
	    //printf("file position: %d\n", (int)infile.tellg());
	    if (BU_STR_EQUAL(keyword.c_str(), "ELEMENT_SHELL")) {
		element_shells.insert((int)infile.tellg());
		continue;
	    }
	    if (BU_STR_EQUAL(keyword.c_str(), "ELEMENT_SOLID")) {
		element_solids.insert((int)infile.tellg());
		continue;
	    }
	    if (BU_STR_EQUAL(keyword.c_str(), "PART")) {
		parts.insert((int)infile.tellg());
		continue;
	    }
	    if (BU_STR_EQUAL(keyword.c_str(), "NODE")) {
		nodes.insert((int)infile.tellg());
		continue;
	    }
	    if (keyword_match("ELEMENT", keyword)) {
		printf("Warning: have unhandled element type: %s\n", keyword.c_str());
	    }
	}
    }
    for (std::set<int>::iterator it = nodes.begin(); it != nodes.end(); it++) {
	process_nodes(infile, *it, world);
    }
    for (std::set<int>::iterator it = element_shells.begin(); it != element_shells.end(); it++) {
	process_element_shell(infile, *it, world);
    }
    for (std::set<int>::iterator it = element_solids.begin(); it != element_solids.end(); it++) {
        process_element_solid(infile, *it, world);
    }
    for (std::set<int>::iterator it = parts.begin(); it != parts.end(); it++) {
	process_parts(infile, *it, world);
    }

    /* If we have no nodes, this .k file does not have the information needed
     * to define geometry - bail. */
    if (nodes.empty()) {
	bu_exit(1, "No geometry NODE information present in file - cannot define geometry\n");
    }

    /* We need to reference the various dyna objects by their id numbers - build maps */
    std::map<long,long> NID_to_world;
    for (size_t i = 0; i < BU_PTBL_LEN(world->nodes); i++) {
	struct dyna_node *n = (struct dyna_node *)BU_PTBL_GET(world->nodes, i);
	NID_to_world.insert(std::pair<long,long>(n->NID, (long)i));
    }
    std::map<long,long> EIDSHELLS_to_world;
    std::set<long> EIDSHELLS;
    for (size_t i = 0; i < BU_PTBL_LEN(world->element_shells); i++) {
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, i);
	EIDSHELLS_to_world.insert(std::pair<long,long>(es->EID, (long)i));
	EIDSHELLS.insert(es->EID);
    }
    std::map<long,long> EIDSOLIDS_to_world;
    std::set<long> EIDSOLIDS;
    for (size_t i = 0; i < BU_PTBL_LEN(world->element_solids); i++) {
	struct dyna_element_solid *es = (struct dyna_element_solid *)BU_PTBL_GET(world->element_solids, i);
	EIDSOLIDS_to_world.insert(std::pair<long,long>(es->EID, (long)i));
	EIDSOLIDS.insert(es->EID);
    }
    std::map<long,long> PID_to_world;
    for (size_t i = 0; i < BU_PTBL_LEN(world->parts); i++) {
	struct dyna_part *p = (struct dyna_part *)BU_PTBL_GET(world->parts, i);
	PID_to_world.insert(std::pair<long,long>(p->PID, (long)i));
    }


    /* Time for a .g file */
    struct rt_wdb *fd_out;
    if ((fd_out=wdb_fopen(argv[1])) == NULL) {
	bu_log("Error: cannot open BRL-CAD file (%s)\n", argv[1]);
	bu_exit(1, NULL);
    }


    /* We need to know what all our active PIDs are */
    std::set<long> PIDs;

    /* Construct map of part ids to EIS for element_shell objects */
    std::multimap<long,long> PID_to_EISSHELLS;
    for (size_t i = 0; i < BU_PTBL_LEN(world->element_shells); i++) {
	struct dyna_element_shell *es = (struct dyna_element_shell *)BU_PTBL_GET(world->element_shells, i);
	PID_to_EISSHELLS.insert(std::pair<long,long>(es->PID,es->EID));
	PIDs.insert(es->PID);
    }

    /* Construct map of part ids to EIS for element_solid objects */
    std::multimap<long,long> PID_to_EISSOLIDS;
    for (size_t i = 0; i < BU_PTBL_LEN(world->element_solids); i++) {
	struct dyna_element_solid *es = (struct dyna_element_solid *)BU_PTBL_GET(world->element_solids, i);
	PID_to_EISSOLIDS.insert(std::pair<long,long>(es->PID,es->EID));
	PIDs.insert(es->PID);
    }

    /* If we have parts, for each PID make a combination and associated solids */
    for (std::set<long>::iterator it = PIDs.begin(); it != PIDs.end(); it++) {

	/* If it's not in the world, keep going */
	if (PID_to_world.find(*it) == PID_to_world.end()) continue;

	/* Set up containing region */
	struct wmember head;
	BU_LIST_INIT(&head.l);
	struct bu_vls rname = BU_VLS_INIT_ZERO;
	long wid = PID_to_world.find(*it)->second;
	struct dyna_part *p = (struct dyna_part *)BU_PTBL_GET(world->parts, wid);
	bu_vls_sprintf(&rname, "%s_%ld.r", p->heading, p->PID);
	/* steal this from step-g coloring - really need to fold into a libbu random color function
	 * once the API gets figured out... */
	unsigned char rgb[3];
	fastf_t hsv[3] = { 0.0, 0.5, 0.95 };
	double golden_ratio_conjugate = 0.618033988749895;
	fastf_t h = drand48();
	h = fmod(h+golden_ratio_conjugate,1.0);
	*hsv = h * 360.0;
	bu_hsv_to_rgb(hsv,rgb);


	/* Handle shells associated with this PID, unless we're going to aggregate */
	if (!all_elements_one_bot) {
	    if (PID_to_EISSHELLS.find(*it) != PID_to_EISSHELLS.end()) {
		add_element_shell_mesh(EIDSHELLS_to_world, NID_to_world, PID_to_EISSHELLS, EIDSHELLS, *it, &head, world, fd_out);
	    }
	}

	/* Solids */
	std::pair<std::multimap<long,long>::iterator, std::multimap<long,long>::iterator> ret;
	if (PID_to_EISSOLIDS.find(*it) != PID_to_EISSOLIDS.end()) {
	    ret = PID_to_EISSOLIDS.equal_range(*it);
	    for (std::multimap<long,long>::iterator eit=ret.first; eit != ret.second; eit++) {
		wid = EIDSOLIDS_to_world.find(eit->second)->second;
		add_element_solid(wid, NID_to_world, &head, world, fd_out);
		EIDSOLIDS.erase(eit->second);
	    }
	}


	/* We've got everything now - make the region comb and add it to the top level comb */
	if (!all_elements_one_bot || (all_elements_one_bot && PID_to_EISSOLIDS.find(*it) != PID_to_EISSOLIDS.end())) {
	    mk_comb(fd_out, bu_vls_addr(&rname), &head.l, 1, (char *)NULL, (char *)NULL, rgb, *it, 0, 0, 0, 0, 0, 0);
	    (void)mk_addmember(bu_vls_addr(&rname), &all_head.l, NULL, WMOP_UNION);
	}

	/* Clean up */
	bu_vls_free(&rname);
    }

    /* Collect any leftover shells that didn't have parent parts */
    if (EIDSHELLS.size() > 0 && !all_elements_one_bot) {
	bu_log("Warning - %zu unorganized ELEMENT_SHELL objects.  This may be an indication that this file uses features of the dyna keyword format we don't support yet.\n", EIDSHELLS.size());
    }
    if (EIDSHELLS.size() > 0) {
	add_element_shell_set(EIDSHELLS_to_world, EIDSHELLS, NID_to_world, &all_head, world, fd_out, all_elements_one_bot);
    }

    /* Collect any leftover solids that didn't have parent parts */
    for (std::set<long>::iterator sit = EIDSOLIDS.begin(); sit != EIDSOLIDS.end(); sit++) {
	long wid = EIDSOLIDS_to_world.find(*sit)->second;
	add_element_solid(wid, NID_to_world, &all_head, world, fd_out);
    }

    mk_comb(fd_out, "all", &all_head.l, 0, (char *)NULL, (char *)NULL, NULL, 0, 0, 0, 0, 0, 0, 0);

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
    bu_ptbl_free(world->element_solids);
    bu_ptbl_free(world->parts);
    BU_PUT(world->element_solids, struct bu_ptbl);
    BU_PUT(world->element_shells, struct bu_ptbl);
    BU_PUT(world->nodes, struct bu_ptbl);
    BU_PUT(world->parts, struct bu_ptbl);
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
