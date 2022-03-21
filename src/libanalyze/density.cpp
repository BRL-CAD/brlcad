/*                   D E N S I T Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2022 United States Government as represented by
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

#include "common.h"
#include <stdlib.h>
#include <string.h>

#include <climits>
#include <map>
#include <string>
#include <sstream>

#include "analyze.h"


/* arbitrary upper limit just to prevent out-of-control behavior */
#define MAX_MATERIAL_CNT 1000000


struct analyze_densities_impl {
    std::map<long int, std::string> id2name;
    std::map<long int, fastf_t> id2density;
    std::multimap<std::string, long int> name2id;
};


extern "C" int
analyze_densities_init(struct analyze_densities *a)
{
    if (!a)
	return -1;

    a->i = new analyze_densities_impl;

    return (!a->i) ? -1 : 0;
}


extern "C" void
analyze_densities_clear(struct analyze_densities *a)
{
    if (!a)
	return;
    if (!a->i)
	return;

    a->i->id2name.clear();
    a->i->id2density.clear();
    a->i->name2id.clear();
    delete a->i;
}


extern "C" int
analyze_densities_create(struct analyze_densities **a)
{
    if (!a)
	return -1;

    (*a) = new analyze_densities;

    return analyze_densities_init(*a);
}


extern "C" void
analyze_densities_destroy(struct analyze_densities *a)
{
    if (!a)
	return;

    analyze_densities_clear(a);
    delete a;
}


extern "C" int
analyze_densities_set(struct analyze_densities *a, long int id, fastf_t density, const char *name, struct bu_vls *msgs)
{
    if (!a || !a->i)
	return -1;

    if (id < 0 || density < 0 || !name)
	return -1;

    if (a->i->id2density.size() >= MAX_MATERIAL_CNT) {
	if (msgs) {
	    bu_vls_printf(msgs, "ERROR: Maximum materials (%d) exceeded in density table.\n", MAX_MATERIAL_CNT);
	}
	return -1;
    }

    a->i->id2name[id] = std::string(name);
    a->i->id2density[id] = density;
    a->i->name2id.insert(std::pair<std::string, long int>(std::string(name), id));

    return 0;
}


static int
parse_densities_line(struct analyze_densities *a, const char *obuf, size_t len, struct bu_vls *result_str)
{
    double density = -1;
    char *name = NULL;
    long int material_id = -1;

    int have_density = 0;
    char *p, *q, *last;
    long idx;
    char *buf = (char *)bu_malloc(sizeof(char)*len+1, "buffer copy");
    memcpy(buf, obuf, len);

    buf[len] = '\0';
    last = &buf[len];

    p = buf;

    /* Skip initial whitespace */
    while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
	p++;

    /* Skip initial comments */
    while (*p == '#') {
	/* Skip comment */
	while (*p && *p != '\n') {
	    p++;
	}
    }

    /* Skip whitespace */
    while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
	p++;

    while (*p) {
	/* Skip comments */
	if (*p == '#') {
	    /* Skip comment */
	    while (*p && *p != '\n')
		p++;

	    /* Skip whitespace */
	    while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
		p++;

	    continue;
	}

	if (have_density) {
	    bu_free(buf, "free buf copy");
	    bu_free(name, "free name copy");
	    if (result_str) {
		bu_vls_printf(result_str, "Error processing line: \"%s\"\nExtra content after density entry\n", obuf);
	    }
	    return -1;
	}

	idx = strtol(p, &q, 10);
	if (q == (char *)NULL) {
	    bu_free(buf, "free buf copy");
	    if (result_str) {
		bu_vls_printf(result_str, "Error processing line: \"%s\"\ncould not convert idx\n", obuf);
	    }
	    return -1;
	}

	if (idx < 0) {
	    bu_free(buf, "free buf copy");
	    if (result_str) {
		bu_vls_printf(result_str, "Error processing line: \"%s\"\nbad density index (%ld < 0)\n", obuf, idx);
	    }
	    return -1;
	}

	density = strtod(q, &p);
	if (q == p) {
	    bu_free(buf, "free buf copy");
	    if (result_str) {
		bu_vls_printf(result_str, "Error processing line: \"%s\"\ncould not convert density\n", obuf);
	    }
	    return -1;
	}

	if (density < 0.0) {
	    bu_free(buf, "free buf copy");
	    if (result_str) {
		bu_vls_printf(result_str, "Error processing line: \"%s\"\nbad density (%lf < 0)\n", obuf, density);
	    }
	    return -1;
	}

	/* Skip tabs and spaces */
	while (*p && (*p == '\t' || *p == ' '))
	    p++;
	if (!*p)
	    break;

	q = strchr(p, '\n');
	if (q)
	    *q++ = '\0';
	else
	    q = last;


	/* Trim off comments and ws, if any. */
	std::string wstr(p);
	std::string nstr;
	size_t ep = wstr.find_first_of("#");
	if (ep != std::string::npos) {
	    wstr = wstr.substr(0, ep);
	}
	ep = wstr.find_last_not_of(" \t\n\v\f\r");
	size_t sp = wstr.find_first_not_of(" \t\n\v\f\r");
	nstr = wstr.substr(sp, ep-sp+1);
	/* since BRL-CAD does computation in mm, but the table is in
	 * grams / (cm^3) we convert the table on input
	 */
	density = density / 1000.0;
	name = bu_strdup(nstr.c_str());
	material_id = idx;

	p = q;

	/* Skip whitespace */
	while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
	    p++;
    }

    /* If the whole thing was a comment or empty, return 0 */
    if (!name &&  material_id == -1 && density < 0) {
	return 0;
    }


    /* Whatever we saw, if it wasn't a valid definition bail now */
    if (!name || material_id == -1 || density < 0) {
	return -1;
    }

    /* If we have a valid definition, try to insert it */
    if (analyze_densities_set(a, material_id, density, name, result_str) < 0) {
	bu_free(buf, "free buf copy");
	if (result_str) {
	    bu_vls_printf(result_str, "Error inserting density %ld, %g, %s\n", material_id, density, name);
	}
	bu_free(name, "free name copy");
	return -1;

    }

    bu_free(name, "free name copy");
    bu_free(buf, "free buf copy");
    return 1;
}


extern "C" int
analyze_densities_load(struct analyze_densities *a, const char *buff, struct bu_vls *msgs, int *ecnt)
{
    if (!a || !a->i || !buff)
	return 0;

    std::string dbuff(buff);
    std::istringstream ss(dbuff);
    std::string line;
    int dcnt = 0;

    while (std::getline(ss, line)) {
	struct bu_vls lmsg = BU_VLS_INIT_ZERO;

	int ret = parse_densities_line(a, line.c_str(), line.length(), &lmsg);

	if (!ret)
	    continue;

	if (ret < 0) {
	    if (msgs && bu_vls_strlen(&lmsg)) {
		bu_vls_printf(msgs, "%s", bu_vls_cstr(&lmsg));
	    }
	    if (ecnt) {
		(*ecnt)++;
	    }
	} else {
	    dcnt++;
	}
	bu_vls_free(&lmsg);
    }
    return dcnt;
}


extern "C" long int
analyze_densities_write(char **buff, struct analyze_densities *a)
{
    if (!a || !a->i || !buff)
	return 0;

    std::string obuff;
    std::ostringstream os(obuff);

    std::map<long int, std::string>::iterator id_it;
    for (id_it = a->i->id2name.begin(); id_it != a->i->id2name.end(); id_it++) {
	long int nid = id_it->first;
	std::string nname = id_it->second;
	fastf_t ndensity = analyze_densities_density(a, nid);
	os << nid << "\t" << ndensity * 1000 << "\t" << nname << "\n";
    }

    std::string ostring = os.str();
    (*buff) = bu_strdup(ostring.c_str());

    return (long int)ostring.length();
}


extern "C" char *
analyze_densities_name(struct analyze_densities *a, long int id)
{
    if (!a || !a->i || id < 0)
	return NULL;

    if (a->i->id2name.find(id) == a->i->id2name.end())
	return NULL;

    return bu_strdup(a->i->id2name[id].c_str());
}


extern "C" long int
analyze_densities_id(long int *ids, long int max_ids, struct analyze_densities *a, const char *name)
{
    int mcnt = 0;
    std::multimap<std::string, long int>::iterator m_it;
    std::pair<std::multimap<std::string, long int>::iterator, std::multimap<std::string, long int>::iterator> eret;
    if (!a || !a->i || !name)
	return -1;

    eret = a->i->name2id.equal_range(std::string(name));
    for (m_it = eret.first; m_it != eret.second; m_it++) {
	mcnt++;
    }

    if (!ids || max_ids == 0)
	return mcnt;

    /* Have an id buffer, return what we can */
    if (mcnt > 0) {
	int ccnt = 0;
	for (m_it = eret.first; m_it != eret.second; m_it++) {
	    if (ccnt < max_ids) {
		ids[ccnt] = m_it->second;
		ccnt++;
	    }
	}
    }

    return (mcnt > max_ids) ? (max_ids - mcnt) : mcnt;
}


extern "C" fastf_t
analyze_densities_density(struct analyze_densities *a, long int id)
{
    if (!a || !a->i || id < 0)
	return -1.0;

    if (a->i->id2density.find(id) == a->i->id2density.end())
	return -1.0;

    return a->i->id2density[id];
}


extern "C" long int
analyze_densities_next(struct analyze_densities *a, long int id)
{
    if (!a || !a->i)
	return -1;

    if (id < 0) {
	if (a->i->id2density.size()) {
	    std::map<long int, fastf_t>::iterator i = a->i->id2density.begin();
	    return i->first;
	} else {
	    return -1;
	}
    }

    std::map<long int, fastf_t>::iterator idc = a->i->id2density.find(id);
    if (idc != a->i->id2density.end()) {
	std::map<long int, fastf_t>::iterator idn = std::next(idc);
	if (idn != a->i->id2density.end()) {
	    return idn->first;
	} else {
	    return -1;
	}
    }

    return -1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

