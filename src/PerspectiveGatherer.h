#pragma once

#include "pch.h"

/**
 * The PerspectiveGatherer files contain utility methods for the RenderHandler class.
 * 
 * These methods predominantly feature different rendering types / perspectives.
 *
 */

enum RenderingFace
{
	FRONT,
	TOP,
	RIGHT,
	LEFT,
	BACK,
	BOTTOM
};

// TODO: add correct parameters and return type
std::string renderAmbientOcclusion();
std::string renderPerspective(RenderingFace face);