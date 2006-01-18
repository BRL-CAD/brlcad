/*                     D I S P L A Y . C
 *
 * @file display.c
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
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


#if 0
{
SDL_Rect **modes;
int i;

/* Get available fullscreen/hardware modes */
modes=SDL_ListModes(NULL, SDL_HWSURFACE | SDL_DOUBLEBUF);

/* Check is there are any modes available */
if(modes == (SDL_Rect **)0){
  printf("No modes available!\n");
  exit(-1);
}


/* Check if our resolution is restricted */
if(modes == (SDL_Rect **)-1){
  printf("All resolutions available.\n");
} else{
  /* Print valid modes */
  printf("Available Modes\n");
  for(i=0;modes[i];++i)
    printf("  %d x %d\n", modes[i]->w, modes[i]->h);
}
}
#endif


  /*  util_display_screen = SDL_SetVideoMode(util_display_screen_w, util_display_screen_h, 32, SDL_HWSURFACE | SDL_DOUBLEBUF); */
  util_display_screen = SDL_SetVideoMode(util_display_screen_w, util_display_screen_h, 32, SDL_SWSURFACE);


  util_display_screen = SDL_SetVideoMode(util_display_screen_w, util_display_screen_h, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
  /*  util_display_screen = SDL_SetVideoMode(util_display_screen_w, util_display_screen_h, 32, SDL_SWSURFACE); */

  util_display_buffer = SDL_CreateRGBSurface(SDL_HWSURFACE, util_display_screen_w, util_display_screen_h, 24,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                                             0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000);
#else
                                             0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000);
#endif


#if 0
{
  const SDL_VideoInfo *foo;

  foo = SDL_GetVideoInfo();
  printf("hw: %d %d %d %d\n", foo->hw_available, foo->blit_hw, foo->blit_hw_CC, foo->blit_hw_A);
}
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
  SDL_EnableKeyRepeat(200, 33);

  /* Clean up on exit */
  atexit(SDL_Quit);
}


void util_display_free() {
  SDL_FreeSurface(util_display_screen);
  SDL_FreeSurface(util_display_buffer);
  SDL_FreeSurface(util_display_font);
}


void util_display_draw(void *frame) {
/*  memcpy(util_display_buffer->pixels, frame, util_display_screen_w * util_display_screen_h * 3); */
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


void util_display_editor(char **content_buffer, int *content_lines, char **console_buffer, int *console_lines, void (*fcb_process)(char *content, char *response)) {
  SDL_Event event;
  SDL_Rect rect;
  int i, h_ind, v_ind, console_y;
  char paste[80];


  h_ind = 0;
  v_ind = 0;
  paste[0] = 0;
  console_y = (2 * (util_display_screen_h / UTIL_DISPLAY_FONT_HEIGHT)) / 3 + 1;

  /* Keyboard handling */
  while(SDL_WaitEvent(&event) >= 0) {
    /* Draw Content Window Blue */
    rect.x = 0;
    rect.y = 0;
    rect.w = util_display_screen_w;
    rect.h = util_display_screen_h;
    SDL_FillRect(util_display_screen, &rect, 0xff000040);

    /* Cursor */
    rect.x = UTIL_DISPLAY_FONT_WIDTH*(h_ind + 1); /* +1 for '>' */
    rect.y = UTIL_DISPLAY_FONT_HEIGHT*(v_ind + 1);
    rect.w = UTIL_DISPLAY_FONT_WIDTH;
    rect.h = UTIL_DISPLAY_FONT_HEIGHT;
    SDL_FillRect(util_display_screen, &rect, 0xffffffff);

    /* Content */
    util_display_text("[Content]", 0, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);
    for(i = 0; i <= *content_lines; i++)
      util_display_text(content_buffer[i], 1, i + 1, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);

    /* Console */
    util_display_text("[Console]", 0, console_y, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);
    for(i = 0; i < *console_lines; i++)
      util_display_text(console_buffer[i], 1, i + console_y + 1, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);

    SDL_Flip(util_display_screen);

    switch(event.type) {
      case SDL_KEYDOWN:
        switch(event.key.keysym.sym) {
          case SDLK_BACKQUOTE:
            return;
            break;

          case SDLK_HOME:
            h_ind = 0;
            break;

          case SDLK_END:
            h_ind = strlen(content_buffer[v_ind]);
            break;

          case SDLK_PAGEUP:
            v_ind = 0;
            break;

          case SDLK_PAGEDOWN:
            v_ind = *content_lines;
            break;

          case SDLK_LEFT:
            if(h_ind)
              h_ind--;
            break;

          case SDLK_RIGHT:
            if(h_ind < strlen(content_buffer[v_ind]))
              h_ind++;
            break;

          case SDLK_DOWN:
            if(v_ind < *content_lines)
              v_ind++;

            if(h_ind > strlen(content_buffer[v_ind]))
              h_ind = strlen(content_buffer[v_ind]);
            break;

          case SDLK_UP:
            if(v_ind)
              v_ind--;

            if(h_ind > strlen(content_buffer[v_ind]))
              h_ind = strlen(content_buffer[v_ind]);
            break;

          case SDLK_BACKSPACE:
            if(h_ind) {
              for(i = h_ind-1; i < strlen(content_buffer[v_ind]); i++)
                content_buffer[v_ind][i] = content_buffer[v_ind][i+1];
              h_ind--;
            }
            break;

          case SDLK_DELETE:
            for(i = h_ind; i < strlen(content_buffer[v_ind]); i++)
              content_buffer[v_ind][i] = content_buffer[v_ind][i+1];
            break;

          case SDLK_RETURN:
            {
              h_ind = 0;
              if(v_ind == *content_lines) {
                (*content_lines)++;
                content_buffer[*content_lines][0] = 0;
              }
              v_ind++;
            }
            break;

          default:
            /* First check for any special commands */
            if(event.key.keysym.mod & KMOD_CTRL) {
              switch(event.key.keysym.sym) {
                case SDLK_p: /* process code */
                  {
                    char *code, response[1024];
                    int n;

                    code = (char *)malloc((*content_lines+1) * 80);

                    code[0] = 0;
                    for(i = 0; i <= *content_lines; i++) {
                      strcat(code, content_buffer[i]);
                      strcat(code, "\n");
                    }
                    fcb_process(code, response);

                    i = 0;
                    n = 0;
                    while(i < strlen(response)) {
                      console_buffer[*console_lines][n] = response[i];
                      console_buffer[*console_lines][n+1] = 0;
                      n++;
                      if(response[i] == '\n') {
                        (*console_lines)++;
                        n = 0;
                      }
                      i++;
                    }

                    free(code);
                  }
                  break;

                case SDLK_l: /* clear buffers */
                  content_buffer[0][0] = 0;
                  console_buffer[0][0] = 0;
                  (*content_lines) = 0;
                  (*console_lines) = 0;
                  h_ind = 0;
                  v_ind = 0;
                  break;

                case SDLK_x: /* cut */
                  strcpy(paste, content_buffer[v_ind]);
                  for(i = v_ind; i < *content_lines; i++)
                    strcpy(content_buffer[i], content_buffer[i+1]);
                  if(*content_lines) {
                    (*content_lines)--;
                  } else {
                    content_buffer[0][0] = 0;
                  }
                  h_ind = 0;
                  break;

                case SDLK_v: /* paste */
                  for(i = *content_lines; i >= v_ind; i--)
                    strcpy(content_buffer[i+1], content_buffer[i]);
                  strcpy(content_buffer[v_ind], paste);
                  (*content_lines)++;
                  h_ind = 0;
                  break;

              }
            } else {
              if(h_ind < 80)
                if(event.key.keysym.unicode & 0x7F) {
                  for(i = strlen(content_buffer[v_ind]); i >= h_ind; i--)
                    content_buffer[v_ind][i+1] = content_buffer[v_ind][i];
                  content_buffer[v_ind][h_ind++] = event.key.keysym.unicode & 0x7F;
                }
            }
            break;
        }
        break;

      default:
        break;
    }
  }
}
