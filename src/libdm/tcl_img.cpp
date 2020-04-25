/*                     T C L _ I M G . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file tcl_img.cpp
 *
 * Self contained example of working with image data
 * in Tcl/Tk in C/C++
 *
 * Eventually this should probably turn into a Togl-esque
 * widget that we properly include, so we ensure that our
 * window is behaving the way Tk expects it to for things
 * like refresh.  Our current behavior in that regard is
 * a bit iffy - ogl works but I'm not convinced that's
 * because we're doing the Tcl/Tk bits right, and X is
 * doing low level blitting and other X calls (which
 * isn't great for maintainability/portability and
 * can be a headache for debugging.)
 *
 */

#include "common.h"

#include <random>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

#include "tcl.h"
#include "tk.h"

const char *DM_PHOTO = ".dm0.photo";
const char *DM_LABEL = ".dm0";

void
update_data(Tcl_Interp *interp, int r, int g, int b)
{
    Tk_PhotoImageBlock dm_data;
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoGetImage(dm_img, &dm_data);

    std::default_random_engine gen;
    std::uniform_int_distribution<int> vals(0,255);
    for (int i = 0; i < (dm_data.width * dm_data.height * 4); i+=4) {
	dm_data.pixelPtr[i] = (r) ? vals(gen) : 0;
	dm_data.pixelPtr[i+1] = (g) ? vals(gen) : 0;
	dm_data.pixelPtr[i+2] = (b) ? vals(gen) : 0;
	// Alpha stays at 255
    }

    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    // Pause a second after updating
    std::this_thread::sleep_for (std::chrono::seconds(1));
}

int
main(int UNUSED(argc), const char *argv[])
{
    std::default_random_engine gen;
    std::uniform_int_distribution<int> vals(0,255);
    std::vector<int> rgb;
    int wsize = 512;

    for (int i = 0; i < wsize; i++) {
	for (int j = 0; j < wsize; j++) {
	    rgb.push_back(0);
	    rgb.push_back(vals(gen));
	    rgb.push_back(0);
	    rgb.push_back(255);
	}
    }

    Tcl_FindExecutable(argv[0]);
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);
    Tk_Init(interp);
    Tk_Window tkwin = Tk_MainWindow(interp);
    Tk_GeometryRequest(tkwin, wsize, wsize);
    Tk_MakeWindowExist(tkwin);
    Tk_MapWindow(tkwin);

    /* Note: confirmed with Tcl/Tk community that (at least as of Tcl/Tk 8.6)
     * Tcl_Eval is the ONLY way to create an image object.  The C API just
     * doesn't expose that ability, although it does support manipulation of
     * the created object. */
    std::string img_cmd = std::string("image create photo ") + std::string(DM_PHOTO);
    Tcl_Eval(interp, img_cmd.c_str());
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoBlank(dm_img);
    Tk_PhotoSetSize(interp, dm_img, wsize, wsize);

    Tk_PhotoImageBlock dm_data;
    dm_data.width = wsize;
    dm_data.height = wsize;
    dm_data.pixelSize = 4;
    dm_data.pitch = wsize * 4;
    dm_data.offset[0] = 0;
    dm_data.offset[1] = 1;
    dm_data.offset[2] = 2;
    dm_data.offset[3] = 3;
    dm_data.pixelPtr = (unsigned char *)Tcl_AttemptAlloc(dm_data.width * dm_data.height * 4);
    if (!dm_data.pixelPtr) {
	std::cerr << "Tcl/Tk photo memory allocation failed!\n";
	exit(1);
    }
    for (int i = 0; i < (dm_data.width * dm_data.height * 4); i++) {
	// Initialize the buffer with random values (simulates on-the-fly
	// buffer generation from a rasterization or raytrace)
	dm_data.pixelPtr[i] = rgb[i];
    }

    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, wsize, wsize, TK_PHOTO_COMPOSITE_SET);

    // TODO - examples use a label to pack and display an image - is this the
    // right way for us to do it in MGED/Archer?
    std::string label_cmd = std::string("label ") + std::string(DM_LABEL) + std::string(" -image ") + std::string(DM_PHOTO);
    Tcl_Eval(interp, label_cmd.c_str());
    std::string pack_cmd = std::string("pack ") + std::string(DM_LABEL);

    Tcl_Eval(interp, pack_cmd.c_str());
    std::string bind_cmd = std::string("bind . <Button-1> \"puts A\"");
    Tcl_Eval(interp, bind_cmd.c_str());
    Tcl_Eval(interp, "event generate . <Button-1>");

    std::uniform_int_distribution<int> colors(0,1);
    while (1) {
	int handled = 0;
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT)) {
            handled++;
        }
	if (!Tk_GetNumMainWindows()) {
	    // If we've closed the window, we're done
	    exit(0);
	}

	// Generate a new random pattern on the same image
	update_data(interp, colors(gen), colors(gen), colors(gen));
    }

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
