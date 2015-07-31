/*
* Copyright (C) 2014 - plutoo
* Copyright (C) 2014 - ichfly
* Copyright (C) 2015 - purpasmart
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

#define GLFW_INCLUDE_NONE
#include "gl/gl_3_2_core.h"
#include <GLFW/glfw3.h>
// Let’s use our own GL header, instead of one from GLFW.

#include "util.h"
#include "gpu.h"
#include "hid_user.h"
#include "color.h"
#include "shaders.h"
#include "shader_load.h"
#include "screen.h"
#include <SDL.h>

#include "arm11.h"

//Regs g_regs;

TextureInfo textures[2];
ScreenRectVertex coords;
matrix3x2 orth;

GLFWwindow* window = NULL;

SDL_Window *win = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *bitmapTex = NULL;
SDL_Surface *bitmapSurface = NULL;


ScreenRectVertex screen_rect(GLfloat x, GLfloat y, GLfloat u, GLfloat v)
{
	coords.position[0]  = x;
	coords.position[1]  = y;
	coords.tex_coord[0] = u;
	coords.tex_coord[1] = v;

	return coords;
}

void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}


void screen_Init()
{
    SDL_Init(SDL_INIT_EVERYTHING);

    if (!glfwInit())
        exit(1);

	glfwSetErrorCallback(error_callback);

    GLFWwindow* window = glfwCreateWindow(400, (240 + 240), "3dmoo", NULL, NULL);

    if (window == NULL) {
        DEBUG("Error creating GLFW window\n");
        exit(1);
    }
}

void screen_Free()
{
    // Destroy window
    glfwTerminate();
    window = NULL;

    // Quit SDL subsystems
    SDL_Quit();
}


/**
* Defines a 1:1 pixel ortographic projection matrix with (0,0) on the top-left
* corner and (width, height) on the lower-bottom.
*
* The projection part of the matrix is trivial, hence these operations are represented
* by a 3x2 matrix.
*/
matrix3x2 screen_MakeOrthographicMatrix(float width, float height)
{
	orth.matrix[0] = 2.f / width; orth.matrix[2] = 0.f;           orth.matrix[4] = -1.f;
	orth.matrix[1] = 0.f;         orth.matrix[3] = -2.f / height; orth.matrix[5] = 1.f;
	// Last matrix row is implicitly assumed to be [0, 0, 1].

	return orth;
}

void screen_SwapBuffers()
{
    for (int i = 0; i <= 2; ++i)  {
        // Main LCD (0): 0x1ED02204, Sub LCD (1): 0x1ED02A04
        //u32 lcd_color_addr = (i == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        //lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        //LCD::Regs::ColorFill color_fill = {0};
        //LCD::Read(color_fill.raw, lcd_color_addr);

		    if (textures[i].width != (GLsizei)framebuffer_config[i].width ||
                textures[i].height != (GLsizei)framebuffer_config[i].height ||
                textures[i].format != framebuffer_config[i].color_format) {
                // Reallocate texture if the framebuffer size has changed.
                // This is expected to not happen very often and hence should not be a
                // performance problem.
                screen_ConfigureFramebufferTexture(textures[i], framebuffer_config[i]);
            }
            screen_LoadFBToActiveGLTexture(framebuffer_config[i], textures[i]);

            // Resize the texture in case the framebuffer size has changed
            textures[i].width = framebuffer_config[i].width;
            textures[i].height = framebuffer_config[i].height;
    }

    screen_DrawScreens();

}

/**
* Loads framebuffer from emulated memory into the active OpenGL texture.
*/
void screen_LoadFBToActiveGLTexture(FramebufferConfig framebuffer, TextureInfo texture) {

    const u32 framebuffer_vaddr = framebuffer.active_fb == 0 ? framebuffer.address_left1 : framebuffer.address_left2;

    DEBUG("0x%08x bytes from 0x%08x(%dx%d), fmt %x\n",
        framebuffer.stride * framebuffer.height,
        framebuffer_vaddr, (int)framebuffer.width,
        (int)framebuffer.height, (int)framebuffer.format);

    const u8* framebuffer_data = gpu_GetPhysicalMemoryBuff(framebuffer_vaddr);

    int bpp = gpu_BytesPerPixel(framebuffer.color_format);
    size_t pixel_stride = framebuffer.stride / bpp;

    // OpenGL only supports specifying a stride in units of pixels, not bytes, unfortunately
    //(pixel_stride * bpp == framebuffer.stride);

    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT, which by default
    // only allows rows to have a memory alignement of 4.
    //ASSERT(pixel_stride % 4 == 0);

	glBindTexture(GL_TEXTURE_2D, texture.handle);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)pixel_stride);

	// Update existing texture
	// TODO: Test what happens on hardware when you change the framebuffer dimensions so that they
	//       differ from the LCD resolution.
	// TODO: Applications could theoretically crash Citra here by specifying too large
	//       framebuffer sizes. We should make sure that this cannot happen.
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, framebuffer.width, framebuffer.height,
		texture.gl_format, texture.gl_type, framebuffer_data);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}


/**
 * Fills active OpenGL texture with the given RGB color.
 * Since the color is solid, the texture can be 1x1 but will stretch across whatever it's rendered on.
 * This has the added benefit of being *really fast*.
 */
void screen_LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, TextureInfo texture) 
{
    glBindTexture(GL_TEXTURE_2D, texture.handle);

    u8 framebuffer_data[3] = { color_r, color_g, color_b };

    // Update existing texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer_data);
    glBindTexture(GL_TEXTURE_2D, 0);
}


void screen_InitOpenGLObjects()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);

    // Link shaders and get variable locations
    program_id = LoadShaders(g_vertex_shader, g_fragment_shader);
    uniform_modelview_matrix = glGetUniformLocation(program_id, "modelview_matrix");
    uniform_color_texture = glGetUniformLocation(program_id, "color_texture");
    attrib_position = glGetAttribLocation(program_id, "vert_position");
    attrib_tex_coord = glGetAttribLocation(program_id, "vert_tex_coord");

    // Generate VBO handle for drawing
    glGenBuffers(1, &vertex_buffer_handle);

    // Generate VAO
    glGenVertexArrays(1, &vertex_array_handle);
    glBindVertexArray(vertex_array_handle);

    // Attach vertex data to VAO
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_handle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ScreenRectVertex) * 4, NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(attrib_position, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (GLvoid*)offsetof(ScreenRectVertex, position));
    glVertexAttribPointer(attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, sizeof(ScreenRectVertex), (GLvoid*)offsetof(ScreenRectVertex, tex_coord));
    glEnableVertexAttribArray(attrib_position);
    glEnableVertexAttribArray(attrib_tex_coord);

    // Allocate textures for each screen
        glGenTextures(1, &textures[0].handle);
        glGenTextures(1, &textures[1].handle);

        // Allocation of storage is deferred until the first frame, when we
        // know the framebuffer size.

        glBindTexture(GL_TEXTURE_2D, textures[0].handle);
		glBindTexture(GL_TEXTURE_2D, textures[1].handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void screen_ConfigureFramebufferTexture(TextureInfo texture, FramebufferConfig framebuffer) 
{
    u32 format = framebuffer.color_format;
    GLint internal_format;

    texture.format = format;
    texture.width = framebuffer.width;
    texture.height = framebuffer.height;

    switch (format) {
    case 0: //RGBA8
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_INT_8_8_8_8;
        break;

    case 1: //RGB8
        // This pixel format uses BGR since GL_UNSIGNED_BYTE specifies byte-order, unlike every
        // specific OpenGL type used in this function using native-endian (that is, little-endian
        // mostly everywhere) for words or half-words.
        // TODO: check how those behave on big-endian processors.
        internal_format = GL_RGB;
        texture.gl_format = GL_BGR;
        texture.gl_type = GL_UNSIGNED_BYTE;
        break;

    case 2: //RGB565
        internal_format = GL_RGB;
        texture.gl_format = GL_RGB;
        texture.gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case 3: //RGB5A1
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_5_5_5_1;
        break;

    case 4: //RGBA4
        internal_format = GL_RGBA;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
        break;

    default:
		break;
    }

    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width, texture.height, 0, texture.gl_format, texture.gl_type, NULL);
}

/*
 * Draws a single texture to the emulator window, rotating the texture to correct for the 3DS's LCD rotation.
 */
void screen_DrawSingleScreenRotated(TextureInfo texture, float x, float y, float w, float h) {
	ScreenRectVertex vertices[4] =
	{
		screen_rect(x, y, 1.f, 0.f),
		screen_rect(x + w, y, 1.f, 1.f),
		screen_rect(x, y + h, 0.f, 0.f),
		screen_rect(x + w, y + h, 0.f, 1.f),
    };

    glBindTexture(GL_TEXTURE_2D, texture.handle);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_handle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), &vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/**
* Draws the emulated screens to the emulator window.
*/
void screen_DrawScreens() {

	glViewport(0, 0, 400, 480);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(program_id);

	// Set projection matrix
	matrix3x2 ortho_matrix = screen_MakeOrthographicMatrix((float)400, (float)480);
	glUniformMatrix3x2fv(uniform_modelview_matrix, 1, GL_FALSE, ortho_matrix.matrix);

	// Bind texture in Texture Unit 0
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(uniform_color_texture, 0);

	//screen_DrawSingleScreenRotated(textures[0], (float)top_screen.left, (float)top_screen.top, (float)top_screen.GetWidth(), (float)top_screen.GetHeight);
	//screen_DrawSingleScreenRotated(textures[1], (float)bottom_screen.left, (float)bottom_screen.top, (float)bottom_screen.GetWidth(), (float)bottom_screen.GetHeight);

	m_current_frame++;
}


bool mausedown = false;
void screen_HandleEvent()
{
    // Event handler
    SDL_Event e;

    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
        case SDL_KEYUP:
            hid_keyup(&e.key);
            break;
        case SDL_KEYDOWN:
            hid_keypress(&e.key);
            break;
        case SDL_QUIT:
            exit(0);
            break;
        case SDL_MOUSEMOTION: /**< Mouse moved */
            if (mausedown)
                hid_position(e.motion.x - 40, e.motion.y - 240);
            break;
        case SDL_MOUSEBUTTONDOWN:/**< Mouse button pressed */
            mausedown = true;
            hid_position(e.motion.x - 40, e.motion.y - 240);
            break;
        case SDL_MOUSEBUTTONUP:          /**< Mouse button released */
            mausedown = false;
            break;
        default:
            break;
        }
    }
}
u32 svcsleep()
{
    //DEBUG("sleep %08x %08x\n", arm11_R(1),arm11_R(0) );
    return 0;
}
