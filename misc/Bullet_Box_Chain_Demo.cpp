/*
 * Copyright (c) 2014-2021 United States Government as represented by
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

/* TODO - for this type of problem, Simbody might also be work a look:
 * https://github.com/simbody/simbody
 * /

/** @file Bullet_Box_Chain_Demo.cpp
 *
 * Based on Bullet Constraint demo, which has the following license:
 *
 * Bullet Continuous Collision Detection and Physics Library
 * Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it freely,
 * subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <iostream>

#include "btBulletDynamicsCommon.h"
#include "LinearMath/btIDebugDraw.h"

#include "GLDebugDrawer.h"

#include "GLDebugFont.h"
#include <stdio.h> //printf debugging

#include "ConstraintDemo.h"
#include "GL_ShapeDrawer.h"
#include "GlutStuff.h"

#include "GLDebugDrawer.h"
static GLDebugDrawer	gDebugDrawer;

#define CUBE_HALF_EXTENTS 50.f

void	ConstraintDemo::setupEmptyDynamicsWorld()
{
    m_collisionConfiguration = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);
    m_overlappingPairCache = new btDbvtBroadphase();
    m_constraintSolver = new btSequentialImpulseConstraintSolver();
    m_dynamicsWorld = new btDiscreteDynamicsWorld(m_dispatcher,m_overlappingPairCache,m_constraintSolver,m_collisionConfiguration);

}

void btv(const btVector3 *vect)
{
    std::cout << (*vect)[0] << "," << (*vect)[1] << "," << (*vect)[2];
}

void rbcom(const char *name, btRigidBody *body)
{
    std::cout << name << " center of mass: ";
    btv(&(body->getCenterOfMassPosition()));
    std::cout << "\n";
}

void	ConstraintDemo::clientResetScene()
{
    exitPhysics();
    initPhysics();
}

btRigidBody* body5;

void	ConstraintDemo::initPhysics()
{
    setTexturing(true);
    setShadows(true);

    setCameraDistance(26.f);
    m_Time = 0;

    setupEmptyDynamicsWorld();

    m_dynamicsWorld->setDebugDrawer(&gDebugDrawer);

    int hd = 4;
    int d = 2*hd;
    int xoffset = 1;
    int yoffset = 12;
    int zoffset = 5;
    float damping = 0.5;
    btCollisionShape* shape1 = new btBoxShape(btVector3(hd, 1, 1));
    btCollisionShape* shape2 = new btBoxShape(btVector3(hd, 2, 2));
    btCollisionShape* shape3 = new btBoxShape(btVector3(hd, 3, 3));
    btCollisionShape* shape4 = new btBoxShape(btVector3(hd+1, 4, 4));
    btCollisionShape* shape5 = new btBoxShape(btVector3(hd, 5, 5));
    m_collisionShapes.push_back(shape1);
    m_collisionShapes.push_back(shape2);
    m_collisionShapes.push_back(shape3);
    m_collisionShapes.push_back(shape4);
    m_collisionShapes.push_back(shape5);
    float mass = 1.f;
    //point to point constraint (ball socket)
    {
	btTransform trans1;
	trans1.setIdentity();
	trans1.setOrigin(btVector3(xoffset, yoffset, zoffset));
	btRigidBody* body1 = localCreateRigidBody(mass,trans1,shape1);
	body1->setDamping(damping,damping);
	btTypedConstraint* p2p1 = new btPoint2PointConstraint(*body1, trans1.inverse() * (body1->getCenterOfMassPosition() + btVector3(-hd, 0 , 0)));
	m_dynamicsWorld->addConstraint(p2p1);
	p2p1->setDbgDrawSize(btScalar(5.f));


	btTransform trans2;
	trans2.setIdentity();
	trans2.setOrigin(btVector3(1*d+xoffset, yoffset, zoffset));
	btRigidBody* body2 = localCreateRigidBody(mass,trans2,shape2);
	body2->setDamping(damping,damping);
	btTypedConstraint* p2p2 = new btPoint2PointConstraint(*body1, *body2, trans1.inverse() * (body1->getCenterOfMassPosition() + btVector3(hd, 0 , 0)), trans2.inverse() * (body2->getCenterOfMassPosition() + btVector3(-hd, 0 , 0)));
	m_dynamicsWorld->addConstraint(p2p2);
	p2p2->setDbgDrawSize(btScalar(5.f));


	btTransform trans3;
	trans3.setIdentity();
	trans3.setOrigin(btVector3(2*d+xoffset, yoffset, zoffset));
	btRigidBody* body3 = localCreateRigidBody(mass,trans3,shape3);
	body3->setDamping(damping,damping);
	btTypedConstraint* p3p3 = new btPoint2PointConstraint(*body2, *body3, trans2.inverse() * (body2->getCenterOfMassPosition() + btVector3(hd, 0 , 0)), trans3.inverse() * (body3->getCenterOfMassPosition() + btVector3(-hd, 0 , 0)));
	m_dynamicsWorld->addConstraint(p3p3);
	p3p3->setDbgDrawSize(btScalar(5.f));

	btTransform trans4;
	trans4.setIdentity();
	trans4.setOrigin(btVector3(3*d+xoffset, yoffset, zoffset));
	btRigidBody* body4 = localCreateRigidBody(mass,trans4,shape4);
	body4->setDamping(damping,damping);
	btTypedConstraint* p4p4 = new btPoint2PointConstraint(*body3, *body4, trans3.inverse() * (body3->getCenterOfMassPosition() + btVector3(hd, 0 , 0)), trans4.inverse() * (body4->getCenterOfMassPosition() + btVector3(-hd-1, 0 , 0)));
	m_dynamicsWorld->addConstraint(p4p4);
	p4p4->setDbgDrawSize(btScalar(5.f));

	btTransform trans5;
	trans5.setIdentity();
	trans5.setOrigin(btVector3(3*d+(d+2)+xoffset, yoffset, zoffset));
	/*btRigidBody**/ body5 = localCreateRigidBody(mass,trans5,shape5);
	body5->setDamping(damping,damping);
	btTypedConstraint* p5p5 = new btPoint2PointConstraint(*body4, *body5, trans4.inverse() * (body4->getCenterOfMassPosition() + btVector3(hd+1, 0 , 0)), trans5.inverse() * (body5->getCenterOfMassPosition() + btVector3(-hd, 0 , 0)));
	m_dynamicsWorld->addConstraint(p5p5);
	p5p5->setDbgDrawSize(btScalar(5.f));
    }

}

void	ConstraintDemo::exitPhysics()
{

    int i;

    //removed/delete constraints
    for (i=m_dynamicsWorld->getNumConstraints()-1; i>=0 ;i--)
    {
	btTypedConstraint* constraint = m_dynamicsWorld->getConstraint(i);
	m_dynamicsWorld->removeConstraint(constraint);
	delete constraint;
    }
    m_ctc = NULL;

    //remove the rigidbodies from the dynamics world and delete them
    for (i=m_dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--)
    {
	btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[i];
	btRigidBody* body = btRigidBody::upcast(obj);
	if (body && body->getMotionState())
	{
	    delete body->getMotionState();
	}
	m_dynamicsWorld->removeCollisionObject( obj );
	delete obj;
    }




    //delete collision shapes
    for (int j=0;j<m_collisionShapes.size();j++)
    {
	btCollisionShape* shape = m_collisionShapes[j];
	delete shape;
    }

    m_collisionShapes.clear();

    //delete dynamics world
    delete m_dynamicsWorld;

    //delete solver
    delete m_constraintSolver;

    //delete broadphase
    delete m_overlappingPairCache;

    //delete dispatcher
    delete m_dispatcher;

    delete m_collisionConfiguration;

}

ConstraintDemo::~ConstraintDemo()
{
    //cleanup in the reverse order of creation/initialization

    exitPhysics();

}

/* evil global variable hack */
int run_sim;

void ConstraintDemo::clientMoveAndDisplay()
{

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
if (run_sim) {
    float dt = float(getDeltaTimeMicroseconds()) * 0.000001f;

    {
	static bool once = true;
	if ( m_dynamicsWorld->getDebugDrawer() && once)
	{
	    m_dynamicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_DrawConstraints+btIDebugDraw::DBG_DrawConstraintLimits);
	    once=false;
	}
    }


    {
	//during idle mode, just run 1 simulation step maximum
	int maxSimSubSteps = m_idle ? 1 : 1;
	if (m_idle)
	    dt = 1.0f/420.f;

	int numSimSteps = m_dynamicsWorld->stepSimulation(dt,maxSimSubSteps);

	//optional but useful: debug drawing
	m_dynamicsWorld->debugDrawWorld();
    }
}
    renderme();

    glFlush();
    swapBuffers();
}




void ConstraintDemo::displayCallback(void) {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_dynamicsWorld)
	m_dynamicsWorld->debugDrawWorld();

    renderme();

    glFlush();
    swapBuffers();
}


void ConstraintDemo::keyboardCallback(unsigned char key, int x, int y)
{
    (void)x;
    (void)y;
    switch (key)
    {
	case 'r':
	    {
		run_sim = 1;
	    }
	    break;
	case 'p':
	    {
		std::cout << "body5 position: " << body5->getOrientation()[0] << "," << body5->getOrientation()[1] << "," << body5->getOrientation()[2] << "," << body5->getOrientation()[3] << "\n";
	    }
	    break;

	default :
	    {
		float dt = 1.0f/20.f;
		run_sim = 0;
		m_dynamicsWorld->stepSimulation(dt,1);
		std::cout << "body5 position: " << body5->getOrientation()[0] << "," << body5->getOrientation()[1] << "," << body5->getOrientation()[2] << "," << body5->getOrientation()[3] << "\n";
		//DemoApplication::keyboardCallback(key, x, y);
	    }
	    break;
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
