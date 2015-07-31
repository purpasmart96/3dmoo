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
static u32 REGS_BEGIN = 0x1EB00000;


display_transfer_config display_transfer;
Command cmd;
FrameBufferUpdate gpu_FrameBufferUpdate;


//#define DUMP_CMDLIST


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
                u32 addr = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                u32 size = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                u32 flags = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);

                GPUDEBUG("GX SetCommandList Last 0x%08x 0x%08x 0x%08x\n", addr, size, flags);

#ifdef DUMP_CMDLIST
                char name[0x100];
                static u32 cmdlist_ctr;

                sprintf(name, "Cmdlist%08x.dat", cmdlist_ctr++);
                FILE* out = fopen(name, "wb");
#endif

                u8* buffer = malloc(size);
                mem_Read(buffer, addr, size);
                gpu_ExecuteCommands(buffer, size);

#ifdef DUMP_CMDLIST
                fwrite(buffer, size, 1, out);
                fclose(out);
#endif
                free(buffer);
                break;
            }

            case GSP_ID_SET_MEMFILL: { //speedup todo
				u32 start1, val1, addrend1, addr2, val2, addrend2, width;
				start1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                val1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                addrend1 = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);
                addr2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x10);
                val2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x14);
                addrend2 = *(u32*)(base_addr + (j + 1) * 0x20 + 0x18);
                width = *(u32*)(base_addr + (j + 1) * 0x20 + 0x1C);

				GPUDEBUG("GX SetMemoryFill 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n", start1, val1, addrend1, addr2, val2, addrend2, width);
				if (start1 - 0x1f000000 > 0x600000 || addrend1 - 0x1f000000 > 0x600000) {
                    GPUDEBUG("SetMemoryFill into non VRAM not suported\r\n");
                } else {
					u32 size = gpu_BytesPerPixel(width & 0xFFFF);
					for (u32 k = start1; k < addrend1; k += size) {
                        for(s32 m = size - 1; m >= 0; m--)
                        {
                            VRAM_MemoryBuff[m + (k - 0x1F000000)] = (u8)(val1 >> (m * 8));
                        }
                    }
                }
                if (addr2 - 0x1f000000 > 0x600000 || addrend2 - 0x1f000000 > 0x600000) {
                    if (addr2 && addrend2)
                        GPUDEBUG("SetMemoryFill into non VRAM not suported\r\n");
                } else {
					u32 size = gpu_BytesPerPixel((width >> 16) & 0xFFFF);
                    for(u32 k = addr2; k < addrend2; k += size) {
                        for (s32 m = size - 1; m >= 0; m--)
                            VRAM_MemoryBuff[m + (k - 0x1F000000)] = (u8)(val2 >> (m * 8));
                    }
                }
                gpu_SendInterruptToAll(0);
                break;
            }
            case GSP_ID_SET_DISPLAY_TRANSFER:
            {
                    gpu_SendInterruptToAll(4);


					u32 inpaddr, outputaddr, input_size, output_size, flags, unk;
                    inpaddr = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
                    outputaddr = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                    input_size = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);
					output_size = *(u32*)(base_addr + (j + 1) * 0x20 + 0x10);
                    flags = *(u32*)(base_addr + (j + 1) * 0x20 + 0x14);
                    unk = *(u32*)(base_addr + (j + 1) * 0x20 + 0x18);
					GPUDEBUG("GX SetDisplayTransfer 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n", inpaddr, outputaddr, input_size, output_size, flags, unk);

					cmd.display_transfer.in_buffer_address = inpaddr;
					cmd.display_transfer.out_buffer_address = outputaddr;
					cmd.display_transfer.in_buffer_size = input_size;
					cmd.display_transfer.out_buffer_size = output_size;
					cmd.display_transfer.flags = flags;


					display_transfer.input_address = inpaddr;
					display_transfer.output_address = outputaddr;
					display_transfer.input_size = input_size;
					display_transfer.output_size = output_size;
					display_transfer.flags = flags;

                    u8* inaddr = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(inpaddr));
                    u8* outaddr = gpu_GetPhysicalMemoryBuff(gpu_ConvertVirtualToPhysical(outputaddr));


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
					u32 config_ScaleX = 1;
					u32 config_ScaleXY = 2;

					bool horizontal_scale = config_scaling != config_NoScale;
					bool vertical_scale = config_scaling == config_ScaleXY;

					u32 output_width = config_output_width >> horizontal_scale;
					u32 output_height = config_output_height >> vertical_scale;

					u32 input_size_ = config_input_width * config_input_height * gpu_BytesPerPixel(config_input_format);
					u32 output_size_ = output_width * output_height * gpu_BytesPerPixel(config_output_format);


                    u32 rely = (input_size & 0xFFFF);
                    u32 relx = ((input_size >> 0x10) & 0xFFFF);

					u32 outy = (output_size & 0xFFFF);
					u32 outx = ((output_size >> 0x10) & 0xFFFF);

					if (input_size != output_size) {
                        /*FILE *test = fopen("Conversion.bin", "wb");
                        u32 len = 0;
                        switch(flags & 0x700)
                        {
                            case 0: //RGBA8
                                len = rely * relx * 4;
                                break;
                            case 0x100: //RGB8
                                len = rely * relx * 3;
                                break;
                            case 0x200: //RGB565
                            case 0x300: //RGB5A1
                            case 0x400: //RGBA4
                                len = rely * relx * 2;
                                break;
                        }
                        fwrite(inaddr, 1, len, test);
                        fclose(test);*/

                        //Lets just skip the first 80*240*bytesperpixel or so as its blank then continue as usual
                        switch(flags & 0x700)
                        {
                            case 0: //RGBA8
                                inaddr += outy * abs(relx - outx) * 4;
                                break;
                            case 0x100: //RGB8
                                inaddr += outy * abs(relx - outx) * 3;
                                break;
                            case 0x200: //RGB565
                            case 0x300: //RGB5A1
                            case 0x400: //RGBA4
                                inaddr += outy * abs(relx - outx) * 2;
                                break;
                        }
                        //GPUDEBUG("error converting from %08x to %08x\n", input_size, output_size);
                        //break;
                    }

                    if((flags & 0x700) == ((flags & 0x7000) >> 4))
                    {
                        u32 len = 0;
                        switch(flags & 0x700)
                        {
                            case 0: //RGBA8
                                len = rely * relx * 4;
                                break;
                            case 0x100: //RGB8
                                len = rely * relx * 3;
                                break;
                            case 0x200: //RGB565
                            case 0x300: //RGB5A1
                            case 0x400: //RGBA4
                                len = rely * relx * 2;
                                break;
                        }
                        GPUDEBUG("copying %d (width %d/%d, height %d/%d)\n", len, relx, outx, rely, outy);
                        memcpy(outaddr, inaddr, len);
                    }
                    else
                    {
                        GPUDEBUG("converting %d to %d (width %d/%d, height %d/%d)\n", (flags & 0x700) >> 8, (flags & 0x7000) >> 12, relx, outx, rely, outy);
                        Color color;
                        
                        for(u32 y = 0; y < outy; ++y)
                        {
                            for(u32 x = 0; x < outx; ++x) 
                            {
                                switch(flags & 0x700) { //input format

                                    case 0: //RGBA8
                                        color_decode(inaddr, RGBA8, &color);
                                        inaddr += 4;
                                        break;
                                    case 0x100: //RGB8
                                        color_decode(inaddr, RGB8, &color);
                                        inaddr += 3;
                                        break;
                                    case 0x200: //RGB565
                                        color_decode(inaddr, RGB565, &color);
                                        inaddr += 2;
                                        break;
                                    case 0x300: //RGB5A1
                                        color_decode(inaddr, RGBA5551, &color);
                                        inaddr += 2;
                                        break;
                                    case 0x400: //RGBA4
                                        color_decode(inaddr, RGBA4, &color);
                                        inaddr += 2;
                                        break;
                                    default:
                                        GPUDEBUG("error unknown input format %04X\n", flags & 0x700);
                                        break;
                                }
                                //write it back

                                switch(flags & 0x7000) { //output format

                                    case 0: //RGBA8
                                        color_encode(&color, RGBA8, outaddr);
                                        outaddr += 4;
                                        break;
                                    case 0x1000: //RGB8
                                        color_encode(&color, RGB8, outaddr);
                                        outaddr += 3;
                                        break;
                                    case 0x2000: //RGB565
                                        color_encode(&color, RGB565, outaddr);
                                        outaddr += 2;
                                        break;
                                    case 0x3000: //RGB5A1
                                        color_encode(&color, RGBA5551, outaddr);
                                        outaddr += 2;
                                        break;
                                    case 0x4000: //RGBA4
                                        color_encode(&color, RGBA4, outaddr);
                                        outaddr += 2;
                                        break;
                                    default:
                                        GPUDEBUG("error unknown output format %04X\n", flags & 0x7000);
                                        break;
                                }

                            }
                        }
                    }
                    break;
                }
            case GSP_ID_SET_TEXTURE_COPY: {
                gpu_SendInterruptToAll(1);
				u32 in_buffer_address, out_buffer_address, size, in_width_gap, out_width_gap, flags;
				in_buffer_address = *(u32*)(base_addr + (j + 1) * 0x20 + 0x4);
				out_buffer_address = *(u32*)(base_addr + (j + 1) * 0x20 + 0x8);
                size = *(u32*)(base_addr + (j + 1) * 0x20 + 0xC);
				in_width_gap = *(u32*)(base_addr + (j + 1) * 0x20 + 0x10);
				out_width_gap = *(u32*)(base_addr + (j + 1) * 0x20 + 0x14);
                flags = *(u32*)(base_addr + (j + 1) * 0x20 + 0x18);
				GPUDEBUG("GX SetTextureCopy 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n", in_buffer_address, out_buffer_address, size, in_width_gap, out_width_gap, flags);

				cmd.texture_copy.in_buffer_address = in_buffer_address;
				cmd.texture_copy.out_buffer_address = out_buffer_address;
				cmd.texture_copy.size = size;
				cmd.texture_copy.in_width_gap = in_width_gap;
				cmd.texture_copy.out_width_gap = out_width_gap;
				cmd.texture_copy.flags = flags;
                //gpu_UpdateFramebuffer();
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
                GPUDEBUG("GX cmd 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\r\n", *(u32*)(base_addr + (j + 1) * 0x20), *(u32*)((base_addr + (j + 1) * 0x20) + 0x4), *(u32*)((base_addr + (j + 1) * 0x20) + 0x8), *(u32*)((base_addr + (j + 1) * 0x20) + 0xC), *(u32*)((base_addr + (j + 1) * 0x20) + 0x10), *(u32*)((base_addr + (j + 1) * 0x20) + 0x14), *(u32*)((base_addr + (j + 1) * 0x20) + 0x18), *(u32*)((base_addr + (j + 1) * 0x20)) + 0x1C);
                break;
            }
        }
    }
}

void gpu_RegisterInterruptRelayQueue(u32 flags, u32 Kevent, u32*threadID, u32*outMemHandle)
{
    *threadID = numReqQueue++;
    *outMemHandle = handle_New(HANDLE_TYPE_SHAREDMEM, MEM_TYPE_GSP_0);
    trigevent = Kevent;
    handleinfo* h = handle_Get(Kevent);
    if (h == NULL) {
        GPUDEBUG("failed to get Event\n");
        PAUSE();
    }
    h->locked = false; //unlock we are fast

    *(u32*)(GSP_SharedBuff + *threadID * 0x40) = 0x0; //dump from save GSP v0 flags 0
    *(u32*)(GSP_SharedBuff + *threadID * 0x44) = 0x0; //dump from save GSP v0 flags 0
    *(u32*)(GSP_SharedBuff + *threadID * 0x48) = 0x0; //dump from save GSP v0 flags 0
}

/// Gets a pointer to the interrupt relay queue for a given thread index
InterruptRelayQueue* gpu_GetInterruptRelayQueue(u32 threadID)
{
	u8* base_addr = *(u8*)(GSP_SharedBuff + threadID * sizeof(InterruptRelayQueue));
	DEBUG("base_addr=0x%08x", base_addr);
	return base_addr;
}

/**
* Checks if the parameters in a register write call are valid and logs in the case that
* they are not
* @param base_address The first address in the sequence of registers that will be written
* @param size_in_bytes The number of registers that will be written
* @return true if the parameters are valid, false otherwise
*/
bool CheckWriteParameters(u32 base_address, u32 size_in_bytes) {
	// TODO: Return proper error codes
	if (base_address + size_in_bytes >= 0x420000) {
		ERROR("Write address out of range! (address=0x%08x, size=0x%08x)\n",
			base_address, size_in_bytes);
		return false;
	}

	// size should be word-aligned
	if ((size_in_bytes % 4) != 0) {
		ERROR("Invalid size 0x%08x\n", size_in_bytes);
		return false;
	}

	return true;
}

void WriteHWRegs(u32 base_address, u32 size_in_bytes, const u32* data) {
	// TODO: Return proper error codes
	if (!CheckWriteParameters(base_address, size_in_bytes))
		return;

	while (size_in_bytes > 0) {
		gpu_WriteReg32(base_address + REGS_BEGIN, *data);

		size_in_bytes -= 4;
		++data;
		base_address += 4;
	}
}

void SetBufferSwap(u32 screen_id, const FrameBufferInfo info) {
	u32 base_address = 0x400000;
	u32 phys_address_left = gpu_ConvertVirtualToPhysical(info.address_left);
	u32 phys_address_right = gpu_ConvertVirtualToPhysical(info.address_right);
	if (info.active_fb == 0) {
		WriteHWRegs(base_address + 4 * (u32)((framebuffer_config[screen_id].address_left1)), 4, &phys_address_left);
		WriteHWRegs(base_address + 4 * (u32)(framebuffer_config[screen_id].address_right1), 4, &phys_address_right);
	} else {
		WriteHWRegs(base_address + 4 * (u32)(framebuffer_config[screen_id].address_left2), 4, &phys_address_left);
		WriteHWRegs(base_address + 4 * (u32)(framebuffer_config[screen_id].address_right2), 4, &phys_address_right);
	}
	WriteHWRegs(base_address + 4 * (u32)(framebuffer_config[screen_id].stride), 4, &info.stride);
	WriteHWRegs(base_address + 4 * (u32)(framebuffer_config[screen_id].color_format), 4, &info.format);
	WriteHWRegs(base_address + 4 * (u32)(framebuffer_config[screen_id].active_fb), 4, &info.shown_fb);
	DEBUG("BufferSwap called");
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
        for (u32 i = 0; i < length; i += 4) {
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
        u32 i;
        for (i = 0; i < length; i += 4)
            mem_Write32((u32)(outaddr + i), gpu_ReadReg32((u32)(addr + i)));
    }

    RESP(1, ret);
    return 0;
}

SERVICE_CMD(0x50200) // SetBufferSwap
{
    GPUDEBUG("SetBufferSwap screen=%08x\n", CMD(1));

    u32 screen_id = CMD(1);
	u32 fb_junk = CMD(2);

	FrameBufferInfo* fb_info = (FrameBufferInfo*)&fb_junk;
	SetBufferSwap(screen_id, *fb_info);

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
	u32 flags = CMD(1);
	u32 handle_event = CMD(3);
    GPUDEBUG("RegisterInterruptRelayQueue flags=%08x\n", flags);

	u32 *threadID = numReqQueue++;
	u32 *outMemHandle = handle_New(HANDLE_TYPE_SHAREDMEM, MEM_TYPE_GSP_0);
	trigevent = handle_event;
	handleinfo* h = handle_Get(handle_event);
	if (h == NULL) {
		GPUDEBUG("failed to get Event\n");
		PAUSE();
	}
	h->locked = false; //unlock we are fast

	//gpu_RegisterInterruptRelayQueue(flags, handle_event, threadID, outMemHandle);


	RESP(1, 0x2A07);
    RESP(2, threadID);
    RESP(4, outMemHandle);
    return 0;
}

SERVICE_CMD(0x160042) // AcquireRight
{
    GPUDEBUG("AcquireRight %08x %08x --todo--\n", mem_Read32(arm11_ServiceBufferAddress() + 0x84), mem_Read32(arm11_ServiceBufferAddress() + 0x8C));
    //mem_Write32(arm11_ServiceBufferAddress() + 0x84, 0); //no error
	RESP(1, 0); // no error
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
0 = PSC0 private?
1 = PSC1 private?
2 = PDC0 public?
3 = PDC1 public?
4 = PPF  private?
5 = P3D  private?
6 = DMA  private?

PDC0 called every line?
PDC1 called every VBlank?
*/
void gpu_SendInterruptToAll(u32 ID)
{
    dsp_sync_soundinter(0); //this is not correct but it patches all games that wait for this
    handleinfo* h = handle_Get(trigevent);
    if (h == NULL) {
        return;
    }
    h->locked = false; //unlock we are fast
    for (int i = 0; i < 4; i++) {
		u8 next = *(u8*)(GSP_SharedBuff + i * 0x40);
		u8 number_interrupts = *(u8*)(GSP_SharedBuff + i * 0x40 + 1);
		next += number_interrupts;

		number_interrupts += 1;

        *(u8*)(GSP_SharedBuff + i * 0x40 + 2) = 0x0; //no error
        next = next % 0x34; // 0x34 is the number of interrupt slots
        *(u8*)(GSP_SharedBuff + i * 0x40 + 0xC + next) = ID;

		// Update framebuffer information if requested
		// TODO(yuriks): Confirm where this code should be called. It is definitely updated without
		//               executing any GSP commands, only waiting on the event.
		int screen_id = (ID == 2) ? 0 : (ID == 3) ? 1 : -1;
		if (screen_id != -1) {
			FrameBufferUpdate* info = gpu_GetFrameBufferInfo(i, screen_id);
			if (info->is_dirty) {
				SetBufferSwap(screen_id, info->framebuffer_info[info->index]);
				info->is_dirty = false;
			}
		}

    }

    if (ID == 4)
    {
        extern int noscreen;
        if (!noscreen) {
            screen_HandleEvent();
        }
    }

}
