#include "display.h"
#include <stdlib.h>
#include <string.h>
#include "font.h"

#define UTIL_DISPLAY_FONT_WIDTH 8
#define	UTIL_DISPLAY_FONT_HEIGHT 15


SDL_Surface *util_display_screen;
SDL_Surface *util_display_buffer;
SDL_Surface *util_display_font;
SDL_Rect util_display_rect;

int util_display_screen_w;
int util_display_screen_h;


void util_display_init(int w, int h) {
  util_display_screen_w = w;
  util_display_screen_h = h;

  /* Initialize the SDL library */
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
    fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
    exit(1);
  }

  util_display_screen = SDL_SetVideoMode(util_display_screen_w, util_display_screen_h, 32, SDL_SWSURFACE);

  util_display_buffer = SDL_CreateRGBSurface(SDL_SWSURFACE, util_display_screen_w, util_display_screen_h, 24,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                                             0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000);
#else
                                             0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000);
#endif

  util_display_rect.x = 0;
  util_display_rect.y = 0;
  util_display_rect.w = util_display_screen_w;
  util_display_rect.h = util_display_screen_h;

  if(util_display_screen == NULL) {
    fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
    exit(1);
  }

  util_display_font = SDL_CreateRGBSurface(SDL_SWSURFACE, util_font.width, util_font.height, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                                             0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
#else
                                             0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
#endif

/*  printf("util_font width: %d, %d\n", util_font.width, util_font.height); */
  memcpy(util_display_font->pixels, util_font.pixel_data, util_font.width * util_font.height * 4);

  /* Clean up on exit */
  atexit(SDL_Quit);
}


void util_display_free() {
  SDL_FreeSurface(util_display_screen);
  SDL_FreeSurface(util_display_buffer);
  SDL_FreeSurface(util_display_font);
}


void util_display_draw(void *frame) {
  memcpy(util_display_buffer->pixels, frame, util_display_screen_w * util_display_screen_h * 3);
  SDL_BlitSurface(util_display_buffer, &util_display_rect, util_display_screen, &util_display_rect);
}


void util_display_text(char *text, int x, int y, int jh, int jv) {
  SDL_Rect font, dest;
  int i, loc_x, loc_y;

  if(jh == UTIL_JUSTIFY_LEFT) {
    loc_x = 0 + x*UTIL_DISPLAY_FONT_WIDTH;
  } else {
    loc_x = util_display_screen_w - x - UTIL_DISPLAY_FONT_WIDTH*strlen(text);
  }

  if(jv == UTIL_JUSTIFY_TOP) {
    loc_y = 0 + y*UTIL_DISPLAY_FONT_HEIGHT;
  } else {
    loc_y = util_display_screen_h - (y+1)*UTIL_DISPLAY_FONT_HEIGHT;
  }

  for(i = 0; i < strlen(text); i++) {
    font.x = UTIL_DISPLAY_FONT_WIDTH*text[i];
    font.y = 0;
    font.w = UTIL_DISPLAY_FONT_WIDTH;
    font.h = UTIL_DISPLAY_FONT_HEIGHT;

    dest.x = loc_x + i*UTIL_DISPLAY_FONT_WIDTH;
    dest.y = loc_y;
    dest.w = UTIL_DISPLAY_FONT_WIDTH;
    dest.h = UTIL_DISPLAY_FONT_HEIGHT;

    SDL_BlitSurface(util_display_font, &font, util_display_screen, &dest);
  }
}

void util_display_flush() {
  memset(util_display_screen->pixels, 0, 4 * util_display_screen_w * util_display_screen_h);
}


/* Draw 32x32 cross-hairs */
void util_display_cross() {
  int i, n, ind;

  /* Horizontal */
  for(n = -1; n <= 1; n++) {
    ind = 4*(util_display_screen_w * (util_display_screen_h / 2) + (util_display_screen_w - 32) / 2 + n*util_display_screen_w);
    for(i = 0; i < 33; i++) {
      ((unsigned char *)util_display_screen->pixels)[ind+4*i+0] = 255 - ((unsigned char *)util_display_screen->pixels)[ind+4*i+0];
      ((unsigned char *)util_display_screen->pixels)[ind+4*i+1] = 255 - ((unsigned char *)util_display_screen->pixels)[ind+4*i+1];
      ((unsigned char *)util_display_screen->pixels)[ind+4*i+2] = 255 - ((unsigned char *)util_display_screen->pixels)[ind+4*i+2];
      ((unsigned char *)util_display_screen->pixels)[ind+4*i+3] = 0;
    }
  }

  /* Vertical */
  for(n = -1; n <= 1; n++) {
    ind = 4*(util_display_screen_w * ((util_display_screen_h - 32) / 2) + (util_display_screen_w / 2) + n);
    for(i = 0; i < 33; i++) {
      ((unsigned char *)util_display_screen->pixels)[ind+4*i*util_display_screen_w+0] = 255 - ((unsigned char *)util_display_screen->pixels)[ind+4*i+0];
      ((unsigned char *)util_display_screen->pixels)[ind+4*i*util_display_screen_w+1] = 255 - ((unsigned char *)util_display_screen->pixels)[ind+4*i+1];
      ((unsigned char *)util_display_screen->pixels)[ind+4*i*util_display_screen_w+2] = 255 - ((unsigned char *)util_display_screen->pixels)[ind+4*i+2];
      ((unsigned char *)util_display_screen->pixels)[ind+4*i*util_display_screen_w+3] = 0;
    }
  }
}


void util_display_flip() {
  SDL_Flip(util_display_screen);
}


void util_display_console(void (*fcb_cmd)(char *cmd)) {
#if 0
  SDL_Event event;
  SDL_Rect rect;
  unsigned int color;
  int i, ind;

  ind = strlen(text) - 1;

  /* Keyboard handling */
  while(SDL_WaitEvent(&event) >= 0) {
    /* Box */
    rect.x = UTIL_DISPLAY_FONT_WIDTH-3;
    rect.y = UTIL_DISPLAY_FONT_HEIGHT-3;
    rect.w = (width+2) * UTIL_DISPLAY_FONT_WIDTH+6;
    rect.h = 3*UTIL_DISPLAY_FONT_HEIGHT+6;
    color = 0xff606060;
    SDL_FillRect(util_display_screen, &rect, color);

    /* Title */
    rect.x = UTIL_DISPLAY_FONT_WIDTH-1;
    rect.y = 1*UTIL_DISPLAY_FONT_HEIGHT-1;
    rect.w = (width+2) * UTIL_DISPLAY_FONT_WIDTH+2;
    rect.h = 1*UTIL_DISPLAY_FONT_HEIGHT+2;
    color = 0xff808080;
    SDL_FillRect(util_display_screen, &rect, color);
    util_display_text(title, 2, 1, 0, 0);

    /* Input */
    rect.x = UTIL_DISPLAY_FONT_WIDTH-1;
    rect.y = 3*UTIL_DISPLAY_FONT_HEIGHT-1;
    rect.w = (width+2) * UTIL_DISPLAY_FONT_WIDTH+2;
    rect.h = 1*UTIL_DISPLAY_FONT_HEIGHT+2;
    color = 0xff404040;
    SDL_FillRect(util_display_screen, &rect, color);
    util_display_text(text, 2, 3, 0, 0);

    /* Cursor */
    rect.x = UTIL_DISPLAY_FONT_WIDTH*(3+ind)+1;
    rect.y = 3*UTIL_DISPLAY_FONT_HEIGHT+1;
    rect.w = 2;
    rect.h = 1*UTIL_DISPLAY_FONT_HEIGHT-2;
    color = 0xffB0B0B0;
    SDL_FillRect(util_display_screen, &rect, color);

    rect.x = UTIL_DISPLAY_FONT_WIDTH-3;
    rect.y = UTIL_DISPLAY_FONT_HEIGHT-3;
    rect.w = (width+2) * UTIL_DISPLAY_FONT_WIDTH+6;
    rect.h = 3*UTIL_DISPLAY_FONT_HEIGHT+6;
    SDL_UpdateRect(util_display_screen, rect.x, rect.y, rect.w, rect.h);

    switch(event.type) {
      case SDL_KEYDOWN:
        switch(event.key.keysym.sym) {
          case SDLK_HOME:
            ind = -1;
            break;

          case SDLK_END:
            ind = strlen(text)-1;
            break;

          case SDLK_LEFT:
            if(ind)
              ind--;
            break;

          case SDLK_RIGHT:
            if(ind+1 < strlen(text))
              ind++;
            break;

          case SDLK_RETURN:
            return;
            break;

          case SDLK_BACKSPACE:
            if(event.key.keysym.mod & KMOD_ALT) {
              text[0] = NULL;
              ind = -1;
            } else {
              if(ind >= 0) {
                /* shift characters down */
                for(i = ind; i < width; i++)
                  text[i] = text[i+1];
                ind--;
              }
            }
            break;

          default:
            if(event.key.keysym.sym >= 32 && event.key.keysym.sym < 127) {
              if(ind+1 < width) {
                /* shift characters down */
                for(i = width-1; i > ind; i--)
                  text[i] = text[i-1];
                text[++ind] = event.key.keysym.sym;
              }
            }
            break;
        }
        break;

        default:
          break;
    }
  }
#endif
}
