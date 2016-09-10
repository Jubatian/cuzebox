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



#ifdef USE_SDL1

/* SDL screen */
static SDL_Surface*  guicore_surface;

#else

/* SDL window */
static SDL_Window*   guicore_window;
/* SDL renderer */
static SDL_Renderer* guicore_renderer;
/* SDL texture which is used to interface with the renderer */
static SDL_Texture*  guicore_texture;

#endif

/* Target pixel buffer */
static uint32        guicore_pixels[640U * 560U];

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

#ifdef __EMSCRIPTEN__

 EM_ASM(
  SDL.defaults.copyOnLock = false;
  SDL.defaults.discardOnLock = true;
  SDL.defaults.opaqueFrontBuffer = false;
 );

#endif

 if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_n;
 }

#ifdef USE_SDL1

 guicore_surface = SDL_SetVideoMode(640U, 560U, 32U, SDL_HWSURFACE);
 if (guicore_surface == NULL){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_qt;
 }

 for (i = 0U; i < 256U; i++){
  r = (((i >> 0) & 7U) * 255U) / 7U;
  g = (((i >> 3) & 7U) * 255U) / 7U;
  b = (((i >> 6) & 3U) * 255U) / 3U;
  guicore_palette[i] = SDL_MapRGB(guicore_surface->format, r, g, b);
 }

#else

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

 guicore_texture = SDL_CreateTexture(
     guicore_renderer,
     SDL_PIXELFORMAT_RGBA8888,
     SDL_TEXTUREACCESS_STREAMING,
     640U,
     560U);
 if (guicore_texture == NULL){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_ren;
 }

 SDL_RenderClear(guicore_renderer);
 SDL_RenderPresent(guicore_renderer);

 for (i = 0U; i < 256U; i++){
  r = (((i >> 0) & 7U) * 255U) / 7U;
  g = (((i >> 3) & 7U) * 255U) / 7U;
  b = (((i >> 6) & 3U) * 255U) / 3U;
  guicore_palette[i] = (r << 24) | (g << 16) | (b << 8) | 0xFFU;
 }

#endif

 return TRUE;

#ifdef USE_SDL1

#else

fail_ren:
 SDL_DestroyRenderer(guicore_renderer);
fail_wnd:
 SDL_DestroyWindow(guicore_window);

#endif

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
#ifdef USE_SDL1

#else

 SDL_DestroyTexture(guicore_texture);
 SDL_DestroyRenderer(guicore_renderer);
 SDL_DestroyWindow(guicore_window);

#endif

 SDL_Quit();
}



/*
** Retrieves 640 x 560 GUI surface's pixel buffer. A matching
** guicore_relpixbuf() must be made after rendering. It may return NULL if it
** is not possible to lock the pixel buffer for access.
*/
uint32* guicore_getpixbuf(void)
{
 return &(guicore_pixels[0]);
}



/*
** Releases the pixel buffer.
*/
void  guicore_relpixbuf(void)
{
}



/*
** Retrieves pitch of GUI surface
*/
auint guicore_getpitch(void)
{
 return 640U;
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
#ifdef USE_SDL1

 auint i;

 if (drop){ return; }

 if (SDL_LockSurface(guicore_surface) == 0){
  for (i = 0U; i < 560U; i++){
   memcpy( (char*)(guicore_surface->pixels) + ((guicore_surface->pitch) * i),
           &(guicore_pixels[640U * i]),
           640U * sizeof(uint32) );
  }
  SDL_UnlockSurface(guicore_surface);
 }
 SDL_UpdateRect(guicore_surface, 0, 0, 0, 0);

#else

 auint i;
 void* pixels;
 int   pitch;

 if (drop){ return; }

 if (SDL_LockTexture(guicore_texture, NULL, &pixels, &pitch) == 0){
  for (i = 0U; i < 560U; i++){
   memcpy( (char*)(pixels) + ((auint)(pitch) * i),
           &(guicore_pixels[640U * i]),
           640U * sizeof(uint32) );
  }
  SDL_UnlockTexture(guicore_texture);
 }

 SDL_RenderClear(guicore_renderer);
 SDL_RenderCopy(guicore_renderer, guicore_texture, NULL, NULL);
 SDL_RenderPresent(guicore_renderer);

#endif
}



/*
** Sets the window caption (if any can be displayed)
*/
void guicore_setcaption(const char* title)
{
#ifndef __EMSCRIPTEN__

#ifdef USE_SDL1

 SDL_WM_SetCaption(title, NULL);

#else

 SDL_SetWindowTitle(guicore_window, title);

#endif

#endif
}
