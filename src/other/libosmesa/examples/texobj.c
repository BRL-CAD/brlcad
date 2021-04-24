// TinyGL texture mapping example
//
// updated by Norbert Kett (anchor@rocketmail.com)
//

#include <stdio.h>
#include "OSMesa/gl.h"
#include "OSMesa/osmesa.h"
#include <SDL/SDL.h>

static GLuint TexObj[2];
static GLfloat Angle = 0.0f;
static int cnt=0,v=0;

void draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glColor3f(1.0, 1.0, 1.0);

    /* draw first polygon */
    glPushMatrix();
    glTranslatef(-1.0, 0.0, 0.0);
    glRotatef(Angle, 0.0, 0.0, 1.0);
    glBindTexture(GL_TEXTURE_2D, TexObj[v]);

    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex2f(-1.0, -1.0);
    glTexCoord2f(1.0, 0.0);
    glVertex2f(1.0, -1.0);
    glTexCoord2f(1.0, 1.0);
    glVertex2f(1.0, 1.0);
    glTexCoord2f(0.0, 1.0);
    glVertex2f(-1.0, 1.0);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();

    /* draw second polygon */
    glPushMatrix();
    glTranslatef(1.0, 0.0, 0.0);
    glRotatef(Angle - 90.0, 0.0, 1.0, 0.0);

    glBindTexture(GL_TEXTURE_2D, TexObj[1-v]);

    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex2f(-1.0, -1.0);
    glTexCoord2f(1.0, 0.0);
    glVertex2f(1.0, -1.0);
    glTexCoord2f(1.0, 1.0);
    glVertex2f(1.0, 1.0);
    glTexCoord2f(0.0, 1.0);
    glVertex2f(-1.0, 1.0);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
}

void reshape(int width, int height)
{
    glViewport(0, 0, (GLint) width, (GLint) height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-2.0, 2.0, -2.0, 2.0, 6.0, 20.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -8.0);
}

void bind_texture(int texobj,int image)
{
    static int width = 8, height = 8;
    static int color[2][3]= {
	{255,0,0},
	{0,255,0},
    };
    GLubyte tex[64][3];
    static GLubyte texchar[2][8*8] = {
	{
	    0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 1, 0, 0, 0,
	    0, 0, 0, 1, 1, 0, 0, 0,
	    0, 0, 0, 0, 1, 0, 0, 0,
	    0, 0, 0, 0, 1, 0, 0, 0,
	    0, 0, 0, 0, 1, 0, 0, 0,
	    0, 0, 0, 1, 1, 1, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0
	},
	{
	    0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 2, 2, 0, 0, 0,
	    0, 0, 2, 0, 0, 2, 0, 0,
	    0, 0, 0, 0, 0, 2, 0, 0,
	    0, 0, 0, 0, 2, 0, 0, 0,
	    0, 0, 0, 2, 0, 0, 0, 0,
	    0, 0, 2, 2, 2, 2, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0
	}
    };

    int i,j;

    glBindTexture(GL_TEXTURE_2D, texobj);

    /* red on white */
    for (i = 0; i < height; i++) {
	for (j = 0; j < width; j++) {
	    int p = i * width + j;
	    if (texchar[image][(height - i - 1) * width + j]) {
		tex[p][0] = color[image][0];
		tex[p][1] = color[image][1];
		tex[p][2] = color[image][2];
	    } else {
		tex[p][0] = 255;
		tex[p][1] = 255;
		tex[p][2] = 255;
	    }
	}
    }
    glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void init()
{
    glEnable(GL_DEPTH_TEST);

    /* Setup texturing */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    /* generate texture object IDs */
    glGenTextures(2, TexObj);
    bind_texture(TexObj[0],0);
    bind_texture(TexObj[1],1);
}

void idle()
{
    Angle += 2.0;

    if (++cnt==5) {
	cnt=0;
	//v=!v;
    }
    draw();
}

int main(int argc, char **argv)
{
    // initialize SDL video:
    int winSizeX = 640;
    int winSizeY = 480;
    SDL_Surface* screen;
    unsigned int pitch;
    int	mode;
    int frames;
    int x;
    double t, t0, fps;
    char titlestr[ 200 ];
    int running;
    OSMesaContext ctx;
    void *frameBuffer;
    SDL_Event evt;
    const SDL_VideoInfo* info;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	fprintf(stderr,"ERROR: cannot initialize SDL video.\n");
	return 1;
    }
    info = SDL_GetVideoInfo();
    screen = NULL;
    if ((screen=SDL_SetVideoMode(winSizeX, winSizeY, info->vfmt->BitsPerPixel, SDL_SWSURFACE)) == 0) {
	fprintf(stderr,"ERROR: Video mode set failed.\n");
	return 1;
    }

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

    init();
    reshape(winSizeX, winSizeY);

    // main loop:
    frames = 0;
    x = 0;
    running = GL_TRUE;
    t0 = (double)SDL_GetTicks()/1000.0;
    while (running) {
	// calculate and display FPS (frames per second):
	t = (double)SDL_GetTicks()/1000.0;
	if ((t-t0) > 1.0 || frames == 0) {
	    fps = (double)frames / (t-t0);
	    sprintf(titlestr, "Texture Mapping (%.1f FPS)", fps);
	    SDL_WM_SetCaption(titlestr,0);
	    t0 = t;
	    frames = 0;
	}
	++frames;

	idle();

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

	// check if the ESC key was pressed or the window was closed:

	while (SDL_PollEvent(&evt)) {
	    switch (evt.type) {
		case SDL_KEYDOWN:
		    if (evt.key.keysym.sym == SDLK_ESCAPE) {
			running = 0;
		    }
		    break;
		case SDL_QUIT:
		    running = 0;
		    break;
	    }
	}
    }

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
