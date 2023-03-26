#include "PerspectiveGatherer.h"

std::map<char, FaceDetails> getFaceDetails()
{
	std::map<char, FaceDetails> output;

	output['0'] = {FRONT, "Front", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_front.png", LENGTH, HEIGHT};
	output['1'] = {TOP, "Top", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_top.png", LENGTH, DEPTH};
	output['2'] = {RIGHT, "Right", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_right.png", DEPTH, HEIGHT};
	output['3'] = {LEFT, "Left", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_left.png", DEPTH, HEIGHT};
	output['4'] = {BACK, "Back", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_back.png", LENGTH, HEIGHT};
	output['5'] = {BOTTOM, "Bottom", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_bottom.png", LENGTH, DEPTH};
	output['6'] = {BACK_MIRRORED, "Back-Mirrored", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_back_mirrored.png", LENGTH, HEIGHT};
	output['7'] = {BOTTOM_MIRRORED, "Bottom-Mirrored", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_bottom_mirrored.png", LENGTH, DEPTH};
	output['8'] = {DETAILED, "Ambient Occlusion", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ambient_occ.png"};

	return output;
}

std::string renderPerspective(RenderingFace face)
{
	std::map<char, FaceDetails> faceDetails = getFaceDetails();
	for (std::pair<char, FaceDetails> x : faceDetails)
		if (x.second.face == face)
			return x.second.file_path;
}