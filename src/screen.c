/*
 * Copyright (C) 2014 - plutoo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <SDL.h>

#include "util.h"
#include "SrvtoIO.h"

SDL_Window *win = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *bitmapTex = NULL;
SDL_Surface *bitmapSurface = NULL;

void screen_Init()
{
    SDL_Init(SDL_INIT_EVERYTHING);

    int posX = 100, posY = 100, width = 400, height = 240;

    win = SDL_CreateWindow("3dmoo", posX, posY, width, height, 0);
    if (win == NULL) {
        DEBUG("Error creating SDL window\n");
        exit(1);
    }

    bitmapSurface = SDL_GetWindowSurface(win);
}

void screen_Free()
{
    // Destroy window
    SDL_DestroyWindow(win);
    win = NULL;

    // Quit SDL subsystems
    SDL_Quit();
}

void screen_RenderGPU()
{
    u32 addr = ((GPUreadreg32(frameselectoben) & 0x1) == 0) ? GPUreadreg32(RGBuponeleft) : GPUreadreg32(RGBuptwoleft);
    u8* buffer = get_pymembuffer(addr);

    if (buffer != NULL) {
        SDL_LockSurface(bitmapSurface);
 
        u8 *bitmapPixels = (u8 *)bitmapSurface->pixels;

        for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 400; x++) {
                u8* row = (u8*)(bitmapPixels + ((239 - y) * 400 * 4) + (x * 4));
                *(row + 0) = buffer[((x * 240 + y) * 3) + 0];
                *(row + 1) = buffer[((x * 240 + y) * 3) + 1];
                *(row + 2) = buffer[((x * 240 + y) * 3) + 2];
                *(row + 3) = 0xFF;
            }
        }

        SDL_UnlockSurface(bitmapSurface);
        bitmapTex = SDL_CreateTextureFromSurface(renderer, bitmapSurface);

        if (bitmapTex == NULL) {
            DEBUG("Error creating bitmap texture\n");
            exit(1);
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, bitmapTex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
}
