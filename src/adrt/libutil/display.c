/*                     D I S P L A Y . C
 *
 * @file display.c
 *
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 *  Comments -
 *      Utilities Library - Display (SDL Stuff)
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

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
  if(!SDL_WasInit(SDL_INIT_VIDEO))
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

  SDL_EnableUNICODE(1);

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


void util_display_console(char **command_buffer, int *command_lines, char **console_buffer, int *console_lines, void (*fcb_cmd)(char *commmand, char *response)) {
  SDL_Event event;
  SDL_Rect rect;
  unsigned int color;
  int i, ind, h_ind;
  char command[80];
  char history[80];
  char response[1024];

  command[0] = 0;

  ind = 0;
  h_ind = 0;

  /* Keyboard handling */
  while(SDL_WaitEvent(&event) >= 0) {
    /* Draw Command Window */
    rect.x = 0;
    rect.y = 0;
    rect.w = util_display_screen_w;
    rect.h = util_display_screen_h;
    color = 0xff000080;
    SDL_FillRect(util_display_screen, &rect, color);

    for(i = 0; i < *console_lines; i++)
      util_display_text(console_buffer[i], 1, *console_lines - i, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_BOTTOM);

    /* Cursor */
    rect.x = UTIL_DISPLAY_FONT_WIDTH*(ind + 1); /* +1 for '>' */
    rect.y = util_display_screen_h - UTIL_DISPLAY_FONT_HEIGHT;
    rect.w = UTIL_DISPLAY_FONT_WIDTH;
    rect.h = UTIL_DISPLAY_FONT_HEIGHT;
    color = 0xffffffff;
    SDL_FillRect(util_display_screen, &rect, color);

    /* Input */
    util_display_text(">", 0, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_BOTTOM);
    util_display_text(command, 1, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_BOTTOM);

    SDL_Flip(util_display_screen);

    switch(event.type) {
      case SDL_KEYDOWN:
        switch(event.key.keysym.sym) {
          case SDLK_BACKQUOTE:
            return;
            break;

          case SDLK_HOME:
            ind = 0;
            break;

          case SDLK_END:
            ind = strlen(command);
            break;

          case SDLK_LEFT:
            if(ind)
              ind--;
            break;

          case SDLK_RIGHT:
            if(ind < strlen(command))
              ind++;
            break;

          case SDLK_UP:
            if(h_ind) {
              if(h_ind == *command_lines)
                strcpy(history, command);
              strcpy(command, command_buffer[h_ind-1]);
              ind = strlen(command);

              if(h_ind)
                h_ind--;
            }
            break;

          case SDLK_DOWN:
            if(h_ind < *command_lines) {
              h_ind++;
              if(h_ind == *command_lines) {
                strcpy(command, history);
              } else {
                strcpy(command, command_buffer[h_ind]);
              }
              ind = strlen(command);
            }
            break;

          case SDLK_BACKSPACE:
            if(ind) {
              for(i = ind-1; i < strlen(command); i++)
                command[i] = command[i+1];
                ind--;
            }
            break;

          case SDLK_RETURN:
            {
              char *lines[16];
              int n;

              response[0] = 0;
              if(strlen(command))
                fcb_cmd(command, response);

              /* Append command to the history and console */
              strcpy(command_buffer[(*command_lines)++], command);
              strcpy(console_buffer[(*console_lines)++], command);

              /* Place the response onto the console */
              n = 0;
              if(strlen(response)) {
                lines[0] = strtok(response, "\n");
                if(lines[0]) {
                  for(n = 1; n < 16; n++) {
                    lines[n] = strtok(NULL, "\n");
                    if(!lines[n])
                      break;
                  }
                }
                for(i = 0; i < n; i++)
                  strcpy(console_buffer[(*console_lines)++], lines[i]);
              }

              command[0] = 0;
              ind = 0;
              h_ind = *command_lines;
            }
            break;

          default:
            if(ind < 80)
              if(event.key.keysym.unicode & 0x7F) {
                for(i = strlen(command); i >= ind; i--)
                  command[i+1] = command[i];
                command[ind++] = event.key.keysym.unicode & 0x7F;
              }
            break;
        }
        break;

      default:
        break;
    }
  }
}
