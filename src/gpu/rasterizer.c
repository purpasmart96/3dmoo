/*
* Copyright (C) 2014 - Tony Wasserka.
* Copyright (C) 2014 - plutoo
* Copyright (C) 2014 - ichfly
* Copyright (C) 2014 - Normmatt
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
//this is based on citra, copyright (C) 2014 Tony Wasserka.

#include "util.h"
#include "arm11.h"
#include "handles.h"
#include "mem.h"
#include "gpu.h"
#include "color.h"
#include <math.h>
//#define testtriang

#define IntMask 0xFFF0
#define FracMask 0xF

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#ifdef testtriang
u32 numb = 0x111;
#endif

/*
struct vec3_12P4 {
    s16 v[3];//x, y,z;
};
*/


typedef struct {

    clov4 combiner_output;
    clov4 combiner_constant;
    clov4 combiner_buffer;
    clov4 primary_color;
    clov4 texture_color[3];
    clov4 dest;
    u16 x;
    u16 y;

} RasterState;

RasterState state;

//x, y,z;
typedef struct {
    s16 x, y, z;
} vec3_12P4;

static u16 min3(s16 v1,s16 v2,s16 v3)
{
    if (v1 < v2 && v1 < v3) return v1;
    if (v2 < v3) return v2;
    return v3;
}
static u16 max3(s16 v1, s16 v2, s16 v3)
{
    if (v1 > v2 && v1 > v3) return v1;
    if (v2 > v3) return v2;
    return v3;
}

static bool IsRightSideOrFlatBottomEdge(vec3_12P4 * vtx, vec3_12P4 *line1, vec3_12P4 *line2)
{
    if (line1->y == line2->y) {
        // just check if vertex is above us => bottom line parallel to x-axis
        return vtx->y < line1->y;
    } else {
        // check if vertex is on our left => right side
        // TODO: Not sure how likely this is to overflow
        return (int)vtx->x < (int)line1->x + ((int)line2->x - (int)line1->x) * ((int)vtx->y - (int)line1->y) / ((int)line2->y - (int)line1->y);
    }
}

static s32 SignedArea(u16 vtx1x, u16  vtx1y, u16  vtx2x, u16  vtx2y, u16  vtx3x, u16  vtx3y)
{
    s32 vec1x = vtx2x - vtx1x;
    s32 vec1y = vtx2y - vtx1y;
    s32 vec2x = vtx3x - vtx1x;
    s32 vec2y = vtx3y - vtx1y;
    // TODO: There is a very small chance this will overflow for sizeof(int) == 4
    return vec1x*vec2y - vec1y*vec2x;
}

static void DrawPixel(int x, int y, const clov4 color)
{
    //u8* color_buffer = gpu_ConvertPhysicalToVirtual(GPU_Regs[COLOR_BUFFER_ADDRESS] << 3);
    u8* addr = (u8*)gpu_GetPhysicalMemoryBuff(GPU_Regs[COLOR_BUFFER_ADDRESS] << 3);
    //const u32 addr = gpu_ColorBufferPhysicalAddress();

    u32 framebuffer_width = GPU_Regs[FRAME_BUFFER_DIM] & 0xFFFF;
    u32 framebuffer_height = (GPU_Regs[FRAME_BUFFER_DIM] >> 12) & 0xFFFF;

    y = (framebuffer_height - y);
    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = gpu_BytesPerPixel((GPU_Regs[COLOR_BUFFER_FORMAT] >> 16));
    u32 dst_offset = GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer_width  * bytes_per_pixel;
    u8* dst_pixel = gpu_ConvertPhysicalToVirtual(addr) + dst_offset;

    //DEBUG("x=%d,y=%d,outx=%d,outy=%d,format=%d,inputdim=%08X,bufferformat=%08X\n", x, y, outx, outy, (GPU_Regs[BUFFER_FORMAT] & 0x7000) >> 12, inputdim, GPU_Regs[BUFFER_FORMAT]);

    // Assuming RGB8 format until actual framebuffer format handling is implemented
    switch ((GPU_Regs[COLOR_BUFFER_FORMAT] >> 16) & 0x7000) { //input format

    case 0: //RGBA8
        EncodeRGBA8(color, dst_pixel);
        break;
    case 1: //RGB8
        EncodeRGB8(color, dst_pixel);
        break;
    case 2: //RGB565
        EncodeRGB565(color, dst_pixel);
        break;
    case 3: //RGB5A1
        EncodeRGB5A1(color, dst_pixel);
        break;
    case 4: //RGBA4
        EncodeRGBA4(color, dst_pixel);
        break;
    default:
        DEBUG("error unknown output format %04X\n", GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000);
        break;
    }

}

#define GetPixel RetrievePixel
static const clov4 RetrievePixel(int x, int y)
{
    //u8* color_buffer = (u8*)gpu_GetPhysicalMemoryBuff(GPU_Regs[COLOR_BUFFER_ADDRESS] << 3);
    u8* addr = (u8*)gpu_GetPhysicalMemoryBuff(GPU_Regs[COLOR_BUFFER_ADDRESS] << 3);
    //const u32 addr = gpu_ColorBufferPhysicalAddress();

    u32 framebuffer_width = GPU_Regs[FRAME_BUFFER_DIM] & 0xFFF;
    u32 framebuffer_height = (GPU_Regs[FRAME_BUFFER_DIM] >> 12) & 0xFFF;

    y = (framebuffer_height - y);
    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = gpu_BytesPerPixel((GPU_Regs[COLOR_BUFFER_FORMAT] >> 16));
    u32 src_offset = GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * framebuffer_width * bytes_per_pixel;
    u8* src_pixel = gpu_ConvertPhysicalToVirtual(addr) + src_offset;

    //DEBUG("x=%d,y=%d,outx=%d,outy=%d,format=%d,inputdim=%08X,colorbufferformat=%08X\n", x, y, outx, outy, (GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000) >> 12, inputdim, GPU_Regs[BUFFERFORMAT]);

    //color ncolor;
    //memset(&ncolor, 0, sizeof(color));

    switch ((GPU_Regs[COLOR_BUFFER_FORMAT] >> 16) & 7) { //input format

    case 0: //RGBA8
        return DecodeRGBA8(src_pixel);
    case 1: //RGB8
        return DecodeRGB8(src_pixel);
    case 2: //RGB565
        return DecodeRGB565(src_pixel);
    case 3: //RGB5A1
        return DecodeRGB5A1(src_pixel);
    case 4: //RGBA4
        return DecodeRGBA4(src_pixel);
    default:
        DEBUG("error unknown output format %04X\n", GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000);
        break;
    }
}

static u32 GetDepth(int x, int y)
{
    u8* depth_buffer = gpu_GetPhysicalMemoryBuff(GPU_Regs[DEPTH_BUFFER_ADDRESS] << 3);

    u32 framebuffer_width = GPU_Regs[FRAME_BUFFER_DIM] & 0xFFFF;
    u32 framebuffer_height = (GPU_Regs[FRAME_BUFFER_DIM] >> 12) & 0xFFFF;

    y = (framebuffer_height - y);

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = gpu_BytesPerDepthPixel(GPU_Regs[DEPTH_BUFFER_FORMAT]);
    u32 stride = framebuffer_width * bytes_per_pixel;

    u32 src_offset = GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* src_pixel = depth_buffer + src_offset;

    switch ((GPU_Regs[DEPTH_BUFFER_FORMAT]) & 7) {
    case 0: // D16
        return DecodeD16(src_pixel);
    case 2: // D24
        return DecodeD24(src_pixel);
    case 3: // D24S8
        return DecodeD24(src_pixel); // TODO
    default:
        DEBUG("Unimplemented depth format %u\n", GPU_Regs[DEPTH_BUFFER_FORMAT]);
        return 0;
    }
}

static void SetDepth(int x, int y, u32 value)
{
    //const u32 addr = gpu_DepthBufferPhysicalAddress();
    //u8* depth_buffer = gpu_ConvertPhysicalToVirtual(addr);
    u8* depth_buffer = gpu_GetPhysicalMemoryBuff(GPU_Regs[DEPTH_BUFFER_ADDRESS] << 3);

    u32 framebuffer_width = GPU_Regs[FRAME_BUFFER_DIM] & 0xFFFF;
    u32 framebuffer_height = (GPU_Regs[FRAME_BUFFER_DIM] >> 12) & 0xFFFF;

    y = framebuffer_height - y;

    const u32 coarse_y = y & ~7;
    u32 bytes_per_pixel = gpu_BytesPerDepthPixel(GPU_Regs[DEPTH_BUFFER_FORMAT]);
    u32 stride = framebuffer_width * bytes_per_pixel;

    u32 dst_offset = GetMortonOffset(x, y, bytes_per_pixel) + coarse_y * stride;
    u8* dst_pixel = depth_buffer + dst_offset;

    switch ((GPU_Regs[DEPTH_BUFFER_FORMAT]) & 7) {
    case 0: //D16
        EncodeD16(value, dst_pixel);
        break;

    case 2: //D24
        EncodeD24(value, dst_pixel);
        break;

    case 3: //D24S8
        EncodeD24X8(value, dst_pixel);
        break;

    default:
        GPUDEBUG("Unimplemented depth format %u\n", framebuffer.depth_format);
        break;
    }
}

static float GetInterpolatedAttribute(float attr0, float attr1, float attr2, OutputVertex *v0, OutputVertex * v1, OutputVertex * v2, float w0,float w1, float w2)
{
    float interpolated_attr_over_w = (attr0 / v0->position.v[2])*w0 + (attr1 / v1->position.v[2])*w1 + (attr2 / v2->position.v[2])*w2;
    float interpolated_w_inverse = ((1.f) / v0->position.v[3])*w0 + ((1.f) / v1->position.v[2])*w1 + ((1.f) / v2->position.v[2])*w2;
    return interpolated_attr_over_w / interpolated_w_inverse;
}


clov4 GetSource(u32 source) 
{
    switch (source)
    {
    case 0: //PrimaryColor

    // HACK: Until we implement fragment lighting, use primary_color
    case 1: //PrimaryFragmentColor
        return state.primary_color;

    // HACK: Until we implement fragment lighting, use zero
    //case 2: //SecondaryFragmentColor
    //    return 0;

    case 3: //Texture0
        return state.texture_color[0];

    case 4: //Texture1
        return state.texture_color[1];

    case 5: //Texture2
        return state.texture_color[2];

    case 6: //PreviousBuffer
        return state.combiner_buffer;

    case 7: //Constant
        return state.combiner_constant;

    case 8: //Previous
        return state.combiner_output;

    default:
        ERROR("Unknown color combiner source %d\n", (int)source);
    }
}

clov3 GetColorModifier(u32 factor, clov4 values)
{
    clov3 ret;

    switch (factor) {
    case 0: //SourceColor
        ret.v[0] = values.v[0];
        ret.v[1] = values.v[1];
        ret.v[2] = values.v[2];
        break;
    case 1: //OneMinusSourceColor
        ret.v[0] = 255 - values.v[0];
        ret.v[1] = 255 - values.v[1];
        ret.v[2] = 255 - values.v[2];
        break;
    case 2: //SourceAlpha
        ret.v[0] = values.v[3];
        ret.v[1] = values.v[3];
        ret.v[2] = values.v[3];
        break;
    case 3: //OneMinusSourceAlpha
        ret.v[0] = 255 - values.v[3];
        ret.v[1] = 255 - values.v[3];
        ret.v[2] = 255 - values.v[3];
        break;
    case 4: //SourceRed
        ret.v[0] = values.v[0];
        ret.v[1] = values.v[0];
        ret.v[2] = values.v[0];
        break;
    case 5: //OneMinusSourceRed
        ret.v[0] = 255 - values.v[0];
        ret.v[1] = 255 - values.v[0];
        ret.v[2] = 255 - values.v[0];
        break;
    case 8: //SourceGreen
        ret.v[0] = values.v[1];
        ret.v[1] = values.v[1];
        ret.v[2] = values.v[1];
        break;
    case 9: //OneMinusSourceGreen
        ret.v[0] = 255 - values.v[1];
        ret.v[2] = 255 - values.v[1];
        ret.v[1] = 255 - values.v[1];
        break;
    case 0xC: //SourceBlue
        ret.v[0] = values.v[2];
        ret.v[1] = values.v[2];
        ret.v[2] = values.v[2];
        break;
    case 0xD: //OneMinusSourceBlue
        ret.v[0] = 255 - values.v[2];
        ret.v[1] = 255 - values.v[2];
        ret.v[2] = 255 - values.v[2];
        break;
    default:
        GPUDEBUG("Unknown color factor %d\n", (int)factor);
        break;
    }

    return ret;
}


static u8 GetAlphaModifier(u32 factor, clov4 values)
{
    switch (factor) {
    case 0: //SourceAlpha
		return values.v[3];

    case 1: //OneMinusSourceAlpha
        return 255 - values.v[3];

    case 2: //SourceRed
        return values.v[0];

    case 3: //OneMinusRed
        return 255 - values.v[0];

    case 4: //SourceGreen
        return values.v[1];

    case 5: //OneMinusSourceGreen
        return 255 - values.v[1];

    case 6: //SourceBlue
        return values.v[2];

    case 7: //OneMinusSourceBlue
        return 255 - values.v[2];
    }
};

static u8 AlphaCombine(u32 op, clov3 *input)
{
	switch (op) {
	case 0://Replace:
		return input->v[0];
	case 1://Modulate:
		return input->v[0] * input->v[1] / 255;
	case 2://Add:
		return CLAMP(input->v[0] + input->v[1], 0, 255);
	case 3://Add Signed:
		return CLAMP(input->v[0] + input->v[1] - 128, 0, 255);
	case 4://Lerp:
		return CLAMP((input->v[0] * input->v[2] + input->v[1] * (255 - input->v[2])) / 255, 0, 255);
	case 5://Subtract:
		return CLAMP(input->v[0] - input->v[1], 0, 255);
	case 8://Multiply Addition:
		return CLAMP((input->v[0] * input->v[1] / 255) + input->v[2], 0, 255);
	case 9://Addition Multiply:
		return CLAMP((input->v[0] + input->v[1]) * input->v[2] / 255, 0, 255);
	default:
		GPUDEBUG("Unknown alpha combiner operation %d\n", (int)op);
		return 0;
	}
};

clov3 ColorCombine(u32 op, clov3 input[3])
{
    clov3 ret;

    switch (op) {
	case 0: //Replace
        ret.v[0] = input[0].v[0];
        ret.v[1] = input[0].v[1];
        ret.v[2] = input[0].v[2];
        break;

   case 1: //Modulate
        ret.v[0] = (input)[0].v[0] * (input)[1].v[0] / 255;
        ret.v[1] = (input)[0].v[1] * (input)[1].v[1] / 255;
        ret.v[2] = (input)[0].v[2] * (input)[1].v[2] / 255;
        break;

	case 2: //Add:
    {
		vec3_int result;
        result.v[0] = input[0].v[0] + input[1].v[0];
        result.v[1] = input[0].v[1] + input[1].v[1];
        result.v[2] = input[0].v[2] + input[1].v[2];

        ret.v[0] = min(255, result.v[0]);
        ret.v[1] = min(255, result.v[1]);
        ret.v[2] = min(255, result.v[2]);
        break;
    }

	case 3: //Add Signed
    {
        // TODO(bunnei): Verify that the color conversion from (float) 0.5f to (byte) 128 is correct
        vec3_int result;
        result.v[0] = input[0].v[0] + input[1].v[0] - 128;
        result.v[1] = input[0].v[1] + input[1].v[1] - 128;
        result.v[2] = input[0].v[2] + input[1].v[2] - 128;

        ret.v[0] = CLAMP(result.v[0], 0, 255);
        ret.v[1] = CLAMP(result.v[1], 0, 255);
        ret.v[2] = CLAMP(result.v[2], 0, 255);
        break;
    }

   case 4: //Lerp:
        ret.v[0] = ((input[0].v[0] * input[2].v[0] + input[1].v[0] * 255 - input[2].v[0]) / 255);
        ret.v[1] = ((input[0].v[1] * input[2].v[1] + input[1].v[1] * 255 - input[2].v[1]) / 255);
        ret.v[2] = ((input[0].v[2] * input[2].v[2] + input[1].v[2] * 255 - input[2].v[2]) / 255);
        break;

    case 5: //Subtract:
    {
        vec3_int result;
        result.v[0] = input[0].v[0] - input[1].v[0];
        result.v[1] = input[0].v[1] - input[1].v[1];
        result.v[2] = input[0].v[2] - input[1].v[2];

        ret.v[0] = max(0, result.v[0]);
        ret.v[1] = max(0, result.v[1]);
        ret.v[2] = max(0, result.v[2]);
        break;
    }

    case 6: //Dot3_RGB
    {
        // Not fully accurate.
        // Worst case scenario seems to yield a +/-3 error
        // Some HW results indicate that the per-component computation can't have a higher precision than 1/256,
        // while dot3_rgb( (0x80,g0,b0),(0x7F,g1,b1) ) and dot3_rgb( (0x80,g0,b0),(0x80,g1,b1) ) give different results
        vec3_int result;
        result.v[0] = ((input[0].v[0] * 2 - 255) * (input[1].v[0] * 2 - 255) + 128) / 256;
        result.v[1] = ((input[0].v[1] * 2 - 255) * (input[1].v[1] * 2 - 255) + 128) / 256;
        result.v[2] = ((input[0].v[2] * 2 - 255) * (input[1].v[2] * 2 - 255) + 128) / 256;

        ret.v[0] = max(0, min(255, result.v[0]));
        ret.v[1] = max(0, min(255, result.v[1]));
        ret.v[2] = max(0, min(255, result.v[2]));
        break;
	}

    case 8: //MultiplyThenAdd
    {
        vec3_int result;
        result.v[0] = (input[0].v[0] * input[1].v[0] + 255 * input[2].v[0]) / 255;
        result.v[1] = (input[0].v[1] * input[1].v[1] + 255 * input[2].v[1]) / 255;
        result.v[2] = (input[0].v[2] * input[1].v[2] + 255 * input[2].v[2]) / 255;
        ret.v[0] = min(255, result.v[0]);
        ret.v[1] = min(255, result.v[1]);
        ret.v[2] = min(255, result.v[2]);
        break;
    }

	case 9: //AddThenMultiply
    {
        vec3_int result;
        result.v[0] = input[0].v[0] + input[1].v[0];
        result.v[1] = input[0].v[1] + input[1].v[1];
        result.v[2] = input[0].v[2] + input[1].v[2];

        result.v[0] = min(255, result.v[0]);
        result.v[1] = min(255, result.v[1]);
        result.v[2] = min(255, result.v[2]);

        ret.v[0] = (result.v[0] * input[2].v[0]) / 255;
        ret.v[1] = (result.v[1] * input[2].v[1]) / 255;
        ret.v[2] = (result.v[2] * input[2].v[2]) / 255;
        break;
    }

    default:
        ERROR("Unknown color combiner operation %d\n", (int)op);
    }

    return ret;
}



typedef enum {
    Zero                    = 0,
    One                     = 1,
    SourceColor             = 2,
    OneMinusSourceColor     = 3,
    DestColor               = 4,
    OneMinusDestColor       = 5,
    SourceAlpha             = 6,
    OneMinusSourceAlpha     = 7,
    DestAlpha               = 8,
    OneMinusDestAlpha       = 9,
    ConstantColor           = 10,
    OneMinusConstantColor   = 11,
    ConstantAlpha           = 12,
    OneMinusConstantAlpha   = 13,
    SourceAlphaSaturate     = 14,
} BlendFactor;


static clov3 LookupFactorRGB(BlendFactor factor)
{
    clov3 output;
    switch (factor)
    {
    case Zero:
        output.v[0] = 0;
        output.v[1] = 0;
        output.v[2] = 0;
        break;
    case One:
        output.v[0] = 255;
        output.v[1] = 255;
        output.v[2] = 255;
        break;
    case SourceColor:
        output.v[0] = state.combiner_output.v[0];
        output.v[1] = state.combiner_output.v[1];
        output.v[2] = state.combiner_output.v[2];
        break;
    case OneMinusSourceColor:
        output.v[0] = 255 - state.combiner_output.v[0];
        output.v[1] = 255 - state.combiner_output.v[1];
        output.v[2] = 255 - state.combiner_output.v[2];
        break;
    case DestColor:
        output.v[0] = state.dest.v[0];
        output.v[1] = state.dest.v[1];
        output.v[2] = state.dest.v[2];
        break;
    case OneMinusDestColor:
        output.v[0] = 255 - state.dest.v[0];
        output.v[1] = 255 - state.dest.v[1];
        output.v[2] = 255 - state.dest.v[2];
        break;
    case SourceAlpha:
        output.v[0] = state.combiner_output.v[3];
        output.v[1] = state.combiner_output.v[3];
        output.v[2] = state.combiner_output.v[3];
        break;
    case OneMinusSourceAlpha:
        output.v[0] = 255 - state.combiner_output.v[3];
        output.v[1] = 255 - state.combiner_output.v[3];
        output.v[2] = 255 - state.combiner_output.v[3];
        break;
    case DestAlpha:
        output.v[0] = state.dest.v[3];
        output.v[1] = state.dest.v[3];
        output.v[2] = state.dest.v[3];
        break;
    case OneMinusDestAlpha:
        output.v[0] = 255 - state.dest.v[3];
        output.v[1] = 255 - state.dest.v[3];
        output.v[2] = 255 - state.dest.v[3];
        break;

    //case ConstantColor:
    //    return Math::Vec3<u8>(registers.output_merger.blend_const.r, registers.output_merger.blend_const.g, registers.output_merger.blend_const.b);

    //case OneMinusConstantColor:
    //    return Math::Vec3<u8>(255 - registers.output_merger.blend_const.r, 255 - registers.output_merger.blend_const.g, 255 - registers.output_merger.blend_const.b);

    case ConstantAlpha:
        output.v[0] = output.v[1] = output.v[2] = GPU_Regs[BLEND_COLOR] >> 24;
        break;
    case OneMinusConstantAlpha:
        output.v[0] = output.v[1] = output.v[2] = 255 - (GPU_Regs[BLEND_COLOR] >> 24);
        break;

	default:
        DEBUG("Unknown color blend factor %x\n", factor);
        break;
    }

    return output;
}

static u8 LookupFactorA(BlendFactor factor)
{
    switch (factor)
    {
    case Zero:
        return 0;

    case One:
        return 255;

    case SourceAlpha:
        return state.combiner_output.v[3];

    case OneMinusSourceAlpha:
        return 255 - state.combiner_output.v[3];

    case ConstantAlpha:
        return GPU_Regs[BLEND_COLOR] >> 24;

    case OneMinusConstantAlpha:
        return 255 - (GPU_Regs[BLEND_COLOR] >> 24);

    default:
        DEBUG("Unknown alpha blend factor %x\n", factor);
        break;
    }
}


typedef enum {
    ClampToEdge   = 0,
    ClampToBorder = 1,
    Repeat        = 2,
    MirrorRepeat  = 3,
} WrapMode;

static int GetWrappedTexCoord(WrapMode wrap, int val, int size)
{
    if (size == 0)
        return val;
    int ret = 0;
    switch (wrap)
    {
    case ClampToEdge:
        ret = MAX(val, 0);
        ret = MIN(ret, size - 1);
        break;
    case Repeat:
        ret = (int)((unsigned)val % size);
        break;
    case MirrorRepeat:
        ret = (int)(((unsigned)val) % (2 * size));
        if (ret >= size)
            ret = 2 * size - 1 - ret;
        break;
    default:
        DEBUG("Unknown wrap format %08X\n", wrap);
        ret = 0;
        break;
    }

    return ret;
}


static unsigned NibblesPerPixel(TextureFormat format)
{
    switch (format) {
    case RGBA8:
        return 8;

    case RGB8:
        return 6;

    case RGB5A1:
    case RGB565:
    case RGBA4:
    case IA8:
        return 4;

    case I4:
    case A4:
        return 1;

    case I8:
    case A8:
    case IA4:
    case ETC1:
    case ETC1A4:
    default:  // placeholder for yet unknown formats
        return 2;
    }
}

const int ETC1Modifiers[][2] =
{
    { 2, 8 },
    { 5, 17 },
    { 9, 29 },
    { 13, 42 },
    { 18, 60 },
    { 24, 80 },
    { 33, 106 },
    { 47, 183 }
};

static int ColorClamp(int Color)
{
    if(Color > 255) Color = 255;
    if(Color < 0) Color = 0;
    return Color;
}

int Etc1BlockStart(int width, int x, int y, bool HasAlpha)
{
    int num = x / 4;
    int num2 = y / 4;
    int num3 = x / 8;
    int num4 = y / 8;
    int numBlocks = num3 + (num4*(width / 8));
    int numBlocksInBytes = numBlocks * 4;
    numBlocksInBytes += num & 1;
    numBlocksInBytes += (num2 & 1) * 2;
    return (numBlocksInBytes*(HasAlpha ? 16 : 8));
}

const clov4 LookupTexture(const u8* source, int x, int y, const TextureFormat format, int stride, int width, int height, bool disable_alpha)
{
    const u32 coarse_x = x & ~7;
    const u32 coarse_y = y & ~7;
    clov4 ret;

    switch (format) {
    case RGBA8:
    {
        clov4 res = DecodeRGBA8(source + GetMortonOffset(x, y, 4));
        ret.v[0] = res.v[3];
        ret.v[1] = res.v[2];
        ret.v[2] = res.v[1];
        ret.v[3] = disable_alpha ? 255 : res.v[0];
        break;
    }

    case RGB8:
    {
        clov4 res = DecodeRGB8(source + GetMortonOffset(x, y, 3));
        ret.v[0] = res.v[0];
        ret.v[1] = res.v[1];
        ret.v[2] = res.v[2];
        ret.v[3] = 255;
        break;
    }

    case RGB5A1:
    {
        clov4 res = DecodeRGB5A1(source + GetMortonOffset(x, y, 2));

        ret.v[0] = res.v[0];
        ret.v[1] = res.v[1];
        ret.v[2] = res.v[2];
        ret.v[3] = disable_alpha ? 255 : (res.v[3]);
        break;
    }

    case RGB565:
    {
        clov4 res = DecodeRGB565(source + GetMortonOffset(x, y, 2));
        ret.v[0] = res.v[0];
        ret.v[1] = res.v[1];
        ret.v[2] = res.v[2];
        ret.v[3] = 255;
        break;
    }

    case RGBA4:
    {
        clov4 res = DecodeRGBA4(source + GetMortonOffset(x, y, 2));

        ret.v[0] = res.v[0];
        ret.v[1] = res.v[1];
        ret.v[2] = res.v[2];
        ret.v[3] = disable_alpha ? 255 : res.v[3];
        break;
    }

    case IA8:
    {
        const u8* source_ptr = source + GetMortonOffset(x, y, 2);

        if (disable_alpha) {
            // Show intensity as red, alpha as green
            ret.v[0] = source_ptr[1];
            ret.v[1] = source_ptr[0];
            ret.v[2] = 0;
            ret.v[3] = 255;
        }
        else {
            ret.v[0] = source_ptr[1];
            ret.v[1] = source_ptr[1];
            ret.v[2] = source_ptr[1];
            ret.v[3] = source_ptr[0];

        }
    }

    case I8:
    {
        const u8* source_ptr = source + GetMortonOffset(x, y, 1);

        // TODO: Better control this...
        ret.v[0] = *source_ptr;
        ret.v[1] = *source_ptr;
        ret.v[2] = *source_ptr;
        ret.v[3] = 255;
        break;
	}

	case A8:
	{
        const u8* source_ptr = source + GetMortonOffset(x, y, 1);

        // TODO: Better control this...
        if (disable_alpha) {
            ret.v[0] = *source_ptr;
            ret.v[1] = *source_ptr;
            ret.v[2] = *source_ptr;
            ret.v[3] = 255;
        }
        else {
            ret.v[0] = 0;
            ret.v[1] = 0;
            ret.v[2] = 0;
            ret.v[3] = *source_ptr;
        }
        break;
    }

    case IA4:
    {
        const u8* source_ptr = source + GetMortonOffset(x, y, 1);

        u8 i = Convert4To8(((*source_ptr) & 0xF0) >> 4);
        u8 a = Convert4To8((*source_ptr) & 0xF);

        // TODO: Better control this...
        if (disable_alpha) {
            ret.v[0] = i;
            ret.v[1] = a;
            ret.v[2] = 0;
            ret.v[3] = 255;
        }
        else {
            ret.v[0] = i;
            ret.v[1] = i;
            ret.v[2] = i;
            ret.v[3] = a;
        }
        break;
    }

    case I4:
    {
        u32 morton_offset = GetMortonOffset(x, y, 1);
        const u8* source_ptr = source + morton_offset / 2;

        u8 i = (morton_offset % 2) ? ((*source_ptr & 0xF0) >> 4) : (*source_ptr & 0xF);
        i = Convert4To8(i);

        ret.v[0] = i;
        ret.v[1] = i;
        ret.v[2] = i;
        ret.v[3] = 255;
        break;
    }

    case A4:
    {
        u32 morton_offset = GetMortonOffset(x, y, 1);
        const u8* source_ptr = source + morton_offset / 2;

        u8 a = (morton_offset % 2) ? ((*source_ptr & 0xF0) >> 4) : (*source_ptr & 0xF);
        a = Convert4To8(a);

        // TODO: Better control this...
        if (disable_alpha) {
            ret.v[0] = a;
            ret.v[1] = a;
            ret.v[2] = a;
            ret.v[3] = 255;
        }
        else {
            ret.v[0] = 0;
            ret.v[1] = 0;
            ret.v[2] = 0;
            ret.v[3] = a;
        }
        break;
    }

    case ETC1:
    case ETC1A4:
    {
        //Based on code from EveryFileExplorer by Gericom
        const u8* source_ptr = source;
        u32 offs = Etc1BlockStart(stride, x, y, format == ETC1A4);

        u64 alpha = 0xFFFFFFFFFFFFFFFF;
        if (format == ETC1A4)
        {
            alpha = *(u64*)&source_ptr[offs];
            offs += 8;
        }
        u64 data = *(u64*)&source_ptr[offs];
        bool diffbit = ((data >> 33) & 1) == 1;
        bool flipbit = ((data >> 32) & 1) == 1; //0: |||, 1: |-|
        int r1, r2, g1, g2, b1, b2;
        if (diffbit) //'differential' mode
        {
            int r = (int)((data >> 59) & 0x1F);
            int g = (int)((data >> 51) & 0x1F);
            int b = (int)((data >> 43) & 0x1F);
            r1 = (r << 3) | ((r & 0x1C) >> 2);
            g1 = (g << 3) | ((g & 0x1C) >> 2);
            b1 = (b << 3) | ((b & 0x1C) >> 2);
            r += (int)((data >> 56) & 0x7) << 29 >> 29;
            g += (int)((data >> 48) & 0x7) << 29 >> 29;
            b += (int)((data >> 40) & 0x7) << 29 >> 29;
            r2 = (r << 3) | ((r & 0x1C) >> 2);
            g2 = (g << 3) | ((g & 0x1C) >> 2);
            b2 = (b << 3) | ((b & 0x1C) >> 2);
        }
        else //'individual' mode
        {
            r1 = (int)((data >> 60) & 0xF) * 0x11;
            g1 = (int)((data >> 52) & 0xF) * 0x11;
            b1 = (int)((data >> 44) & 0xF) * 0x11;
            r2 = (int)((data >> 56) & 0xF) * 0x11;
            g2 = (int)((data >> 48) & 0xF) * 0x11;
            b2 = (int)((data >> 40) & 0xF) * 0x11;
        }
        int Table1 = (int)((data >> 37) & 0x7);
        int Table2 = (int)((data >> 34) & 0x7);

        int wantedY = y & 3;
        int wantedX = x & 3;
        int val = (int)((data >> (wantedX * 4 + wantedY)) & 0x1);
        bool neg = ((data >> (wantedX * 4 + wantedY + 16)) & 0x1) == 1;

        if ((flipbit && wantedY < 2) || (!flipbit && wantedX < 2))
        {
            int add = ETC1Modifiers[Table1][val] * (neg ? -1 : 1);
            ret.v[0] = (u8)ColorClamp(r1 + add);
            ret.v[1] = (u8)ColorClamp(g1 + add);
            ret.v[2] = (u8)ColorClamp(b1 + add);
        }
        else
        {
            int add = ETC1Modifiers[Table2][val] * (neg ? -1 : 1);
            ret.v[0] = (u8)ColorClamp(r2 + add);
            ret.v[1] = (u8)ColorClamp(g2 + add);
            ret.v[2] = (u8)ColorClamp(b2 + add);
        }
        ret.v[3] = disable_alpha ? 255 : (u8)(((alpha >> ((wantedX * 4 + wantedY) * 4)) & 0xF) * 0x11);
        break;
    }


    default:
        DEBUG("Unknown texture format: 0x%08X\n", (u32)format);
        break;
    }

    return ret;
}


static void ProcessTriangleInternal(OutputVertex* v0,
                                    OutputVertex* v1,
                                    OutputVertex* v2,
                                    bool reversed)
{
	// NOTE: Assuming that rasterizer coordinates are 12.4 fixed-point values
	vec3_12P4 vtxpos[3];
	vec3_12P4 vtxpostemp;
	OutputVertex *vtemp;
    for (int i = 0; i < 3; i++)
        vtxpos[0].x = (s16)(roundf(v0->screenpos.x * 16.0f));
    for (int i = 0; i < 3; i++)
        vtxpos[1].y = (s16)(roundf(v1->screenpos.y * 16.0f));
    for (int i = 0; i < 3; i++)
        vtxpos[2].z = (s16)(roundf(v2->screenpos.z * 16.0f));

    if ((GPU_Regs[CULL_MODE] & 0x3) == 0) { //KeepAll
        // Make sure we always end up with a triangle wound counter-clockwise
        if (!reversed && SignedArea(vtxpos[0].x, vtxpos[0].y, vtxpos[1].x, vtxpos[1].y, vtxpos[2].x, vtxpos[2].y) <= 0) {
            ProcessTriangleInternal(v0, v2, v1, true);
            return;
        }
    } else {
        if (!reversed && ((GPU_Regs[CULL_MODE] & 0x3)) == 1) { //KeepClockWise
            // Reverse vertex order and use the CCW code path.
            ProcessTriangleInternal(v0, v2, v1, true);
            return;
        }

        // Cull away triangles which are wound clockwise.
        if (SignedArea(vtxpos[0].x, vtxpos[0].y, vtxpos[1].x, vtxpos[1].y, vtxpos[2].x, vtxpos[2].y) <= 0)
            return;
    }

	// TODO: Proper scissor rect test!
    u16 min_x = min3(vtxpos[0].x, vtxpos[1].x, vtxpos[2].x);
    u16 min_y = min3(vtxpos[0].y, vtxpos[1].y, vtxpos[2].y);
    u16 max_x = max3(vtxpos[0].x, vtxpos[1].x, vtxpos[2].x);
    u16 max_y = max3(vtxpos[0].y, vtxpos[1].y, vtxpos[2].y);
    min_x &= IntMask;
    min_y &= IntMask;
    max_x = ((max_x + FracMask) & IntMask);
    max_y = ((max_y + FracMask) & IntMask);
    // Triangle filling rules: Pixels on the right-sided edge or on flat bottom edges are not
    // drawn. Pixels on any other triangle border are drawn. This is implemented with three bias
    // values which are added to the barycentric coordinates w0, w1 and w2, respectively.
    // NOTE: These are the PSP filling rules. Not sure if the 3DS uses the same ones...

    int bias0 = IsRightSideOrFlatBottomEdge(&vtxpos[0], &vtxpos[1], &vtxpos[2]) ? -1 : 0;
    int bias1 = IsRightSideOrFlatBottomEdge(&vtxpos[1], &vtxpos[2], &vtxpos[0]) ? -1 : 0;
    int bias2 = IsRightSideOrFlatBottomEdge(&vtxpos[2], &vtxpos[0], &vtxpos[1]) ? -1 : 0;
    // TODO: Not sure if looping through x first might be faster
    for (state.y = min_y + 8; state.y < max_y; state.y += 0x10) {
        for (state.x = min_x + 8; state.x < max_x; state.x += 0x10) {
            // Calculate the barycentric coordinates w0, w1 and w2
            int w0 = bias0 + SignedArea(vtxpos[1].x, vtxpos[1].y, vtxpos[2].x, vtxpos[2].y, state.x, state.y);
            int w1 = bias1 + SignedArea(vtxpos[2].x, vtxpos[2].y, vtxpos[0].x, vtxpos[0].y, state.x, state.y);
            int w2 = bias2 + SignedArea(vtxpos[0].x, vtxpos[0].y, vtxpos[1].x, vtxpos[1].y, state.x, state.y);
            int wsum = w0 + w1 + w2;
            // If current pixel is not covered by the current primitive
            if (w0 < 0 || w1 < 0 || w2 < 0)
                continue;
            // Perspective correct attribute interpolation:
            // Attribute values cannot be calculated by simple linear interpolation since
            // they are not linear in screen space. For example, when interpolating a
            // texture coordinate across two vertices, something simple like
            // u = (u0*w0 + u1*w1)/(w0+w1)
            // will not work. However, the attribute value divided by the
            // clipspace w-coordinate (u/w) and and the inverse w-coordinate (1/w) are linear
            // in screenspace. Hence, we can linearly interpolate these two independently and
            // calculate the interpolated attribute by dividing the results.
            // I.e.
            // u_over_w = ((u0/v0.pos.w)*w0 + (u1/v1.pos.w)*w1)/(w0+w1)
            // one_over_w = (( 1/v0.pos.w)*w0 + ( 1/v1.pos.w)*w1)/(w0+w1)
            // u = u_over_w / one_over_w
            //
            // The generalization to three vertices is straightforward in baricentric coordinates.

            state.primary_color.v[0] = (u8)(GetInterpolatedAttribute(v0->color.v[0], v1->color.v[0], v2->color.v[0], v0, v1, v2, (float)w0, (float)w1, (float)w2) * 255.f);
            state.primary_color.v[1] = (u8)(GetInterpolatedAttribute(v0->color.v[1], v1->color.v[1], v2->color.v[1], v0, v1, v2, (float)w0, (float)w1, (float)w2) * 255.f);
            state.primary_color.v[2] = (u8)(GetInterpolatedAttribute(v0->color.v[2], v1->color.v[2], v2->color.v[2], v0, v1, v2, (float)w0, (float)w1, (float)w2) * 255.f);
            state.primary_color.v[3] = (u8)(GetInterpolatedAttribute(v0->color.v[3], v1->color.v[3], v2->color.v[3], v0, v1, v2, (float)w0, (float)w1, (float)w2) * 255.f);

            vec2 uv[3];
            uv[0].x = GetInterpolatedAttribute(v0->texcoord0.x, v1->texcoord0.x, v2->texcoord0.x, v0, v1, v2, (float)w0, (float)w1, (float)w2);
            uv[0].y = GetInterpolatedAttribute(v0->texcoord0.y, v1->texcoord0.y, v2->texcoord0.y, v0, v1, v2, (float)w0, (float)w1, (float)w2);
            uv[1].x = GetInterpolatedAttribute(v0->texcoord1.x, v1->texcoord1.x, v2->texcoord1.x, v0, v1, v2, (float)w0, (float)w1, (float)w2);
            uv[1].y = GetInterpolatedAttribute(v0->texcoord1.y, v1->texcoord1.y, v2->texcoord1.y, v0, v1, v2, (float)w0, (float)w1, (float)w2);
            uv[2].x = GetInterpolatedAttribute(v0->texcoord2.x, v1->texcoord2.x, v2->texcoord2.x, v0, v1, v2, (float)w0, (float)w1, (float)w2);
            uv[2].y = GetInterpolatedAttribute(v0->texcoord2.y, v1->texcoord2.y, v2->texcoord2.y, v0, v1, v2, (float)w0, (float)w1, (float)w2);

            for (int i = 0; i < 3; ++i) {
                if (GPU_Regs[TEXTURE_UNITS_CONFIG] & (0x1 << i)) {
                    u8* texture_data = NULL;
                    switch (i) {
                    case 0:
                        texture_data = (u8*)(gpu_GetPhysicalMemoryBuff(GPU_Regs[TEXTURE_CONFIG0_ADDR] << 3));
                        break;
                    case 1:
                        texture_data = (u8*)(gpu_GetPhysicalMemoryBuff(GPU_Regs[TEXTURE_CONFIG1_ADDR] << 3));
                        break;
                    case 2:
                        texture_data = (u8*)(gpu_GetPhysicalMemoryBuff(GPU_Regs[TEXTURE_CONFIG2_ADDR] << 3));
                        break;
                    }

                    int row_stride = 0;
                    int format = 0;
                    int height = 0;
                    int width = 0;
                    int wrap_s = 0;
                    int wrap_t = 0;
                    switch (i) {
                    case 0:
                        height = (GPU_Regs[TEXTURE0_SIZE] & 0xFFFF);
                        width =  (GPU_Regs[TEXTURE0_SIZE] >> 16) & 0xFFFF;
                        wrap_t = (GPU_Regs[TEXTURE0_WRAP_FILTER] >> 8) & 3;
                        wrap_s = (GPU_Regs[TEXTURE0_WRAP_FILTER] >> 12) & 3;
                        format =  GPU_Regs[TEXTURE_CONFIG0_TYPE] & 0xF;
                        break;
                    case 1:
                        height = (GPU_Regs[TEXTURE_CONFIG1_SIZE] & 0xFFFF);
                        width =  (GPU_Regs[TEXTURE_CONFIG1_SIZE] >> 16) & 0xFFFF;
                        wrap_t = (GPU_Regs[TEXTURE_CONFIG1_WRAP] >> 8) & 3;
                        wrap_s = (GPU_Regs[TEXTURE_CONFIG1_WRAP] >> 12) & 3;
                        format =  GPU_Regs[TEXTURE_CONFIG1_TYPE] & 0xF;
                        break;
					case 2:
                        height = (GPU_Regs[TEXTURE_CONFIG2_SIZE] & 0xFFFF);
                        width =  (GPU_Regs[TEXTURE_CONFIG2_SIZE] >> 16) & 0xFFFF;
                        wrap_t = (GPU_Regs[TEXTURE_CONFIG2_WRAP] >> 8) & 3;
                        wrap_s = (GPU_Regs[TEXTURE_CONFIG2_WRAP] >> 12) & 3;
                        format =  GPU_Regs[TEXTURE_CONFIG2_TYPE] & 0xF;
                        break;
                    }


                    int s = (int)(uv[i].x * ((float)(width)));
                    int t = (int)(uv[i].y * ((float)(height)));
                    s = GetWrappedTexCoord(wrap_s, s, width);
                    t = height - 1 - GetWrappedTexCoord(wrap_t, t, height);

                    row_stride = NibblesPerPixel(format) * width / 2;

                    //Flip it vertically
                    //Don't flip for ETC1/ETC1A4
                    if (format < ETC1)
                        t = height - 1 - t;

                    state.texture_color[i] = LookupTexture(texture_data, s, t, format, row_stride, width, height, false);

                    /*FILE *f = fopen("test.bin", "wb");
                    fwrite(texture_data, 1, 0x400000, f);
                    fclose(f);*/
                }
            }

            state.combiner_buffer.v[0] =  GPU_Regs[TEX_ENV_BUF_COLOR] & 0xFF;
            state.combiner_buffer.v[1] = (GPU_Regs[TEX_ENV_BUF_COLOR] >> 8) & 0xFF;
            state.combiner_buffer.v[2] = (GPU_Regs[TEX_ENV_BUF_COLOR] >> 16) & 0xFF;
            state.combiner_buffer.v[3] = (GPU_Regs[TEX_ENV_BUF_COLOR] >> 24) & 0xFF;

            for (int i = 0; i < 6; i++) {
                u32 reg_num_addr = GL_TEX_ENV + i * 8;
                if (i > 3) reg_num_addr += 0x10;

                if (i > 0 && i < 5) {
                    if (((GPU_Regs[TEX_ENV_BUF_INPUT] >> (i + 7)) & 1) == 1) {
                        state.combiner_buffer.v[0] = state.combiner_output.v[0];
                        state.combiner_buffer.v[1] = state.combiner_output.v[1];
                        state.combiner_buffer.v[2] = state.combiner_output.v[2];
                    }
                    else {
                        state.combiner_buffer.v[0] = state.combiner_buffer.v[0];
                        state.combiner_buffer.v[1] = state.combiner_buffer.v[1];
                        state.combiner_buffer.v[2] = state.combiner_buffer.v[2];
                    }
                    if (((GPU_Regs[TEX_ENV_BUF_INPUT] >> (i + 11)) & 1) == 1) {
                        state.combiner_buffer.v[3] = state.combiner_output.v[3];
                    }
                    else {
                        state.combiner_buffer.v[3] = state.combiner_buffer.v[3];
                    }
                }

                state.combiner_constant.v[0] =  GPU_Regs[reg_num_addr + 3] & 0xFF;
                state.combiner_constant.v[1] = (GPU_Regs[reg_num_addr + 3] >> 8) & 0xFF;
                state.combiner_constant.v[2] = (GPU_Regs[reg_num_addr + 3] >> 16) & 0xFF;
                state.combiner_constant.v[3] = (GPU_Regs[reg_num_addr + 3] >> 24) & 0xFF;

                // color combiner
                // NOTE: Not sure if the alpha combiner might use the color output of the previous
                //       stage as input. Hence, we currently don't directly write the result to
                //       combiner_output.rgb(), but instead store it in a temporary variable until
                //       alpha combining has been done.
                clov3 color_result[3] = {
                    GetColorModifier((GPU_Regs[reg_num_addr + 1] >> 0) & 0xF, GetSource((GPU_Regs[reg_num_addr] >> 0) & 0xF)),
                    GetColorModifier((GPU_Regs[reg_num_addr + 1] >> 4) & 0xF, GetSource((GPU_Regs[reg_num_addr] >> 4) & 0xF)),
                    GetColorModifier((GPU_Regs[reg_num_addr + 1] >> 8) & 0xF, GetSource((GPU_Regs[reg_num_addr] >> 8) & 0xF)),
                };

                clov3 color_output = ColorCombine((GPU_Regs[reg_num_addr + 2] >> 0) & 0xF, color_result);

                u8 alpha_result[3] = {
                   GetAlphaModifier((GPU_Regs[reg_num_addr + 1] >> 12) & 0xB, GetSource((GPU_Regs[reg_num_addr] >> 16) & 0xF)),
                   GetAlphaModifier((GPU_Regs[reg_num_addr + 1] >> 16) & 0xB, GetSource((GPU_Regs[reg_num_addr] >> 20) & 0xF)),
                   GetAlphaModifier((GPU_Regs[reg_num_addr + 1] >> 20) & 0xB, GetSource((GPU_Regs[reg_num_addr] >> 24) & 0xF)),
                };

                u8 alpha_output = AlphaCombine((GPU_Regs[reg_num_addr + 2] >> 16) & 0xF, alpha_result);

                //state.combiner_output.v[0] = min((unsigned)255, color_output.v[0] * tev_stage.GetColorMultiplier);
                //state.combiner_output.v[1] = min((unsigned)255, color_output.v[1] * tev_stage.GetColorMultiplier);
                //state.combiner_output.v[2] = min((unsigned)255, color_output.v[2] * tev_stage.GetColorMultiplier);
                //state.combiner_output.v[3] = min((unsigned)255, alpha_output * tev_stage.GetAlphaMultiplier);

                //if (regs.tev_combiner_buffer_input.TevStageUpdatesCombinerBufferColor(tev_stage_index)) {
                //    combiner_buffer.r() = combiner_output.r();
                //    combiner_buffer.g() = combiner_output.g();
                //    combiner_buffer.b() = combiner_output.b();
                }

                //if (regs.tev_combiner_buffer_input.TevStageUpdatesCombinerBufferAlpha(tev_stage_index)) {
                //    combiner_buffer.v[3] = combiner_output.v[3];
                //}
            //}

            if (GPU_Regs[ALPHA_TEST] & 1)
            {
                bool pass = false;

                switch (((GPU_Regs[ALPHA_TEST] >> 4) & 7)) {
                case 0: //Never
                    pass = false;
                    break;

                case 1: //Always
                    pass = true;
                    break;

                case 2: //Equal
                    pass = state.combiner_output.v[3] == ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 3: //NotEqual
                    pass = state.combiner_output.v[3] != ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 4: //LessThan
                    pass = state.combiner_output.v[3] < ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 5: //LessThanOrEqual
                    pass = state.combiner_output.v[3] <= ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 6: //GreaterThan
                    pass = state.combiner_output.v[3] > ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 7: //GreaterThanOrEqual
                    pass = state.combiner_output.v[3] >= ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                default:
                    DEBUG("Unknown alpha test function %x\n", (GPU_Regs[ALPHA_TEST] >> 4) & 7);
                    break;
                }

                if (!pass)
                    continue;
            }

            // TODO: Does depth indeed only get written even if depth testing is enabled?
            if (GPU_Regs[DEPTH_COLOR_MASK] & 1)
            {
                u32 num_bits = gpu_DepthBitsPerPixel(framebuffer.depth_format);
                u32 z = (u32)((v0->screenpos.z * w0 +
                               v1->screenpos.z * w1 +
                               v2->screenpos.z * w2) * ((1 << num_bits) - 1) / wsum);

                u32 ref_z = GetDepth(state.x >> 4, state.y >> 4);

                bool pass = false;

                switch ((GPU_Regs[DEPTH_COLOR_MASK] >> 4) & 7) {
                case 0: //Never
                    pass = false;
                    break;

                case 1: //Always
                    pass = true;
                    break;

                case 2: //Equal
                    pass = z == ref_z;
                    break;

                case 3: //NotEqual
                    pass = z != ref_z;
                    break;

                case 4: //LessThan
                    pass = z < ref_z;
                    break;

                case 5: //LessThanOrEqual
                    pass = z <= ref_z;
                    break;

                case 6: //GreaterThan
                    pass = z > ref_z;
                    break;

                case 7: //GreaterThanOrEqual
                    pass = z >= ref_z;
                    break;

                default:
                    DEBUG("Unknown depth test function %x\n", (GPU_Regs[DEPTH_COLOR_MASK] >> 4) & 7);
                    break;
                }

                if (!pass)
                    continue;

                if ((GPU_Regs[DEPTH_COLOR_MASK] >> 12) & 1)
                    SetDepth(state.x >> 4, state.y >> 4, z);
            }

            clov4 srcfactor, dstfactor, result;
            state.dest = GetPixel(state.x >> 4, state.y >> 4);
            clov4 blend_output = state.combiner_output;

            //Alpha blending
            if ((GPU_Regs[COLOR_OP] >> 8) & 1) //Alpha blending enabled
            {
                u32 factor_source_rgb = (GPU_Regs[BLEND_FUNC] >> 16) & 0xF;
                u32 factor_source_a = (GPU_Regs[BLEND_FUNC] >> 24) & 0xF;
                srcfactor = make_vec4_from_vec3(LookupFactorRGB(factor_source_rgb), LookupFactorA(factor_source_a));

                u32 factor_dest_rgb = (GPU_Regs[BLEND_FUNC] >> 20) & 0xF;
                u32 factor_dest_a = (GPU_Regs[BLEND_FUNC] >> 28) & 0xF;
                dstfactor = make_vec4_from_vec3(LookupFactorRGB(factor_dest_rgb), LookupFactorA(factor_dest_a));

                u32 blend_equation_rgb = GPU_Regs[BLEND_FUNC] & 0xFF;
                u32 blend_equation_a = (GPU_Regs[BLEND_FUNC] >> 8) & 0xFF;

                switch (blend_equation_rgb)
                {
                case 0: //Add
                    result.v[0] = CLAMP((((int)state.combiner_output.v[0] * (int)srcfactor.v[0] + (int)state.dest.v[0] * (int)dstfactor.v[0]) / 255), 0, 255);
                    result.v[1] = CLAMP((((int)state.combiner_output.v[1] * (int)srcfactor.v[1] + (int)state.dest.v[1] * (int)dstfactor.v[1]) / 255), 0, 255);
                    result.v[2] = CLAMP((((int)state.combiner_output.v[2] * (int)srcfactor.v[2] + (int)state.dest.v[2] * (int)dstfactor.v[2]) / 255), 0, 255);
                    break;
                case 1: //Subtract
                    result.v[0] = CLAMP((((int)state.combiner_output.v[0] * (int)srcfactor.v[0] - (int)state.dest.v[0] * (int)dstfactor.v[0]) / 255), 0, 255);
                    result.v[1] = CLAMP((((int)state.combiner_output.v[1] * (int)srcfactor.v[1] - (int)state.dest.v[1] * (int)dstfactor.v[1]) / 255), 0, 255);
                    result.v[2] = CLAMP((((int)state.combiner_output.v[2] * (int)srcfactor.v[2] - (int)state.dest.v[2] * (int)dstfactor.v[2]) / 255), 0, 255);
                    break;
                case 2: //Reverse Subtract
                    result.v[0] = CLAMP((((int)state.dest.v[0] * (int)dstfactor.v[0] - (int)state.combiner_output.v[0] * (int)srcfactor.v[0]) / 255), 0, 255);
                    result.v[1] = CLAMP((((int)state.dest.v[1] * (int)dstfactor.v[1] - (int)state.combiner_output.v[1] * (int)srcfactor.v[1]) / 255), 0, 255);
                    result.v[2] = CLAMP((((int)state.dest.v[2] * (int)dstfactor.v[2] - (int)state.combiner_output.v[2] * (int)srcfactor.v[2]) / 255), 0, 255);
                    break;
                default:
                    DEBUG("Unknown RGB blend equation %x", blend_equation_rgb);
                    break;
                }

                switch (blend_equation_a)
                {
                case 0: //Add
                    result.v[3] = CLAMP((((int)state.combiner_output.v[3] * (int)srcfactor.v[3] + (int)state.dest.v[3] * (int)dstfactor.v[3]) / 255), 0, 255);
                    break;
                case 1: //Subtract
                    result.v[3] = CLAMP((((int)state.combiner_output.v[3] * (int)srcfactor.v[3] - (int)state.dest.v[3] * (int)dstfactor.v[3]) / 255), 0, 255);
                    break;
                case 2: //Reverse Subtract
                    result.v[3] = CLAMP((((int)state.dest.v[3] * (int)dstfactor.v[3] - (int)state.combiner_output.v[3] * (int)srcfactor.v[3]) / 255), 0, 255);
                    break;
                default:
                    DEBUG("Unknown A blend equation %x", blend_equation_a);
                    break;
				}

                memcpy(&state.combiner_output, &result, sizeof(clov4));

            }
            else
            {
                DEBUG("logic op: %x", GPU_Regs[LOGIC_OP] & 0xF);
			}

            ((GPU_Regs[LOGIC_OP] >> 8) & 1) ? state.combiner_output.v[0] : state.dest.v[0];
            ((GPU_Regs[LOGIC_OP] >> 9) & 1) ? state.combiner_output.v[1] : state.dest.v[1];
            ((GPU_Regs[LOGIC_OP] >> 10) & 1) ? state.combiner_output.v[2] : state.dest.v[2];
            ((GPU_Regs[LOGIC_OP] >> 11) & 1) ? state.combiner_output.v[3] : state.dest.v[3];

            DrawPixel((state.x >> 4), (state.y >> 4), result);
        }
    }
}

void rasterizer_ProcessTriangle(OutputVertex * v0,
                                OutputVertex * v1,
                                OutputVertex * v2) {
    ProcessTriangleInternal(v0, v1, v2, false);
}
