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

#include <climits>
#include <random>
#include <iostream>
#include <stdlib.h>
#include <errno.h>

#include "bu/malloc.h"

#include "tcl.h"
#include "tk.h"

const char *DM_PHOTO = ".dm0.photo";
const char *DM_CANVAS = ".dm0";

TCL_DECLARE_MUTEX(dilock)
TCL_DECLARE_MUTEX(threadMutex)

/* Container holding image generation information - need to be able
 * to pass these to the update command */
struct img_data {
    // These flags should be mutex guarded.
    int render_needed;
    int render_running;
    int render_ready;

    // Parent application sets this when it's time to shut
    // down the dm rendering
    int render_shutdown;

    // Main thread id
    Tcl_ThreadId parent;
    // DO NOT USE in thread - present here to pass through event back to parent
    Tcl_Interp *parent_interp;

    // Image info
    int width;
    int height;

    // This gets interesting - a Tcl interp can only be used by the
    // thread that created it.  We provide the Tk_PhotoImageBlock
    // to the thread, but the thread can't do any operations on
    // the data that require the interp, so the update process
    // must be partially in the thread and partially in the parent.
    long buff_size;
    unsigned char *pixelPtr;
};

struct DmEventResult {
    Tcl_Condition done;
    int ret;
};

struct DmRenderEvent {
    Tcl_Event event;            /* Must be first */
    struct DmEventResult *result;
    struct img_data *idata;
    int width;
    int height;
};

static int
DmRenderProc(Tcl_Event *evPtr, int UNUSED(mask))
{
    struct DmRenderEvent *DmEventPtr = (DmRenderEvent *) evPtr;
    struct img_data *idata = DmEventPtr->idata;
    Tcl_Interp *interp = idata->parent_interp;
    int width = DmEventPtr->width;
    int height = DmEventPtr->height;

    // Let Tcl/Tk know the photo data has changed, so it can update the visuals
    // accordingly
    Tk_PhotoImageBlock dm_data;
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoGetImage(dm_img, &dm_data);
    if (dm_data.width != width || dm_data.height != height) {
	Tk_PhotoSetSize(interp, dm_img, width, height);
	Tk_PhotoGetImage(dm_img, &dm_data);
    }
    dm_data.width = width;
    dm_data.height = height;
    // Tk_PhotoPutBlock appears to be making a copy of the data, so we should
    // be able to point to our thread's rendered data to feed it in for
    // copying.
    dm_data.pixelPtr = idata->pixelPtr;
    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    // Render complete
    Tcl_MutexLock(&dilock);
    idata->render_ready = 0;
    Tcl_MutexUnlock(&dilock);

    DmEventPtr->result->ret = 0;
    Tcl_ConditionNotify(&resultPtr->done);

    return 1;
}

int
image_update_data(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct img_data *idata = (struct img_data *)clientData;

    if (argc == 3) {
	// Unpack the current width and height.  If these don't match what idata has
	// or thinks its working on, set the flag indicating we need a render.
	// (Note: checking errno, although it may not truly be necessary if we
	// trust Tk to always give us valid coordinates...)
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

	Tcl_MutexLock(&dilock);
	idata->width = width;
	idata->height = height;
	Tcl_MutexUnlock(&dilock);

	Tk_PhotoImageBlock dm_data;
	Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
	Tk_PhotoGetImage(dm_img, &dm_data);
	if (dm_data.width != idata->width || dm_data.height != idata->height) {
	    Tk_PhotoSetSize(interp, dm_img, idata->width, idata->height);
	}
    }

    Tcl_MutexLock(&dilock);
    idata->render_needed = 1; // Ready - thread can now render
    Tcl_MutexUnlock(&dilock);

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

static Tcl_ThreadCreateType
Dm_Draw(ClientData clientData)
{
    struct img_data *idata = (struct img_data *)clientData;
    std::default_random_engine gen;
    std::uniform_int_distribution<int> colors(0,1);
    std::uniform_int_distribution<int> vals(0,255);

    while (!idata->render_shutdown) {
	int width, height;
	if (!idata->render_needed) {
	    // If we havne't been asked for an update,
	    // sleep a bit and then check again.
	    Tcl_Sleep(1000);
	    continue;
	}

	// If we do need to render, get started.  Lock updating of the idata values until we've
	// gotten this run's key information - we don't want this changing mid-render.
	Tcl_MutexLock(&dilock);

	idata->render_needed = 0;
	idata->render_running = 1;
	idata->render_ready = 0;

	// If we have insufficient memory, allocate or reallocate
	// a local buffer big enough for the purpose.  We use our
	// own buffer for rendering to allow Tk full control over
	// what it wants to do behind the scenes
	long b_minsize = idata->width * idata->height * 4;
	if (!idata->pixelPtr || idata->buff_size < b_minsize) {
	    idata->pixelPtr = (unsigned char *)bu_realloc(idata->pixelPtr, 2*b_minsize*sizeof(char), "realloc pixbuf");
	    idata->buff_size = b_minsize * 2;
	}

	// Lock in this width and height - that's what we've got memory for,
	// so that's what the remainder of this rendering pass will use.
	width = idata->width;
	height = idata->height;

	// Have the key values now, we can unlock and allow interactivity in
	// the parent.  We'll finish this rendering pass before paying any
	// further attention to parent settings, but the parent may now set
	// them for future consideration.
	Tcl_MutexUnlock(&dilock);

	//////////////////////////////////////////////////////////////////////////////
	// Rendering operation - this is where the work of a DM/FB render pass
	// would occur in a real application
	//////////////////////////////////////////////////////////////////////////////
	// To get a little visual variation and make it easer to see changes,
	// randomly turn on/off the individual colors for each pass.
	int r = colors(gen);
	int g = colors(gen);
	int b = colors(gen);

	// For each pixel, get a random color value and set it in the image memory buffer.
	// This alters the actual data, but Tcl/Tk doesn't know about it yet.
	for (int i = 0; i < b_minsize; i+=4) {
	    // Red
	    idata->pixelPtr[i] = (r) ? vals(gen) : 0;
	    // Green
	    idata->pixelPtr[i+1] = (g) ? vals(gen) : 0;
	    // Blue
	    idata->pixelPtr[i+2] = (b) ? vals(gen) : 0;
	    // Alpha stays at 255 (Don't really need to set it since it should
	    // already be set, just doing so for local clarity about what should be
	    // happening with the buffer data...)
	    idata->pixelPtr[i+3] = 255;
	}
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	// Update done - let the parent structure know.  We don't set the
	// needed flag here, since the parent window may have changed between
	// the start and the end of this render and if it has we need to start
	// over.
	Tcl_MutexLock(&dilock);
	idata->render_running = 0;
	idata->render_ready = 1;
	Tcl_MutexUnlock(&dilock);

	// Generate an event for the parent thread to let it know its time
	// to update the Tk_Photo holding the DM
	Tcl_MutexLock(&threadMutex);
	struct DmRenderEvent *threadEventPtr = (struct DmRenderEvent *)ckalloc(sizeof(DmRenderEvent));
	struct DmEventResult *resultPtr = (struct DmEventResult *)ckalloc(sizeof(DmEventResult));
	threadEventPtr->result = resultPtr;
	threadEventPtr->idata = idata;
	threadEventPtr->width = width;
	threadEventPtr->height = height;
	threadEventPtr->event.proc = DmRenderProc;
	resultPtr->done = NULL;
	resultPtr->ret = 1;
	Tcl_ThreadQueueEvent(idata->parent, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
	Tcl_ThreadAlert(idata->parent);
	while (resultPtr->ret) {
	    Tcl_ConditionWait(&resultPtr->done, &threadMutex, NULL);
	}

	Tcl_MutexUnlock(&threadMutex);

	Tcl_ConditionFinalize(&resultPtr->done);
	ckfree(resultPtr);
    }

    // We're well and truly done - the application is closing down - free the
    // rendering buffer and quit the thread
    if (idata->pixelPtr) {
	bu_free(idata->pixelPtr, "free pixbuf");
    }
    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
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
    struct img_data idata;
    idata.render_needed = 1;
    idata.render_running = 0;
    idata.render_ready = 0;
    idata.render_shutdown = 0;
    idata.pixelPtr = NULL;
    idata.parent = Tcl_GetCurrentThread();

    // Set up Tcl/Tk
    Tcl_FindExecutable(argv[0]);
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);
    Tk_Init(interp);

    // Save a pointer so we can get at the interp later
    idata.parent_interp = interp;

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

    // Initialize. This alters the actual data, but Tcl/Tk doesn't know about it yet.
    for (int i = 0; i < (dm_data.width * dm_data.height * 4); i+=4) {
	// Red
	dm_data.pixelPtr[i] = 0;
	// Green
	dm_data.pixelPtr[i+1] = 255;
	// Blue
	dm_data.pixelPtr[i+2] = 0;
	// Alpha at 255 - we dont' want transparency for this demo.
	dm_data.pixelPtr[i+3] = 255;
    }

    // Let Tk_Photo know we have data
    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);

    // We now have a window - set the default idata size
    idata.width = dm_data.width;
    idata.height = dm_data.height;

    // Define a canvas widget, pack it into the root window, and associate the image with it
    // TODO - should the canvas be inside a frame?
    std::string canvas_cmd = std::string("canvas ") + std::string(DM_CANVAS) + std::string(" -width ") + std::to_string(wsize) + std::string(" -height ")  + std::to_string(wsize) + std::string(" -borderwidth 0");
    Tcl_Eval(interp, canvas_cmd.c_str());
    std::string pack_cmd = std::string("pack ") + std::string(DM_CANVAS) + std::string(" -fill both -expand 1");
    Tcl_Eval(interp, pack_cmd.c_str());
    std::string canvas_img_cmd = std::string(DM_CANVAS) + std::string(" create image 0 0 -image ") + std::string(DM_PHOTO) + std::string(" -anchor nw");
    Tcl_Eval(interp, canvas_img_cmd.c_str());


    // Register a paint command so we can change the image contents near the cursor position
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

    // Update the photo if the window changes
    bind_cmd = std::string("bind ") + std::string(DM_CANVAS) + std::string(" <Configure> {image_update [winfo width %W] [winfo height %W]\"}");
    Tcl_Eval(interp, bind_cmd.c_str());

    // Multithreading experiment
    Tcl_ThreadId threadID;
    if (Tcl_CreateThread(&threadID, Dm_Draw, (ClientData)&idata, TCL_THREAD_STACK_DEFAULT, TCL_THREAD_JOINABLE) != TCL_OK) {
	std::cerr << "can't create thread\n";
    }

    // Enter the main applicatio loop - the initial image will appear, and Button-1 mouse
    // clicks on the window should generate and display new images
    while (1) {
	Tcl_DoOneEvent(0);
	if (!Tk_GetNumMainWindows()) {
	    // If we've closed the window, we're done
	    Tcl_MutexLock(&dilock);
	    idata.render_shutdown = 1;
	    Tcl_MutexUnlock(&dilock);
	    int tret;
	    Tcl_JoinThread(threadID, &tret);
	    if (tret != TCL_OK) {
		std::cerr << "Failure to shut down display thread\n";
	    } else {
		std::cout << "Successful display thread shutdown\n";
	    }
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
