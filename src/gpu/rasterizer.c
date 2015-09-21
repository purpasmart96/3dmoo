/*
* Copyright (C) 2014 - Tony Wasserka.
* Copyright (C) 2014 - plutoo
* Copyright (C) 2014 - ichfly
* Copyright (C) 2014 - Normmatt
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

struct vec3_12P4 {
    s16 v[3];//x, y,z;
};


typedef struct {

    clov4 combiner_output;
	clov4 combiner_constant;
    clov4 combiner_buffer;
	clov4 primary_color;
	clov4 texture_color[4];
	clov4 dest;
	u16 x;
	u16 y;

} RasterState;

RasterState state;


static bool IsRightSideOrFlatBottomEdge(struct vec3_12P4 * vtx, struct vec3_12P4 *line1, struct vec3_12P4 *line2)
{
    if (line1->v[1] == line2->v[1]) {
        // just check if vertex is above us => bottom line parallel to x-axis
        return vtx->v[1] < line1->v[1];
    } else {
        // check if vertex is on our left => right side
        // TODO: Not sure how likely this is to overflow
        return (int)vtx->v[0] < (int)line1->v[0] + ((int)line2->v[0] - (int)line1->v[0]) * ((int)vtx->v[1] - (int)line1->v[1]) / ((int)line2->v[1] - (int)line1->v[1]);
    }
}

static int orient2d(u16 vtx1x, u16  vtx1y, u16  vtx2x, u16  vtx2y, u16  vtx3x, u16  vtx3y)
{
    s32 vec1x = vtx2x - vtx1x;
    s32 vec1y = vtx2y - vtx1y;
    s32 vec2x = vtx3x - vtx1x;
    s32 vec2y = vtx3y - vtx1y;
    // TODO: There is a very small chance this will overflow for sizeof(int) == 4
    return vec1x*vec2y - vec1y*vec2x;
}

static u16 GetDepth(int x, int y) 
{
    u32 inputdim = GPU_Regs[Framebuffer_FORMAT11E];
    u32 outy = (inputdim & 0x7FF);
    u32 outx = ((inputdim >> 12) & 0x3FF);

    y = (outx - y);
    u16* depth_buffer = (u16*)gpu_GetPhysicalMemoryBuff(GPU_Regs[DEPTH_BUFFER_ADDRESS] << 3);

    return *(depth_buffer + x + y * (GPU_Regs[Framebuffer_FORMAT11E] & 0xFFF) / 2);
}

static void SetDepth(int x, int y, u16 value)
{
    u32 inputdim = GPU_Regs[Framebuffer_FORMAT11E];
    u32 outy = (inputdim & 0x7FF);
    u32 outx = ((inputdim >> 12) & 0x3FF);

    y = (outx - y);
    u16* depth_buffer = (u16*)gpu_GetPhysicalMemoryBuff(GPU_Regs[DEPTH_BUFFER_ADDRESS] << 3);

    // Assuming 16-bit depth buffer format until actual format handling is implemented
    if (depth_buffer) //there is no depth_buffer
        *(depth_buffer + x + y * (GPU_Regs[Framebuffer_FORMAT11E] & 0xFFF) / 2) = value;
}


static void DrawPixel(int x, int y, const clov4* color)
{
    u8* color_buffer = (u8*)gpu_GetPhysicalMemoryBuff(GPU_Regs[COLOR_BUFFER_ADDRESS] << 3);

    u32 inputdim = GPU_Regs[Framebuffer_FORMAT11E];
    u32 outy = (inputdim & 0x7FF);
    u32 outx = ((inputdim >> 12) & 0x3FF);

    y = (outx - y);

    //TODO: workout why this seems required for ctrulib gpu demo (outy=480)
    if(outy > 240) outy = 240;

    //DEBUG("x=%d,y=%d,outx=%d,outy=%d,format=%d,inputdim=%08X,bufferformat=%08X\n", x, y, outx, outy, (GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000) >> 12, inputdim, GPU_Regs[COLOR_BUFFER_FORMAT]);

    Color ncolor;
    ncolor.r = color->v[0];
    ncolor.g = color->v[1];
    ncolor.b = color->v[2];
    ncolor.a = color->v[3];

    u8* outaddr;
    // Assuming RGB8 format until actual framebuffer format handling is implemented
    switch (GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000) { //input format

    case 0: //RGBA8
        outaddr = color_buffer + x * 4 + y * (outy)* 4; //check if that is correct
        color_encode(&ncolor, RGBA8, outaddr);
        break;
    case 0x1000: //RGB8
        outaddr = color_buffer + x * 3 + y * (outy)* 3; //check if that is correct
        color_encode(&ncolor, RGB8, outaddr);
        break;
    case 0x2000: //RGB565
        outaddr = color_buffer + x * 2 + y * (outy)* 2; //check if that is correct
        color_encode(&ncolor, RGB565, outaddr);
        break;
    case 0x3000: //RGB5A1
        outaddr = color_buffer + x * 2 + y * (outy)* 2; //check if that is correct
        color_encode(&ncolor, RGBA5551, outaddr);
        break;
    case 0x4000: //RGBA4
        outaddr = color_buffer + x * 2 + y * (outy)* 2; //check if that is correct
        color_encode(&ncolor, RGBA4, outaddr);
        break;
    default:
        DEBUG("error unknown output format %04X\n", GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000);
        break;
    }

}

#define GetPixel RetrievePixel
static clov4 RetrievePixel(int x, int y)
{

    u8* color_buffer = (u8*)gpu_GetPhysicalMemoryBuff(GPU_Regs[COLOR_BUFFER_ADDRESS] << 3);

    u32 inputdim = GPU_Regs[Framebuffer_FORMAT11E];
    u32 outy = (inputdim & 0x7FF);
    u32 outx = ((inputdim >> 12) & 0x3FF);

    y = (outx - y);
    //TODO: workout why this seems required for ctrulib gpu demo (outy=480)
    if(outy > 240) outy = 240;

    //DEBUG("x=%d,y=%d,outx=%d,outy=%d,format=%d,inputdim=%08X,bufferformat=%08X\n", x, y, outx, outy, (GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000) >> 12, inputdim, GPU_Regs[COLOR_BUFFER_FORMAT]);

    Color ncolor;
    memset(&ncolor, 0, sizeof(Color));

    u8* addr;
    // Assuming RGB8 format until actual framebuffer format handling is implemented
    switch(GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000) { //input format

        case 0: //RGBA8
            addr = color_buffer + x * 4 + y * (outy)* 4; //check if that is correct
            color_decode(addr, RGBA8, &ncolor);
            break;
        case 0x1000: //RGB8
            addr = color_buffer + x * 3 + y * (outy)* 3; //check if that is correct
            color_decode(addr, RGB8, &ncolor);
            break;
        case 0x2000: //RGB565
            addr = color_buffer + x * 2 + y * (outy)* 2; //check if that is correct
            color_decode(addr, RGB565, &ncolor);
            break;
        case 0x3000: //RGB5A1
            addr = color_buffer + x * 2 + y * (outy)* 2; //check if that is correct
            color_decode(addr, RGBA5551, &ncolor);
            break;
        case 0x4000: //RGBA4
            addr = color_buffer + x * 2 + y * (outy)* 2; //check if that is correct
            color_decode(addr, RGBA4, &ncolor);
            break;
        default:
            DEBUG("error unknown output format %04X\n", GPU_Regs[COLOR_BUFFER_FORMAT] & 0x7000);
            break;
    }

    clov4 ret;
	ret.v[0] = ncolor.r;
	ret.v[1] = ncolor.g;
	ret.v[2] = ncolor.b;
	ret.v[3] = ncolor.a;

	return ret;
}

static float GetInterpolatedAttribute(float attr0, float attr1, float attr2, const struct OutputVertex *v0, const struct OutputVertex * v1,
                                      const struct OutputVertex * v2,float w0,float w1, float w2)
{
    float interpolated_attr_over_w = (attr0 / v0->position.v[3])*w0 + (attr1 / v1->position.v[3])*w1 + (attr2 / v2->position.v[3])*w2;
    float interpolated_w_inverse = ((1.f) / v0->position.v[3])*w0 + ((1.f) / v1->position.v[3])*w1 + ((1.f) / v2->position.v[3])*w2;
    return interpolated_attr_over_w / interpolated_w_inverse;
}

clov4 GetSource(u32 source) 
{
    switch (source)
    {
    case 0u: //PrimaryColor

    // HACK: Until we implement fragment lighting, use primary_color
    case 1u: //PrimaryFragmentColor
        return state.primary_color;

    // HACK: Until we implement fragment lighting, use zero
    // case 2: //SecondaryFragmentColor
    //    return 0;

    case 3u: //Texture0
        return state.texture_color[0];

    case 4u: //Texture1
        return state.texture_color[1];

    case 5u: //Texture2
        return state.texture_color[2];

    case 13u: //PreviousBuffer
        return state.combiner_buffer;

    case 14u: //Constant
        return state.combiner_constant;

    case 15u: //Previous
        return state.combiner_output;

    default:
        ERROR("Unknown combiner source %d\n", (int)source);
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
    case 0u: //SourceAlpha
		return values.v[3];

    case 1u: //OneMinusSourceAlpha
        return 255 - values.v[3];

    case 2u: //SourceRed
        return values.v[0];

    case 3u: //OneMinusRed
        return 255 - values.v[0];

    case 4u: //SourceGreen
        return values.v[1];

    case 5u: //OneMinusSourceGreen
        return 255 - values.v[1];

    case 6u: //SourceBlue
        return values.v[2];

    case 7u: //OneMinusSourceBlue
        return 255 - values.v[2];
    }
};

static u8 AlphaCombine (u32 op, u8 input[3])
{
    switch (op) {
    case 0: // Replace
        return input[0];

    case 1: // Modulate
        return input[0] * input[1] / 255;

    case 2: // Add
        return MIN(255, input[0] + input[1]);

    case 3: // AddSigned
    {
        // TODO(bunnei): Verify that the color conversion from (float) 0.5f to (byte) 128 is correct
        int result = (int)(input[0]) + (int)(input[1]) - 128;
        return (u8)(CLAMP(result, 0, 255));
    }

    case 4: // Lerp
        return (input[0] * input[2] + input[1] * (255 - input[2])) / 255;

    case 5: // Subtract
        return MAX(0, (int)input[0] - (int)input[1]);

    case 8: // MultiplyThenAdd
        return MIN(255, (input[0] * input[1] + 255 * input[2]) / 255);

    case 9: // AddThenMultiply
        return (MIN(255, (input[0] + input[1])) * input[2]) / 255;

    default:
        ERROR("Unknown alpha combiner operation %d\n", (int)op);
        return 0;
    }
}

clov3 ColorCombine(u32 op, clov3 input[3])
{
    clov3 ret;

    switch (op) {
	case 0: // Replace
        ret.v[0] = input[0].v[0];
        ret.v[1] = input[0].v[1];
        ret.v[2] = input[0].v[2];
        break;

   case 1: // Modulate
        ret.v[0] = (input)[0].v[0] * (input)[1].v[0] / 255;
        ret.v[1] = (input)[0].v[1] * (input)[1].v[1] / 255;
        ret.v[2] = (input)[0].v[2] * (input)[1].v[2] / 255;
        break;

	case 2: // Add
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

	case 3: // Add Signed
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

   case 4: // Lerp
        ret.v[0] = ((input[0].v[0] * input[2].v[0] + input[1].v[0] * 255 - input[2].v[0]) / 255);
        ret.v[1] = ((input[0].v[1] * input[2].v[1] + input[1].v[1] * 255 - input[2].v[1]) / 255);
        ret.v[2] = ((input[0].v[2] * input[2].v[2] + input[1].v[2] * 255 - input[2].v[2]) / 255);
        break;

    case 5: // Subtract
    {
        vec3_int result;
        result.v[0] = input[0].v[0] - input[1].v[0];
        result.v[1] = input[0].v[1] - input[1].v[1];
        result.v[2] = input[0].v[2] - input[1].v[2];

        ret.v[0] = MAX(0, result.v[0]);
        ret.v[1] = MAX(0, result.v[1]);
        ret.v[2] = MAX(0, result.v[2]);
        break;
    }

    case 6: // Dot3_RGB
    {
        // Not fully accurate.
        // Worst case scenario seems to yield a +/-3 error
        // Some HW results indicate that the per-component computation can't have a higher precision than 1/256,
        // while dot3_rgb( (0x80,g0,b0),(0x7F,g1,b1) ) and dot3_rgb( (0x80,g0,b0),(0x80,g1,b1) ) give different results
        vec3_int result;
        result.v[0] = ((input[0].v[0] * 2 - 255) * (input[1].v[0] * 2 - 255) + 128) / 256;
        result.v[1] = ((input[0].v[1] * 2 - 255) * (input[1].v[1] * 2 - 255) + 128) / 256;
        result.v[2] = ((input[0].v[2] * 2 - 255) * (input[1].v[2] * 2 - 255) + 128) / 256;

        ret.v[0] = MAX(0, MIN(255, result.v[0]));
        ret.v[1] = MAX(0, MIN(255, result.v[1]));
        ret.v[2] = MAX(0, MIN(255, result.v[2]));
        break;
	}

    case 8: // MultiplyThenAdd
    {
        vec3_int result;
        result.v[0] = (input[0].v[0] * input[1].v[0] + 255 * input[2].v[0]) / 255;
        result.v[1] = (input[0].v[1] * input[1].v[1] + 255 * input[2].v[1]) / 255;
        result.v[2] = (input[0].v[2] * input[1].v[2] + 255 * input[2].v[2]) / 255;
        ret.v[0] = MIN(255, result.v[0]);
        ret.v[1] = MIN(255, result.v[1]);
        ret.v[2] = MIN(255, result.v[2]);
        break;
    }

	case 9: // AddThenMultiply
    {
        vec3_int result;
        result.v[0] = input[0].v[0] + input[1].v[0];
        result.v[1] = input[0].v[1] + input[1].v[1];
        result.v[2] = input[0].v[2] + input[1].v[2];

        result.v[0] = MIN(255, result.v[0]);
        result.v[1] = MIN(255, result.v[1]);
        result.v[2] = MIN(255, result.v[2]);

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
    //    return vec3_u8(255 - registers.output_merger.blend_const.r, 255 - registers.output_merger.blend_const.g, 255 - registers.output_merger.blend_const.b);

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
		return 0;
    }
}

typedef enum
{
	BLEND_ADD = 0,
	BLEND_SUBTRACT = 1,
	BLEND_REVERSE_SUBTRACT = 2,
	BLEND_MIN = 3,
	BLEND_MAX = 4
} BlendEquation;

clov4 EvaluateBlendEquation(clov4 src, clov4 srcfactor, clov4 dest, clov4 destfactor, BlendEquation equation)
{
    vec4_int result;
    vec4_int src_result;
    vec4_int dst_result;

    src_result.v[0] = (int)(src.v[0] * srcfactor.v[0]);
    src_result.v[1] = (int)(src.v[1] * srcfactor.v[1]);
    src_result.v[2] = (int)(src.v[2] * srcfactor.v[2]);
    src_result.v[3] = (int)(src.v[3] * srcfactor.v[3]);

    dst_result.v[0] = (int)(dest.v[0] * destfactor.v[0]);
    dst_result.v[1] = (int)(dest.v[1] * destfactor.v[1]);
    dst_result.v[2] = (int)(dest.v[2] * destfactor.v[2]);
    dst_result.v[3] = (int)(dest.v[3] * destfactor.v[3]);

    switch (equation) {
    case BLEND_ADD:
        result.v[0] = (src_result.v[0] + dst_result.v[0]) / 255;
        result.v[1] = (src_result.v[1] + dst_result.v[1]) / 255;
        result.v[2] = (src_result.v[2] + dst_result.v[2]) / 255;
        result.v[3] = (src_result.v[3] + dst_result.v[3]) / 255;
        break;

    case BLEND_SUBTRACT:
        result.v[0] = (src_result.v[0] - dst_result.v[0]) / 255;
        result.v[1] = (src_result.v[1] - dst_result.v[1]) / 255;
        result.v[2] = (src_result.v[2] - dst_result.v[2]) / 255;
        result.v[3] = (src_result.v[3] - dst_result.v[3]) / 255;
        break;

    case BLEND_REVERSE_SUBTRACT:
        result.v[0] = (dst_result.v[0] - src_result.v[0]) / 255;
        result.v[1] = (dst_result.v[1] - src_result.v[1]) / 255;
        result.v[2] = (dst_result.v[2] - src_result.v[2]) / 255;
        result.v[3] = (dst_result.v[3] - src_result.v[3]) / 255;
        break;

    case BLEND_MIN:
        result.v[0] = MIN(src.v[0], dest.v[0]);
        result.v[1] = MIN(src.v[1], dest.v[1]);
        result.v[2] = MIN(src.v[2], dest.v[2]);
        result.v[3] = MIN(src.v[3], dest.v[3]);
        break;

    case BLEND_MAX:
        result.v[0] = MAX(src.v[0], dest.v[0]);
        result.v[1] = MAX(src.v[1], dest.v[1]);
        result.v[2] = MAX(src.v[2], dest.v[2]);
        result.v[3] = MAX(src.v[3], dest.v[3]);
        break;

    default:
        ERROR("Unknown RGB blend equation %x\n", equation);
    }

    clov4 ret;

    ret.v[0] = CLAMP(result.v[0], 0, 255);
    ret.v[1] = CLAMP(result.v[1], 0, 255);
    ret.v[2] = CLAMP(result.v[2], 0, 255);
    ret.v[3] = CLAMP(result.v[3], 0, 255);

    return ret;
}


typedef enum
{
    Clear        =  0,
    And          =  1,
    AndReverse   =  2,
    Copy         =  3,
    Set          =  4,
    CopyInverted =  5,
    NoOp         =  6,
    Invert       =  7,
    Nand         =  8,
    Or           =  9,
    Nor          = 10,
    Xor          = 11,
    Equiv        = 12,
    AndInverted  = 13,
    OrReverse    = 14,
    OrInverted   = 15,
} LogicOpMode;

static u8 LogicOp(u8 src, u8 dest, LogicOpMode op)
{
    switch (op)
    {
    case Clear:
        return 0;

    case And:
        return src & dest;

    case AndReverse:
        return src & ~dest;

    case Copy:
        return src;

    case Set:
        return 255;

    case CopyInverted:
        return ~src;

    case NoOp:
        return dest;

    case Invert:
        return ~dest;

    case Nand:
        return ~(src & dest);

    case Or:
        return src | dest;

    case Nor:
        return ~(src | dest);

    case Xor:
        return src ^ dest;

    case Equiv:
        return ~(src ^ dest);

    case AndInverted:
        return ~src & dest;

    case OrReverse:
        return src | ~dest;

    case OrInverted:
        return ~src | dest;
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
    case ClampToBorder:
        ret = val;
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


static unsigned NibblesPerPixel(TextureFormat format) {
    switch(format) {
        case RGBA8:
            return 8;

        case RGB8:
            return 6;

        case RGBA5551:
        case RGB565:
        case RGBA4:
        case IA8:
            return 4;

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
    //DEBUG("Format=%d\n", format);
    // Images are split into 8x8 tiles. Each tile is composed of four 4x4 subtiles each
    // of which is composed of four 2x2 subtiles each of which is composed of four texels.
    // Each structure is embedded into the next-bigger one in a diagonal pattern, e.g.
    // texels are laid out in a 2x2 subtile like this:
    // 2 3
    // 0 1
    //
    // The full 8x8 tile has the texels arranged like this:
    //
    // 42 43 46 47 58 59 62 63
    // 40 41 44 45 56 57 60 61
    // 34 35 38 39 50 51 54 55
    // 32 33 36 37 48 49 52 53
    // 10 11 14 15 26 27 30 31
    // 08 09 12 13 24 25 28 29
    // 02 03 06 07 18 19 22 23
    // 00 01 04 05 16 17 20 21

    // TODO(neobrain): Not sure if this swizzling pattern is used for all textures.
    // To be flexible in case different but similar patterns are used, we keep this
    // somewhat inefficient code around for now.
    int texel_index_within_tile = 0;
    for(int block_size_index = 0; block_size_index < 3; ++block_size_index) {
        int sub_tile_width = 1 << block_size_index;
        int sub_tile_height = 1 << block_size_index;

        int sub_tile_index = (x & sub_tile_width) << block_size_index;
        sub_tile_index += 2 * ((y & sub_tile_height) << block_size_index);
        texel_index_within_tile += sub_tile_index;
    }

    const int block_width = 8;
    const int block_height = 8;

    int coarse_x = (x / block_width) * block_width;
    int coarse_y = (y / block_height) * block_height;

    clov4 ret;

    switch(format) {
        case RGBA8:
        {
            const u8* source_ptr = source + coarse_x * block_height * 4 + coarse_y * stride + texel_index_within_tile * 4;
            ret.v[0] = source_ptr[3];
            ret.v[1] = source_ptr[2];
            ret.v[2] = source_ptr[1];
            ret.v[3] = disable_alpha ? 255 : source_ptr[0];
            break;
        }

        case RGB8:
        {
            const u8* source_ptr = source + coarse_x * block_height * 3 + coarse_y * stride + texel_index_within_tile * 3;
            ret.v[0] = source_ptr[0];
            ret.v[1] = source_ptr[1];
            ret.v[2] = source_ptr[2];
            ret.v[3] = 255;
            break;
        }

        case RGBA5551:
        {
            const u16 source_ptr = *(const u16*)(source + coarse_x * block_height * 2 + coarse_y * stride + texel_index_within_tile * 2);
            u8 r = (source_ptr >> 11) & 0x1F;
            u8 g = ((source_ptr) >> 6) & 0x1F;
            u8 b = (source_ptr >> 1) & 0x1F;
            u8 a = source_ptr & 1;

            ret.v[0] = (r << 3) | (r >> 2);
            ret.v[1] = (g << 3) | (g >> 2);
            ret.v[2] = (b << 3) | (b >> 2);
            ret.v[3] = disable_alpha ? 255 : (a * 255);
            break;
        }

        case RGB565:
        {
            const u16 source_ptr = *(const u16*)(source + coarse_x * block_height * 2 + coarse_y * stride + texel_index_within_tile * 2);
            u8 r = (source_ptr >> 11) & 0x1F;
            u8 g = ((source_ptr) >> 5) & 0x3F;
            u8 b = (source_ptr)& 0x1F;
            ret.v[0] = (r << 3) | (r >> 2);
            ret.v[1] = (g << 2) | (g >> 4);
            ret.v[2] = (b << 3) | (b >> 2);
            ret.v[3] = 255;
            break;
        }

        case RGBA4:
        {
            const u8* source_ptr = source + coarse_x * block_height * 2 + coarse_y * stride + texel_index_within_tile * 2;
            u8 r = source_ptr[1] >> 4;
            u8 g = source_ptr[1] & 0xFF;
            u8 b = source_ptr[0] >> 4;
            u8 a = source_ptr[0] & 0xFF;
            r = (r << 4) | r;
            g = (g << 4) | g;
            b = (b << 4) | b;
            a = (a << 4) | a;

            ret.v[0] = r;
            ret.v[1] = g;
            ret.v[2] = b;
            ret.v[3] = disable_alpha ? 255 : a;
            break;
        }

        case IA8:
        {
            const u8* source_ptr = source + coarse_x * block_height * 2 + coarse_y * stride + texel_index_within_tile * 2;

            // TODO: Better control this...
            if(disable_alpha) {
                ret.v[0] = *(source_ptr + 1);
                ret.v[1] = *source_ptr;
                ret.v[2] = 0;
                ret.v[3] = 255;
            }
            else {
                ret.v[0] = *source_ptr;
                ret.v[1] = *source_ptr;
                ret.v[2] = *source_ptr;
                ret.v[3] = *(source_ptr + 1);
            }
            break;
        }

        case I8:
        {
            const u8* source_ptr = source + coarse_x * block_height + coarse_y * stride + texel_index_within_tile;

            // TODO: Better control this...
            ret.v[0] = *source_ptr;
            ret.v[1] = *source_ptr;
            ret.v[2] = *source_ptr;
            ret.v[3] = 255;
            break;
        }

        case A8:
        {
            const u8* source_ptr = source + coarse_x * block_height + coarse_y * stride + texel_index_within_tile;

            // TODO: Better control this...
            if(disable_alpha) {
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
            const u8* source_ptr = source + coarse_x * block_height + coarse_y * stride + texel_index_within_tile;

            // TODO: Order?
            u8 i = ((*source_ptr) & 0xF0) >> 4;
            u8 a = (*source_ptr) & 0xF;
            a |= a << 4;
            i |= i << 4;

            // TODO: Better control this...
            if(disable_alpha) {
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
            const u8* source_ptr = source + coarse_x * block_height / 2 + coarse_y * stride + texel_index_within_tile / 2;

            // TODO: Order?
            u8 i = (coarse_x % 2) ? ((*source_ptr) & 0xF) : (((*source_ptr) & 0xF0) >> 4);
            i |= i << 4;

            ret.v[0] = i;
            ret.v[1] = i;
            ret.v[2] = i;
            ret.v[3] = 255;
            break;
        }

        case A4:
        {
            const u8* source_ptr = source + coarse_x * block_height / 2 + coarse_y * stride + texel_index_within_tile / 2;

            // TODO: Order?
            u8 a = (coarse_x % 2) ? ((*source_ptr) & 0xF) : (((*source_ptr) & 0xF0) >> 4);
            a |= a << 4;

            // TODO: Better control this...
            if(disable_alpha) {
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
            if(format == ETC1A4)
            {
                alpha = *(u64*)&source_ptr[offs];
                offs += 8;
            }
            u64 data = *(u64*)&source_ptr[offs];
            bool diffbit = ((data >> 33) & 1) == 1;
            bool flipbit = ((data >> 32) & 1) == 1; //0: |||, 1: |-|
            int r1, r2, g1, g2, b1, b2;
            if(diffbit) //'differential' mode
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

            if((flipbit && wantedY < 2) || (!flipbit && wantedX < 2))
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

static u16 FracMask()
{ 
    return 0xF;
}

static u16 IntMask()
{ 
    return (u16)~0xF; 
}

static s16 FloatToFix(float flt)
{
	// TODO: Rounding here is necessary to prevent garbage pixels at
	//       triangle borders. Is it that the correct solution, though?
	return ((s16)(roundf(flt * 16.0f)));
}

vec3_Fix12P4 ScreenToRasterizerCoordinates(vec3 vec)
{
	vec3_Fix12P4 m = { FloatToFix(vec.x), FloatToFix(vec.y), FloatToFix(vec.z) };
	return m;
}

void rasterizer_ProcessTriangle(struct OutputVertex * v0,
                                struct OutputVertex * v1,
                                struct OutputVertex * v2)
{
    // NOTE: Assuming that rasterizer coordinates are 12.4 fixed-point values

	    vec3_Fix12P4 vtxpostemp;
        struct OutputVertex *vtemp;

    vec3_Fix12P4 vtxpos[3] = {
        ScreenToRasterizerCoordinates(v0->screenpos),
        ScreenToRasterizerCoordinates(v1->screenpos),
        ScreenToRasterizerCoordinates(v2->screenpos),
	};

    // TODO: Proper scissor rect test!

    if ((GPU_Regs[CULL_MODE] & 0x3) == 1) { //KeepClockWise
        // Reverse vertex order and use the CW code path.
		memcpy(&vtxpostemp, &vtxpos[2], sizeof(vec3_Fix12P4));
		memcpy(&vtxpos[2], &vtxpos[1], sizeof(vec3_Fix12P4));
		memcpy(&vtxpos[1], &vtxpostemp, sizeof(vec3_Fix12P4));

        vtemp = v2;
        v2 = v1;
        v1 = vtemp;
    }
    if ((GPU_Regs[CULL_MODE] & 0x3) != 0) { //mode KeepCounterClockWise or undefined        // Cull away triangles which are wound counter-clockwise.
        // TODO: Make work :(
		if (orient2d(vtxpos[0].x, vtxpos[0].y, vtxpos[1].x, vtxpos[1].y, vtxpos[2].x, vtxpos[2].y) <= 0)
        {
			memcpy(&vtxpostemp, &vtxpos[2], sizeof(vec3_Fix12P4));
			memcpy(&vtxpos[2], &vtxpos[1], sizeof(vec3_Fix12P4));
			memcpy(&vtxpos[1], &vtxpostemp, sizeof(vec3_Fix12P4));

            vtemp = v2;
            v2 = v1;
            v1 = vtemp;
        }
    }
    else {
        // TODO: Consider A check for degenerate triangles ("SignedArea == 0")
        if (orient2d(vtxpos[0].x, vtxpos[0].y, vtxpos[1].x, vtxpos[1].y, vtxpos[2].x, vtxpos[2].y) <= 0)
        {
			memcpy(&vtxpostemp, &vtxpos[2], sizeof(vec3_Fix12P4));
			memcpy(&vtxpos[2], &vtxpos[1], sizeof(vec3_Fix12P4));
			memcpy(&vtxpos[1], &vtxpostemp, sizeof(vec3_Fix12P4));

            vtemp = v2;
            v2 = v1;
            v1 = vtemp;
        }
    }


    u16 min_x = MIN3(vtxpos[0].x, vtxpos[1].x, vtxpos[2].x);
    u16 min_y = MIN3(vtxpos[0].y, vtxpos[1].y, vtxpos[2].y);
    u16 max_x = MAX3(vtxpos[0].x, vtxpos[1].x, vtxpos[2].x);
    u16 max_y = MAX3(vtxpos[0].y, vtxpos[1].y, vtxpos[2].y);
    min_x &= IntMask();
    min_y &= IntMask();
    max_x = ((max_x + FracMask()) & IntMask());
    max_y = ((max_y + FracMask()) & IntMask());
    // Triangle filling rules: Pixels on the right-sided edge or on flat bottom edges are not
    // drawn. Pixels on any other triangle border are drawn. This is implemented with three bias
    // values which are added to the barycentric coordinates w0, w1 and w2, respectively.
    // NOTE: These are the PSP filling rules. Not sure if the 3DS uses the same ones...

    int bias0 = IsRightSideOrFlatBottomEdge(&vtxpos[0], &vtxpos[1], &vtxpos[2]) ? -1 : 0;
    int bias1 = IsRightSideOrFlatBottomEdge(&vtxpos[1], &vtxpos[2], &vtxpos[0]) ? -1 : 0;
    int bias2 = IsRightSideOrFlatBottomEdge(&vtxpos[2], &vtxpos[0], &vtxpos[1]) ? -1 : 0;
    // TODO: Not sure if looping through x first might be faster
    for (u16 y = min_y + 8; y < max_y; y += 0x10) {
        for (u16 x = min_x + 8; x < max_x; x += 0x10) {

			int w0 = bias0 + orient2d(vtxpos[1].x, vtxpos[1].y, vtxpos[2].x, vtxpos[2].y, x, y );
            int w1 = bias1 + orient2d(vtxpos[2].x, vtxpos[2].y, vtxpos[0].x, vtxpos[0].y, x, y );
            int w2 = bias2 + orient2d(vtxpos[0].x, vtxpos[0].y, vtxpos[1].x, vtxpos[1].y, x, y );
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
                        texture_data = (u8*)(gpu_GetPhysicalMemoryBuff(GPU_Regs[TEXTURE0_ADDR1] << 3));
                        break;
                    case 1:
						texture_data = (u8*)(gpu_GetPhysicalMemoryBuff(GPU_Regs[TEXTURE1_ADDR] << 3));
                        break;
                    case 2:
						texture_data = (u8*)(gpu_GetPhysicalMemoryBuff(GPU_Regs[TEXTURE2_ADDR] << 3));
                        break;
                    }

                    int s=0;
                    int t=0;
                    int row_stride=0;
                    int format=0;
                    int height=0;
                    int width=0;
                    int wrap_t=0;
                    int wrap_s=0;
                    switch (i) {
                    case 0:
                        height = (GPU_Regs[TEXTURE0_SIZE] & 0xFFFF);
                        width =  (GPU_Regs[TEXTURE0_SIZE] >> 16) & 0xFFFF;
                        wrap_t = (GPU_Regs[TEXTURE0_WRAP_FILTER] >> 8) & 3;
                        wrap_s = (GPU_Regs[TEXTURE0_WRAP_FILTER] >> 12) & 3;
                        format =  GPU_Regs[TEXTURE0_TYPE] & 0xF;
                        break;
                    case 1:
                        height = (GPU_Regs[TEXTURE1_SIZE] & 0xFFFF);
                        width =  (GPU_Regs[TEXTURE1_SIZE] >> 16) & 0xFFFF;
                        wrap_t = (GPU_Regs[TEXTURE1_WRAP_FILTER] >> 8) & 3;
                        wrap_s = (GPU_Regs[TEXTURE1_WRAP_FILTER] >> 12) & 3;
                        format =  GPU_Regs[TEXTURE1_TYPE] & 0xF;
                        break;
                    case 2:
                        height = (GPU_Regs[TEXTURE2_SIZE] & 0xFFFF);
                        width  = (GPU_Regs[TEXTURE2_SIZE] >> 16) & 0xFFFF;
                        wrap_t = (GPU_Regs[TEXTURE2_WRAP_FILTER] >> 8) & 3;
                        wrap_s = (GPU_Regs[TEXTURE2_WRAP_FILTER] >> 12) & 3;
                        format =  GPU_Regs[TEXTURE2_TYPE] & 0xF;
                        break;
                    }
                    
                    s = (int)(uv[i].x * (width *1.0f));
                    s = GetWrappedTexCoord((WrapMode)wrap_s, s, width);
                    t = (int)(uv[i].y * (height*1.0f));
                    t = GetWrappedTexCoord((WrapMode)wrap_t, t, height);
                    row_stride = NibblesPerPixel(format) * width / 2;

                    //Flip it vertically
                    //Don't flip for ETC1/ETC1A4
                    if(format < ETC1)
                        t = height - 1 - t;
                    //TODO: Fix this up so it works correctly.

                    /*int texel_index_within_tile = 0;
                    for (int block_size_index = 0; block_size_index < 3; ++block_size_index) {
                        int sub_tile_width = 1 << block_size_index;
                        int sub_tile_height = 1 << block_size_index;
                        int sub_tile_index = (s & sub_tile_width) << block_size_index;
                        sub_tile_index += 2 * ((t & sub_tile_height) << block_size_index);
                        texel_index_within_tile += sub_tile_index;
                    }
                    const int block_width = 8;
                    const int block_height = 8;
                    int coarse_s = (s / block_width) * block_width;
                    int coarse_t = (t / block_height) * block_height;

                    u8* source_ptr = (u8*)texture_data + coarse_s * block_height * 3 + coarse_t * row_stride + texel_index_within_tile * 3;
                    texture_color[i].v[0] = source_ptr[2];
                    texture_color[i].v[1] = source_ptr[1];
                    texture_color[i].v[2] = source_ptr[0];
                    texture_color[i].v[3] = 0xFF;*/

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

                // I doubt the combiner_buffer code below works correctly
                if (i > 0 && i < 5) {
                    if (((GPU_Regs[TEX_ENV_BUF_INPUT] >> (i + 7)) & 1) == 1) {
                        state.combiner_buffer.v[0] = state.combiner_output.v[0];
                        state.combiner_buffer.v[1] = state.combiner_output.v[1];
                        state.combiner_buffer.v[2] = state.combiner_output.v[2];
                    }
                    else
                    {
                        state.combiner_buffer.v[0] = state.combiner_buffer.v[0];
                        state.combiner_buffer.v[1] = state.combiner_buffer.v[1];
                        state.combiner_buffer.v[2] = state.combiner_buffer.v[2];
                    }
                    if (((GPU_Regs[TEX_ENV_BUF_INPUT] >> (i + 11)) & 1) == 1) {
                        state.combiner_buffer.v[3] = state.combiner_output.v[3];
                    }
                    else
                    {
                        state.combiner_buffer.v[3] = state.combiner_buffer.v[3];
                    }
                }

                state.combiner_constant.v[0] =  GPU_Regs[reg_num_addr + 3] & 0xFF;
                state.combiner_constant.v[1] = (GPU_Regs[reg_num_addr + 3] >> 8) & 0xFF;
                state.combiner_constant.v[2] = (GPU_Regs[reg_num_addr + 3] >> 16) & 0xFF;
                state.combiner_constant.v[3] = (GPU_Regs[reg_num_addr + 3] >> 24) & 0xFF;

                u8 color_multiplier = GetColorMultiplier(GPU_Regs[reg_num_addr + 4] & 3);
                u8 alpha_multiplier = GetAlphaMultiplier((GPU_Regs[reg_num_addr + 4] >> 16) & 3);

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
                   GetAlphaModifier((GPU_Regs[reg_num_addr + 1] >> 12) & 7, GetSource((GPU_Regs[reg_num_addr] >> 16) & 0xF)),
                   GetAlphaModifier((GPU_Regs[reg_num_addr + 1] >> 16) & 7, GetSource((GPU_Regs[reg_num_addr] >> 20) & 0xF)),
                   GetAlphaModifier((GPU_Regs[reg_num_addr + 1] >> 20) & 7, GetSource((GPU_Regs[reg_num_addr] >> 24) & 0xF)),
                };

                u8 alpha_output = AlphaCombine((GPU_Regs[reg_num_addr + 2] >> 16) & 0xF, alpha_result);

                state.combiner_output.v[0] = MIN((unsigned)255, color_output.v[0] * color_multiplier);
                state.combiner_output.v[1] = MIN((unsigned)255, color_output.v[1] * color_multiplier);
                state.combiner_output.v[2] = MIN((unsigned)255, color_output.v[2] * color_multiplier);
                state.combiner_output.v[3] = MIN((unsigned)255, alpha_output * alpha_multiplier);

                //if (regs.tev_combiner_buffer_input.TevStageUpdatesCombinerBufferColor(tev_stage_index)) {
                //    combiner_buffer.v[0] = combiner_output.v[0];
                //    combiner_buffer.v[1] = combiner_output.v[1];
                //    combiner_buffer.v[2] = combiner_output.v[2];
                //}

                //if (regs.tev_combiner_buffer_input.TevStageUpdatesCombinerBufferAlpha(tev_stage_index)) {
                //    combiner_buffer.v[3] = combiner_output.v[3];
                //}
            }

			if (GPU_Regs[ALPHA_TEST] & 1)
            {
                bool pass = false;

                switch (((GPU_Regs[ALPHA_TEST] >> 4) & 7)) {
                case 0u: //Never
                    pass = false;
                    break;

                case 1u: //Always
                    pass = true;
                    break;

                case 2u: //Equal
                    pass = state.combiner_output.v[3] == ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 3u: //NotEqual
                    pass = state.combiner_output.v[3] != ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 4u: //LessThan
                    pass = state.combiner_output.v[3] < ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 5u: //LessThanOrEqual
                    pass = state.combiner_output.v[3] <= ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 6u: //GreaterThan
                    pass = state.combiner_output.v[3] > ((GPU_Regs[ALPHA_TEST] >> 8) & 0xFF);
                    break;

                case 7u: //GreaterThanOrEqual
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
                u16 z = (u16)(((float)v0->screenpos.z * w0 +
                               (float)v1->screenpos.z * w1 +
                               (float)v2->screenpos.z * w2) * 65535.f / wsum); // TODO: Shouldn't need to multiply by 65536?

                u16 ref_z = GetDepth(x >> 4, y >> 4);

                bool pass = false;

                switch ((GPU_Regs[DEPTH_COLOR_MASK] >> 4) & 7) {
                case 0u: //Never
                    pass = false;
                    break;

                case 1u: //Always
                    pass = true;
                    break;

                case 2u: //Equal
                    pass = z == ref_z;
                    break;

                case 3u: //NotEqual
                    pass = z != ref_z;
                    break;

                case 4u: //LessThan
                    pass = z < ref_z;
                    break;

                case 5u: //LessThanOrEqual
                    pass = z <= ref_z;
                    break;

                case 6u: //GreaterThan
                    pass = z > ref_z;
                    break;

                case 7u: //GreaterThanOrEqual
                    pass = z >= ref_z;
                    break;

                    default:
                        DEBUG("Unknown depth test function %x\n", (GPU_Regs[DEPTH_COLOR_MASK] >> 4) & 7);
                        break;
                }

                if(!pass)
                    continue;

                if ((GPU_Regs[DEPTH_COLOR_MASK] >> 12) & 1)
                     SetDepth(x >> 4, y >> 4, z);
            }

            //Alpha blending
			clov4 srcfactor, dstfactor;
			state.dest = GetPixel(x >> 4, y >> 4);
			clov4 blend_output = state.combiner_output;

            //Alpha blending
			if ((GPU_Regs[COLOR_OP] >> 8) & 1) //Alpha blending enabled
            {
                u32 factor_source_rgb = (GPU_Regs[BLEND_FUNC] >> 16) & 0xF;
                u32 factor_source_a   = (GPU_Regs[BLEND_FUNC] >> 24) & 0xF;
                srcfactor = make_vec4_u8_from_vec3_u8(LookupFactorRGB(factor_source_rgb), LookupFactorA(factor_source_a));

                u32 factor_dest_rgb = (GPU_Regs[BLEND_FUNC] >> 20) & 0xF;
                u32 factor_dest_a   = (GPU_Regs[BLEND_FUNC] >> 28) & 0xF;
                dstfactor = make_vec4_u8_from_vec3_u8(LookupFactorRGB(factor_dest_rgb), LookupFactorA(factor_dest_a));

                u32 blend_equation_rgb =  GPU_Regs[BLEND_FUNC] & 0xFF;
                u32 blend_equation_a   = (GPU_Regs[BLEND_FUNC] >> 8) & 0xFF;

                blend_output = EvaluateBlendEquation(state.combiner_output, srcfactor, state.dest, dstfactor, blend_equation_rgb);
                blend_output.v[3] = EvaluateBlendEquation(state.combiner_output, srcfactor, state.dest, dstfactor, blend_equation_a).v[3];
            }
            else
            {
                blend_output.v[0] = LogicOp(state.combiner_output.v[0], state.dest.v[0], GPU_Regs[LOGIC_OP] & 0xF);
                blend_output.v[1] = LogicOp(state.combiner_output.v[1], state.dest.v[1], GPU_Regs[LOGIC_OP] & 0xF);
                blend_output.v[2] = LogicOp(state.combiner_output.v[2], state.dest.v[2], GPU_Regs[LOGIC_OP] & 0xF);
                blend_output.v[3] = LogicOp(state.combiner_output.v[3], state.dest.v[3], GPU_Regs[LOGIC_OP] & 0xF);
            }

            bool red_enable   = ((GPU_Regs[DEPTH_COLOR_MASK] >> 8) & 1);
            bool green_enable = ((GPU_Regs[DEPTH_COLOR_MASK] >> 9) & 1);
            bool blue_enable  = ((GPU_Regs[DEPTH_COLOR_MASK] >> 10) & 1);
            bool alpha_enable = ((GPU_Regs[DEPTH_COLOR_MASK] >> 11) & 1);

            clov4 result = {
                red_enable   ? blend_output.v[0] : state.dest.v[0],
                green_enable ? blend_output.v[1] : state.dest.v[1],
                blue_enable  ? blend_output.v[2] : state.dest.v[2],
                alpha_enable ? blend_output.v[3] : state.dest.v[3],
            };

            DrawPixel((x >> 4), (y >> 4), &result);
        }
    }
}
