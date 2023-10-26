#pragma once

#include "pch.h"
/**
 * The PerspectiveGatherer files contain utility methods for the RenderHandler class.
 * 
 * These methods predominantly feature different rendering types / perspectives.
 *
 */

class Options;

enum RenderingFace
{
	FRONT,
	TOP,
	RIGHT,
	LEFT,
	BACK,
	BOTTOM,
	DETAILED,
    GHOST
};

enum ModelDimension
{
	LENGTH,
	DEPTH,
	HEIGHT
};

struct FaceDetails
{
	RenderingFace face;
	std::string title;

	ModelDimension widthContributor;
	ModelDimension heightContributor;
};

std::map<char,FaceDetails> getFaceDetails();

// TODO: add correct parameters and return type
std::string renderPerspective(RenderingFace face, Options& opt, std::string component, std::string ghost="");
