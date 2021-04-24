// TinyGL gears example
//
// by Brian Paul. This program is in the public domain.
// ported to libSDL/TinyGL by Gerald Franz (gfz@o2online.de)
// updated by Norbert Kett (anchor@rocketmail.com)

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "OSMesa/gl.h"
#include "OSMesa/glu.h"
#include "OSMesa/osmesa.h"
#include <SDL/SDL.h>

#ifndef M_PI
#  define M_PI 3.14159265
#endif

/*
* Draw a gear wheel.  You'll probably want to call this function when
* building a display list since we do a lot of trig here.
*
* Input:  inner_radius - radius of hole at center
*         outer_radius - radius at center of teeth
*         width - width of gear
*         teeth - number of teeth
*         tooth_depth - depth of tooth
*/
void gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width, GLint teeth, GLfloat tooth_depth)
{
    GLint i;
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth/2.0;
    r2 = outer_radius + tooth_depth/2.0;

    da = 2.0*M_PI / teeth / 4.0;

    glShadeModel(GL_FLAT);

    glNormal3f(0.0, 0.0, 1.0);

    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (i=0; i<=teeth; i++) {
	angle = i * 2.0*M_PI / teeth;
	glVertex3f(r0*cos(angle), r0*sin(angle), width*0.5);
	glVertex3f(r1*cos(angle), r1*sin(angle), width*0.5);
	glVertex3f(r0*cos(angle), r0*sin(angle), width*0.5);
	glVertex3f(r1*cos(angle+3*da), r1*sin(angle+3*da), width*0.5);
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0*M_PI / teeth / 4.0;
    for (i=0; i<teeth; i++) {
	angle = i * 2.0*M_PI / teeth;

	glVertex3f(r1*cos(angle),      r1*sin(angle),      width*0.5);
	glVertex3f(r2*cos(angle+da),   r2*sin(angle+da),   width*0.5);
	glVertex3f(r2*cos(angle+2*da), r2*sin(angle+2*da), width*0.5);
	glVertex3f(r1*cos(angle+3*da), r1*sin(angle+3*da), width*0.5);
    }
    glEnd();

    glNormal3f(0.0, 0.0, -1.0);

    /* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (i=0; i<=teeth; i++) {
	angle = i * 2.0*M_PI / teeth;
	glVertex3f(r1*cos(angle), r1*sin(angle), -width*0.5);
	glVertex3f(r0*cos(angle), r0*sin(angle), -width*0.5);
	glVertex3f(r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5);
	glVertex3f(r0*cos(angle), r0*sin(angle), -width*0.5);
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0*M_PI / teeth / 4.0;
    for (i=0; i<teeth; i++) {
	angle = i * 2.0*M_PI / teeth;

	glVertex3f(r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5);
	glVertex3f(r2*cos(angle+2*da), r2*sin(angle+2*da), -width*0.5);
	glVertex3f(r2*cos(angle+da),   r2*sin(angle+da),   -width*0.5);
	glVertex3f(r1*cos(angle),      r1*sin(angle),      -width*0.5);
    }
    glEnd();

    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i=0; i<teeth; i++) {
	angle = i * 2.0*M_PI / teeth;

	glVertex3f(r1*cos(angle),      r1*sin(angle),       width*0.5);
	glVertex3f(r1*cos(angle),      r1*sin(angle),      -width*0.5);
	u = r2*cos(angle+da) - r1*cos(angle);
	v = r2*sin(angle+da) - r1*sin(angle);
	len = sqrt(u*u + v*v);
	u /= len;
	v /= len;
	glNormal3f(v, -u, 0.0);
	glVertex3f(r2*cos(angle+da),   r2*sin(angle+da),    width*0.5);
	glVertex3f(r2*cos(angle+da),   r2*sin(angle+da),   -width*0.5);
	glNormal3f(cos(angle), sin(angle), 0.0);
	glVertex3f(r2*cos(angle+2*da), r2*sin(angle+2*da),  width*0.5);
	glVertex3f(r2*cos(angle+2*da), r2*sin(angle+2*da), -width*0.5);
	u = r1*cos(angle+3*da) - r2*cos(angle+2*da);
	v = r1*sin(angle+3*da) - r2*sin(angle+2*da);
	glNormal3f(v, -u, 0.0);
	glVertex3f(r1*cos(angle+3*da), r1*sin(angle+3*da),  width*0.5);
	glVertex3f(r1*cos(angle+3*da), r1*sin(angle+3*da), -width*0.5);
	glNormal3f(cos(angle), sin(angle), 0.0);
    }

    glVertex3f((float)(r1*cos(0.f)), (float)(r1*sin(0.f)), (float)(width*0.5f));
    glVertex3f((float)(r1*cos(0.f)), (float)(r1*sin(0.f)), (float)(-width*0.5f));

    glEnd();

    glShadeModel(GL_SMOOTH);

    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i=0; i<=teeth; i++) {
	angle = i * 2.0*M_PI / teeth;
	glNormal3f(-cos(angle), -sin(angle), 0.0);
	glVertex3f(r0*cos(angle), r0*sin(angle), -width*0.5);
	glVertex3f(r0*cos(angle), r0*sin(angle), width*0.5);
    }
    glEnd();

}

static GLfloat view_rotx=20.0, view_roty=30.0;
static GLfloat angle = 0.0;

void draw()
{
    static GLfloat pos[4] = {5.0, 5.0, 10.0, 0.0 };
    static GLfloat red[4] = {0.8, 0.1, 0.0, 1.0 };
    static GLfloat green[4] = {0.0, 0.8, 0.2, 1.0 };
    static GLfloat blue[4] = {0.2, 0.2, 1.0, 1.0 };

    angle += 2.0;

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);

    /* make the gears */

    glPushMatrix();
    glRotatef(view_rotx, 1.0, 0.0, 0.0);
    glRotatef(view_roty, 0.0, 1.0, 0.0);

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
    glPushMatrix();
    glTranslatef(-3.0, -2.0, 0.0);
    glRotatef(angle, 0.0, 0.0, 1.0);
    gear(1.0, 4.0, 1.0, 20, 0.7);
    glPopMatrix();

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
    glPushMatrix();
    glTranslatef(3.1, -2.0, 0.0);
    glRotatef(-2.0*angle-9.0, 0.0, 0.0, 1.0);
    gear(0.5, 2.0, 2.0, 10, 0.7);
    glPopMatrix();

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
    glPushMatrix();
    glTranslatef(-3.1, 4.2, 0.0);
    glRotatef(-2.0*angle-25.0, 0.0, 0.0, 1.0);
    gear(1.3, 2.0, 0.5, 10, 0.7);
    glPopMatrix();

    glPopMatrix();
}

int main(int argc, char **argv)
{
    SDL_Surface* screen;
    unsigned int pitch;
    int	mode;
    int winSizeX = 640;
    int winSizeY = 480;
    OSMesaContext ctx;
    void *frameBuffer;
    void *frameBuffer_inv;
    GLfloat h;
    unsigned int frames;
    unsigned int tNow;
    unsigned int tLastFps;
    int isRunning;
    SDL_Event evt;
    char* sdl_error;
    const SDL_VideoInfo* info;

    // initialize SDL video:
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	fprintf(stderr,"ERROR: cannot initialize SDL video.\n");
	return 1;
    }
    screen = NULL;
    info = SDL_GetVideoInfo();
    if ((screen=SDL_SetVideoMode(winSizeX, winSizeY, info->vfmt->BitsPerPixel, SDL_SWSURFACE)) == 0) {
	fprintf(stderr,"ERROR: Video mode set failed.\n");
	return 1;
    }
    //SDL_ShowCursor(SDL_DISABLE);
    SDL_WM_SetCaption(glGetString(GL_RENDERER),0);

    ctx = OSMesaCreateContextExt( OSMESA_RGBA, 16, 0, 0, NULL );
    if (!ctx) {
	printf("OSMesaCreateContext failed!\n");
	exit(1);
    }

    frameBuffer = malloc(winSizeX * winSizeY * sizeof(long));
    if (!OSMesaMakeCurrent(ctx, frameBuffer, GL_UNSIGNED_BYTE, winSizeX, winSizeY)) {
	printf("OSMesaMakeCurrent failed!\n");
	exit(1);
    }
    frameBuffer_inv = malloc(winSizeX * winSizeY * sizeof(long));

    // initialize GL:
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glViewport(0, 0, winSizeX, winSizeY);
    glEnable(GL_DEPTH_TEST);
    h = (GLfloat) winSizeY / (GLfloat) winSizeX;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -45.0);
    glEnable(GL_NORMALIZE);

    // variables for timing:
    frames = 0;
    tNow = SDL_GetTicks();
    tLastFps = tNow;

    // main loop:
    isRunning = 1;
    while (isRunning) {
	++frames;
	tNow = SDL_GetTicks();
	// do event handling:
	while (SDL_PollEvent(&evt)) {
	    switch (evt.type) {
		case SDL_KEYDOWN:
		    switch (evt.key.keysym.sym) {
			case SDLK_UP:
			    view_rotx += 5.0;
			    break;
			case SDLK_DOWN:
			    view_rotx -= 5.0;
			    break;
			case SDLK_LEFT:
			    view_roty += 5.0;
			    break;
			case SDLK_RIGHT:
			    view_roty -= 5.0;
			    break;
			case SDLK_ESCAPE :
			case SDLK_q :
			    isRunning = 0;
			default:
			    break;
		    }
		    break;
		case SDL_QUIT:
		    isRunning = 0;
		    break;
	    }
	}

	// draw scene:

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	draw();

	// swap buffers:
	if (SDL_MUSTLOCK(screen) && (SDL_LockSurface(screen)<0)) {
	    fprintf(stderr, "SDL ERROR: Can't lock screen: %s\n", SDL_GetError());
	    return 1;
	}

	glReadPixels(0, 0, winSizeX, winSizeY, GL_RGBA, GL_UNSIGNED_BYTE, screen->pixels);

	// Image will be upside-down from SDL's perspective, flip manually using an RGBA
	// variation of the math from:
	// https://github.com/vallentin/GLCollection/blob/master/screenshot.cpp
	//
	// Apparently this is necessary, since OpenGL doesn't provide a built-in way
	// to handle this:
	// https://www.opengl.org/archives/resources/features/KilgardTechniques/oglpitfall/
	for (int y = 0; y < winSizeY / 2; ++y) {
	    for (int x = 0; x < winSizeX; ++x) {
		int top = (x + y * winSizeX) * 4;
		int bottom = (x + (winSizeY - y - 1) * winSizeX) * 4;
		char rgba[4];
		memcpy(rgba, screen->pixels + top, sizeof(rgba));
		memcpy(screen->pixels + top, screen->pixels + bottom, sizeof(rgba));
		memcpy(screen->pixels + bottom, rgba, sizeof(rgba));
	    }
	}

	if (SDL_MUSTLOCK(screen)) {
	    SDL_UnlockSurface(screen);
	}
	SDL_Flip(screen);

	// check for error conditions:
	sdl_error = SDL_GetError();
	if (sdl_error[0] != '\0') {
	    fprintf(stderr,"SDL ERROR: \"%s\"\n",sdl_error);
	    SDL_ClearError();
	}

	// update fps:
	if (tNow >= tLastFps+5000) {
	    printf("%i frames in %f secs, %f frames per second.\n",frames,(float)(tNow-tLastFps)*0.001f,(float)frames*1000.0f/(float)(tNow-tLastFps));
	    tLastFps=tNow;
	    frames = 0;
	}
    }
    printf("%i frames in %f secs, %f frames per second.\n",frames,(float)(tNow-tLastFps)*0.001f,(float)frames*1000.0f/(float)(tNow-tLastFps));

    // cleanup:
    OSMesaDestroyContext( ctx );
    free(frameBuffer);

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    SDL_Quit();

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
