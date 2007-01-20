/*                  P A R T I T I O N . J A V A
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file Partition.java
 *
 */

package mil.army.arl.muves.geometry;

import java.io.Serializable;

/**
 * Replicates the "important" information from the librt partition
 * structure.
 */
public class Partition implements Serializable {
    String item;
    Point inHit;
    Point outHit;
    float obliquity;

    /**
     * Retrieves the obliquity value.
     *
     * @return    An obliquity value between 0 and PI/2.
     */
    public float getObliquity() {
        return obliquity;
    }

    /**
     * Sets the obliquity value.
     *
     * @param obl The obliquity value.
     */
    public void setObliquity(float obl) {
        this.obliquity = obl;
    }
}
