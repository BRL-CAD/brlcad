/*
 * STEPEntity.h
 *
 *  Created on: May 7, 2009
 *      Author: wbowman
 */

#ifndef STEPENTITY_H_
#define STEPENTITY_H_

#include <string>
#include <vector>
#include <list>
#include <map>
using namespace std;

class STEPWrapper;
class ON_Brep;

#define POINT_CLOSENESS_TOLERANCE 1e-6
#define TAB(j) \
	{ \
		for ( int i=0; i< j; i++) \
			cout << "    "; \
	}

class STEPEntity {
protected:
	int id;
	int ON_id;
	STEPWrapper *step;

public:
	STEPEntity();
	virtual ~STEPEntity();

	int GetId() {return id;}
	int GetONId() {return ON_id;}
	void SetONId(int id) {ON_id = id;}
	int STEPid();
	STEPWrapper *Step();
};

#endif /* STEPENTITY_H_ */
