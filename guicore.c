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



/* Display initialized internal marker in flags */
#define GUICORE_INIT 0x10000U

/* Window dimensions, double scan */
#define WND_W    640U
#define WND_H    560U
/* Window dimensions, single scan */
#define WNDS_W   320U
#define WNDS_H   280U
/* Window dimensions, double scan, game only */
#define WNDG_W   620U
#define WNDG_H   456U
/* Window dimensions, single scan, game only */
#define WNDSG_W  310U
#define WNDSG_H  228U

/* Target pixel buffer offsets for game only */
#define TGOG_X   5U
#define TGOG_Y   18U



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

/* Alpha mask, to cancel any alpha the destination might have */
static uint32        guicore_amask;

/* Initialization flags */
static auint         guicore_flags;

/* String constant for SDL error output */
static const char*   guicore_sdlerr = "SDL Error: %s\n";



/*
** Renders double scan output. The source is the target pixel buffer
** (guicore_pixels). Locations are as for single scan 320 x 270 output.
*/
static void guicore_render_2x(uint32* dest, auint dpitch,
                              auint xs, auint ys,
                              auint w,  auint h)
{
 auint  i;
 auint  j;
 auint  destoff0;
 auint  destoff1;
 auint  srcoff;
 uint32 col;

 for (i = 0U; i < (h << 1); i += 2U){
  destoff0 = (dpitch * (i     ));
  destoff1 = (dpitch * (i + 1U));
  srcoff   = (640U * (ys + (i >> 1))) + (xs << 1);
  for (j = 0U; j < (w << 1); j ++){
   col = guicore_pixels[srcoff + j];
   dest[destoff0 + j] = col;
   dest[destoff1 + j] = (((col & 0xF8F8F8F8) >> 3) * 7U) | guicore_amask;
  }
 }
}



/*
** Renders single scan output. The source is the target pixel buffer
** (guicore_pixels). Locations are as for single scan 320 x 270 output.
*/
static void guicore_render_1x(uint32* dest, auint dpitch,
                              auint xs, auint ys,
                              auint w,  auint h)
{
 auint  i;
 auint  j;
 auint  destoff;
 auint  srcoff;
 uint32 col;

 for (i = 0U; i < h; i ++){
  destoff = (dpitch * i);
  srcoff  = (640U * (ys + i)) + (xs << 1);
  for (j = 0U; j < w; j ++){
   col = ( ((guicore_pixels[srcoff + (j << 1)     ] & 0xFEFEFEFEU) >> 1) +
           ((guicore_pixels[srcoff + (j << 1) + 1U] & 0xFEFEFEFEU) >> 1) ) | guicore_amask;
   dest[destoff + j] = col;
  }
 }
}



/*
** Attempts to initialize the GUI. Returns TRUE on success. This must be
** called first before any platform specific elements as it initializes
** those. It can be recalled to change the display's properties (flags).
** Note that if it fails, it tears fown the GUI and any platform specific
** extensions!
*/
boole guicore_init(auint flags, const char* title)
{
 auint i;
 auint r;
 auint g;
 auint b;
#ifndef USE_SDL1
 auint renflags;
 auint wndflags;
 boole wndnew = TRUE;
#endif
 auint wndw;
 auint wndh;

 /* Prepare parameters */

#ifdef USE_SDL1

#else

 wndflags = 0U;
 if ((flags & GUICORE_FULLSCREEN) != 0U){ wndflags |= SDL_WINDOW_FULLSCREEN_DESKTOP; }
 else{ wndflags |= SDL_WINDOW_RESIZABLE; }

 renflags = SDL_RENDERER_ACCELERATED;
 if ((flags & GUICORE_NOVSYNC) == 0U){ renflags |= SDL_RENDERER_PRESENTVSYNC; }

#endif

 switch (flags & (GUICORE_SMALL | GUICORE_GAMEONLY)){
  case 0U:
   wndw = WND_W;   wndh = WND_H;   break;
  case GUICORE_SMALL:
   wndw = WNDS_W;  wndh = WNDS_H;  break;
  case GUICORE_GAMEONLY:
   wndw = WNDG_W;  wndh = WNDG_H;  break;
  default:
   wndw = WNDSG_W; wndh = WNDSG_H; break;
 }

 /* Check whether initializing or doing a partial reinit. */

 if ((guicore_flags & GUICORE_INIT) == 0U){ /* If it wasn't initialized yet, init */

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

 }else{ /* Already initialized */

#ifdef USE_SDL1

#else

 SDL_DestroyTexture(guicore_texture);
 if (((guicore_flags ^ flags) & ~(GUICORE_INIT |
                                  GUICORE_SMALL |
                                  GUICORE_GAMEONLY)) != 0U){
  SDL_DestroyRenderer(guicore_renderer);
  SDL_DestroyWindow(guicore_window);
 }else{
  wndnew = FALSE; /* Don't need to create and init new window */
 }

#endif

 }

 /* Perform initializations */

#ifdef USE_SDL1

 guicore_surface = SDL_SetVideoMode(wndw, wndh, 32U, SDL_HWSURFACE);
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

 guicore_amask = guicore_surface->format->Amask;

#else

 if (wndnew){

  guicore_window = SDL_CreateWindow(
      title,
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      wndw,
      wndh,
      wndflags);
  if (guicore_window == NULL){
   fprintf(stderr, guicore_sdlerr, SDL_GetError());
   goto fail_qt;
  }
  SDL_SetWindowMinimumSize(guicore_window, wndw, wndh);

  guicore_renderer = SDL_CreateRenderer(
      guicore_window,
      -1,
      renflags);
  if (guicore_renderer == NULL){
   fprintf(stderr, guicore_sdlerr, SDL_GetError());
   goto fail_wnd;
  }

 }

 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
 if (SDL_RenderSetLogicalSize(guicore_renderer, wndw, wndh) != 0U){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_ren;
 }

 guicore_texture = SDL_CreateTexture(
     guicore_renderer,
     SDL_PIXELFORMAT_RGBX8888, /* Note: If necessary, would also work with RGBA8888 */
     SDL_TEXTUREACCESS_STREAMING,
     wndw,
     wndh);
 if (guicore_texture == NULL){
  fprintf(stderr, guicore_sdlerr, SDL_GetError());
  goto fail_ren;
 }

 if (wndnew){
  SDL_RenderClear(guicore_renderer);
  SDL_RenderPresent(guicore_renderer);
 }

 for (i = 0U; i < 256U; i++){
  r = (((i >> 0) & 7U) * 255U) / 7U;
  g = (((i >> 3) & 7U) * 255U) / 7U;
  b = (((i >> 6) & 3U) * 255U) / 3U;
  guicore_palette[i] = (r << 24) | (g << 16) | (b << 8) | 0xFFU;
 }

 guicore_amask = 0x000000FFU;

#endif

 guicore_flags = flags | GUICORE_INIT;
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
 guicore_flags = 0U;
 return FALSE;
}



/*
** Tears down the GUI.
*/
void  guicore_quit(void)
{
 if ((guicore_flags & GUICORE_INIT) != 0U){

#ifdef USE_SDL1

#else

  SDL_DestroyTexture(guicore_texture);
  SDL_DestroyRenderer(guicore_renderer);
  SDL_DestroyWindow(guicore_window);

#endif

  SDL_Quit();

 }

 guicore_flags = 0U;
}



/*
** Gets current GUI flags.
*/
auint guicore_getflags(void)
{
 return guicore_flags;
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

 auint   wndw;
 auint   wndh;
 auint   offx;
 auint   offy;

 if (drop){ return; }

 if ((guicore_flags & GUICORE_GAMEONLY) != 0U){
  wndw = WNDSG_W;
  wndh = WNDSG_H;
  offx = TGOG_X;
  offy = TGOG_Y;
 }else{
  wndw = WNDS_W;
  wndh = WNDS_H;
  offx = 0U;
  offy = 0U;
 }

 if (SDL_LockSurface(guicore_surface) == 0){
  if ((guicore_flags & GUICORE_SMALL) != 0U){
   guicore_render_1x(
       guicore_surface->pixels, (guicore_surface->pitch) >> 2,
       offx, offy,
       wndw, wndh);
  }else{
   guicore_render_2x(
       guicore_surface->pixels, (guicore_surface->pitch) >> 2,
       offx, offy,
       wndw, wndh);
  }
  SDL_UnlockSurface(guicore_surface);
 }
 SDL_UpdateRect(guicore_surface, 0, 0, 0, 0);

#else

 auint   wndw;
 auint   wndh;
 auint   offx;
 auint   offy;
 void*   pixels;
 int     pitch;

 if (drop){ return; }

 if ((guicore_flags & GUICORE_GAMEONLY) != 0U){
  wndw = WNDSG_W;
  wndh = WNDSG_H;
  offx = TGOG_X;
  offy = TGOG_Y;
 }else{
  wndw = WNDS_W;
  wndh = WNDS_H;
  offx = 0U;
  offy = 0U;
 }

 if (SDL_LockTexture(guicore_texture, NULL, &pixels, &pitch) == 0){
  if ((guicore_flags & GUICORE_SMALL) != 0U){
   guicore_render_1x(
       pixels, pitch >> 2,
       offx, offy,
       wndw, wndh);
  }else{
   guicore_render_2x(
       pixels, pitch >> 2,
       offx, offy,
       wndw, wndh);
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
