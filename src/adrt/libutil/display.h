#ifndef _UTIL_DISPLAY_H
#define _UTIL_DISPLAY_H

#include <SDL.h>

#define UTIL_JUSTIFY_LEFT	0
#define UTIL_JUSTIFY_RIGHT	1
#define UTIL_JUSTIFY_TOP	0
#define UTIL_JUSTIFY_BOTTOM	1

#define UTIL_JUSTIFY_LEFT	0
#define UTIL_JUSTIFY_RIGHT	1
#define UTIL_JUSTIFY_TOP	0
#define UTIL_JUSTIFY_BOTTOM	1

void util_display_init(int w, int h);
void util_display_free(void);
void util_display_draw(void *frame);
void util_display_text(char *text, int x, int y, int jh, int jv);
void util_display_flush(void);
void util_display_cross(void);
void util_display_flip(void);
void util_display_console(void (*fcb_cmd)(char *cmd));

extern SDL_Surface *util_display_screen;

#endif
