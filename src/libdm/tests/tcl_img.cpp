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
#include <iostream>
#include <stdlib.h>
#include <errno.h>

#include "tcl.h"
#include "tk.h"

const char *DM_PHOTO = ".dm0.photo";
const char *DM_CANVAS = ".dm0";

/* Container holding image generation information - need to be able
 * to pass these to the update command */
struct img_data {
    std::default_random_engine *gen;
    std::uniform_int_distribution<int> *colors;
    std::uniform_int_distribution<int> *vals;
};

int
image_update_data(ClientData clientData, Tcl_Interp *interp, int UNUSED(argc), char **UNUSED(argv))
{
    struct img_data *idata = (struct img_data *)clientData;

    // Look up the internals of the image - we're going to directly manipulate
    // the values of the image to simulate a display manager or framebuffer
    // changing the visual via rendering.
    Tk_PhotoImageBlock dm_data;
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoGetImage(dm_img, &dm_data);

    // To get a little visual variation and make it easer to see changes,
    // randomly turn on/off the individual colors for each pass.
    int r = (*idata->colors)((*idata->gen));
    int g = (*idata->colors)((*idata->gen));
    int b = (*idata->colors)((*idata->gen));

    // For each pixel, get a random color value and set it in the image memory buffer.
    // This alters the actual data, but Tcl/Tk doesn't know about it yet.
    for (int i = 0; i < (dm_data.width * dm_data.height * 4); i+=4) {
	// Red
	dm_data.pixelPtr[i] = (r) ? (*idata->vals)((*idata->gen)) : 0;
	// Green
	dm_data.pixelPtr[i+1] = (g) ? (*idata->vals)((*idata->gen)) : 0;
	// Blue
	dm_data.pixelPtr[i+2] = (b) ? (*idata->vals)((*idata->gen)) : 0;
	// Alpha stays at 255 (Don't really need to set it since it should
	// already be set, just doing so for local clarity about what should be
	// happening with the buffer data...)
	dm_data.pixelPtr[i+3] = 255;
    }

    // Let Tcl/Tk know the photo data has changed, so it can update the visuals accordingly
    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    return TCL_OK;
}

// Given an X,Y coordinate in a TkPhoto image and a desired offset in
// X and Y, return the index value into the pixelPtr array N such that
// N is the integer value of the R color at that X,Y coordiante, N+1
// is the G value, N+2 is the B value and N+3 is the Alpha value.
// If either desired offset is beyond the width and height boundaries,
// cap the return at the minimum/maximum allowed value.
static int
img_xy_index(int width, int height, int x, int y, int dx, int dy)
{
    int nx = ((x + dx) > width)  ? width  : ((x + dx) < 0) ? 0 : x + dx;
    int ny = ((y + dy) > height) ? height : ((y + dy) < 0) ? 0 : y + dy;
    return (ny * width * 4) + nx * 4;
}

int
image_paint_xy(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    if (argc != 3) {
	std::cerr << "Unexpected argc: " << argc << "\n";
	return TCL_ERROR;
    }

    // Unpack the coordinates (checking errno, although it may not truly be
    // necessary if we trust Tk to always give us valid coordinates...)
    char *p_end;
    errno = 0;
    long xcoor = strtol(argv[1], &p_end, 10);
    if (errno == ERANGE || (errno != 0 && xcoor == 0) || p_end == argv[1]) {
	std::cerr << "Invalid image_paint_xy X coordinate: " << argv[1] << "\n";
	return TCL_ERROR;
    }
    errno = 0;
    long ycoor = strtol(argv[2], &p_end, 10);
    if (errno == ERANGE || (errno != 0 && ycoor == 0) || p_end == argv[1]) {
	std::cerr << "Invalid image_paint_xy Y coordinate: " << argv[2] << "\n";
	return TCL_ERROR;
    }

    // Look up the internals of the image - we're going to directly manipulate
    // the values of the image to simulate a display manager or framebuffer
    // changing the visual via rendering.
    Tk_PhotoImageBlock dm_data;
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoGetImage(dm_img, &dm_data);


    for (int i = -2; i < 3; i++) {
	for (int j = -2; j < 3; j++) {
	    int pindex = img_xy_index(dm_data.width, dm_data.height, xcoor, ycoor, i, j);
	    // Set to opaque white
	    dm_data.pixelPtr[pindex] = 255;
	    dm_data.pixelPtr[pindex+1] = 255;
	    dm_data.pixelPtr[pindex+2] = 255;
	    dm_data.pixelPtr[pindex+3] = 255;
	}
    }

    // Let Tcl/Tk know the photo data has changed, so it can update the visuals accordingly.
    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    return TCL_OK;
}

int
image_resize_view(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    if (argc != 3) {
	std::cerr << "Unexpected argc: " << argc << "\n";
	return TCL_ERROR;
    }

    // Unpack the coordinates (checking errno, although it may not truly be
    // necessary if we trust Tk to always give us valid coordinates...)
    char *p_end;
    errno = 0;
    long width = strtol(argv[1], &p_end, 10);
    if (errno == ERANGE || (errno != 0 && width == 0) || p_end == argv[1]) {
	std::cerr << "Invalid width: " << argv[1] << "\n";
	return TCL_ERROR;
    }
    errno = 0;
    long height = strtol(argv[2], &p_end, 10);
    if (errno == ERANGE || (errno != 0 && height == 0) || p_end == argv[1]) {
	std::cerr << "Invalid height: " << argv[2] << "\n";
	return TCL_ERROR;
    }

    // Set the new photo size.
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoSetSize(interp, dm_img, width, height);

    image_update_data(clientData, interp, 0, NULL);

    return TCL_OK;
}


int
main(int UNUSED(argc), const char *argv[])
{
    std::string bind_cmd;

    // For now, just use a fixed 512x512 image size
    int wsize = 512;

    // Set up random image data generation so we can simulate a raytrace or
    // raster changing the view data.  We need to use these for subsequent
    // image updating, so pack them into a structure we can pass through Tcl's
    // evaluations.
    std::default_random_engine gen;
    std::uniform_int_distribution<int> colors(0,1);
    std::uniform_int_distribution<int> vals(0,255);
    struct img_data idata;
    idata.gen = &gen;
    idata.colors = &colors;
    idata.vals = &vals;

    // Set up Tcl/Tk
    Tcl_FindExecutable(argv[0]);
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);
    Tk_Init(interp);

    // Make a simple toplevel window
    Tk_Window tkwin = Tk_MainWindow(interp);
    Tk_GeometryRequest(tkwin, wsize, wsize);
    Tk_MakeWindowExist(tkwin);
    Tk_MapWindow(tkwin);

    // Create the (initially empty) Tcl/Tk Photo (a.k.a color image) we will
    // use as our rendering canvas for the images.
    //
    // Note: confirmed with Tcl/Tk community that (at least as of Tcl/Tk 8.6)
    // Tcl_Eval is the ONLY way to create an image object.  The C API doesn't
    // expose that ability, although it does support manipulation of the
    // created object.
    std::string img_cmd = std::string("image create photo ") + std::string(DM_PHOTO);
    Tcl_Eval(interp, img_cmd.c_str());
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoBlank(dm_img);
    Tk_PhotoSetSize(interp, dm_img, wsize, wsize);

    // Initialize the PhotoImageBlock information for a color image of size
    // 500x500 pixels.
    Tk_PhotoImageBlock dm_data;
    dm_data.width = wsize;
    dm_data.height = wsize;
    dm_data.pixelSize = 4;
    dm_data.pitch = wsize * 4;
    dm_data.offset[0] = 0;
    dm_data.offset[1] = 1;
    dm_data.offset[2] = 2;
    dm_data.offset[3] = 3;

    // Actually create our memory for the image buffer.  Expects RGBA information
    dm_data.pixelPtr = (unsigned char *)Tcl_AttemptAlloc(dm_data.width * dm_data.height * 4);
    if (!dm_data.pixelPtr) {
	std::cerr << "Tcl/Tk photo memory allocation failed!\n";
	exit(1);
    }

    // For each pixel, get a random color value and set it in the image memory buffer.
    // This alters the actual data, but Tcl/Tk doesn't know about it yet.
    for (int i = 0; i < (dm_data.width * dm_data.height * 4); i+=4) {
	// Red
	dm_data.pixelPtr[i] = 0;
	// Green
	dm_data.pixelPtr[i+1] = (*idata.vals)((*idata.gen));
	// Blue
	dm_data.pixelPtr[i+2] = 0;
	// Alpha at 255 - we dont' want transparency for this demo.
	dm_data.pixelPtr[i+3] = 255;
    }

    // Let Tk_Photo know we have data
    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);


    // Define a canvas widget, pack it into the root window, and associate the image with it
    // TODO - should the canvas be inside a frame?
    std::string canvas_cmd = std::string("canvas ") + std::string(DM_CANVAS) + std::string(" -width ") + std::to_string(wsize) + std::string(" -height ")  + std::to_string(wsize) + std::string(" -borderwidth 0");
    Tcl_Eval(interp, canvas_cmd.c_str());
    std::string pack_cmd = std::string("pack ") + std::string(DM_CANVAS) + std::string(" -fill both -expand 1");
    Tcl_Eval(interp, pack_cmd.c_str());
    std::string canvas_img_cmd = std::string(DM_CANVAS) + std::string(" create image 0 0 -image ") + std::string(DM_PHOTO) + std::string(" -anchor nw");
    Tcl_Eval(interp, canvas_img_cmd.c_str());


    // Register a paint command so we can change the image contents neary the cursor position
    (void)Tcl_CreateCommand(interp, "image_paint", (Tcl_CmdProc *)image_paint_xy, NULL, (Tcl_CmdDeleteProc* )NULL);
    // Establish the Button-1+Motion combination event as the trigger for drawing on the image
    bind_cmd = std::string("bind . <B1-Motion> {image_paint %x %y}");
    Tcl_Eval(interp, bind_cmd.c_str());


    // Register an update command so we can change the image contents
    (void)Tcl_CreateCommand(interp, "image_update", (Tcl_CmdProc *)image_update_data, (ClientData)&idata, (Tcl_CmdDeleteProc* )NULL);
    // Establish the Button-2 and Button-3 event as triggers for updating the image contents
    bind_cmd = std::string("bind . <Button-2> {image_update}");
    Tcl_Eval(interp, bind_cmd.c_str());
    bind_cmd = std::string("bind . <Button-3> {image_update}");
    Tcl_Eval(interp, bind_cmd.c_str());

    // Register a callback to change the image size in response to a window change
    (void)Tcl_CreateCommand(interp, "image_resize", (Tcl_CmdProc *)image_resize_view, (ClientData)&idata, (Tcl_CmdDeleteProc* )NULL);
    // Establish the Button-1+Motion combination event as the trigger for drawing on the image
    bind_cmd = std::string("bind ") + std::string(DM_CANVAS) + std::string(" <Configure> {image_resize [winfo width %W] [winfo height %W]\"}");
    Tcl_Eval(interp, bind_cmd.c_str());

    // Enter the main applicatio loop - the initial image will appear, and Button-1 mouse
    // clicks on the window should generate and display new images
    while (1) {
	Tcl_DoOneEvent(0);
	if (!Tk_GetNumMainWindows()) {
	    // If we've closed the window, we're done
	    exit(0);
	}
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
