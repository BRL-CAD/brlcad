/*          G L T F _ D R A C O . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifndef ASSETIMPORT_GLTF_DRACO_H
#define ASSETIMPORT_GLTF_DRACO_H
#include "common.h"

/* Unit right triangle encoded with Draco 1.5.7, compression level 0 and
 * 14-bit positions. No uncompressed fallback can hide a missing decoder.
 * Shared by the runtime codec query and conversion regressions. */
inline constexpr char assetimport_draco_triangle[] = R"({
  "asset": {
    "version": "2.0"
  },
  "extensionsUsed": [
    "KHR_draco_mesh_compression"
  ],
  "extensionsRequired": [
    "KHR_draco_mesh_compression"
  ],
  "buffers": [
    {
      "byteLength": 66,
      "uri": "data:application/octet-stream;base64,RFJBQ08CAgEAAAABAwEAAQIBAQAJAwAAAgABAQADA60qVRUDoHqBiAEAAAAA/z8AAAAAAAAAAAAAAAAAAAAAgD8O"
    }
  ],
  "bufferViews": [
    {
      "buffer": 0,
      "byteOffset": 0,
      "byteLength": 66
    }
  ],
  "accessors": [
    {
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "min": [
        0,
        0,
        0
      ],
      "max": [
        1,
        1,
        0
      ]
    },
    {
      "componentType": 5123,
      "count": 3,
      "type": "SCALAR"
    }
  ],
  "meshes": [
    {
      "primitives": [
        {
          "attributes": {
            "POSITION": 0
          },
          "indices": 1,
          "mode": 4,
          "extensions": {
            "KHR_draco_mesh_compression": {
              "bufferView": 0,
              "attributes": {
                "POSITION": 0
              }
            }
          }
        }
      ]
    }
  ],
  "nodes": [
    {
      "mesh": 0
    }
  ],
  "scenes": [
    {
      "nodes": [
        0
      ]
    }
  ],
  "scene": 0
})";
#endif
