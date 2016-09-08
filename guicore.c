/*
 *  GUI core elements
 *
 *  Copyright (C) 2016
 *    Sandor Zsuga (Jubatian)
 *  Uzem (the base of CUzeBox) is copyright (C)
 *    David Etherton,
 *    Eric Anderton,
 *    Alec Bourque (Uze),
 *    Filipe Rinaldi,
 *    Sandor Zsuga (Jubatian),
 *    Matt Pandina (Artcfox)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#include "guicore.h"
#include <SDL2/SDL.h>



/* SDL window */
static SDL_Window*   guicore_window;
/* SDL renderer */
static SDL_Renderer* guicore_renderer;
/* SDL surface which is used as target pixel buffer */
static SDL_Surface*  guicore_surface;
/* SDL texture which is used to interface with the renderer */
static SDL_Texture*  guicore_texture;

/* Uzebox palette */
static uint32        guicore_palette[256];

/* String constant for SDL error output */
static const char*   guicore_sdlerr = "SDL Error: %s\n";


/*
** Attempts to initialize the GUI. Returns TRUE on success. This must be
** called first before any platform specific elements as it initializes
** those.
*/
boole guicore_init(auint flags, const char* title)
{
 auint i;
 auint r;
 auint g;
 auint b;

 if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_n;
 }

 guicore_window = SDL_CreateWindow(
     title,
     SDL_WINDOWPOS_CENTERED,
     SDL_WINDOWPOS_CENTERED,
     640U,
     560U,
     SDL_WINDOW_RESIZABLE);
 if (guicore_window == NULL){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_qt;
 }

 guicore_renderer = SDL_CreateRenderer(
     guicore_window,
     -1,
     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
 if (guicore_renderer == NULL){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_wnd;
 }

 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
 if (SDL_RenderSetLogicalSize(guicore_renderer, 640U, 560U) != 0U){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_ren;
 }

 guicore_surface = SDL_CreateRGBSurface(
     0U, 640U, 560U, 32U, 0U, 0U, 0U, 0U);
 if (guicore_surface == NULL){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_ren;
 }

 guicore_texture = SDL_CreateTexture(
     guicore_renderer,
     guicore_surface->format->format,
     SDL_TEXTUREACCESS_STATIC,
     guicore_surface->w,
     guicore_surface->h);
 if (guicore_texture == NULL){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_sur;
 }

 SDL_RenderClear(guicore_renderer);
 SDL_RenderPresent(guicore_renderer);

 for (i = 0U; i < 256U; i++){
  r = (((i >> 0) & 7U) * 255U) / 7U;
  g = (((i >> 3) & 7U) * 255U) / 7U;
  b = (((i >> 6) & 3U) * 255U) / 3U;
  guicore_palette[i] = SDL_MapRGB(guicore_surface->format, r, g, b);
 }

 return TRUE;

fail_sur:
 SDL_FreeSurface(guicore_surface);
fail_ren:
 SDL_DestroyRenderer(guicore_renderer);
fail_wnd:
 SDL_DestroyWindow(guicore_window);
fail_qt:
 SDL_Quit();
fail_n:
 return FALSE;
}



/*
** Tears down the GUI.
*/
void  guicore_quit(void)
{
 SDL_DestroyTexture(guicore_texture);
 SDL_FreeSurface(guicore_surface);
 SDL_DestroyRenderer(guicore_renderer);
 SDL_DestroyWindow(guicore_window);
 SDL_Quit();
}



/*
** Retrieves 640 x 560 GUI surface's pixel buffer
*/
uint32* guicore_getpixbuf(void)
{
 return (uint32*)(guicore_surface->pixels);
}



/*
** Retrieves pitch of GUI surface
*/
auint guicore_getpitch(void)
{
 return ((guicore_surface->pitch) / sizeof(uint32));
}



/*
** Retrieves 256 color Uzebox palette which should be used to generate
** pixels. Algorithms can assume some 8 bit per channel representation, but
** channel order might vary.
*/
uint32 const* guicore_getpalette(void)
{
 return &guicore_palette[0];
}



/*
** Updates display. Normally this sticks to the display's refresh rate. If
** drop is TRUE, it does nothing (drops the frame).
*/
void guicore_update(boole drop)
{
 if (drop){ return; }
 SDL_UpdateTexture(guicore_texture, NULL,
                   guicore_surface->pixels, guicore_surface->pitch);
 SDL_RenderClear(guicore_renderer);
 SDL_RenderCopy(guicore_renderer, guicore_texture, NULL, NULL);
 SDL_RenderPresent(guicore_renderer);
}



/*
** Clears a region on the surface
*/
void guicore_clear(auint x, auint y, auint w, auint h)
{
 SDL_Rect rect;
 if (x < 640U){ rect.x = x; }else{ rect.x = 639U; }
 if (y < 560U){ rect.y = y; }else{ rect.y = 559U; }
 if (((auint)(rect.x) + w) < 640U){ rect.w = w; }else{ rect.w = 640U - (auint)(rect.x); }
 if (((auint)(rect.y) + h) < 560U){ rect.h = h; }else{ rect.h = 560U - (auint)(rect.y); }
 SDL_FillRect(guicore_surface, &rect, guicore_palette[0]);
}



/*
** Sets the window caption (if any can be displayed)
*/
void guicore_setcaption(const char* title)
{
 SDL_SetWindowTitle(guicore_window, title);
}
