/*              S I M _ A N I M A T E . H P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
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
/** @file sim_animate.hpp
 *
 * Animation generation support for the simulate GED command.
 * Renders a frame after each simulation step and assembles the
 * result into an MJPEG AVI file.
 */

#ifndef SIMULATE_SIM_ANIMATE_H
#define SIMULATE_SIM_ANIMATE_H

#include "common.h"

#include <string>
#include <vector>

#include "ged.h"


namespace simulate
{


/**
 * Options controlling animation rendering and encoding.
 */
struct SimAnimOptions {
    std::string output_file;   /**< @brief Output .avi file path (empty = no animation) */

    int width;                 /**< @brief Frame width in pixels */
    int height;                /**< @brief Frame height in pixels */
    int fps;                   /**< @brief Frames per second in output video */
    int jpeg_quality;          /**< @brief JPEG quality 1-100 for MJPEG encoding */

    /* Fixed camera view (mutually exclusive with chase camera) */
    bool has_view_quat;        /**< @brief Use explicit quaternion orientation */
    double view_quat[4];       /**< @brief Orientation quaternion (x,y,z,w) for rt */

    bool has_view_ae;          /**< @brief Use azimuth/elevation for orientation */
    double view_ae[2];         /**< @brief Azimuth and elevation in degrees */

    bool has_view_eye;         /**< @brief Override eye position */
    double view_eye[3];        /**< @brief Eye position in model coordinates (mm) */

    bool has_view_center;      /**< @brief Override view center */
    double view_center[3];     /**< @brief View center in model coordinates (mm) */

    bool has_view_size;        /**< @brief Override view size */
    double view_size;          /**< @brief View size (distance) in mm */

    SimAnimOptions();
};


/**
 * Manages per-simulation-run animation state: temporary frame files,
 * camera tracking, and final video assembly.
 */
class SimAnimState
{
public:
    explicit SimAnimState(const SimAnimOptions &opts,
			  struct ged *gedp,
			  const std::string &scene_path);
    ~SimAnimState();

    /** Render the current database state as frame @p frame_num (1-based). */
    int renderFrame(int frame_num);

    /** Encode all rendered frames into the output AVI file. */
    int encodeVideo();

private:
    SimAnimOptions m_opts;
    struct ged    *m_gedp;
    std::string    m_scene_path;    /**< @brief Primary scene object path */
    std::string    m_tmpdir;        /**< @brief Temporary directory for frame files */
    int            m_frame_count;

    std::string framePixPath(int frame_num) const;

    int getSceneCenter(double center[3]) const;
    void computeChaseCamera(const double center[3],
			    double eye_out[3],
			    double quat_out[4]) const;

    /* AVI MJPEG encoding helpers */
    int readPixToJpeg(const std::string &pix_path,
		      std::vector<unsigned char> &jpeg_data) const;
    int writeMjpegAvi(const std::vector<std::string> &frame_paths,
		      const std::string &out_path) const;
};


} /* namespace simulate */


#endif /* SIMULATE_SIM_ANIMATE_H */


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
