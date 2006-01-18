/*             B R L C A D G E O M E T R Y . J A V A
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file BrlcadGeometry.java
 *
 */

package mil.army.arl.brlcad;

import java.rmi.*;
import java.util.Vector;

/**
 * Provides a wrapper for BRL-CAD library capabilities.
 */
public interface BrlcadGeometry extends Remote {
    /**
     * Specifies the BRL-CAD target description and top level
     * object to be managed by the server.
     *
     * @param geomInfo the name of the target description
     * @return         a boolean indicating success or failure
     * @see            SomethingElse
     */
    public boolean loadGeometry(String geomInfo)
	throws RemoteException;

    /**
     * Performs a ray trace of the currently-loaded geometry.
     * <p>
     * Should return an ordered sequence(Vector) of Partitions
     *
     * @param   origin The point at which the ray originates.
     *          dir    The direction in which the ray points.
     * @return         An ordered sequence(Vector) of Partitions.
     * @see            Elsewhere.
     */
    public Vect shootRay(Point origin, Vect dir)
	throws RemoteException;

    /**
     * Returns the bounding box of the top-level object
     * managed by this server.
     *
     * @return       Bounding box of the top level object(s)
     */
    public BoundingBox getBoundingBox()
	throws RemoteException;

    /**
     * Returns bounding box of the specified object
     *
     * @param item  The name of the object.
     * @return      The bounding box of the object, null if the object does not exist.
     */
    public BoundingBox getBoundingBox(String item)
	throws RemoteException;

    /**
     * Returns the Title field in the database.
     *
     * @return      Database title if it is set, null otherwise.
     */
    public String getTitle()
	throws RemoteException;

    /**
     * Creates a tgc hole in the geometry.
     * <p>
     * Note that there is no indication of the objects through
     * which the hole goes. Will need to determine which regions
     * from which to subtract the hole.
     *
     * @param  origin   The origin of the hole.
     *         dir      The direction of the hole
     *         baseDiam The diameter of the hole at the origin.
     *         topDiam  The diameter of the hole at the
     * @return          Boolean true if successful.
     */
    public boolean makeHole(Point origin, Vect dir,
			    float baseDiam, float topDiam)
	throws RemoteException;

    // FUTURE
    // shootBundle ()
    // shoots a bundle of rays and returns plates?
    // To support homogenization.
}
