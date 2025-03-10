/*                    B O M B A R D I E R . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file util/bombardier.h
 *
 * interface header for the bombardier tool, provides an icon bitmap
 * for a startup dialog.
 *
 */

#ifndef UTIL_BOMBARDIER_H
#define UTIL_BOMBARDIER_H

#define bomb_icon_width 192
#define bomb_icon_height 56

static unsigned char bomb_icon_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
    0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
    0xe7, 0x01, 0x00, 0x00, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67,
    0xe6, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x01, 0x00, 0x00, 0x80, 0xcf,
    0x89, 0x1f, 0x00, 0x00, 0x80, 0xc3, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x3f,
    0xfc, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x87, 0x01, 0x00, 0x00, 0xf8, 0x91,
    0x35, 0xf0, 0xff, 0xff, 0xff, 0x15, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x79,
    0xde, 0x00, 0xc0, 0x13, 0x00, 0x00, 0xd8, 0xff, 0xff, 0xff, 0x0f, 0xa2,
    0x45, 0x45, 0x00, 0x80, 0x00, 0xa4, 0xad, 0x2a, 0x15, 0x07, 0x00, 0x63,
    0x86, 0x00, 0xe0, 0x88, 0x54, 0x55, 0x22, 0x80, 0x20, 0x00, 0x50, 0xcd,
    0x93, 0x2a, 0xdd, 0x2d, 0x55, 0x3b, 0x22, 0x40, 0x40, 0x0c, 0x00, 0x0f,
    0xf1, 0x00, 0x30, 0x22, 0x01, 0x90, 0xdc, 0x2a, 0x8d, 0xde, 0xa6, 0x90,
    0x27, 0xea, 0xfb, 0x5a, 0xad, 0xb6, 0x8c, 0x0a, 0x95, 0x09, 0x00, 0x5b,
    0xda, 0x00, 0x90, 0x20, 0x55, 0x37, 0x79, 0x55, 0xb9, 0xfd, 0x4f, 0xe5,
    0xaf, 0xf2, 0xdf, 0xff, 0xff, 0x6d, 0xb1, 0x6a, 0x25, 0x0a, 0x00, 0x73,
    0xce, 0x00, 0x30, 0x4a, 0x52, 0x60, 0x67, 0xff, 0xf7, 0xfb, 0x8b, 0xb4,
    0x4d, 0x04, 0x20, 0xdb, 0xd6, 0xec, 0x4f, 0x58, 0x0a, 0x0d, 0x00, 0xeb,
    0xc2, 0x00, 0xb0, 0xb4, 0x8d, 0x9e, 0xbb, 0xdf, 0xbd, 0x04, 0x68, 0xb9,
    0x79, 0xa9, 0x4a, 0x20, 0x21, 0xb9, 0xbe, 0xff, 0xdf, 0x08, 0x00, 0x53,
    0xee, 0x00, 0x10, 0xf9, 0x7f, 0xfb, 0x2d, 0x20, 0x42, 0xa2, 0x12, 0x9e,
    0xe5, 0x8a, 0x92, 0x0a, 0x4c, 0xea, 0xf4, 0xdb, 0x3d, 0x0b, 0xc0, 0x67,
    0xea, 0x03, 0x70, 0x35, 0xf5, 0x57, 0x4e, 0x85, 0x94, 0x4a, 0xa4, 0xd6,
    0xab, 0x3f, 0x25, 0x59, 0x91, 0x90, 0x09, 0xae, 0x33, 0x0e, 0x70, 0x6b,
    0xc7, 0x0e, 0x30, 0xed, 0x3d, 0x92, 0x15, 0xac, 0x54, 0x92, 0xf9, 0xc5,
    0x47, 0xff, 0x5b, 0xb3, 0x36, 0xff, 0x77, 0xff, 0xf7, 0x0c, 0x3c, 0x77,
    0xd6, 0x3c, 0x70, 0xee, 0x77, 0xf5, 0xff, 0x51, 0x89, 0xad, 0xff, 0xf0,
    0x3e, 0xd0, 0xff, 0xef, 0xff, 0xff, 0xdf, 0x01, 0x38, 0x07, 0x4e, 0xe3,
    0xce, 0x71, 0x60, 0x19, 0xc8, 0xef, 0xfb, 0xff, 0xff, 0xff, 0x0f, 0x5a,
    0xee, 0x01, 0xfa, 0xff, 0xef, 0x01, 0xf8, 0xab, 0x32, 0x07, 0xc7, 0x77,
    0xd7, 0xe7, 0xe0, 0x5a, 0x43, 0x1f, 0x82, 0xef, 0xff, 0x76, 0xd0, 0x7a,
    0x78, 0x17, 0x02, 0x00, 0x00, 0xfa, 0x7f, 0x28, 0x3a, 0x03, 0xfb, 0x63,
    0xce, 0xcb, 0xc0, 0x5c, 0x5a, 0xfe, 0x1e, 0x08, 0x00, 0x80, 0xb2, 0x3f,
    0xd8, 0xff, 0x41, 0x92, 0xd4, 0x7d, 0xab, 0xcc, 0x54, 0x83, 0xd1, 0xc7,
    0xe2, 0x9d, 0xc1, 0x9a, 0x14, 0xd7, 0xf7, 0xa3, 0x52, 0x55, 0xef, 0x1b,
    0xb0, 0xf8, 0xff, 0xff, 0xff, 0x8f, 0x7e, 0x1b, 0x3b, 0xc3, 0xfc, 0x7b,
    0xce, 0xbf, 0xc3, 0x18, 0x71, 0x7e, 0xe9, 0xff, 0xff, 0xff, 0x5f, 0x0d,
    0xf0, 0x25, 0xfe, 0xff, 0x6f, 0xf1, 0x7b, 0x57, 0x68, 0xc3, 0x5a, 0x63,
    0xd6, 0x3e, 0xc3, 0x3a, 0xe7, 0xfc, 0x2b, 0xfc, 0xfe, 0x6f, 0x50, 0x0e,
    0xc0, 0x6f, 0x00, 0x00, 0x48, 0xef, 0xc3, 0x61, 0x99, 0x63, 0xbe, 0x73,
    0xc7, 0x74, 0xc6, 0x5a, 0xc6, 0xc3, 0xdf, 0x81, 0x00, 0x48, 0xf7, 0x03,
    0x80, 0xfe, 0xaf, 0xd4, 0xf6, 0x1b, 0xf8, 0xbc, 0x3a, 0x67, 0x3b, 0x57,
    0xee, 0xfd, 0xe6, 0x2c, 0x1d, 0x0b, 0xf6, 0xaf, 0x25, 0xb1, 0x7e, 0x03,
    0x80, 0xd1, 0xdf, 0xbb, 0xbf, 0x46, 0xf7, 0x5b, 0x58, 0x7e, 0xcf, 0xf7,
    0x5e, 0xbb, 0x7c, 0x5b, 0xf8, 0xff, 0x48, 0xff, 0xff, 0xff, 0x95, 0x01,
    0x80, 0x17, 0xf9, 0xf7, 0x2a, 0xd8, 0x4f, 0x87, 0x73, 0x5c, 0x5d, 0x3f,
    0xf8, 0xf2, 0x3d, 0xdc, 0xe1, 0xfa, 0x2b, 0x54, 0xfd, 0xbf, 0xf5, 0x01,
    0x00, 0xbf, 0x42, 0x95, 0xc0, 0xfe, 0x15, 0xd7, 0x55, 0xb1, 0xd7, 0x1e,
    0xfa, 0xf3, 0x45, 0x4d, 0xe3, 0xc4, 0x7f, 0x15, 0x00, 0x40, 0x76, 0x00,
    0x00, 0xb8, 0x9f, 0x34, 0xfd, 0x5f, 0xf8, 0x6f, 0x71, 0xa2, 0xed, 0x3f,
    0xf4, 0xae, 0x1a, 0x86, 0xef, 0x1f, 0xfc, 0xbf, 0xeb, 0xbb, 0x3d, 0x00,
    0x00, 0xf0, 0xfe, 0xff, 0xff, 0x12, 0xff, 0x5d, 0xcc, 0x7a, 0xcf, 0x4e,
    0xf0, 0xf7, 0xed, 0x3b, 0x3c, 0xfb, 0x81, 0xed, 0x7f, 0xf7, 0x0f, 0x00,
    0x00, 0x80, 0xe9, 0xf7, 0x87, 0xe0, 0x0b, 0x7e, 0xde, 0xbf, 0xbe, 0x1f,
    0xf2, 0xc6, 0xfd, 0xf3, 0x7c, 0xb1, 0x1f, 0x69, 0xef, 0x9f, 0x01, 0x00,
    0x00, 0x80, 0x0f, 0x00, 0x24, 0xff, 0xe9, 0x73, 0x83, 0xbb, 0xe5, 0x37,
    0xe4, 0xfd, 0xad, 0xe4, 0xec, 0xc3, 0x7e, 0x40, 0x08, 0xf0, 0x01, 0x00,
    0x00, 0x00, 0x6e, 0x55, 0xc9, 0x2f, 0xd2, 0xec, 0x75, 0xb0, 0x6f, 0x0f,
    0xf0, 0xa7, 0x4d, 0x8d, 0x8f, 0x1f, 0xf5, 0x2f, 0x52, 0x77, 0x00, 0x00,
    0x00, 0x00, 0xbc, 0xaa, 0xfe, 0x55, 0x7c, 0xd9, 0x39, 0xb5, 0xd6, 0x57,
    0xe4, 0xeb, 0x9d, 0x1a, 0x3b, 0xbc, 0x88, 0xdf, 0x95, 0x3c, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0xff, 0x7f, 0x80, 0x1f, 0xcf, 0x9b, 0xbd, 0x6f, 0xff,
    0xff, 0x6b, 0xbd, 0xf9, 0xf7, 0x71, 0x21, 0xfc, 0xff, 0x0f, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x8f, 0xbd, 0xd3, 0x33, 0xaf, 0x76, 0xce, 0x7f,
    0xff, 0xf6, 0xb5, 0xf3, 0xe6, 0xe3, 0xcf, 0xe8, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xfc, 0xe3, 0xe2, 0xbc, 0xdb, 0x6c, 0xdb, 0x03,
    0xe8, 0x5b, 0x6e, 0xbb, 0x1d, 0x4f, 0xb7, 0x3f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xf0, 0xdf, 0x38, 0xd9, 0xfc, 0xd6, 0x3e, 0x27,
    0xd2, 0xf5, 0x27, 0x6e, 0x5b, 0x9a, 0xfc, 0x0f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x7e, 0xd4, 0xe7, 0xe7, 0x76, 0x7f,
    0xfe, 0xdd, 0xee, 0xef, 0x72, 0xbd, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x4d, 0x77, 0xa6, 0xff, 0x5e, 0x7f,
    0xfe, 0xba, 0xdf, 0x6b, 0x57, 0xb0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x93, 0x36, 0x6f, 0xbd, 0x75, 0x23,
    0xe0, 0xbd, 0xf5, 0xc6, 0xee, 0xd5, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x53, 0x33, 0xef, 0xfb, 0x07,
    0xd8, 0xd7, 0xbf, 0xdc, 0x98, 0xea, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5e, 0x9d, 0x73, 0x6d, 0xd7, 0x27,
    0xf2, 0xdf, 0xf6, 0x9d, 0x3b, 0x7b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xec, 0x5c, 0xcf, 0xbe, 0x2f,
    0xe4, 0x76, 0xd9, 0x58, 0x73, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x67, 0x9a, 0xdd, 0x67, 0x4f,
    0xe8, 0xbb, 0xb3, 0x33, 0xc6, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0xae, 0x89, 0xbd, 0x1f,
    0xf4, 0xfd, 0xbb, 0x69, 0xfd, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0xc6, 0x3d, 0x79, 0x36,
    0x78, 0x8f, 0xb1, 0xeb, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0xcb, 0x58, 0xf3, 0x4f,
    0xf0, 0xcb, 0x0e, 0xcb, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xea, 0x5d, 0xc7, 0x1f,
    0xf6, 0xe5, 0xb8, 0x53, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x61, 0x32, 0x0e, 0x4e,
    0x70, 0x71, 0x5a, 0xc6, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xef, 0xd8, 0xfc, 0x1f,
    0xfc, 0xe5, 0x1a, 0xdf, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x1b, 0xe9, 0x3d,
    0xb8, 0x8f, 0x5c, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xf7, 0x7f, 0x5c,
    0x3c, 0xfe, 0xbb, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x07, 0x38,
    0x3c, 0xe0, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x78,
    0x1e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
    0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
    0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0,
    0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#endif /* UTIL_BOMBARDIER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
