/*                       S E M C H K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file semchk.cpp
 *
 * Check that all non-LAST *_SEM_* definitions in the BRL-CAD headers are
 * numerically unique.
 */

// TODO - if this is needed long term, we need to do as the env cmd builder
// does and assemble (and figure out the definitions for everything in a
// bu_semaphore_acquire call as well as just pattern matching the SEM
// definitions themselves...

#include "common.h"

#include <stdlib.h> /* for strtol */
#include <errno.h> /* for errno */

#include <fstream>
#include <iostream>
#include <regex>
#include <map>
#include <queue>
#include <string>

void
_trim_whitespace(std::string &s)
{
    size_t ep = s.find_last_not_of(" \t\n\v\f\r");
    size_t sp = s.find_first_not_of(" \t\n\v\f\r");
    if (sp == std::string::npos) {
	s.clear();
	return;
    }
    s = s.substr(sp, ep-sp+1);
}

int
process_file(std::map<std::string, std::string> *sem_defs, std::map<std::string, std::string> *sem_files, std::string f)
{
    int ret = 0;
    std::string sline;
    std::ifstream fs(f);
    if (!fs.is_open()) {
	std::cerr << "Unable to open " << f << " for reading, skipping\n";
	exit(-1);
    }

    std::regex sem_match(".*define [A-Z]+_SEM_[A-Z0-9]+ .*");
    std::regex sem_split(".*define ([A-Z]+_SEM_[A-Z0-9]+) ([^/]*)");
    while (std::getline(fs, sline)) {
	if (std::regex_match(sline, sem_match)) {
	    std::smatch sdef;
	    if (!std::regex_search(sline, sdef, sem_split)) {
		std::cerr << "Error, could not parse sem definition:" << sline << "\nquitting\n";
		exit(-1);
	    } else {
		std::string key = std::string(sdef[1]);
		std::string val = std::string(sdef[2]);
		std::replace(val.begin(), val.end(), '(', ' ');
		std::replace(val.begin(), val.end(), ')', ' ');
		_trim_whitespace(key);
		_trim_whitespace(val);
		if (sem_defs->find(key) != sem_defs->end()) {
		    std::cerr << "Error - duplicate definition of semaphore " << key << " found in file " << f << "\n";
		    std::cerr << "Previous definition found in file " << (*sem_files)[key] << "\n";
		    exit(-1);
		}
		(*sem_defs)[key] = val;
		(*sem_files)[key] = f;
		ret++;
	    }
	}
    }
    fs.close();
    return ret;
}

int
validate_semaphores(std::map<std::string, std::string> *sem_defs, int verbose)
{
    long int max_val = -1;
    std::queue<std::string> unevaluated_1, unevaluated_2;
    std::queue<std::string> *working, *next;
    std::map<std::string, long int> semaphore_values;
    std::map<long int, std::string> val_names;
    std::multimap<long int, std::string> semaphore_names;

    std::map<std::string, std::string>::iterator s_it;
    std::regex last_match("[A-Z]+_SEM_LAST");

    for (s_it = sem_defs->begin(); s_it != sem_defs->end(); s_it++) {
	std::string key = (*s_it).first;
	std::string val = (*s_it).second;
	long int l;
	char *endptr = NULL;
	errno = 0;
	l = strtol(val.c_str(), &endptr, 0);
	if ((endptr != NULL && strlen(endptr) > 0) || errno == ERANGE) {
	    unevaluated_1.push(key);
	} else {
	    semaphore_values[key] = l;
	    val_names[l] = key;
	    semaphore_names.insert(std::pair<long int, std::string>(l, key));
	}
    }

    working = &unevaluated_1;
    next = &unevaluated_2;
    int wcnt = working->size();

    while (!working->empty()) {

	std::string key = working->front();
	std::string val = (*sem_defs)[key];

	working->pop();

	//std::cerr << "Processing " << key << " -> " << val << "\n";

	if (semaphore_values.find(val) != semaphore_values.end()) {

	    // The string value has a numerical definition - use it
	    semaphore_values[key] = semaphore_values[val];

	    //std::cout << "DEFINED " << key <<": " << val << " = " << semaphore_values[val] << "\n";

	    // LAST values get reused by the next library in the chain,
	    // so their numerical definitions are duplicates.  For anything
	    // but a LAST variable, we need uniqueness.  Use a multimap
	    // to keep track of this.
	    if (!std::regex_match(key, last_match)) {
		semaphore_names.insert(std::pair<long int, std::string>(semaphore_values[val], key));
		max_val = (semaphore_values[val] > max_val) ? semaphore_values[val] : max_val;
		val_names[semaphore_values[val]] = key; 
	    }

	} else {

	    // If the above didn't work, see if we have an expression we can evaluate
	    std::regex exp_split("([A-Z]+_SEM_[A-Z0-9]+)[+]([0-9]+)");
	    std::smatch sem_exp;
	    if (!std::regex_search(val, sem_exp, exp_split)) {

		// Not an expression - must be a string key we don't know the
		// value of yet. queue and continue
		next->push(key);

		if (!key.length()) {
		    std::cerr << "Empty key string??\n";
		    exit(1);
		}

	    } else {

		std::string ekey = std::string(sem_exp[1]);
		std::string einc = std::string(sem_exp[2]);
		long int ninc;
		char *endptr = NULL;
		errno = 0;
		ninc = strtol(einc.c_str(), &endptr, 0);
		if ((endptr != NULL && strlen(endptr) > 0) || errno == ERANGE) {
		    std::cerr << "Could not evalute expression number: " << einc << "\n";
		    exit(1);
		}

		if (semaphore_values.find(ekey) != semaphore_values.end()) {


		    long int nval = semaphore_values[ekey] + ninc;
		    semaphore_values[key] = nval;

		    //std::cout << "EVAL " << key <<": " << ekey << " + 1" << " = " << nval << "\n";

		    // LAST values get reused by the next library in the chain,
		    // so their numerical definitions are duplicates.  For anything
		    // but a LAST variable, we need uniqueness.  Use a multimap
		    // to keep track of this.
		    if (!std::regex_match(key, last_match)) {
			semaphore_names.insert(std::pair<long int, std::string>(nval, key));
			max_val = (nval > max_val) ? nval : max_val;
			val_names[nval] = key;
		    }

		} else {

		    // Don't know the value of the string in the expression
		    // yet, queue and continue
		    next->push(key);

		    if (!key.length()) {
			std::cerr << "Empty key string??\n";
			exit(1);
		    }
		}
	    }
	}

	if (working->empty()) {
	    std::queue<std::string> *qtmp = next;
	    next = working;
	    working = qtmp;
	    if ((int)working->size() == wcnt) {
		// Infinite loop - we should always process at least one definition per pass
		std::cerr << "Error processing semaphore definitions.  (Perhaps the regex pattern matching didn't catch a string being used as a semaphore?):\n";
		while (!working->empty()) {
		    key = working->front();
		    val = (*sem_defs)[key];
		    std::cerr << key << " -> " << val << "\n";
		    working->pop();
		}
		exit(-1);
	    }
	    wcnt = working->size();
	}
    }

    // All are processed - now, validate
    int have_error = 0;
    for (long int i = 0; i <= max_val; i++) {
	if (!semaphore_names.count(i)) {
	    if (verbose) {
		std::cerr << "WARNING: semaphore number " << i << " is within the active semaphore number range but doesn't have an assigned name\n";
	    }
	    //have_error = 1;
	}
	if (semaphore_names.count(i) > 1) {
	    std::cerr << "\nERROR: multiple BRL-CAD semaphore names share the number " << i << ":\n";
	    std::multimap<long int, std::string>::iterator n_it;
	    std::pair<std::multimap<long int, std::string>::iterator,  std::multimap<long int, std::string>::iterator> eret;
	    eret = semaphore_names.equal_range(i);
	    for (n_it = eret.first; n_it != eret.second; ++n_it) {
		std::cerr << n_it->second << "\n";
	    }
	    have_error = 1;
	}
    }


    // TODO - validate numerical ordering vs naming as a function of library group - e.g. BU always less than RT

    if (have_error) {
	return 1;
    } else {
	if (verbose) {
	    std::cout << "Found " << val_names.size() << " semaphore definitions:\n";
	    std::map<long int, std::string>::iterator os_it;
	    for (os_it = val_names.begin(); os_it != val_names.end(); os_it++) {
		std::cout << os_it->first << " " << os_it->second << "\n";
	    }
	}
    }

    return 0;
}


int
main(int argc, const char *argv[])
{
    int ret = 0;
    if (argc != 2) {
	std::cerr << "Usage: semchk file_list\n";
	return -1;
    }

    bu_setprogname(av[0]);

    std::map<std::string, std::string> sem_defs;
    std::map<std::string, std::string> sem_files;

    std::string sfile;
    std::ifstream fs;
    fs.open(argv[1]);
    if (!fs.is_open()) {
	std::cerr << "Unable to open file list " << argv[1] << "\n";
    }
    while (std::getline(fs, sfile)) {
	//std::cout << "Processing " << sfile << "\n";
	ret += process_file(&sem_defs, &sem_files, sfile);
    }
    fs.close();

    if (ret > 0) {
	ret = validate_semaphores(&sem_defs, 0);
    } else {
	std::cerr << "No semaphore definitions found??\n";
	exit(-1);
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

