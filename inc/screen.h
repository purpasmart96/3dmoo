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

#include "gpu.h"

void screen_Init();
void screen_Free();
void screen_HandleEvent();
void screen_SwapBuffers();
void screen_DrawScreens();

matrix3x2 screen_MakeOrthographicMatrix(float width, float height);
void screen_LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, TextureInfo texture);
void screen_LoadFBToActiveGLTexture(FramebufferConfig framebuffer, TextureInfo texture);
void screen_ConfigureFramebufferTexture(TextureInfo texture, FramebufferConfig framebuffer);
void screen_DrawSingleScreenRotated(TextureInfo texture, float x, float y, float w, float h);

int m_current_frame;