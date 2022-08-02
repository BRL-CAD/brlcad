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
#include <string>
#include <stdlib.h>
#include <errno.h>

#include "bu/app.h"
#include "bu/malloc.h"

#include "tcl.h"
#include "tk.h"

const char *DM_PHOTO = ".dm0.photo";
const char *DM_CANVAS = ".dm0";

TCL_DECLARE_MUTEX(dilock)
TCL_DECLARE_MUTEX(fblock)
TCL_DECLARE_MUTEX(threadMutex)

/* Container holding image generation information - need to be able
 * to pass these to the update command */
struct img_data {
    // These flags should be mutex guarded.
    int dm_render_needed;
    int dm_render_running;
    int dm_render_ready;

    int fb_render_running;
    int fb_render_ready;

    // Parent application sets this when it's time to shut
    // down all rendering
    int shutdown;

    // Main thread id - used to send a rendering event back to
    // the parent thread.
    Tcl_ThreadId parent_id;

    // Tcl interp can only be used by the thread that created it, and the
    // Tk_Photo calls require an interpreter.  Thus we keep track of which
    // interpreter is the parent interp in the primary data container, as this
    // interp (and ONLY this interp) is requried to do Tk_Photo processing when
    // triggered from thread events sent back to the parent.  Those callbacks
    // don't by default encode knowledge about the parent's Tcl_Interp, so we
    // store it where we know we can access it.
    Tcl_Interp *parent_interp;

    // DM thread id
    Tcl_ThreadId dm_id;
   
    // FB thread_id 
    Tcl_ThreadId fb_id;

    // Image info.  Reflects the currently requested parent image
    // size - the render thread may have a different size during
    // rendering, as it "snapshots" these numbers to have a stable
    // buffer size when actually writing pixels.
    int dm_width;
    int dm_height;

    // Actual allocated sizes of image buffer
    int dm_bwidth;
    int dm_bheight;

    // Used for image generation - these are presisted across renders so the
    // image contents will vary as expected.
    std::default_random_engine *gen;
    std::uniform_int_distribution<int> *colors;
    std::uniform_int_distribution<int> *vals;

    // The rendering memory used to actually generate the DM scene contents.
    long dm_buff_size;
    unsigned char *dmpixel;

    // The rendering memory used to actually generate the framebuffer contents.
    long fb_buff_size;
    int fb_width;
    int fb_height;
    unsigned char *fbpixel;
};

// Event container passed to routines triggered by events.
struct RenderEvent {
    Tcl_Event event;            /* Must be first */
    struct img_data *idata;
};

// Event container passed to routines triggered by events.
struct FbRenderEvent {
    Tcl_Event event;            /* Must be first */
    struct img_data *idata;
};

// Even for events where we don't intend to actually run a proc,
// we need to tell Tcl it successfully processed them.  For that
// we define a no-op callback proc.
static int
noop_proc(Tcl_Event *UNUSED(evPtr), int UNUSED(mask))
{
    // Return one to signify a successful completion of the process execution
    return 1;
}

// The actual Tk_Photo updating must be done by the creater of the
// Tcl_Interp - which is the parent thread.  The interp below must
// thus be the PARENT's interp, and we have passed it through the
// various structures to be available here.
//
// TODO - see if we can use the Tk_PhotoPutZoomedBlock API to keep the image in
// sync with the window size as we're moving; even if it's not a fully crisp
// rendering, then catch up once a properly sized rendering is done...
static int
UpdateProc(Tcl_Event *evPtr, int UNUSED(mask))
{
    struct RenderEvent *DmEventPtr = (RenderEvent *) evPtr;
    struct img_data *idata = DmEventPtr->idata;
    Tcl_Interp *interp = idata->parent_interp;

    Tcl_MutexLock(&dilock);
    Tcl_MutexLock(&fblock);

    // Render processing now - reset the ready flag
    Tcl_MutexLock(&threadMutex);
    idata->dm_render_ready = 0;
    Tcl_MutexUnlock(&threadMutex);

    int width = idata->dm_bwidth;
    int height = idata->dm_bheight;

    // If we're mid-render, don't update - that means another render
    // got triggered after this proc got started, and another update
    // event will eventually be triggered.
    if (idata->dm_render_running) {
	Tcl_MutexUnlock(&dilock);
	Tcl_MutexUnlock(&fblock);
	return 1;
    }

    std::cout << "Window updating!\n";

    // Let Tcl/Tk know the photo data has changed, so it can update the visuals
    // accordingly
    Tk_PhotoImageBlock dm_data;
    Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
    Tk_PhotoGetImage(dm_img, &dm_data);
    if (dm_data.width != width || dm_data.height != height) {
	Tk_PhotoSetSize(interp, dm_img, width, height);
	Tk_PhotoGetImage(dm_img, &dm_data);
    }


    // Tk_PhotoPutBlock appears to be making a copy of the data, so we should
    // be able to point to our thread's rendered data to feed it in for
    // copying without worrying that Tk might alter it.
    dm_data.pixelPtr = idata->dmpixel;
    Tk_PhotoPutBlock(interp, dm_img, &dm_data, 0, 0, dm_data.width, dm_data.height, TK_PHOTO_COMPOSITE_SET);


    // Now overlay the framebuffer.
    Tk_PhotoImageBlock fb_data;
    Tk_PhotoGetImage(dm_img, &fb_data);
    fb_data.width = idata->fb_width;
    fb_data.height = idata->fb_height;
    fb_data.pitch = fb_data.width * 4;
    fb_data.pixelPtr = idata->fbpixel;
    Tk_PhotoPutBlock(interp, dm_img, &fb_data, 0, 0, idata->fb_width, idata->fb_height, TK_PHOTO_COMPOSITE_OVERLAY);

    Tcl_MutexUnlock(&dilock);
    Tcl_MutexUnlock(&fblock);

    // Return one to signify a successful completion of the process execution
    return 1;
}

int
image_update_data(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct img_data *idata = (struct img_data *)clientData;

    if (argc == 3) {
	// Unpack the current width and height.  If these don't match what idata has
	// or thinks its working on, update the image size.
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
	idata->dm_width = width;
	idata->dm_height = height;
	Tcl_MutexUnlock(&dilock);

	Tk_PhotoImageBlock dm_data;
	Tk_PhotoHandle dm_img = Tk_FindPhoto(interp, DM_PHOTO);
	Tk_PhotoGetImage(dm_img, &dm_data);
	if (dm_data.width != idata->dm_width || dm_data.height != idata->dm_height) {
	    Tk_PhotoSetSize(interp, dm_img, idata->dm_width, idata->dm_height);
	}
    }

    // Generate an event for the manager thread to let it know we need work
    Tcl_MutexLock(&threadMutex);
    struct RenderEvent *threadEventPtr = (struct RenderEvent *)ckalloc(sizeof(RenderEvent));
    threadEventPtr->idata = idata;
    threadEventPtr->event.proc = noop_proc;
    Tcl_ThreadQueueEvent(idata->dm_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(idata->dm_id);
    Tcl_MutexUnlock(&threadMutex);

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

/* This is a test of directly manipulating the Tk_Photo data itself, without
 * copying in a buffer.  Probably not something we want to do for real... */
int
image_paint_xy(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    if (argc != 3) {
	std::cerr << "Unexpected argc: " << argc << "\n";
	return TCL_ERROR;
    }

    struct img_data *idata = (struct img_data *)clientData;

    // If we're mid-render, don't draw - we're about to get superceded by
    // another frame.
    if (idata->dm_render_running) {
	return TCL_OK;
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
	    if (pindex >= idata->dm_buff_size - 1) {
		continue;
	    }
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
Dm_Render(ClientData clientData)
{
    // Unpack the shared data object
    struct img_data *idata = (struct img_data *)clientData;

    // Lock updating of the idata values until we've gotten this run's key
    // information - we don't want this changing mid-render.
    Tcl_MutexLock(&dilock);

    // Anything before this point will be convered by this render.  (This
    // flag may get set after this render starts by additional events, but
    // to this point this rendering will handle it.)
    idata->dm_render_needed = 0;

    // We're now in process - don't start another frame until this one
    // is complete.
    idata->dm_render_running = 1;

    // Until we're done, make sure nobody thinks we are ready
    idata->dm_render_ready = 0;

    // Lock in this width and height - that's what we've got memory for,
    // so that's what the remainder of this rendering pass will use.
    idata->dm_bwidth = idata->dm_width;
    idata->dm_bheight = idata->dm_height;

    // If we have insufficient memory, allocate or reallocate a local
    // buffer big enough for the purpose.  We use our own buffer for
    // rendering to allow Tk full control over what it wants to do behind
    // the scenes.  We need this memory to persist, so handle it from the
    // management thread.
    long b_minsize = idata->dm_bwidth * idata->dm_bheight * 4;
    if (!idata->dmpixel || idata->dm_buff_size < b_minsize) {
	idata->dmpixel = (unsigned char *)bu_realloc(idata->dmpixel, 2*b_minsize*sizeof(char), "realloc pixbuf");
	idata->dm_buff_size = b_minsize * 2;
    }

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
    int r = (*idata->colors)((*idata->gen));
    int g = (*idata->colors)((*idata->gen));
    int b = (*idata->colors)((*idata->gen));

    // For each pixel, get a random color value and set it in the image memory buffer.
    // This alters the actual data, but Tcl/Tk doesn't know about it yet.
    for (int i = 0; i < b_minsize; i+=4) {
	// Red
	idata->dmpixel[i] = (r) ? (*idata->vals)((*idata->gen)) : 0;
	// Green
	idata->dmpixel[i+1] = (g) ? (*idata->vals)((*idata->gen)) : 0;
	// Blue
	idata->dmpixel[i+2] = (b) ? (*idata->vals)((*idata->gen)) : 0;
	// Alpha stays at 255 (Don't really need to set it since it should
	// already be set, just doing so for local clarity about what should be
	// happening with the buffer data...)
	idata->dmpixel[i+3] = 255;
    }
    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    std::cout << "DM update: " << r << "," << g << "," << b << "\n";

    // Update done - let the parent structure know.  We don't clear the
    // render_needed flag here, since the parent window may have changed
    // between the start and the end of this render and if it has we need
    // to start over.
    Tcl_MutexLock(&dilock);
    idata->dm_render_running = 0;
    Tcl_MutexLock(&threadMutex);
    idata->dm_render_ready = 1;
    Tcl_MutexUnlock(&threadMutex);
    Tcl_MutexUnlock(&dilock);

    // Generate an event for the manager thread to let it know we're done
    Tcl_MutexLock(&threadMutex);
    struct RenderEvent *threadEventPtr = (struct RenderEvent *)ckalloc(sizeof(RenderEvent));
    threadEventPtr->idata = idata;
    threadEventPtr->event.proc = noop_proc;
    Tcl_ThreadQueueEvent(idata->dm_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(idata->dm_id);
    Tcl_MutexUnlock(&threadMutex);

    // Render complete, we're done with this thread
    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
}


static Tcl_ThreadCreateType
Fb_Render(ClientData clientData)
{
    // Unpack the shared data object
    struct img_data *idata = (struct img_data *)clientData;

    // Lock updating of the idata values until we've gotten this run's key
    // information - we don't want this changing mid-render.
    Tcl_MutexLock(&fblock);

    // We're now in process - don't start another frame until this one
    // is complete.
    idata->fb_render_running = 1;

    // Until we're done, make sure nobody thinks we are ready
    Tcl_MutexLock(&threadMutex);
    idata->fb_render_ready = 0;
    Tcl_MutexUnlock(&threadMutex);

    // Lock in this width and height - that's what we've got memory for,
    // so that's what the remainder of this rendering pass will use.
    idata->fb_width = idata->dm_width;
    idata->fb_height = idata->dm_height;

    // If we have insufficient memory, allocate or reallocate a local
    // buffer big enough for the purpose.  We use our own buffer for
    // rendering to allow Tk full control over what it wants to do behind
    // the scenes.  We need this memory to persist, so handle it from the
    // management thread.
    long b_minsize = idata->fb_width * idata->fb_height * 4;
    if (!idata->fbpixel || idata->fb_buff_size < b_minsize) {
	idata->fbpixel = (unsigned char *)bu_realloc(idata->fbpixel, 2*b_minsize*sizeof(char), "realloc pixbuf");
	idata->fb_buff_size = b_minsize * 2;
    }

    // Have the key values now, we can unlock and allow interactivity in
    // the parent.  We'll finish this rendering pass before paying any
    // further attention to parent settings, but the parent may now set
    // them for future consideration.
    Tcl_MutexUnlock(&fblock);

    //////////////////////////////////////////////////////////////////////////////
    // Rendering operation - this is where the work of a FB render pass
    // would occur in a real application
    //////////////////////////////////////////////////////////////////////////////

    int r = (*idata->colors)((*idata->gen));
    int g = (*idata->colors)((*idata->gen));
    int b = (*idata->colors)((*idata->gen));

    // Initialize. This alters the actual data, but Tcl/Tk doesn't know about it yet.
    for (int i = 0; i < (idata->fb_width * idata->fb_height * 4); i+=4) {
	// Red
	idata->fbpixel[i] = (r) ? 255 : 0;
	// Green
	idata->fbpixel[i+1] = (g) ? 255 : 0;
	// Blue
	idata->fbpixel[i+2] = (b) ? 255 : 0;
	// Alpha
	idata->fbpixel[i+3] = 100;
    }

    //////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    Tcl_MutexLock(&fblock);
    idata->fb_render_running = 0;
    Tcl_MutexLock(&threadMutex);
    idata->fb_render_ready = 1;
    Tcl_MutexUnlock(&threadMutex);
    Tcl_MutexUnlock(&fblock);

    std::cout << "FB update: " << r << "," << g << "," << b << "\n";

    // Generate an event for the manager thread to let it know we're done, if the
    // display manager isn't already about to generate such an event
    Tcl_MutexLock(&threadMutex);
    struct RenderEvent *threadEventPtr = (struct RenderEvent *)ckalloc(sizeof(RenderEvent));
    threadEventPtr->idata = idata;
    threadEventPtr->event.proc = noop_proc;
    Tcl_ThreadQueueEvent(idata->fb_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(idata->fb_id);
    Tcl_MutexUnlock(&threadMutex);

    // Render complete, we're done with this thread
    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
}


static Tcl_ThreadCreateType
Dm_Update_Manager(ClientData clientData)
{
    // This thread needs to process events - give it its own interp so it can do so.
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);

    // Unpack the shared data object
    struct img_data *idata = (struct img_data *)clientData;

    // Set up our data sources - we'll reuse these for each render, so they are
    // set up in the managing thread
    std::default_random_engine gen;
    std::uniform_int_distribution<int> colors(0,1);
    std::uniform_int_distribution<int> vals(0,255);
    idata->gen = &gen;
    idata->colors = &colors;
    idata->vals = &vals;

    // Until we're shutting down, check for and process events - this thread will persist
    // for the life of the application.
    while (!idata->shutdown) {

	// Events can come from either the parent (requesting a rendering, announcing a shutdown)
	// or from the rendering thread (announcing completion of a rendering.)
	Tcl_DoOneEvent(0);

	// If anyone flipped the shutdown switch while we were waiting on an
	// event, don't do any more work - there's no point.
	if (idata->shutdown) {
	    continue;
	}

	// If we have a render ready, let the parent know to update the GUI before we
	// go any further.  Even if a completed render is out of data, it's closer to
	// current than what was previously displayed so the application should go ahead
	// and show it.
	if (idata->dm_render_ready) {

	    // Generate an event for the parent thread - its time to update the
	    // Tk_Photo in the GUI that's holding the image
	    Tcl_MutexLock(&threadMutex);
	    struct RenderEvent *threadEventPtr = (struct RenderEvent *)ckalloc(sizeof(RenderEvent));
	    threadEventPtr->idata = idata;
	    threadEventPtr->event.proc = UpdateProc;
	    Tcl_ThreadQueueEvent(idata->parent_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
	    Tcl_ThreadAlert(idata->parent_id);
	    Tcl_MutexUnlock(&threadMutex);

	    // If we don't have any work pending, go back to waiting.  If we know
	    // we're already out of date, proceed without waiting for another event
	    // from the parent - we've already had one.
	    if (!idata->dm_render_needed) {
		continue;
	    }
	}

	// If we're already rendering and we have another rendering event, it's
	// an update request from the parent - set the flag and continue
	if (idata->dm_render_running) {
	    idata->dm_render_needed = 1;
	    continue;
	}

	// If we need a render and aren't already running, it's time to go to work.


	// Start a rendering thread.
	Tcl_ThreadId threadID;
	if (Tcl_CreateThread(&threadID, Dm_Render, (ClientData)idata, TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS) != TCL_OK) {
	    std::cerr << "can't create dm render thread\n";
	}
    }

    // We're well and truly done - the application is closing down - free the
    // rendering buffer and quit the thread
    bu_free(idata->dmpixel, "free pixbuf");

    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
}

static Tcl_ThreadCreateType
Fb_Update_Manager(ClientData clientData)
{
    // This thread needs to process events - give it its own interp so it can do so.
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);

    // Unpack the shared data object
    struct img_data *idata = (struct img_data *)clientData;

    // Until we're shutting down, check for and process events - this thread will persist
    // for the life of the application.
    while (!idata->shutdown) {

	// Events can come from either the parent (requesting a rendering, announcing a shutdown)
	// or from the rendering thread (announcing completion of a rendering.)

	// Eventually we'll want separate events to trigger this, but for now just update it at
	// regular intervals
	if (!idata->fb_render_ready) {
	    Tcl_Sleep(2000);
	}
	//Tcl_DoOneEvent(0);


	// If anyone flipped the shutdown switch while we were waiting, don't
	// do any more work - there's no point.
	if (idata->shutdown) {
	    continue;
	}

	// If we have a render ready, let the parent know to update the GUI.
	if (idata->fb_render_ready) {

	    // Generate an event for the parent thread - its time to update the
	    // Tk_Photo in the GUI that's holding the image
	    Tcl_MutexLock(&threadMutex);
	    struct RenderEvent *threadEventPtr = (struct RenderEvent *)ckalloc(sizeof(RenderEvent));
	    threadEventPtr->idata = idata;
	    threadEventPtr->event.proc = UpdateProc;
	    Tcl_ThreadQueueEvent(idata->parent_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
	    Tcl_ThreadAlert(idata->parent_id);
	    Tcl_MutexUnlock(&threadMutex);

	    // Go back to waiting.
	    Tcl_MutexLock(&threadMutex);
	    idata->fb_render_ready = 0;
	    Tcl_MutexUnlock(&threadMutex);
	    continue;
	}

	// If we need a render and aren't already running, it's time to go to work.
	// Start a framebuffer thread.
	std::cout << "Framebuffer updating!\n";
	Tcl_ThreadId fbthreadID;
	if (Tcl_CreateThread(&fbthreadID, Fb_Render, (ClientData)idata, TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS) != TCL_OK) {
	    std::cerr << "can't create fb render thread\n";
	}

    }

    // We're well and truly done - the application is closing down - free the
    // rendering buffer and quit the thread
    bu_free(idata->fbpixel, "free pixbuf");

    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
}

int
main(int UNUSED(argc), const char *argv[])
{
    std::string bind_cmd;

    bu_setprogname(argv[0]);

    // For now, just use a fixed 512x512 image size
    int wsize = 512;

    // Set up random image data generation so we can simulate a raytrace or
    // raster changing the view data.  We need to use these for subsequent
    // image updating, so pack them into a structure we can pass through Tcl's
    // evaluations.
    struct img_data *idata;
    BU_GET(idata, struct img_data);
    idata->dm_render_needed = 1;
    idata->dm_render_running = 0;
    idata->dm_render_ready = 0;
    idata->shutdown = 0;
    idata->dm_buff_size = 0;
    idata->dmpixel = NULL;
    idata->fb_buff_size = 0;
    idata->fbpixel = NULL;
    idata->parent_id = Tcl_GetCurrentThread();

    // Set up Tcl/Tk
    Tcl_FindExecutable(argv[0]);
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);
    Tk_Init(interp);

    // Save a pointer so we can get at the interp later
    idata->parent_interp = interp;

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
    idata->dm_width = dm_data.width;
    idata->dm_height = dm_data.height;

    // Define a canvas widget, pack it into the root window, and associate the image with it
    // TODO - should the canvas be inside a frame?
    std::string canvas_cmd = std::string("canvas ") + std::string(DM_CANVAS) + std::string(" -width ") + std::to_string(wsize) + std::string(" -height ")  + std::to_string(wsize) + std::string(" -borderwidth 0");
    Tcl_Eval(interp, canvas_cmd.c_str());
    std::string pack_cmd = std::string("pack ") + std::string(DM_CANVAS) + std::string(" -fill both -expand 1");
    Tcl_Eval(interp, pack_cmd.c_str());
    std::string canvas_img_cmd = std::string(DM_CANVAS) + std::string(" create image 0 0 -image ") + std::string(DM_PHOTO) + std::string(" -anchor nw");
    Tcl_Eval(interp, canvas_img_cmd.c_str());


    // Register a paint command so we can change the image contents near the cursor position
    (void)Tcl_CreateCommand(interp, "image_paint", (Tcl_CmdProc *)image_paint_xy, (ClientData)idata, (Tcl_CmdDeleteProc* )NULL);
    // Establish the Button-1+Motion combination event as the trigger for drawing on the image
    bind_cmd = std::string("bind . <B1-Motion> {image_paint %x %y}");
    Tcl_Eval(interp, bind_cmd.c_str());


    // Register an update command so we can change the image contents
    (void)Tcl_CreateCommand(interp, "image_update", (Tcl_CmdProc *)image_update_data, (ClientData)idata, (Tcl_CmdDeleteProc* )NULL);
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
    if (Tcl_CreateThread(&threadID, Dm_Update_Manager, (ClientData)idata, TCL_THREAD_STACK_DEFAULT, TCL_THREAD_JOINABLE) != TCL_OK) {
	std::cerr << "can't create thread\n";
    }
    idata->dm_id = threadID;

    Tcl_ThreadId fbthreadID;
    if (Tcl_CreateThread(&fbthreadID, Fb_Update_Manager, (ClientData)idata, TCL_THREAD_STACK_DEFAULT, TCL_THREAD_JOINABLE) != TCL_OK) {
	std::cerr << "can't create thread\n";
    }
    idata->fb_id = fbthreadID;


    // Enter the main applicatio loop - the initial image will appear, and Button-1 mouse
    // clicks on the window should generate and display new images
    while (1) {
	Tcl_DoOneEvent(0);
	if (!Tk_GetNumMainWindows()) {
	    int tret;

	    // If we've closed the window, we're done - start spreading the word
	    idata->shutdown = 1;


	    // After setting the shutdown flag, send an event to wake up the FB update manager thread.
	    // It will see the change in status and proceed to shut itself down.
	    Tcl_MutexLock(&fblock);
	    struct RenderEvent *threadEventPtr = (struct RenderEvent *)ckalloc(sizeof(RenderEvent));
	    threadEventPtr->idata = idata;
	    threadEventPtr->event.proc = noop_proc;
	    Tcl_ThreadQueueEvent(idata->fb_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
	    Tcl_ThreadAlert(idata->fb_id);
	    Tcl_MutexUnlock(&fblock);

	    // Wait for the thread cleanup to complete
	    Tcl_JoinThread(idata->fb_id, &tret);
	    if (tret != TCL_OK) {
		std::cerr << "Failed to shut down framebuffer management thread\n";
	    } else {
		std::cout << "Successful framebuffer management thread shutdown\n";
	    }

	    // After setting the shutdown flag, send an event to wake up the update manager thread.
	    // It will see the change in status and proceed to shut itself down.
	    Tcl_MutexLock(&dilock);
	    threadEventPtr = (struct RenderEvent *)ckalloc(sizeof(RenderEvent));
	    threadEventPtr->idata = idata;
	    threadEventPtr->event.proc = noop_proc;
	    Tcl_ThreadQueueEvent(idata->dm_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
	    Tcl_ThreadAlert(idata->dm_id);
	    Tcl_MutexUnlock(&dilock);

	    // Wait for the thread cleanup to complete
	    Tcl_JoinThread(idata->dm_id, &tret);
	    if (tret != TCL_OK) {
		std::cerr << "Failed to shut down display management thread\n";
	    } else {
		std::cout << "Successful display management thread shutdown\n";
	    }


	    // Clean up memory and exit
	    BU_PUT(idata, struct img_data);
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
