/*              S I M _ A N I M A T E . C P P
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
/** @file sim_animate.cpp
 *
 * Animation generation for the simulate GED command.  After each
 * simulation step the caller invokes SimAnimState::renderFrame() to
 * raytrace the current database state into a temporary PIX file.
 * When the simulation finishes, SimAnimState::encodeVideo() reads
 * the frames back through libicv, JPEG-encodes them, and writes a
 * standard MJPEG AVI file.
 *
 * If libjpeg was not available at build time the encoder falls back
 * to writing a sequence of numbered PNG files next to the output
 * path, printing a diagnostic that explains how to assemble the
 * video manually.
 */

#include "common.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bio.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "icv.h"

#include "rt/calc.h"
#include "rt/db_io.h"
#include "ged.h"

#include "sim_animate.hpp"


/* ------------------------------------------------------------------ */
/* RIFF/AVI MJPEG writer (only needed when libjpeg is available)       */
/* ------------------------------------------------------------------ */

namespace
{

/* Threshold below which a vector length is considered zero. */
static const double VEC_EPSILON = 1e-10;

/* Default chase-camera parameters (matching render_frames.py convention). */
static const double CHASE_AZ_DEG  = 225.0;  /**< Azimuth in degrees. */
static const double CHASE_EL_DEG  =  35.0;  /**< Elevation in degrees. */
static const double CHASE_DIST_MM = 35000.0; /**< Camera-to-subject distance in mm. */

/* rt process timeout in seconds (5 minutes per frame). */
static const int RT_TIMEOUT_SECONDS = 300;

#ifdef HAVE_JPEGLIB_H

/* Write a 4-byte little-endian uint32 to a file. */
static void
avi_write_u32(FILE *fp, uint32_t v)
{
    unsigned char buf[4];
    buf[0] = (unsigned char)(v & 0xFF);
    buf[1] = (unsigned char)((v >>  8) & 0xFF);
    buf[2] = (unsigned char)((v >> 16) & 0xFF);
    buf[3] = (unsigned char)((v >> 24) & 0xFF);
    fwrite(buf, 1, 4, fp);
}

/* Write a 2-byte little-endian uint16 to a file. */
static void
avi_write_u16(FILE *fp, uint16_t v)
{
    unsigned char buf[2];
    buf[0] = (unsigned char)(v & 0xFF);
    buf[1] = (unsigned char)((v >> 8) & 0xFF);
    fwrite(buf, 1, 2, fp);
}

/* Write an ASCII FourCC (4 bytes, no NUL). */
static void
avi_write_cc(FILE *fp, const char *cc)
{
    fwrite(cc, 1, 4, fp);
}

/* Seek to @p pos and patch a previously-written size field. */
static void
avi_patch_size(FILE *fp, long size_pos, uint32_t size)
{
    long save = ftell(fp);
    fseek(fp, size_pos, SEEK_SET);
    avi_write_u32(fp, size);
    fseek(fp, save, SEEK_SET);
}

/**
 * Write a minimal MJPEG AVI file.
 *
 * @p frames  vector of raw JPEG byte buffers (one per frame)
 * @p width   frame width in pixels
 * @p height  frame height in pixels
 * @p fps     frames per second
 * @p out_path output file path
 */
static int
write_mjpeg_avi(const std::vector<std::vector<unsigned char> > &frames,
		int width, int height, int fps,
		const std::string &out_path)
{
    FILE *fp = fopen(out_path.c_str(), "wb");
    if (!fp) {
	bu_log("simulate: cannot open '%s' for writing\n", out_path.c_str());
	return BRLCAD_ERROR;
    }

    uint32_t num_frames = (uint32_t)frames.size();
    uint32_t usec_per_frame = (fps > 0) ? (1000000u / (uint32_t)fps) : 41667u;

    /* Compute max frame size for buffer-size hints */
    uint32_t max_frame_bytes = 0;
    for (size_t i = 0; i < frames.size(); ++i) {
	if ((uint32_t)frames[i].size() > max_frame_bytes)
	    max_frame_bytes = (uint32_t)frames[i].size();
    }

    /* ----- RIFF 'AVI ' header ---------------------------------------- */
    avi_write_cc(fp, "RIFF");
    long riff_size_pos = ftell(fp);
    avi_write_u32(fp, 0); /* patched later */
    avi_write_cc(fp, "AVI ");

    /* ----- LIST 'hdrl' ----------------------------------------------- */
    avi_write_cc(fp, "LIST");
    long hdrl_size_pos = ftell(fp);
    avi_write_u32(fp, 0); /* patched later */
    long hdrl_start = ftell(fp);
    avi_write_cc(fp, "hdrl");

    /* 'avih' - AVI main header (56 bytes of data) */
    avi_write_cc(fp, "avih");
    avi_write_u32(fp, 56); /* chunk data size */
    avi_write_u32(fp, usec_per_frame);  /* dwMicroSecPerFrame */
    avi_write_u32(fp, max_frame_bytes * (uint32_t)fps); /* dwMaxBytesPerSec */
    avi_write_u32(fp, 0);              /* dwPaddingGranularity */
    avi_write_u32(fp, 0x10);           /* dwFlags: AVIF_HASINDEX=0x10 */
    avi_write_u32(fp, num_frames);     /* dwTotalFrames */
    avi_write_u32(fp, 0);              /* dwInitialFrames */
    avi_write_u32(fp, 1);              /* dwStreams */
    avi_write_u32(fp, max_frame_bytes);/* dwSuggestedBufferSize */
    avi_write_u32(fp, (uint32_t)width);  /* dwWidth */
    avi_write_u32(fp, (uint32_t)height); /* dwHeight */
    avi_write_u32(fp, 0); /* dwReserved[0] */
    avi_write_u32(fp, 0); /* dwReserved[1] */
    avi_write_u32(fp, 0); /* dwReserved[2] */
    avi_write_u32(fp, 0); /* dwReserved[3] */

    /* LIST 'strl' - stream list */
    avi_write_cc(fp, "LIST");
    long strl_size_pos = ftell(fp);
    avi_write_u32(fp, 0); /* patched later */
    long strl_start = ftell(fp);
    avi_write_cc(fp, "strl");

    /* 'strh' - stream header (56 bytes of data) */
    avi_write_cc(fp, "strh");
    avi_write_u32(fp, 56); /* chunk data size */
    avi_write_cc(fp, "vids"); /* fccType */
    avi_write_cc(fp, "MJPG"); /* fccHandler */
    avi_write_u32(fp, 0);    /* dwFlags */
    avi_write_u16(fp, 0);    /* wPriority */
    avi_write_u16(fp, 0);    /* wLanguage */
    avi_write_u32(fp, 0);    /* dwInitialFrames */
    avi_write_u32(fp, 1);    /* dwScale */
    avi_write_u32(fp, (uint32_t)fps); /* dwRate */
    avi_write_u32(fp, 0);    /* dwStart */
    avi_write_u32(fp, num_frames); /* dwLength */
    avi_write_u32(fp, max_frame_bytes); /* dwSuggestedBufferSize */
    avi_write_u32(fp, (uint32_t)-1); /* dwQuality (-1 = default) */
    avi_write_u32(fp, 0);    /* dwSampleSize */
    avi_write_u16(fp, 0);    /* rcFrame.left */
    avi_write_u16(fp, 0);    /* rcFrame.top */
    avi_write_u16(fp, (uint16_t)width);  /* rcFrame.right */
    avi_write_u16(fp, (uint16_t)height); /* rcFrame.bottom */

    /* 'strf' - stream format (BITMAPINFOHEADER, 40 bytes) */
    avi_write_cc(fp, "strf");
    avi_write_u32(fp, 40); /* chunk data size */
    avi_write_u32(fp, 40); /* biSize */
    avi_write_u32(fp, (uint32_t)width);  /* biWidth */
    /* Positive height = bottom-up DIB; our JPEG frames are already
     * top-to-bottom, but most MJPEG decoders ignore this field and
     * rely on the JPEG SOF marker, so positive is safe here. */
    avi_write_u32(fp, (uint32_t)height); /* biHeight */
    avi_write_u16(fp, 1);   /* biPlanes */
    avi_write_u16(fp, 24);  /* biBitCount */
    avi_write_cc(fp, "MJPG"); /* biCompression */
    avi_write_u32(fp, (uint32_t)(width * height * 3)); /* biSizeImage */
    avi_write_u32(fp, 0);   /* biXPelsPerMeter */
    avi_write_u32(fp, 0);   /* biYPelsPerMeter */
    avi_write_u32(fp, 0);   /* biClrUsed */
    avi_write_u32(fp, 0);   /* biClrImportant */

    /* Patch strl size */
    long strl_end = ftell(fp);
    avi_patch_size(fp, strl_size_pos, (uint32_t)(strl_end - strl_start));

    /* Patch hdrl size */
    long hdrl_end = ftell(fp);
    avi_patch_size(fp, hdrl_size_pos, (uint32_t)(hdrl_end - hdrl_start));

    /* ----- LIST 'movi' ----------------------------------------------- */
    avi_write_cc(fp, "LIST");
    long movi_size_pos = ftell(fp);
    avi_write_u32(fp, 0); /* patched later */
    long movi_start = ftell(fp);
    avi_write_cc(fp, "movi");

    /* Track frame offsets relative to movi_start+4 for the index */
    std::vector<uint32_t> frame_offsets;
    std::vector<uint32_t> frame_sizes;

    for (size_t i = 0; i < frames.size(); ++i) {
	const std::vector<unsigned char> &jpeg = frames[i];
	uint32_t data_size = (uint32_t)jpeg.size();
	/* Pad to even byte boundary as required by AVI spec */
	uint32_t padded = (data_size + 1) & ~1u;

	/* Offset = position of this chunk's data from movi_start+4 */
	frame_offsets.push_back((uint32_t)(ftell(fp) - movi_start - 4));

	avi_write_cc(fp, "00dc"); /* video frame chunk */
	avi_write_u32(fp, data_size);
	fwrite(jpeg.data(), 1, data_size, fp);
	if (padded != data_size)
	    fputc(0, fp); /* padding byte */

	frame_sizes.push_back(data_size);
    }

    /* Patch movi size */
    long movi_end = ftell(fp);
    avi_patch_size(fp, movi_size_pos, (uint32_t)(movi_end - movi_start));

    /* ----- 'idx1' - legacy AVI index ---------------------------------- */
    avi_write_cc(fp, "idx1");
    avi_write_u32(fp, num_frames * 16u); /* 16 bytes per entry */

    for (uint32_t i = 0; i < num_frames; ++i) {
	avi_write_cc(fp, "00dc");
	avi_write_u32(fp, 0x10); /* AVIIF_KEYFRAME */
	avi_write_u32(fp, frame_offsets[i]);
	avi_write_u32(fp, frame_sizes[i]);
    }

    /* Patch root RIFF size */
    long file_end = ftell(fp);
    avi_patch_size(fp, riff_size_pos, (uint32_t)(file_end - 8));

    fclose(fp);
    return BRLCAD_OK;
}

#endif /* HAVE_JPEGLIB_H */


/* ------------------------------------------------------------------ */
/* Camera mathematics                                                  */
/* ------------------------------------------------------------------ */

/**
 * Convert azimuth/elevation angles (degrees) and a chase distance
 * into a Cartesian offset vector.
 *
 * Convention matches render_frames.py:
 *   x = dist * cos(el) * cos(az)
 *   y = dist * cos(el) * sin(az)
 *   z = dist * sin(el)
 */
static void
az_el_to_offset(double az_deg, double el_deg, double dist,
		double off[3])
{
    const double deg2rad = M_PI / 180.0;
    double az = az_deg * deg2rad;
    double el = el_deg * deg2rad;
    off[0] = dist * std::cos(el) * std::cos(az);
    off[1] = dist * std::cos(el) * std::sin(az);
    off[2] = dist * std::sin(el);
}

/**
 * Build a view orientation quaternion so that the camera at @p eye
 * looks toward @p look_at with world-up (0,0,1).
 *
 * Produces the quaternion (x,y,z,w) accepted by rt's
 * "-c orientation ..." option.
 *
 * The math mirrors render_frames.py's camera_quaternion().
 */
static void
look_at_quaternion(const double eye[3], const double look_at[3],
		   double quat_out[4])
{
    /* forward = normalize(look_at - eye) */
    double fwd[3];
    fwd[0] = look_at[0] - eye[0];
    fwd[1] = look_at[1] - eye[1];
    fwd[2] = look_at[2] - eye[2];
    double flen = std::sqrt(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]);
    if (flen < VEC_EPSILON) {
	quat_out[0] = quat_out[1] = quat_out[2] = 0.0;
	quat_out[3] = 1.0;
	return;
    }
    fwd[0] /= flen; fwd[1] /= flen; fwd[2] /= flen;

    /* world up */
    double up_w[3] = {0.0, 0.0, 1.0};

    /* right = normalize(fwd x up_w) */
    double right[3];
    right[0] = fwd[1]*up_w[2] - fwd[2]*up_w[1];
    right[1] = fwd[2]*up_w[0] - fwd[0]*up_w[2];
    right[2] = fwd[0]*up_w[1] - fwd[1]*up_w[0];
    double rlen = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    if (rlen < VEC_EPSILON) {
	right[0] = 1.0; right[1] = 0.0; right[2] = 0.0;
    } else {
	right[0] /= rlen; right[1] /= rlen; right[2] /= rlen;
    }

    /* up = right x fwd  (recomputed for orthogonality) */
    double up[3];
    up[0] = right[1]*fwd[2] - right[2]*fwd[1];
    up[1] = right[2]*fwd[0] - right[0]*fwd[2];
    up[2] = right[0]*fwd[1] - right[1]*fwd[0];

    /* Build rotation matrix R with rows:
     *   row0 = right
     *   row1 = up
     *   row2 = -fwd   (camera looks along -Z in BRL-CAD convention)
     *
     * Same layout as quat_mat2quat expects (column-major, 4x4 with
     * homogeneous row/col):  we use the 3x3 sub-matrix approach.
     *
     *  R = | right[0]  right[1]  right[2] |
     *      |   up[0]    up[1]    up[2]    |
     *      |  -fwd[0]  -fwd[1]  -fwd[2]  |
     *
     * Convert to quaternion via Shepperd's method:
     */
    double m00 = right[0], m01 = right[1], m02 = right[2];
    double m10 = up[0],    m11 = up[1],    m12 = up[2];
    double m20 = -fwd[0],  m21 = -fwd[1],  m22 = -fwd[2];

    double trace = m00 + m11 + m22;
    double qx, qy, qz, qw;

    if (trace > 0.0) {
	double s = 0.5 / std::sqrt(trace + 1.0);
	qw = 0.25 / s;
	qx = (m21 - m12) * s;
	qy = (m02 - m20) * s;
	qz = (m10 - m01) * s;
    } else if (m00 > m11 && m00 > m22) {
	double s = 2.0 * std::sqrt(1.0 + m00 - m11 - m22);
	qw = (m21 - m12) / s;
	qx = 0.25 * s;
	qy = (m01 + m10) / s;
	qz = (m02 + m20) / s;
    } else if (m11 > m22) {
	double s = 2.0 * std::sqrt(1.0 + m11 - m00 - m22);
	qw = (m02 - m20) / s;
	qx = (m01 + m10) / s;
	qy = 0.25 * s;
	qz = (m12 + m21) / s;
    } else {
	double s = 2.0 * std::sqrt(1.0 + m22 - m00 - m11);
	qw = (m10 - m01) / s;
	qx = (m02 + m20) / s;
	qy = (m12 + m21) / s;
	qz = 0.25 * s;
    }

    quat_out[0] = qx;
    quat_out[1] = qy;
    quat_out[2] = qz;
    quat_out[3] = qw;
}


/**
 * Compute the view orientation quaternion for a camera at azimuth/
 * elevation (degrees) looking toward the origin.  Useful for
 * precomputing a fixed chase-camera orientation.
 */
static void
ae_to_quaternion(double az_deg, double el_deg, double quat_out[4])
{
    double off[3];
    az_el_to_offset(az_deg, el_deg, 1.0, off);
    const double origin[3] = {0.0, 0.0, 0.0};
    look_at_quaternion(off, origin, quat_out);
}


} /* anonymous namespace */


/* ------------------------------------------------------------------ */
/* SimAnimOptions                                                      */
/* ------------------------------------------------------------------ */

namespace simulate
{


SimAnimOptions::SimAnimOptions()
    : width(1280), height(720), fps(24), jpeg_quality(85),
      has_view_quat(false), has_view_ae(false),
      has_view_eye(false), has_view_center(false),
      has_view_size(false), view_size(15000.0)
{
    view_quat[0] = view_quat[1] = view_quat[2] = 0.0; view_quat[3] = 1.0;
    view_ae[0] = 225.0; view_ae[1] = 35.0;
    view_eye[0] = view_eye[1] = view_eye[2] = 0.0;
    view_center[0] = view_center[1] = view_center[2] = 0.0;
}


/* ------------------------------------------------------------------ */
/* SimAnimState                                                        */
/* ------------------------------------------------------------------ */

SimAnimState::SimAnimState(const SimAnimOptions &opts,
			   struct ged *gedp,
			   const std::string &scene_path)
    : m_opts(opts), m_gedp(gedp), m_scene_path(scene_path),
      m_frame_count(0)
{
    /* Create a temporary directory for PIX frames, using the process ID
     * to avoid collisions between concurrent runs. */
    char tmpbase[MAXPATHLEN];
    bu_dir(tmpbase, MAXPATHLEN, BU_DIR_TEMP, NULL);
    std::ostringstream dss;
    dss << std::string(tmpbase) << "/sim_frames_" << (long)bu_pid();
    m_tmpdir = dss.str();
    bu_mkdir(m_tmpdir.c_str());
}


SimAnimState::~SimAnimState()
{
    /* Clean up temporary PIX files */
    for (int i = 1; i <= m_frame_count; ++i) {
	std::string ppath = framePixPath(i);
	if (bu_file_exists(ppath.c_str(), NULL))
	    bu_file_delete(ppath.c_str());
    }
    /* Try to remove the temp directory (succeeds only if empty) */
    bu_file_delete(m_tmpdir.c_str());
}


std::string
SimAnimState::framePixPath(int frame_num) const
{
    std::ostringstream ss;
    ss << m_tmpdir << "/frame_" << std::setfill('0') << std::setw(4)
       << frame_num << ".pix";
    return ss.str();
}


int
SimAnimState::getSceneCenter(double center[3]) const
{
    /* Use rt_bound_internal to compute the axis-aligned bounding box
     * of the scene object, then take its midpoint as the center. */
    struct directory *dp = db_lookup(m_gedp->dbip,
				     m_scene_path.c_str(), LOOKUP_QUIET);
    if (!dp) {
	center[0] = center[1] = center[2] = 0.0;
	return BRLCAD_ERROR;
    }

    point_t rpp_min, rpp_max;
    if (rt_bound_internal(m_gedp->dbip, dp, rpp_min, rpp_max) < 0) {
	center[0] = center[1] = center[2] = 0.0;
	return BRLCAD_ERROR;
    }

    center[0] = 0.5 * (rpp_min[X] + rpp_max[X]);
    center[1] = 0.5 * (rpp_min[Y] + rpp_max[Y]);
    center[2] = 0.5 * (rpp_min[Z] + rpp_max[Z]);
    return BRLCAD_OK;
}


void
SimAnimState::computeChaseCamera(const double center[3],
				 double eye_out[3],
				 double quat_out[4]) const
{
    /* Default chase-camera parameters matching render_frames.py */
    const double AZ  = CHASE_AZ_DEG;
    const double EL  = CHASE_EL_DEG;
    const double DIST = CHASE_DIST_MM;

    double off[3];
    az_el_to_offset(AZ, EL, DIST, off);

    eye_out[0] = center[0] + off[0];
    eye_out[1] = center[1] + off[1];
    eye_out[2] = center[2] + off[2];

    /* Orientation: looking from the offset direction toward origin */
    ae_to_quaternion(AZ, EL, quat_out);
}


int
SimAnimState::renderFrame(int frame_num)
{
    if (m_opts.output_file.empty())
	return BRLCAD_OK; /* animation not requested */

    /* --------------------------------------------------------------- */
    /* Determine camera parameters                                      */
    /* --------------------------------------------------------------- */

    double eye[3]    = {0.0, 0.0, 0.0};
    double quat[4]   = {0.0, 0.0, 0.0, 1.0};
    double vsize     = m_opts.view_size;
    double center[3] = {0.0, 0.0, 0.0};

    bool fixed_view = (m_opts.has_view_quat || m_opts.has_view_ae);

    if (!fixed_view) {
	/* Chase camera: compute eye from scene center */
	getSceneCenter(center);

	if (m_opts.has_view_center) {
	    center[0] = m_opts.view_center[0];
	    center[1] = m_opts.view_center[1];
	    center[2] = m_opts.view_center[2];
	}

	if (m_opts.has_view_eye) {
	    eye[0] = m_opts.view_eye[0];
	    eye[1] = m_opts.view_eye[1];
	    eye[2] = m_opts.view_eye[2];
	    ae_to_quaternion(225.0, 35.0, quat); /* default orientation */
	} else {
	    computeChaseCamera(center, eye, quat);
	}

    } else {
	/* Fixed view: use user-supplied orientation */
	if (m_opts.has_view_quat) {
	    quat[0] = m_opts.view_quat[0];
	    quat[1] = m_opts.view_quat[1];
	    quat[2] = m_opts.view_quat[2];
	    quat[3] = m_opts.view_quat[3];
	} else {
	    /* has_view_ae */
	    ae_to_quaternion(m_opts.view_ae[0], m_opts.view_ae[1], quat);
	}

	if (m_opts.has_view_eye) {
	    eye[0] = m_opts.view_eye[0];
	    eye[1] = m_opts.view_eye[1];
	    eye[2] = m_opts.view_eye[2];
	} else {
	    /* No explicit eye: compute from scene center + az/el offset */
	    getSceneCenter(center);
	    if (m_opts.has_view_center) {
		center[0] = m_opts.view_center[0];
		center[1] = m_opts.view_center[1];
		center[2] = m_opts.view_center[2];
	    }
	    double az = m_opts.has_view_ae ? m_opts.view_ae[0] : CHASE_AZ_DEG;
	    double el = m_opts.has_view_ae ? m_opts.view_ae[1] : CHASE_EL_DEG;
	    double off[3];
	    az_el_to_offset(az, el, CHASE_DIST_MM, off);
	    eye[0] = center[0] + off[0];
	    eye[1] = center[1] + off[1];
	    eye[2] = center[2] + off[2];
	}

	if (m_opts.has_view_center) {
	    center[0] = m_opts.view_center[0];
	    center[1] = m_opts.view_center[1];
	    center[2] = m_opts.view_center[2];
	}
    }

    if (m_opts.has_view_size)
	vsize = m_opts.view_size;

    /* --------------------------------------------------------------- */
    /* Build rt argument list                                           */
    /* --------------------------------------------------------------- */

    std::string pix_path = framePixPath(frame_num);

    char rt_path[MAXPATHLEN];
    bu_dir(rt_path, MAXPATHLEN, BU_DIR_BIN, "rt", BU_DIR_EXT, NULL);

    /* Format floating-point values for -c strings */
    std::ostringstream viewsize_str, eyept_str, orient_str;
    viewsize_str << "viewsize " << std::fixed << std::setprecision(6) << vsize;
    eyept_str    << "eye_pt "
		 << std::fixed << std::setprecision(6)
		 << eye[0] << " " << eye[1] << " " << eye[2];
    orient_str   << "orientation "
		 << std::fixed << std::setprecision(10)
		 << quat[0] << " " << quat[1] << " "
		 << quat[2] << " " << quat[3];

    std::ostringstream w_str, n_str;
    w_str << m_opts.width;
    n_str << m_opts.height;

    std::vector<const char *> rt_args;
    rt_args.push_back(rt_path);
    rt_args.push_back("-w");  rt_args.push_back(w_str.str().c_str());
    rt_args.push_back("-n");  rt_args.push_back(n_str.str().c_str());
    rt_args.push_back("-c");  rt_args.push_back(viewsize_str.str().c_str());
    rt_args.push_back("-c");  rt_args.push_back(eyept_str.str().c_str());
    rt_args.push_back("-c");  rt_args.push_back(orient_str.str().c_str());
    rt_args.push_back("-o");  rt_args.push_back(pix_path.c_str());
    rt_args.push_back(m_gedp->dbip->dbi_filename);
    rt_args.push_back(m_scene_path.c_str());
    rt_args.push_back(NULL);

    /* --------------------------------------------------------------- */
    /* Spawn rt and wait for completion                                 */
    /* --------------------------------------------------------------- */

    /* Store string values so pointers remain valid */
    std::string w_s    = w_str.str();
    std::string n_s    = n_str.str();
    std::string vs_s   = viewsize_str.str();
    std::string ep_s   = eyept_str.str();
    std::string or_s   = orient_str.str();

    rt_args.clear();
    rt_args.push_back(rt_path);
    rt_args.push_back("-w");  rt_args.push_back(w_s.c_str());
    rt_args.push_back("-n");  rt_args.push_back(n_s.c_str());
    rt_args.push_back("-c");  rt_args.push_back(vs_s.c_str());
    rt_args.push_back("-c");  rt_args.push_back(ep_s.c_str());
    rt_args.push_back("-c");  rt_args.push_back(or_s.c_str());
    rt_args.push_back("-o");  rt_args.push_back(pix_path.c_str());
    rt_args.push_back(m_gedp->dbip->dbi_filename);
    rt_args.push_back(m_scene_path.c_str());
    rt_args.push_back(NULL);

    struct bu_process *proc = NULL;
    bu_process_create(&proc, rt_args.data(), BU_PROCESS_DEFAULT);
    if (!proc) {
	bu_log("simulate: failed to launch rt for frame %d\n", frame_num);
	return BRLCAD_ERROR;
    }

    int rc = bu_process_wait_n(&proc, RT_TIMEOUT_SECONDS);
    if (rc != 0) {
	bu_log("simulate: rt exited with code %d for frame %d\n",
	       rc, frame_num);
	return BRLCAD_ERROR;
    }

    ++m_frame_count;
    return BRLCAD_OK;
}


int
SimAnimState::readPixToJpeg(const std::string &pix_path,
			    std::vector<unsigned char> &jpeg_data) const
{
#ifdef HAVE_JPEGLIB_H
    /* Read the PIX file into an icv_image */
    icv_image_t *img = icv_read(pix_path.c_str(), BU_MIME_IMAGE_PIX,
				(size_t)m_opts.width, (size_t)m_opts.height);
    if (!img) {
	bu_log("simulate: failed to read PIX frame '%s'\n", pix_path.c_str());
	return BRLCAD_ERROR;
    }

    /* Write JPEG to a named temporary file, then read the bytes back */
    char tmp_jpg[MAXPATHLEN];
    bu_dir(tmp_jpg, MAXPATHLEN, BU_DIR_TEMP, NULL);
    std::ostringstream tss;
    tss << std::string(tmp_jpg) << "/sim_jpgtmp_" << (long)bu_pid() << ".jpg";
    std::string tmp_jpg_path = tss.str();

    if (icv_write(img, tmp_jpg_path.c_str(), BU_MIME_IMAGE_JPEG) != BRLCAD_OK) {
	icv_destroy(img);
	return BRLCAD_ERROR;
    }
    icv_destroy(img);

    FILE *rfp = fopen(tmp_jpg_path.c_str(), "rb");
    if (!rfp) {
	bu_file_delete(tmp_jpg_path.c_str());
	return BRLCAD_ERROR;
    }
    fseek(rfp, 0, SEEK_END);
    long jsize = ftell(rfp);
    fseek(rfp, 0, SEEK_SET);
    if (jsize > 0) {
	jpeg_data.resize((size_t)jsize);
	size_t nr = fread(jpeg_data.data(), 1, (size_t)jsize, rfp);
	if ((long)nr != jsize)
	    jpeg_data.clear();
    }
    fclose(rfp);
    bu_file_delete(tmp_jpg_path.c_str());

    return jpeg_data.empty() ? BRLCAD_ERROR : BRLCAD_OK;

#else
    (void)pix_path;
    jpeg_data.clear();
    return BRLCAD_ERROR;
#endif
}


int
SimAnimState::encodeVideo()
{
    if (m_opts.output_file.empty() || m_frame_count == 0)
	return BRLCAD_OK;

#ifdef HAVE_JPEGLIB_H
    /* --------------------------------------------------------------- */
    /* JPEG-encode every PIX frame                                      */
    /* --------------------------------------------------------------- */
    std::vector<std::vector<unsigned char> > jpeg_frames;
    jpeg_frames.reserve((size_t)m_frame_count);

    for (int i = 1; i <= m_frame_count; ++i) {
	std::string ppath = framePixPath(i);
	if (!bu_file_exists(ppath.c_str(), NULL)) {
	    bu_log("simulate: missing frame file '%s', skipping\n",
		   ppath.c_str());
	    continue;
	}

	/* Read PIX and encode to JPEG */
	icv_image_t *img = icv_read(ppath.c_str(), BU_MIME_IMAGE_PIX,
				    (size_t)m_opts.width,
				    (size_t)m_opts.height);
	if (!img) {
	    bu_log("simulate: could not read frame '%s'\n", ppath.c_str());
	    continue;
	}

	/* Write to a temp file and read back as bytes */
	char tmp_jpg[MAXPATHLEN];
	bu_dir(tmp_jpg, MAXPATHLEN, BU_DIR_TEMP, NULL);
	std::ostringstream tss;
	tss << std::string(tmp_jpg) << "/sim_jpgtmp_"
	    << std::setfill('0') << std::setw(4) << i << ".jpg";
	std::string tmp_jpg_path = tss.str();

	if (icv_write(img, tmp_jpg_path.c_str(), BU_MIME_IMAGE_JPEG) != BRLCAD_OK) {
	    icv_destroy(img);
	    bu_log("simulate: JPEG encode failed for frame %d\n", i);
	    continue;
	}
	icv_destroy(img);

	/* Read the JPEG bytes */
	FILE *jfp = fopen(tmp_jpg_path.c_str(), "rb");
	if (!jfp) {
	    bu_file_delete(tmp_jpg_path.c_str());
	    continue;
	}
	fseek(jfp, 0, SEEK_END);
	long jsize = ftell(jfp);
	fseek(jfp, 0, SEEK_SET);

	std::vector<unsigned char> buf;
	if (jsize > 0) {
	    buf.resize((size_t)jsize);
	    if ((long)fread(buf.data(), 1, (size_t)jsize, jfp) != jsize)
		buf.clear();
	}
	fclose(jfp);
	bu_file_delete(tmp_jpg_path.c_str());

	if (!buf.empty())
	    jpeg_frames.push_back(buf);
    }

    if (jpeg_frames.empty()) {
	bu_log("simulate: no frames to encode\n");
	return BRLCAD_ERROR;
    }

    return write_mjpeg_avi(jpeg_frames, m_opts.width, m_opts.height,
			   m_opts.fps, m_opts.output_file);

#else
    /* --------------------------------------------------------------- */
    /* No JPEG support: fall back to writing numbered PNG files         */
    /* --------------------------------------------------------------- */
    bu_log("simulate: libjpeg not available; writing PNG frames instead.\n");

    /* Derive output base name by stripping extension */
    std::string base = m_opts.output_file;
    size_t dot = base.rfind('.');
    if (dot != std::string::npos)
	base = base.substr(0, dot);

    int written = 0;
    for (int i = 1; i <= m_frame_count; ++i) {
	std::string ppath = framePixPath(i);
	if (!bu_file_exists(ppath.c_str(), NULL))
	    continue;

	icv_image_t *img = icv_read(ppath.c_str(), BU_MIME_IMAGE_PIX,
				    (size_t)m_opts.width,
				    (size_t)m_opts.height);
	if (!img)
	    continue;

	std::ostringstream pss;
	pss << base << "_" << std::setfill('0') << std::setw(4) << i << ".png";
	std::string png_path = pss.str();

	if (icv_write(img, png_path.c_str(), BU_MIME_IMAGE_PNG) == BRLCAD_OK)
	    ++written;

	icv_destroy(img);
    }

    bu_log("simulate: wrote %d PNG frame(s) with base name '%s_NNNN.png'.\n"
	   "  To create a video: ffmpeg -framerate %d -i '%s_%%04d.png' "
	   "-c:v libx264 '%s'\n",
	   written, base.c_str(), m_opts.fps, base.c_str(),
	   m_opts.output_file.c_str());

    return BRLCAD_OK;
#endif
}


} /* namespace simulate */


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
