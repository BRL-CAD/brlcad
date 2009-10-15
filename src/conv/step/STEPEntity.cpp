/*
 * STEPEntity.cpp
 *
 *  Created on: May 7, 2009
 *      Author: wbowman
 */

#include "STEPEntity.h"

STEPEntity::STEPEntity() {
	step=NULL;
	id=0;
	ON_id = -1;
}

STEPEntity::~STEPEntity() {
}

int
STEPEntity::STEPid() {
	return id;
}

STEPWrapper *STEPEntity::Step() {
	return step;
}
