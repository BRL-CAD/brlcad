/*       B R L C A D G E O M E T R Y S E R V E R . J A V A
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file BrlcadGeometryServer.java
 *
 */

package mil.army.arl.brlcad;

import mil.army.arl.muves.geometry.*;
import java.rmi.RemoteException;

public class BrlcadGeometryServer implements GeometryServer {
    public BrlcadGeometryServer()
	throws java.rmi.RemoteException {

    }

    public boolean loadGeometry(String geomInfo)
	throws RemoteException {
        return false;
    }

    public Vect shootRay(Point origin, Vect dir)
	throws RemoteException {
        return null;
    }

    public BoundingBox getBoundingBox()
	throws RemoteException {
        return null;
    }

    public BoundingBox getBoundingBox(String item)
	throws RemoteException {
        return null;
    }

    public String getTitle() throws RemoteException {
        return null;
    }

    public boolean makeHole(Point origin, Vect dir,
			    float baseDiam, float topDiam)
	throws RemoteException {
        return false;
    }
}
