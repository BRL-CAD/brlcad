/*                          T I L E . H
 * BRL-CAD
 *
 * Copyright (c) 2022-2024 United States Government as represented by
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

#include "renderer/kernel/rendering/itilecallback.h"

class ArtTileCallback
   :public renderer::ITileCallback
{
    public:
	//
	// Methods called by tile-based renderers.
	//

	// This method is called before a frame is rendered.
	void on_tiled_frame_begin(const renderer::Frame*) {;}

	// This method is called after a frame is rendered.
	void on_tiled_frame_end(const renderer::Frame*) {;}

	// This method is called before a tile is rendered.
	void on_tile_begin(
		const renderer::Frame* frame,
		const size_t    tile_x,
		const size_t    tile_y)
	{;}
	// This method is called after a tile is rendered.
	void on_tile_end(
		const renderer::Frame*,           //frame
		const size_t,           //tile_x
		const size_t            //tile_y
		);

	// This method is called after the frame has been updated.
	void on_progressive_frame_update(const renderer::Frame* frame)
	{;}
	void release() {};
};

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
