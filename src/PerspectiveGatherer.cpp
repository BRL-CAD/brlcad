#include "PerspectiveGatherer.h"

std::map<RenderingFace, FaceDetails> getFaceDetails()
{
	std::map<RenderingFace, FaceDetails> output;

	output[FRONT] = { "Front", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_front.png", LENGTH, HEIGHT };
	output[TOP] = { "Top", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_top.png", LENGTH, DEPTH };
	output[RIGHT] = { "Right", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_right.png", DEPTH, HEIGHT };
	output[LEFT] = { "Left", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_left.png", DEPTH, HEIGHT };
	output[BACK] = { "Back", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_back.png", LENGTH, HEIGHT };
	output[BOTTOM] = { "Bottom", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_bottom.png", LENGTH, DEPTH };
	output[BACK_MIRRORED] = { "Back-Mirrored", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_back_mirrored.png", LENGTH, HEIGHT };
	output[BOTTOM_MIRRORED] = { "Bottom-Mirrored", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ortho_bottom_mirrored.png", LENGTH, DEPTH };
	output[DETAILED] = { "Ambient Occlusion", "D:\\Mark\\ProgrammingWorkspace\\brlcad\\buildapple\\src\\gtools\\rgen\\visualization\\src\\ambient_occ.png" };

	return output;
}

std::string renderPerspective(RenderingFace face)
{
	return getFaceDetails()[face].file_path;
}