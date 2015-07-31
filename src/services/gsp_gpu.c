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

#include "util.h"
#include "arm11.h"
#include "handles.h"
#include "mem.h"
#include "gpu.h"

#include "screen.h"
#include "color.h"
#include "service_macros.h"


u32 numReqQueue = 1;
u32 trigevent = 0;

Regs g_regs;
Command cmd;

//#define DUMP_CMDLIST

static clov4 DecodePixel(u8 input_format, const u8* src_pixel)
{
	switch (input_format) {
	case 0: //RGBA8:
		return DecodeRGBA8(src_pixel);

	case 1: //RGB8:
		return DecodeRGB8(src_pixel);

	case 2: //RGB565:
		return DecodeRGB565(src_pixel);

	case 3: //RGB5A1:
		return DecodeRGB5A1(src_pixel);

	case 4: //RGBA4:
		return DecodeRGBA4(src_pixel);

	default:
		GPUDEBUG("Unknown source framebuffer format %x", input_format);
		return DecodeRGBA8(src_pixel);
	}
}

void gsp_ExecuteCommandFromSharedMem()
{
    // For all threads
    for (int i = 0; i < 0x4; i++) {
        u8* base_addr = (u8*)(GSP_SharedBuff + 0x800 + i * 0x200);
        u32 header = *(u32*)base_addr;
        u32 toprocess = (header >> 8) & 0xFF;

        //mem_Dbugdump();

        *(u32*)base_addr = 0;
        for (u32 j = 0; j < toprocess; j++) {

            u32 cmd_id = *(u32*)(base_addr + (j + 1) * 0x20);

            switch (cmd_id & 0xFF) {
            case GSP_ID_REQUEST_DMA: { /* GX::RequestDma */
                u32 src = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                u32 dest = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                u32 size = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);

                GPUDEBUG("GX RequestDma 0x%08x 0x%08x 0x%08x\n", src, dest, size);

                if (dest - 0x1f000000 > 0x600000 || dest + size - 0x1f000000 > 0x600000) {
                    GPUDEBUG("dma copy into non VRAM not suported\n");
                    continue;
                }

                if((src - 0x1f000000 > 0x600000 || src + size - 0x1f000000 > 0x600000))
                {
                    mem_Read(&VRAM_MemoryBuff[dest - 0x1F000000], src, size);
                }
                else
                {
                    //Can safely assume this is a copy from VRAM to VRAM
                    memcpy(&VRAM_MemoryBuff[dest - 0x1F000000], &VRAM_MemoryBuff[src - 0x1F000000], size);
                }

                gpu_SendInterruptToAll(6);
                break;
            }

            case GSP_ID_SET_CMDLIST: { /* GX::SetCmdList Last */
                u32 address = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                u32 size = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                u32 trigger = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);

                GPUDEBUG("GX SetCommandList Last 0x%08x 0x%08x 0x%08x\n", address, size, trigger);


#ifdef DUMP_CMDLIST
                char name[0x100];
                static u32 cmdlist_ctr;

                sprintf(name, "Cmdlist%08x.dat", cmdlist_ctr++);
                FILE* out = fopen(name, "wb");
#endif

                u8* buffer = malloc(size);
                mem_Read(buffer, address, size);
                gpu_ExecuteCommands(buffer, size);

#ifdef DUMP_CMDLIST
                fwrite(buffer, size, 1, out);
                fclose(out);
#endif
                free(buffer);
                break;
            }

            case GSP_ID_SET_MEMFILL: { //speedup todo

				u32 start1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
				u32 value1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
				u32 end1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);
				u32 start2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x10);
				u32 value2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x14);
				u32 end2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x18);
				u16 control1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x1C);
				u16 control2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x1E);

				GPUDEBUG("GX SetMemoryFill 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n", start1, value1, end1, start2, value2, end2, control1, control2);
/*
   
                g_regs.memory_fill_config[0].address_start = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(start1) >> 3);
                g_regs.memory_fill_config[0].address_end = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(end1) >> 3);
                g_regs.memory_fill_config[0].value_32bit = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(value1) >> 3);
                g_regs.memory_fill_config[0].control = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(control1) >> 3);

                g_regs.memory_fill_config[0].address_start = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(start1) >> 3);
                g_regs.memory_fill_config[0].address_end = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(end1) >> 3);
                g_regs.memory_fill_config[0].value_32bit = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(value1) >> 3);
                g_regs.memory_fill_config[0].control = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(control1) >> 3);
                //gpu_WriteReg32((g_regs.memory_fill_config[0].address_start), gpu_ConvertVirtualToPhysical(start1) >> 3);
	            //gpu_WriteReg32((g_regs.memory_fill_config[0].address_end), gpu_ConvertVirtualToPhysical(end1) >> 3);
	            //gpu_WriteReg32((g_regs.memory_fill_config[0].value_32bit), gpu_ConvertVirtualToPhysical(value1) >> 3);
	            //gpu_WriteReg32((g_regs.memory_fill_config[0].control), gpu_ConvertVirtualToPhysical(control1) >> 3);

	            //gpu_WriteReg32((u32)(g_regs.memory_fill_config[1].address_start), gpu_ConvertVirtualToPhysical(start2) >> 3);
	            //gpu_WriteReg32((u32)(g_regs.memory_fill_config[1].address_end), gpu_ConvertVirtualToPhysical(end2) >> 3);
	            //gpu_WriteReg32((u32)(g_regs.memory_fill_config[1].value_32bit), gpu_ConvertVirtualToPhysical(value2) >> 3);
	            //gpu_WriteReg32((u32)(g_regs.memory_fill_config[1].control), gpu_ConvertVirtualToPhysical(control2) >> 3);
*/


                if (start1 - 0x1f000000 > 0x600000 || end1 - 0x1f000000 > 0x600000) {
                    GPUDEBUG("SetMemoryFill into non VRAM not suported\r\n");
                } else {
                    u32 size = gpu_BytesPerPixel(control1);

                    for(u32 k = start1; k < end1; k+=size) {
                        for(s32 m = size - 1; m >= 0; m--)
                        {
                            VRAM_MemoryBuff[m + (k - 0x1F000000)] = (u8)(value1 >> (m * 8));
                        }
                    }
                }
                if (start2 - 0x1f000000 > 0x600000 || end2 - 0x1f000000 > 0x600000) {
                    if (start2 && end2)
                        GPUDEBUG("SetMemoryFill into non VRAM not suported\r\n");
                } else {
                    u32 size = gpu_BytesPerPixel(control2);
                    for(u32 k = start2; k < end2; k += size) {
                        for (s32 m = size - 1; m >= 0; m--)
                            VRAM_MemoryBuff[m + (k - 0x1F000000)] = (u8)(value2 >> (m * 8));
                    }
                }
                gpu_SendInterruptToAll(0);
	            break;
            }

            case GSP_ID_SET_DISPLAY_TRANSFER:
			{

                    gpu_SendInterruptToAll(4);

                    u32 inpaddr = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                    u32 outputaddr = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
					u32 input_size = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);
                    u32 output_size = *(u32*)(base_addr + (j + 1) * 0x20 + 0x10);
                    u32 flags = *(u32*)(base_addr + (j + 1) * 0x20 + 0x14);
                    u32 unk = *(u32*)(base_addr + (j + 1) * 0x20 + 0x18);
					GPUDEBUG("GX SetDisplayTransfer 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n", inpaddr, outputaddr, input_size, output_size, flags, unk);

					cmd.image_copy.in_buffer_address = inpaddr;
					cmd.image_copy.out_buffer_address = outputaddr;
					cmd.image_copy.in_buffer_size = input_size;
					cmd.image_copy.out_buffer_size = output_size;
					cmd.image_copy.flags = flags;

					GPUDEBUG("Flags=%u\n", cmd.image_copy.flags);

                    u8* src_pointer = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(inpaddr));
                    u8* dst_pointer = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(outputaddr));

                    u32 config_output_width = (output_size & 0xFFFF);
                    u32 config_output_height = ((output_size >> 0x10) & 0xFFFF);

                    u32 config_input_width = (input_size & 0xFFFF);
                    u32 config_input_height = ((input_size >> 0x10) & 0xFFFF);


                    u32 config_flip_vertically = (flags & 1);
                    u32 config_output_tiled = (flags >> 1) & 1;
                    u32 config_raw_copy = (flags >> 3) & 1;
                    u32 config_input_format = (flags >> 0x8) & 5;
                    u32 config_output_format = (flags >> 0x12) & 5;

                    u32 config_scaling = (flags >> 24) & 3;

                    u32 config_NoScale = 0;
                    u32 config_ScaleX  = 1;
                    u32 config_ScaleXY = 2;

                    bool horizontal_scale = config_scaling != config_NoScale;
                    bool vertical_scale = config_scaling == config_ScaleXY;

                    u32 output_width = config_output_width >> horizontal_scale;
                    u32 output_height = config_output_height >> vertical_scale;

                    u32 input_size_ = config_input_width * config_input_height * gpu_BytesPerPixel(config_input_format);
                    u32 output_size_ = output_width * output_height * gpu_BytesPerPixel(config_output_format);

					GPUDEBUG("Flip vertically=%u\n", config_flip_vertically);
                    GPUDEBUG("Scaling mode=%u\n", config_scaling);
					GPUDEBUG("DisplayTriggerTransfer: 0x%08x bytes from 0x%08x(%ux%u)-> 0x%08x(%ux%u), dst format %x\n",
						config_output_height * output_width * gpu_BytesPerPixel(config_output_format),
						inpaddr, config_input_width, config_input_height,
						outputaddr, output_width, output_height,
						config_output_format);

                    if (config_raw_copy) {
                        // Raw copies do not perform color conversion nor tiled->linear / linear->tiled conversions
                        // TODO(Subv): Verify if raw copies perform scaling
                        GPUDEBUG("Raw copies called\n");
                        memcpy(dst_pointer, src_pointer, output_size_);

                        gpu_SendInterruptToAll(4);
                    }

                                            
                    for (u32 y = 0; y < output_height; ++y) {
                        for (u32 x = 0; x < output_width; ++x) {
                            clov4 src_color;

                             // Calculate the [x,y] position of the input image
                             // based on the current output position and the scale
                            u32 input_x = x << horizontal_scale;
                            u32 input_y = y << vertical_scale;


                        if (config_flip_vertically) {
                            // Flip the y value of the output data,
                            // we do this after calculating the [x,y] position of the input image
                            // to account for the scaling options.
                            GPUDEBUG("Flip vertically called\n");
                            y = output_height - y - 1;
                        }

                        u32 dst_bytes_per_pixel = gpu_BytesPerPixel(config_output_format);
                        u32 src_bytes_per_pixel = gpu_BytesPerPixel(config_input_format);
                        u32 src_offset;
                        u32 dst_offset;

                    if (config_output_tiled == 1) {
                        // Interpret the input as linear and the output as tiled
                        u32 coarse_y = y & ~7;
                        u32 stride = output_width * dst_bytes_per_pixel;

                        src_offset = (input_x + input_y * config_input_width) * src_bytes_per_pixel;
                        dst_offset = GetMortonOffset(x, y, dst_bytes_per_pixel) + coarse_y * stride;
                        GPUDEBUG("Output tiled called\n");

                    } else {
                        // Interpret the input as tiled and the output as linear
                        u32 coarse_y = input_y & ~7;
                        u32 stride = config_input_width * src_bytes_per_pixel;

                        src_offset = GetMortonOffset(input_x, input_y, src_bytes_per_pixel) + coarse_y * stride;
                        dst_offset = (x + y * output_width) * dst_bytes_per_pixel;
                    }

                    const u8* src_pixel = src_pointer + src_offset;
                    src_color = DecodePixel(config_input_format, src_pixel);
                    if (config_scaling == config_ScaleX) {
                        clov4 pixel = DecodePixel(config_input_format, src_pixel + src_bytes_per_pixel);
                        src_color.v[0] = ((src_color.v[0] + pixel.v[0]) / 2);
                        src_color.v[1] = ((src_color.v[1] + pixel.v[1]) / 2);
                        src_color.v[2] = ((src_color.v[2] + pixel.v[2]) / 2);
                        src_color.v[3] = ((src_color.v[3] + pixel.v[3]) / 2);
                    } else if (config_scaling == config_ScaleXY) {
                        clov4 pixel1 = DecodePixel(config_input_format, src_pixel + 1 * src_bytes_per_pixel);
                        clov4 pixel2 = DecodePixel(config_input_format, src_pixel + 2 * src_bytes_per_pixel);
                        clov4 pixel3 = DecodePixel(config_input_format, src_pixel + 3 * src_bytes_per_pixel);
                        src_color.v[0] = (((src_color.v[0] + pixel1.v[0]) + (pixel2.v[0] + pixel3.v[0])) / 4);
                        src_color.v[1] = (((src_color.v[1] + pixel1.v[1]) + (pixel2.v[1] + pixel3.v[1])) / 4);
						src_color.v[2] = (((src_color.v[2] + pixel1.v[2]) + (pixel2.v[2] + pixel3.v[2])) / 4);
						src_color.v[3] = (((src_color.v[3] + pixel1.v[3]) + (pixel2.v[3] + pixel3.v[3])) / 4);
                    }

                    u8* dst_pixel = dst_pointer + dst_offset;
                    switch (config_output_format) {
					case 0: //RGBA8
                        EncodeRGBA8(src_color, dst_pixel);
                        break;

					case 1: //RGB8
                        EncodeRGB8(src_color, dst_pixel);
                        break;

					case 2: //RGB565
                        EncodeRGB565(src_color, dst_pixel);
                        break;

					case 3: //RGB5A1
                        EncodeRGB5A1(src_color, dst_pixel);
                        break;

					case 4: //RGBA4
                        EncodeRGBA4(src_color, dst_pixel);
                        break;

                    default:
                        GPUDEBUG("Unknown destination framebuffer format %x", config_output_format);
                        break;
                    }

                    }
                }
					GPUDEBUG("DisplayTriggerTransfer: 0x%08x bytes from 0x%08x(%ux%u)-> 0x%08x(%ux%u), dst format %x\n",
						config_output_height * output_width * gpu_BytesPerPixel(config_output_format),
						inpaddr, config_input_width, config_input_height,
						outputaddr, output_width, output_height,
						config_output_format);
                    gpu_UpdateFramebuffer();
                    break;
                }

            case GSP_ID_SET_TEXTURE_COPY: {
                gpu_SendInterruptToAll(1);
                u32 inpaddr, outputaddr /*,size*/, output_size, input_size, flags;
                inpaddr = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                outputaddr = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                u32 size = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);
                output_size = *(u32*)(base_addr + (j + 1) * 0x20 + 0x10);
                input_size = *(u32*)(base_addr + (j + 1) * 0x20 + 0x14);
                flags = *(u32*)(base_addr + (j + 1) * 0x20 + 0x18);
                GPUDEBUG("GX SetTextureCopy 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X --todo--\r\n", inpaddr, outputaddr, size, output_size, input_size, flags);

                gpu_UpdateFramebuffer();
                //goto theother; //untill I know what is the differnece
                break;
            }
            case GSP_ID_FLUSH_CMDLIST: {
                u32 addr1, size1, addr2, size2, addr3, size3;
                addr1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                size1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                addr2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);
                size2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x10);
                addr3 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x14);
                size3 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x18);
                GPUDEBUG("GX SetCommandList First 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n", addr1, size1, addr2, size2, addr3, size3);
                break;
            }
            default:
                GPUDEBUG("GX cmd 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n",
                        *(u32*)(base_addr + (j + 1) * 0x20), *(u32*)((base_addr + (j + 1) * 0x20) + 0x4),
                        *(u32*)((base_addr + (j + 1) * 0x20) + 0x8), *(u32*)((base_addr + (j + 1) * 0x20) + 0xC), 
                        *(u32*)((base_addr + (j + 1) * 0x20) + 0x10), *(u32*)((base_addr + (j + 1) * 0x20) + 0x14),
                        *(u32*)((base_addr + (j + 1) * 0x20) + 0x18), *(u32*)((base_addr + (j + 1) * 0x20)) + 0x1C);
                break;
            }
        }
    }
}

u32 gpu_RegisterInterruptRelayQueue(u32 flags, u32 Kevent, u32*threadID, u32*outMemHandle)
{
    *threadID = numReqQueue++;
    *outMemHandle = handle_New(HANDLE_TYPE_SHAREDMEM, MEM_TYPE_GSP_0);
    trigevent = Kevent;
    handleinfo* h = handle_Get(Kevent);
    if (h == NULL) {
        GPUDEBUG("failed to get Event\n");
        PAUSE();
        return -1;// -1;
    }
    h->locked = false; //unlock we are fast

    *(u32*)(GSP_SharedBuff + *threadID * 0x40) = 0x0; //dump from save GSP v0 flags 0
    *(u32*)(GSP_SharedBuff + *threadID * 0x44) = 0x0; //dump from save GSP v0 flags 0
    *(u32*)(GSP_SharedBuff + *threadID * 0x48) = 0x0; //dump from save GSP v0 flags 0
    return 0x2A07; //dump from save GSP v0 flags 0
}


SERVICE_START(gsp_gpu);

SERVICE_CMD(0x10082) // WriteHWRegs
{
    u32 inaddr  = CMD(4);
    u32 length  = CMD(2);
    u32 addr    = CMD(1);
    u32 ret     = 0;

    GPUDEBUG("GSPGPU_WriteHWRegs\n");

    if ((addr & 0x3) != 0) {
        GPUDEBUG("Misaligned address\n");
        ret = 0xe0e02a01;
    }
    if (addr > 0x420000) {
        GPUDEBUG("Address out of range\n");
        ret = 0xe0e02a01;
    }
    if (length > 0x80) {
        GPUDEBUG("Too long\n");
        ret = 0xe0e02bec;
    }
    if (length & 0x3) {
        GPUDEBUG("Length misaligned\n");
        ret = 0xe0e02bf2;
    }

    if(ret == 0) {
        u32 i;

        for (i = 0; i < length; i += 4) {
            u32 val = mem_Read32(inaddr + i);
            u32 addr_out = addr + i;

            GPUDEBUG("Writing %08x to register %08x..\n", val, addr_out);
            gpu_WriteReg32(addr_out, val);
        }
    }

    RESP(1, ret);
    return 0;
}

SERVICE_CMD(0x20084) // WriteHWRegsWithMask
{
    u32 inmask  = CMD(6);
    u32 inaddr  = CMD(4);
    u32 length  = CMD(2);
    u32 addr    = CMD(1);
    u32 ret     = 0;

    GPUDEBUG("GSPGPU_WriteHWRegsWithMask\n");

    if ((addr & 0x3) != 0) {
        GPUDEBUG("Misaligned address\n");
        ret = 0xe0e02a01;
    }
    if (addr > 0x420000) {
        GPUDEBUG("Address out of range\n");
        ret = 0xe0e02a01;
    }
    if (length > 0x80) {
        GPUDEBUG("Too long\n");
        ret = 0xe0e02bec;
    }
    if (length & 0x3) {
        GPUDEBUG("Length misaligned\n");
        ret = 0xe0e02bf2;
    }

    if(ret == 0) {
        u32 i;

        for (i = 0; i < length; i += 4) {
            u32 reg = gpu_ReadReg32(addr + i);
            u32 mask = mem_Read32(inaddr + i);
            u32 val = mem_Read32(inaddr + i);

            GPUDEBUG("Writing %08x mask %08x to register %08x..\n", val, mask, addr + i);
            gpu_WriteReg32(addr + i, (reg & ~mask) | (val & mask));
        }
    }

    RESP(1, ret);
    return 0;
}

SERVICE_CMD(0x30082) // WriteHWRegsRepeat
{
    u32 inaddr  = CMD(4);
    u32 length  = CMD(2);
    u32 addr    = CMD(1);
    u32 ret     = 0;

    GPUDEBUG("GSPGPU_WriteHWRegsRepeat\n");

    if ((addr & 0x3) != 0) {
        GPUDEBUG("Misaligned address\n");
        ret = 0xe0e02a01;
    }
    if (addr > 0x420000) {
        GPUDEBUG("Address out of range\n");
        ret = 0xe0e02a01;
    }
    if (length > 0x80) {
        GPUDEBUG("Too long\n");
        ret = 0xe0e02bec;
    }
    if (length & 0x3) {
        GPUDEBUG("Length misaligned\n");
        ret = 0xe0e02bf2;
    }

    if(ret == 0) {
        u32 i;

        for (i = 0; i < length; i += 4) {
            u32 val = mem_Read32(inaddr + i);

            GPUDEBUG("Writing %08x to register %08x..\n", val, addr);
            gpu_WriteReg32(addr, val);
        }
    }

    RESP(1, ret);
    return 0;
}


SERVICE_CMD(0x40080) // ReadHWRegs
{
    u32 outaddr = EXTENDED_CMD(1);
    u32 length  = CMD(2);
    u32 addr    = CMD(1);
    u32 ret     = 0;

    GPUDEBUG("GSPGPU_ReadHWRegs addr=%08x to=%08x length=%08x\n", addr, outaddr, length);

    if ((addr & 0x3) != 0) {
        GPUDEBUG("Misaligned address\n");
        ret = 0xe0e02a01;
    }
    if (addr > 0x420000) {
        GPUDEBUG("Address out of range\n");
        ret = 0xe0e02a01;
    }
    if (length > 0x80) {
        GPUDEBUG("Too long\n");
        ret = 0xe0e02bec;
    }
    if (length & 0x3) {
        GPUDEBUG("Length misaligned\n");
        ret = 0xe0e02bf2;
    }

    if(ret == 0) {
        for (u32 i = 0; i < length; i += 4)
            mem_Write32((u32)(outaddr + i), gpu_ReadReg32((u32)(addr + i)));
    }

    RESP(1, ret);
    return 0;
}


//FrameBufferInfo info;

//info.active_fb



/*

SERVICE_CMD(0x50200) // SetBufferSwap
{
    u32 screen = CMD(1);
	GPUDEBUG("SetBufferSwap screen=%08x\n", screen);

	FrameBufferInfo* fb_info = (FrameBufferInfo*)CMD(2);
    SetBufferSwap(screen, *fb_info);

    screen_RenderGPU(); //display new stuff
	RESP(1, 0); // No error
    return 0;
}

*/

SERVICE_CMD(0x50200) // SetBufferSwap
{
    GPUDEBUG("SetBufferSwap screen=%08x\n", CMD(1));

    u32 screen = CMD(1);
    u32 reg = screen ? 0x400554 : 0x400454;

    if(gpu_ReadReg32(reg) < 0x52) {
        // init screen
        // TODO: reverse this.
    }

    // TODO: Get rid of this:
    gpu_UpdateFramebufferAddr(arm11_ServiceBufferAddress() + 0x88, //don't use CMD(2) here it is not working!
                          screen & 0x1);

    screen_RenderGPU(); //display new stuff

    RESP(1, 0);
    return 0;
}



SERVICE_CMD(0x80082) // FlushDataCache
{
    GPUDEBUG("FlushDataCache addr=%08x, size=%08x, h=%08x\n", CMD(1), CMD(2), CMD(4));
    RESP(1, 0);
    return 0;
}

SERVICE_CMD(0xB0040) // SetLcdForceBlack
{
    u8 flag = mem_Read8(arm11_ServiceBufferAddress() + 0x84);
    GPUDEBUG("SetLcdForceBlack %02x\n", flag);

    if (flag == 0)
        gpu_WriteReg32(0x202204, 0);
    else
        gpu_WriteReg32(0x202204, 0x1000000);
    if (flag == 0)
        gpu_WriteReg32(0x202a04, 0);
    else
        gpu_WriteReg32(0x202a04, 0x1000000);

    RESP(1, 0);
    return 0;
}

SERVICE_CMD(0xC0000) // TriggerCmdReqQueue
{
    GPUDEBUG("TriggerCmdReqQueue\n");
    gsp_ExecuteCommandFromSharedMem();

    RESP(1, 0);
    return 0;
}

SERVICE_CMD(0x00100040) // SetAxiConfigQoSMode
{
    GPUDEBUG("SetAxiConfigQoSMode\n");

    RESP(1, 0);
    return 0;
}

SERVICE_CMD(0x130042) // RegisterInterruptRelayQueue
{
    GPUDEBUG("RegisterInterruptRelayQueue %08x %08x\n",
          mem_Read32(arm11_ServiceBufferAddress() + 0x84),
          mem_Read32(arm11_ServiceBufferAddress() + 0x8C));

    u32 threadID = 0;
    u32 outMemHandle = 0;

    mem_Write32(arm11_ServiceBufferAddress() + 0x84,
                gpu_RegisterInterruptRelayQueue(mem_Read32(arm11_ServiceBufferAddress() + 0x84),
                        mem_Read32(arm11_ServiceBufferAddress() + 0x8C), &threadID, &outMemHandle));

    mem_Write32(arm11_ServiceBufferAddress() + 0x88, threadID);
    mem_Write32(arm11_ServiceBufferAddress() + 0x90, outMemHandle);
    return 0;
}

SERVICE_CMD(0x160042) // AcquireRight
{
    GPUDEBUG("AcquireRight %08x %08x --todo--\n", mem_Read32(arm11_ServiceBufferAddress() + 0x84), mem_Read32(arm11_ServiceBufferAddress() + 0x8C));
    mem_Write32(arm11_ServiceBufferAddress() + 0x84, 0); //no error
    return 0;
}

SERVICE_CMD(0x1E0080) // SetInternalPriorities
{
    GPUDEBUG("SetInternalPriorities %08x %08x %08x %08x %08x %08x %08x %08x --todo--\n", CMD(1), CMD(2), CMD(3), CMD(4), CMD(5), CMD(6), CMD(7), CMD(8));
    RESP(1, 0);
    return 0;
}

SERVICE_END();


/*
0 = PSC0
1 = PSC1
2 = PDC0
3 = PDC1
4 = PPF
5 = P3D
6 = DMA

PDC0 called every line?
PDC1 called every VBlank?
*/
void gpu_SendInterruptToAll(u32 ID)
{
    dsp_sync_soundinterupt(0); //this is not correct but it patches all games that wait for this
    handleinfo* h = handle_Get(trigevent);
    if (h == NULL) {
        return;
    }
    h->locked = false; //unlock we are fast
    for (int i = 0; i < 4; i++) {
        u8 next = *(u8*)(GSP_SharedBuff + i * 0x40);        //0x33 next is 00
        u8 inuse = *(u8*)(GSP_SharedBuff + i * 0x40 + 1);
        next += inuse;
        if (inuse > 0x20 && ((ID == 2) || (ID == 3)))
            continue; //todo

        *(u8*)(GSP_SharedBuff + i * 0x40 + 1) = inuse + 1;
        *(u8*)(GSP_SharedBuff + i * 0x40 + 2) = 0x0; //no error
        next = next % 0x34;
        *(u8*)(GSP_SharedBuff + i * 0x40 + 0xC + next) = ID;
    }

    if (ID == 4)
    {
        extern int noscreen;
        if (!noscreen) {
            screen_HandleEvent();
            screen_RenderGPU();
        }
    }

}
