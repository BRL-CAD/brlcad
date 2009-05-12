/*                 step-g.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step-g.cpp
 *
 * C++ main() for step-g converter.
 *
 */
#include <iostream>

extern void SchemaInit (class Registry &);
//
// include NIST step related headers
//
#include <sdai.h>
#include <STEPfile.h>
#include <SdaiCONFIG_CONTROL_DESIGN.h>
//
// step-g related headers
//
#include <STEPWrapper.h>
#include <BRLCADWrapper.h>


InstMgr   instance_list;

using namespace std;
//namespace po = boost::program_options;

char usage[] =  { "Usage: step-g infile.stp outfile.g\n" };


/* sample loadProgramOptions from boost library
void loadProgramOptions(int ac, char *av[]){
	// Declare the supported options.
	po::options_description desc("Usage: step-g [options] filename.stp filename.g");
	desc.add_options()
	    ("help", "produce help message")
	    ("version", "print program version")
	    ("other", po::value<string>(), "some other string variable")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(ac, av, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
	    cout << desc << "\n";
	    return 1;
	}

	if (vm.count("version")) {
	    cout << "Version: X.XX.X.\n";
	}

	if (vm.count("other")) {
	    cout << "other: "
	 << vm["other"].as<string>() << ".\n";
	} else {
	    cout << "'other' not set.\n";
	}
}
*/
void
printEntity(SCLP23(Application_instance) *se, int level) {
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Entity:" << se->EntityName() << "(" << se->STEPfile_id << ")" << endl;
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Description:" << se->eDesc->Description() << endl;
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Entity Type:" << se->eDesc->Type() << endl;
	for ( int i=0; i< level; i++)
		cout << "    ";
	cout << "Atributes:" << endl;

	STEPattribute *attr;
	se->ResetAttributes();
	while ( (attr = se->NextAttribute()) != NULL ) {
		SCLstring attrval;

		for ( int i=0; i<= level; i++)
			cout << "    ";
		cout << attr->Name() << ": " << attr->asStr(attrval) << " TypeName: " << attr->TypeName() << " Type: " << attr->Type() << endl;
		if ( attr->Type() == 256 ) {
			if (attr->IsDerived()) {
				cout << "        ********* DERIVED *********" << endl;
			} else {
			    printEntity(*(attr->ptr.c),level+2);
			}
		}
	}
}
int main(int argc, char *argv[]) {

	/* started to use boost to handle input args but 
	 * our current version is a bit dated and does not 
	 * have the boost::program_options (actually a
	 * compiled part of the boost library)
	 * 
	 * loadProgramOptions(argc,argv);
         */
	if ( argc != 3){
		cout << usage << endl;
		exit(0);
	}

        // replace with legitimate program otions parser
	std::string iflnm = argv[1];
	std::string oflnm = argv[2];

	STEPWrapper *step = new STEPWrapper();

	/* load STEP file */
	if ( step->loadSTEP(iflnm) ) {
		step->printLoadStatistics();
	}

	return 0;
}
