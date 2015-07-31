#include "util.h"
#include "color.h"


/// Convert a 1-bit color component to 8 bit
u8 Convert1To8(u8 value) {
	return value * 255;
}

/// Convert a 4-bit color component to 8 bit
u8 Convert4To8(u8 value) {
	return (value << 4) | value;
}

/// Convert a 5-bit color component to 8 bit
u8 Convert5To8(u8 value) {
	return (value << 3) | (value >> 2);
}

/// Convert a 6-bit color component to 8 bit
u8 Convert6To8(u8 value) {
	return (value << 2) | (value >> 4);
}

/// Convert a 8-bit color component to 1 bit
u8 Convert8To1(u8 value) {
	return value >> 7;
}

/// Convert a 8-bit color component to 4 bit
u8 Convert8To4(u8 value) {
	return value >> 4;
}

/// Convert a 8-bit color component to 5 bit
u8 Convert8To5(u8 value) {
	return value >> 3;
}

/// Convert a 8-bit color component to 6 bit
u8 Convert8To6(u8 value) {
	return value >> 2;
}

// Interleave the lower 3 bits of each coordinate to get the intra-block offsets, which are
// arranged in a Z-order curve. More details on the bit manipulation at:
// https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
//
u32 MortonInterleave(u32 x, u32 y) {
	u32 i = (x & 7) | ((y & 7) << 8); // ---- -210
	i = (i ^ (i << 2)) & 0x1313;      // ---2 --10
	i = (i ^ (i << 1)) & 0x1515;      // ---2 -1-0
	i = (i | (i >> 7)) & 0x3F;
	return i;
}

// Calculates the offset of the position of the pixel in Morton order 
u32 GetMortonOffset(u32 x, u32 y, u32 bytes_per_pixel) {
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
	//
	// This pattern is what's called Z-order curve, or Morton order.

	const u32 block_height = 8;
	const u32 coarse_x = x & ~7;

	u32 i = MortonInterleave(x, y);

	const u32 offset = coarse_x * block_height;

	return (i + offset) * bytes_per_pixel;
}

clov4 DecodeRGBA8(u8* input) {
	clov4 ret;
	ret.v[0] = input[3];
	ret.v[1] = input[2];
	ret.v[2] = input[1];
	ret.v[3] = input[0];
	return ret;
}

clov4 DecodeRGB8(u8* input) {
	clov4 ret;
	u16 val = (u16*)(input);
	ret.v[0] = input[2];
	ret.v[1] = input[1];
	ret.v[2] = input[0];
	ret.v[3] = 255;
	return ret;
}

clov4 DecodeRGB565(u8* input) {
	clov4 ret;
	u16 val = (u16*)(input);
	ret.v[0] = Convert5To8((val >> 11) & 0x1F);
	ret.v[1] = Convert6To8((val >> 5) & 0x3F);
	ret.v[2] = Convert5To8((val >> 1) & 0x1F);
	ret.v[3] = 255;
	return ret;
}

clov4 DecodeRGB5A1(u8* input) {
	clov4 ret;
	u16 val = (u16*)(input);
	ret.v[0] = Convert5To8((val >> 11) & 0x1F);
	ret.v[1] = Convert5To8((val >> 6) & 0x1F);
	ret.v[2] = Convert5To8((val >> 1) & 0x1F);
	ret.v[3] = Convert1To8(val & 0x1);
	return ret;
}

clov4 DecodeRGBA4(u8* input) {
	clov4 ret;
	u16 val = (u16*)(input);
	ret.v[0] = Convert4To8((val >> 12) & 0xF);
	ret.v[1] = Convert4To8((val >> 8) & 0xF);
	ret.v[2] = Convert4To8((val >> 4) & 0xF);
	ret.v[3] = Convert4To8(val & 0xF);
    return ret;
}

/**
* Decode a depth value stored in D16 format
* @param bytes Pointer to encoded source value
* @return Depth value as an u32
*/
u32 DecodeD16(const u8* bytes) {
	return *(u16*)(bytes);
}

/**
* Decode a depth value stored in D24 format
* @param bytes Pointer to encoded source value
* @return Depth value as an u32
*/
u32 DecodeD24(const u8* bytes) {
	return (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
}


/**
* Encode a color as RGBA8 format
* @param color Source color to encode
* @param bytes Destination pointer to store encoded color
*/
void EncodeRGBA8(const clov4 color, u8* bytes) {
	bytes[3] = color.v[0];
	bytes[2] = color.v[1];
	bytes[1] = color.v[2];
	bytes[0] = color.v[3];
}

/**
* Encode a color as RGB8 format
* @param color Source color to encode
* @param bytes Destination pointer to store encoded color
*/
void EncodeRGB8(const clov4 color, u8* bytes) {
	bytes[2] = color.v[0];
	bytes[1] = color.v[1];
	bytes[0] = color.v[2];
}

/**
* Encode a color as RGB565 format
* @param color Source color to encode
* @param bytes Destination pointer to store encoded color
*/
void EncodeRGB565(const clov4 color, u8* bytes) {
	*(u16*)(bytes) = (Convert8To5(color.v[0]) << 11) |
		(Convert8To6(color.v[1]) << 5) | Convert8To5(color.v[2]);
}

/**
* Encode a color as RGB5A1 format
* @param color Source color to encode
* @param bytes Destination pointer to store encoded color
*/
void EncodeRGB5A1(const clov4 color, u8* bytes) {
	*(u16*)(bytes) = (Convert8To5(color.v[0]) << 11) |
		(Convert8To5(color.v[1]) << 6) | (Convert8To5(color.v[2]) << 1) | Convert8To1(color.v[3]);
}

/**
* Encode a color as RGBA4 format
* @param color Source color to encode
* @param bytes Destination pointer to store encoded color
*/
void EncodeRGBA4(const clov4 color, u8* bytes) {
	*(u16*)(bytes) = (Convert8To4(color.v[0]) << 12) |
		(Convert8To4(color.v[1]) << 8) | (Convert8To4(color.v[2]) << 4) | Convert8To4(color.v[3]);
}

/**
* Encode a 16 bit depth value as D16 format
* @param value 16 bit source depth value to encode
* @param bytes Pointer where to store the encoded value
*/
void EncodeD16(u32 value, u8* bytes) {
	*(u16*)(bytes) = value & 0xFFFF;
}

/**
* Encode a 24 bit depth value as D24 format
* @param value 24 bit source depth value to encode
* @param bytes Pointer where to store the encoded value
*/
void EncodeD24(u32 value, u8* bytes) {
	bytes[0] = value & 0xFF;
	bytes[1] = (value >> 8) & 0xFF;
	bytes[2] = (value >> 16) & 0xFF;
}

/**
* Encode a 24 bit depth and 8 bit stencil values as D24S8 format
* @param depth 24 bit source depth value to encode
* @param stencil 8 bit source stencil value to encode
* @param bytes Pointer where to store the encoded value
*/
void EncodeD24S8(u32 depth, u8 stencil, u8* bytes) {
	bytes[0] = depth & 0xFF;
	bytes[1] = (depth >> 8) & 0xFF;
	bytes[2] = (depth >> 16) & 0xFF;
	bytes[3] = stencil;
}

/**
* Encode a 24 bit depth value as D24X8 format (32 bits per pixel with 8 bits unused)
* @param depth 24 bit source depth value to encode
* @param bytes Pointer where to store the encoded value
* @note unused bits will not be modified
*/
void EncodeD24X8(u32 depth, u8* bytes) {
	bytes[0] = depth & 0xFF;
	bytes[1] = (depth >> 8) & 0xFF;
	bytes[2] = (depth >> 16) & 0xFF;
}

/**
* Encode an 8 bit stencil value as X24S8 format (32 bits per pixel with 24 bits unused)
* @param stencil 8 bit source stencil value to encode
* @param bytes Pointer where to store the encoded value
* @note unused bits will not be modified
*/
void EncodeX24S8(u8 stencil, u8* bytes) {
	bytes[3] = stencil;
}

void color_decode(u8* input, const TextureFormat format, color *output)
{
    switch(format) {
    case RGBA8:
    {
        output->r = input[3];
        output->g = input[2];
        output->b = input[1];
        output->a = input[0];
        break;
    }

    case RGB8:
    {
        output->r = input[2];
        output->g = input[1];
        output->b = input[0];
        output->a = 255;
        break;
    }

    case RGB5A1:
    {
        u16 val = input[0] | input[1] << 8;
        output->r = Convert5To8((val >> 11) & 0x1F);
		output->g = Convert5To8((val >> 6) & 0x1F);
		output->b = Convert5To8((val >> 1) & 0x1F);
		output->a = Convert1To8(val & 0x1);
        break;
    }

    case RGB565:
    {
        u16 val = input[0] | input[1] << 8;
		output->r = Convert5To8((val >> 11) & 0x1F);
		output->g = Convert6To8((val >> 5) & 0x3F);
		output->b = Convert5To8(val & 0x1F);
        output->a = 255;
        break;
    }

    case RGBA4:
    {
        u16 val = input[0] | input[1] << 8;
		output->r = Convert4To8((val >> 12) & 0xF);
		output->g = Convert4To8((val >> 8) & 0xF);
		output->b = Convert4To8((val >> 4) & 0xF);
		output->a = Convert4To8(val & 0xF);
        break;
    }

    case IA8:
    {
        output->r = input[0];
        output->g = input[0];
        output->b = input[0];
        output->a = input[1];
        break;
    }

    case I8:
    {
        output->r = input[0];
        output->g = input[0];
        output->b = input[0];
        output->a = 255;
        break;
    }

    case A8:
    {
        output->r = 0;
        output->g = 0;
        output->b = 0;
        output->a = input[0];
    }

    case IA4:
    {
        output->r = ((input[0] >> 4) & 0xF) * 16;
        output->g = ((input[0] >> 4) & 0xF) * 16;
        output->b = ((input[0] >> 4) & 0xF) * 16;
        output->a = (input[0] & 0xF) * 16;
        break;
    }

    /*case I4:
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
      }*/

    default:
        DEBUG("Unknown texture format: 0x%08X\n", (u32)format);
        break;
    }
}

void color_encode(color *input, const TextureFormat format, u8* output)
{
    switch(format) {
    case RGBA8:
    {
        output[3] = input->r;
        output[2] = input->g;
        output[1] = input->b;
        output[0] = input->a;
        break;
    }

    case RGB8:
    {
        output[2] = input->r;
        output[1] = input->g;
        output[0] = input->b;
        break;
    }

    case RGB5A1:
    {
		(u16*)output = (Convert8To5(input->r) << 11) | (Convert8To5(input->g) << 6) | (Convert8To6(input->b) << 1) | (Convert8To5(input->a));

        //output[0] = val & 0xFF;
        //output[1] = (val >> 8) & 0xFF;
        break;
    }

    case RGB565:
    {
        u16 val = 0;

        val |= ((input->r / 8) & 0x1F) << 11;
        val |= ((input->g / 4) & 0x3F) << 5;
        val |= ((input->b / 8) & 0x1F) << 0;

        output[0] = val & 0xFF;
        output[1] = (val >> 8) & 0xFF;
        break;
    }

    case RGBA4:
    {
        u16 val = 0;

        val |= ((input->r / 16) & 0xF) << 12;
        val |= ((input->g / 16) & 0xF) << 8;
        val |= ((input->b / 16) & 0xF) << 4;
        val |= ((input->a / 16) & 0xF) << 0;

        output[0] = val & 0xFF;
        output[1] = (val >> 8) & 0xFF;

        break;
    }

    case IA8:
    {
        output[0] = input->r;
        output[1] = input->a;
        break;
    }

    case I8:
    {
        output[0] = input->r;
        break;
    }

    case A8:
    {
        output[0] = input->a;
    }

    case IA4:
    {
        u8 val = 0;
        val |= ((input->r / 16) & 0xF) << 4;
        val |= ((input->a / 16) & 0xF) << 0;
        output[0] = val;
        break;
    }

    /*case I4:
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
      }*/

    default:
        DEBUG("Unknown texture format: 0x%08X\n", (u32)format);
        break;
    }
}
