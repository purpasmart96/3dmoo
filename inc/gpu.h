/*
* Copyright (C) 2014 - plutoo
* Copyright (C) 2014 - ichfly
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

#ifndef _GPU_H_
#define _GPU_H_

#include <stddef.h>
#include "vec4.h"

#define GPU_REG_INDEX(field_name) (offsetof(Regs, field_name) / sizeof(u32))

/* Commands for gsp shared-mem. */
#define GSP_ID_REQUEST_DMA          0
#define GSP_ID_SET_CMDLIST          1
#define GSP_ID_SET_MEMFILL          2
#define GSP_ID_SET_DISPLAY_TRANSFER 3
#define GSP_ID_SET_TEXTURE_COPY     4
#define GSP_ID_FLUSH_CMDLIST        5

/* Interrupt ids. */
#define GSP_INT_PSC0 0 // Memory fill A
#define GSP_INT_PSC1 1 // Memory fill B
#define GSP_INT_VBLANK_TOP 2
#define GSP_INT_VBLANK_BOT 3
#define GSP_INT_PPF 4
#define GSP_INT_P3D 5 // Triggered when GPU-cmds have finished executing?
#define GSP_INT_DMA 6 // Triggered when DMA has finished.



#define LCDCOLORFILLMAIN 0x202204
#define LCDCOLORFILLSUB 0x202A04

#define framestridetop 0x400490
#define frameformattop 0x400470
#define frameselecttop 0x400478
#define framebuffer_top_size 0x40045C
#define RGBuponeleft 0x400468
#define RGBuptwoleft 0x40046C
#define RGBuponeright 0x400494
#define RGBuptworight 0x400498

#define framestridebot 0x400590
#define frameformatbot 0x400570
#define frameselectbot 0x400578
#define RGBdownoneleft 0x400568
#define RGBdowntwoleft 0x40056C


u8* LINEAR_MemoryBuff;
u8* VRAM_MemoryBuff;
u8* GSP_SharedBuff;
extern u32 GPU_Regs[0xFFFF];

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define STACK_MAX 64

#define Format_BYTE  0
#define Format_UBYTE 1
#define Format_SHORT 2
#define Format_FLOAT 3

#define VS_INVALID_ADDRESS 0xFFFFFFFF

#define GSP_SHARED_BUFF_SIZE 0x1000 //dumped from GSP module in Firm 4.4

#define TRIGGER_IRQ 0x10

#define CULL_MODE            0x40
#define VIEWPORT_WIDTH       0x41
#define VIEWPORT_WIDTH2      0x42
#define VIEWPORT_HEIGHT      0x43
#define VIEWPORT_HEIGHT2     0x44

#define DEPTH_SCALE          0x4D
#define DEPTH_OFFSET         0x4E

#define VS_ATTRIBUTE_OUTPUT_MAP 0x50
// untill 0x56

#define SCISSOR_TEST_MODE    0x65
#define SCISSOR_TEST_POS     0x66
#define SCISSOR_TEST_DIM     0x67
#define GL_VIEWPORT          0x68

#define TEXTURE_UNITS_CONFIG  0x80
#define TEXTURE0_BORDER_COLOR 0x81
#define TEXTURE0_SIZE         0x82
#define TEXTURE0_WRAP_FILTER  0x83
#define TEXTURE0_ADDR1        0x85
#define TEXTURE0_ADDR2        0x85
#define TEXTURE0_ADDR3        0x85
#define TEXTURE0_ADDR4        0x88
#define TEXTURE0_ADDR5        0x89
#define TEXTURE0_ADDR6        0x8A
#define TEXTURE0_SHADOW       0x85

#define TEXTURE_CONFIG0_ADDR 0x85
#define TEXTURE_CONFIG0_TYPE 0x8E

#define TEXTURE_CONFIG1_SIZE 0x92
#define TEXTURE_CONFIG1_WRAP 0x93
#define TEXTURE_CONFIG1_ADDR 0x95
#define TEXTURE_CONFIG1_TYPE 0x96

#define TEXTURE_CONFIG2_SIZE 0x9A
#define TEXTURE_CONFIG2_WRAP 0x9B
#define TEXTURE_CONFIG2_ADDR 0x9D
#define TEXTURE_CONFIG2_TYPE 0x9E

#define GL_TEX_ENV             0xC0

#define FOG_COLOR              0xE0
#define FOG_LUT_DATA           0xE8

#define COLOR_OP               0x100
#define BLEND_FUNC             0x101
#define LOGIC_OP               0x102
#define BLEND_COLOR            0x103
#define ALPHA_TEST             0x104
#define STENCIL_TEST           0x105
#define STENCIL_OP             0x106
#define DEPTH_COLOR_MASK       0x107
#define COLOR_BUFFER_READ      0x112
#define COLOR_BUFFER_WRITE     0x113
#define DEPTH_STENCIL_READ     0x114
#define DEPTH_STENCIL_WRITE    0x115
#define DEPTH_BUFFER_FORMAT    0x116
#define COLOR_BUFFER_FORMAT    0x117

#define DEPTH_BUFFER_ADDRESS   0x11C
#define COLOR_BUFFER_ADDRESS   0x11D

#define FRAME_BUFFER_DIM       0x11E

#define ATTRIBUTE_CONFIG       0x200
// untill 0x226
#define INDEX_ARRAY_CONFIG     0x227
#define NUMBER_OF_VERTICES     0x228
#define GEOSTAGE_CONFIG        0x229
#define TRIGGER_DRAW           0x22E
#define TRIGGER_DRAW_INDEXED   0x22F

#define TRIANGLE_TOPOLOGY      0x25E

#define VS_REST_TRIANGEL       0x25F

#define GS_BOOL_UNIFORM        0x280
#define GS_INT0_UNIFORM        0x281
#define GS_INT1_UNIFORM        0x282
#define GS_INT2_UNIFORM        0x283
#define GS_INT3_UNIFORM        0x284

#define GS_INPUT_BUFFER_CONFIG 0x2B9
#define GS_MAIN_OFFSET         0x2BA

#define GS_PROGRAM_ADDR        0x29B
#define GS_CODETRANSFER_DATA   0x29C


#define VS_BOOL_UNIFORM         0x2B0
#define VS_INT0_UNIFORM         0x2B1
#define VS_INT1_UNIFORM         0x2B2
#define VS_INT2_UNIFORM         0x2B3
#define VS_INT3_UNIFORM         0x2B4

#define VS_INPUT_BUFFER_CONFIG  0x2B9
#define VS_START_ADDR           0x2BA
#define VS_INPUT_REGISTER_MAP   0x2BB
// untill 0x2BC
#define VS_FLOAT_UNIFORM_SETUP  0x2C0
// untill 0x2C8
#define VS_PROGRAM_ADDR         0x2CB
#define VS_PROGRAM_DATA1        0x2CC
#define VS_PROGRAM_DATA2        0x2CD
#define VS_PROGRAM_DATA3        0x2CE
#define VS_PROGRAM_DATA4        0x2CF
#define VS_PROGRAM_DATA5        0x2D0
#define VS_PROGRAM_DATA6        0x2D1
#define VS_PROGRAM_DATA7        0x2D2
#define VS_PROGRAM_DATA8        0x2D3

#define VS_PROGRAM_SWIZZLE_ADDR 0x2D5
#define VS_PROGRAM_SWIZZLE_DATA 0x2D6
// untill 0x2DD

#define SHDR_ADD     0x0
#define SHDR_DP3     0x1
#define SHDR_DP4     0x2
#define SHDR_DPH     0x3
#define SHDR_DST     0x4
#define SHDR_EX2     0x5
#define SHDR_LG2     0x6
#define SHDR_LITP    0x7
#define SHDR_MUL     0x8
#define SHDR_SGE     0x9
#define SHDR_SLT     0xA
#define SHDR_FLR     0xB
#define SHDR_MAX     0xC
#define SHDR_MIN     0xD
#define SHDR_RCP     0xE
#define SHDR_RSQ     0xF

#define SHDR_MOVA    0x12
#define SHDR_MOV     0x13

#define SHDR_NOP     0x21
#define SHDR_END     0x22
#define SHDR_BREAKC  0x23
#define SHDR_CALL    0x24
#define SHDR_CALLC   0x25
#define SHDR_CALLB   0x26
#define SHDR_IFB     0x27
#define SHDR_IFC     0x28
#define SHDR_LOOP    0x29
#define SHDR_JPC     0x2C
#define SHDR_JPB     0x2D
#define SHDR_CMP     0x2E
#define SHDR_CMP2    0x2F
#define SHDR_MAD1    0x38
#define SHDR_MAD2    0x39
#define SHDR_MAD3    0x3A
#define SHDR_MAD4    0x3B
#define SHDR_MAD5    0x3C
#define SHDR_MAD6    0x3D
#define SHDR_MAD7    0x3E
#define SHDR_MAD8    0x3F

typedef struct {

    // VS output attributes

    vec4 position;
    vec4 dummy; // quaternions (not implemented, yet)
    vec4 color;
    vec2 texcoord0;
    vec2 texcoord1;
    float texcoord0_w;
    vec3 View;
    vec2 texcoord2;

    // Padding for optimal alignment
    float pad[10];

    // Attributes used to store intermediate results

    // position after perspective divide
    vec3 screenpos;
} OutputVertex;


typedef enum
{
	NoScale = 0,  // Doesn't scale the image
	ScaleX  = 1,  // Downscales the image in half in the X axis and applies a box filter
	ScaleXY = 2,  // Downscales the image in half in both the X and Y axes and applies a box filter
} ScalingMode;

typedef enum
{
    pixel_RGBA8 = 0,
    pixel_RGB8 = 1,
    pixel_RGB565 = 2,
    pixel_RGB5A1 = 3,
    pixel_RGBA4 = 4,
} PixelFormat;


typedef enum {
	D16   = 0,
	D24   = 2,
	D24S8 = 3,
} DepthFormat;


typedef enum
{
    REQUEST_DMA            = 0x00,
    SET_COMMAND_LIST_LAST  = 0x01,

    // Fills a given memory range with a particular value
    SET_MEMORY_FILL        = 0x02,

    // Copies an image and optionally performs color-conversion or scaling.
    // This is highly similar to the GameCube's EFB copy feature
    SET_DISPLAY_TRANSFER   = 0x03,

    // Conceptionally similar to SET_DISPLAY_TRANSFER and presumable uses the same hardware path
    SET_TEXTURE_COPY       = 0x04,

    SET_COMMAND_LIST_FIRST = 0x05,
} CommandId;


typedef struct {
    CommandId id : 8;

    union {
        struct 
        {
            u32 source_address;
            u32 dest_address;
            u32 size;
        } dma_request;

        struct
        {
            u32 address;
            u32 size;
        } set_command_list_last;

        struct
        {
            u32 start1;
            u32 value1;
            u32 end1;

            u32 start2;
            u32 value2;
            u32 end2;

            u16 control1;
            u16 control2;
        } memory_fill;

        struct 
        {
            u32 in_buffer_address;
            u32 out_buffer_address;
            u32 in_buffer_size;
            u32 out_buffer_size;
            u32 flags;
        } image_copy;

        u8 raw_data[0x1C];
    };
} Command;


typedef struct {

	u32 pad4[0x4];

union {
    u32 address_start;
    u32 address_end;

    u32 value_32bit;

    struct
    {
        u32 value_16bit : 16;
    };

    struct
    {
        u32 value_24bit_r : 8;
        u32 value_24bit_g : 8;
        u32 value_24bit_b : 8;
    };

    u32 control;

    struct
    {
		u32 trigger    : 1;
		u32 finished   : 1;
		u32 padding    : 6;
		u32 fill_24bit : 1;
		u32 fill_32bit : 1;
	};

} memory_fill_config[2];

u32 pad10b[0x10b];


union FramebufferConfig {

	u32 size;
    struct
    {
        u32 width : 16;
        u32 height : 16;
	};

    u32 pad1; // (0x2);
    u32 pad2;

    u32 address_left1;
    u32 address_left2;

    u32 format;

	struct
    {
        u32 color_format : 3;
    };

	u32 pad3;

    u32 active_fb;

    struct
    {
        // 0: Use parameters ending with "1"
        // 1: Use parameters ending with "2"
        u32 second_fb_active : 1;
    };

    u32 pad4; // (0x5);
    u32 pad5;
    u32 pad6;
    u32 pad7;
    u32 pad8;

    // Distance between two pixel rows, in bytes
    u32 stride;

    u32 address_right1;
    u32 address_right2;

    u32 pad_fill[0x30];
} framebuffer_config[2];

u32 pad169[0x169];

union {
	u32 input_address;
	u32 output_address;

    u32 output_size;

    struct
    {
		u32 output_width : 16;
		u32 output_height : 16;
	};

    u32 input_size;

    struct
    {
        u32 input_width : 16;
        u32 input_height : 16;
    };

    u32 flags;

    struct
    {
        u32 flip_vertically : 1;     // flips input data vertically
        u32 output_tiled    : 1;     // Converts from linear to tiled format
        u32 raw_copy        : 1;     // Copies the data without performing any processing
        u32                 : 1;
        u32 dont_swizzle    : 1;
		u32                 : 2;
        PixelFormat input_format  : 3;
        PixelFormat output_format : 3;

        u32                 : 9;

		ScalingMode scaling : 2; // Determines the scaling mode of the transfer
	};

	u32 pad4;

	// it seems that writing to this field triggers the display transfer
	u32 trigger;
} display_transfer_config;

u32 pad331[0x331];

struct {
	// command list size (in bytes)
	u32 size;

	u32 pad1;

	// command list address
	u32 address;

	u32 pad2;

	// it seems that writing to this field triggers command list processing
	u32 trigger;
} command_processor_config;

u32 pad9c3[0x9c3];

} Regs;



union {
        u32 pad1[0x6];

        u32 depth_format; // TODO: Should be a BitField!
        u32                      : 16;
        u32 color_format : 3;

        u32 pad2[0x4];

        u32 depth_buffer_address;
        u32 color_buffer_address;

        struct
        {
            // Apparently, the framebuffer width is stored as expected,
            // while the height is stored as the actual height minus one.
            // Hence, don't access these fields directly but use the accessors
            // GetWidth() and GetHeight() instead.
            u32 width  : 11;
            u32 height : 10;
        };

        u32 pad3[0x1];

} framebuffer;



typedef struct {
    u8 v[4];
} clov4;


typedef struct {
	u8 _[3];
} vec3_u8;

typedef struct {
	u8 _[4];
} vec4_u8;



/*
void vec3_to_vec4(vec3 in, vec4 out)
{
	out.v[0] = in.x;
	out.v[1] = in.y;
	out.v[2] = in.z;
	out.v[3] = 0;
}


void vec4_to_vec3(vec4 in, vec3 out)
{
	out.x = in.v[0];
	out.y = in.v[1];
	out.z = in.v[2];
	//out.w = 0;
}

void clov4_to_clov3(clov4 in, clov3 out)
{
	out.v[0] = in.v[0];
	out.v[1] = in.v[1];
	out.v[2] = in.v[2];
}
*/

typedef struct {
	int v[4];
} vec4_Int;

typedef struct {
    u8 v[3];
} clov3;


clov4 make_vec4_from_vec3(const clov3 xyz, u8 w);

typedef struct {
	u32 v[2];
} vec2_u32;


typedef struct {
	u32 active_fb : 1; // 0 = first, 1 = second
	u32 address_left;
	u32 address_right;
	u32 stride;    // maps to 0x1EF00X90 ?
	u32 format;    // maps to 0x1EF00X70 ?
	u32 shown_fb;  // maps to 0x1EF00X78 ?
	u32 unknown;
} FrameBufferInfo;


void SetBufferSwap(u32 screen_id, FrameBufferInfo info);

void gpu_Init();
void gpu_WriteReg32(u32 addr, u32 data);
u32  gpu_ReadReg32(u32 addr);
void gpu_TriggerCmdReqQueue();
u32  gpu_RegisterInterruptRelayQueue(u32 Flags, u32 Kevent, u32*threadID, u32*outMemHandle);
u8*  gpu_GetPhysicalMemoryBuff(u32 addr);
u32  gpu_GetPhysicalMemoryRestSize(u32 addr);
void gpu_SendInterruptToAll(u32 ID);
void gpu_ExecuteCommands(u8* buffer, u32 size);
u32  gpu_ConvertVirtualToPhysical(u32 addr);
u32  gpu_ConvertPhysicalToVirtual(u32 addr);
void gpu_UpdateFramebuffer();
void gpu_UpdateFramebufferAddr(u32 addr, bool bottom);
void gpu_WriteID(u16 ID, u8 mask, u32 size, u32* buffer);
u32  gpu_BytesPerPixel(u32 color_type);
u32  gpu_BytesPerColorPixel(u32 format);
u32  gpu_BytesPerDepthPixel(u32 format);
u32  gpu_DepthBitsPerPixel(u32 format);
u32  gpu_DecodeAddressRegister(u32 register_value);
u32  gpu_ColorBufferPhysicalAddress();
u32  gpu_DepthBufferPhysicalAddress();
u32  gpu_GetWidth();
u32  gpu_GetHeight();
u32  gpu_GetStartAddress();
u32  gpu_GetEndAddress();

//clipper.c
void Clipper_ProcessTriangle(OutputVertex *v0, OutputVertex *v1, OutputVertex *v2);

//rasterizer.c
void rasterizer_ProcessTriangle(OutputVertex *v0,
                                OutputVertex *v1,
                                OutputVertex *v2);

#endif
