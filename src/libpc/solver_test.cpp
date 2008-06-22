/*                 S O L V E R _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file solver_test.cpp
 *
 * @brief Simple Test cases for Constraint Solver
 *
 * @author Dawn Thomas
 *
 */

#include "pcVariable.h"

int main()
{
    Interval<int> I;
    Interval<float> K;
    Interval<float> J;
    Domain<float> D,E;
    Variable<float> V;
    J.assign(3.4,8.3,0.6);
    //D.addInterval(J);
    V.D.addInterval(J);
    K.assign(10.4,20.3,0.8);
    //D.addInterval(K);
    V.D.addInterval(K);
    K.assign(1.2,2.3,0.8);
    V.D.addInterval(K);
    K.assign(-2.4,-1,0.8);
    V.D.addInterval(K);
    K.assign(0.4,1.6,0.8);
    V.D.addInterval(K);
    K.assign(0.2,0.6,0.8);
    V.D.addInterval(K);
    K.assign(20.1,30.6,0.8);
    V.D.addInterval(K);
    K.assign(30.8,40.4,0.8);
    V.D.addInterval(K);
    K.assign(-10.4,-5.6,0.8);
    V.D.addInterval(K);
    K.assign(15.4,18.8,0.8);
    V.D.addInterval(K);
    V.setValue(8.1);
    std::cout<<"V value befor increment"<<V.getValue()<<std::endl;
    ++V;std::cout<<"V value after increment"<<V.getValue()<<std::endl;
    
    /*try {
      K=D.getInterval(6.3);
      }
      catch(pcException E) {
      cout<<"Exception: "<<E.Error()<<std::endl;
      goto test_label;
      }
      cout<<K.getLow()<<" "<<K.getHigh()<<" "<<K.getWidth()<<" "<<K.getStep()<<std::endl;
      
      test_label:*/
    V.D.display(); 
    I.assign(2,8,1);
    std::cout<<I.getLow()<<" "<<I.getHigh()<<" "<<I.getWidth()<<" "<<I.getStep()<<std::endl;
    std::cout<<J.getLow()<<" "<<J.getHigh()<<" "<<J.getWidth()<<" "<<J.getStep()<<std::endl;

    return 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
